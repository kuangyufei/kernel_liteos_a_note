/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
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
 * @defgroup los_vm_syscall vm syscall definition
 * @ingroup kernel
 */

#include "los_typedef.h"
#include "los_vm_syscall.h"
#include "los_vm_common.h"
#include "los_rbtree.h"
#include "los_vm_map.h"
#include "los_vm_dump.h"
#include "los_vm_lock.h"
#include "los_vm_filemap.h"
#include "los_process_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
//检查用户空间虚拟内存参数
STATUS_T OsCheckMMapParams(VADDR_T vaddr, unsigned prot, unsigned long flags, size_t len, unsigned long pgoff)
{
    if ((vaddr != 0) && !LOS_IsUserAddressRange(vaddr, len)) {//因系统调用,[vaddr,vaddr+len]必须在用户空间
        return -EINVAL;
    }

    if (len == 0) {
        return -EINVAL;
    }

    /* we only support some prot and flags */
    if ((prot & PROT_SUPPORT_MASK) == 0) {//不能超过权限范围
        return -EINVAL;
    }
    if ((flags & MAP_SUPPORT_MASK) == 0) {//映射权限限制
        return -EINVAL;
    }
    if (((flags & MAP_SHARED_PRIVATE) == 0) || ((flags & MAP_SHARED_PRIVATE) == MAP_SHARED_PRIVATE)) {
        return -EINVAL;
    }

    if (((len >> PAGE_SHIFT) + pgoff) < pgoff) {
        return -EINVAL;
    }

    return LOS_OK;
}
//线性区映射类型:匿名映射
STATUS_T OsAnonMMap(LosVmMapRegion *region)
{
    LOS_SetRegionTypeAnon(region);
    return LOS_OK;
}
/**************************************************
系统调用|申请虚拟内存
参数		描述		
addr	用来请求使用某个特定的虚拟内存地址。如果取NULL，结果地址就将自动分配（这是推荐的做法），
		否则会降低程序的可移植性，因为不同系统的可用地址范围不一样。
length	内存段的大小。
prot	用于设置内存段的访问权限，有如下权限：
		PROT_READ：允许读该内存段。
		PROT_WRITE：允许写该内存段。
		PROT_EXEC：允许执行该内存段。
		PROT_NONE：不能访问。
flags	控制程序对内存段的改变所造成的影响，有如下属性：
		MAP_PRIVATE：内存段私有，对它的修改值仅对本进程有效。
		MAP_SHARED：把对该内存段的修改保存到磁盘文件中。
fd		打开的文件描述符。
offset	以文件开始处的偏移量, 必须是分页大小的整数倍, 通常为0, 表示从文件头开始映射。
成功返回：虚拟内存地址，这地址是页对齐。
失败返回：(void *)-1。
**************************************************/
VADDR_T LOS_MMap(VADDR_T vaddr, size_t len, unsigned prot, unsigned long flags, int fd, unsigned long pgoff)
{
    STATUS_T status;
    VADDR_T resultVaddr;
    UINT32 regionFlags;
    LosVmMapRegion *newRegion = NULL;//应用的内存分配对应到内核就是分配一个线性区
    struct file *filep = NULL;// inode : file = 1:N ,一对多关系,一个inode可以被多个进程打开,返回不同的file但都指向同一个inode 
    LosVmSpace *vmSpace = OsCurrProcessGet()->vmSpace;

    vaddr = ROUNDUP(vaddr, PAGE_SIZE);
    len = ROUNDUP(len, PAGE_SIZE);
    STATUS_T checkRst = OsCheckMMapParams(vaddr, prot, flags, len, pgoff);
    if (checkRst != LOS_OK) {
        return checkRst;
    }
	
    if (LOS_IsNamedMapping(flags)) {//是否文件映射
        status = fs_getfilep(fd, &filep);//获取文件描述符和状态
        if (status < 0) {
            return -EBADF;
        }
    }

    (VOID)LOS_MuxAcquire(&vmSpace->regionMux);
    /* user mode calls mmap to release heap physical memory without releasing heap virtual space */
    status = OsUserHeapFree(vmSpace, vaddr, len);//用户模式释放堆物理内存而不释放堆虚拟空间
    if (status == LOS_OK) {//OsUserHeapFree 干两件事 1.解除映射关系 2.释放物理页
        resultVaddr = vaddr;
        goto MMAP_DONE;
    }

    regionFlags = OsCvtProtFlagsToRegionFlags(prot, flags);//将参数flag转换Region的flag
    newRegion = LOS_RegionAlloc(vmSpace, vaddr, len, regionFlags, pgoff);//分配一个线性区
    if (newRegion == NULL) {
        resultVaddr = (VADDR_T)-ENOMEM;//ENOMEM:内存溢出
        goto MMAP_DONE;
    }
    newRegion->regionFlags |= VM_MAP_REGION_FLAG_MMAP;
    resultVaddr = newRegion->range.base;

    if (LOS_IsNamedMapping(flags)) {
        status = OsNamedMMap(filep, newRegion);//文件映射
    } else {
        status = OsAnonMMap(newRegion);//匿名映射
    }

    if (status != LOS_OK) {
        LOS_RbDelNode(&vmSpace->regionRbTree, &newRegion->rbNode);//从红黑树和双循环链表中删除
        LOS_RegionFree(vmSpace, newRegion);//释放
        resultVaddr = (VADDR_T)-ENOMEM;//linux错误代码含义 ENOMEM:内存溢出
        goto MMAP_DONE;
    }

MMAP_DONE:
    (VOID)LOS_MuxRelease(&vmSpace->regionMux);
    return resultVaddr;
}
//解除映射关系
STATUS_T LOS_UnMMap(VADDR_T addr, size_t size)
{
    if ((addr <= 0) || (size <= 0)) {
        return -EINVAL;
    }

    return OsUnMMap(OsCurrProcessGet()->vmSpace, addr, size);
}
/******************************************************************
用户进程向内核申请空间，进一步说用于扩展用户堆栈空间，或者回收用户堆栈空间
扩展当前进程的堆空间
一个进程所有的线性区都在进程指定的线性地址范围内，
线性区之间是不会有地址的重叠的，开始都是连续的，随着进程的运行出现了释放再分配的情况
由此出现了断断续续的线性区，内核回收线性区时会检测是否和周边的线性区可合并成一个更大
的线性区用于分配。
******************************************************************/
VOID *LOS_DoBrk(VOID *addr)
{
    LosVmSpace *space = OsCurrProcessGet()->vmSpace;
    size_t size;
    VOID *ret = NULL;
    LosVmMapRegion *region = NULL;
    VOID *alignAddr = NULL;
    VADDR_T newBrk, oldBrk;

    if (addr == NULL) {//参数地址未传情况
        return (void *)(UINTPTR)space->heapNow;//以现有指向地址为基础进行扩展
    }

    if ((UINTPTR)addr < (UINTPTR)space->heapBase) {//heapBase是堆区的开始地址，所以参数地址不能低于它
        return (VOID *)-ENOMEM;
    }

    size = (UINTPTR)addr - (UINTPTR)space->heapBase;//算出大小
    size = ROUNDUP(size, PAGE_SIZE);	//圆整size
    alignAddr = (CHAR *)(UINTPTR)(space->heapBase) + size;//得到新的线性区的结束地址
    PRINT_INFO("brk addr %p , size 0x%x, alignAddr %p, align %d\n", addr, size, alignAddr, PAGE_SIZE);

    if (addr < (VOID *)(UINTPTR)space->heapNow) {//如果新的地址小于线性区现有的结束地址，则只需做裁剪的工作
        newBrk = LOS_Align((VADDR_T)(UINTPTR)addr, PAGE_SIZE);
        oldBrk = LOS_Align(space->heapNow, PAGE_SIZE);
        if (LOS_UnMMap(newBrk, (oldBrk - newBrk)) < 0) {//只需要裁剪掉多余部分的映射关系
            return (void *)(UINTPTR)space->heapNow;//失败返回老的结束地址
        }
        space->heapNow = (VADDR_T)(UINTPTR)addr;//更新线性区结束地址
        return addr;//返回新地址
    }

    (VOID)LOS_MuxAcquire(&space->regionMux);
    if (space->heapBase == space->heapNow) {//往往是第一次调用this函数才会出现，因为初始化时 heapBase = heapNow
        region = LOS_RegionAlloc(space, space->heapBase, size,//分配一个可读/可写/可使用的线性区，只需分配一次
                                 VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE |//线性区的大小由range.size决定
                                 VM_MAP_REGION_FLAG_PERM_USER, 0);
        if (region == NULL) {
            ret = (VOID *)-ENOMEM;
            VM_ERR("LOS_RegionAlloc failed");
            goto REGION_ALLOC_FAILED;
        }
        region->regionFlags |= VM_MAP_REGION_FLAG_HEAP;//贴上线性区类型为堆区的标签,注意一个线性区可以有多种标签
        space->heap = region;//更新虚拟空间堆区为分配的线性区
    }

    if ((UINTPTR)alignAddr >= space->mapBase) {//在进程的虚拟空间中，映射区是贴着堆区的，所以堆区的结束地址是不能超过映射区的开始地址的
        VM_ERR("Process heap memory space is insufficient");//提示进程堆内存空间不足
        goto REGION_ALLOC_FAILED;//这段判断代码应该放在 得到 alignAddr后执行 @note_thinking
    }

    space->heapNow = (VADDR_T)(UINTPTR)alignAddr;//更新线性区结束地址
    space->heap->range.size = size;	//更新堆区大小,经此操作线性区变大或缩小了
    ret = (VOID *)(UINTPTR)space->heapNow;//返回线性区最新的地址

REGION_ALLOC_FAILED:
    (VOID)LOS_MuxRelease(&space->regionMux);
    return ret;
}
//修改内存段的访问权限
int LOS_DoMprotect(VADDR_T vaddr, size_t len, unsigned long prot)
{
    LosVmSpace *space = OsCurrProcessGet()->vmSpace;
    LosVmMapRegion *region = NULL;
    UINT32 vmFlags;
    UINT32 count;
    int ret;

    (VOID)LOS_MuxAcquire(&space->regionMux);
    region = LOS_RegionFind(space, vaddr);
    if (!IS_ALIGNED(vaddr, PAGE_SIZE) || (region == NULL) || (vaddr > vaddr + len)) {
        ret = -EINVAL;
        goto OUT_MPROTECT;
    }

    if ((prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC))) {
        ret = -EINVAL;
        goto OUT_MPROTECT;
    }

    len = LOS_Align(len, PAGE_SIZE);
    /* can't operation cross region */
    if (region->range.base + region->range.size < vaddr + len) {
        ret = -EINVAL;
        goto OUT_MPROTECT;
    }

    /* if only move some part of region, we need to split first */
    if (region->range.size > len) {
        OsVmRegionAdjust(space, vaddr, len);
    }

    vmFlags = OsCvtProtFlagsToRegionFlags(prot, 0);//转换FLAGS
    vmFlags |= (region->regionFlags & VM_MAP_REGION_FLAG_SHARED) ? VM_MAP_REGION_FLAG_SHARED : 0;
    region = LOS_RegionFind(space, vaddr);
    if (region == NULL) {
        ret = -ENOMEM;
        goto OUT_MPROTECT;
    }
    region->regionFlags = vmFlags;
    count = len >> PAGE_SHIFT;
    ret = LOS_ArchMmuChangeProt(&space->archMmu, vaddr, count, region->regionFlags);//修改访问权限实体函数
    if (ret) {
        ret = -ENOMEM;
        goto OUT_MPROTECT;
    }
    ret = LOS_OK;

OUT_MPROTECT:
#ifdef LOSCFG_VM_OVERLAP_CHECK
    if (VmmAspaceRegionsOverlapCheck(aspace) < 0) {
        (VOID)OsShellCmdDumpVm(0, NULL);
        ret = -ENOMEM;
    }
#endif

    (VOID)LOS_MuxRelease(&space->regionMux);
    return ret;
}

STATUS_T OsMremapCheck(VADDR_T addr, size_t oldLen, VADDR_T newAddr, size_t newLen, unsigned int flags)
{
    LosVmSpace *space = OsCurrProcessGet()->vmSpace;
    LosVmMapRegion *region = LOS_RegionFind(space, addr);
    VADDR_T regionEnd;

    if ((region == NULL) || (region->range.base > addr) || (newLen == 0)) {
        return -EINVAL;
    }

    if (flags & ~(MREMAP_FIXED | MREMAP_MAYMOVE)) {
        return -EINVAL;
    }

    if (((flags & MREMAP_FIXED) == MREMAP_FIXED) && ((flags & MREMAP_MAYMOVE) == 0)) {
        return -EINVAL;
    }

    if (!IS_ALIGNED(addr, PAGE_SIZE)) {
        return -EINVAL;
    }

    regionEnd = region->range.base + region->range.size;

    /* we can't operate across region */
    if (oldLen > regionEnd - addr) {
        return -EFAULT;
    }

    /* avoiding overflow */
    if (newLen > oldLen) {
        if ((addr + newLen) < addr) {
            return -EINVAL;
        }
    }

    /* avoid new region overlaping with the old one */
    if (flags & MREMAP_FIXED) {
        if (((region->range.base + region->range.size) > newAddr) &&
            (region->range.base < (newAddr + newLen))) {
            return -EINVAL;
        }

        if (!IS_ALIGNED(newAddr, PAGE_SIZE)) {
            return -EINVAL;
        }
    }

    return LOS_OK;
}
//重新映射虚拟内存地址。
VADDR_T LOS_DoMremap(VADDR_T oldAddress, size_t oldSize, size_t newSize, int flags, VADDR_T newAddr)
{
    LosVmMapRegion *regionOld = NULL;
    LosVmMapRegion *regionNew = NULL;
    STATUS_T status;
    VADDR_T ret;
    LosVmSpace *space = OsCurrProcessGet()->vmSpace;

    oldSize = LOS_Align(oldSize, PAGE_SIZE);
    newSize = LOS_Align(newSize, PAGE_SIZE);

    (VOID)LOS_MuxAcquire(&space->regionMux);

    status = OsMremapCheck(oldAddress, oldSize, newAddr, newSize, (unsigned int)flags);
    if (status) {
        ret = status;
        goto OUT_MREMAP;
    }

    /* if only move some part of region, we need to split first */
    status = OsVmRegionAdjust(space, oldAddress, oldSize);
    if (status) {
        ret = -ENOMEM;
        goto OUT_MREMAP;
    }

    regionOld = LOS_RegionFind(space, oldAddress);
    if (regionOld == NULL) {
        ret = -ENOMEM;
        goto OUT_MREMAP;
    }

    if ((unsigned int)flags & MREMAP_FIXED) {
        regionNew = OsVmRegionDup(space, regionOld, newAddr, newSize);
        if (!regionNew) {
            ret = -ENOMEM;
            goto OUT_MREMAP;
        }
        status = LOS_ArchMmuMove(&space->archMmu, oldAddress, newAddr, newSize >> PAGE_SHIFT, regionOld->regionFlags);
        if (status) {
            LOS_RegionFree(space, regionNew);
            ret = -ENOMEM;
            goto OUT_MREMAP;
        }
        LOS_RegionFree(space, regionOld);
        ret = newAddr;
        goto OUT_MREMAP;
    }
    // take it as shrink operation
    if (oldSize > newSize) {
        LOS_UnMMap(oldAddress + newSize, oldSize - newSize);
        ret = oldAddress;
        goto OUT_MREMAP;
    }
    status = OsIsRegionCanExpand(space, regionOld, newSize);
    // we can expand directly.
    if (!status) {
        regionOld->range.size = newSize;
        ret = oldAddress;
        goto OUT_MREMAP;
    }

    if ((unsigned int)flags & MREMAP_MAYMOVE) {
        regionNew = OsVmRegionDup(space, regionOld, 0, newSize);
        if (regionNew  == NULL) {
            ret = -ENOMEM;
            goto OUT_MREMAP;
        }
        status = LOS_ArchMmuMove(&space->archMmu, oldAddress, regionNew->range.base, newSize >> PAGE_SHIFT,
                                 regionOld->regionFlags);
        if (status) {
            LOS_RegionFree(space, regionNew);
            ret = -ENOMEM;
            goto OUT_MREMAP;
        }
        LOS_RegionFree(space, regionOld);
        ret = regionNew->range.base;
        goto OUT_MREMAP;
    }

    ret = -EINVAL;
OUT_MREMAP:
#ifdef LOSCFG_VM_OVERLAP_CHECK
    if (VmmAspaceRegionsOverlapCheck(aspace) < 0) {
        (VOID)OsShellCmdDumpVm(0, NULL);
        ret = -ENOMEM;
    }
#endif

    (VOID)LOS_MuxRelease(&space->regionMux);
    return ret;
}
//输出内存线性区
VOID LOS_DumpMemRegion(VADDR_T vaddr)
{
    LosVmSpace *space = NULL;

    space = OsCurrProcessGet()->vmSpace;
    if (space == NULL) {
        return;
    }

    if (LOS_IsRangeInSpace(space, ROUNDDOWN(vaddr, MB), MB) == FALSE) {//是否在空间范围内
        return;
    }

    OsDumpPte(vaddr);//dump L1 L2
    OsDumpAspace(space);//dump 空间
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
