/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*!
 * @file    los_arch_mmu.c
 * @brief 虚实映射其实就是一个建立页表的过程
 * @link http://weharmonyos.com/openharmony/zh-cn/device-dev/kernel/kernel-small-basic-inner-reflect.html
 * @verbatim
 
	虚实映射是指系统通过内存管理单元（MMU，Memory Management Unit）将进程空间的虚拟地址与实际的物理地址做映射，
	并指定相应的访问权限、缓存属性等。程序执行时，CPU访问的是虚拟内存，通过MMU页表条目找到对应的物理内存，
	并做相应的代码执行或数据读写操作。MMU的映射由页表（Page Table）来描述，其中保存虚拟地址和物理地址的映射关系以及访问权限等。
	每个进程在创建的时候都会创建一个页表，页表由一个个页表条目（Page Table Entry， PTE）构成，
	每个页表条目描述虚拟地址区间与物理地址区间的映射关系。MMU中有一块页表缓存，称为快表（TLB, Translation Lookaside Buffers），
	做地址转换时，MMU首先在TLB中查找，如果找到对应的页表条目可直接进行转换，提高了查询效率。

	虚实映射其实就是一个建立页表的过程。MMU有多级页表，LiteOS-A内核采用二级页表描述进程空间。每个一级页表条目描述符占用4个字节，
	可表示1MiB的内存空间的映射关系，即1GiB用户空间（LiteOS-A内核中用户空间占用1GiB）的虚拟内存空间需要1024个。系统创建用户进程时，
	在内存中申请一块4KiB大小的内存块作为一级页表的存储区域，二级页表根据当前进程的需要做动态的内存申请。

	用户程序加载启动时，会将代码段、数据段映射进虚拟内存空间（详细可参考动态加载与链接），此时并没有物理页做实际的映射；
	程序执行时，如下图粗箭头所示，CPU访问虚拟地址，通过MMU查找是否有对应的物理内存，若该虚拟地址无对应的物理地址则触发缺页异常，
	内核申请物理内存并将虚实映射关系及对应的属性配置信息写进页表，并把页表条目缓存至TLB，接着CPU可直接通过转换关系访问实际的物理内存；
	若CPU访问已缓存至TLB的页表条目，无需再访问保存在内存中的页表，可加快查找速度。

	开发流程
		1. 虚实映射相关接口的使用：
			通过LOS_ArchMmuMap映射一块物理内存。

		2. 对映射的地址区间做相关操作：
			通过LOS_ArchMmuQuery可以查询相应虚拟地址区间映射的物理地址区间及映射属性；
			通过LOS_ArchMmuChangeProt修改映射属性；
			通过LOS_ArchMmuMove做虚拟地址区间的重映射。
		3. 通过LOS_ArchMmuUnmap解除映射关系。
 * @details
 *   - 主要实现了虚拟地址到物理地址的映射、解除映射、权限修改、映射迁移、MMU上下文切换等功能。
 *   - 支持二级页表，LiteOS-A内核采用二级页表描述进程空间。
 *   - 支持页表锁、TLB失效、ASID分配与回收等机制。
 *   - 代码中包含大量注释，详细说明了每个函数的作用和关键实现细节。
 * @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2025-07-05
 *
 * @history
 *
 */
#include "los_arch_mmu.h"
#include "los_asid.h"
#include "los_pte_ops.h"
#include "los_tlb_v6.h"
#include "los_printf.h"
#include "los_vm_common.h"
#include "los_vm_map.h"
#include "los_vm_boot.h"
#include "los_mmu_descriptor_v6.h"
#include "los_process_pri.h"

#ifdef LOSCFG_KERNEL_MMU  // 如果启用了MMU功能
typedef struct {  // MMU映射信息结构体
    LosArchMmu *archMmu;  // 指向架构相关MMU控制结构
    VADDR_T *vaddr;       // 虚拟地址指针
    PADDR_T *paddr;       // 物理地址指针
    UINT32 *flags;        // 映射标志位
} MmuMapInfo;

#define TRY_MAX_TIMES 10  // 最大尝试次数定义

// 定义一级页表，按L1页表项数量对齐，并放置在.bss.prebss.translation_table段
__attribute__((aligned(MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS))) \
    __attribute__((section(".bss.prebss.translation_table"))) UINT8 \
    g_firstPageTable[MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS];
#ifdef LOSCFG_KERNEL_SMP  // 如果支持SMP（对称多处理）
// 定义临时页表，用于SMP启动过程
__attribute__((aligned(MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS))) \
    __attribute__((section(".bss.prebss.translation_table"))) UINT8 \
    g_tempPageTable[MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS];
UINT8 *g_mmuJumpPageTable = g_tempPageTable;  // 跳转页表指针初始化为临时页表
#else
extern CHAR __mmu_ttlb_begin; /* 在链接脚本中定义 */
// 跳转页表指针初始化为链接脚本定义的页表起始地址（仅用于系统启动）
UINT8 *g_mmuJumpPageTable = (UINT8 *)&__mmu_ttlb_begin; /* temp page table, this is only used when system power up */
#endif

// 获取页表项锁，支持细粒度锁或全局锁
STATIC SPIN_LOCK_S *OsGetPteLock(LosArchMmu *archMmu, PADDR_T paddr, UINT32 *intSave)
{
    SPIN_LOCK_S *lock = NULL;  // 锁指针初始化为空
#ifdef LOSCFG_PAGE_TABLE_FINE_LOCK  // 如果启用页表细粒度锁
    LosVmPage *vmPage = NULL;  // 虚拟内存页指针

    vmPage = OsVmPaddrToPage(paddr);  // 将物理地址转换为虚拟内存页
    if (vmPage == NULL) {  // 如果虚拟内存页不存在
        return NULL;  // 返回空指针
    }
    lock = &vmPage->lock;  // 获取该页的锁
#else
    lock = &archMmu->lock;  // 使用MMU结构的全局锁
#endif

    LOS_SpinLockSave(lock, intSave);  // 获取自旋锁并保存中断状态
    return lock;  // 返回获取到的锁
}

// 获取一级页表项锁
STATIC SPIN_LOCK_S *OsGetPte1Lock(LosArchMmu *archMmu, PADDR_T paddr, UINT32 *intSave)
{
    return OsGetPteLock(archMmu, paddr, intSave);  // 调用通用页表项锁获取函数
}

// 释放一级页表项锁
STATIC INLINE VOID OsUnlockPte1(SPIN_LOCK_S *lock, UINT32 intSave)
{
    if (lock == NULL) {  // 如果锁指针为空
        return;  // 直接返回
    }
    LOS_SpinUnlockRestore(lock, intSave);  // 释放自旋锁并恢复中断状态
}

// 获取一级页表项临时锁（仅细粒度锁模式下有效）
STATIC SPIN_LOCK_S *OsGetPte1LockTmp(LosArchMmu *archMmu, PADDR_T paddr, UINT32 *intSave)
{
    SPIN_LOCK_S *spinLock = NULL;  // 自旋锁指针初始化为空
#ifdef LOSCFG_PAGE_TABLE_FINE_LOCK  // 如果启用页表细粒度锁
    spinLock = OsGetPteLock(archMmu, paddr, intSave);  // 获取页表项锁
#else
    (VOID)archMmu;  // 未使用参数，避免编译警告
    (VOID)paddr;   // 未使用参数，避免编译警告
    (VOID)intSave; // 未使用参数，避免编译警告
#endif
    return spinLock;  // 返回获取到的锁
}

// 释放一级页表项临时锁
STATIC INLINE VOID OsUnlockPte1Tmp(SPIN_LOCK_S *lock, UINT32 intSave)
{
#ifdef LOSCFG_PAGE_TABLE_FINE_LOCK  // 如果启用页表细粒度锁
    if (lock == NULL) {  // 如果锁指针为空
        return;  // 直接返回
    }
    LOS_SpinUnlockRestore(lock, intSave);  // 释放自旋锁并恢复中断状态
#else
    (VOID)lock;    // 未使用参数，避免编译警告
    (VOID)intSave; // 未使用参数，避免编译警告
#endif
}

// 获取二级页表项锁
STATIC INLINE SPIN_LOCK_S *OsGetPte2Lock(LosArchMmu *archMmu, PTE_T pte1, UINT32 *intSave)
{
    PADDR_T pa = MMU_DESCRIPTOR_L1_PAGE_TABLE_ADDR(pte1);  // 从L1页表项中提取物理地址
    return OsGetPteLock(archMmu, pa, intSave);  // 调用通用页表项锁获取函数
}

// 释放二级页表项锁
STATIC INLINE VOID OsUnlockPte2(SPIN_LOCK_S *lock, UINT32 intSave)
{
    return OsUnlockPte1(lock, intSave);  // 调用一级页表项锁释放函数
}

// 获取二级页表基地址指针
STATIC INLINE PTE_T *OsGetPte2BasePtr(PTE_T pte1)
{
    PADDR_T pa = MMU_DESCRIPTOR_L1_PAGE_TABLE_ADDR(pte1);  // 从L1页表项中提取物理地址
    return LOS_PaddrToKVaddr(pa);  // 将物理地址转换为内核虚拟地址
}

// 获取首个页表的虚拟地址
VADDR_T *OsGFirstTableGet(VOID)
{
    return (VADDR_T *)g_firstPageTable;  // 返回g_firstPageTable的地址
}

// 处理一级页表中无效项的解除映射
STATIC INLINE UINT32 OsUnmapL1Invalid(vaddr_t *vaddr, UINT32 *count)
{
    UINT32 unmapCount;  // 解除映射的数量

    // 计算可解除映射的页数：取剩余空间和请求数量的最小值
    unmapCount = MIN2((MMU_DESCRIPTOR_L1_SMALL_SIZE - (*vaddr % MMU_DESCRIPTOR_L1_SMALL_SIZE)) >>
        MMU_DESCRIPTOR_L2_SMALL_SHIFT, *count);
    *vaddr += unmapCount << MMU_DESCRIPTOR_L2_SMALL_SHIFT;  // 更新虚拟地址
    *count -= unmapCount;  // 更新剩余计数

    return unmapCount;  // 返回实际解除映射的数量
}

// 映射参数检查
STATIC INT32 OsMapParamCheck(UINT32 flags, VADDR_T vaddr, PADDR_T paddr)
{
#if !WITH_ARCH_MMU_PICK_SPOT  // 如果未启用MMU地址自动分配
    if (flags & VM_MAP_REGION_FLAG_NS) {  // 如果请求非安全内存
        /* 需要WITH_ARCH_MMU_PICK_SPOT才能支持NS内存 */
        LOS_Panic("NS mem is not supported\n");  // 触发断言
    }
#endif

    /* 物理地址和虚拟地址必须对齐 */
    if (!MMU_DESCRIPTOR_IS_L2_SIZE_ALIGNED(vaddr) || !MMU_DESCRIPTOR_IS_L2_SIZE_ALIGNED(paddr)) {
        return LOS_ERRNO_VM_INVALID_ARGS;  // 返回参数无效错误
    }

    return 0;  // 参数检查通过
}

// 将二级页表项属性转换为标志位
STATIC VOID OsCvtPte2AttsToFlags(PTE_T l1Entry, PTE_T l2Entry, UINT32 *flags)
{
    *flags = 0;  // 初始化标志位为0
    /* NS标志仅存在于L1页表项中 */
    if (l1Entry & MMU_DESCRIPTOR_L1_PAGETABLE_NON_SECURE) {  // 如果L1页表项设置了非安全位
        *flags |= VM_MAP_REGION_FLAG_NS;  // 设置非安全标志
    }

    // 根据L2页表项的内存类型设置缓存标志
    switch (l2Entry & MMU_DESCRIPTOR_L2_TEX_TYPE_MASK) {
        case MMU_DESCRIPTOR_L2_TYPE_STRONGLY_ORDERED:
            *flags |= VM_MAP_REGION_FLAG_STRONGLY_ORDERED;  // 强序内存
            break;
        case MMU_DESCRIPTOR_L2_TYPE_NORMAL_NOCACHE:
            *flags |= VM_MAP_REGION_FLAG_UNCACHED;  // 非缓存
            break;
        case MMU_DESCRIPTOR_L2_TYPE_DEVICE_SHARED:
        case MMU_DESCRIPTOR_L2_TYPE_DEVICE_NON_SHARED:
            *flags |= VM_MAP_REGION_FLAG_UNCACHED_DEVICE;  // 设备非缓存
            break;
        default:
            break;  // 其他类型不设置标志
    }

    *flags |= VM_MAP_REGION_FLAG_PERM_READ;  // 默认设置读权限

    // 根据访问权限位设置权限标志
    switch (l2Entry & MMU_DESCRIPTOR_L2_AP_MASK) {
        case MMU_DESCRIPTOR_L2_AP_P_RO_U_NA:  // 特权读，用户无权限
            break;  // 仅读权限已设置
        case MMU_DESCRIPTOR_L2_AP_P_RW_U_NA:  // 特权读写，用户无权限
            *flags |= VM_MAP_REGION_FLAG_PERM_WRITE;  // 添加写权限
            break;
        case MMU_DESCRIPTOR_L2_AP_P_RO_U_RO:  // 特权读，用户读
            *flags |= VM_MAP_REGION_FLAG_PERM_USER;  // 添加用户权限
            break;
        case MMU_DESCRIPTOR_L2_AP_P_RW_U_RW:  // 特权读写，用户读写
            *flags |= VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_WRITE;  // 添加用户和写权限
            break;
        default:
            break;
    }
    // 如果不是执行禁止，则设置执行权限
    if ((l2Entry & MMU_DESCRIPTOR_L2_TYPE_MASK) != MMU_DESCRIPTOR_L2_TYPE_SMALL_PAGE_XN) {
        *flags |= VM_MAP_REGION_FLAG_PERM_EXECUTE;  // 添加执行权限
    }
}

// 释放二级页表（如果不再被引用）
STATIC VOID OsPutL2Table(const LosArchMmu *archMmu, UINT32 l1Index, paddr_t l2Paddr)
{
    UINT32 index;  // 循环索引
    PTE_T ttEntry;  // 页表项
    /* 检查是否有任何L1页表项指向此L2页表 */
    for (index = 0; index < MMU_DESCRIPTOR_L1_SMALL_L2_TABLES_PER_PAGE; index++) {
        ttEntry = archMmu->virtTtb[ROUNDDOWN(l1Index, MMU_DESCRIPTOR_L1_SMALL_L2_TABLES_PER_PAGE) + index];
        if ((ttEntry &  MMU_DESCRIPTOR_L1_TYPE_MASK) == MMU_DESCRIPTOR_L1_TYPE_PAGE_TABLE) {
            return;  // 发现引用，不释放
        }
    }
#ifdef LOSCFG_KERNEL_VM  // 如果启用了内核虚拟内存
    /* 可以释放此L2页表 */
    LosVmPage *vmPage = LOS_VmPageGet(l2Paddr);  // 通过物理地址获取虚拟内存页
    if (vmPage == NULL) {  // 如果虚拟内存页不存在
        LOS_Panic("bad page table paddr %#x\n", l2Paddr);  // 触发断言
        return;
    }

    LOS_ListDelete(&vmPage->node);  // 从链表中删除该页
    LOS_PhysPageFree(vmPage);  // 释放物理页
#else
    (VOID)LOS_MemFree(OS_SYS_MEM_ADDR, LOS_PaddrToKVaddr(l2Paddr));  // 释放内存
#endif
}

// 尝试解除一级页表项映射（如果所有二级页表项都已解除）
STATIC VOID OsTryUnmapL1PTE(LosArchMmu *archMmu, PTE_T *l1Entry, vaddr_t vaddr, UINT32 scanIndex, UINT32 scanCount)
{
    /*
     * 检查与此L1页表项相关的所有页是否都已释放
     * 只需检查从scanIndex开始到SECTION结束的未清除页
     */
    UINT32 l1Index;  // L1页表索引
    PTE_T *pte2BasePtr = NULL;  // L2页表基地址指针
    SPIN_LOCK_S *pte1Lock = NULL;  // L1页表锁
    SPIN_LOCK_S *pte2Lock = NULL;  // L2页表锁
    UINT32 pte1IntSave;  // L1页表中断状态
    UINT32 pte2IntSave;  // L2页表中断状态
    PTE_T pte1Val;  // L1页表项值
    PADDR_T pte1Paddr;  // L1页表物理地址

    pte1Paddr = OsGetPte1Paddr(archMmu->physTtb, vaddr);  // 获取L1页表物理地址
    pte2Lock = OsGetPte2Lock(archMmu, *l1Entry, &pte2IntSave);  // 获取L2页表锁
    if (pte2Lock == NULL) {  // 如果锁获取失败
        return;  // 直接返回
    }
    pte2BasePtr = OsGetPte2BasePtr(*l1Entry);  // 获取L2页表基地址
    if (pte2BasePtr == NULL) {  // 如果基地址为空
        OsUnlockPte2(pte2Lock, pte2IntSave);  // 释放L2页表锁
        return;  // 返回
    }

    // 扫描剩余的页表项
    while (scanCount) {
        if (scanIndex == MMU_DESCRIPTOR_L2_NUMBERS_PER_L1) {  // 如果到达L2页表项数量上限
            scanIndex = 0;  // 重置索引
        }
        if (pte2BasePtr[scanIndex++]) {  // 如果页表项不为空
            break;  // 发现有效项，退出循环
        }
        scanCount--;  // 减少剩余计数
    }

    if (!scanCount) {  // 如果所有页表项都已清除
        /*
         * 内核进程的pte1在编译时放在内核镜像中，因此pte1Lock将为空
         * 不存在同时访问内核进程pte1的情况
         */
        pte1Lock = OsGetPte1LockTmp(archMmu, pte1Paddr, &pte1IntSave);  // 获取L1临时锁
        if (!OsIsPte1PageTable(*l1Entry)) {  // 如果不是页表类型
            OsUnlockPte1Tmp(pte1Lock, pte1IntSave);  // 释放L1临时锁
            OsUnlockPte2(pte2Lock, pte2IntSave);  // 释放L2锁
            return;  // 返回
        }
        pte1Val = *l1Entry;  // 保存当前L1页表项值
        /* 可以清除L1页表项 */
        OsClearPte1(l1Entry);  // 清除L1页表项
        l1Index = OsGetPte1Index(vaddr);  // 获取L1页表索引
        OsArmInvalidateTlbMvaNoBarrier(l1Index << MMU_DESCRIPTOR_L1_SMALL_SHIFT);  // 无效化TLB

        /* 尝试释放L2页表本身 */
        OsPutL2Table(archMmu, l1Index, MMU_DESCRIPTOR_L1_PAGE_TABLE_ADDR(pte1Val));  // 释放L2页表
        OsUnlockPte1Tmp(pte1Lock, pte1IntSave);  // 释放L1临时锁
    }
    OsUnlockPte2(pte2Lock, pte2IntSave);  // 释放L2锁
}

// 将段缓存标志转换为MMU标志
STATIC UINT32 OsCvtSecCacheFlagsToMMUFlags(UINT32 flags)
{
    UINT32 mmuFlags = 0;  // MMU标志

    // 根据缓存标志设置MMU缓存类型
    switch (flags & VM_MAP_REGION_FLAG_CACHE_MASK) {
        case VM_MAP_REGION_FLAG_CACHED:  // 缓存
            mmuFlags |= MMU_DESCRIPTOR_L1_TYPE_NORMAL_WRITE_BACK_ALLOCATE;  // 回写分配
#ifdef LOSCFG_KERNEL_SMP  // 如果支持SMP
            mmuFlags |= MMU_DESCRIPTOR_L1_SECTION_SHAREABLE;  // 设置共享
#endif
            break;
        case VM_MAP_REGION_FLAG_STRONGLY_ORDERED:  // 强序
            mmuFlags |= MMU_DESCRIPTOR_L1_TYPE_STRONGLY_ORDERED;  // 强序类型
            break;
        case VM_MAP_REGION_FLAG_UNCACHED:  // 非缓存
            mmuFlags |= MMU_DESCRIPTOR_L1_TYPE_NORMAL_NOCACHE;  // 普通非缓存
            break;
        case VM_MAP_REGION_FLAG_UNCACHED_DEVICE:  // 设备非缓存
            mmuFlags |= MMU_DESCRIPTOR_L1_TYPE_DEVICE_SHARED;  // 设备共享
            break;
        default:  // 无效缓存类型
            return LOS_ERRNO_VM_INVALID_ARGS;  // 返回错误
    }
    return mmuFlags;  // 返回MMU标志
}

// 将段访问权限标志转换为MMU标志
STATIC UINT32 OsCvtSecAccessFlagsToMMUFlags(UINT32 flags)
{
    UINT32 mmuFlags = 0;  // MMU标志

    // 根据访问权限组合设置MMU访问权限位
    switch (flags & (VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE)) {
        case 0:  // 无权限
            mmuFlags |= MMU_DESCRIPTOR_L1_AP_P_NA_U_NA;  // 特权无，用户无
            break;
        case VM_MAP_REGION_FLAG_PERM_READ:  // 只读
        case VM_MAP_REGION_FLAG_PERM_USER:  // 用户
            mmuFlags |= MMU_DESCRIPTOR_L1_AP_P_RO_U_NA;  // 特权读，用户无
            break;
        case VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_READ:  // 用户读
            mmuFlags |= MMU_DESCRIPTOR_L1_AP_P_RO_U_RO;  // 特权读，用户读
            break;
        case VM_MAP_REGION_FLAG_PERM_WRITE:  // 只写
        case VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE:  // 读写
            mmuFlags |= MMU_DESCRIPTOR_L1_AP_P_RW_U_NA;  // 特权读写，用户无
            break;
        case VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_WRITE:  // 用户写
        case VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE:  // 用户读写
            mmuFlags |= MMU_DESCRIPTOR_L1_AP_P_RW_U_RW;  // 特权读写，用户读写
            break;
        default:  // 其他组合
            break;
    }
    return mmuFlags;  // 返回MMU标志
}

/* 将用户级MMU标志转换为L1描述符标志 */
STATIC UINT32 OsCvtSecFlagsToAttrs(UINT32 flags)
{
    UINT32 mmuFlags;  // MMU标志

    mmuFlags = OsCvtSecCacheFlagsToMMUFlags(flags);  // 转换缓存标志
    if (mmuFlags == LOS_ERRNO_VM_INVALID_ARGS) {  // 如果缓存标志无效
        return mmuFlags;  // 返回错误
    }

    mmuFlags |= MMU_DESCRIPTOR_L1_SMALL_DOMAIN_CLIENT;  // 设置域为客户端

    mmuFlags |= OsCvtSecAccessFlagsToMMUFlags(flags);  // 转换访问权限标志

    if (!(flags & VM_MAP_REGION_FLAG_PERM_EXECUTE)) {  // 如果没有执行权限
        mmuFlags |= MMU_DESCRIPTOR_L1_SECTION_XN;  // 设置执行禁止
    }

    if (flags & VM_MAP_REGION_FLAG_NS) {  // 如果需要非安全内存
        mmuFlags |= MMU_DESCRIPTOR_L1_SECTION_NON_SECURE;  // 设置非安全位
    }

    if (flags & VM_MAP_REGION_FLAG_PERM_USER) {  // 如果需要用户权限
        mmuFlags |= MMU_DESCRIPTOR_L1_SECTION_NON_GLOBAL;  // 设置非全局位
    }

    return mmuFlags;  // 返回完整的MMU标志
}

// 将段页表项属性转换为标志位
STATIC VOID OsCvtSecAttsToFlags(PTE_T l1Entry, UINT32 *flags)
{
    *flags = 0;  // 初始化标志位
    if (l1Entry & MMU_DESCRIPTOR_L1_SECTION_NON_SECURE) {  // 如果是非安全段
        *flags |= VM_MAP_REGION_FLAG_NS;  // 设置非安全标志
    }

    // 根据内存类型设置缓存标志
    switch (l1Entry & MMU_DESCRIPTOR_L1_TEX_TYPE_MASK) {
        case MMU_DESCRIPTOR_L1_TYPE_STRONGLY_ORDERED:  // 强序
            *flags |= VM_MAP_REGION_FLAG_STRONGLY_ORDERED;  // 设置强序标志
            break;
        case MMU_DESCRIPTOR_L1_TYPE_NORMAL_NOCACHE:  // 普通非缓存
            *flags |= VM_MAP_REGION_FLAG_UNCACHED;  // 设置非缓存标志
            break;
        case MMU_DESCRIPTOR_L1_TYPE_DEVICE_SHARED:  // 设备共享
        case MMU_DESCRIPTOR_L1_TYPE_DEVICE_NON_SHARED:  // 设备非共享
            *flags |= VM_MAP_REGION_FLAG_UNCACHED_DEVICE;  // 设置设备非缓存标志
            break;
        default:  // 其他类型
            break;
    }

    *flags |= VM_MAP_REGION_FLAG_PERM_READ;  // 默认设置读权限

    // 根据访问权限位设置权限标志
    switch (l1Entry & MMU_DESCRIPTOR_L1_AP_MASK) {
        case MMU_DESCRIPTOR_L1_AP_P_RO_U_NA:  // 特权读，用户无
            break;  // 仅读权限已设置
        case MMU_DESCRIPTOR_L1_AP_P_RW_U_NA:  // 特权读写，用户无
            *flags |= VM_MAP_REGION_FLAG_PERM_WRITE;  // 添加写权限
            break;
        case MMU_DESCRIPTOR_L1_AP_P_RO_U_RO:  // 特权读，用户读
            *flags |= VM_MAP_REGION_FLAG_PERM_USER;  // 添加用户权限
            break;
        case MMU_DESCRIPTOR_L1_AP_P_RW_U_RW:  // 特权读写，用户读写
            *flags |= VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_WRITE;  // 添加用户和写权限
            break;
        default:  // 其他权限
            break;
    }

    if (!(l1Entry & MMU_DESCRIPTOR_L1_SECTION_XN)) {  // 如果不是执行禁止
        *flags |= VM_MAP_REGION_FLAG_PERM_EXECUTE;  // 添加执行权限
    }
}

// 解除二级页表项映射
STATIC UINT32 OsUnmapL2PTE(LosArchMmu *archMmu, PTE_T *pte1, vaddr_t vaddr, UINT32 *count)
{
    UINT32 unmapCount;  // 解除映射的数量
    UINT32 pte2Index;  // 二级页表索引
    UINT32 intSave;  // 中断状态
    PTE_T *pte2BasePtr = NULL;  // 二级页表基地址
    SPIN_LOCK_S *lock = NULL;  // 锁

    pte2Index = OsGetPte2Index(vaddr);  // 获取二级页表索引
    // 计算可解除映射的页数：取剩余空间和请求数量的最小值
    unmapCount = MIN2(MMU_DESCRIPTOR_L2_NUMBERS_PER_L1 - pte2Index, *count);

    lock = OsGetPte2Lock(archMmu, *pte1, &intSave);  // 获取二级页表锁
    if (lock == NULL) {  // 如果锁获取失败
        return unmapCount;  // 返回计算的数量
    }

    pte2BasePtr = OsGetPte2BasePtr(*pte1);  // 获取二级页表基地址
    if (pte2BasePtr == NULL) {  // 如果基地址为空
        OsUnlockPte2(lock, intSave);  // 释放锁
        return unmapCount;  // 返回计算的数量
    }

    /* 解除页表项映射 */
    OsClearPte2Continuous(&pte2BasePtr[pte2Index], unmapCount);  // 连续清除页表项

    /* 无效化TLB */
    OsArmInvalidateTlbMvaRangeNoBarrier(vaddr, unmapCount);  // 无效化TLB范围
    OsUnlockPte2(lock, intSave);  // 释放锁

    *count -= unmapCount;  // 更新剩余计数
    return unmapCount;  // 返回实际解除映射的数量
}

// 解除段映射
STATIC UINT32 OsUnmapSection(LosArchMmu *archMmu, PTE_T *l1Entry, vaddr_t *vaddr, UINT32 *count)
{
    UINT32 intSave;  // 中断状态
    PADDR_T pte1Paddr;  // 一级页表物理地址
    SPIN_LOCK_S *lock = NULL;  // 锁

    pte1Paddr = OsGetPte1Paddr(archMmu->physTtb, *vaddr);  // 获取一级页表物理地址
    lock = OsGetPte1Lock(archMmu, pte1Paddr, &intSave);  // 获取一级页表锁
    if (!OsIsPte1Section(*l1Entry)) {  // 如果不是段类型
        OsUnlockPte1(lock, intSave);  // 释放锁
        return 0;  // 返回0
    }
    OsClearPte1(OsGetPte1Ptr((PTE_T *)archMmu->virtTtb, *vaddr));  // 清除一级页表项
    OsArmInvalidateTlbMvaNoBarrier(*vaddr);  // 无效化TLB
    OsUnlockPte1(lock, intSave);  // 释放锁

    *vaddr += MMU_DESCRIPTOR_L1_SMALL_SIZE;  // 更新虚拟地址
    *count -= MMU_DESCRIPTOR_L2_NUMBERS_PER_L1;  // 更新剩余计数

    return MMU_DESCRIPTOR_L2_NUMBERS_PER_L1;  // 返回解除映射的页数
}

// 初始化架构相关MMU结构
BOOL OsArchMmuInit(LosArchMmu *archMmu, VADDR_T *virtTtb)
{
#ifdef LOSCFG_KERNEL_VM  // 如果启用了内核虚拟内存
    if (OsAllocAsid(&archMmu->asid) != LOS_OK) {  // 分配ASID失败
        VM_ERR("alloc arch mmu asid failed");  // 打印错误信息
        return FALSE;  // 返回失败
    }
#endif

#ifndef LOSCFG_PAGE_TABLE_FINE_LOCK  // 如果未启用页表细粒度锁
    LOS_SpinInit(&archMmu->lock);  // 初始化自旋锁
#endif
    LOS_ListInit(&archMmu->ptList);  // 初始化页表链表
    archMmu->virtTtb = virtTtb;  // 设置虚拟页表基地址
    // 计算物理页表基地址：虚拟地址 - 内核空间基地址 + 系统内存基地址
    archMmu->physTtb = (VADDR_T)(UINTPTR)virtTtb - KERNEL_ASPACE_BASE + SYS_MEM_BASE;
    return TRUE;  // 初始化成功
}

// 查询MMU映射关系
STATUS_T LOS_ArchMmuQuery(const LosArchMmu *archMmu, VADDR_T vaddr, PADDR_T *paddr, UINT32 *flags)
{
    PTE_T l1Entry = OsGetPte1(archMmu->virtTtb, vaddr);  // 获取一级页表项
    PTE_T l2Entry;  // 二级页表项
    PTE_T* l2Base = NULL;  // 二级页表基地址

    if (OsIsPte1Invalid(l1Entry)) {  // 如果一级页表项无效
        return LOS_ERRNO_VM_NOT_FOUND;  // 返回未找到错误
    } else if (OsIsPte1Section(l1Entry)) {  // 如果是段映射
        if (paddr != NULL) {  // 如果需要返回物理地址
            // 计算物理地址：段基地址 + 页内偏移
            *paddr = MMU_DESCRIPTOR_L1_SECTION_ADDR(l1Entry) + (vaddr & (MMU_DESCRIPTOR_L1_SMALL_SIZE - 1));
        }

        if (flags != NULL) {  // 如果需要返回标志位
            OsCvtSecAttsToFlags(l1Entry, flags);  // 转换段属性为标志位
        }
    } else if (OsIsPte1PageTable(l1Entry)) {  // 如果是页表映射
        l2Base = OsGetPte2BasePtr(l1Entry);  // 获取二级页表基地址
        if (l2Base == NULL) {  // 如果二级页表基地址为空
            return LOS_ERRNO_VM_NOT_FOUND;  // 返回未找到错误
        }
        l2Entry = OsGetPte2(l2Base, vaddr);  // 获取二级页表项
        if (OsIsPte2SmallPage(l2Entry) || OsIsPte2SmallPageXN(l2Entry)) {  // 如果是小页或小页执行禁止
            if (paddr != NULL) {  // 如果需要返回物理地址
                // 计算物理地址：页基地址 + 页内偏移
                *paddr = MMU_DESCRIPTOR_L2_SMALL_PAGE_ADDR(l2Entry) + (vaddr & (MMU_DESCRIPTOR_L2_SMALL_SIZE - 1));
            }

            if (flags != NULL) {  // 如果需要返回标志位
                OsCvtPte2AttsToFlags(l1Entry, l2Entry, flags);  // 转换页属性为标志位
            }
        } else if (OsIsPte2LargePage(l2Entry)) {  // 如果是大页
            LOS_Panic("%s %d, large page unimplemented\n", __FUNCTION__, __LINE__);  // 大页未实现
        } else {  // 无效页表项
            return LOS_ERRNO_VM_NOT_FOUND;  // 返回未找到错误
        }
    }

    return LOS_OK;  // 查询成功
}

// 解除MMU映射
STATUS_T LOS_ArchMmuUnmap(LosArchMmu *archMmu, VADDR_T vaddr, size_t count)
{
    PTE_T *l1Entry = NULL;  // 一级页表项指针
    INT32 unmapped = 0;  // 已解除映射的数量
    UINT32 unmapCount = 0;  // 本次解除映射的数量
    INT32 tryTime = TRY_MAX_TIMES;  // 尝试次数

    while (count > 0) {  // 当还有需要解除映射的页数时
        l1Entry = OsGetPte1Ptr(archMmu->virtTtb, vaddr);  // 获取一级页表项指针
        if (OsIsPte1Invalid(*l1Entry)) {  // 如果一级页表项无效
            unmapCount = OsUnmapL1Invalid(&vaddr, &count);  // 处理无效项
        } else if (OsIsPte1Section(*l1Entry)) {  // 如果是段映射
            // 如果地址对齐且数量足够
            if (MMU_DESCRIPTOR_IS_L1_SIZE_ALIGNED(vaddr) && count >= MMU_DESCRIPTOR_L2_NUMBERS_PER_L1) {
                unmapCount = OsUnmapSection(archMmu, l1Entry, &vaddr, &count);  // 解除段映射
            } else {  // 未对齐或数量不足
                LOS_Panic("%s %d, unimplemented\n", __FUNCTION__, __LINE__);  // 未实现
            }
        } else if (OsIsPte1PageTable(*l1Entry)) {  // 如果是页表映射
            unmapCount = OsUnmapL2PTE(archMmu, l1Entry, vaddr, &count);  // 解除二级页表项映射
            // 尝试解除一级页表项映射
            OsTryUnmapL1PTE(archMmu, l1Entry, vaddr, OsGetPte2Index(vaddr) + unmapCount,
                            MMU_DESCRIPTOR_L2_NUMBERS_PER_L1);
            vaddr += unmapCount << MMU_DESCRIPTOR_L2_SMALL_SHIFT;  // 更新虚拟地址
        } else {  // 其他类型
            LOS_Panic("%s %d, unimplemented\n", __FUNCTION__, __LINE__);  // 未实现
        }
        tryTime = (unmapCount == 0) ? (tryTime - 1) : tryTime;  // 如果未解除映射，减少尝试次数
        if (tryTime == 0) {  // 如果尝试次数用尽
            return LOS_ERRNO_VM_FAULT;  // 返回故障错误
        }
        unmapped += unmapCount;  // 累加已解除映射的数量
    }
    OsArmInvalidateTlbBarrier();  // 执行TLB无效化屏障
    return unmapped;  // 返回已解除映射的总数量
}

// 映射段
STATIC UINT32 OsMapSection(MmuMapInfo *mmuMapInfo, UINT32 *count)
{
    UINT32 mmuFlags = 0;  // MMU标志
    UINT32 intSave;  // 中断状态
    PADDR_T pte1Paddr;  // 一级页表物理地址
    SPIN_LOCK_S *lock = NULL;  // 锁

    mmuFlags |= OsCvtSecFlagsToAttrs(*mmuMapInfo->flags);  // 转换段标志为MMU属性
    pte1Paddr = OsGetPte1Paddr(mmuMapInfo->archMmu->physTtb, *mmuMapInfo->vaddr);  // 获取一级页表物理地址
    lock = OsGetPte1Lock(mmuMapInfo->archMmu, pte1Paddr, &intSave);  // 获取一级页表锁
    // 保存一级页表项：物理地址 | MMU标志 | 段类型
    OsSavePte1(OsGetPte1Ptr(mmuMapInfo->archMmu->virtTtb, *mmuMapInfo->vaddr),
        OsTruncPte1(*mmuMapInfo->paddr) | mmuFlags | MMU_DESCRIPTOR_L1_TYPE_SECTION);
    OsUnlockPte1(lock, intSave);  // 释放锁
    *count -= MMU_DESCRIPTOR_L2_NUMBERS_PER_L1;  // 更新剩余计数
    *mmuMapInfo->vaddr += MMU_DESCRIPTOR_L1_SMALL_SIZE;  // 更新虚拟地址
    *mmuMapInfo->paddr += MMU_DESCRIPTOR_L1_SMALL_SIZE;  // 更新物理地址

    return MMU_DESCRIPTOR_L2_NUMBERS_PER_L1;  // 返回映射的页数
}

// 获取二级页表
STATIC STATUS_T OsGetL2Table(LosArchMmu *archMmu, UINT32 l1Index, paddr_t *ppa)
{
    UINT32 index;  // 循环索引
    PTE_T ttEntry;  // 页表项
    VADDR_T *kvaddr = NULL;  // 内核虚拟地址
    // 计算L2页表内偏移
    UINT32 l2Offset = (MMU_DESCRIPTOR_L2_SMALL_SIZE / MMU_DESCRIPTOR_L1_SMALL_L2_TABLES_PER_PAGE) *
        (l1Index & (MMU_DESCRIPTOR_L1_SMALL_L2_TABLES_PER_PAGE - 1));
    /* 查找已存在的L2页表 */
    for (index = 0; index < MMU_DESCRIPTOR_L1_SMALL_L2_TABLES_PER_PAGE; index++) {
        ttEntry = archMmu->virtTtb[ROUNDDOWN(l1Index, MMU_DESCRIPTOR_L1_SMALL_L2_TABLES_PER_PAGE) + index];
        if ((ttEntry & MMU_DESCRIPTOR_L1_TYPE_MASK) == MMU_DESCRIPTOR_L1_TYPE_PAGE_TABLE) {  // 如果是页表类型
            // 计算物理地址：页表基地址 + 偏移
            *ppa = (PADDR_T)ROUNDDOWN(MMU_DESCRIPTOR_L1_PAGE_TABLE_ADDR(ttEntry), MMU_DESCRIPTOR_L2_SMALL_SIZE) +
                l2Offset;
            return LOS_OK;  // 成功找到
        }
    }

#ifdef LOSCFG_KERNEL_VM  // 如果启用了内核虚拟内存
    /* 未找到：分配一个（物理地址） */
    LosVmPage *vmPage = LOS_PhysPageAlloc();  // 分配物理页
    if (vmPage == NULL) {  // 如果分配失败
        VM_ERR("have no memory to save l2 page");  // 打印错误信息
        return LOS_ERRNO_VM_NO_MEMORY;  // 返回内存不足错误
    }
    LOS_ListAdd(&archMmu->ptList, &vmPage->node);  // 将页添加到页表链表
    kvaddr = OsVmPageToVaddr(vmPage);  // 将物理页转换为虚拟地址
#else
    kvaddr = LOS_MemAlloc(OS_SYS_MEM_ADDR, MMU_DESCRIPTOR_L2_SMALL_SIZE);  // 分配内存
    if (kvaddr == NULL) {  // 如果分配失败
        VM_ERR("have no memory to save l2 page");  // 打印错误信息
        return LOS_ERRNO_VM_NO_MEMORY;  // 返回内存不足错误
    }
#endif
    // 初始化页表内存为0
    (VOID)memset_s(kvaddr, MMU_DESCRIPTOR_L2_SMALL_SIZE, 0, MMU_DESCRIPTOR_L2_SMALL_SIZE);

    /* 获取物理地址 */
    *ppa = OsKVaddrToPaddr((VADDR_T)kvaddr) + l2Offset;  // 计算物理地址
    return LOS_OK;  // 成功获取
}

// 将二级页表缓存标志转换为MMU标志
STATIC UINT32 OsCvtPte2CacheFlagsToMMUFlags(UINT32 flags)
{
    UINT32 mmuFlags = 0;  // MMU标志

    // 根据缓存标志设置MMU缓存类型
    switch (flags & VM_MAP_REGION_FLAG_CACHE_MASK) {
        case VM_MAP_REGION_FLAG_CACHED:  // 缓存
#ifdef LOSCFG_KERNEL_SMP  // 如果支持SMP
            mmuFlags |= MMU_DESCRIPTOR_L2_SHAREABLE;  // 设置共享
#endif
            mmuFlags |= MMU_DESCRIPTOR_L2_TYPE_NORMAL_WRITE_BACK_ALLOCATE;  // 回写分配
            break;
        case VM_MAP_REGION_FLAG_STRONGLY_ORDERED:  // 强序
            mmuFlags |= MMU_DESCRIPTOR_L2_TYPE_STRONGLY_ORDERED;  // 强序类型
            break;
        case VM_MAP_REGION_FLAG_UNCACHED:  // 非缓存
            mmuFlags |= MMU_DESCRIPTOR_L2_TYPE_NORMAL_NOCACHE;  // 普通非缓存
            break;
        case VM_MAP_REGION_FLAG_UNCACHED_DEVICE:  // 设备非缓存
            mmuFlags |= MMU_DESCRIPTOR_L2_TYPE_DEVICE_SHARED;  // 设备共享
            break;
        default:  // 无效缓存类型
            return LOS_ERRNO_VM_INVALID_ARGS;  // 返回错误
    }
    return mmuFlags;  // 返回MMU标志
}

/**
 * @brief 将访问权限标志转换为MMU页表项标志
 * @param flags 访问权限标志，包含用户/内核权限、读写权限组合
 * @return MMU页表项中的访问控制位值
 */
STATIC UINT32 OsCvtPte2AccessFlagsToMMUFlags(UINT32 flags)
{
    UINT32 mmuFlags = 0;  // MMU标志变量，用于存储转换后的结果

    // 根据权限标志组合设置对应的MMU访问控制位
    switch (flags & (VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE)) {
        case 0:  // 无任何权限
            mmuFlags |= MMU_DESCRIPTOR_L1_AP_P_NA_U_NA;  // 特权级不可访问，用户级不可访问
            break;
        case VM_MAP_REGION_FLAG_PERM_READ:  // 仅特权级读权限
        case VM_MAP_REGION_FLAG_PERM_USER:  // 仅用户级权限(无读写权限，实际应为无效组合)
            mmuFlags |= MMU_DESCRIPTOR_L2_AP_P_RO_U_NA;  // 特权级只读，用户级不可访问
            break;
        case VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_READ:  // 用户级读权限
            mmuFlags |= MMU_DESCRIPTOR_L2_AP_P_RO_U_RO;  // 特权级只读，用户级只读
            break;
        case VM_MAP_REGION_FLAG_PERM_WRITE:  // 仅特权级写权限
        case VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE:  // 特权级读写权限
            mmuFlags |= MMU_DESCRIPTOR_L2_AP_P_RW_U_NA;  // 特权级读写，用户级不可访问
            break;
        case VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_WRITE:  // 用户级写权限
        case VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE:  // 用户级读写权限
            mmuFlags |= MMU_DESCRIPTOR_L2_AP_P_RW_U_RW;  // 特权级读写，用户级读写
            break;
        default:  // 其他未定义权限组合
            break;
    }
    return mmuFlags;  // 返回转换后的MMU访问控制位
}

/**
 * @brief 将用户级MMU标志转换为L2页表描述符属性
 * @param flags 用户级MMU标志，包含缓存策略、访问权限、执行权限等
 * @return 完整的L2页表项属性值，失败时返回错误码
 */
/* convert user level mmu flags to L2 descriptors flags */
STATIC UINT32 OsCvtPte2FlagsToAttrs(UINT32 flags)
{
    UINT32 mmuFlags;  // MMU标志变量，用于存储转换后的结果

    mmuFlags = OsCvtPte2CacheFlagsToMMUFlags(flags);  // 转换缓存策略标志
    if (mmuFlags == LOS_ERRNO_VM_INVALID_ARGS) {  // 检查缓存标志转换是否失败
        return mmuFlags;  // 返回错误码
    }

    mmuFlags |= OsCvtPte2AccessFlagsToMMUFlags(flags);  // 叠加访问权限标志

    if (!(flags & VM_MAP_REGION_FLAG_PERM_EXECUTE)) {  // 如果没有执行权限
        mmuFlags |= MMU_DESCRIPTOR_L2_TYPE_SMALL_PAGE_XN;  // 设置XN(禁止执行)位
    } else {  // 有执行权限
        mmuFlags |= MMU_DESCRIPTOR_L2_TYPE_SMALL_PAGE;  // 标准小页类型(允许执行)
    }

    if (flags & VM_MAP_REGION_FLAG_PERM_USER) {  // 如果是用户空间映射
        mmuFlags |= MMU_DESCRIPTOR_L2_NON_GLOBAL;  // 设置非全局位(进程私有)
    }

    return mmuFlags;  // 返回完整的L2页表项属性
}

/**
 * @brief 映射L1页表项并分配L2页表
 * @param mmuMapInfo MMU映射信息结构体指针
 * @param l1Entry L1页表项指针
 * @param count 剩余需要映射的页数指针
 * @return 成功映射的页数
 */
STATIC UINT32 OsMapL1PTE(MmuMapInfo *mmuMapInfo, PTE_T *l1Entry, UINT32 *count)
{
    PADDR_T pte2Base = 0;  // L2页表物理基地址
    PADDR_T pte1Paddr;  // L1页表项物理地址
    SPIN_LOCK_S *pte1Lock = NULL;  // L1页表自旋锁
    SPIN_LOCK_S *pte2Lock = NULL;  // L2页表自旋锁
    PTE_T *pte2BasePtr = NULL;  // L2页表虚拟地址指针
    UINT32 saveCounts, archFlags, pte1IntSave, pte2IntSave;  // 临时变量：保存的页数、架构标志、中断状态

    pte1Paddr = OsGetPte1Paddr(mmuMapInfo->archMmu->physTtb, *mmuMapInfo->vaddr);  // 获取L1页表项物理地址
    pte1Lock = OsGetPte1Lock(mmuMapInfo->archMmu, pte1Paddr, &pte1IntSave);  // 获取L1页表锁并禁止中断
    if (!OsIsPte1Invalid(*l1Entry)) {  // 检查L1页表项是否已映射
        OsUnlockPte1(pte1Lock, pte1IntSave);  // 解锁并恢复中断
        return 0;  // 已映射，返回0页
    }
    if (OsGetL2Table(mmuMapInfo->archMmu, OsGetPte1Index(*mmuMapInfo->vaddr), &pte2Base) != LOS_OK) {  // 分配L2页表
        LOS_Panic("%s %d, failed to allocate pagetable\n", __FUNCTION__, __LINE__);  // 分配失败，系统 panic
    }

    *l1Entry = pte2Base | MMU_DESCRIPTOR_L1_TYPE_PAGE_TABLE;  // 设置L1页表项为页表类型
    if (*mmuMapInfo->flags & VM_MAP_REGION_FLAG_NS) {  // 如果需要非安全模式
        *l1Entry |= MMU_DESCRIPTOR_L1_PAGETABLE_NON_SECURE;  // 设置非安全位
    }
    *l1Entry &= MMU_DESCRIPTOR_L1_SMALL_DOMAIN_MASK;  // 清除域位
    *l1Entry |= MMU_DESCRIPTOR_L1_SMALL_DOMAIN_CLIENT;  // 设置域为客户端模式
    OsSavePte1(OsGetPte1Ptr(mmuMapInfo->archMmu->virtTtb, *mmuMapInfo->vaddr), *l1Entry);  // 保存L1页表项
    OsUnlockPte1(pte1Lock, pte1IntSave);  // 解锁L1页表并恢复中断

    pte2Lock = OsGetPte2Lock(mmuMapInfo->archMmu, *l1Entry, &pte2IntSave);  // 获取L2页表锁并禁止中断
    if (pte2Lock == NULL) {  // 检查锁是否获取成功
        LOS_Panic("pte2 should not be null!\n");  // 失败，系统 panic
    }
    pte2BasePtr = (PTE_T *)LOS_PaddrToKVaddr(pte2Base);  // 将L2页表物理地址转换为内核虚拟地址

    /* compute the arch flags for L2 4K pages */
    archFlags = OsCvtPte2FlagsToAttrs(*mmuMapInfo->flags);  // 计算L2页表项架构标志
    saveCounts = OsSavePte2Continuous(pte2BasePtr, OsGetPte2Index(*mmuMapInfo->vaddr), *mmuMapInfo->paddr | archFlags,
                                      *count);  // 连续保存L2页表项
    OsUnlockPte2(pte2Lock, pte2IntSave);  // 解锁L2页表并恢复中断
    *mmuMapInfo->paddr += (saveCounts << MMU_DESCRIPTOR_L2_SMALL_SHIFT);  // 更新物理地址（左移12位=乘以4KB）
    *mmuMapInfo->vaddr += (saveCounts << MMU_DESCRIPTOR_L2_SMALL_SHIFT);  // 更新虚拟地址
    *count -= saveCounts;  // 更新剩余需要映射的页数
    return saveCounts;  // 返回成功映射的页数
}

/**
 * @brief 在已存在的L2页表中映射连续物理页
 * @param mmuMapInfo MMU映射信息结构体指针
 * @param pte1 L1页表项指针
 * @param count 剩余需要映射的页数指针
 * @return 成功映射的页数
 */
STATIC UINT32 OsMapL2PageContinous(MmuMapInfo *mmuMapInfo, PTE_T *pte1, UINT32 *count)
{
    PTE_T *pte2BasePtr = NULL;  // L2页表虚拟地址指针
    UINT32 archFlags;  // 架构相关标志
    UINT32 saveCounts;  // 成功保存的页表项数量
    UINT32 intSave;  // 中断状态保存
    SPIN_LOCK_S *lock = NULL;  // 自旋锁指针

    lock = OsGetPte2Lock(mmuMapInfo->archMmu, *pte1, &intSave);  // 获取L2页表锁并禁止中断
    if (lock == NULL) {  // 检查锁是否获取成功
        return 0;  // 失败，返回0
    }
    pte2BasePtr = OsGetPte2BasePtr(*pte1);  // 从L1页表项获取L2页表基地址
    if (pte2BasePtr == NULL) {  // 检查L2页表基地址是否有效
        OsUnlockPte2(lock, intSave);  // 解锁并恢复中断
        return 0;  // 无效，返回0
    }

    /* compute the arch flags for L2 4K pages */
    archFlags = OsCvtPte2FlagsToAttrs(*mmuMapInfo->flags);  // 计算L2页表项架构标志
    saveCounts = OsSavePte2Continuous(pte2BasePtr, OsGetPte2Index(*mmuMapInfo->vaddr), *mmuMapInfo->paddr | archFlags,
                                      *count);  // 连续保存L2页表项
    OsUnlockPte2(lock, intSave);  // 解锁L2页表并恢复中断
    *mmuMapInfo->paddr += (saveCounts << MMU_DESCRIPTOR_L2_SMALL_SHIFT);  // 更新物理地址
    *mmuMapInfo->vaddr += (saveCounts << MMU_DESCRIPTOR_L2_SMALL_SHIFT);  // 更新虚拟地址
    *count -= saveCounts;  // 更新剩余需要映射的页数
    return saveCounts;  // 返回成功映射的页数
}

/**
 * @brief MMU地址映射核心函数，支持大页(section)和小页(page)映射
 * @param archMmu MMU架构信息结构体指针
 * @param vaddr 虚拟地址起始
 * @param paddr 物理地址起始
 * @param count 需要映射的页数(4KB为单位)
 * @param flags 映射标志(权限、缓存策略等)
 * @return 成功映射的页数，失败返回负数错误码
 */
status_t LOS_ArchMmuMap(LosArchMmu *archMmu, VADDR_T vaddr, PADDR_T paddr, size_t count, UINT32 flags)
{
    PTE_T *l1Entry = NULL;  // L1页表项指针
    UINT32 saveCounts = 0;  // 单次映射的页数
    INT32 mapped = 0;  // 总映射页数
    INT32 tryTime = TRY_MAX_TIMES;  // 最大尝试次数，防止死循环
    INT32 checkRst;  // 参数检查结果
    MmuMapInfo mmuMapInfo = {  // MMU映射信息结构体初始化
        .archMmu = archMmu,
        .vaddr = &vaddr,
        .paddr = &paddr,
        .flags = &flags,
    };

    checkRst = OsMapParamCheck(flags, vaddr, paddr);  // 检查映射参数合法性
    if (checkRst < 0) {  // 参数不合法
        return checkRst;  // 返回错误码
    }

    /* see what kind of mapping we can use */
    while (count > 0) {  // 循环映射直到所有页完成
        // 检查是否满足大页映射条件：虚拟地址、物理地址均1MB对齐，且剩余页数>=256(1MB/4KB)
        if (MMU_DESCRIPTOR_IS_L1_SIZE_ALIGNED(*mmuMapInfo.vaddr) &&
            MMU_DESCRIPTOR_IS_L1_SIZE_ALIGNED(*mmuMapInfo.paddr) &&
            count >= MMU_DESCRIPTOR_L2_NUMBERS_PER_L1) {
            /* compute the arch flags for L1 sections cache, r ,w ,x, domain and type */
            saveCounts = OsMapSection(&mmuMapInfo, &count);  // 执行大页映射
        } else {  // 小页映射
            /* have to use a L2 mapping, we only allocate 4KB for L1, support 0 ~ 1GB */
            l1Entry = OsGetPte1Ptr(archMmu->virtTtb, *mmuMapInfo.vaddr);  // 获取L1页表项指针
            if (OsIsPte1Invalid(*l1Entry)) {  // L1页表项未映射
                saveCounts = OsMapL1PTE(&mmuMapInfo, l1Entry, &count);  // 分配L2页表并映射
            } else if (OsIsPte1PageTable(*l1Entry)) {  // L1页表项已指向L2页表
                saveCounts = OsMapL2PageContinous(&mmuMapInfo, l1Entry, &count);  // 连续映射L2页
            } else {  // L1页表项为其他类型(如大页)，不支持
                LOS_Panic("%s %d, unimplemented tt_entry %x\n", __FUNCTION__, __LINE__, l1Entry);  // 系统 panic
            }
        }
        mapped += saveCounts;  // 累加映射页数
        tryTime = (saveCounts == 0) ? (tryTime - 1) : tryTime;  // 若未映射成功，减少尝试次数
        if (tryTime == 0) {  // 达到最大尝试次数
            return LOS_ERRNO_VM_TIMED_OUT;  // 返回超时错误
        }
    }

    return mapped;  // 返回总映射页数
}

/**
 * @brief 修改虚拟地址区域的访问权限
 * @param archMmu MMU架构信息结构体指针
 * @param vaddr 虚拟地址起始
 * @param count 需要修改的页数(4KB为单位)
 * @param flags 新的访问权限标志
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATUS_T LOS_ArchMmuChangeProt(LosArchMmu *archMmu, VADDR_T vaddr, size_t count, UINT32 flags)
{
    STATUS_T status;  // 操作状态
    PADDR_T paddr = 0;  // 物理地址

    if ((archMmu == NULL) || (vaddr == 0) || (count == 0)) {  // 参数合法性检查
        VM_ERR("invalid args: archMmu %p, vaddr %p, count %d", archMmu, vaddr, count);  // 打印错误信息
        return LOS_NOK;  // 返回错误
    }

    while (count > 0) {  // 循环处理每一页
        count--;  // 递减剩余页数
        status = LOS_ArchMmuQuery(archMmu, vaddr, &paddr, NULL);  // 查询当前映射的物理地址
        if (status != LOS_OK) {  // 查询失败
            vaddr += MMU_DESCRIPTOR_L2_SMALL_SIZE;  // 移动到下一页
            continue;  // 跳过当前页
        }

        status = LOS_ArchMmuUnmap(archMmu, vaddr, 1);  // 解除当前映射
        if (status < 0) {  // 解除映射失败
            VM_ERR("invalid args:aspace %p, vaddr %p, count %d", archMmu, vaddr, count);  // 打印错误
            return LOS_NOK;  // 返回错误
        }

        status = LOS_ArchMmuMap(archMmu, vaddr, paddr, 1, flags);  // 使用新权限重新映射
        if (status < 0) {  // 重新映射失败
            VM_ERR("invalid args:aspace %p, vaddr %p, count %d",
                   archMmu, vaddr, count);  // 打印错误
            return LOS_NOK;  // 返回错误
        }
        vaddr += MMU_DESCRIPTOR_L2_SMALL_SIZE;  // 移动到下一页
    }
    return LOS_OK;  // 全部完成，返回成功
}

/**
 * @brief 将虚拟地址区域从旧地址移动到新地址
 * @param archMmu MMU架构信息结构体指针
 * @param oldVaddr 旧虚拟地址起始
 * @param newVaddr 新虚拟地址起始
 * @param count 需要移动的页数(4KB为单位)
 * @param flags 新映射的标志
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATUS_T LOS_ArchMmuMove(LosArchMmu *archMmu, VADDR_T oldVaddr, VADDR_T newVaddr, size_t count, UINT32 flags)
{
    STATUS_T status;  // 操作状态
    PADDR_T paddr = 0;  // 物理地址

    if ((archMmu == NULL) || (oldVaddr == 0) || (newVaddr == 0) || (count == 0)) {  // 参数合法性检查
        VM_ERR("invalid args: archMmu %p, oldVaddr %p, newVaddr %p, count %d",
               archMmu, oldVaddr, newVaddr, count);  // 打印错误
        return LOS_NOK;  // 返回错误
    }

    while (count > 0) {  // 循环处理每一页
        count--;  // 递减剩余页数
        status = LOS_ArchMmuQuery(archMmu, oldVaddr, &paddr, NULL);  // 查询旧地址对应的物理地址
        if (status != LOS_OK) {  // 查询失败
            oldVaddr += MMU_DESCRIPTOR_L2_SMALL_SIZE;  // 移动到下一页
            newVaddr += MMU_DESCRIPTOR_L2_SMALL_SIZE;  // 移动到下一页
            continue;  // 跳过当前页
        }
        // we need to clear the mapping here and remain the phy page.
        status = LOS_ArchMmuUnmap(archMmu, oldVaddr, 1);  // 解除旧地址映射
        if (status < 0) {  // 解除映射失败
            VM_ERR("invalid args: archMmu %p, vaddr %p, count %d",
                   archMmu, oldVaddr, count);  // 打印错误
            return LOS_NOK;  // 返回错误
        }

        status = LOS_ArchMmuMap(archMmu, newVaddr, paddr, 1, flags);  // 在新地址建立映射
        if (status < 0) {  // 建立映射失败
            VM_ERR("invalid args:archMmu %p, old_vaddr %p, new_addr %p, count %d",
                   archMmu, oldVaddr, newVaddr, count);  // 打印错误
            return LOS_NOK;  // 返回错误
        }
        oldVaddr += MMU_DESCRIPTOR_L2_SMALL_SIZE;  // 移动到下一页
        newVaddr += MMU_DESCRIPTOR_L2_SMALL_SIZE;  // 移动到下一页
    }

    return LOS_OK;  // 全部完成，返回成功
}

/**
 * @brief 切换MMU上下文(地址空间)
 * @param archMmu 目标MMU架构信息结构体指针，NULL表示禁用用户空间
 */
VOID LOS_ArchMmuContextSwitch(LosArchMmu *archMmu)
{
    UINT32 ttbr;  // TTBR(Translation Table Base Register)值
    UINT32 ttbcr = OsArmReadTtbcr();  // 读取TTBCR(Translation Table Base Control Register)
    if (archMmu) {  // 如果目标地址空间有效
        ttbr = MMU_TTBRx_FLAGS | (archMmu->physTtb);  // 构造TTBR值(包含标志和页表物理基地址)
        /* enable TTBR0 */
        ttbcr &= ~MMU_DESCRIPTOR_TTBCR_PD0;  // 清除PD0位，启用TTBR0
    } else {  // 目标地址空间无效
        ttbr = 0;  // TTBR设为0
        /* disable TTBR0 */
        ttbcr |= MMU_DESCRIPTOR_TTBCR_PD0;  // 设置PD0位，禁用TTBR0
    }

#ifdef LOSCFG_KERNEL_VM  // 如果启用了内核虚拟内存
    /* from armv7a arm B3.10.4, we should do synchronization changes of ASID and TTBR. */
    OsArmWriteContextidr(LOS_GetKVmSpace()->archMmu.asid);  // 写入内核ASID
    ISB;  // 指令同步屏障
#endif
    OsArmWriteTtbr0(ttbr);  // 写入TTBR0寄存器
    ISB;  // 指令同步屏障
    OsArmWriteTtbcr(ttbcr);  // 写入TTBCR寄存器
    ISB;  // 指令同步屏障
#ifdef LOSCFG_KERNEL_VM  // 如果启用了内核虚拟内存
    if (archMmu) {  // 如果目标地址空间有效
        OsArmWriteContextidr(archMmu->asid);  // 写入用户进程ASID
        ISB;  // 指令同步屏障
    }
#endif
}

/**
 * @brief 销毁MMU上下文(释放页表和ASID)
 * @param archMmu 需要销毁的MMU架构信息结构体指针
 * @return 成功返回LOS_OK
 */
STATUS_T LOS_ArchMmuDestroy(LosArchMmu *archMmu)
{
#ifdef LOSCFG_KERNEL_VM  // 如果启用了内核虚拟内存
    LosVmPage *page = NULL;  // 虚拟内存页指针
    /* free all of the pages allocated in archMmu->ptList */
    while ((page = LOS_ListRemoveHeadType(&archMmu->ptList, LosVmPage, node)) != NULL) {  // 遍历页表列表
        LOS_PhysPageFree(page);  // 释放物理页
    }

    OsArmWriteTlbiasidis(archMmu->asid);  // 使TLB中对应ASID的条目无效
    OsFreeAsid(archMmu->asid);  // 释放ASID
#endif
    return LOS_OK;  // 返回成功
}

/**
 * @brief 切换到临时页表，用于内核页表初始化
 */
STATIC VOID OsSwitchTmpTTB(VOID)
{
    PTE_T *tmpTtbase = NULL;  // 临时页表基地址
    errno_t err;  // 错误码
    LosVmSpace *kSpace = LOS_GetKVmSpace();  // 获取内核地址空间

    /* ttbr address should be 16KByte align */
    tmpTtbase = LOS_MemAllocAlign(m_aucSysMem0, MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS,
                                  MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS);  // 分配16KB对齐的临时页表
    if (tmpTtbase == NULL) {  // 分配失败
        VM_ERR("memory alloc failed");  // 打印错误
        return;  // 返回
    }

    kSpace->archMmu.virtTtb = tmpTtbase;  // 设置内核地址空间的虚拟页表基地址
    err = memcpy_s(kSpace->archMmu.virtTtb, MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS,
                   g_firstPageTable, MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS);  // 复制初始页表内容
    if (err != EOK) {  // 复制失败
        (VOID)LOS_MemFree(m_aucSysMem0, tmpTtbase);  // 释放临时页表
        kSpace->archMmu.virtTtb = (VADDR_T *)g_firstPageTable;  // 恢复原页表
        VM_ERR("memcpy failed, errno: %d", err);  // 打印错误
        return;  // 返回
    }
    kSpace->archMmu.physTtb = LOS_PaddrQuery(kSpace->archMmu.virtTtb);  // 查询临时页表物理地址
    OsArmWriteTtbr0(kSpace->archMmu.physTtb | MMU_TTBRx_FLAGS);  // 写入TTBR0寄存器
    ISB;  // 指令同步屏障
}

/**
 * @brief 设置内核段属性(缓存策略等)
 * @param virtAddr 虚拟地址起始
 * @param uncached 是否使用非缓存属性
 */
STATIC VOID OsSetKSectionAttr(UINTPTR virtAddr, BOOL uncached)
{
    UINT32 offset = virtAddr - KERNEL_VMM_BASE;  // 计算地址偏移
    /* every section should be page aligned */
    UINTPTR textStart = (UINTPTR)&__text_start + offset;  // 文本段起始地址
    UINTPTR textEnd = (UINTPTR)&__text_end + offset;  // 文本段结束地址
    UINTPTR rodataStart = (UINTPTR)&__rodata_start + offset;  // 只读数据段起始地址
    UINTPTR rodataEnd = (UINTPTR)&__rodata_end + offset;  // 只读数据段结束地址
    UINTPTR ramDataStart = (UINTPTR)&__ram_data_start + offset;  // 数据段起始地址
    UINTPTR bssEnd = (UINTPTR)&__bss_end + offset;  // BSS段结束地址
    UINT32 bssEndBoundary = ROUNDUP(bssEnd, MB);  // BSS段结束地址向上取整到MB
    LosArchMmuInitMapping mmuKernelMappings[] = {  // 内核段映射表
        {
            .phys = SYS_MEM_BASE + textStart - virtAddr,  // 物理地址
            .virt = textStart,  // 虚拟地址
            .size = ROUNDUP(textEnd - textStart, MMU_DESCRIPTOR_L2_SMALL_SIZE),  // 大小(向上取整到4KB)
            .flags = VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_EXECUTE,  // 权限：可读可执行
            .name = "kernel_text"  // 段名称
        },
        {
            .phys = SYS_MEM_BASE + rodataStart - virtAddr,  // 物理地址
            .virt = rodataStart,  // 虚拟地址
            .size = ROUNDUP(rodataEnd - rodataStart, MMU_DESCRIPTOR_L2_SMALL_SIZE),  // 大小
            .flags = VM_MAP_REGION_FLAG_PERM_READ,  // 权限：只读
            .name = "kernel_rodata"  // 段名称
        },
        {
            .phys = SYS_MEM_BASE + ramDataStart - virtAddr,  // 物理地址
            .virt = ramDataStart,  // 虚拟地址
            .size = ROUNDUP(bssEndBoundary - ramDataStart, MMU_DESCRIPTOR_L2_SMALL_SIZE),  // 大小
            .flags = VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE,  // 权限：读写
            .name = "kernel_data_bss"  // 段名称
        }
    };
    LosVmSpace *kSpace = LOS_GetKVmSpace();  // 获取内核地址空间
    status_t status;  // 操作状态
    UINT32 length;  // 映射表长度
    INT32 i;  // 循环变量
    LosArchMmuInitMapping *kernelMap = NULL;  // 内核段映射项指针
    UINT32 kmallocLength;  // kmalloc区域长度
    UINT32 flags;  // 映射标志

    /* use second-level mapping of default READ and WRITE */
    kSpace->archMmu.virtTtb = (PTE_T *)g_firstPageTable;  // 设置虚拟页表基地址
    kSpace->archMmu.physTtb = LOS_PaddrQuery(kSpace->archMmu.virtTtb);  // 查询物理页表基地址
    // 解除原有映射
    status = LOS_ArchMmuUnmap(&kSpace->archMmu, virtAddr,
                              (bssEndBoundary - virtAddr) >> MMU_DESCRIPTOR_L2_SMALL_SHIFT);
    if (status != ((bssEndBoundary - virtAddr) >> MMU_DESCRIPTOR_L2_SMALL_SHIFT)) {  // 检查是否完全解除
        VM_ERR("unmap failed, status: %d", status);  // 打印错误
        return;  // 返回
    }

    flags = VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE | VM_MAP_REGION_FLAG_PERM_EXECUTE;  // 默认标志
    if (uncached) {  // 如果需要非缓存
        flags |= VM_MAP_REGION_FLAG_UNCACHED;  // 添加非缓存标志
    }
    // 映射内核起始到text段前的区域
    status = LOS_ArchMmuMap(&kSpace->archMmu, virtAddr, SYS_MEM_BASE,
                            (textStart - virtAddr) >> MMU_DESCRIPTOR_L2_SMALL_SHIFT,
                            flags);
    if (status != ((textStart - virtAddr) >> MMU_DESCRIPTOR_L2_SMALL_SHIFT)) {  // 检查映射是否成功
        VM_ERR("mmap failed, status: %d", status);  // 打印错误
        return;  // 返回
    }

    length = sizeof(mmuKernelMappings) / sizeof(LosArchMmuInitMapping);  // 计算映射表长度
    for (i = 0; i < length; i++) {  // 遍历映射表
        kernelMap = &mmuKernelMappings[i];  // 获取当前映射项
        if (uncached) {  // 如果需要非缓存
            kernelMap->flags |= VM_MAP_REGION_FLAG_UNCACHED;  // 添加非缓存标志
        }
        // 映射当前段
        status = LOS_ArchMmuMap(&kSpace->archMmu, kernelMap->virt, kernelMap->phys,
                                 kernelMap->size >> MMU_DESCRIPTOR_L2_SMALL_SHIFT, kernelMap->flags);
        if (status != (kernelMap->size >> MMU_DESCRIPTOR_L2_SMALL_SHIFT)) {  // 检查映射是否成功
            VM_ERR("mmap failed, status: %d", status);  // 打印错误
            return;  // 返回
        }
        LOS_VmSpaceReserve(kSpace, kernelMap->size, kernelMap->virt);  // 保留虚拟地址空间
    }

    kmallocLength = virtAddr + SYS_MEM_SIZE_DEFAULT - bssEndBoundary;  // 计算kmalloc区域长度
    flags = VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE;  // kmalloc区域权限：读写
    if (uncached) {  // 如果需要非缓存
        flags |= VM_MAP_REGION_FLAG_UNCACHED;  // 添加非缓存标志
    }
    // 映射kmalloc区域
    status = LOS_ArchMmuMap(&kSpace->archMmu, bssEndBoundary,
                            SYS_MEM_BASE + bssEndBoundary - virtAddr,
                            kmallocLength >> MMU_DESCRIPTOR_L2_SMALL_SHIFT,
                            flags);
    if (status != (kmallocLength >> MMU_DESCRIPTOR_L2_SMALL_SHIFT)) {  // 检查映射是否成功
        VM_ERR("mmap failed, status: %d", status);  // 打印错误
        return;  // 返回
    }
    LOS_VmSpaceReserve(kSpace, kmallocLength, bssEndBoundary);  // 保留kmalloc虚拟地址空间
}

/**
 * @brief 启用新的内核段属性设置
 */
STATIC VOID OsKSectionNewAttrEnable(VOID)
{
    LosVmSpace *kSpace = LOS_GetKVmSpace();  // 获取内核地址空间
    paddr_t oldTtPhyBase;  // 旧页表物理基地址

    kSpace->archMmu.virtTtb = (PTE_T *)g_firstPageTable;  // 恢复页表虚拟基地址
    kSpace->archMmu.physTtb = LOS_PaddrQuery(kSpace->archMmu.virtTtb);  // 查询页表物理基地址

    /* we need free tmp ttbase */
    oldTtPhyBase = OsArmReadTtbr0();  // 读取当前TTBR0
    oldTtPhyBase = oldTtPhyBase & MMU_DESCRIPTOR_L2_SMALL_FRAME;  // 提取物理基地址
    OsArmWriteTtbr0(kSpace->archMmu.physTtb | MMU_TTBRx_FLAGS);  // 写入新的TTBR0
    ISB;  // 指令同步屏障

    /* we changed page table entry, so we need to clean TLB here */
    OsCleanTLB();  // 清除TLB

    (VOID)LOS_MemFree(m_aucSysMem0, (VOID *)(UINTPTR)(oldTtPhyBase - SYS_MEM_BASE + KERNEL_VMM_BASE));  // 释放临时页表
}

/**
 * @brief 初始化每个CPU的MMU设置
 * @note 禁用TTBCR0并设置TTBR0和TTBR1的地址空间划分
 */
/* disable TTBCR0 and set the split between TTBR0 and TTBR1 */
VOID OsArchMmuInitPerCPU(VOID)
{
    UINT32 n = __builtin_clz(KERNEL_ASPACE_BASE) + 1;  // 计算地址划分位
    UINT32 ttbcr = MMU_DESCRIPTOR_TTBCR_PD0 | n;  // 构造TTBCR值

    OsArmWriteTtbr1(OsArmReadTtbr0());  // 设置TTBR1等于TTBR0(内核页表)
    ISB;  // 指令同步屏障
    OsArmWriteTtbcr(ttbcr);  // 写入TTBCR寄存器
    ISB;  // 指令同步屏障
    OsArmWriteTtbr0(0);  // 清除TTBR0
    ISB;  // 指令同步屏障
}

/**
 * @brief 启动阶段初始化内存映射
 */
VOID OsInitMappingStartUp(VOID)
{
    OsArmInvalidateTlbBarrier();  // 使TLB无效并同步

    OsSwitchTmpTTB();  // 切换到临时页表

    OsSetKSectionAttr(KERNEL_VMM_BASE, FALSE);  // 设置内核缓存区域属性
    OsSetKSectionAttr(UNCACHED_VMM_BASE, TRUE);  // 设置内核非缓存区域属性
    OsKSectionNewAttrEnable();  // 启用新的内核段属性
}
#endif
