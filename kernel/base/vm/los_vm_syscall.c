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


#ifdef LOSCFG_KERNEL_VM

STATUS_T OsCheckMMapParams(VADDR_T *vaddr, unsigned long flags, size_t len, unsigned long pgoff)
{
    if ((len == 0) || (len > USER_ASPACE_SIZE)) {
        return -EINVAL;
    }
    if (len > OsCurrProcessGet()->vmSpace->mapSize) {
        return -ENOMEM;
    }

    if (((flags & MAP_FIXED) == 0) && ((flags & MAP_FIXED_NOREPLACE) == 0)) {
        *vaddr = ROUNDUP(*vaddr, PAGE_SIZE);
        if ((*vaddr != 0) && (!LOS_IsUserAddressRange(*vaddr, len))) {
            *vaddr = 0;
        }
    } else if ((!LOS_IsUserAddressRange(*vaddr, len)) || (!IS_ALIGNED(*vaddr, PAGE_SIZE))) {
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

STATUS_T OsNamedMmapingPermCheck(struct file *filep, unsigned long flags, unsigned prot)
{
    if (!((unsigned int)filep->f_oflags & O_RDWR) && (((unsigned int)filep->f_oflags & O_ACCMODE) ^ O_RDONLY)) {
        return -EACCES;
    }
    if (flags & MAP_SHARED) {
        if (((unsigned int)filep->f_oflags & O_APPEND) && (prot & PROT_WRITE)) {
            return -EACCES;
        }
        if ((prot & PROT_WRITE) && !((unsigned int)filep->f_oflags & O_RDWR)) {
            return -EACCES;
        }
    }

    return LOS_OK;
}
///匿名映射
STATUS_T OsAnonMMap(LosVmMapRegion *region)
{
    LOS_SetRegionTypeAnon(region);
    return LOS_OK;
}

/**
 * @brief 
    @verbatim
    mmap基础概念:
    一种内存映射文件的方法，即将一个文件或者其它对象映射到进程的地址空间，实现文件磁盘地址和进程虚拟地址空间中一段虚拟地址的一一对映关系.
    实现这样的映射关系后，进程就可以采用指针的方式读写操作这一段内存，而系统会自动回写脏页面到对应的文件磁盘上，
    即完成了对文件的操作而不必再调用read,write等系统调用函数。相反，内核空间对这段区域的修改也直接反映用户空间，
    从而可以实现不同进程间的文件共享。

    https://www.cnblogs.com/huxiao-tee/p/4660352.html
    http://abcdxyzk.github.io/blog/2015/09/11/kernel-mm-mmap/

    参数		描述		
    addr	指向欲映射的内存起始地址，通常设为 NULL，代表让系统自动选定地址，映射成功后返回该地址。
    length	代表将文件中多大的部分映射到内存。
    prot	用于设置内存段的访问权限，有如下权限：
            PROT_EXEC 映射区域可被执行
            PROT_READ 映射区域可被读取
            PROT_WRITE 映射区域可被写入
            PROT_NONE 映射区域不能存取
            
    flags	控制程序对内存段的改变所造成的影响，有如下属性：
            MAP_FIXED 如果参数start所指的地址无法成功建立映射时，则放弃映射，不对地址做修正。通常不鼓励用此旗标。
            MAP_SHARED 对映射区域的写入数据会复制回文件内，而且允许其他映射该文件的进程共享。
            MAP_PRIVATE 对映射区域的写入操作会产生一个映射文件的复制，即私人的“写入时复制”（copy on write）对此区域作的任何修改都不会写回原来的文件内容。
            MAP_ANONYMOUS建立匿名映射。此时会忽略参数fd，不涉及文件，而且映射区域无法和其他进程共享。
            MAP_DENYWRITE只允许对映射区域的写入操作，其他对文件直接写入的操作将会被拒绝。
            MAP_LOCKED 将映射区域锁定住，这表示该区域不会被置换（swap）。

    fd:		要映射到内存中的文件描述符。如果使用匿名内存映射时，即flags中设置了MAP_ANONYMOUS，fd设为-1。
            有些系统不支持匿名内存映射，则可以使用fopen打开/dev/zero文件，然后对该文件进行映射，可以同样达到匿名内存映射的效果。

    offset	文件映射的偏移量，通常设置为0，代表从文件最前方开始对应，offset必须是PAGE_SIZE的整数倍。
    成功返回：虚拟内存地址，这地址是页对齐。
    失败返回：(void *)-1。
    @endverbatim   
 * @param vaddr 
 * @param len 
 * @param prot 
 * @param flags 
 * @param fd 
 * @param pgoff 
 * @return VADDR_T 
 */
VADDR_T LOS_MMap(VADDR_T vaddr, size_t len, unsigned prot, unsigned long flags, int fd, unsigned long pgoff)
{
    STATUS_T status;
    VADDR_T resultVaddr;
    UINT32 regionFlags;
    LosVmMapRegion *newRegion = NULL;//应用的内存分配对应到内核就是分配一个线性区
    struct file *filep = NULL;// inode : file = 1:N ,一对多关系,一个inode可以被多个进程打开,返回不同的file但都指向同一个inode 
    LosVmSpace *vmSpace = OsCurrProcessGet()->vmSpace;

    len = ROUNDUP(len, PAGE_SIZE);
    STATUS_T checkRst = OsCheckMMapParams(&vaddr, flags, len, pgoff);
    if (checkRst != LOS_OK) {
        return checkRst;
    }
	
    if (LOS_IsNamedMapping(flags)) {//是否文件映射
        status = fs_getfilep(fd, &filep);//获取文件描述符和状态
        if (status < 0) {
            return -EBADF;
        }

        status = OsNamedMmapingPermCheck(filep, flags, prot);
        if (status < 0) {
            return status;
        }
    }

    (VOID)LOS_MuxAcquire(&vmSpace->regionMux);
    /* user mode calls mmap to release heap physical memory without releasing heap virtual space */
    status = OsUserHeapFree(vmSpace, vaddr, len);//用户模式释放堆物理内存而不释放堆虚拟空间
    if (status == LOS_OK) {//OsUserHeapFree 干两件事 1.解除映射关系 2.释放物理页
        resultVaddr = vaddr;
        goto MMAP_DONE;
    }
	//地址不在堆区
    regionFlags = OsCvtProtFlagsToRegionFlags(prot, flags);//将参数flag转换Region的flag
    newRegion = LOS_RegionAlloc(vmSpace, vaddr, len, regionFlags, pgoff);//分配一个线性区
    if (newRegion == NULL) {
        resultVaddr = (VADDR_T)-ENOMEM;//ENOMEM:内存溢出
        goto MMAP_DONE;
    }
    newRegion->regionFlags |= VM_MAP_REGION_FLAG_MMAP;
    resultVaddr = newRegion->range.base;//线性区基地址为分配的地址

    if (LOS_IsNamedMapping(flags)) {
        status = OsNamedMMap(filep, newRegion);//文件映射
    } else {
        status = OsAnonMMap(newRegion);//匿名映射
    }

    if (status != LOS_OK) {
        LOS_RbDelNode(&vmSpace->regionRbTree, &newRegion->rbNode);//从红黑树和双循环链表中删除
        LOS_RegionFree(vmSpace, newRegion);//释放
        resultVaddr = (VADDR_T)-ENOMEM;
        goto MMAP_DONE;
    }

MMAP_DONE:
    (VOID)LOS_MuxRelease(&vmSpace->regionMux);
    return resultVaddr;
}
///解除映射关系
STATUS_T LOS_UnMMap(VADDR_T addr, size_t size)
{
    if ((addr <= 0) || (size <= 0)) {
        return -EINVAL;
    }

    return OsUnMMap(OsCurrProcessGet()->vmSpace, addr, size);
}
STATIC INLINE BOOL OsProtMprotectPermCheck(unsigned long prot, LosVmMapRegion *region)
{
    UINT32 protFlags = 0;
    UINT32 permFlags = 0;
    UINT32 fileFlags = region->unTypeData.rf.f_oflags;
    permFlags |= ((fileFlags & O_ACCMODE) ^ O_RDONLY) ? 0 : VM_MAP_REGION_FLAG_PERM_READ;
    permFlags |= (fileFlags & O_WRONLY) ? VM_MAP_REGION_FLAG_PERM_WRITE : 0;
    permFlags |= (fileFlags & O_RDWR) ? (VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE) : 0;
    protFlags |= (prot & PROT_READ) ? VM_MAP_REGION_FLAG_PERM_READ : 0;
    protFlags |= (prot & PROT_WRITE) ? VM_MAP_REGION_FLAG_PERM_WRITE : 0;

    return ((protFlags & permFlags) == protFlags);
}
/// 收缩堆区
VOID *OsShrinkHeap(VOID *addr, LosVmSpace *space)
{
    VADDR_T newBrk, oldBrk;

    newBrk = LOS_Align((VADDR_T)(UINTPTR)addr, PAGE_SIZE);//新堆顶
    oldBrk = LOS_Align(space->heapNow, PAGE_SIZE);//旧堆顶
    if (LOS_UnMMap(newBrk, (oldBrk - newBrk)) < 0) {//解除相差区的映射
        return (void *)(UINTPTR)space->heapNow;//解除失败就持续现有的
    }
    space->heapNow = (VADDR_T)(UINTPTR)addr;//返回新堆顶
    return addr;
}

/**
 * @brief 
	@verbatim
    用户进程向内核申请空间，进一步说用于扩展用户堆栈空间，或者回收用户堆栈空间
    扩展当前进程的堆空间
    一个进程所有的线性区都在进程指定的线性地址范围内，
    线性区之间是不会有地址的重叠的，开始都是连续的，随着进程的运行出现了释放再分配的情况
    由此出现了断断续续的线性区，内核回收线性区时会检测是否和周边的线性区可合并成一个更大
    的线性区用于分配。
	@endverbatim
 * @param addr 
 * @return VOID* 
 */
VOID *LOS_DoBrk(VOID *addr)
{
    LosVmSpace *space = OsCurrProcessGet()->vmSpace;
    size_t size;
    VOID *ret = NULL;
    LosVmMapRegion *region = NULL;
    VOID *alignAddr = NULL;
    VOID *shrinkAddr = NULL;

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

    (VOID)LOS_MuxAcquire(&space->regionMux);
    if (addr < (VOID *)(UINTPTR)space->heapNow) {//如果地址小于堆区现地址
        shrinkAddr = OsShrinkHeap(addr, space);//收缩堆区
        (VOID)LOS_MuxRelease(&space->regionMux);
        return shrinkAddr;
    }

    if ((UINTPTR)alignAddr >= space->mapBase) {//参数地址 大于映射区地址
        VM_ERR("Process heap memory space is insufficient");//进程堆空间不足
        ret = (VOID *)-ENOMEM;
        goto REGION_ALLOC_FAILED;
    }

    if (space->heapBase == space->heapNow) {//往往是第一次调用本函数才会出现，因为初始化时 heapBase = heapNow
        region = LOS_RegionAlloc(space, space->heapBase, size,//分配一个可读/可写/可使用的线性区，只需分配一次
                                 VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE |//线性区的大小由range.size决定
                                 VM_MAP_REGION_FLAG_FIXED | VM_MAP_REGION_FLAG_PERM_USER, 0);
        if (region == NULL) {
            ret = (VOID *)-ENOMEM;
            VM_ERR("LOS_RegionAlloc failed");
            goto REGION_ALLOC_FAILED;
        }
        region->regionFlags |= VM_MAP_REGION_FLAG_HEAP;//贴上线性区类型为堆区的标签,注意一个线性区可以有多种标签
        space->heap = region;//指定线性区为堆区
    }

    space->heapNow = (VADDR_T)(UINTPTR)alignAddr;//更新堆区顶部位置
    space->heap->range.size = size;	//更新堆区大小,经此操作线性区变大或缩小了
    ret = (VOID *)(UINTPTR)space->heapNow;//返回堆顶

REGION_ALLOC_FAILED:
    (VOID)LOS_MuxRelease(&space->regionMux);
    return ret;
}
/// 继承老线性区的标签
STATIC UINT32 OsInheritOldRegionName(UINT32 oldRegionFlags)
{
    UINT32 vmFlags = 0;

    if (oldRegionFlags & VM_MAP_REGION_FLAG_HEAP) { //如果是从大堆区中申请的
        vmFlags |= VM_MAP_REGION_FLAG_HEAP; //线性区则贴上堆区标签
    } else if (oldRegionFlags & VM_MAP_REGION_FLAG_STACK) {
        vmFlags |= VM_MAP_REGION_FLAG_STACK;
    } else if (oldRegionFlags & VM_MAP_REGION_FLAG_TEXT) {
        vmFlags |= VM_MAP_REGION_FLAG_TEXT;
    } else if (oldRegionFlags & VM_MAP_REGION_FLAG_VDSO) {
        vmFlags |= VM_MAP_REGION_FLAG_VDSO;
    } else if (oldRegionFlags & VM_MAP_REGION_FLAG_MMAP) {
        vmFlags |= VM_MAP_REGION_FLAG_MMAP;
    } else if (oldRegionFlags & VM_MAP_REGION_FLAG_SHM) {
        vmFlags |= VM_MAP_REGION_FLAG_SHM;
    }

    return vmFlags;
}
///修改内存段的访问权限
INT32 LOS_DoMprotect(VADDR_T vaddr, size_t len, unsigned long prot)
{
    LosVmSpace *space = OsCurrProcessGet()->vmSpace;
    LosVmMapRegion *region = NULL;
    UINT32 vmFlags;
    UINT32 count;
    int ret;

    (VOID)LOS_MuxAcquire(&space->regionMux);
    region = LOS_RegionFind(space, vaddr);//通过虚拟地址找到线性区
    if (!IS_ALIGNED(vaddr, PAGE_SIZE) || (region == NULL) || (vaddr > vaddr + len)) {
        ret = -EINVAL;
        goto OUT_MPROTECT;
    }

    if ((prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC))) {
        ret = -EINVAL;
        goto OUT_MPROTECT;
    }
	//如果是堆区或VDSO区,说明区内容是不能修改的
    if ((region->regionFlags & VM_MAP_REGION_FLAG_VDSO) || (region->regionFlags & VM_MAP_REGION_FLAG_HEAP)) {
        ret = -EPERM;
        goto OUT_MPROTECT;
    }
	//如果是共享文件,说明内容也不能修改
    if (LOS_IsRegionTypeFile(region) && (region->regionFlags & VM_MAP_REGION_FLAG_SHARED)) {
        if (!OsProtMprotectPermCheck(prot, region)) {
            ret = -EACCES;
            goto OUT_MPROTECT;
        }
    }
    len = LOS_Align(len, PAGE_SIZE);
    /* can't operation cross region */
    if ((region->range.base + region->range.size) < (vaddr + len)) {
        ret = -EINVAL;
        goto OUT_MPROTECT;
    }

    /* if only move some part of region, we need to split first */
    if (region->range.size > len) {//如果只修改部分区域，我们需要先拆分区
        OsVmRegionAdjust(space, vaddr, len);//调整下线性区范围
    }

    vmFlags = OsCvtProtFlagsToRegionFlags(prot, 0);//转换FLAGS
    vmFlags |= (region->regionFlags & VM_MAP_REGION_FLAG_SHARED) ? VM_MAP_REGION_FLAG_SHARED : 0;
    vmFlags |= OsInheritOldRegionName(region->regionFlags);
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
///重新映射虚拟内存地址。
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
        status = LOS_ArchMmuMove(&space->archMmu, oldAddress, newAddr,
                                 ((newSize < regionOld->range.size) ? newSize : regionOld->range.size) >> PAGE_SHIFT,
                                 regionOld->regionFlags);
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
        status = LOS_ArchMmuMove(&space->archMmu, oldAddress, regionNew->range.base,
                                 regionOld->range.size >> PAGE_SHIFT, regionOld->regionFlags);
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
///输出内存线性区
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
#endif

