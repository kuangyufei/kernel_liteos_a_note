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
 * @defgroup los_vm_fault vm fault definition
 * @ingroup kernel
 */
#include "los_vm_fault.h"
#include "los_vm_map.h"
#include "los_vm_dump.h"
#include "los_vm_filemap.h"
#include "los_vm_page.h"
#include "los_vm_lock.h"
#include "los_exc.h"
#include "los_oom.h"
#include "los_printf.h"
#include "los_process_pri.h"
#include "arm.h"

#ifdef LOSCFG_FS_VFS
#include "vnode.h"
#endif


#ifdef LOSCFG_KERNEL_VM

/**
 * @file los_vm_fault.c
 * @brief 虚拟内存页面故障处理模块
 * @details 实现Linux-like的页面故障处理机制，包括权限检查、异常修复、COW(写时复制)、文件映射
 *          故障处理等核心功能，是虚拟内存子系统的关键组件
 * @note 该模块依赖体系结构相关的内存管理单元(MMU)操作接口
 */

/**
 * @brief 异常处理表起始地址
 * @details 链接脚本定义的异常处理表起始标记，用于存储异常地址与修复函数的映射关系
 */
extern char __exc_table_start[];
/**
 * @brief 异常处理表结束地址
 * @details 链接脚本定义的异常处理表结束标记，与__exc_table_start配合确定表大小
 */
extern char __exc_table_end[];

/**
 * @brief 虚拟内存区域权限检查
 * @details 验证访问虚拟内存区域的权限是否满足操作要求，是内存安全的关键检查点
 * @param[in] region 虚拟内存区域结构体指针，包含区域的权限信息
 * @param[in] flags 访问标志，包含读/写/执行权限要求，取值为VM_MAP_PF_FLAG_xxx宏的组合
 * @retval LOS_OK 权限检查通过
 * @retval LOS_NOK 权限检查失败，具体失败原因通过VM_ERR输出
 * @note 该函数必须在获取区域锁的上下文中调用，防止区域信息被并发修改
 */
STATIC STATUS_T OsVmRegionPermissionCheck(LosVmMapRegion *region, UINT32 flags)
{
    // 检查读权限：所有内存访问都需要读权限基础
    if ((region->regionFlags & VM_MAP_REGION_FLAG_PERM_READ) != VM_MAP_REGION_FLAG_PERM_READ) {
        VM_ERR("read permission check failed operation flags %x, region flags %x", flags, region->regionFlags);
        return LOS_NOK;
    }

    // 检查写权限：仅当操作包含写标志时需要验证
    if ((flags & VM_MAP_PF_FLAG_WRITE) == VM_MAP_PF_FLAG_WRITE) {
        if ((region->regionFlags & VM_MAP_REGION_FLAG_PERM_WRITE) != VM_MAP_REGION_FLAG_PERM_WRITE) {
            VM_ERR("write permission check failed operation flags %x, region flags %x", flags, region->regionFlags);
            return LOS_NOK;
        }
    }

    // 检查执行权限：仅当操作为指令获取时需要验证
    if ((flags & VM_MAP_PF_FLAG_INSTRUCTION) == VM_MAP_PF_FLAG_INSTRUCTION) {
        if ((region->regionFlags & VM_MAP_REGION_FLAG_PERM_EXECUTE) != VM_MAP_REGION_FLAG_PERM_EXECUTE) {
            VM_ERR("exec permission check failed operation flags %x, region flags %x", flags, region->regionFlags);
            return LOS_NOK;
        }
    }

    return LOS_OK;  // 所有权限检查通过
}

/**
 * @brief 异常修复尝试
 * @details 通过异常处理表查找并修复可恢复的页面故障异常，是内核稳定性的重要保障机制
 * @param[in,out] frame 异常上下文结构体指针，包含CPU寄存器状态
 * @param[in] excVaddr 触发异常的虚拟地址
 * @param[in,out] status 状态指针，成功修复时会被设置为LOS_OK
 * @note 仅处理内核模式下的异常修复，用户模式异常直接返回错误
 * @warning 异常处理表必须在链接阶段正确生成，否则可能导致修复失败
 */
STATIC VOID OsFaultTryFixup(ExcContext *frame, VADDR_T excVaddr, STATUS_T *status)
{
    INT32 tableNum = (__exc_table_end - __exc_table_start) / sizeof(LosExcTable);  // 计算异常表项数量
    LosExcTable *excTable = (LosExcTable *)__exc_table_start;                     // 获取异常表起始地址

    // 仅处理内核模式异常(非用户模式)
    if ((frame->regCPSR & CPSR_MODE_MASK) != CPSR_MODE_USR) {
        // 遍历异常表查找匹配的异常地址
        for (int i = 0; i < tableNum; ++i, ++excTable) {
            if (frame->PC == (UINTPTR)excTable->excAddr) {
                frame->PC = (UINTPTR)excTable->fixAddr;  // 设置PC为修复函数地址
                frame->R2 = (UINTPTR)excVaddr;           // 将异常地址存入R2寄存器供修复函数使用
                *status = LOS_OK;                        // 更新状态为修复成功
                return;
            }
        }
    }
}

#ifdef LOSCFG_FS_VFS  // 当启用虚拟文件系统时编译以下代码
/**
 * @brief 处理读操作引起的页面故障
 * @details 从文件系统或页面缓存中读取数据并建立内存映射，实现按需加载机制
 * @param[in] region 虚拟内存区域结构体指针，必须是文件映射类型区域
 * @param[in,out] vmPgFault 页面故障信息结构体，包含故障地址和输出页面信息
 * @retval LOS_OK 读故障处理成功
 * @retval LOS_ERRNO_VM_INVALID_ARGS 参数无效，通常是文件操作接口未实现
 * @retval LOS_ERRNO_VM_NO_MEMORY 内存分配或映射失败
 * @note 该函数会调用文件系统的fault接口来填充页面数据
 * @see OsDoFileFault, OsDoSharedFault
 */
STATIC STATUS_T OsDoReadFault(LosVmMapRegion *region, LosVmPgFault *vmPgFault)
{
    status_t ret;
    PADDR_T paddr;                  // 物理地址
    LosVmPage *page = NULL;         // 虚拟内存页面结构体指针
    VADDR_T vaddr = (VADDR_T)vmPgFault->vaddr;  // 故障虚拟地址
    LosVmSpace *space = region->space;          // 获取进程虚拟地址空间

    // 检查地址是否已映射(理论上不应该发生，防御性检查)
    ret = LOS_ArchMmuQuery(&space->archMmu, vaddr, NULL, NULL);
    if (ret == LOS_OK) {
        return LOS_OK;  // 已映射，无需处理
    }

    // 验证文件映射操作接口是否有效
    if (region->unTypeData.rf.vmFOps == NULL || region->unTypeData.rf.vmFOps->fault == NULL) {
        VM_ERR("region args invalid, file path: %s", region->unTypeData.rf.vnode->filePath);
        return LOS_ERRNO_VM_INVALID_ARGS;
    }

    // 获取文件映射的互斥锁，确保线程安全
    (VOID)LOS_MuxAcquire(&region->unTypeData.rf.vnode->mapping.mux_lock);
    // 调用文件系统的fault接口填充页面数据
    ret = region->unTypeData.rf.vmFOps->fault(region, vmPgFault);
    if (ret == LOS_OK) {
        // 从内核虚拟地址查询物理地址
        paddr = LOS_PaddrQuery(vmPgFault->pageKVaddr);
        // 获取页面结构体
        page = LOS_VmPageGet(paddr);
        if (page != NULL) { /* 防御性检查：页面指针非空 */
            LOS_AtomicInc(&page->refCounts);  // 增加页面引用计数
            OsCleanPageLocked(page);          // 清理页面锁定状态
        }
        // 建立映射，移除写权限(读操作)
        ret = LOS_ArchMmuMap(&space->archMmu, vaddr, paddr, 1,
                             region->regionFlags & (~VM_MAP_REGION_FLAG_PERM_WRITE));
        if (ret < 0) {
            VM_ERR("LOS_ArchMmuMap failed");
            OsDelMapInfo(region, vmPgFault, false);  // 删除映射信息
            (VOID)LOS_MuxRelease(&region->unTypeData.rf.vnode->mapping.mux_lock);
            return LOS_ERRNO_VM_NO_MEMORY;
        }

        (VOID)LOS_MuxRelease(&region->unTypeData.rf.vnode->mapping.mux_lock);
        return LOS_OK;
    }
    (VOID)LOS_MuxRelease(&region->unTypeData.rf.vnode->mapping.mux_lock);

    return LOS_ERRNO_VM_NO_MEMORY;
}

/**
 * @brief 写时复制(COW)原始页面解除映射
 * @details 当发生写时复制时，解除原始共享页面的映射，为后续复制做准备
 * @param[in] archMmu MMU操作结构体指针，包含架构相关的MMU操作接口
 * @param[in] region 虚拟内存区域结构体指针
 * @param[in] vmf 页面故障信息结构体
 * @return LosVmPage* 原始页面结构体指针，NULL表示解除映射失败
 * @note 该函数仅在COW场景下被调用，需要关闭中断保护临界区
 * @see OsDoCowFault
 */
STATIC LosVmPage *OsCowUnmapOrg(LosArchMmu *archMmu, LosVmMapRegion *region, LosVmPgFault *vmf)
{
    UINT32 intSave;                 // 中断状态保存变量
    LosVmPage *oldPage = NULL;      // 原始页面结构体指针
    LosMapInfo *mapInfo = NULL;     // 映射信息结构体指针
    LosFilePage *fpage = NULL;      // 文件页面结构体指针
    VADDR_T vaddr = (VADDR_T)vmf->vaddr;  // 故障虚拟地址

    // 关中断并获取自旋锁，保护文件页面链表操作
    LOS_SpinLockSave(&region->unTypeData.rf.vnode->mapping.list_lock, &intSave);
    // 查找文件页面缓存项
    fpage = OsFindGetEntry(&region->unTypeData.rf.vnode->mapping, vmf->pgoff);
    if (fpage != NULL) {
        oldPage = fpage->vmPage;    // 获取原始页面
        OsSetPageLocked(oldPage);   // 锁定页面，防止被换出
        // 获取映射信息
        mapInfo = OsGetMapInfo(fpage, archMmu, vaddr);
        if (mapInfo != NULL) {
            OsUnmapPageLocked(fpage, mapInfo);  // 解除页面映射
        } else {
            // 映射信息不存在，直接解除MMU映射
            LOS_ArchMmuUnmap(archMmu, vaddr, 1);
        }
    } else {
        // 文件页面缓存未命中，直接解除MMU映射
        LOS_ArchMmuUnmap(archMmu, vaddr, 1);
    }
    // 恢复中断状态并释放自旋锁
    LOS_SpinUnlockRestore(&region->unTypeData.rf.vnode->mapping.list_lock, intSave);

    return oldPage;  // 返回原始页面指针
}
#endif  /* LOSCFG_FS_VFS */

/**
 * @brief 处理写时复制(COW)页面故障
 * @details 实现写时复制机制，当多个进程共享页面且其中一个进程写入时，复制页面并建立新映射
 * @param[in] region 虚拟内存区域结构体指针
 * @param[in,out] vmPgFault 页面故障信息结构体，输入故障地址，输出页面信息
 * @retval LOS_OK COW处理成功
 * @retval LOS_ERRNO_VM_INVALID_ARGS 参数无效，如区域或故障信息为空
 * @retval LOS_ERRNO_VM_NO_MEMORY 内存分配或映射失败
 * @note COW机制是优化内存使用的关键技术，避免了进程创建时的大量页面复制
 * @warning 该函数涉及复杂的内存操作，错误处理路径需要确保资源正确释放
 * @see OsCowUnmapOrg, OsDoFileFault
 */
status_t OsDoCowFault(LosVmMapRegion *region, LosVmPgFault *vmPgFault)
{
    STATUS_T ret;
    VOID *kvaddr = NULL;            // 内核虚拟地址
    PADDR_T oldPaddr = 0;           // 原始物理地址
    PADDR_T newPaddr;               // 新物理地址
    LosVmPage *oldPage = NULL;      // 原始页面结构体指针
    LosVmPage *newPage = NULL;      // 新页面结构体指针
    LosVmSpace *space = NULL;       // 虚拟地址空间结构体指针

    // 参数合法性检查
    if ((vmPgFault == NULL) || (region == NULL) ||
        (region->unTypeData.rf.vmFOps == NULL) || (region->unTypeData.rf.vmFOps->fault == NULL)) {
        VM_ERR("region args invalid");
        return LOS_ERRNO_VM_INVALID_ARGS;
    }

    space = region->space;  // 获取虚拟地址空间
    // 查询原始物理地址
    ret = LOS_ArchMmuQuery(&space->archMmu, (VADDR_T)vmPgFault->vaddr, &oldPaddr, NULL);
    if (ret == LOS_OK) {
        // 解除原始页面映射
        oldPage = OsCowUnmapOrg(&space->archMmu, region, vmPgFault);
    }

    // 分配新的物理页面
    newPage = LOS_PhysPageAlloc();
    if (newPage == NULL) {
        VM_ERR("LOS_PhysPageAlloc failed");
        ret = LOS_ERRNO_VM_NO_MEMORY;
        goto ERR_OUT;  // 跳转至错误处理
    }

    newPaddr = VM_PAGE_TO_PHYS(newPage);  // 计算新页面物理地址
    kvaddr = OsVmPageToVaddr(newPage);    // 转换为内核虚拟地址

    // 获取文件映射互斥锁
    (VOID)LOS_MuxAcquire(&region->unTypeData.rf.vnode->mapping.mux_lock);
    // 调用文件系统fault接口获取原始页面数据
    ret = region->unTypeData.rf.vmFOps->fault(region->unTypeData.rf.vnode->filePath, vmPgFault); //@note_ai
    if (ret != LOS_OK) {
        VM_ERR("call region->vm_ops->fault fail");
        (VOID)LOS_MuxRelease(&region->unTypeData.rf.vnode->mapping.mux_lock);
        goto ERR_OUT;  // 跳转至错误处理
    }

    /**
     * 两种情况需要处理：
     * 1. 页面未映射或从页面缓存映射：标准文件COW映射
     * 2. 页面已完成文件COW映射：匿名COW映射
     */
    if ((oldPaddr == 0) || (LOS_PaddrToKVaddr(oldPaddr) == vmPgFault->pageKVaddr)) {
        // 从文件缓存复制页面数据
        (VOID)memcpy_s(kvaddr, PAGE_SIZE, vmPgFault->pageKVaddr, PAGE_SIZE);
        LOS_AtomicInc(&newPage->refCounts);  // 增加新页面引用计数
        // 清理原始页面锁定状态
        OsCleanPageLocked(LOS_VmPageGet(LOS_PaddrQuery(vmPgFault->pageKVaddr)));
    } else {
        // 复制物理共享页面
        OsPhysSharePageCopy(oldPaddr, &newPaddr, newPage);
        // 如果新地址与旧地址相同，表示共享页面无需复制
        if (newPaddr == oldPaddr) {
            LOS_PhysPageFree(newPage);  // 释放新分配的页面
            newPage = NULL;
        }
    }

    // 建立新页面的内存映射
    ret = LOS_ArchMmuMap(&space->archMmu, (VADDR_T)vmPgFault->vaddr, newPaddr, 1, region->regionFlags);
    if (ret < 0) {
        VM_ERR("LOS_ArchMmuMap failed");
        ret =  LOS_ERRNO_VM_NO_MEMORY;
        (VOID)LOS_MuxRelease(&region->unTypeData.rf.vnode->mapping.mux_lock);
        goto ERR_OUT;  // 跳转至错误处理
    }
    (VOID)LOS_MuxRelease(&region->unTypeData.rf.vnode->mapping.mux_lock);

    // 清理原始页面锁定状态
    if (oldPage != NULL) {
        OsCleanPageLocked(oldPage);
    }

    return LOS_OK;  // COW处理成功

ERR_OUT:  // 错误处理出口
    if (newPage != NULL) {
        LOS_PhysPageFree(newPage);  // 释放新页面
    }
    if (oldPage != NULL) {
        OsCleanPageLocked(oldPage);  // 清理原始页面状态
    }

    return ret;  // 返回错误码
}

/**
 * @brief 处理共享页面写故障
 * @details 对共享映射的页面执行写操作时，更新页面映射并标记为脏页
 * @param[in] region 虚拟内存区域结构体指针
 * @param[in,out] vmPgFault 页面故障信息结构体
 * @retval LOS_OK 共享故障处理成功
 * @retval LOS_ERRNO_VM_INVALID_ARGS 参数无效
 * @retval LOS_ERRNO_VM_NO_MEMORY 内存映射失败
 * @note 共享映射的写操作会直接修改原始页面，并同步回文件系统
 * @see OsDoFileFault, OsDoReadFault
 */
status_t OsDoSharedFault(LosVmMapRegion *region, LosVmPgFault *vmPgFault)
{
    STATUS_T ret;
    UINT32 intSave;                  // 中断状态保存变量
    PADDR_T paddr = 0;               // 物理地址
    VADDR_T vaddr = (VADDR_T)vmPgFault->vaddr;  // 故障虚拟地址
    LosVmSpace *space = region->space;          // 虚拟地址空间
    LosVmPage *page = NULL;          // 页面结构体指针
    LosFilePage *fpage = NULL;       // 文件页面结构体指针

    // 参数合法性检查
    if ((region->unTypeData.rf.vmFOps == NULL) || (region->unTypeData.rf.vmFOps->fault == NULL)) {
        VM_ERR("region args invalid");
        return LOS_ERRNO_VM_INVALID_ARGS;
    }

    // 查询当前地址映射
    ret = LOS_ArchMmuQuery(&space->archMmu, vmPgFault->vaddr, &paddr, NULL);
    if (ret == LOS_OK) {
        // 解除旧映射
        LOS_ArchMmuUnmap(&space->archMmu, vmPgFault->vaddr, 1);
        // 重新建立映射(使用新权限)
        ret = LOS_ArchMmuMap(&space->archMmu, vaddr, paddr, 1, region->regionFlags);
        if (ret < 0) {
            VM_ERR("LOS_ArchMmuMap failed. ret=%d", ret);
            return LOS_ERRNO_VM_NO_MEMORY;
        }

        // 标记页面为脏页
        LOS_SpinLockSave(&region->unTypeData.rf.vnode->mapping.list_lock, &intSave);
        fpage = OsFindGetEntry(&region->unTypeData.rf.vnode->mapping, vmPgFault->pgoff);
        if (fpage) {
            OsMarkPageDirty(fpage, region, 0, 0);  // 标记脏页，触发回写
        }
        LOS_SpinUnlockRestore(&region->unTypeData.rf.vnode->mapping.list_lock, intSave);

        return LOS_OK;
    }

    // 获取文件映射互斥锁
    (VOID)LOS_MuxAcquire(&region->unTypeData.rf.vnode->mapping.mux_lock);
    // 调用文件系统fault接口
    ret = region->unTypeData.rf.vmFOps->fault(region, vmPgFault);
    if (ret == LOS_OK) {
        paddr = LOS_PaddrQuery(vmPgFault->pageKVaddr);  // 获取物理地址
        page = LOS_VmPageGet(paddr);                    // 获取页面结构体
         /* 防御性检查：页面指针非空 */
        if (page != NULL) {
            LOS_AtomicInc(&page->refCounts);  // 增加引用计数
            OsCleanPageLocked(page);          // 清理页面锁定状态
        }
        // 建立内存映射
        ret = LOS_ArchMmuMap(&space->archMmu, vaddr, paddr, 1, region->regionFlags);
        if (ret < 0) {
            VM_ERR("LOS_ArchMmuMap failed. ret=%d", ret);
            OsDelMapInfo(region, vmPgFault, TRUE);  // 删除映射信息
            (VOID)LOS_MuxRelease(&region->unTypeData.rf.vnode->mapping.mux_lock);
            return LOS_ERRNO_VM_NO_MEMORY;
        }

        (VOID)LOS_MuxRelease(&region->unTypeData.rf.vnode->mapping.mux_lock);
        return LOS_OK;
    }
    (VOID)LOS_MuxRelease(&region->unTypeData.rf.vnode->mapping.mux_lock);
    return ret;
}

/**
 * @brief 文件映射页面故障分发处理
 * @details 根据访问类型(读/写)和区域属性(共享/私有)分发到不同处理函数
 * @param[in] region 虚拟内存区域结构体指针
 * @param[in,out] vmPgFault 页面故障信息结构体
 * @param[in] flags 访问标志，包含读/写/执行权限信息
 * @return STATUS_T 处理结果，LOS_OK表示成功
 * @note 该函数是文件映射故障处理的总入口，实现了故障类型的路由逻辑
 * @par 工作流程:
 * @verbatim
 * 读操作 ──────────────→ OsDoReadFault
 *                        ↑
 * 写操作 ─→ 共享映射 ─→ OsDoSharedFault
 *          ↓
 *          私有映射 ─→ OsDoCowFault
 * @endverbatim
 */
STATIC STATUS_T OsDoFileFault(LosVmMapRegion *region, LosVmPgFault *vmPgFault, UINT32 flags)
{
    STATUS_T ret;

    if (flags & VM_MAP_PF_FLAG_WRITE) {  // 写操作
        if (region->regionFlags & VM_MAP_REGION_FLAG_SHARED) {  // 共享映射
            ret = OsDoSharedFault(region, vmPgFault);
        } else {  // 私有映射(COW)
            ret = OsDoCowFault(region, vmPgFault);
        }
    } else {  // 读操作
        ret = OsDoReadFault(region, vmPgFault);
    }
    return ret;
}

/**
 * @brief 虚拟内存页面故障处理总入口
 * @details 处理所有虚拟内存页面故障，是虚拟内存子系统的核心函数之一
 * @param[in] vaddr 触发故障的虚拟地址
 * @param[in] flags 故障标志，包含访问类型和权限信息，取值为VM_MAP_PF_FLAG_xxx宏组合
 * @param[in,out] frame 异常上下文结构体指针，用于异常修复
 * @retval LOS_OK 故障处理成功
 * @retval LOS_ERRNO_VM_NOT_FOUND 虚拟地址空间或区域不存在
 * @retval LOS_ERRNO_VM_ACCESS_DENIED 访问权限被拒绝
 * @retval LOS_ERRNO_VM_NO_MEMORY 内存不足或分配失败
 * @note 该函数会被异常处理程序直接调用，需要确保快速响应和正确的错误恢复
 * @warning 错误处理路径必须小心清理资源，避免内存泄漏
 * @see OsVmRegionPermissionCheck, OsDoFileFault, OsFaultTryFixup
 */
STATUS_T OsVmPageFaultHandler(VADDR_T vaddr, UINT32 flags, ExcContext *frame)
{
    LosVmSpace *space = LOS_SpaceGet(vaddr);  // 获取虚拟地址空间
    LosVmMapRegion *region = NULL;            // 虚拟内存区域
    STATUS_T status;                          // 处理状态
    PADDR_T oldPaddr;                         // 旧物理地址
    PADDR_T newPaddr;                         // 新物理地址
    VADDR_T excVaddr = vaddr;                 // 异常虚拟地址
    LosVmPage *newPage = NULL;                // 新页面结构体
    LosVmPgFault vmPgFault = { 0 };           // 页面故障信息结构体

    // 检查虚拟地址空间是否存在
    if (space == NULL) {
        VM_ERR("vm space not exists, vaddr: %#x", vaddr);
        status = LOS_ERRNO_VM_NOT_FOUND;
        OsFaultTryFixup(frame, excVaddr, &status);  // 尝试异常修复
        return status;
    }

    // 检查用户空间访问合法性
    if (((flags & VM_MAP_PF_FLAG_USER) != 0) && (!LOS_IsUserAddress(vaddr))) {
        VM_ERR("user space not allowed to access invalid address: %#x", vaddr);
        return LOS_ERRNO_VM_ACCESS_DENIED;
    }

#ifdef LOSCFG_KERNEL_PLIMITS  // 内存限制功能
    // 检查内存限制
    if (OsMemLimitCheckAndMemAdd(PAGE_SIZE) != LOS_OK) {
        return LOS_ERRNO_VM_NO_MEMORY;
    }
#endif

    // 获取区域互斥锁，保护区域查找
    (VOID)LOS_MuxAcquire(&space->regionMux);
    // 查找地址所属的虚拟内存区域
    region = LOS_RegionFind(space, vaddr);
    if (region == NULL) {
        VM_ERR("region not exists, vaddr: %#x", vaddr);
        status = LOS_ERRNO_VM_NOT_FOUND;
        goto CHECK_FAILED;  // 跳转至检查失败处理
    }

    // 检查区域访问权限
    status = OsVmRegionPermissionCheck(region, flags);
    if (status != LOS_OK) {
        status = LOS_ERRNO_VM_ACCESS_DENIED;
        goto CHECK_FAILED;  // 跳转至检查失败处理
    }

    // 检查内存是否充足
    if (OomCheckProcess()) {
        /*
         * 内存不足时，用户进程内存分配会失败
         * 结果为LOS_NOK且当前用户进程会被删除
         * 同时打印内存使用详情
         */
        status = LOS_ERRNO_VM_NO_MEMORY;
        goto CHECK_FAILED;  // 跳转至检查失败处理
    }

    vaddr = ROUNDDOWN(vaddr, PAGE_SIZE);  // 将地址对齐到页面边界
#ifdef LOSCFG_FS_VFS  // 文件系统支持
    // 检查是否为文件映射区域
    if (LOS_IsRegionFileValid(region)) {
        if (region->unTypeData.rf.vnode == NULL) {
            goto  CHECK_FAILED;  // 文件节点无效
        }
        // 初始化页面故障信息
        vmPgFault.vaddr = vaddr;
        vmPgFault.pgoff = ((vaddr - region->range.base) >> PAGE_SHIFT) + region->pgOff;
        vmPgFault.flags = flags;
        vmPgFault.pageKVaddr = NULL;

        // 处理文件映射故障
        status = OsDoFileFault(region, &vmPgFault, flags);
        if (status) {
            VM_ERR("vm fault error, status=%d", status);
            goto CHECK_FAILED;  // 跳转至检查失败处理
        }
        goto DONE;  // 处理成功，跳转至正常出口
    }
#endif

    // 分配新的物理页面(匿名映射)
    newPage = LOS_PhysPageAlloc();
    if (newPage == NULL) {
        status = LOS_ERRNO_VM_NO_MEMORY;
        goto CHECK_FAILED;  // 跳转至检查失败处理
    }

    newPaddr = VM_PAGE_TO_PHYS(newPage);  // 计算物理地址
    // 将页面清零(安全要求)
    (VOID)memset_s(OsVmPageToVaddr(newPage), PAGE_SIZE, 0, PAGE_SIZE);
    // 查询旧物理地址
    status = LOS_ArchMmuQuery(&space->archMmu, vaddr, &oldPaddr, NULL);
    if (status >= 0) {
        // 解除旧映射
        LOS_ArchMmuUnmap(&space->archMmu, vaddr, 1);
        // 复制共享页面
        OsPhysSharePageCopy(oldPaddr, &newPaddr, newPage);
        // 如果新地址与旧地址相同，表示共享页面无需复制
        if (newPaddr == oldPaddr) {
            LOS_PhysPageFree(newPage);  // 释放新页面
            newPage = NULL;
        }

        // 建立新映射
        status = LOS_ArchMmuMap(&space->archMmu, vaddr, newPaddr, 1, region->regionFlags);
        if (status < 0) {
            VM_ERR("failed to map replacement page, status:%d", status);
            status = LOS_ERRNO_VM_MAP_FAILED;
            goto VMM_MAP_FAILED;  // 跳转至映射失败处理
        }

        status = LOS_OK;
        goto DONE;  // 跳转至正常出口
    } else {
        // 建立新映射(首次映射)
        LOS_AtomicInc(&newPage->refCounts);  // 增加引用计数
        status = LOS_ArchMmuMap(&space->archMmu, vaddr, newPaddr, 1, region->regionFlags);
        if (status < 0) {
            VM_ERR("failed to map page, status:%d", status);
            status = LOS_ERRNO_VM_MAP_FAILED;
            goto VMM_MAP_FAILED;  // 跳转至映射失败处理
        }
    }

    status = LOS_OK;
    goto DONE;  // 跳转至正常出口

VMM_MAP_FAILED:  // 映射失败处理
    if (newPage != NULL) {
        LOS_PhysPageFree(newPage);  // 释放新页面
    }
CHECK_FAILED:  // 检查失败处理
    OsFaultTryFixup(frame, excVaddr, &status);  // 尝试异常修复
#ifdef LOSCFG_KERNEL_PLIMITS
    OsMemLimitMemFree(PAGE_SIZE);  // 释放内存限制计数
#endif
DONE:  // 正常出口
    (VOID)LOS_MuxRelease(&space->regionMux);  // 释放区域互斥锁
    return status;
}

#endif

