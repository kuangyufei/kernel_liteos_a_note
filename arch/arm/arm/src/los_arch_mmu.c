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

 * @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-11-17
 *
 * @history
 *
 */
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

/**
 * @defgroup los_arch_mmu architecture mmu
 * @ingroup kernel
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

#ifdef LOSCFG_KERNEL_MMU
typedef struct {
    LosArchMmu *archMmu;
    VADDR_T *vaddr;
    PADDR_T *paddr;
    UINT32 *flags;
} MmuMapInfo;

#define TRY_MAX_TIMES 10

__attribute__((aligned(MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS))) \
    __attribute__((section(".bss.prebss.translation_table"))) UINT8 \
    g_firstPageTable[MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS];
#ifdef LOSCFG_KERNEL_SMP
__attribute__((aligned(MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS))) \
    __attribute__((section(".bss.prebss.translation_table"))) UINT8 \
    g_tempPageTable[MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS];
UINT8 *g_mmuJumpPageTable = g_tempPageTable;
#else
extern CHAR __mmu_ttlb_begin; /* defined in .ld script | 内核临时页表在系统使能mmu到使用虚拟地址运行这段期间使用,其虚拟地址保存在g_mmuJumpPageTable这个指针中*/
UINT8 *g_mmuJumpPageTable = (UINT8 *)&__mmu_ttlb_begin; /* temp page table, this is only used when system power up | 临时页表,用于系统启动阶段*/
#endif

STATIC SPIN_LOCK_S *OsGetPteLock(LosArchMmu *archMmu, PADDR_T paddr, UINT32 *intSave)
{
    SPIN_LOCK_S *lock = NULL;
#ifdef LOSCFG_PAGE_TABLE_FINE_LOCK
    LosVmPage *vmPage = NULL;

    vmPage = OsVmPaddrToPage(paddr);
    if (vmPage == NULL) {
        return NULL;
    }
    lock = &vmPage->lock;
#else
    lock = &archMmu->lock;
#endif

    LOS_SpinLockSave(lock, intSave);
    return lock;
}

STATIC SPIN_LOCK_S *OsGetPte1Lock(LosArchMmu *archMmu, PADDR_T paddr, UINT32 *intSave)
{
    return OsGetPteLock(archMmu, paddr, intSave);
}

STATIC INLINE VOID OsUnlockPte1(SPIN_LOCK_S *lock, UINT32 intSave)
{
    if (lock == NULL) {
        return;
    }
    LOS_SpinUnlockRestore(lock, intSave);
}

STATIC SPIN_LOCK_S *OsGetPte1LockTmp(LosArchMmu *archMmu, PADDR_T paddr, UINT32 *intSave)
{
    SPIN_LOCK_S *spinLock = NULL;
#ifdef LOSCFG_PAGE_TABLE_FINE_LOCK
    spinLock = OsGetPteLock(archMmu, paddr, intSave);
#else
    (VOID)archMmu;
    (VOID)paddr;
    (VOID)intSave;
#endif
    return spinLock;
}

STATIC INLINE VOID OsUnlockPte1Tmp(SPIN_LOCK_S *lock, UINT32 intSave)
{
#ifdef LOSCFG_PAGE_TABLE_FINE_LOCK
    if (lock == NULL) {
        return;
    }
    LOS_SpinUnlockRestore(lock, intSave);
#else
    (VOID)lock;
    (VOID)intSave;
#endif
}

STATIC INLINE SPIN_LOCK_S *OsGetPte2Lock(LosArchMmu *archMmu, PTE_T pte1, UINT32 *intSave)
{
    PADDR_T pa = MMU_DESCRIPTOR_L1_PAGE_TABLE_ADDR(pte1);
    return OsGetPteLock(archMmu, pa, intSave);
}

STATIC INLINE VOID OsUnlockPte2(SPIN_LOCK_S *lock, UINT32 intSave)
{
    return OsUnlockPte1(lock, intSave);
}
/// 获取页表基地址
STATIC INLINE PTE_T *OsGetPte2BasePtr(PTE_T pte1)
{
    PADDR_T pa = MMU_DESCRIPTOR_L1_PAGE_TABLE_ADDR(pte1);
    return LOS_PaddrToKVaddr(pa);
}

VADDR_T *OsGFirstTableGet(VOID)
{
    return (VADDR_T *)g_firstPageTable;
}

/// 解除L1表的映射关系
STATIC INLINE UINT32 OsUnmapL1Invalid(vaddr_t *vaddr, UINT32 *count)
{
    UINT32 unmapCount;

    unmapCount = MIN2((MMU_DESCRIPTOR_L1_SMALL_SIZE - (*vaddr % MMU_DESCRIPTOR_L1_SMALL_SIZE)) >>
        MMU_DESCRIPTOR_L2_SMALL_SHIFT, *count);
    *vaddr += unmapCount << MMU_DESCRIPTOR_L2_SMALL_SHIFT;
    *count -= unmapCount;

    return unmapCount;
}

STATIC INT32 OsMapParamCheck(UINT32 flags, VADDR_T vaddr, PADDR_T paddr)
{
#if !WITH_ARCH_MMU_PICK_SPOT
    if (flags & VM_MAP_REGION_FLAG_NS) {
        /* WITH_ARCH_MMU_PICK_SPOT is required to support NS memory */
        LOS_Panic("NS mem is not supported\n");
    }
#endif

    /* paddr and vaddr must be aligned */
    if (!MMU_DESCRIPTOR_IS_L2_SIZE_ALIGNED(vaddr) || !MMU_DESCRIPTOR_IS_L2_SIZE_ALIGNED(paddr)) {
        return LOS_ERRNO_VM_INVALID_ARGS;
    }

    return 0;
}

STATIC VOID OsCvtPte2AttsToFlags(PTE_T l1Entry, PTE_T l2Entry, UINT32 *flags)
{
    *flags = 0;
    /* NS flag is only present on L1 entry */
    if (l1Entry & MMU_DESCRIPTOR_L1_PAGETABLE_NON_SECURE) {
        *flags |= VM_MAP_REGION_FLAG_NS;
    }

    switch (l2Entry & MMU_DESCRIPTOR_L2_TEX_TYPE_MASK) {
        case MMU_DESCRIPTOR_L2_TYPE_STRONGLY_ORDERED:
            *flags |= VM_MAP_REGION_FLAG_STRONGLY_ORDERED;
            break;
        case MMU_DESCRIPTOR_L2_TYPE_NORMAL_NOCACHE:
            *flags |= VM_MAP_REGION_FLAG_UNCACHED;
            break;
        case MMU_DESCRIPTOR_L2_TYPE_DEVICE_SHARED:
        case MMU_DESCRIPTOR_L2_TYPE_DEVICE_NON_SHARED:
            *flags |= VM_MAP_REGION_FLAG_UNCACHED_DEVICE;
            break;
        default:
            break;
    }

    *flags |= VM_MAP_REGION_FLAG_PERM_READ;

    switch (l2Entry & MMU_DESCRIPTOR_L2_AP_MASK) {
        case MMU_DESCRIPTOR_L2_AP_P_RO_U_NA:
            break;
        case MMU_DESCRIPTOR_L2_AP_P_RW_U_NA:
            *flags |= VM_MAP_REGION_FLAG_PERM_WRITE;
            break;
        case MMU_DESCRIPTOR_L2_AP_P_RO_U_RO:
            *flags |= VM_MAP_REGION_FLAG_PERM_USER;
            break;
        case MMU_DESCRIPTOR_L2_AP_P_RW_U_RW:
            *flags |= VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_WRITE;
            break;
        default:
            break;
    }
    if ((l2Entry & MMU_DESCRIPTOR_L2_TYPE_MASK) != MMU_DESCRIPTOR_L2_TYPE_SMALL_PAGE_XN) {
        *flags |= VM_MAP_REGION_FLAG_PERM_EXECUTE;
    }
}

STATIC VOID OsPutL2Table(const LosArchMmu *archMmu, UINT32 l1Index, paddr_t l2Paddr)
{
    UINT32 index;
    PTE_T ttEntry;
    /* check if any l1 entry points to this l2 table */
    for (index = 0; index < MMU_DESCRIPTOR_L1_SMALL_L2_TABLES_PER_PAGE; index++) {
        ttEntry = archMmu->virtTtb[ROUNDDOWN(l1Index, MMU_DESCRIPTOR_L1_SMALL_L2_TABLES_PER_PAGE) + index];
        if ((ttEntry &  MMU_DESCRIPTOR_L1_TYPE_MASK) == MMU_DESCRIPTOR_L1_TYPE_PAGE_TABLE) {
            return;
        }
    }
#ifdef LOSCFG_KERNEL_VM
    /* we can free this l2 table */
    LosVmPage *vmPage = LOS_VmPageGet(l2Paddr);
    if (vmPage == NULL) {
        LOS_Panic("bad page table paddr %#x\n", l2Paddr);
        return;
    }

    LOS_ListDelete(&vmPage->node);
    LOS_PhysPageFree(vmPage);
#else
    (VOID)LOS_MemFree(OS_SYS_MEM_ADDR, LOS_PaddrToKVaddr(l2Paddr));
#endif
}

STATIC VOID OsTryUnmapL1PTE(LosArchMmu *archMmu, PTE_T *l1Entry, vaddr_t vaddr, UINT32 scanIndex, UINT32 scanCount)
{
    /*
     * Check if all pages related to this l1 entry are deallocated.
     * We only need to check pages that we did not clear above starting
     * from scanIndex and wrapped around SECTION.
     */
    UINT32 l1Index;
    PTE_T *pte2BasePtr = NULL;
    SPIN_LOCK_S *pte1Lock = NULL;
    SPIN_LOCK_S *pte2Lock = NULL;
    UINT32 pte1IntSave;
    UINT32 pte2IntSave;
    PTE_T pte1Val;
    PADDR_T pte1Paddr;

    pte1Paddr = OsGetPte1Paddr(archMmu->physTtb, vaddr);
    pte2Lock = OsGetPte2Lock(archMmu, *l1Entry, &pte2IntSave);
    if (pte2Lock == NULL) {
        return;
    }
    pte2BasePtr = OsGetPte2BasePtr(*l1Entry);
    if (pte2BasePtr == NULL) {
        OsUnlockPte2(pte2Lock, pte2IntSave);
        return;
    }

    while (scanCount) {
        if (scanIndex == MMU_DESCRIPTOR_L2_NUMBERS_PER_L1) {
            scanIndex = 0;
        }
        if (pte2BasePtr[scanIndex++]) {
            break;
        }
        scanCount--;
    }

    if (!scanCount) {
        /*
         * The pte1 of kprocess is placed in kernel image when compiled. So the pte1Lock will be null.
         * There is no situation to simultaneous access the pte1 of kprocess.
         */
        pte1Lock = OsGetPte1LockTmp(archMmu, pte1Paddr, &pte1IntSave);
        if (!OsIsPte1PageTable(*l1Entry)) {
            OsUnlockPte1Tmp(pte1Lock, pte1IntSave);
            OsUnlockPte2(pte2Lock, pte2IntSave);
            return;
        }
        pte1Val = *l1Entry;
        /* we can kill l1 entry */
        OsClearPte1(l1Entry);
        l1Index = OsGetPte1Index(vaddr);
        OsArmInvalidateTlbMvaNoBarrier(l1Index << MMU_DESCRIPTOR_L1_SMALL_SHIFT);

        /* try to free l2 page itself */
        OsPutL2Table(archMmu, l1Index, MMU_DESCRIPTOR_L1_PAGE_TABLE_ADDR(pte1Val));
        OsUnlockPte1Tmp(pte1Lock, pte1IntSave);
    }
    OsUnlockPte2(pte2Lock, pte2IntSave);
}

STATIC UINT32 OsCvtSecCacheFlagsToMMUFlags(UINT32 flags)
{
    UINT32 mmuFlags = 0;

    switch (flags & VM_MAP_REGION_FLAG_CACHE_MASK) {
        case VM_MAP_REGION_FLAG_CACHED:
            mmuFlags |= MMU_DESCRIPTOR_L1_TYPE_NORMAL_WRITE_BACK_ALLOCATE;
#ifdef LOSCFG_KERNEL_SMP
            mmuFlags |= MMU_DESCRIPTOR_L1_SECTION_SHAREABLE;
#endif
            break;
        case VM_MAP_REGION_FLAG_STRONGLY_ORDERED:
            mmuFlags |= MMU_DESCRIPTOR_L1_TYPE_STRONGLY_ORDERED;
            break;
        case VM_MAP_REGION_FLAG_UNCACHED:
            mmuFlags |= MMU_DESCRIPTOR_L1_TYPE_NORMAL_NOCACHE;
            break;
        case VM_MAP_REGION_FLAG_UNCACHED_DEVICE:
            mmuFlags |= MMU_DESCRIPTOR_L1_TYPE_DEVICE_SHARED;
            break;
        default:
            return LOS_ERRNO_VM_INVALID_ARGS;
    }
    return mmuFlags;
}

STATIC UINT32 OsCvtSecAccessFlagsToMMUFlags(UINT32 flags)
{
    UINT32 mmuFlags = 0;

    switch (flags & (VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE)) {
        case 0:
            mmuFlags |= MMU_DESCRIPTOR_L1_AP_P_NA_U_NA;
            break;
        case VM_MAP_REGION_FLAG_PERM_READ:
        case VM_MAP_REGION_FLAG_PERM_USER:
            mmuFlags |= MMU_DESCRIPTOR_L1_AP_P_RO_U_NA;
            break;
        case VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_READ:
            mmuFlags |= MMU_DESCRIPTOR_L1_AP_P_RO_U_RO;
            break;
        case VM_MAP_REGION_FLAG_PERM_WRITE:
        case VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE:
            mmuFlags |= MMU_DESCRIPTOR_L1_AP_P_RW_U_NA;
            break;
        case VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_WRITE:
        case VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE:
            mmuFlags |= MMU_DESCRIPTOR_L1_AP_P_RW_U_RW;
            break;
        default:
            break;
    }
    return mmuFlags;
}

/* convert user level mmu flags to L1 descriptors flags */
STATIC UINT32 OsCvtSecFlagsToAttrs(UINT32 flags)
{
    UINT32 mmuFlags;

    mmuFlags = OsCvtSecCacheFlagsToMMUFlags(flags);
    if (mmuFlags == LOS_ERRNO_VM_INVALID_ARGS) {
        return mmuFlags;
    }

    mmuFlags |= MMU_DESCRIPTOR_L1_SMALL_DOMAIN_CLIENT;

    mmuFlags |= OsCvtSecAccessFlagsToMMUFlags(flags);

    if (!(flags & VM_MAP_REGION_FLAG_PERM_EXECUTE)) {
        mmuFlags |= MMU_DESCRIPTOR_L1_SECTION_XN;
    }

    if (flags & VM_MAP_REGION_FLAG_NS) {
        mmuFlags |= MMU_DESCRIPTOR_L1_SECTION_NON_SECURE;
    }

    if (flags & VM_MAP_REGION_FLAG_PERM_USER) {
        mmuFlags |= MMU_DESCRIPTOR_L1_SECTION_NON_GLOBAL;
    }

    return mmuFlags;
}

STATIC VOID OsCvtSecAttsToFlags(PTE_T l1Entry, UINT32 *flags)
{
    *flags = 0;
    if (l1Entry & MMU_DESCRIPTOR_L1_SECTION_NON_SECURE) {
        *flags |= VM_MAP_REGION_FLAG_NS;
    }

    switch (l1Entry & MMU_DESCRIPTOR_L1_TEX_TYPE_MASK) {
        case MMU_DESCRIPTOR_L1_TYPE_STRONGLY_ORDERED:
            *flags |= VM_MAP_REGION_FLAG_STRONGLY_ORDERED;
            break;
        case MMU_DESCRIPTOR_L1_TYPE_NORMAL_NOCACHE:
            *flags |= VM_MAP_REGION_FLAG_UNCACHED;
            break;
        case MMU_DESCRIPTOR_L1_TYPE_DEVICE_SHARED:
        case MMU_DESCRIPTOR_L1_TYPE_DEVICE_NON_SHARED:
            *flags |= VM_MAP_REGION_FLAG_UNCACHED_DEVICE;
            break;
        default:
            break;
    }

    *flags |= VM_MAP_REGION_FLAG_PERM_READ;

    switch (l1Entry & MMU_DESCRIPTOR_L1_AP_MASK) {
        case MMU_DESCRIPTOR_L1_AP_P_RO_U_NA:
            break;
        case MMU_DESCRIPTOR_L1_AP_P_RW_U_NA:
            *flags |= VM_MAP_REGION_FLAG_PERM_WRITE;
            break;
        case MMU_DESCRIPTOR_L1_AP_P_RO_U_RO:
            *flags |= VM_MAP_REGION_FLAG_PERM_USER;
            break;
        case MMU_DESCRIPTOR_L1_AP_P_RW_U_RW:
            *flags |= VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_WRITE;
            break;
        default:
            break;
    }

    if (!(l1Entry & MMU_DESCRIPTOR_L1_SECTION_XN)) {
        *flags |= VM_MAP_REGION_FLAG_PERM_EXECUTE;
    }
}

STATIC UINT32 OsUnmapL2PTE(LosArchMmu *archMmu, PTE_T *pte1, vaddr_t vaddr, UINT32 *count)
{
    UINT32 unmapCount;
    UINT32 pte2Index;
    UINT32 intSave;
    PTE_T *pte2BasePtr = NULL;
    SPIN_LOCK_S *lock = NULL;

    pte2Index = OsGetPte2Index(vaddr);
    unmapCount = MIN2(MMU_DESCRIPTOR_L2_NUMBERS_PER_L1 - pte2Index, *count);

    lock = OsGetPte2Lock(archMmu, *pte1, &intSave);
    if (lock == NULL) {
        return unmapCount;
    }

    pte2BasePtr = OsGetPte2BasePtr(*pte1);
    if (pte2BasePtr == NULL) {
        OsUnlockPte2(lock, intSave);
        return unmapCount;
    }

    /* unmap page run */
    OsClearPte2Continuous(&pte2BasePtr[pte2Index], unmapCount);

    /* invalidate tlb */
    OsArmInvalidateTlbMvaRangeNoBarrier(vaddr, unmapCount);
    OsUnlockPte2(lock, intSave);

    *count -= unmapCount;
    return unmapCount;
}

STATIC UINT32 OsUnmapSection(LosArchMmu *archMmu, PTE_T *l1Entry, vaddr_t *vaddr, UINT32 *count)
{
    UINT32 intSave;
    PADDR_T pte1Paddr;
    SPIN_LOCK_S *lock = NULL;

    pte1Paddr = OsGetPte1Paddr(archMmu->physTtb, *vaddr);
    lock = OsGetPte1Lock(archMmu, pte1Paddr, &intSave);
    if (!OsIsPte1Section(*l1Entry)) {
        OsUnlockPte1(lock, intSave);
        return 0;
    }
    OsClearPte1(OsGetPte1Ptr((PTE_T *)archMmu->virtTtb, *vaddr));
    OsArmInvalidateTlbMvaNoBarrier(*vaddr);
    OsUnlockPte1(lock, intSave);

    *vaddr += MMU_DESCRIPTOR_L1_SMALL_SIZE;
    *count -= MMU_DESCRIPTOR_L2_NUMBERS_PER_L1;

    return MMU_DESCRIPTOR_L2_NUMBERS_PER_L1;
}

BOOL OsArchMmuInit(LosArchMmu *archMmu, VADDR_T *virtTtb)
{
#ifdef LOSCFG_KERNEL_VM
    if (OsAllocAsid(&archMmu->asid) != LOS_OK) {//分配一个asid,ASID可用来唯一标识进程空间
        VM_ERR("alloc arch mmu asid failed");//地址空间标识码（address-space identifier，ASID）为CP15协处理器 C13寄存器
        return FALSE;
    }
#endif

#ifndef LOSCFG_PAGE_TABLE_FINE_LOCK
    LOS_SpinInit(&archMmu->lock);
#endif
    LOS_ListInit(&archMmu->ptList);//初始化页表，双循环进程所有物理页框 LOS_ListAdd(&processCB->vmSpace->archMmu.ptList, &(vmPage->node));
    archMmu->virtTtb = virtTtb;//为L1页表在内存位置 section(".bss.prebss.translation_table") UINT8 g_firstPageTable[MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS]
    archMmu->physTtb = (VADDR_T)(UINTPTR)virtTtb - KERNEL_ASPACE_BASE + SYS_MEM_BASE;// TTB寄存器是CP15协处理器的C2寄存器，存页表的基地址
    //SYS_MEM_BASE = 0x80000000  KERNEL_ASPACE_BASE = 0x40000000	见于 ..\vendor\hi3516dv300\config\board\include\board.h
    //archMmu->physTtb = (VADDR_T)(UINTPTR)virtTtb + 0x40000000; 
    return TRUE;
}


/*!
 * @brief LOS_ArchMmuQuery 获取进程空间虚拟地址对应的物理地址以及映射属性。	
 * 本函数是内核高频函数,通过MMU查询虚拟地址是否映射过,带走映射的物理地址和权限
 * @param archMmu	
 * @param flags	
 * @param paddr	
 * @param vaddr	
 * @return	
 *
 * @see
 */
STATUS_T LOS_ArchMmuQuery(const LosArchMmu *archMmu, VADDR_T vaddr, PADDR_T *paddr, UINT32 *flags)
{//archMmu->virtTtb:转换表基地址
    PTE_T l1Entry = OsGetPte1(archMmu->virtTtb, vaddr);//获取PTE vaddr右移20位 得到L1描述子地址
    PTE_T l2Entry;
    PTE_T* l2Base = NULL;

    if (OsIsPte1Invalid(l1Entry)) {//判断L1描述子地址是否有效
        return LOS_ERRNO_VM_NOT_FOUND;//无效返回虚拟地址未查询到
    } else if (OsIsPte1Section(l1Entry)) {// section页表项: l1Entry低二位是否为 10
        if (paddr != NULL) {//物理地址 = 节基地址(section页表项的高12位) + 虚拟地址低20位
            *paddr = MMU_DESCRIPTOR_L1_SECTION_ADDR(l1Entry) + (vaddr & (MMU_DESCRIPTOR_L1_SMALL_SIZE - 1));
        }

        if (flags != NULL) {
            OsCvtSecAttsToFlags(l1Entry, flags);//获取虚拟内存的flag信息
        }
    } else if (OsIsPte1PageTable(l1Entry)) {//PAGE_TABLE页表项: l1Entry低二位是否为 01
        l2Base = OsGetPte2BasePtr(l1Entry);//获取L2转换表基地址
        if (l2Base == NULL) {
            return LOS_ERRNO_VM_NOT_FOUND;
        }
        l2Entry = OsGetPte2(l2Base, vaddr);//获取L2描述子地址
        if (OsIsPte2SmallPage(l2Entry) || OsIsPte2SmallPageXN(l2Entry)) {
            if (paddr != NULL) {//物理地址 = 小页基地址(L2页表项的高20位) + 虚拟地址低12位
                *paddr = MMU_DESCRIPTOR_L2_SMALL_PAGE_ADDR(l2Entry) + (vaddr & (MMU_DESCRIPTOR_L2_SMALL_SIZE - 1));
            }

            if (flags != NULL) {
                OsCvtPte2AttsToFlags(l1Entry, l2Entry, flags);//获取虚拟内存的flag信息
            }
        } else if (OsIsPte2LargePage(l2Entry)) {//鸿蒙目前暂不支持64K大页，未来手机版应该会支持。
            LOS_Panic("%s %d, large page unimplemented\n", __FUNCTION__, __LINE__);
        } else {
            return LOS_ERRNO_VM_NOT_FOUND;
        }
    }

    return LOS_OK;
}

/*!
 * @brief LOS_ArchMmuUnmap	解除进程空间虚拟地址区间与物理地址区间的映射关系
 *
 * @param archMmu	
 * @param count	
 * @param vaddr	
 * @return	
 *
 * @see
 */
STATUS_T LOS_ArchMmuUnmap(LosArchMmu *archMmu, VADDR_T vaddr, size_t count)
{
    PTE_T *l1Entry = NULL;
    INT32 unmapped = 0;
    UINT32 unmapCount = 0;
    INT32 tryTime = TRY_MAX_TIMES;

    while (count > 0) {
        l1Entry = OsGetPte1Ptr(archMmu->virtTtb, vaddr);//获取L1表
        if (OsIsPte1Invalid(*l1Entry)) {//L1表是否有效
            unmapCount = OsUnmapL1Invalid(&vaddr, &count);//取消L1表内的映射
        } else if (OsIsPte1Section(*l1Entry)) {
            if (MMU_DESCRIPTOR_IS_L1_SIZE_ALIGNED(vaddr) && count >= MMU_DESCRIPTOR_L2_NUMBERS_PER_L1) {
                unmapCount = OsUnmapSection(archMmu, l1Entry, &vaddr, &count);
            } else {
                LOS_Panic("%s %d, unimplemented\n", __FUNCTION__, __LINE__);
            }
        } else if (OsIsPte1PageTable(*l1Entry)) {
            unmapCount = OsUnmapL2PTE(archMmu, l1Entry, vaddr, &count);//取消L2表的映射
            OsTryUnmapL1PTE(archMmu, l1Entry, vaddr, OsGetPte2Index(vaddr) + unmapCount,
                            MMU_DESCRIPTOR_L2_NUMBERS_PER_L1);
            vaddr += unmapCount << MMU_DESCRIPTOR_L2_SMALL_SHIFT;
        } else {
            LOS_Panic("%s %d, unimplemented\n", __FUNCTION__, __LINE__);
        }
        tryTime = (unmapCount == 0) ? (tryTime - 1) : tryTime;
        if (tryTime == 0) {
            return LOS_ERRNO_VM_FAULT;
        }
        unmapped += unmapCount;
    }
    OsArmInvalidateTlbBarrier();
    return unmapped;
}

/*!
 * @brief OsMapSection	section页表格式项映射
 *
 * @param archMmu	
 * @param count	
 * @param flags	
 * @param paddr	
 * @param vaddr	
 * @return	
 *
 * @see
 */
STATIC UINT32 OsMapSection(MmuMapInfo *mmuMapInfo, UINT32 *count)
{
    UINT32 mmuFlags = 0;
    UINT32 intSave;
    PADDR_T pte1Paddr;
    SPIN_LOCK_S *lock = NULL;

    mmuFlags |= OsCvtSecFlagsToAttrs(*mmuMapInfo->flags);
    pte1Paddr = OsGetPte1Paddr(mmuMapInfo->archMmu->physTtb, *mmuMapInfo->vaddr);
    lock = OsGetPte1Lock(mmuMapInfo->archMmu, pte1Paddr, &intSave);
    OsSavePte1(OsGetPte1Ptr(mmuMapInfo->archMmu->virtTtb, *mmuMapInfo->vaddr),
        OsTruncPte1(*mmuMapInfo->paddr) | mmuFlags | MMU_DESCRIPTOR_L1_TYPE_SECTION);
    OsUnlockPte1(lock, intSave);
    *count -= MMU_DESCRIPTOR_L2_NUMBERS_PER_L1;
    *mmuMapInfo->vaddr += MMU_DESCRIPTOR_L1_SMALL_SIZE;
    *mmuMapInfo->paddr += MMU_DESCRIPTOR_L1_SMALL_SIZE;

    return MMU_DESCRIPTOR_L2_NUMBERS_PER_L1;
}
/// 获取L2页表,分配L2表(需物理内存)
STATIC STATUS_T OsGetL2Table(LosArchMmu *archMmu, UINT32 l1Index, paddr_t *ppa)
{
    UINT32 index;
    PTE_T ttEntry;
    VADDR_T *kvaddr = NULL;
    UINT32 l2Offset = (MMU_DESCRIPTOR_L2_SMALL_SIZE / MMU_DESCRIPTOR_L1_SMALL_L2_TABLES_PER_PAGE) *
        (l1Index & (MMU_DESCRIPTOR_L1_SMALL_L2_TABLES_PER_PAGE - 1));//计算偏移量,因为一个L2表最大是1K,而一个物理页框是4K,内核不铺张浪费把4个L2表放在一个页框中
    /* lookup an existing l2 page table *///查询已存在的L2表
    for (index = 0; index < MMU_DESCRIPTOR_L1_SMALL_L2_TABLES_PER_PAGE; index++) {
        ttEntry = archMmu->virtTtb[ROUNDDOWN(l1Index, MMU_DESCRIPTOR_L1_SMALL_L2_TABLES_PER_PAGE) + index];
        if ((ttEntry & MMU_DESCRIPTOR_L1_TYPE_MASK) == MMU_DESCRIPTOR_L1_TYPE_PAGE_TABLE) {
            *ppa = (PADDR_T)ROUNDDOWN(MMU_DESCRIPTOR_L1_PAGE_TABLE_ADDR(ttEntry), MMU_DESCRIPTOR_L2_SMALL_SIZE) +
                l2Offset;
            return LOS_OK;
        }
    }

#ifdef LOSCFG_KERNEL_VM
    /* not found: allocate one (paddr) */
    LosVmPage *vmPage = LOS_PhysPageAlloc();
    if (vmPage == NULL) {
        VM_ERR("have no memory to save l2 page");
        return LOS_ERRNO_VM_NO_MEMORY;
    }
    LOS_ListAdd(&archMmu->ptList, &vmPage->node);
    kvaddr = OsVmPageToVaddr(vmPage);
#else
    kvaddr = LOS_MemAlloc(OS_SYS_MEM_ADDR, MMU_DESCRIPTOR_L2_SMALL_SIZE);
    if (kvaddr == NULL) {
        VM_ERR("have no memory to save l2 page");
        return LOS_ERRNO_VM_NO_MEMORY;
    }
#endif
    (VOID)memset_s(kvaddr, MMU_DESCRIPTOR_L2_SMALL_SIZE, 0, MMU_DESCRIPTOR_L2_SMALL_SIZE);

    /* get physical address */
    *ppa = OsKVaddrToPaddr((VADDR_T)kvaddr) + l2Offset;
    return LOS_OK;
}

STATIC UINT32 OsCvtPte2CacheFlagsToMMUFlags(UINT32 flags)
{
    UINT32 mmuFlags = 0;

    switch (flags & VM_MAP_REGION_FLAG_CACHE_MASK) {
        case VM_MAP_REGION_FLAG_CACHED:
#ifdef LOSCFG_KERNEL_SMP
            mmuFlags |= MMU_DESCRIPTOR_L2_SHAREABLE;
#endif
            mmuFlags |= MMU_DESCRIPTOR_L2_TYPE_NORMAL_WRITE_BACK_ALLOCATE;
            break;
        case VM_MAP_REGION_FLAG_STRONGLY_ORDERED:
            mmuFlags |= MMU_DESCRIPTOR_L2_TYPE_STRONGLY_ORDERED;
            break;
        case VM_MAP_REGION_FLAG_UNCACHED:
            mmuFlags |= MMU_DESCRIPTOR_L2_TYPE_NORMAL_NOCACHE;
            break;
        case VM_MAP_REGION_FLAG_UNCACHED_DEVICE:
            mmuFlags |= MMU_DESCRIPTOR_L2_TYPE_DEVICE_SHARED;
            break;
        default:
            return LOS_ERRNO_VM_INVALID_ARGS;
    }
    return mmuFlags;
}

STATIC UINT32 OsCvtPte2AccessFlagsToMMUFlags(UINT32 flags)
{
    UINT32 mmuFlags = 0;

    switch (flags & (VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE)) {
        case 0:
            mmuFlags |= MMU_DESCRIPTOR_L1_AP_P_NA_U_NA;
            break;
        case VM_MAP_REGION_FLAG_PERM_READ:
        case VM_MAP_REGION_FLAG_PERM_USER:
            mmuFlags |= MMU_DESCRIPTOR_L2_AP_P_RO_U_NA;
            break;
        case VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_READ:
            mmuFlags |= MMU_DESCRIPTOR_L2_AP_P_RO_U_RO;
            break;
        case VM_MAP_REGION_FLAG_PERM_WRITE:
        case VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE:
            mmuFlags |= MMU_DESCRIPTOR_L2_AP_P_RW_U_NA;
            break;
        case VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_WRITE:
        case VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE:
            mmuFlags |= MMU_DESCRIPTOR_L2_AP_P_RW_U_RW;
            break;
        default:
            break;
    }
    return mmuFlags;
}

/* convert user level mmu flags to L2 descriptors flags */
STATIC UINT32 OsCvtPte2FlagsToAttrs(UINT32 flags)
{
    UINT32 mmuFlags;

    mmuFlags = OsCvtPte2CacheFlagsToMMUFlags(flags);
    if (mmuFlags == LOS_ERRNO_VM_INVALID_ARGS) {
        return mmuFlags;
    }

    mmuFlags |= OsCvtPte2AccessFlagsToMMUFlags(flags);

    if (!(flags & VM_MAP_REGION_FLAG_PERM_EXECUTE)) {
        mmuFlags |= MMU_DESCRIPTOR_L2_TYPE_SMALL_PAGE_XN;
    } else {
        mmuFlags |= MMU_DESCRIPTOR_L2_TYPE_SMALL_PAGE;
    }

    if (flags & VM_MAP_REGION_FLAG_PERM_USER) {
        mmuFlags |= MMU_DESCRIPTOR_L2_NON_GLOBAL;
    }

    return mmuFlags;
}

STATIC UINT32 OsMapL1PTE(MmuMapInfo *mmuMapInfo, PTE_T *l1Entry, UINT32 *count)
{
    PADDR_T pte2Base = 0;
    PADDR_T pte1Paddr;
    SPIN_LOCK_S *pte1Lock = NULL;
    SPIN_LOCK_S *pte2Lock = NULL;
    PTE_T *pte2BasePtr = NULL;
    UINT32 saveCounts, archFlags, pte1IntSave, pte2IntSave;

    pte1Paddr = OsGetPte1Paddr(mmuMapInfo->archMmu->physTtb, *mmuMapInfo->vaddr);
    pte1Lock = OsGetPte1Lock(mmuMapInfo->archMmu, pte1Paddr, &pte1IntSave);
    if (!OsIsPte1Invalid(*l1Entry)) {
        OsUnlockPte1(pte1Lock, pte1IntSave);
        return 0;
    }
    if (OsGetL2Table(mmuMapInfo->archMmu, OsGetPte1Index(*mmuMapInfo->vaddr), &pte2Base) != LOS_OK) {
        LOS_Panic("%s %d, failed to allocate pagetable\n", __FUNCTION__, __LINE__);
    }

    *l1Entry = pte2Base | MMU_DESCRIPTOR_L1_TYPE_PAGE_TABLE;
    if (*mmuMapInfo->flags & VM_MAP_REGION_FLAG_NS) {
        *l1Entry |= MMU_DESCRIPTOR_L1_PAGETABLE_NON_SECURE;
    }
    *l1Entry &= MMU_DESCRIPTOR_L1_SMALL_DOMAIN_MASK;
    *l1Entry |= MMU_DESCRIPTOR_L1_SMALL_DOMAIN_CLIENT; // use client AP
    OsSavePte1(OsGetPte1Ptr(mmuMapInfo->archMmu->virtTtb, *mmuMapInfo->vaddr), *l1Entry);
    OsUnlockPte1(pte1Lock, pte1IntSave);

    pte2Lock = OsGetPte2Lock(mmuMapInfo->archMmu, *l1Entry, &pte2IntSave);
    if (pte2Lock == NULL) {
        LOS_Panic("pte2 should not be null!\n");
    }
    pte2BasePtr = (PTE_T *)LOS_PaddrToKVaddr(pte2Base);

    /* compute the arch flags for L2 4K pages */
    archFlags = OsCvtPte2FlagsToAttrs(*mmuMapInfo->flags);
    saveCounts = OsSavePte2Continuous(pte2BasePtr, OsGetPte2Index(*mmuMapInfo->vaddr), *mmuMapInfo->paddr | archFlags,
                                      *count);
    OsUnlockPte2(pte2Lock, pte2IntSave);
    *mmuMapInfo->paddr += (saveCounts << MMU_DESCRIPTOR_L2_SMALL_SHIFT);
    *mmuMapInfo->vaddr += (saveCounts << MMU_DESCRIPTOR_L2_SMALL_SHIFT);
    *count -= saveCounts;
    return saveCounts;
}

STATIC UINT32 OsMapL2PageContinous(MmuMapInfo *mmuMapInfo, PTE_T *pte1, UINT32 *count)
{
    PTE_T *pte2BasePtr = NULL;
    UINT32 archFlags;
    UINT32 saveCounts;
    UINT32 intSave;
    SPIN_LOCK_S *lock = NULL;

    lock = OsGetPte2Lock(mmuMapInfo->archMmu, *pte1, &intSave);
    if (lock == NULL) {
        return 0;
    }
    pte2BasePtr = OsGetPte2BasePtr(*pte1);
    if (pte2BasePtr == NULL) {
        OsUnlockPte2(lock, intSave);
        return 0;
    }

    /* compute the arch flags for L2 4K pages */
    archFlags = OsCvtPte2FlagsToAttrs(*mmuMapInfo->flags);
    saveCounts = OsSavePte2Continuous(pte2BasePtr, OsGetPte2Index(*mmuMapInfo->vaddr), *mmuMapInfo->paddr | archFlags,
                                      *count);
    OsUnlockPte2(lock, intSave);
    *mmuMapInfo->paddr += (saveCounts << MMU_DESCRIPTOR_L2_SMALL_SHIFT);
    *mmuMapInfo->vaddr += (saveCounts << MMU_DESCRIPTOR_L2_SMALL_SHIFT);
    *count -= saveCounts;
    return saveCounts;
}
/*!
 * @brief LOS_ArchMmuMap 映射进程空间虚拟地址区间与物理地址区间	
 * 所谓的map就是生成L1,L2页表项的过程
 * @param archMmu	
 * @param count	
 * @param flags	
 * @param paddr	
 * @param vaddr	
 * @return	
 *
 * @see
 */
status_t LOS_ArchMmuMap(LosArchMmu *archMmu, VADDR_T vaddr, PADDR_T paddr, size_t count, UINT32 flags)
{
    PTE_T *l1Entry = NULL;
    UINT32 saveCounts = 0;
    INT32 mapped = 0;
    INT32 tryTime = TRY_MAX_TIMES;
    INT32 checkRst;
    MmuMapInfo mmuMapInfo = {
        .archMmu = archMmu,
        .vaddr = &vaddr,
        .paddr = &paddr,
        .flags = &flags,
    };

    checkRst = OsMapParamCheck(flags, vaddr, paddr);//检查参数
    if (checkRst < 0) {
        return checkRst;
    }

    /* see what kind of mapping we can use */
    while (count > 0) {
        if (MMU_DESCRIPTOR_IS_L1_SIZE_ALIGNED(*mmuMapInfo.vaddr) &&
            MMU_DESCRIPTOR_IS_L1_SIZE_ALIGNED(*mmuMapInfo.paddr) &&
            count >= MMU_DESCRIPTOR_L2_NUMBERS_PER_L1) {
            /* compute the arch flags for L1 sections cache, r ,w ,x, domain and type */
            saveCounts = OsMapSection(&mmuMapInfo, &count);
        } else {
            /* have to use a L2 mapping, we only allocate 4KB for L1, support 0 ~ 1GB */
            l1Entry = OsGetPte1Ptr(archMmu->virtTtb, *mmuMapInfo.vaddr);
            if (OsIsPte1Invalid(*l1Entry)) {
                saveCounts = OsMapL1PTE(&mmuMapInfo, l1Entry, &count);
            } else if (OsIsPte1PageTable(*l1Entry)) {
                saveCounts = OsMapL2PageContinous(&mmuMapInfo, l1Entry, &count);
            } else {
                LOS_Panic("%s %d, unimplemented tt_entry %x\n", __FUNCTION__, __LINE__, l1Entry);
            }
        }
        mapped += saveCounts;
        tryTime = (saveCounts == 0) ? (tryTime - 1) : tryTime;
        if (tryTime == 0) {
            return LOS_ERRNO_VM_TIMED_OUT;
        }
    }

    return mapped;
}

/*!
 * @brief LOS_ArchMmuChangeProt	修改进程空间虚拟地址区间的映射属性
 * 改变内存段的访问权限,例如: 读/写/可执行/不可用 == 
 * @param archMmu	
 * @param count	
 * @param flags	
 * @param vaddr	
 * @return	
 *
 * @see
 */
STATUS_T LOS_ArchMmuChangeProt(LosArchMmu *archMmu, VADDR_T vaddr, size_t count, UINT32 flags)
{
    STATUS_T status;
    PADDR_T paddr = 0;

    if ((archMmu == NULL) || (vaddr == 0) || (count == 0)) {
        VM_ERR("invalid args: archMmu %p, vaddr %p, count %d", archMmu, vaddr, count);
        return LOS_NOK;
    }

    while (count > 0) {
        count--;
        status = LOS_ArchMmuQuery(archMmu, vaddr, &paddr, NULL);//1. 先查出物理地址
        if (status != LOS_OK) {
            vaddr += MMU_DESCRIPTOR_L2_SMALL_SIZE;
            continue;
        }

        status = LOS_ArchMmuUnmap(archMmu, vaddr, 1);//2. 取消原有映射
        if (status < 0) {
            VM_ERR("invalid args:aspace %p, vaddr %p, count %d", archMmu, vaddr, count);
            return LOS_NOK;
        }

        status = LOS_ArchMmuMap(archMmu, vaddr, paddr, 1, flags);//3. 重新映射 虚实地址
        if (status < 0) {
            VM_ERR("invalid args:aspace %p, vaddr %p, count %d",
                   archMmu, vaddr, count);
            return LOS_NOK;
        }
        vaddr += MMU_DESCRIPTOR_L2_SMALL_SIZE;
    }
    return LOS_OK;
}

/*!
 * @brief LOS_ArchMmuMove 将进程空间一个虚拟地址区间的映射关系转移至另一块未使用的虚拟地址区间重新做映射。	
 *
 * @param archMmu	
 * @param count	
 * @param flags	
 * @param newVaddr	
 * @param oldVaddr	
 * @return	
 *
 * @see
 */
STATUS_T LOS_ArchMmuMove(LosArchMmu *archMmu, VADDR_T oldVaddr, VADDR_T newVaddr, size_t count, UINT32 flags)
{
    STATUS_T status;
    PADDR_T paddr = 0;

    if ((archMmu == NULL) || (oldVaddr == 0) || (newVaddr == 0) || (count == 0)) {
        VM_ERR("invalid args: archMmu %p, oldVaddr %p, newVaddr %p, count %d",
               archMmu, oldVaddr, newVaddr, count);
        return LOS_NOK;
    }

    while (count > 0) {
        count--;
        status = LOS_ArchMmuQuery(archMmu, oldVaddr, &paddr, NULL);
        if (status != LOS_OK) {
            oldVaddr += MMU_DESCRIPTOR_L2_SMALL_SIZE;
            newVaddr += MMU_DESCRIPTOR_L2_SMALL_SIZE;
            continue;
        }
        // we need to clear the mapping here and remain the phy page.
        status = LOS_ArchMmuUnmap(archMmu, oldVaddr, 1);
        if (status < 0) {
            VM_ERR("invalid args: archMmu %p, vaddr %p, count %d",
                   archMmu, oldVaddr, count);
            return LOS_NOK;
        }

        status = LOS_ArchMmuMap(archMmu, newVaddr, paddr, 1, flags);
        if (status < 0) {
            VM_ERR("invalid args:archMmu %p, old_vaddr %p, new_addr %p, count %d",
                   archMmu, oldVaddr, newVaddr, count);
            return LOS_NOK;
        }
        oldVaddr += MMU_DESCRIPTOR_L2_SMALL_SIZE;
        newVaddr += MMU_DESCRIPTOR_L2_SMALL_SIZE;
    }

    return LOS_OK;
}

/*!
 * @brief LOS_ArchMmuContextSwitch	切换MMU上下文
 *
 * @param archMmu	
 * @return	
 *
 * @see
 */
VOID LOS_ArchMmuContextSwitch(LosArchMmu *archMmu)
{
    UINT32 ttbr;
    UINT32 ttbcr = OsArmReadTtbcr();//读取TTB寄存器的状态值
    if (archMmu) {
        ttbr = MMU_TTBRx_FLAGS | (archMmu->physTtb);//进程TTB物理地址值
        /* enable TTBR0 */
        ttbcr &= ~MMU_DESCRIPTOR_TTBCR_PD0;//使能TTBR0
    } else {
        ttbr = 0;
        /* disable TTBR0 */
        ttbcr |= MMU_DESCRIPTOR_TTBCR_PD0;
    }

#ifdef LOSCFG_KERNEL_VM
    /* from armv7a arm B3.10.4, we should do synchronization changes of ASID and TTBR. */
    OsArmWriteContextidr(LOS_GetKVmSpace()->archMmu.asid);//这里先把asid切到内核空间的ID
    ISB;
#endif
    OsArmWriteTtbr0(ttbr);//通过r0寄存器将进程页面基址写入TTB
    ISB;
    OsArmWriteTtbcr(ttbcr);//写入TTB状态位
    ISB;
#ifdef LOSCFG_KERNEL_VM
    if (archMmu) {
        OsArmWriteContextidr(archMmu->asid);//通过R0寄存器写入进程标识符至C13寄存器
        ISB;
    }
#endif
}

/*!
 * @brief LOS_ArchMmuDestroy 销毁MMU 和 initMmu 相呼应,释放页表页
 * 
 * @param archMmu	
 * @return	
 *
 * @see
 */
STATUS_T LOS_ArchMmuDestroy(LosArchMmu *archMmu)
{
#ifdef LOSCFG_KERNEL_VM
    LosVmPage *page = NULL;
    /* free all of the pages allocated in archMmu->ptList */
    while ((page = LOS_ListRemoveHeadType(&archMmu->ptList, LosVmPage, node)) != NULL) {
        LOS_PhysPageFree(page);//释放物理页
    }

    OsArmWriteTlbiasidis(archMmu->asid);
    OsFreeAsid(archMmu->asid);//释放asid
#endif
    return LOS_OK;
}

STATIC VOID OsSwitchTmpTTB(VOID)
{
    PTE_T *tmpTtbase = NULL;
    errno_t err;
    LosVmSpace *kSpace = LOS_GetKVmSpace();

    /* ttbr address should be 16KByte align */
    tmpTtbase = LOS_MemAllocAlign(m_aucSysMem0, MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS,
                                  MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS);
    if (tmpTtbase == NULL) {
        VM_ERR("memory alloc failed");
        return;
    }

    kSpace->archMmu.virtTtb = tmpTtbase;
    err = memcpy_s(kSpace->archMmu.virtTtb, MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS,
                   g_firstPageTable, MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS);
    if (err != EOK) {
        (VOID)LOS_MemFree(m_aucSysMem0, tmpTtbase);
        kSpace->archMmu.virtTtb = (VADDR_T *)g_firstPageTable;
        VM_ERR("memcpy failed, errno: %d", err);
        return;
    }
    kSpace->archMmu.physTtb = LOS_PaddrQuery(kSpace->archMmu.virtTtb);
    OsArmWriteTtbr0(kSpace->archMmu.physTtb | MMU_TTBRx_FLAGS);
    ISB;
}

/// 设置内核空间段属性,可看出内核空间是固定映射到物理地址
STATIC VOID OsSetKSectionAttr(UINTPTR virtAddr, BOOL uncached)
{
    UINT32 offset = virtAddr - KERNEL_VMM_BASE;
    /* every section should be page aligned */
    UINTPTR textStart = (UINTPTR)&__text_start + offset;
    UINTPTR textEnd = (UINTPTR)&__text_end + offset;
    UINTPTR rodataStart = (UINTPTR)&__rodata_start + offset;
    UINTPTR rodataEnd = (UINTPTR)&__rodata_end + offset;
    UINTPTR ramDataStart = (UINTPTR)&__ram_data_start + offset;
    UINTPTR bssEnd = (UINTPTR)&__bss_end + offset;
    UINT32 bssEndBoundary = ROUNDUP(bssEnd, MB);
    LosArchMmuInitMapping mmuKernelMappings[] = {//初始化映射关系
        {	//映射代码区
            .phys = SYS_MEM_BASE + textStart - virtAddr,
            .virt = textStart,//内核代码区
            .size = ROUNDUP(textEnd - textStart, MMU_DESCRIPTOR_L2_SMALL_SIZE),//代码区大小
            .flags = VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_EXECUTE,//代码段只读，可执行
            .name = "kernel_text"
        },
        {	//映射只读数据
            .phys = SYS_MEM_BASE + rodataStart - virtAddr,
            .virt = rodataStart,//内核常量区
            .size = ROUNDUP(rodataEnd - rodataStart, MMU_DESCRIPTOR_L2_SMALL_SIZE),//4K对齐
            .flags = VM_MAP_REGION_FLAG_PERM_READ,//常量段只读
            .name = "kernel_rodata"
        },
        {	//映射全局未初始化区
            .phys = SYS_MEM_BASE + ramDataStart - virtAddr,
            .virt = ramDataStart,
            .size = ROUNDUP(bssEndBoundary - ramDataStart, MMU_DESCRIPTOR_L2_SMALL_SIZE),
            .flags = VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE,//全局变量区可读可写
            .name = "kernel_data_bss"
        }
    };
    LosVmSpace *kSpace = LOS_GetKVmSpace();//获取内核空间
    status_t status;
    UINT32 length;
    INT32 i;
    LosArchMmuInitMapping *kernelMap = NULL;//内核映射
    UINT32 kmallocLength;
    UINT32 flags;

    /* use second-level mapping of default READ and WRITE */
    kSpace->archMmu.virtTtb = (PTE_T *)g_firstPageTable;//__attribute__((section(".bss.prebss.translation_table"))) UINT8 g_firstPageTable[MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS];
    kSpace->archMmu.physTtb = LOS_PaddrQuery(kSpace->archMmu.virtTtb);//通过TTB虚拟地址查询TTB物理地址
    status = LOS_ArchMmuUnmap(&kSpace->archMmu, virtAddr,
                               (bssEndBoundary - virtAddr) >> MMU_DESCRIPTOR_L2_SMALL_SHIFT);
    if (status != ((bssEndBoundary - virtAddr) >> MMU_DESCRIPTOR_L2_SMALL_SHIFT)) {
        VM_ERR("unmap failed, status: %d", status);
        return;
    }

    flags = VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE | VM_MAP_REGION_FLAG_PERM_EXECUTE;
    if (uncached) {
        flags |= VM_MAP_REGION_FLAG_UNCACHED;
    }
    status = LOS_ArchMmuMap(&kSpace->archMmu, virtAddr, SYS_MEM_BASE,
                             (textStart - virtAddr) >> MMU_DESCRIPTOR_L2_SMALL_SHIFT,
                             flags);
    if (status != ((textStart - virtAddr) >> MMU_DESCRIPTOR_L2_SMALL_SHIFT)) {
        VM_ERR("mmap failed, status: %d", status);
        return;
    }

    length = sizeof(mmuKernelMappings) / sizeof(LosArchMmuInitMapping);
    for (i = 0; i < length; i++) {
        kernelMap = &mmuKernelMappings[i];
	if (uncached) {
            kernelMap->flags |= VM_MAP_REGION_FLAG_UNCACHED;
	}
        status = LOS_ArchMmuMap(&kSpace->archMmu, kernelMap->virt, kernelMap->phys,
                                 kernelMap->size >> MMU_DESCRIPTOR_L2_SMALL_SHIFT, kernelMap->flags);
        if (status != (kernelMap->size >> MMU_DESCRIPTOR_L2_SMALL_SHIFT)) {
            VM_ERR("mmap failed, status: %d", status);
            return;
        }
        LOS_VmSpaceReserve(kSpace, kernelMap->size, kernelMap->virt);//保留区
    }
	//将剩余空间映射好
    kmallocLength = virtAddr + SYS_MEM_SIZE_DEFAULT - bssEndBoundary;
    flags = VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE;
    if (uncached) {
        flags |= VM_MAP_REGION_FLAG_UNCACHED;
    }
    status = LOS_ArchMmuMap(&kSpace->archMmu, bssEndBoundary,
                             SYS_MEM_BASE + bssEndBoundary - virtAddr,
                             kmallocLength >> MMU_DESCRIPTOR_L2_SMALL_SHIFT,
                             flags);
    if (status != (kmallocLength >> MMU_DESCRIPTOR_L2_SMALL_SHIFT)) {
        VM_ERR("mmap failed, status: %d", status);
        return;
    }
    LOS_VmSpaceReserve(kSpace, kmallocLength, bssEndBoundary);
}

STATIC VOID OsKSectionNewAttrEnable(VOID)
{
    LosVmSpace *kSpace = LOS_GetKVmSpace();
    paddr_t oldTtPhyBase;

    kSpace->archMmu.virtTtb = (PTE_T *)g_firstPageTable;
    kSpace->archMmu.physTtb = LOS_PaddrQuery(kSpace->archMmu.virtTtb);

    /* we need free tmp ttbase */
    oldTtPhyBase = OsArmReadTtbr0();//读取TTB值
    oldTtPhyBase = oldTtPhyBase & MMU_DESCRIPTOR_L2_SMALL_FRAME;
    OsArmWriteTtbr0(kSpace->archMmu.physTtb | MMU_TTBRx_FLAGS);//内核页表基地址写入CP15 c2(TTB寄存器)
    ISB;

    /* we changed page table entry, so we need to clean TLB here */
    OsCleanTLB();//清空TLB缓冲区

    (VOID)LOS_MemFree(m_aucSysMem0, (VOID *)(UINTPTR)(oldTtPhyBase - SYS_MEM_BASE + KERNEL_VMM_BASE));//释放内存池
}

/* disable TTBCR0 and set the split between TTBR0 and TTBR1 */
VOID OsArchMmuInitPerCPU(VOID)
{
    UINT32 n = __builtin_clz(KERNEL_ASPACE_BASE) + 1;
    UINT32 ttbcr = MMU_DESCRIPTOR_TTBCR_PD0 | n;

    OsArmWriteTtbr1(OsArmReadTtbr0());//读取地址转换表基地址
    ISB;//指令同步隔离。最严格：它会清洗流水线，以保证所有它前面的指令都执行完毕之后，才执行它后面的指令。
    OsArmWriteTtbcr(ttbcr);
    ISB;
    OsArmWriteTtbr0(0);
    ISB;
}

/*!
 * @brief OsInitMappingStartUp	开始初始化mmu
 *
 * @return	
 *
 * @see
 */
VOID OsInitMappingStartUp(VOID)
{
    OsArmInvalidateTlbBarrier();//使TLB失效

    OsSwitchTmpTTB();//切换到临时TTB ,请想想为何要切换到临时 @note_thinking

    OsSetKSectionAttr(KERNEL_VMM_BASE, FALSE);
    OsSetKSectionAttr(UNCACHED_VMM_BASE, TRUE);
    OsKSectionNewAttrEnable();
}
#endif


