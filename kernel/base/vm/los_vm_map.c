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

#include "los_vm_map.h"
#include "los_vm_page.h"
#include "los_vm_phys.h"
#include "los_vm_dump.h"
#include "los_vm_lock.h"
#include "los_vm_zone.h"
#include "los_vm_common.h"
#include "los_vm_filemap.h"
#include "los_vm_shm_pri.h"
#include "los_arch_mmu.h"
#include "los_process_pri.h"
#include "fs/fs.h"
#include "los_task.h"
#include "los_memory_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define VM_MAP_WASTE_MEM_LEVEL          (PAGE_SIZE >> 2)
LosMux g_vmSpaceListMux;
LOS_DL_LIST_HEAD(g_vmSpaceList);
LosVmSpace g_kVmSpace;	//内核空间地址
LosVmSpace g_vMallocSpace;//虚拟分配空间地址
//通过虚拟地址获取所属空间地址
LosVmSpace *LOS_SpaceGet(VADDR_T vaddr)
{
    if (LOS_IsKernelAddress(vaddr)) {	//是否为内核空间
        return LOS_GetKVmSpace();		//获取内核空间
    } else if (LOS_IsUserAddress(vaddr)) {//是否为用户空间
        return OsCurrProcessGet()->vmSpace;//当前进程的虚拟空间
    } else if (LOS_IsVmallocAddress(vaddr)) {//是否为内核分配空间
        return LOS_GetVmallocSpace();//获取内核分配空间
    } else {
        return NULL;
    }
}
//内核虚拟空间只有g_kVmSpace一个
LosVmSpace *LOS_GetKVmSpace(VOID)
{
    return &g_kVmSpace;
}
//获取虚拟空间双循环链表 g_vmSpaceList中存放的是 g_kVmSpace, g_vMallocSpace,所有用户进程虚拟空间(每个用户进程独有一个)
LOS_DL_LIST *LOS_GetVmSpaceList(VOID)
{
    return &g_vmSpaceList;
}
//获取内核分配空间的全局变量
LosVmSpace *LOS_GetVmallocSpace(VOID)
{
    return &g_vMallocSpace;
}
//释放红黑树节点
ULONG_T OsRegionRbFreeFn(LosRbNode *pstNode)
{
    LOS_MemFree(m_aucSysMem0, pstNode);
    return LOS_OK;
}

VOID *OsRegionRbGetKeyFn(LosRbNode *pstNode)
{
    LosVmMapRegion *region = (LosVmMapRegion *)LOS_DL_LIST_ENTRY(pstNode, LosVmMapRegion, rbNode);
    return (VOID *)&region->range;
}

ULONG_T OsRegionRbCmpKeyFn(VOID *pNodeKeyA, VOID *pNodeKeyB)
{
    LosVmMapRange rangeA = *(LosVmMapRange *)pNodeKeyA;
    LosVmMapRange rangeB = *(LosVmMapRange *)pNodeKeyB;
    UINT32 startA = rangeA.base;
    UINT32 endA = rangeA.base + rangeA.size - 1;
    UINT32 startB = rangeB.base;
    UINT32 endB = rangeB.base + rangeB.size - 1;

    if (startA > endB) {
        return RB_BIGGER;
    } else if (startA >= startB) {
        if (endA <= endB) {
            return RB_EQUAL;
        } else {
            return RB_BIGGER;
        }
    } else if (startA <= startB) {
        if (endA >= endB) {
            return RB_EQUAL;
        } else {
            return RB_SMALLER;
        }
    } else if (endA < startB) {
        return RB_SMALLER;
    }
    return RB_EQUAL;
}

STATIC BOOL OsVmSpaceInitCommon(LosVmSpace *vmSpace, VADDR_T *virtTtb)
{
    LOS_RbInitTree(&vmSpace->regionRbTree, OsRegionRbCmpKeyFn, OsRegionRbFreeFn, OsRegionRbGetKeyFn);//初始化虚拟存储空间-以红黑树组织方式

    LOS_ListInit(&vmSpace->regions);//初始化虚拟存储区域-以双循环链表组织方式
    status_t retval = LOS_MuxInit(&vmSpace->regionMux, NULL);//初始化互斥量
    if (retval != LOS_OK) {
        VM_ERR("Create mutex for vm space failed, status: %d", retval);
        return FALSE;
    }

    (VOID)LOS_MuxAcquire(&g_vmSpaceListMux);
    LOS_ListAdd(&g_vmSpaceList, &vmSpace->node);//加入到虚拟空间双循环链表
    (VOID)LOS_MuxRelease(&g_vmSpaceListMux);

    return OsArchMmuInit(&vmSpace->archMmu, virtTtb);//对空间mmu初始化
}

VOID OsVmMapInit(VOID)
{
    status_t retval = LOS_MuxInit(&g_vmSpaceListMux, NULL);
    if (retval != LOS_OK) {
        VM_ERR("Create mutex for g_vmSpaceList failed, status: %d", retval);
    }
}
//内核虚拟空间初始化
BOOL OsKernVmSpaceInit(LosVmSpace *vmSpace, VADDR_T *virtTtb)//内核空间页表是编译时放在bbs段指定的,共用 L1表
{
    vmSpace->base = KERNEL_ASPACE_BASE;//内核虚拟空间基地址
    vmSpace->size = KERNEL_ASPACE_SIZE;//内核虚拟空间大小
    vmSpace->mapBase = KERNEL_VMM_BASE;//内核虚拟空间映射基地址
    vmSpace->mapSize = KERNEL_VMM_SIZE;//内核虚拟空间映射大小
#ifdef LOSCFG_DRIVERS_TZDRIVER
    vmSpace->codeStart = 0;
    vmSpace->codeEnd = 0;
#endif
    return OsVmSpaceInitCommon(vmSpace, virtTtb);//virtTtb 用于初始化 mmu
}
//内核虚拟分配空间初始化
BOOL OsVMallocSpaceInit(LosVmSpace *vmSpace, VADDR_T *virtTtb)//内核动态空间的页表是动态申请得来，共用 L1表
{
    vmSpace->base = VMALLOC_START;//内核虚拟分配空间基地址
    vmSpace->size = VMALLOC_SIZE;//内核虚拟分配空间大小
    vmSpace->mapBase = VMALLOC_START;//内核虚拟分配空间映射基地址
    vmSpace->mapSize = VMALLOC_SIZE;//内核虚拟分配空间映射区大小
#ifdef LOSCFG_DRIVERS_TZDRIVER
    vmSpace->codeStart = 0;
    vmSpace->codeEnd = 0;
#endif
    return OsVmSpaceInitCommon(vmSpace, virtTtb);
}
//用户虚拟空间初始化
BOOL OsUserVmSpaceInit(LosVmSpace *vmSpace, VADDR_T *virtTtb)//用户空间的页表是动态申请得来,每个进程有属于自己的L1,L2表
{
    vmSpace->base = USER_ASPACE_BASE;//用户空间基地址
    vmSpace->size = USER_ASPACE_SIZE;//用户空间大小
    vmSpace->mapBase = USER_MAP_BASE;//用户空间映射基地址
    vmSpace->mapSize = USER_MAP_SIZE;//用户空间映射大小
    vmSpace->heapBase = USER_HEAP_BASE;//用户堆区开始地址
    vmSpace->heapNow = USER_HEAP_BASE;//用户堆区当前地址默认 == 开始地址
    vmSpace->heap = NULL;
#ifdef LOSCFG_DRIVERS_TZDRIVER
    vmSpace->codeStart = 0;
    vmSpace->codeEnd = 0;
#endif
    return OsVmSpaceInitCommon(vmSpace, virtTtb);
}
//鸿蒙内核空间有两个(内核进程空间和内核动态分配空间),共用一张L1页表
VOID OsKSpaceInit(VOID)
{
    OsVmMapInit();// 初始化互斥量
    OsKernVmSpaceInit(&g_kVmSpace, OsGFirstTableGet());// 初始化内核虚拟空间，OsGFirstTableGet 为L1表基地址
    OsVMallocSpaceInit(&g_vMallocSpace, OsGFirstTableGet());// 初始化动态分配区虚拟空间，OsGFirstTableGet 为L1表基地址
}//g_kVmSpace g_vMallocSpace 共用一个L1页表

STATIC BOOL OsVmSpaceParamCheck(LosVmSpace *vmSpace)//这么简单也要写个函数?
{
    if (vmSpace == NULL) {
        return FALSE;
    }
    return TRUE;
}
//克隆共享线性区，输入老区，输出新区
LosVmMapRegion *OsShareRegionClone(LosVmMapRegion *oldRegion)
{
    /* no need to create vm object */
    LosVmMapRegion *newRegion = LOS_MemAlloc(m_aucSysMem0, sizeof(LosVmMapRegion));
    if (newRegion == NULL) {
        VM_ERR("malloc new region struct failed.");
        return NULL;
    }

    /* todo: */
    *newRegion = *oldRegion;
    return newRegion;
}
//克隆私有线性区，输入老区，输出新区
LosVmMapRegion *OsPrivateRegionClone(LosVmMapRegion *oldRegion)
{
    /* need to create vm object */
    LosVmMapRegion *newRegion = LOS_MemAlloc(m_aucSysMem0, sizeof(LosVmMapRegion));
    if (newRegion == NULL) {
        VM_ERR("malloc new region struct failed.");
        return NULL;
    }

    /* todo: */
    *newRegion = *oldRegion;
    return newRegion;
}
//虚拟内存空间克隆，被用于fork进程
STATUS_T LOS_VmSpaceClone(LosVmSpace *oldVmSpace, LosVmSpace *newVmSpace)
{
    LosVmMapRegion *oldRegion = NULL;
    LosVmMapRegion *newRegion = NULL;
    LosRbNode *pstRbNode = NULL;
    LosRbNode *pstRbNodeNext = NULL;
    STATUS_T ret = LOS_OK;
    UINT32 numPages;
    PADDR_T paddr;
    VADDR_T vaddr;
    UINT32 intSave;
    LosVmPage *page = NULL;
    UINT32 flags;
    UINT32 i;

    if ((OsVmSpaceParamCheck(oldVmSpace) == FALSE) || (OsVmSpaceParamCheck(newVmSpace) == FALSE)) {
        return LOS_ERRNO_VM_INVALID_ARGS;
    }

    if ((OsIsVmRegionEmpty(oldVmSpace) == TRUE) || (oldVmSpace == &g_kVmSpace)) {
        return LOS_ERRNO_VM_INVALID_ARGS;
    }

    /* search the region list */
    newVmSpace->mapBase = oldVmSpace->mapBase;
    newVmSpace->heapBase = oldVmSpace->heapBase;
    newVmSpace->heapNow = oldVmSpace->heapNow;
    (VOID)LOS_MuxAcquire(&oldVmSpace->regionMux);
    RB_SCAN_SAFE(&oldVmSpace->regionRbTree, pstRbNode, pstRbNodeNext)
        oldRegion = (LosVmMapRegion *)pstRbNode;
        newRegion = OsVmRegionDup(newVmSpace, oldRegion, oldRegion->range.base, oldRegion->range.size);
        if (newRegion == NULL) {
            VM_ERR("dup new region failed");
            ret = LOS_ERRNO_VM_NO_MEMORY;
            goto ERR_CLONE_ASPACE;
        }

        if (oldRegion->regionFlags & VM_MAP_REGION_FLAG_SHM) {
            OsShmFork(newVmSpace, oldRegion, newRegion);
            continue;
        }

        if (oldRegion == oldVmSpace->heap) {
            newVmSpace->heap = newRegion;
        }

        numPages = newRegion->range.size >> PAGE_SHIFT;
        for (i = 0; i < numPages; i++) {
            vaddr = newRegion->range.base + (i << PAGE_SHIFT);
            if (LOS_ArchMmuQuery(&oldVmSpace->archMmu, vaddr, &paddr, &flags) != LOS_OK) {
                continue;
            }

            page = LOS_VmPageGet(paddr);
            if (page != NULL) {
                LOS_AtomicInc(&page->refCounts);//refCounts 自增
            }
            if (flags & VM_MAP_REGION_FLAG_PERM_WRITE) {
                LOS_ArchMmuUnmap(&oldVmSpace->archMmu, vaddr, 1);
                LOS_ArchMmuMap(&oldVmSpace->archMmu, vaddr, paddr, 1, flags & ~VM_MAP_REGION_FLAG_PERM_WRITE);
            }
            LOS_ArchMmuMap(&newVmSpace->archMmu, vaddr, paddr, 1, flags & ~VM_MAP_REGION_FLAG_PERM_WRITE);

#ifdef LOSCFG_FS_VFS
            if (LOS_IsRegionFileValid(oldRegion)) {
                LosFilePage *fpage = NULL;
                LOS_SpinLockSave(&oldRegion->unTypeData.rf.file->f_mapping->list_lock, &intSave);
                fpage = OsFindGetEntry(oldRegion->unTypeData.rf.file->f_mapping, newRegion->pgOff + i);
                if ((fpage != NULL) && (fpage->vmPage == page)) { /* cow page no need map */
                    OsAddMapInfo(fpage, &newVmSpace->archMmu, vaddr);
                }
                LOS_SpinUnlockRestore(&oldRegion->unTypeData.rf.file->f_mapping->list_lock, intSave);
            }
#endif
        }
    RB_SCAN_SAFE_END(&oldVmSpace->regionRbTree, pstRbNode, pstRbNodeNext)
    goto OUT_CLONE_ASPACE;
ERR_CLONE_ASPACE:
    if (LOS_VmSpaceFree(newVmSpace) != LOS_OK) {
        VM_ERR("LOS_VmSpaceFree failed");
    }
OUT_CLONE_ASPACE:
    (VOID)LOS_MuxRelease(&oldVmSpace->regionMux);
    return ret;
}
//通过虚拟(线性)地址查找所属线性区,红黑树
LosVmMapRegion *OsFindRegion(LosRbTree *regionRbTree, VADDR_T vaddr, size_t len)
{
    LosVmMapRegion *regionRst = NULL;
    LosRbNode *pstRbNode = NULL;
    LosVmMapRange rangeKey;
    rangeKey.base = vaddr;
    rangeKey.size = len;

    if (LOS_RbGetNode(regionRbTree, (VOID *)&rangeKey, &pstRbNode)) {
        regionRst = (LosVmMapRegion *)LOS_DL_LIST_ENTRY(pstRbNode, LosVmMapRegion, rbNode);
    }
    return regionRst;
}

LosVmMapRegion *LOS_RegionFind(LosVmSpace *vmSpace, VADDR_T addr)
{
    return OsFindRegion(&vmSpace->regionRbTree, addr, 1);
}

LosVmMapRegion *LOS_RegionRangeFind(LosVmSpace *vmSpace, VADDR_T addr, size_t len)
{
    return OsFindRegion(&vmSpace->regionRbTree, addr, len);
}

VADDR_T OsAllocRange(LosVmSpace *vmSpace, size_t len)
{
    LosVmMapRegion *curRegion = NULL;
    LosRbNode *pstRbNode = NULL;
    LosRbNode *pstRbNodeTmp = NULL;
    LosRbTree *regionRbTree = &vmSpace->regionRbTree;
    VADDR_T curEnd = vmSpace->mapBase;
    VADDR_T nextStart;

    curRegion = LOS_RegionFind(vmSpace, vmSpace->mapBase);
    if (curRegion != NULL) {
        pstRbNode = &curRegion->rbNode;
        curEnd = curRegion->range.base + curRegion->range.size;
        RB_MID_SCAN(regionRbTree, pstRbNode)
            curRegion = (LosVmMapRegion *)pstRbNode;
            nextStart = curRegion->range.base;
            if (nextStart < curEnd) {
                continue;
            }
            if ((nextStart - curEnd) >= len) {
                return curEnd;
            } else {
                curEnd = curRegion->range.base + curRegion->range.size;
            }
        RB_MID_SCAN_END(regionRbTree, pstRbNode)
    } else {//红黑树扫描排序，从小到大
        /* rbtree scan is sorted, from small to big */
        RB_SCAN_SAFE(regionRbTree, pstRbNode, pstRbNodeTmp)
            curRegion = (LosVmMapRegion *)pstRbNode;
            nextStart = curRegion->range.base;
            if (nextStart < curEnd) {
                continue;
            }
            if ((nextStart - curEnd) >= len) {
                return curEnd;
            } else {
                curEnd = curRegion->range.base + curRegion->range.size;
            }
        RB_SCAN_SAFE_END(regionRbTree, pstRbNode, pstRbNodeTmp)
    }

    nextStart = vmSpace->mapBase + vmSpace->mapSize;
    if ((nextStart - curEnd) >= len) {
        return curEnd;
    }

    return 0;
}

VADDR_T OsAllocSpecificRange(LosVmSpace *vmSpace, VADDR_T vaddr, size_t len)
{
    STATUS_T status;

    if (LOS_IsRangeInSpace(vmSpace, vaddr, len) == FALSE) {
        return 0;
    }

    if ((LOS_RegionFind(vmSpace, vaddr) != NULL) ||
        (LOS_RegionFind(vmSpace, vaddr + len - 1) != NULL) ||
        (LOS_RegionRangeFind(vmSpace, vaddr, len - 1) != NULL)) {
        status = LOS_UnMMap(vaddr, len);
        if (status != LOS_OK) {
            VM_ERR("unmap specific range va: %#x, len: %#x failed, status: %d", vaddr, len, status);
            return 0;
        }
    }

    return vaddr;
}

BOOL LOS_IsRegionFileValid(LosVmMapRegion *region)
{
    struct file *filep = NULL;
    if ((region != NULL) && (LOS_IsRegionTypeFile(region)) &&
        (region->unTypeData.rf.file != NULL)) {
        filep = region->unTypeData.rf.file;
        if (region->unTypeData.rf.fileMagic == filep->f_magicnum) {
            return TRUE;
        }
    }
    return FALSE;
}
//向红黑树中插入线性区
BOOL OsInsertRegion(LosRbTree *regionRbTree, LosVmMapRegion *region)
{
    if (LOS_RbAddNode(regionRbTree, (LosRbNode *)region) == FALSE) {
        VM_ERR("insert region failed, base: %#x, size: %#x", region->range.base, region->range.size);
        OsDumpAspace(region->space);
        return FALSE;
    }
    return TRUE;
}
//创建一个线性区
LosVmMapRegion *OsCreateRegion(VADDR_T vaddr, size_t len, UINT32 regionFlags, unsigned long offset)
{
    LosVmMapRegion *region = LOS_MemAlloc(m_aucSysMem0, sizeof(LosVmMapRegion));//分配结构体
    if (region == NULL) {
        VM_ERR("memory allocate for LosVmMapRegion failed");
        return region;
    }

    region->range.base = vaddr;//虚拟地址作为线性区的基地址
    region->range.size = len;	//区大小
    region->pgOff = offset;		//区域页面到文件的偏移
    region->regionFlags = regionFlags;//标识,可读可写可执行这些
    region->regionType = VM_MAP_REGION_TYPE_NONE;//未映射
    region->forkFlags = 0;//
    region->shmid = -1;
    return region;
}
//通过虚拟地址查询物理地址
PADDR_T LOS_PaddrQuery(VOID *vaddr)
{
    PADDR_T paddr = 0;
    STATUS_T status;
    LosVmSpace *space = NULL;
    LosArchMmu *archMmu = NULL;
    //先取出对应空间的mmu
    if (LOS_IsKernelAddress((VADDR_T)(UINTPTR)vaddr)) {//是否是内核空间地址
        archMmu = &g_kVmSpace.archMmu;
    } else if (LOS_IsUserAddress((VADDR_T)(UINTPTR)vaddr)) {//是否为用户空间地址
        space = OsCurrProcessGet()->vmSpace;
        archMmu = &space->archMmu;
    } else if (LOS_IsVmallocAddress((VADDR_T)(UINTPTR)vaddr)) {//是否为分配空间地址,堆区地址
        archMmu = &g_vMallocSpace.archMmu;
    } else {
        VM_ERR("vaddr is beyond range");
        return 0;
    }

    status = LOS_ArchMmuQuery(archMmu, (VADDR_T)(UINTPTR)vaddr, &paddr, 0);//查询物理地址
    if (status == LOS_OK) {
        return paddr;
    } else {
        return 0;
    }
}
//这里不是真的分配物理内存，而是逻辑上画一个连续的区域，vaddr可以是0，
//再映射到物理地址
LosVmMapRegion *LOS_RegionAlloc(LosVmSpace *vmSpace, VADDR_T vaddr, size_t len, UINT32 regionFlags, VM_OFFSET_T pgoff)
{
    VADDR_T rstVaddr;
    LosVmMapRegion *newRegion = NULL;
    BOOL isInsertSucceed = FALSE;
    /**
     * If addr is NULL, then the kernel chooses the address at which to create the mapping;
     * this is the most portable method of creating a new mapping.  If addr is not NULL,
     * then the kernel takes it as where to place the mapping;
     */
    (VOID)LOS_MuxAcquire(&vmSpace->regionMux);//获得互斥锁
    if (vaddr == 0) {//如果地址是0，则由内核选择创建映射的虚拟地址，    这是创建新映射的最便捷的方法。
        rstVaddr = OsAllocRange(vmSpace, len);
    } else {
        /* if it is already mmapped here, we unmmap it */
        rstVaddr = OsAllocSpecificRange(vmSpace, vaddr, len);//如果这个地址已经有映射记录，则要解除
        if (rstVaddr == 0) {
            VM_ERR("alloc specific range va: %#x, len: %#x failed", vaddr, len);
            goto OUT;
        }
    }
    if (rstVaddr == 0) {//没有可供映射的虚拟地址
        goto OUT;
    }

    newRegion = OsCreateRegion(rstVaddr, len, regionFlags, pgoff);//从内存池中创建一个线性区
    if (newRegion == NULL) {
        goto OUT;
    }
    newRegion->space = vmSpace;
    isInsertSucceed = OsInsertRegion(&vmSpace->regionRbTree, newRegion);//插入红黑树和双循环链表中管理
    if (isInsertSucceed == FALSE) {//插入失败
        (VOID)LOS_MemFree(m_aucSysMem0, newRegion);//从内存池中释放
        newRegion = NULL;
    }

OUT:
    (VOID)LOS_MuxRelease(&vmSpace->regionMux);//释放互斥锁
    return newRegion;
}

STATIC VOID OsAnonPagesRemove(LosArchMmu *archMmu, VADDR_T vaddr, UINT32 count)
{
    status_t status;
    paddr_t paddr;
    LosVmPage *page = NULL;

    if ((archMmu == NULL) || (vaddr == 0) || (count == 0)) {
        VM_ERR("OsAnonPagesRemove invalid args, archMmu %p, vaddr %p, count %d", archMmu, vaddr, count);
        return;
    }

    while (count > 0) {
        count--;
        status = LOS_ArchMmuQuery(archMmu, vaddr, &paddr, NULL);
        if (status != LOS_OK) {
            vaddr += PAGE_SIZE;
            continue;
        }

        LOS_ArchMmuUnmap(archMmu, vaddr, 1);

        page = LOS_VmPageGet(paddr);
        if (page != NULL) {
            if (!OsIsPageShared(page)) {
                LOS_PhysPageFree(page);
            }
        }
        vaddr += PAGE_SIZE;
    }
}

STATIC VOID OsDevPagesRemove(LosArchMmu *archMmu, VADDR_T vaddr, UINT32 count)
{
    status_t status;

    if ((archMmu == NULL) || (vaddr == 0) || (count == 0)) {
        VM_ERR("OsAnonPagesRemove invalid args, archMmu %p, vaddr %p, count %d", archMmu, vaddr, count);
        return;
    }

    status = LOS_ArchMmuQuery(archMmu, vaddr, NULL, NULL);
    if (status != LOS_OK) {
        return;
    }

    /* in order to unmap section */
    LOS_ArchMmuUnmap(archMmu, vaddr, count);
}

#ifdef LOSCFG_FS_VFS
STATIC VOID OsFilePagesRemove(LosVmSpace *space, LosVmMapRegion *region)
{
    VM_OFFSET_T offset;
    size_t size;

    if ((space == NULL) || (region == NULL) || (region->unTypeData.rf.vmFOps == NULL)) {
        return;
    }

    offset = region->pgOff;
    size = region->range.size;
    while (size >= PAGE_SIZE) {
        region->unTypeData.rf.vmFOps->remove(region, &space->archMmu, offset);
        offset++;
        size -= PAGE_SIZE;
    }
}
#endif
//释放线性区
STATUS_T LOS_RegionFree(LosVmSpace *space, LosVmMapRegion *region)
{
    if ((space == NULL) || (region == NULL)) {
        VM_ERR("args error, aspace %p, region %p", space, region);
        return LOS_ERRNO_VM_INVALID_ARGS;
    }

    (VOID)LOS_MuxAcquire(&space->regionMux);

#ifdef LOSCFG_FS_VFS
    if (LOS_IsRegionFileValid(region)) {
        OsFilePagesRemove(space, region);
    } else
#endif
    if (OsIsShmRegion(region)) {
        OsShmRegionFree(space, region);
    } else if (LOS_IsRegionTypeDev(region)) {
        OsDevPagesRemove(&space->archMmu, region->range.base, region->range.size >> PAGE_SHIFT);
    } else {
        OsAnonPagesRemove(&space->archMmu, region->range.base, region->range.size >> PAGE_SHIFT);
    }

    /* remove it from space */
    LOS_RbDelNode(&space->regionRbTree, &region->rbNode);
    /* free it */
    LOS_MemFree(m_aucSysMem0, region);
    (VOID)LOS_MuxRelease(&space->regionMux);
    return LOS_OK;
}

LosVmMapRegion *OsVmRegionDup(LosVmSpace *space, LosVmMapRegion *oldRegion, VADDR_T vaddr, size_t size)
{
    LosVmMapRegion *newRegion = NULL;

    (VOID)LOS_MuxAcquire(&space->regionMux);
    newRegion = LOS_RegionAlloc(space, vaddr, size, oldRegion->regionFlags, oldRegion->pgOff);
    if (newRegion == NULL) {
        VM_ERR("LOS_RegionAlloc failed");
        goto REGIONDUPOUT;
    }
    newRegion->regionType = oldRegion->regionType;
    if (OsIsShmRegion(oldRegion)) {
        newRegion->shmid = oldRegion->shmid;
    }

#ifdef LOSCFG_FS_VFS
    if (LOS_IsRegionTypeFile(oldRegion)) {
        newRegion->unTypeData.rf.vmFOps = oldRegion->unTypeData.rf.vmFOps;
        newRegion->unTypeData.rf.file = oldRegion->unTypeData.rf.file;
        newRegion->unTypeData.rf.fileMagic = oldRegion->unTypeData.rf.fileMagic;
    }
#endif

REGIONDUPOUT:
    (VOID)LOS_MuxRelease(&space->regionMux);
    return newRegion;
}

STATIC LosVmMapRegion *OsVmRegionSplit(LosVmMapRegion *oldRegion, VADDR_T newRegionStart)
{
    LosVmMapRegion *newRegion = NULL;
    LosVmSpace *space = oldRegion->space;
    size_t size = LOS_RegionSize(newRegionStart, LOS_RegionEndAddr(oldRegion));

    LOS_RbDelNode(&space->regionRbTree, &oldRegion->rbNode);
    oldRegion->range.size = LOS_RegionSize(oldRegion->range.base, newRegionStart - 1);
    if (oldRegion->range.size != 0) {
        LOS_RbAddNode(&space->regionRbTree, &oldRegion->rbNode);
    }

    newRegion = OsVmRegionDup(oldRegion->space, oldRegion, newRegionStart, size);
    if (newRegion == NULL) {
        VM_ERR("OsVmRegionDup fail");
        return NULL;
    }
#ifdef LOSCFG_FS_VFS
    newRegion->pgOff = oldRegion->pgOff + ((newRegionStart - oldRegion->range.base) >> PAGE_SHIFT);
#endif
    return newRegion;
}

STATUS_T OsVmRegionAdjust(LosVmSpace *space, VADDR_T newRegionStart, size_t size)
{
    LosVmMapRegion *region = NULL;
    VADDR_T nextRegionBase = newRegionStart + size;
    LosVmMapRegion *newRegion = NULL;

    region = LOS_RegionFind(space, newRegionStart);
    if ((region == NULL) || (region->range.base >= nextRegionBase)) {
        return LOS_ERRNO_VM_NOT_FOUND;
    }

    if ((region->range.base == newRegionStart) && (region->range.size == size)) {
        return LOS_OK;
    }

    if (newRegionStart > region->range.base) {
        newRegion = OsVmRegionSplit(region, newRegionStart);
        if (newRegion == NULL) {
            VM_ERR("region split fail");
            return LOS_ERRNO_VM_NO_MEMORY;
        }
    }

    region = LOS_RegionFind(space, nextRegionBase);
    if ((region != NULL) && (nextRegionBase < LOS_RegionEndAddr(region))) {
        newRegion = OsVmRegionSplit(region, nextRegionBase);
        if (newRegion == NULL) {
            VM_ERR("region split fail");
            return LOS_ERRNO_VM_NO_MEMORY;
        }
    }
    return LOS_OK;
}

STATUS_T OsRegionsRemove(LosVmSpace *space, VADDR_T regionBase, size_t size)
{
    STATUS_T status;
    VADDR_T regionEnd = regionBase + size - 1;
    LosVmMapRegion *regionTemp = NULL;
    LosRbNode *pstRbNodeTemp = NULL;
    LosRbNode *pstRbNodeNext = NULL;

    (VOID)LOS_MuxAcquire(&space->regionMux);

    status = OsVmRegionAdjust(space, regionBase, size);
    if (status != LOS_OK) {
        goto ERR_REGION_SPLIT;
    }

    RB_SCAN_SAFE(&space->regionRbTree, pstRbNodeTemp, pstRbNodeNext)
        regionTemp = (LosVmMapRegion *)pstRbNodeTemp;
        if (regionBase <= regionTemp->range.base && regionEnd >= LOS_RegionEndAddr(regionTemp)) {
            status = LOS_RegionFree(space, regionTemp);
            if (status != LOS_OK) {
                VM_ERR("fail to free region, status=%d", status);
                goto ERR_REGION_SPLIT;
            }
        }

        if (regionTemp->range.base > regionEnd) {
            break;
        }
    RB_SCAN_SAFE_END(&space->regionRbTree, pstRbNodeTemp, pstRbNodeNext)

ERR_REGION_SPLIT:
    (VOID)LOS_MuxRelease(&space->regionMux);
    return status;
}
//用户进程释放堆
INT32 OsUserHeapFree(LosVmSpace *vmSpace, VADDR_T addr, size_t len)
{
    LosVmMapRegion *vmRegion = NULL;
    LosVmPage *vmPage = NULL;
    PADDR_T paddr = 0;
    VADDR_T vaddr;
    STATUS_T ret;

    if (vmSpace == LOS_GetKVmSpace() || vmSpace->heap == NULL) {//非内核空间
        return -1;
    }

    vmRegion = LOS_RegionFind(vmSpace, addr);//找到线性区
    if (vmRegion == NULL) {
        return -1;
    }

    if (vmRegion == vmSpace->heap) {//当前指向线性区指针和找到的线性区一致
        vaddr = addr;
        while (len > 0) {//参数0 代表不获取 flags 信息
            if (LOS_ArchMmuQuery(&vmSpace->archMmu, vaddr, &paddr, 0) == LOS_OK) {//通过虚拟地址查到物理地址
                ret = LOS_ArchMmuUnmap(&vmSpace->archMmu, vaddr, 1);//解除映射关系
                if (ret <= 0) {
                    VM_ERR("unmap failed, ret = %d", ret);
                }
                vmPage = LOS_VmPageGet(paddr);//获取页信息
                LOS_PhysPageFree(vmPage);//释放页
            }
            vaddr += PAGE_SIZE;
            len -= PAGE_SIZE;
        }
        return 0;
    }

    return -1;
}
//线性区是否支持扩展
STATUS_T OsIsRegionCanExpand(LosVmSpace *space, LosVmMapRegion *region, size_t size)
{
    LosVmMapRegion *nextRegion = NULL;

    if ((space == NULL) || (region == NULL)) {
        return LOS_NOK;
    }

    /* if next node is head, then we can expand */
    if (OsIsVmRegionEmpty(space) == TRUE) {
        return LOS_OK;
    }

    nextRegion = (LosVmMapRegion *)LOS_RbSuccessorNode(&space->regionRbTree, &region->rbNode);
    /* if the gap is larger than size, then we can expand */
    if ((nextRegion != NULL) && ((nextRegion->range.base - region->range.base - region->range.size) > size)) {
        return LOS_OK;
    }

    return LOS_NOK;
}
//取消映射
STATUS_T OsUnMMap(LosVmSpace *space, VADDR_T addr, size_t size)
{
    size = LOS_Align(size, PAGE_SIZE);
    addr = LOS_Align(addr, PAGE_SIZE);
    (VOID)LOS_MuxAcquire(&space->regionMux);
    STATUS_T status = OsRegionsRemove(space, addr, size);//删除线性区
    if (status != LOS_OK) {
        status = -EINVAL;
        VM_ERR("region_split failed");
        goto ERR_REGION_SPLIT;
    }

ERR_REGION_SPLIT:
    (VOID)LOS_MuxRelease(&space->regionMux);
    return status;
}
//释放虚拟空间
STATUS_T LOS_VmSpaceFree(LosVmSpace *space)
{
    LosVmMapRegion *region = NULL;
    LosRbNode *pstRbNode = NULL;
    LosRbNode *pstRbNodeNext = NULL;
    STATUS_T ret;

    if (space == NULL) {
        return LOS_ERRNO_VM_INVALID_ARGS;
    }

    if (space == &g_kVmSpace) {//不能释放内核虚拟空间,内核空间常驻内存
        VM_ERR("try to free kernel aspace, not allowed");
        return LOS_OK;
    }

    /* pop it out of the global aspace list */
    (VOID)LOS_MuxAcquire(&space->regionMux);
    LOS_ListDelete(&space->node);//从g_vmSpaceList链表里删除，g_vmSpaceList记录了所有空间节点。
    /* free all of the regions */
    RB_SCAN_SAFE(&space->regionRbTree, pstRbNode, pstRbNodeNext)//释放空间中所有线性区 ，RB_SCAN_SAFE是个for循环宏
        region = (LosVmMapRegion *)pstRbNode;
        if (region->range.size == 0) {
            VM_ERR("space free, region: %#x flags: %#x, base:%#x, size: %#x",
                   region, region->regionFlags, region->range.base, region->range.size);
        }
        ret = LOS_RegionFree(space, region);//
        if (ret != LOS_OK) {
            VM_ERR("free region error, space %p, region %p", space, region);
        }
    RB_SCAN_SAFE_END(&space->regionRbTree, pstRbNode, pstRbNodeNext)

    /* make sure the current thread does not map the aspace */
    LosProcessCB *currentProcess = OsCurrProcessGet();
    if (currentProcess->vmSpace == space) {
        LOS_TaskLock();
        currentProcess->vmSpace = NULL;
        LOS_ArchMmuContextSwitch(&space->archMmu);
        LOS_TaskUnlock();
    }

    /* destroy the arch portion of the space */
    LOS_ArchMmuDestroy(&space->archMmu);

    (VOID)LOS_MuxRelease(&space->regionMux);
    (VOID)LOS_MuxDestroy(&space->regionMux);

    /* free the aspace */
    LOS_MemFree(m_aucSysMem0, space);
    return LOS_OK;
}
//虚拟地址和size是否在空间
BOOL LOS_IsRangeInSpace(const LosVmSpace *space, VADDR_T vaddr, size_t size)
{
    /* is the starting address within the address space */
    if (vaddr < space->base || vaddr > space->base + space->size - 1) {
        return FALSE;
    }
    if (size == 0) {
        return TRUE;
    }
    /* see if the size is enough to wrap the integer */
    if (vaddr + size - 1 < vaddr) {
        return FALSE;
    }
    /* see if the end address is within the address space's */
    if (vaddr + size - 1 > space->base + space->size - 1) {
        return FALSE;
    }
    return TRUE;
}

STATUS_T LOS_VmSpaceReserve(LosVmSpace *space, size_t size, VADDR_T vaddr)
{
    uint regionFlags;

    if ((space == NULL) || (size == 0) || (!IS_PAGE_ALIGNED(vaddr) || !IS_PAGE_ALIGNED(size))) {
        return LOS_ERRNO_VM_INVALID_ARGS;
    }

    if (!LOS_IsRangeInSpace(space, vaddr, size)) {
        return LOS_ERRNO_VM_OUT_OF_RANGE;
    }

    /* lookup how it's already mapped */
    LOS_ArchMmuQuery(&space->archMmu, vaddr, NULL, &regionFlags);

    /* build a new region structure */
    LosVmMapRegion *region = LOS_RegionAlloc(space, vaddr, size, regionFlags, 0);

    return region ? LOS_OK : LOS_ERRNO_VM_NO_MEMORY;
}
//实现从虚拟地址到物理地址的映射
STATUS_T LOS_VaddrToPaddrMmap(LosVmSpace *space, VADDR_T vaddr, PADDR_T paddr, size_t len, UINT32 flags)
{
    STATUS_T ret;
    LosVmMapRegion *region = NULL;
    LosVmPage *vmPage = NULL;

    if ((vaddr != ROUNDUP(vaddr, PAGE_SIZE)) ||
        (paddr != ROUNDUP(paddr, PAGE_SIZE)) ||
        (len != ROUNDUP(len, PAGE_SIZE))) {
        VM_ERR("vaddr :0x%x  paddr:0x%x len: 0x%x not page size align", vaddr, paddr, len);
        return LOS_ERRNO_VM_NOT_VALID;
    }

    if (space == NULL) {
        space = OsCurrProcessGet()->vmSpace;//获取当前进程的空间
    }

    region = LOS_RegionFind(space, vaddr);//通过虚拟地址查找线性区
    if (region != NULL) {//已经被映射过了,失败返回
        VM_ERR("vaddr : 0x%x already used!", vaddr);
        return LOS_ERRNO_VM_BUSY;
    }

    region = LOS_RegionAlloc(space, vaddr, len, flags, 0);//通过虚拟地址 创建一个region
    if (region == NULL) {
        VM_ERR("failed");
        return LOS_ERRNO_VM_NO_MEMORY;//内存不够
    }

    while (len > 0) {
        vmPage = LOS_VmPageGet(paddr);//通过物理地址查找页
        LOS_AtomicInc(&vmPage->refCounts);//ref自增

        ret = LOS_ArchMmuMap(&space->archMmu, vaddr, paddr, 1, region->regionFlags);//mmu map
        if (ret <= 0) {
            VM_ERR("LOS_ArchMmuMap failed: %d", ret);
            LOS_RegionFree(space, region);
            return ret;
        }

        paddr += PAGE_SIZE;
        vaddr += PAGE_SIZE;
        len -= PAGE_SIZE;
    }
    return LOS_OK;
}

STATUS_T LOS_UserSpaceVmAlloc(LosVmSpace *space, size_t size, VOID **ptr, UINT8 align_log2, UINT32 regionFlags)
{
    STATUS_T err = LOS_OK;
    VADDR_T vaddr = 0;
    size_t sizeCount;
    size_t count;
    LosVmPage *vmPage = NULL;
    VADDR_T vaddrTemp;
    PADDR_T paddrTemp;
    LosVmMapRegion *region = NULL;

    size = ROUNDUP(size, PAGE_SIZE);
    if (size == 0) {
        return LOS_ERRNO_VM_INVALID_ARGS;
    }
    sizeCount = (size >> PAGE_SHIFT);

    /* if they're asking for a specific spot, copy the address */
    if (ptr != NULL) {
        vaddr = (VADDR_T)(UINTPTR)*ptr;
    }
    /* allocate physical memory up front, in case it cant be satisfied */
    /* allocate a random pile of pages */
    LOS_DL_LIST_HEAD(pageList);

    (VOID)LOS_MuxAcquire(&space->regionMux);
    count = LOS_PhysPagesAlloc(sizeCount, &pageList);//由伙伴算法分配物理页面
    if (count < sizeCount) {
        VM_ERR("failed to allocate enough pages (ask %zu, got %zu)", sizeCount, count);
        err = LOS_ERRNO_VM_NO_MEMORY;
        goto MEMORY_ALLOC_FAIL;
    }

    /* allocate a region and put it in the aspace list */
    region = LOS_RegionAlloc(space, vaddr, size, regionFlags, 0);//分配一个线性区，并挂入空间中
    if (!region) {
        err = LOS_ERRNO_VM_NO_MEMORY;
        VM_ERR("failed to allocate region, vaddr: %#x, size: %#x, space: %#x", vaddr, size, space);
        goto MEMORY_ALLOC_FAIL;
    }

    /* return the vaddr if requested */
    if (ptr != NULL) {
        *ptr = (VOID *)(UINTPTR)region->range.base;
    }

    /* map all of the pages */
    vaddrTemp = region->range.base;
    while ((vmPage = LOS_ListRemoveHeadType(&pageList, LosVmPage, node))) {
        paddrTemp = vmPage->physAddr;
        LOS_AtomicInc(&vmPage->refCounts);
        err = LOS_ArchMmuMap(&space->archMmu, vaddrTemp, paddrTemp, 1, regionFlags);
        if (err != 1) {
            LOS_Panic("%s %d, LOS_ArchMmuMap failed!, err: %d\n", __FUNCTION__, __LINE__, err);
        }
        vaddrTemp += PAGE_SIZE;
    }
    err = LOS_OK;
    goto VMM_ALLOC_SUCCEED;

MEMORY_ALLOC_FAIL:
    (VOID)LOS_PhysPagesFree(&pageList);
VMM_ALLOC_SUCCEED:
    (VOID)LOS_MuxRelease(&space->regionMux);
    return err;
}

VOID *LOS_VMalloc(size_t size)//从g_vMallocSpace中申请物理内存
{
    LosVmSpace *space = &g_vMallocSpace;
    LosVmMapRegion *region = NULL;
    size_t sizeCount;
    size_t count;
    LosVmPage *vmPage = NULL;
    VADDR_T va;
    PADDR_T pa;
    STATUS_T ret;

    size = LOS_Align(size, PAGE_SIZE);//
    if ((size == 0) || (size > space->size)) {
        return NULL;
    }
    sizeCount = size >> PAGE_SHIFT;//按申请 所以需右移12位

    LOS_DL_LIST_HEAD(pageList);
    (VOID)LOS_MuxAcquire(&space->regionMux);//获得互斥锁

    count = LOS_PhysPagesAlloc(sizeCount, &pageList);//一页一页申请，并从pageList尾部插入
    if (count < sizeCount) {
        VM_ERR("failed to allocate enough pages (ask %zu, got %zu)", sizeCount, count);
        goto ERROR;
    }

    /* allocate a region and put it in the aspace list *///分配一个可读写的线性区，并挂在space
    region = LOS_RegionAlloc(space, 0, size, VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE, 0);//注意第二个参数是 vaddr = 0 !!!
    if (region == NULL) {
        VM_ERR("alloc region failed, size = %x", size);
        goto ERROR;
    }

    va = region->range.base;//va 该区范围基地址为虚拟地址的开始位置，理解va怎么来的是理解线性地址的关键！
    while ((vmPage = LOS_ListRemoveHeadType(&pageList, LosVmPage, node))) {//从pageList循环拿page
        pa = vmPage->physAddr;//获取page物理地址，因上面是通过LOS_PhysPagesAlloc分配
        LOS_AtomicInc(&vmPage->refCounts);//refCounts 自增
        ret = LOS_ArchMmuMap(&space->archMmu, va, pa, 1, region->regionFlags);//一页一页的map
        if (ret != 1) {
            VM_ERR("LOS_ArchMmuMap failed!, err;%d", ret);
        }
        va += PAGE_SIZE;//一页映射完成，进入下一页
    }//va 注意 region的虚拟地址页是连续的，但物理页可以不连续! 很重要！！！

    (VOID)LOS_MuxRelease(&space->regionMux);//释放互斥锁
    return (VOID *)(UINTPTR)region->range.base;//返回虚拟基地址供应用使用

ERROR:
    (VOID)LOS_PhysPagesFree(&pageList);//释放物理内存页
    (VOID)LOS_MuxRelease(&space->regionMux);//释放互斥锁
    return NULL;
}
//释放虚拟内存
VOID LOS_VFree(const VOID *addr)
{
    LosVmSpace *space = &g_vMallocSpace;
    LosVmMapRegion *region = NULL;
    STATUS_T ret;

    if (addr == NULL) {
        VM_ERR("addr is NULL!");
        return;
    }

    (VOID)LOS_MuxAcquire(&space->regionMux);

    region = LOS_RegionFind(space, (VADDR_T)(UINTPTR)addr);//先找到线性区
    if (region == NULL) {
        VM_ERR("find region failed");
        goto DONE;
    }

    ret = LOS_RegionFree(space, region);//释放线性区
    if (ret) {
        VM_ERR("free region failed, ret = %d", ret);
    }

DONE:
    (VOID)LOS_MuxRelease(&space->regionMux);
}

STATIC INLINE BOOL OsMemLargeAlloc(UINT32 size)
{
    UINT32 wasteMem;

    if (size < PAGE_SIZE) {
        return FALSE;
    }
    wasteMem = ROUNDUP(size, PAGE_SIZE) - size;
    /* that is 1K ram wasted, waste too much mem ! */
    return (wasteMem < VM_MAP_WASTE_MEM_LEVEL);//浪费大于1K时用伙伴算法
}

VOID *LOS_KernelMalloc(UINT32 size)
{
    VOID *ptr = NULL;

    if (OsMemLargeAlloc(size)) {//超过4K,为大尺寸内存用伙伴算法分配
        ptr = LOS_PhysPagesAllocContiguous(ROUNDUP(size, PAGE_SIZE) >> PAGE_SHIFT);//分配连续的物理内存页
    } else {
        ptr = LOS_MemAlloc(OS_SYS_MEM_ADDR, size);//从内存池分配
    }

    return ptr;
}

VOID *LOS_KernelMallocAlign(UINT32 size, UINT32 boundary)
{
    VOID *ptr = NULL;

    if (OsMemLargeAlloc(size) && IS_ALIGNED(PAGE_SIZE, boundary)) {
        ptr = LOS_PhysPagesAllocContiguous(ROUNDUP(size, PAGE_SIZE) >> PAGE_SHIFT);
    } else {
        ptr = LOS_MemAllocAlign(OS_SYS_MEM_ADDR, size, boundary);
    }

    return ptr;
}

VOID *LOS_KernelRealloc(VOID *ptr, UINT32 size)
{
    VOID *tmpPtr = NULL;
    LosVmPage *page = NULL;
    errno_t ret;

    if (ptr == NULL) {
        tmpPtr = LOS_KernelMalloc(size);
    } else {
        if (OsMemIsHeapNode(ptr) == FALSE) {
            page = OsVmVaddrToPage(ptr);
            if (page == NULL) {
                VM_ERR("page of ptr(%#x) is null", ptr);
                return NULL;
            }
            tmpPtr = LOS_KernelMalloc(size);
            if (tmpPtr == NULL) {
                VM_ERR("alloc memory failed");
                return NULL;
            }
            ret = memcpy_s(tmpPtr, size, ptr, page->nPages << PAGE_SHIFT);
            if (ret != EOK) {
                LOS_KernelFree(tmpPtr);
                VM_ERR("KernelRealloc memcpy error");
                return NULL;
            }
            OsMemLargeNodeFree(ptr);
        } else {
            tmpPtr = LOS_MemRealloc(OS_SYS_MEM_ADDR, ptr, size);
        }
    }

    return tmpPtr;
}

VOID LOS_KernelFree(VOID *ptr)
{
    UINT32 ret;

    if (OsMemIsHeapNode(ptr) == FALSE) {
        ret = OsMemLargeNodeFree(ptr);
        if (ret != LOS_OK) {
            VM_ERR("KernelFree %p failed", ptr);
            return;
        }
    } else {
        (VOID)LOS_MemFree(OS_SYS_MEM_ADDR, ptr);
    }
}

LosMux *OsGVmSpaceMuxGet(VOID)
{
    return &g_vmSpaceListMux;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

