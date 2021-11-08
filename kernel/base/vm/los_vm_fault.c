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

extern char __exc_table_start[];
extern char __exc_table_end[];
//线性区正确性检查
STATIC STATUS_T OsVmRegionRightCheck(LosVmMapRegion *region, UINT32 flags)
{
    if ((region->regionFlags & VM_MAP_REGION_FLAG_PERM_READ) != VM_MAP_REGION_FLAG_PERM_READ) {
        VM_ERR("read permission check failed operation flags %x, region flags %x", flags, region->regionFlags);
        return LOS_NOK;
    }

    if ((flags & VM_MAP_PF_FLAG_WRITE) == VM_MAP_PF_FLAG_WRITE) {//写入许可
        if ((region->regionFlags & VM_MAP_REGION_FLAG_PERM_WRITE) != VM_MAP_REGION_FLAG_PERM_WRITE) {
            VM_ERR("write permission check failed operation flags %x, region flags %x", flags, region->regionFlags);
            return LOS_NOK;
        }
    }

    if ((flags & VM_MAP_PF_FLAG_INSTRUCTION) == VM_MAP_PF_FLAG_INSTRUCTION) {//指令
        if ((region->regionFlags & VM_MAP_REGION_FLAG_PERM_EXECUTE) != VM_MAP_REGION_FLAG_PERM_EXECUTE) {
            VM_ERR("exec permission check failed operation flags %x, region flags %x", flags, region->regionFlags);
            return LOS_NOK;
        }
    }

    return LOS_OK;
}

STATIC VOID OsFaultTryFixup(ExcContext *frame, VADDR_T excVaddr, STATUS_T *status)
{
    INT32 tableNum = (__exc_table_end - __exc_table_start) / sizeof(LosExcTable);
    LosExcTable *excTable = (LosExcTable *)__exc_table_start;

    if ((frame->regCPSR & CPSR_MODE_MASK) != CPSR_MODE_USR) {
        for (int i = 0; i < tableNum; ++i, ++excTable) {
            if (frame->PC == (UINTPTR)excTable->excAddr) {
                frame->PC = (UINTPTR)excTable->fixAddr;
                frame->R2 = (UINTPTR)excVaddr;
                *status = LOS_OK;
                return;
            }
        }
    }
}

#ifdef LOSCFG_FS_VFS 
//读页时发生缺页的处理
STATIC STATUS_T OsDoReadFault(LosVmMapRegion *region, LosVmPgFault *vmPgFault)//读缺页
{
    status_t ret;
    PADDR_T paddr;
    LosVmPage *page = NULL;
    VADDR_T vaddr = (VADDR_T)vmPgFault->vaddr;
    LosVmSpace *space = region->space;

    ret = LOS_ArchMmuQuery(&space->archMmu, vaddr, NULL, NULL);//查询是否缺页
    if (ret == LOS_OK) {//注意这里时LOS_OK却返回,都OK了说明查到了物理地址,有页了。
        return LOS_OK;//查到了就说明不缺页的，缺页就是因为虚拟地址没有映射到物理地址嘛
    }
    if (region->unTypeData.rf.vmFOps == NULL || region->unTypeData.rf.vmFOps->fault == NULL) {//线性区必须有实现了缺页接口
        VM_ERR("region args invalid, file path: %s", region->unTypeData.rf.vnode->filePath);
        return LOS_ERRNO_VM_INVALID_ARGS;
    }

    (VOID)LOS_MuxAcquire(&region->unTypeData.rf.vnode->mapping.mux_lock);
    ret = region->unTypeData.rf.vmFOps->fault(region, vmPgFault);// 函数指针，执行的是g_commVmOps.OsVmmFileFault
    if (ret == LOS_OK) {
        paddr = LOS_PaddrQuery(vmPgFault->pageKVaddr);//查询物理地址
        page = LOS_VmPageGet(paddr);//获取page
        if (page != NULL) { /* just incase of page null */
            LOS_AtomicInc(&page->refCounts);//ref 自增
            OsCleanPageLocked(page);
        }
        ret = LOS_ArchMmuMap(&space->archMmu, vaddr, paddr, 1,
                             region->regionFlags & (~VM_MAP_REGION_FLAG_PERM_WRITE));//重新映射为非可写
        if (ret < 0) {
            VM_ERR("LOS_ArchMmuMap failed");
            OsDelMapInfo(region, vmPgFault, false);
            (VOID)LOS_MuxRelease(&region->unTypeData.rf.vnode->mapping.mux_lock);
            return LOS_ERRNO_VM_NO_MEMORY;
        }

        (VOID)LOS_MuxRelease(&region->unTypeData.rf.vnode->mapping.mux_lock);
        return LOS_OK;
    }
    (VOID)LOS_MuxRelease(&region->unTypeData.rf.vnode->mapping.mux_lock);

    return LOS_ERRNO_VM_NO_MEMORY;
}

/* numap a page when cow happend only *///仅当写时拷贝发生时取消页面映射
STATIC LosVmPage *OsCowUnmapOrg(LosArchMmu *archMmu, LosVmMapRegion *region, LosVmPgFault *vmf)
{
    UINT32 intSave;
    LosVmPage *oldPage = NULL;
    LosMapInfo *mapInfo = NULL;
    LosFilePage *fpage = NULL;
    VADDR_T vaddr = (VADDR_T)vmf->vaddr;

    LOS_SpinLockSave(&region->unTypeData.rf.vnode->mapping.list_lock, &intSave);
    fpage = OsFindGetEntry(&region->unTypeData.rf.vnode->mapping, vmf->pgoff);
    if (fpage != NULL) {
        oldPage = fpage->vmPage;
        OsSetPageLocked(oldPage);
        mapInfo = OsGetMapInfo(fpage, archMmu, vaddr);
        if (mapInfo != NULL) {
            OsUnmapPageLocked(fpage, mapInfo);
        } else {
            LOS_ArchMmuUnmap(archMmu, vaddr, 1);
        }
    } else {
        LOS_ArchMmuUnmap(archMmu, vaddr, 1);
    }
    LOS_SpinUnlockRestore(&region->unTypeData.rf.vnode->mapping.list_lock, intSave);

    return oldPage;
}
#endif
//在私有线性区写入文件时发生缺页的处理
status_t OsDoCowFault(LosVmMapRegion *region, LosVmPgFault *vmPgFault)
{
    STATUS_T ret;
    VOID *kvaddr = NULL;
    PADDR_T oldPaddr = 0;
    PADDR_T newPaddr;
    LosVmPage *oldPage = NULL;
    LosVmPage *newPage = NULL;
    LosVmSpace *space = NULL;

    if ((vmPgFault == NULL) || (region == NULL) ||
        (region->unTypeData.rf.vmFOps == NULL) || (region->unTypeData.rf.vmFOps->fault == NULL)) {
        VM_ERR("region args invalid");
        return LOS_ERRNO_VM_INVALID_ARGS;
    }

    space = region->space;
    ret = LOS_ArchMmuQuery(&space->archMmu, (VADDR_T)vmPgFault->vaddr, &oldPaddr, NULL);//查询出老物理地址
    if (ret == LOS_OK) {
        oldPage = OsCowUnmapOrg(&space->archMmu, region, vmPgFault);//取消页面映射
    }

    newPage = LOS_PhysPageAlloc();//分配一个新页面
    if (newPage == NULL) {
        VM_ERR("pmm_alloc_page fail");
        ret = LOS_ERRNO_VM_NO_MEMORY;
        goto ERR_OUT;
    }

    newPaddr = VM_PAGE_TO_PHYS(newPage);//拿到新的物理地址
    kvaddr = OsVmPageToVaddr(newPage);//拿到新的虚拟地址

    (VOID)LOS_MuxAcquire(&region->unTypeData.rf.vnode->mapping.mux_lock);
    ret = region->unTypeData.rf.vmFOps->fault(region, vmPgFault);// 函数指针 g_commVmOps.OsVmmFileFault
    if (ret != LOS_OK) {
        VM_ERR("call region->vm_ops->fault fail");
        (VOID)LOS_MuxRelease(&region->unTypeData.rf.vnode->mapping.mux_lock);
        goto ERR_OUT;
    }

    /**
     * here we get two conditions, 1.this page hasn't mapped or mapped from pagecache,
     * we can take it as a normal file cow map. 2.this page has done file cow map,
     * we can take it as a anonymous cow map.
     */
    if ((oldPaddr == 0) || (LOS_PaddrToKVaddr(oldPaddr) == vmPgFault->pageKVaddr)) {//没有映射或者 已在pagecache有映射
        (VOID)memcpy_s(kvaddr, PAGE_SIZE, vmPgFault->pageKVaddr, PAGE_SIZE);//直接copy到新页
        LOS_AtomicInc(&newPage->refCounts);//引用ref++
        OsCleanPageLocked(LOS_VmPageGet(LOS_PaddrQuery(vmPgFault->pageKVaddr)));//解锁
    } else {
        OsPhysSharePageCopy(oldPaddr, &newPaddr, newPage);//调用之前 oldPaddr肯定不等于newPaddr
        /* use old page free the new one */
        if (newPaddr == oldPaddr) {//注意这里newPaddr可能已经被改变了,参数传入的是 &newPaddr 
            LOS_PhysPageFree(newPage);//释放新页,别浪费的内存,内核使用内存是一分钱当十块用.
            newPage = NULL;
        }
    }

    ret = LOS_ArchMmuMap(&space->archMmu, (VADDR_T)vmPgFault->vaddr, newPaddr, 1, region->regionFlags);//把新物理地址映射给缺页的虚拟地址,这样就不会缺页啦
    if (ret < 0) {
        VM_ERR("LOS_ArchMmuMap failed");
        ret =  LOS_ERRNO_VM_NO_MEMORY;
        (VOID)LOS_MuxRelease(&region->unTypeData.rf.vnode->mapping.mux_lock);
        goto ERR_OUT;
    }
    (VOID)LOS_MuxRelease(&region->unTypeData.rf.vnode->mapping.mux_lock);

    if (oldPage != NULL) {
        OsCleanPageLocked(oldPage);
    }

    return LOS_OK;

ERR_OUT:
    if (newPage != NULL) {
        LOS_PhysPageFree(newPage);
    }
    if (oldPage != NULL) {
        OsCleanPageLocked(oldPage);
    }

    return ret;
}
///在共享线性区写文件操作发生缺页的情况处理,因为线性区是共享的
status_t OsDoSharedFault(LosVmMapRegion *region, LosVmPgFault *vmPgFault)
{
    STATUS_T ret;
    UINT32 intSave;
    PADDR_T paddr = 0;
    VADDR_T vaddr = (VADDR_T)vmPgFault->vaddr;
    LosVmSpace *space = region->space;
    LosVmPage *page = NULL;
    LosFilePage *fpage = NULL;

    if ((region->unTypeData.rf.vmFOps == NULL) || (region->unTypeData.rf.vmFOps->fault == NULL)) {
        VM_ERR("region args invalid");
        return LOS_ERRNO_VM_INVALID_ARGS;
    }

    ret = LOS_ArchMmuQuery(&space->archMmu, vmPgFault->vaddr, &paddr, NULL);//查询物理地址
    if (ret == LOS_OK) {
        LOS_ArchMmuUnmap(&space->archMmu, vmPgFault->vaddr, 1);//先取消映射
        ret = LOS_ArchMmuMap(&space->archMmu, vaddr, paddr, 1, region->regionFlags);//再重新映射,为啥这么干,是因为regionFlags变了,
        if (ret < 0) {
            VM_ERR("LOS_ArchMmuMap failed. ret=%d", ret);
            return LOS_ERRNO_VM_NO_MEMORY;
        }

        LOS_SpinLockSave(&region->unTypeData.rf.vnode->mapping.list_lock, &intSave);
        fpage = OsFindGetEntry(&region->unTypeData.rf.vnode->mapping, vmPgFault->pgoff);
        if (fpage) {//在页高速缓存(page cache)中找到了
            OsMarkPageDirty(fpage, region, 0, 0);//标记为脏页
        }
        LOS_SpinUnlockRestore(&region->unTypeData.rf.vnode->mapping.list_lock, intSave);

        return LOS_OK;
    }
	//以下是没有映射到物理地址的处理
    (VOID)LOS_MuxAcquire(&region->unTypeData.rf.vnode->mapping.mux_lock);
    ret = region->unTypeData.rf.vmFOps->fault(region, vmPgFault);//函数指针，执行的是g_commVmOps.OsVmmFileFault
    if (ret == LOS_OK) {
        paddr = LOS_PaddrQuery(vmPgFault->pageKVaddr);
        page = LOS_VmPageGet(paddr);
         /* just in case of page null */
        if (page != NULL) {
            LOS_AtomicInc(&page->refCounts);
            OsCleanPageLocked(page);
        }
        ret = LOS_ArchMmuMap(&space->archMmu, vaddr, paddr, 1, region->regionFlags);
        if (ret < 0) {
            VM_ERR("LOS_ArchMmuMap failed. ret=%d", ret);
            OsDelMapInfo(region, vmPgFault, TRUE);
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
 * Page read operation is a simple case, just share the pagecache(save memory)
 * and make a read permission mmapping (region->arch_mmu_flags & (~ARCH_MMU_FLAG_PERM_WRITE)).
 * However for write operation, vmflag (VM_PRIVATE|VM_SHREAD) decides COW or SHARED fault.
 * For COW fault, pagecache is copied to private anonyous pages and the changes on this page
 * won't write through to the underlying file. For SHARED fault, pagecache is mapping with
 * region->arch_mmu_flags and the changes on this page will write through to the underlying file
 */ //操作文件时产生缺页中断
STATIC STATUS_T OsDoFileFault(LosVmMapRegion *region, LosVmPgFault *vmPgFault, UINT32 flags)
{
    STATUS_T ret;

    if (flags & VM_MAP_PF_FLAG_WRITE) {//写页的时候产生缺页
        if (region->regionFlags & VM_MAP_REGION_FLAG_SHARED) {//共享线性区
            ret = OsDoSharedFault(region, vmPgFault);//写操作时的共享缺页,最复杂,此页上的更改将写入磁盘文件
        } else {//非共享线性区
            ret = OsDoCowFault(region, vmPgFault);//(写时拷贝技术)写操作时的私有缺页,pagecache被复制到私有的任意一个页面上，并在此页面上进行更改,不会直接写入磁盘文件
        }
    } else {//读页的时候产生缺页
        ret = OsDoReadFault(region, vmPgFault);//页面读取操作很简单，只需共享页面缓存（节省内存）并进行读权限映射（region->arch_mmu_flags&（~arch_mmu_FLAG_PERM_WRITE））
    }
    return ret;
}
/***************************************************************
缺页中断处理程序
通常有两种情况导致
第一种:由编程错误引起的异常
第二种:属于进程的地址空间范围但还尚未分配物理页框引起的异常
***************************************************************/

/**
 * @brief 
 * @param vaddr 
 * @param flags 
 * @param frame 
 * @return STATUS_T 
 */
STATUS_T OsVmPageFaultHandler(VADDR_T vaddr, UINT32 flags, ExcContext *frame)
{
    LosVmSpace *space = LOS_SpaceGet(vaddr);//获取虚拟地址所属空间
    LosVmMapRegion *region = NULL;
    STATUS_T status;
    PADDR_T oldPaddr;
    PADDR_T newPaddr;
    VADDR_T excVaddr = vaddr;
    LosVmPage *newPage = NULL;
    LosVmPgFault vmPgFault = { 0 };

    if (space == NULL) {
        VM_ERR("vm space not exists, vaddr: %#x", vaddr);
        status = LOS_ERRNO_VM_NOT_FOUND;
        OsFaultTryFixup(frame, excVaddr, &status);
        return status;
    }

    if (((flags & VM_MAP_PF_FLAG_USER) != 0) && (!LOS_IsUserAddress(vaddr))) {//地址保护,用户空间不允许跨界访问
        VM_ERR("user space not allowed to access invalid address: %#x", vaddr);
        return LOS_ERRNO_VM_ACCESS_DENIED;//拒绝访问
    }

    (VOID)LOS_MuxAcquire(&space->regionMux);
    region = LOS_RegionFind(space, vaddr);//通过虚拟地址找到所在线性区
    if (region == NULL) {
        VM_ERR("region not exists, vaddr: %#x", vaddr);
        status = LOS_ERRNO_VM_NOT_FOUND;
        goto CHECK_FAILED;
    }

    status = OsVmRegionRightCheck(region, flags);
    if (status != LOS_OK) {
        status = LOS_ERRNO_VM_ACCESS_DENIED;//拒绝访问
        goto CHECK_FAILED;
    }

    if (OomCheckProcess()) {//低内存检查
        /*
         * under low memory, when user process request memory allocation
         * it will fail, and result is LOS_NOK and current user process
         * will be deleted. memory usage detail will be printed.
         */
        status = LOS_ERRNO_VM_NO_MEMORY;
        goto CHECK_FAILED;
    }

    vaddr = ROUNDDOWN(vaddr, PAGE_SIZE);//为啥要向下圆整，因为这一页要重新使用，需找到页面基地址
#ifdef LOSCFG_FS_VFS 
    if (LOS_IsRegionFileValid(region)) {//是否为文件线性区
        if (region->unTypeData.rf.vnode == NULL) {
            goto  CHECK_FAILED;
        }
        vmPgFault.vaddr = vaddr;//虚拟地址
        vmPgFault.pgoff = ((vaddr - region->range.base) >> PAGE_SHIFT) + region->pgOff;//计算出文件读取位置
        vmPgFault.flags = flags;
        vmPgFault.pageKVaddr = NULL;//缺失页初始化没有物理地址

        status = OsDoFileFault(region, &vmPgFault, flags);//缺页处理
        if (status) {
            VM_ERR("vm fault error, status=%d", status);
            goto CHECK_FAILED;
        }
        goto DONE;
    }
#endif
	//请求调页:推迟到不能再推迟为止
    newPage = LOS_PhysPageAlloc();//分配一个新的物理页
    if (newPage == NULL) {
        status = LOS_ERRNO_VM_NO_MEMORY;
        goto CHECK_FAILED;
    }

    newPaddr = VM_PAGE_TO_PHYS(newPage);//获取物理地址
    (VOID)memset_s(OsVmPageToVaddr(newPage), PAGE_SIZE, 0, PAGE_SIZE);//获取虚拟地址 清0
    status = LOS_ArchMmuQuery(&space->archMmu, vaddr, &oldPaddr, NULL);//通过虚拟地址查询老物理地址
    if (status >= 0) {//已经映射过了,@note_thinking 不是缺页吗,怎么会有页的情况? 
        LOS_ArchMmuUnmap(&space->archMmu, vaddr, 1);//解除映射关系
        OsPhysSharePageCopy(oldPaddr, &newPaddr, newPage);//将oldPaddr的数据拷贝到newPage
        /* use old page free the new one */
        if (newPaddr == oldPaddr) {//新老物理地址一致
            LOS_PhysPageFree(newPage);//继续使用旧页释放新页
            newPage = NULL;
        }

        /* map all of the pages */
        status = LOS_ArchMmuMap(&space->archMmu, vaddr, newPaddr, 1, region->regionFlags);//重新映射新物理地址
        if (status < 0) {
            VM_ERR("failed to map replacement page, status:%d", status);
            status = LOS_ERRNO_VM_MAP_FAILED;
            goto VMM_MAP_FAILED;
        }

        status = LOS_OK;
        goto DONE;
    } else {//
        /* map all of the pages */
        LOS_AtomicInc(&newPage->refCounts);//引用数自增
        status = LOS_ArchMmuMap(&space->archMmu, vaddr, newPaddr, 1, region->regionFlags);//映射新物理地址,如此下次就不会缺页了
        if (status < 0) {
            VM_ERR("failed to map page, status:%d", status);
            status = LOS_ERRNO_VM_MAP_FAILED;
            goto VMM_MAP_FAILED;
        }
    }

    status = LOS_OK;
    goto DONE;
VMM_MAP_FAILED:
    if (newPage != NULL) {
        LOS_PhysPageFree(newPage);
    }
CHECK_FAILED:
    OsFaultTryFixup(frame, excVaddr, &status);
DONE:
    (VOID)LOS_MuxRelease(&space->regionMux);
    return status;
}
#endif

