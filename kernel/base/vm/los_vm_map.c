/*!
 * @file    los_vm_map.c
 * @brief 虚拟内存管理
 * @link vm http://weharmonyos.com/openharmony/zh-cn/device-dev/kernel/kernel-small-basic-memory-virtual.html @endlink
   @verbatim
基本概念
   	虚拟内存管理是计算机系统管理内存的一种技术。每个进程都有连续的虚拟地址空间，虚拟地址空间的大小由CPU的位数决定，
   	32位的硬件平台可以提供的最大的寻址空间为0-4GiB。整个4GiB空间分成两部分，LiteOS-A内核占据3GiB的高地址空间，
   	1GiB的低地址空间留给进程使用。各个进程空间的虚拟地址空间是独立的，代码、数据互不影响。
   
   	系统将虚拟内存分割为称为虚拟页的内存块，大小一般为4KiB或64KiB，LiteOS-A内核默认的页的大小是4KiB，
   	根据需要可以对MMU（Memory Management Units）进行配置。虚拟内存管理操作的最小单位就是一个页，
   	LiteOS-A内核中一个虚拟地址区间region包含地址连续的多个虚拟页，也可只有一个页。同样，物理内存也会按照页大小进行分割，
   	分割后的每个内存块称为页帧。虚拟地址空间划分：内核态占高地址3GiB(0x40000000 ~ 0xFFFFFFFF)，
   	用户态占低地址1GiB(0x01000000 ~ 0x3F000000)，具体见下表，详细可以查看或配置los_vm_zone.h。
   	
内核态地址规划：
   Zone名称 		描述 														属性
   ----------------------------------------------------------------------------
   DMA zone		供IO设备的DMA使用。											Uncache
   
   Normal zone	加载内核代码段、数据段、堆和栈的地址区间。									Cache
   
   high mem zone可以分配连续的虚拟内存，但其所映射的物理内存不一定连续。Cache

用户态地址规划：
  Zone名称	    描述													    属性
  ----------------------------------------------------------------------------
  代码段	   		用户态代码段地址区间。										   		Cache
  堆				用户态堆地址区间。												Cache
  栈				用户态栈地址区间。								   				Cache
  共享库			用于加载用户态共享库的地址区间，包括mmap所映射的区间。							Cache

运行机制
   虚拟内存管理中，虚拟地址空间是连续的，但是其映射的物理内存并不一定是连续的，如下图所示。
   可执行程序加载运行，CPU访问虚拟地址空间的代码或数据时存在两种情况：
   
   1. CPU访问的虚拟地址所在的页，如V0，已经与具体的物理页P0做映射，CPU通过找到进程对应的页表条目（详见虚实映射），
   根据页表条目中的物理地址信息访问物理内存中的内容并返回。
   2. CPU访问的虚拟地址所在的页，如V2，没有与具体的物理页做映射，系统会触发缺页异常，系统申请一个物理页，
   并把相应的信息拷贝到物理页中，并且把物理页的起始地址更新到页表条目中。此时CPU重新执行访问虚拟内存的指令
   便能够访问到具体的代码或数据。

   @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-11-25
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
#ifdef LOSCFG_FS_VFS
#include "fs/file.h"
#include "vnode.h"
#endif
#include "los_task.h"
#include "los_memory_pri.h"
#include "los_vm_boot.h"


#ifdef LOSCFG_KERNEL_VM

#define VM_MAP_WASTE_MEM_LEVEL          (PAGE_SIZE >> 2) ///<	浪费内存大小(1K)
LosMux g_vmSpaceListMux;				///< 用于锁g_vmSpaceList的互斥量
LOS_DL_LIST_HEAD(g_vmSpaceList);		///< 初始化全局虚拟空间节点,所有虚拟空间都挂到此节点上.
LosVmSpace g_kVmSpace;					///< 内核空间,用于内核运行栈,代码区,数据区
LosVmSpace g_vMallocSpace;				///< 内核堆空间,用于内核分配内存

/************************************************************
* 获取进程空间系列接口
************************************************************/
/// 获取当前进程空间结构体指针
LosVmSpace *LOS_CurrSpaceGet(VOID)
{
    return OsCurrProcessGet()->vmSpace;
}
/// 获取虚拟地址对应的进程空间结构体指针
LosVmSpace *LOS_SpaceGet(VADDR_T vaddr)
{
    if (LOS_IsKernelAddress(vaddr)) {	//是否为内核空间
        return LOS_GetKVmSpace();		//获取内核空间
    } else if (LOS_IsUserAddress(vaddr)) {//是否为用户空间
        return LOS_CurrSpaceGet();
    } else if (LOS_IsVmallocAddress(vaddr)) {//是否为内核堆空间
        return LOS_GetVmallocSpace();//获取内核堆空间
    } else {
        return NULL;
    }
}
///内核空间只有g_kVmSpace一个，所有的内核进程都共用一个内核空间
LosVmSpace *LOS_GetKVmSpace(VOID)
{
    return &g_kVmSpace;
}
///获取进程空间链表指针 g_vmSpaceList中挂的是进程空间 g_kVmSpace, g_vMallocSpace,所有用户进程的空间(独有一个进程空间)
LOS_DL_LIST *LOS_GetVmSpaceList(VOID)
{
    return &g_vmSpaceList;
}
///获取内核堆空间的全局变量
LosVmSpace *LOS_GetVmallocSpace(VOID)
{
    return &g_vMallocSpace;
}

/************************************************************
* 虚拟地址区间region相关的操作
************************************************************/
///释放挂在红黑树上节点,等于释放了线性区
ULONG_T OsRegionRbFreeFn(LosRbNode *pstNode)
{
    LOS_MemFree(m_aucSysMem0, pstNode);
    return LOS_OK;
}
///通过红黑树节点找到对应的线性区
VOID *OsRegionRbGetKeyFn(LosRbNode *pstNode)
{
    LosVmMapRegion *region = (LosVmMapRegion *)LOS_DL_LIST_ENTRY(pstNode, LosVmMapRegion, rbNode);
    return (VOID *)&region->range;
}
///比较两个红黑树节点
ULONG_T OsRegionRbCmpKeyFn(const VOID *pNodeKeyA, const VOID *pNodeKeyB)
{
    LosVmMapRange rangeA = *(LosVmMapRange *)pNodeKeyA;
    LosVmMapRange rangeB = *(LosVmMapRange *)pNodeKeyB;
    UINT32 startA = rangeA.base;
    UINT32 endA = rangeA.base + rangeA.size - 1;
    UINT32 startB = rangeB.base;
    UINT32 endB = rangeB.base + rangeB.size - 1;

    if (startA > endB) {// A基地址大于B的结束地址
        return RB_BIGGER; //说明线性区A更大,在右边
    } else if (startA >= startB) {
        if (endA <= endB) {
            return RB_EQUAL; //相等,说明 A在B中
        } else {
            return RB_BIGGER; //说明 A的结束地址更大
        }
    } else if (startA <= startB) { //A基地址小于等于B的基地址
        if (endA >= endB) {
            return RB_EQUAL; //相等 说明 B在A中
        } else {
            return RB_SMALLER;//说明A的结束地址更小
        }
    } else if (endA < startB) {//A结束地址小于B的开始地址
        return RB_SMALLER;//说明A在
    }
    return RB_EQUAL;
}

/*!
 * @brief OsVmSpaceInitCommon	初始化进程虚拟空间，必须提供L1表的虚拟内存地址
 *
 * @param virtTtb L1表的地址，TTB表地址	
 * @param vmSpace	
 * @return	
 *
 * @see
 */
STATIC BOOL OsVmSpaceInitCommon(LosVmSpace *vmSpace, VADDR_T *virtTtb)
{
    LOS_RbInitTree(&vmSpace->regionRbTree, OsRegionRbCmpKeyFn, OsRegionRbFreeFn, OsRegionRbGetKeyFn);//初始化虚拟存储空间-以红黑树组织方式

    status_t retval = LOS_MuxInit(&vmSpace->regionMux, NULL);//初始化互斥量
    if (retval != LOS_OK) {
        VM_ERR("Create mutex for vm space failed, status: %d", retval);
        return FALSE;
    }

    (VOID)LOS_MuxAcquire(&g_vmSpaceListMux);
    LOS_ListAdd(&g_vmSpaceList, &vmSpace->node);//将虚拟空间挂入全局虚拟空间双循环链表上
    (VOID)LOS_MuxRelease(&g_vmSpaceListMux);

    return OsArchMmuInit(&vmSpace->archMmu, virtTtb);//对mmu初始化
}
///@note_thinking 这个函数名称和内容不太搭
VOID OsVmMapInit(VOID)
{
    status_t retval = LOS_MuxInit(&g_vmSpaceListMux, NULL);//初始化虚拟空间的互斥量
    if (retval != LOS_OK) {
        VM_ERR("Create mutex for g_vmSpaceList failed, status: %d", retval);
    }
}
///初始化内核虚拟空间
BOOL OsKernVmSpaceInit(LosVmSpace *vmSpace, VADDR_T *virtTtb)//内核空间页表是编译时放在bbs段指定的,共用 L1表
{
    vmSpace->base = KERNEL_ASPACE_BASE;//内核空间基地址, 线性区将分配在此范围
    vmSpace->size = KERNEL_ASPACE_SIZE;//内核空间大小
    vmSpace->mapBase = KERNEL_VMM_BASE;//内核空间映射区基地址
    vmSpace->mapSize = KERNEL_VMM_SIZE;//内核空间映射区大小
#ifdef LOSCFG_DRIVERS_TZDRIVER
    vmSpace->codeStart = 0;	//代码区开始地址
    vmSpace->codeEnd = 0;	//代码区结束地址
#endif
    return OsVmSpaceInitCommon(vmSpace, virtTtb);//virtTtb 用于初始化 mmu
}
///初始化内核堆空间
BOOL OsVMallocSpaceInit(LosVmSpace *vmSpace, VADDR_T *virtTtb)//内核动态空间的页表是动态申请得来，共用 L1表
{
    vmSpace->base = VMALLOC_START;		//内核堆空间基地址
    vmSpace->size = VMALLOC_SIZE;		//内核堆空间大小
    vmSpace->mapBase = VMALLOC_START;	//内核堆空间映射基地址
    vmSpace->mapSize = VMALLOC_SIZE;	//内核堆空间映射区大小
#ifdef LOSCFG_DRIVERS_TZDRIVER
    vmSpace->codeStart = 0;
    vmSpace->codeEnd = 0;
#endif
    return OsVmSpaceInitCommon(vmSpace, virtTtb);//创建MMU,为后续的虚实映射做好初始化的工作
}
///内核进程虚拟空间初始化
VOID OsKSpaceInit(VOID)
{
    OsVmMapInit();//初始化后续操作 g_vmSpaceList 的互斥锁 
    OsKernVmSpaceInit(&g_kVmSpace, OsGFirstTableGet()); //初始化内核进程虚拟空间
    OsVMallocSpaceInit(&g_vMallocSpace, OsGFirstTableGet());//初始化内核动态分配空间
}
/*!
 * @brief OsUserVmSpaceInit	用户空间的TTB表是动态申请得来,每个进程有属于自己的L1,L2表
 * 初始化用户进程虚拟空间,主要划分数据区,堆区,映射区和创建mmu
 * @param virtTtb	
 * @param vmSpace	
 * @return	
 *
 * @see
 */
BOOL OsUserVmSpaceInit(LosVmSpace *vmSpace, VADDR_T *virtTtb)
{
    vmSpace->base = USER_ASPACE_BASE;//用户空间基地址
    vmSpace->size = USER_ASPACE_SIZE;//用户空间大小
    vmSpace->mapBase = USER_MAP_BASE;//用户空间映射基地址
    vmSpace->mapSize = USER_MAP_SIZE;//用户空间映射大小
    vmSpace->heapBase = USER_HEAP_BASE;//用户堆区开始地址,只有用户进程需要设置这里，动态内存的开始地址
    vmSpace->heapNow = USER_HEAP_BASE;//堆区最新指向地址，用户堆空间大小可通过系统调用 do_brk()扩展
    vmSpace->heap = NULL;	//最近分配的一个堆线性区
#ifdef LOSCFG_DRIVERS_TZDRIVER
    vmSpace->codeStart = 0;
    vmSpace->codeEnd = 0;
#endif
    return OsVmSpaceInitCommon(vmSpace, virtTtb);//创建MMU,为后续的虚实映射做好初始化的工作
}
/// 创建用户进程空间
LosVmSpace *OsCreateUserVmSpace(VOID)
{
    BOOL retVal = FALSE;

    LosVmSpace *space = LOS_MemAlloc(m_aucSysMem0, sizeof(LosVmSpace));//在内核空间申请用户进程空间
    if (space == NULL) {
        return NULL;
    }

    VADDR_T *ttb = LOS_PhysPagesAllocContiguous(1);//分配一个物理页用于存放虚实内存映射关系, 即:L1表
    if (ttb == NULL) {//若连映射页都没有,剩下的也别玩了.
        (VOID)LOS_MemFree(m_aucSysMem0, space);
        return NULL;
    }

    (VOID)memset_s(ttb, PAGE_SIZE, 0, PAGE_SIZE);
    retVal = OsUserVmSpaceInit(space, ttb);//初始化用户空间,mmu
    LosVmPage *vmPage = OsVmVaddrToPage(ttb);
    if ((retVal == FALSE) || (vmPage == NULL)) {
        (VOID)LOS_MemFree(m_aucSysMem0, space);
        LOS_PhysPagesFreeContiguous(ttb, 1);
        return NULL;
    }
    LOS_ListAdd(&space->archMmu.ptList, &(vmPage->node));

    return space;
}

STATIC BOOL OsVmSpaceParamCheck(LosVmSpace *vmSpace)//这么简单也要写个函数?
{
    if (vmSpace == NULL) {
        return FALSE;
    }
    return TRUE;
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

    if ((OsIsVmRegionEmpty(oldVmSpace) == TRUE) || (oldVmSpace == &g_kVmSpace)) {//不允许clone内核空间,内核空间是独一无二的.
        return LOS_ERRNO_VM_INVALID_ARGS;
    }
	//空间克隆的主体实现是:线性区重新一个个分配物理内存,重新映射.
    /* search the region list */
    newVmSpace->mapBase = oldVmSpace->mapBase; //复制映射区基址
    newVmSpace->heapBase = oldVmSpace->heapBase; //复制堆区基址
    newVmSpace->heapNow = oldVmSpace->heapNow;	//复制堆区当前使用到哪了
    (VOID)LOS_MuxAcquire(&oldVmSpace->regionMux);
    RB_SCAN_SAFE(&oldVmSpace->regionRbTree, pstRbNode, pstRbNodeNext)//红黑树循环开始
        oldRegion = (LosVmMapRegion *)pstRbNode;
        newRegion = OsVmRegionDup(newVmSpace, oldRegion, oldRegion->range.base, oldRegion->range.size);//复制线性区
        if (newRegion == NULL) {
            VM_ERR("dup new region failed");
            ret = LOS_ERRNO_VM_NO_MEMORY;
            break;
        }

#ifdef LOSCFG_KERNEL_SHM
        if (oldRegion->regionFlags & VM_MAP_REGION_FLAG_SHM) {//如果老线性区是共享内存
            OsShmFork(newVmSpace, oldRegion, newRegion);//fork共享线性区,如此新虚拟空间也能用那个线性区
            continue;//不往下走了,因为共享内存不需要重新映射,下面无非就是需要MMU映射虚拟地址<-->物理地址
        }
#endif

        if (oldRegion == oldVmSpace->heap) {//如果这个线性区是堆区
            newVmSpace->heap = newRegion;//那么新的线性区也是新虚拟空间的堆区
        }

        numPages = newRegion->range.size >> PAGE_SHIFT;//计算线性区页数
        for (i = 0; i < numPages; i++) {//一页一页进行重新映射
            vaddr = newRegion->range.base + (i << PAGE_SHIFT);
            if (LOS_ArchMmuQuery(&oldVmSpace->archMmu, vaddr, &paddr, &flags) != LOS_OK) {//先查物理地址
                continue;
            }

            page = LOS_VmPageGet(paddr);//通过物理页获取物理内存的页框
            if (page != NULL) {
                LOS_AtomicInc(&page->refCounts);//refCounts 自增
            }
            if (flags & VM_MAP_REGION_FLAG_PERM_WRITE) {//可写入区标签
                LOS_ArchMmuUnmap(&oldVmSpace->archMmu, vaddr, 1);//先删除老空间映射
                LOS_ArchMmuMap(&oldVmSpace->archMmu, vaddr, paddr, 1, flags & ~VM_MAP_REGION_FLAG_PERM_WRITE);//老空间重新映射
            }
            LOS_ArchMmuMap(&newVmSpace->archMmu, vaddr, paddr, 1, flags & ~VM_MAP_REGION_FLAG_PERM_WRITE);//映射新空间

#ifdef LOSCFG_FS_VFS //文件系统开关
            if (LOS_IsRegionFileValid(oldRegion)) {//是都是一个文件映射线性区
                LosFilePage *fpage = NULL;
                LOS_SpinLockSave(&oldRegion->unTypeData.rf.vnode->mapping.list_lock, &intSave);
                fpage = OsFindGetEntry(&oldRegion->unTypeData.rf.vnode->mapping, newRegion->pgOff + i);
                if ((fpage != NULL) && (fpage->vmPage == page)) { /* cow page no need map */
                    OsAddMapInfo(fpage, &newVmSpace->archMmu, vaddr);//添加文件页映射,记录页面被进程映射过
                }
                LOS_SpinUnlockRestore(&oldRegion->unTypeData.rf.vnode->mapping.list_lock, intSave);
            }
#endif
        }
    RB_SCAN_SAFE_END(&oldVmSpace->regionRbTree, pstRbNode, pstRbNodeNext)//红黑树循环结束
    (VOID)LOS_MuxRelease(&oldVmSpace->regionMux);
    return ret;
}
///通过虚拟(线性)地址查找所属线性区,红黑树
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
/// 查找线性区 根据起始地址在进程空间内查找是否存在
LosVmMapRegion *LOS_RegionFind(LosVmSpace *vmSpace, VADDR_T addr)
{
    LosVmMapRegion *region = NULL;

    (VOID)LOS_MuxAcquire(&vmSpace->regionMux);//因进程空间是隔离的,所以此处只会涉及到任务(线程)之间的竞争,故使用互斥锁,而自旋锁则用于CPU核间的竞争
    region = OsFindRegion(&vmSpace->regionRbTree, addr, 1);
    (VOID)LOS_MuxRelease(&vmSpace->regionMux);

    return region;
}
/// 查找线性区 根据地址区间在进程空间内查找是否存在
LosVmMapRegion *LOS_RegionRangeFind(LosVmSpace *vmSpace, VADDR_T addr, size_t len)
{
    LosVmMapRegion *region = NULL;

    (VOID)LOS_MuxAcquire(&vmSpace->regionMux);
    region = OsFindRegion(&vmSpace->regionRbTree, addr, len);
    (VOID)LOS_MuxRelease(&vmSpace->regionMux);

    return region;
}
/// 分配指定长度的线性区
VADDR_T OsAllocRange(LosVmSpace *vmSpace, size_t len)
{
    LosVmMapRegion *curRegion = NULL;
    LosRbNode *pstRbNode = NULL;
    LosRbNode *pstRbNodeTmp = NULL;
    LosRbTree *regionRbTree = &vmSpace->regionRbTree;
    VADDR_T curEnd = vmSpace->mapBase;//获取映射区基地址
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
    if ((nextStart >= curEnd) && ((nextStart - curEnd) >= len)) {
        return curEnd;
    }

    return 0;
}
/// 分配指定开始地址和长度的线性区
VADDR_T OsAllocSpecificRange(LosVmSpace *vmSpace, VADDR_T vaddr, size_t len, UINT32 regionFlags)
{
    STATUS_T status;

    if (LOS_IsRangeInSpace(vmSpace, vaddr, len) == FALSE) {//虚拟地址是否在进程空间范围内
        return 0;
    }

    if ((LOS_RegionFind(vmSpace, vaddr) != NULL) ||
        (LOS_RegionFind(vmSpace, vaddr + len - 1) != NULL) ||
        (LOS_RegionRangeFind(vmSpace, vaddr, len - 1) != NULL)) {//没找到的情况
        if ((regionFlags & VM_MAP_REGION_FLAG_FIXED_NOREPLACE) != 0) {
            return 0;
        } else if ((regionFlags & VM_MAP_REGION_FLAG_FIXED) != 0) {//线性区未填满,则解除这部分空间的映射
            status = LOS_UnMMap(vaddr, len);//解除映射
            if (status != LOS_OK) {
                VM_ERR("unmap specific range va: %#x, len: %#x failed, status: %d", vaddr, len, status);
                return 0;
            }
        } else {
            return OsAllocRange(vmSpace, len);//默认分配一个
        }
    }

    return vaddr;
}
///映射类型为文件的线性区是否有效
BOOL LOS_IsRegionFileValid(LosVmMapRegion *region)
{
    if ((region != NULL) && (LOS_IsRegionTypeFile(region)) &&
        (region->unTypeData.rf.vnode != NULL)) {
        return TRUE;
    }
    return FALSE;
}
///向红黑树中插入线性区
BOOL OsInsertRegion(LosRbTree *regionRbTree, LosVmMapRegion *region)
{
    if (LOS_RbAddNode(regionRbTree, (LosRbNode *)region) == FALSE) {
        VM_ERR("insert region failed, base: %#x, size: %#x", region->range.base, region->range.size);
        OsDumpAspace(region->space);
        return FALSE;
    }
    return TRUE;
}
///创建一个线性区
LosVmMapRegion *OsCreateRegion(VADDR_T vaddr, size_t len, UINT32 regionFlags, unsigned long offset)
{
    LosVmMapRegion *region = LOS_MemAlloc(m_aucSysMem0, sizeof(LosVmMapRegion));//只是分配一个线性区结构体
    if (region == NULL) {
        VM_ERR("memory allocate for LosVmMapRegion failed");
        return region;
    }
	//创建线性区的本质就是在画饼，见如下操作:
    memset_s(region, sizeof(LosVmMapRegion), 0, sizeof(LosVmMapRegion));
    region->range.base = vaddr;	//虚拟地址作为线性区的基地址
    region->range.size = len;	//线性区大小，这是线性区构思最巧妙的地方，只要不过分，蓝图随便画。
    region->pgOff = offset;		//页标
    region->regionFlags = regionFlags;//标识,可读/可写/可执行
    region->regionType = VM_MAP_REGION_TYPE_NONE;//未映射
    region->forkFlags = 0;		//
    region->shmid = -1;			//默认线性区为不共享,无共享资源ID
    return region;
}
///通过虚拟地址查询映射的物理地址
PADDR_T LOS_PaddrQuery(VOID *vaddr)
{
    PADDR_T paddr = 0;
    STATUS_T status;
    LosVmSpace *space = NULL;
    LosArchMmu *archMmu = NULL;
    //先取出对应空间的mmu
    if (LOS_IsKernelAddress((VADDR_T)(UINTPTR)vaddr)) {//是否内核空间地址
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

/*!
 * 这里不是真的分配物理内存，而是逻辑上画一个连续的区域，标记这个区域可以拿用，表示内存已经归你了。
   但真正的物理内存的占用会延迟到使用的时候才由缺页中断调入内存
*/
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
    if (vaddr == 0) {//如果地址是0，根据线性区管理的实际情况,自动创建虚拟地址，    这是创建新映射的最便捷的方法。
        rstVaddr = OsAllocRange(vmSpace, len);
    } else {
        /* if it is already mmapped here, we unmmap it */
        rstVaddr = OsAllocSpecificRange(vmSpace, vaddr, len, regionFlags);//创建包含指定虚拟地址的线性区,       rstVaddr !=        vaddr || rstVaddr == vaddr
        if (rstVaddr == 0) {
            VM_ERR("alloc specific range va: %#x, len: %#x failed", vaddr, len);
            goto OUT;
        }
    }
    if (rstVaddr == 0) {//没有可供映射的虚拟地址
        goto OUT;
    }

    newRegion = OsCreateRegion(rstVaddr, len, regionFlags, pgoff);//创建一个线性区,指定线性区的开始地址rstVaddr ...
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
/*!
 * 删除匿名页,匿名页就是内存映射页
 * 1.解除映射关系 2.释放物理内存
*/
STATIC VOID OsAnonPagesRemove(LosArchMmu *archMmu, VADDR_T vaddr, UINT32 count)
{
    status_t status;
    paddr_t paddr;
    LosVmPage *page = NULL;

    if ((archMmu == NULL) || (vaddr == 0) || (count == 0)) {
        VM_ERR("OsAnonPagesRemove invalid args, archMmu %p, vaddr %p, count %d", archMmu, vaddr, count);
        return;
    }

    while (count > 0) {//一页页操作
        count--;
        status = LOS_ArchMmuQuery(archMmu, vaddr, &paddr, NULL);//通过虚拟地址拿到物理地址
        if (status != LOS_OK) {//失败，拿下一页的物理地址
            vaddr += PAGE_SIZE;
            continue;
        }

        LOS_ArchMmuUnmap(archMmu, vaddr, 1);//解除一页的映射

        page = LOS_VmPageGet(paddr);//通过物理地址获取所在物理页框的起始地址
        if (page != NULL) {//获取成功
            if (!OsIsPageShared(page)) {//不是共享页，共享页会有专门的共享标签，共享本质是有无多个进程对该页的引用
                LOS_PhysPageFree(page);//释放物理页框
            }
        }
        vaddr += PAGE_SIZE;
    }
}

STATIC VOID OsDevPagesRemove(LosArchMmu *archMmu, VADDR_T vaddr, UINT32 count)
{
    status_t status;

    if ((archMmu == NULL) || (vaddr == 0) || (count == 0)) {
        VM_ERR("OsDevPagesRemove invalid args, archMmu %p, vaddr %p, count %d", archMmu, vaddr, count);
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
/// 释放进程空间指定线性区
STATUS_T LOS_RegionFree(LosVmSpace *space, LosVmMapRegion *region)
{
    if ((space == NULL) || (region == NULL)) {
        VM_ERR("args error, aspace %p, region %p", space, region);
        return LOS_ERRNO_VM_INVALID_ARGS;
    }

    (VOID)LOS_MuxAcquire(&space->regionMux);

#ifdef LOSCFG_FS_VFS //文件开关
    if (LOS_IsRegionFileValid(region)) {//是否为文件线性区
        OsFilePagesRemove(space, region);//删除文件页
        VnodeHold();
        region->unTypeData.rf.vnode->useCount--;
        VnodeDrop();
    } else
#endif
#ifdef LOSCFG_KERNEL_SHM	//共享内存开关
    if (OsIsShmRegion(region)) { //是否为共享内存线性区
        OsShmRegionFree(space, region);//释放共享线性区
    } else if (LOS_IsRegionTypeDev(region)) {
#else
    if (LOS_IsRegionTypeDev(region)) {//如果是设备线性区
#endif
        OsDevPagesRemove(&space->archMmu, region->range.base, region->range.size >> PAGE_SHIFT);//删除映射设备
    } else {
        OsAnonPagesRemove(&space->archMmu, region->range.base, region->range.size >> PAGE_SHIFT);//删除匿名映射
    }

    /* remove it from space */
    LOS_RbDelNode(&space->regionRbTree, &region->rbNode);//从红黑树中删除线性区
    /* free it */
    LOS_MemFree(m_aucSysMem0, region);//释放线性区结构体占用的内存
    (VOID)LOS_MuxRelease(&space->regionMux);
    return LOS_OK;
}
/// 复制线性区
LosVmMapRegion *OsVmRegionDup(LosVmSpace *space, LosVmMapRegion *oldRegion, VADDR_T vaddr, size_t size)
{
    LosVmMapRegion *newRegion = NULL;
    UINT32 regionFlags;

    (VOID)LOS_MuxAcquire(&space->regionMux);
    regionFlags = oldRegion->regionFlags;
    if (vaddr == 0) {//不指定地址
        regionFlags &= ~(VM_MAP_REGION_FLAG_FIXED | VM_MAP_REGION_FLAG_FIXED_NOREPLACE); //撕掉两个标签
    } else {
        regionFlags |= VM_MAP_REGION_FLAG_FIXED; //贴上填满线性区标签
    }
    newRegion = LOS_RegionAlloc(space, vaddr, size, regionFlags, oldRegion->pgOff); //分配一个线性区
    if (newRegion == NULL) {
        VM_ERR("LOS_RegionAlloc failed");
        goto REGIONDUPOUT;
    }
    newRegion->regionType = oldRegion->regionType;//复制线性区类型(文件,设备,匿名)

#ifdef LOSCFG_KERNEL_SHM
    if (OsIsShmRegion(oldRegion)) {//如果是共享内存
        newRegion->shmid = oldRegion->shmid;//复制共享ID
    }
#endif

#ifdef LOSCFG_FS_VFS //文件开关
    if (LOS_IsRegionTypeFile(oldRegion)) {//如果是文件线性区
        newRegion->unTypeData.rf.vmFOps = oldRegion->unTypeData.rf.vmFOps; //文件操作接口
        newRegion->unTypeData.rf.vnode = oldRegion->unTypeData.rf.vnode; //文件索引节点
        newRegion->unTypeData.rf.f_oflags = oldRegion->unTypeData.rf.f_oflags;//读写标签
        VnodeHold();
        newRegion->unTypeData.rf.vnode->useCount++;//索引节点使用数增加
        VnodeDrop();
    }
#endif

REGIONDUPOUT:
    (VOID)LOS_MuxRelease(&space->regionMux);
    return newRegion;
}
/// 劈开线性区
STATIC LosVmMapRegion *OsVmRegionSplit(LosVmMapRegion *oldRegion, VADDR_T newRegionStart)
{
    LosVmMapRegion *newRegion = NULL;
    LosVmSpace *space = oldRegion->space;
    size_t size = LOS_RegionSize(newRegionStart, LOS_RegionEndAddr(oldRegion));//获取线性区大小

    oldRegion->range.size = LOS_RegionSize(oldRegion->range.base, newRegionStart - 1);//获取旧线性区大小
    if (oldRegion->range.size == 0) {
        LOS_RbDelNode(&space->regionRbTree, &oldRegion->rbNode);
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
///对线性区进行调整
STATUS_T OsVmRegionAdjust(LosVmSpace *space, VADDR_T newRegionStart, size_t size)
{
    LosVmMapRegion *region = NULL;
    VADDR_T nextRegionBase = newRegionStart + size;
    LosVmMapRegion *newRegion = NULL;

    region = LOS_RegionFind(space, newRegionStart);//先找到线性区
    if ((region != NULL) && (newRegionStart > region->range.base)) {
        newRegion = OsVmRegionSplit(region, newRegionStart);
        if (newRegion == NULL) {
            VM_ERR("region split fail");
            return LOS_ERRNO_VM_NO_MEMORY;
        }
    }

    region = LOS_RegionFind(space, nextRegionBase - 1);
    if ((region != NULL) && (nextRegionBase < LOS_RegionEndAddr(region))) {
        newRegion = OsVmRegionSplit(region, nextRegionBase);
        if (newRegion == NULL) {
            VM_ERR("region split fail");
            return LOS_ERRNO_VM_NO_MEMORY;
        }
    }

    return LOS_OK;
}
///删除线性区
STATUS_T OsRegionsRemove(LosVmSpace *space, VADDR_T regionBase, size_t size)
{
    STATUS_T status;
    VADDR_T regionEnd = regionBase + size - 1;
    LosVmMapRegion *regionTemp = NULL;
    LosRbNode *pstRbNodeTemp = NULL;
    LosRbNode *pstRbNodeNext = NULL;

    (VOID)LOS_MuxAcquire(&space->regionMux);

    status = OsVmRegionAdjust(space, regionBase, size);//线性区调整
    if (status != LOS_OK) {
        goto ERR_REGION_SPLIT;
    }

    RB_SCAN_SAFE(&space->regionRbTree, pstRbNodeTemp, pstRbNodeNext)//扫描虚拟空间内的线性区
        regionTemp = (LosVmMapRegion *)pstRbNodeTemp;
        if (regionTemp->range.base > regionEnd) {
            break;
        }
        if (regionBase <= regionTemp->range.base && regionEnd >= LOS_RegionEndAddr(regionTemp)) {
            status = LOS_RegionFree(space, regionTemp);
            if (status != LOS_OK) {
                VM_ERR("fail to free region, status=%d", status);
                goto ERR_REGION_SPLIT;
            }
        }

    RB_SCAN_SAFE_END(&space->regionRbTree, pstRbNodeTemp, pstRbNodeNext)

ERR_REGION_SPLIT:
    (VOID)LOS_MuxRelease(&space->regionMux);
    return status;
}
///根据指定参数范围[addr,addr+len] 释放用户空间中堆区所占用的物理内存
INT32 OsUserHeapFree(LosVmSpace *vmSpace, VADDR_T addr, size_t len)
{
    LosVmMapRegion *vmRegion = NULL;
    LosVmPage *vmPage = NULL;
    PADDR_T paddr = 0;
    VADDR_T vaddr;
    STATUS_T ret;

    if (vmSpace == LOS_GetKVmSpace() || vmSpace->heap == NULL) {//虚拟空间堆区必须在非内核空间
        return -1;
    }

    vmRegion = LOS_RegionFind(vmSpace, addr);//通过参数虚拟地址红黑树找到线性区,线性区范围内包含了参数虚拟地址
    if (vmRegion == NULL) {
        return -1;
    }

    if (vmRegion == vmSpace->heap) {//地址所在的线性区为堆区
        vaddr = addr;
        while (len > 0) {//参数0 代表不获取 flags 信息
            if (LOS_ArchMmuQuery(&vmSpace->archMmu, vaddr, &paddr, 0) == LOS_OK) {//通过虚拟地址查到物理地址
                ret = LOS_ArchMmuUnmap(&vmSpace->archMmu, vaddr, 1);//解除映射关系以页为单位,这里解除1页
                if (ret <= 0) {
                    VM_ERR("unmap failed, ret = %d", ret);
                }
                vmPage = LOS_VmPageGet(paddr);//获取物理页面信息
                LOS_PhysPageFree(vmPage);//释放页
            }
            vaddr += PAGE_SIZE;
            len -= PAGE_SIZE;
        }
        return 0;
    }

    return -1;
}
///线性区是否支持扩展
STATUS_T OsIsRegionCanExpand(LosVmSpace *space, LosVmMapRegion *region, size_t size)
{
    LosVmMapRegion *nextRegion = NULL;

    if ((space == NULL) || (region == NULL)) {
        return LOS_NOK;
    }

    nextRegion = (LosVmMapRegion *)LOS_RbSuccessorNode(&space->regionRbTree, &region->rbNode);
    /* if the gap is larger than size, then we can expand */
    if ((nextRegion != NULL) && ((nextRegion->range.base - region->range.base) >= size)) {
        return LOS_OK;
    }

    return LOS_NOK;
}
///解除一定范围的虚拟地址的映射关系
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
/// 释放所有线性区
STATIC VOID OsVmSpaceAllRegionFree(LosVmSpace *space)
{
    LosRbNode *pstRbNode = NULL;
    LosRbNode *pstRbNodeNext = NULL;

    /* free all of the regions */
    RB_SCAN_SAFE(&space->regionRbTree, pstRbNode, pstRbNodeNext) //遍历红黑树
        LosVmMapRegion *region = (LosVmMapRegion *)pstRbNode;//拿到线性区
        if (region->range.size == 0) {
            VM_ERR("space free, region: %#x flags: %#x, base:%#x, size: %#x",
                   region, region->regionFlags, region->range.base, region->range.size);
        }
        STATUS_T ret = LOS_RegionFree(space, region);//释放线性区
        if (ret != LOS_OK) {
            VM_ERR("free region error, space %p, region %p", space, region);
        }
    RB_SCAN_SAFE_END(&space->regionRbTree, pstRbNode, pstRbNodeNext)//要好好研究下这几个宏,有点意思

    return;
}
/// 释放虚拟空间
STATUS_T OsVmSpaceRegionFree(LosVmSpace *space)
{
    if (space == NULL) {
        return LOS_ERRNO_VM_INVALID_ARGS;
    }

    if (space == &g_kVmSpace) {
        VM_ERR("try to free kernel aspace, not allowed");
        return LOS_OK;
    }

    (VOID)LOS_MuxAcquire(&space->regionMux);
    OsVmSpaceAllRegionFree(space);
    (VOID)LOS_MuxRelease(&space->regionMux);

    return LOS_OK;
}
///释放虚拟空间,注意内核空间不能被释放掉,永驻内存
STATUS_T LOS_VmSpaceFree(LosVmSpace *space)
{
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

    OsVmSpaceAllRegionFree(space);

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
///虚拟地址和size是否在空间
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
/// 在进程空间中预留一块内存空间
STATUS_T LOS_VmSpaceReserve(LosVmSpace *space, size_t size, VADDR_T vaddr)
{
    UINT32 regionFlags = 0;

    if ((space == NULL) || (size == 0) || (!IS_PAGE_ALIGNED(vaddr) || !IS_PAGE_ALIGNED(size))) {
        return LOS_ERRNO_VM_INVALID_ARGS;
    }

    if (!LOS_IsRangeInSpace(space, vaddr, size)) {
        return LOS_ERRNO_VM_OUT_OF_RANGE;
    }

    /* lookup how it's already mapped */
    (VOID)LOS_ArchMmuQuery(&space->archMmu, vaddr, NULL, &regionFlags);

    /* build a new region structure */
    LosVmMapRegion *region = LOS_RegionAlloc(space, vaddr, size, regionFlags | VM_MAP_REGION_FLAG_FIXED, 0);

    return region ? LOS_OK : LOS_ERRNO_VM_NO_MEMORY;
}
///实现从虚拟地址到物理地址的映射,将指定长度的物理地址区间与虚拟地址区间做映射，需提前申请物理地址区间
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
        vmPage = LOS_VmPageGet(paddr);
        if (vmPage == NULL) {
            LOS_RegionFree(space, region);
            VM_ERR("Page is NULL");
            return LOS_ERRNO_VM_NOT_VALID;
        }
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

//对外接口|申请内核堆空间内存
VOID *LOS_VMalloc(size_t size)
{
    LosVmSpace *space = &g_vMallocSpace;//从内核动态空间申请
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
    sizeCount = size >> PAGE_SHIFT;//按页申请所以需右移12位

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
///对外接口|释放内核堆空间内存
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

LosMux *OsGVmSpaceMuxGet(VOID)
{
	    return &g_vmSpaceListMux;
}
STATIC INLINE BOOL OsMemLargeAlloc(UINT32 size)//是不是分配浪费大于1K的内存
{
    if (g_kHeapInited == FALSE) {
        return FALSE;
    }

    if (size < KMALLOC_LARGE_SIZE) {
        return FALSE;
    }

    return TRUE;
}
#else
PADDR_T LOS_PaddrQuery(VOID *vaddr)
{
    if (!LOS_IsKernelAddress((VADDR_T)vaddr)) {
        return 0;
    }

    return (PADDR_T)VMM_TO_DMA_ADDR((VADDR_T)vaddr);
}
#endif
///内核空间内存分配,申请小于16KiB的内存则通过堆内存池获取，否则申请多个连续物理页
VOID *LOS_KernelMalloc(UINT32 size)
{
    VOID *ptr = NULL;
	//从本函数可知,内核空间的分配有两种方式
#ifdef LOSCFG_KERNEL_VM
    if (OsMemLargeAlloc(size)) {//是不是分配浪费小于1K的内存
        ptr = LOS_PhysPagesAllocContiguous(ROUNDUP(size, PAGE_SIZE) >> PAGE_SHIFT);//分配连续的物理内存页
    } else
#endif
    {
        ptr = LOS_MemAlloc(OS_SYS_MEM_ADDR, size);//从内存池分配
    }

    return ptr;
}
/// 申请具有对齐属性的内存，申请规则：申请小于16KiB的内存则通过堆内存池获取，否则申请多个连续物理页
VOID *LOS_KernelMallocAlign(UINT32 size, UINT32 boundary)
{
    VOID *ptr = NULL;

#ifdef LOSCFG_KERNEL_VM
    if (OsMemLargeAlloc(size) && IS_ALIGNED(PAGE_SIZE, boundary)) {
        ptr = LOS_PhysPagesAllocContiguous(ROUNDUP(size, PAGE_SIZE) >> PAGE_SHIFT);
    } else
#endif
    {
        ptr = LOS_MemAllocAlign(OS_SYS_MEM_ADDR, size, boundary);
    }

    return ptr;
}
/// 重新分配内核内存空间
VOID *LOS_KernelRealloc(VOID *ptr, UINT32 size)
{
    VOID *tmpPtr = NULL;

#ifdef LOSCFG_KERNEL_VM
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
#else
    tmpPtr = LOS_MemRealloc(OS_SYS_MEM_ADDR, ptr, size);
#endif

    return tmpPtr;
}

VOID LOS_KernelFree(VOID *ptr)
{
#ifdef LOSCFG_KERNEL_VM
    UINT32 ret;
    if (OsMemIsHeapNode(ptr) == FALSE) {
        ret = OsMemLargeNodeFree(ptr);
        if (ret != LOS_OK) {
            VM_ERR("KernelFree %p failed", ptr);
            return;
        }
    } else
#endif
    {
        (VOID)LOS_MemFree(OS_SYS_MEM_ADDR, ptr);
    }
}


