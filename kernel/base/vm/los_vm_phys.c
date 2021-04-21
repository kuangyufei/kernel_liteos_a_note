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

#include "los_vm_phys.h"
#include "los_vm_boot.h"
#include "los_vm_common.h"
#include "los_vm_map.h"
#include "los_vm_dump.h"
#include "los_process_pri.h"


#ifdef LOSCFG_KERNEL_VM
/******************************************************************************
鸿蒙物理内存采用段页式管理
 
******************************************************************************/
#define ONE_PAGE    1

/* Physical memory area array */
STATIC struct VmPhysArea g_physArea[] = {//这里只有一个区域,即只生成一个段
    {
        .start = SYS_MEM_BASE, //整个物理内存基地址,#define SYS_MEM_BASE            DDR_MEM_ADDR ,  0x80000000
        .size = SYS_MEM_SIZE_DEFAULT,//整个物理内存总大小 0x07f00000
    },
};

struct VmPhysSeg g_vmPhysSeg[VM_PHYS_SEG_MAX];//最大32段
INT32 g_vmPhysSegNum = 0;	//段数
//获取段数组,全局变量,变量放在 .bbs 区
LosVmPhysSeg *OsGVmPhysSegGet()
{
    return g_vmPhysSeg;
}
//初始化Lru置换链表
STATIC VOID OsVmPhysLruInit(struct VmPhysSeg *seg)
{
    INT32 i;
    UINT32 intSave;
    LOS_SpinInit(&seg->lruLock);//初始化自旋锁,自旋锁用于CPU多核同步

    LOS_SpinLockSave(&seg->lruLock, &intSave);
    for (i = 0; i < VM_NR_LRU_LISTS; i++) { //五个双循环链表
        seg->lruSize[i] = 0;			//记录链表节点数
        LOS_ListInit(&seg->lruList[i]);	//初始化LRU链表
    }
    LOS_SpinUnlockRestore(&seg->lruLock, intSave);
}
//创建物理段,由区划分转成段管理
STATIC INT32 OsVmPhysSegCreate(paddr_t start, size_t size)
{
    struct VmPhysSeg *seg = NULL;

    if (g_vmPhysSegNum >= VM_PHYS_SEG_MAX) {
        return -1;
    }

    seg = &g_vmPhysSeg[g_vmPhysSegNum++];//拿到一段数据
    for (; (seg > g_vmPhysSeg) && ((seg - 1)->start > (start + size)); seg--) {
        *seg = *(seg - 1);
    }
    seg->start = start;
    seg->size = size;

    return 0;
}
//添加物理段
VOID OsVmPhysSegAdd(VOID)
{
    INT32 i, ret;

    LOS_ASSERT(g_vmPhysSegNum < VM_PHYS_SEG_MAX);
	
    for (i = 0; i < (sizeof(g_physArea) / sizeof(g_physArea[0])); i++) {//遍历g_physArea数组
        ret = OsVmPhysSegCreate(g_physArea[i].start, g_physArea[i].size);//由区划分转成段管理
        if (ret != 0) {
            VM_ERR("create phys seg failed");
        }
    }
}
//段区域大小调整
VOID OsVmPhysAreaSizeAdjust(size_t size)
{
    /*
     * The first physics memory segment is used for kernel image and kernel heap,
     * so just need to adjust the first one here.
     */
    g_physArea[0].start += size;
    g_physArea[0].size -= size;
}

//获得物理内存的总页数
UINT32 OsVmPhysPageNumGet(VOID)
{
    UINT32 nPages = 0;
    INT32 i;

    for (i = 0; i < (sizeof(g_physArea) / sizeof(g_physArea[0])); i++) {
        nPages += g_physArea[i].size >> PAGE_SHIFT;//右移12位，相当于除以4K, 计算出总页数
    }

    return nPages;//返回所有物理内存总页数
}
//初始化空闲链表,分配物理页框使用伙伴算法
STATIC INLINE VOID OsVmPhysFreeListInit(struct VmPhysSeg *seg)
{
    int i;
    UINT32 intSave;
    struct VmFreeList *list = NULL;

    LOS_SpinInit(&seg->freeListLock);//初始化用于分配的自旋锁

    LOS_SpinLockSave(&seg->freeListLock, &intSave);
    for (i = 0; i < VM_LIST_ORDER_MAX; i++) {//遍历伙伴算法空闲块组链表
        list = &seg->freeList[i];	//一个个来
        LOS_ListInit(&list->node);	//LosVmPage.node将挂到list->node上
        list->listCnt = 0;			//链表上的数量默认0
    }
    LOS_SpinUnlockRestore(&seg->freeListLock, intSave);
}
//物理段初始化
VOID OsVmPhysInit(VOID)
{
    struct VmPhysSeg *seg = NULL;
    UINT32 nPages = 0;
    int i;

    for (i = 0; i < g_vmPhysSegNum; i++) {
        seg = &g_vmPhysSeg[i];
        seg->pageBase = &g_vmPageArray[nPages];//记录本段首页物理页框地址
        nPages += seg->size >> PAGE_SHIFT;//偏移12位,按4K一页,算出本段总页数
        OsVmPhysFreeListInit(seg);	//初始化空闲链表,分配页框使用伙伴算法
        OsVmPhysLruInit(seg);		//初始化LRU置换链表
    }
}
//将页框挂入空闲链表,分配物理页框从空闲链表里拿
STATIC VOID OsVmPhysFreeListAdd(LosVmPage *page, UINT8 order)
{
    struct VmPhysSeg *seg = NULL;
    struct VmFreeList *list = NULL;

    if (page->segID >= VM_PHYS_SEG_MAX) {
        LOS_Panic("The page segment id(%d) is invalid\n", page->segID);
    }

    page->order = order;// page记录伙伴算法的组序号
    seg = &g_vmPhysSeg[page->segID];//先找到所属段

    list = &seg->freeList[order];//找到对应List
    LOS_ListTailInsert(&list->node, &page->node);//将page节点挂入链表
    list->listCnt++;//链表节点总数++
}
//同上
STATIC VOID OsVmPhysFreeListAddUnsafe(LosVmPage *page, UINT8 order)
{
    struct VmPhysSeg *seg = NULL;
    struct VmFreeList *list = NULL;

    if (page->segID >= VM_PHYS_SEG_MAX) {
        LOS_Panic("The page segment id(%d) is invalid\n", page->segID);
    }

    page->order = order;
    seg = &g_vmPhysSeg[page->segID];

    list = &seg->freeList[order];
    LOS_ListTailInsert(&list->node, &page->node);
    list->listCnt++;
}
//将物理页框从空闲链表上摘除,见于物理页框被分配的情况
STATIC VOID OsVmPhysFreeListDelUnsafe(LosVmPage *page)
{
    struct VmPhysSeg *seg = NULL;
    struct VmFreeList *list = NULL;

    if ((page->segID >= VM_PHYS_SEG_MAX) || (page->order >= VM_LIST_ORDER_MAX)) {//等于VM_LIST_ORDER_MAX也不行,说明伙伴算法最大支持 2^8的分配
        LOS_Panic("The page segment id(%u) or order(%u) is invalid\n", page->segID, page->order);
    }

    seg = &g_vmPhysSeg[page->segID];	//找到物理页框对应的段
    list = &seg->freeList[page->order];	//根据伙伴算法组序号找到空闲链表
    list->listCnt--;					//链表节点总数减一
    LOS_ListDelete(&page->node);		//将自己从链表上摘除
    page->order = VM_LIST_ORDER_MAX;	//告诉系统物理页框已不在空闲链表上, 用于OsVmPhysPagesSpiltUnsafe的断言
}
//同上
STATIC VOID OsVmPhysFreeListDel(LosVmPage *page)
{
    struct VmPhysSeg *seg = NULL;
    struct VmFreeList *list = NULL;

    if ((page->segID >= VM_PHYS_SEG_MAX) || (page->order >= VM_LIST_ORDER_MAX)) {
        LOS_Panic("The page segment id(%u) or order(%u) is invalid\n", page->segID, page->order);
    }

    seg = &g_vmPhysSeg[page->segID];
    list = &seg->freeList[page->order];
    list->listCnt--;
    LOS_ListDelete(&page->node);
    page->order = VM_LIST_ORDER_MAX;
}

/******************************************************************************
 本函数很像卖猪肉的,拿一大块肉剁,先把多余的放回到小块肉堆里去.
 oldOrder:原本要买 2^2肉
 newOrder:却找到个 2^8肉块
******************************************************************************/
STATIC VOID OsVmPhysPagesSpiltUnsafe(LosVmPage *page, UINT8 oldOrder, UINT8 newOrder)
{
    UINT32 order;
    LosVmPage *buddyPage = NULL;

    for (order = newOrder; order > oldOrder;) {//把肉剁碎的过程,把多余的肉块切成2^7,2^6...标准块,
        order--;//越切越小,逐一挂到对应的空闲链表上
        buddyPage = &page[VM_ORDER_TO_PAGES(order)];//@note_good 先把多余的肉割出来,这句代码很赞!因为LosVmPage本身是在一个大数组上,page[nPages]可直接定位
        LOS_ASSERT(buddyPage->order == VM_LIST_ORDER_MAX);//没挂到伙伴算法对应组块空闲链表上的物理页框的order必须是VM_LIST_ORDER_MAX
        OsVmPhysFreeListAddUnsafe(buddyPage, order);//将劈开的节点挂到对应序号的链表上,buddyPage->order = order
    }
}
//通过物理地址获取所属参数段的物理页框
LosVmPage *OsVmPhysToPage(paddr_t pa, UINT8 segID)
{
    struct VmPhysSeg *seg = NULL;
    paddr_t offset;

    if (segID >= VM_PHYS_SEG_MAX) {
        LOS_Panic("The page segment id(%d) is invalid\n", segID);
    }
    seg = &g_vmPhysSeg[segID];
    if ((pa < seg->start) || (pa >= (seg->start + seg->size))) {
        return NULL;
    }

    offset = pa - seg->start;//得到物理地址的偏移量
    return (seg->pageBase + (offset >> PAGE_SHIFT));//得到对应的物理页框
}

/******************************************************************************
 通过page获取内核空间的虚拟地址 参考OsArchMmuInit
 #define SYS_MEM_BASE            DDR_MEM_ADDR /* physical memory base 物理地址的起始地址 * /
 本函数非常重要，通过一个物理地址找到内核虚拟地址
 内核静态映射:提升虚实转化效率,段映射减少页表项
******************************************************************************/
VOID *OsVmPageToVaddr(LosVmPage *page)//
{
    VADDR_T vaddr;
    vaddr = KERNEL_ASPACE_BASE + page->physAddr - SYS_MEM_BASE;//page->physAddr - SYS_MEM_BASE 得到物理地址的偏移量
	//因在整个虚拟内存中内核空间和用户空间是通过地址隔离的，如此很巧妙的就把该物理页映射到了内核空间
	//内核空间的vmPage是不会被置换的，因为是常驻内存，内核空间初始化mmu时就映射好了L1表
    return (VOID *)(UINTPTR)vaddr;
}
//通过虚拟地址找映射的物理页框
LosVmPage *OsVmVaddrToPage(VOID *ptr)
{
    struct VmPhysSeg *seg = NULL;
    PADDR_T pa = LOS_PaddrQuery(ptr);//通过虚拟地址查询物理地址
    UINT32 segID;

    for (segID = 0; segID < g_vmPhysSegNum; segID++) {
        seg = &g_vmPhysSeg[segID];
        if ((pa >= seg->start) && (pa < (seg->start + seg->size))) {
            return seg->pageBase + ((pa - seg->start) >> PAGE_SHIFT);//段基地址+页偏移索引 得到虚拟地址经映射所在物理页框
        }
    }

    return NULL;
}

STATIC INLINE VOID OsVmRecycleExtraPages(LosVmPage *page, size_t startPage, size_t endPage)
{
    if (startPage >= endPage) {
        return;
    }

    OsVmPhysPagesFreeContiguous(page, endPage - startPage);
}

STATIC LosVmPage *OsVmPhysLargeAlloc(struct VmPhysSeg *seg, size_t nPages)
{
    struct VmFreeList *list = NULL;
    LosVmPage *page = NULL;
    LosVmPage *tmp = NULL;
    PADDR_T paStart;
    PADDR_T paEnd;
    size_t size = nPages << PAGE_SHIFT;

    list = &seg->freeList[VM_LIST_ORDER_MAX - 1];
    LOS_DL_LIST_FOR_EACH_ENTRY(page, &list->node, LosVmPage, node) {
        paStart = page->physAddr;
        paEnd = paStart + size;
        if (paEnd > (seg->start + seg->size)) {
            continue;
        }

        for (;;) {
            paStart += PAGE_SIZE << (VM_LIST_ORDER_MAX - 1);
            if ((paStart >= paEnd) || (paStart < seg->start) ||
                (paStart >= (seg->start + seg->size))) {
                break;
            }
            tmp = &seg->pageBase[(paStart - seg->start) >> PAGE_SHIFT];
            if (tmp->order != (VM_LIST_ORDER_MAX - 1)) {
                break;
            }
        }
        if (paStart >= paEnd) {
            return page;
        }
    }

    return NULL;
}

STATIC LosVmPage *OsVmPhysPagesAlloc(struct VmPhysSeg *seg, size_t nPages)
{
    struct VmFreeList *list = NULL;
    LosVmPage *page = NULL;
    LosVmPage *tmp = NULL;
    UINT32 order;
    UINT32 newOrder;

    order = OsVmPagesToOrder(nPages);
    if (order < VM_LIST_ORDER_MAX) {
        for (newOrder = order; newOrder < VM_LIST_ORDER_MAX; newOrder++) {
            list = &seg->freeList[newOrder];
            if (LOS_ListEmpty(&list->node)) {
                continue;
            }
            page = LOS_DL_LIST_ENTRY(LOS_DL_LIST_FIRST(&list->node), LosVmPage, node);
            goto DONE;
        }
    } else {
        newOrder = VM_LIST_ORDER_MAX - 1;
        page = OsVmPhysLargeAlloc(seg, nPages);
        if (page != NULL) {
            goto DONE;
        }
    }
    return NULL;
DONE:

    for (tmp = page; tmp < &page[nPages]; tmp = &tmp[1 << newOrder]) {
        OsVmPhysFreeListDelUnsafe(tmp);
    }
    OsVmPhysPagesSpiltUnsafe(page, order, newOrder);
    OsVmRecycleExtraPages(&page[nPages], nPages, ROUNDUP(nPages, (1 << min(order, newOrder))));

    return page;
}
//释放物理页框,所谓释放物理页就是把页挂到空闲链表中
VOID OsVmPhysPagesFree(LosVmPage *page, UINT8 order)
{
    paddr_t pa;
    LosVmPage *buddyPage = NULL;

    if ((page == NULL) || (order >= VM_LIST_ORDER_MAX)) {
        return;
    }

    if (order < VM_LIST_ORDER_MAX - 1) {//order[0,7]
        pa = VM_PAGE_TO_PHYS(page);//获取物理地址
        do {//按位异或
            pa ^= VM_ORDER_TO_PHYS(order);//@note_good 注意这里是高位和低位的 ^= ,也就是说跳到order块组物理地址处,此处处理甚妙!
            buddyPage = OsVmPhysToPage(pa, page->segID);//通过物理地址拿到页框
            if ((buddyPage == NULL) || (buddyPage->order != order)) {//页框所在组块必须要对应
                break;
            }
            OsVmPhysFreeListDel(buddyPage);//注意buddypage是连续的物理页框 例如order=2时,2^2=4页就是一个块组 |_|_|_|_| 
            order++;
            pa &= ~(VM_ORDER_TO_PHYS(order) - 1);
            page = OsVmPhysToPage(pa, page->segID);
        } while (order < VM_LIST_ORDER_MAX - 1);
    }

    OsVmPhysFreeListAdd(page, order);//伙伴算法 空闲节点增加
}
//连续的释放物理页框, 如果8页连在一块是一起释放的
VOID OsVmPhysPagesFreeContiguous(LosVmPage *page, size_t nPages)
{
    paddr_t pa;
    UINT32 order;
    size_t n;

    while (TRUE) {//死循环
        pa = VM_PAGE_TO_PHYS(page);//获取页面物理地址
        order = VM_PHYS_TO_ORDER(pa);//通过物理地址找到伙伴算法的级别
        n = VM_ORDER_TO_PAGES(order);//通过级别找到物理页块 (1<<order),意思是如果order=3，就可以释放8个页块
        if (n > nPages) {//nPages只剩下小于2^order时，退出循环
            break;
        }
        OsVmPhysPagesFree(page, order);//释放伙伴算法对应块组
        nPages -= n;//总页数减少
        page += n;//释放的页数增多
    }
	//举例剩下 7个页框时，依次用 2^2 2^1 2^0 方式释放
    while (nPages > 0) {
        order = LOS_HighBitGet(nPages);//从高到低块组释放
        n = VM_ORDER_TO_PAGES(order);//2^order次方
        OsVmPhysPagesFree(page, order);//释放块组
        nPages -= n;
        page += n;//相当于page[n]
    }
}

/******************************************************************************
 获取一定数量的页框 LosVmPage实体是放在全局大数组中的,
 LosVmPage->nPages 标记了分配页数
******************************************************************************/
STATIC LosVmPage *OsVmPhysPagesGet(size_t nPages)
{
    UINT32 intSave;
    struct VmPhysSeg *seg = NULL;
    LosVmPage *page = NULL;
    UINT32 segID;

    for (segID = 0; segID < g_vmPhysSegNum; segID++) {
        seg = &g_vmPhysSeg[segID];
        LOS_SpinLockSave(&seg->freeListLock, &intSave);
        page = OsVmPhysPagesAlloc(seg, nPages);//分配指定页数的物理页,nPages需小于伙伴算法一次能分配的最大页数
        if (page != NULL) {//分配成功
            /*  */
            LOS_AtomicSet(&page->refCounts, 0);//设置引用次数为0
            page->nPages = nPages;//页数
            LOS_SpinUnlockRestore(&seg->freeListLock, intSave);
            return page;
        }
        LOS_SpinUnlockRestore(&seg->freeListLock, intSave);
    }
    return NULL;
}
//分配连续的物理页
VOID *LOS_PhysPagesAllocContiguous(size_t nPages)
{
    LosVmPage *page = NULL;

    if (nPages == 0) {
        return NULL;
    }
	//鸿蒙 nPages 不能大于 2^8 次方,即256个页,1M内存,仅限于内核态,用户态不限制分配大小.
    page = OsVmPhysPagesGet(nPages);//通过伙伴算法获取物理上连续的页
    if (page == NULL) {
        return NULL;
    }

    return OsVmPageToVaddr(page);//通过物理页找虚拟地址
}
//释放连续的物理页
VOID LOS_PhysPagesFreeContiguous(VOID *ptr, size_t nPages)
{
    UINT32 intSave;
    struct VmPhysSeg *seg = NULL;
    LosVmPage *page = NULL;

    if (ptr == NULL) {
        return;
    }

    page = OsVmVaddrToPage(ptr);//通过虚拟地址找到页框
    if (page == NULL) {
        VM_ERR("vm page of ptr(%#x) is null", ptr);
        return;
    }
    page->nPages = 0;//被分配的页数置为0,表示不被分配

    seg = &g_vmPhysSeg[page->segID];
    LOS_SpinLockSave(&seg->freeListLock, &intSave);

    OsVmPhysPagesFreeContiguous(page, nPages);//具体释放实现

    LOS_SpinUnlockRestore(&seg->freeListLock, intSave);
}
/******************************************************************************
 通过物理地址获取内核虚拟地址,内核静态映射,减少虚实地址的转换
******************************************************************************/
VADDR_T *LOS_PaddrToKVaddr(PADDR_T paddr)
{
    struct VmPhysSeg *seg = NULL;
    UINT32 segID;

    for (segID = 0; segID < g_vmPhysSegNum; segID++) {
        seg = &g_vmPhysSeg[segID];
        if ((paddr >= seg->start) && (paddr < (seg->start + seg->size))) {
            return (VADDR_T *)(UINTPTR)(paddr - SYS_MEM_BASE + KERNEL_ASPACE_BASE);
        }
    }
	//内核
    return (VADDR_T *)(UINTPTR)(paddr - SYS_MEM_BASE + KERNEL_ASPACE_BASE);//
}
//释放物理页框
VOID LOS_PhysPageFree(LosVmPage *page)
{
    UINT32 intSave;
    struct VmPhysSeg *seg = NULL;

    if (page == NULL) {
        return;
    }

    if (LOS_AtomicDecRet(&page->refCounts) <= 0) {
        seg = &g_vmPhysSeg[page->segID];
        LOS_SpinLockSave(&seg->freeListLock, &intSave);

        OsVmPhysPagesFreeContiguous(page, ONE_PAGE);
        LOS_AtomicSet(&page->refCounts, 0);

        LOS_SpinUnlockRestore(&seg->freeListLock, intSave);
    }
}
//供外部调用
LosVmPage *LOS_PhysPageAlloc(VOID)
{
    return OsVmPhysPagesGet(ONE_PAGE);//分配一页物理页
}
/******************************************************************************
 分配nPages页个物理页框,并将页框挂入list
 返回已分配的页面大小,不负责一定能分配到nPages的页框
******************************************************************************/
size_t LOS_PhysPagesAlloc(size_t nPages, LOS_DL_LIST *list)
{
    LosVmPage *page = NULL;
    size_t count = 0;

    if ((list == NULL) || (nPages == 0)) {
        return 0;
    }

    while (nPages--) {
        page = OsVmPhysPagesGet(ONE_PAGE);//一页一页分配，由伙伴算法分配
        if (page == NULL) {
            break;
        }
        LOS_ListTailInsert(list, &page->node);//从参数链表list尾部挂入新页面结点
        count++;
    }

    return count;
}
//拷贝共享页面
VOID OsPhysSharePageCopy(PADDR_T oldPaddr, PADDR_T *newPaddr, LosVmPage *newPage)
{
    UINT32 intSave;
    LosVmPage *oldPage = NULL;
    VOID *newMem = NULL;
    VOID *oldMem = NULL;
    LosVmPhysSeg *seg = NULL;

    if ((newPage == NULL) || (newPaddr == NULL)) {
        VM_ERR("new Page invalid");
        return;
    }

    oldPage = LOS_VmPageGet(oldPaddr);//由物理地址得到页框
    if (oldPage == NULL) {
        VM_ERR("invalid paddr %p", oldPaddr);
        return;
    }

    seg = &g_vmPhysSeg[oldPage->segID];//拿到物理段
    LOS_SpinLockSave(&seg->freeListLock, &intSave);
    if (LOS_AtomicRead(&oldPage->refCounts) == 1) {//页面引用次数仅一次,说明只有一个进程在操作
        *newPaddr = oldPaddr;//新老指向同一块物理地址
    } else {//是个共享内存
        newMem = LOS_PaddrToKVaddr(*newPaddr);	//新页虚拟地址
        oldMem = LOS_PaddrToKVaddr(oldPaddr);	//老页虚拟地址
        if ((newMem == NULL) || (oldMem == NULL)) {
            LOS_SpinUnlockRestore(&seg->freeListLock, intSave);
            return;
        }//请记住,在保护模式下,物理地址只能用于计算,操作(包括拷贝)需要虚拟地址! 
        if (memcpy_s(newMem, PAGE_SIZE, oldMem, PAGE_SIZE) != EOK) {//老页内容复制给新页,需操作虚拟地址,拷贝一页数据
            VM_ERR("memcpy_s failed");
        }

        LOS_AtomicInc(&newPage->refCounts);//新页引用次数以原子方式自动减量 
        LOS_AtomicDec(&oldPage->refCounts);//老页引用次数以原子方式自动减量
    }
    LOS_SpinUnlockRestore(&seg->freeListLock, intSave);
    return;
}
//获取物理页框所在段
struct VmPhysSeg *OsVmPhysSegGet(LosVmPage *page)
{
    if ((page == NULL) || (page->segID >= VM_PHYS_SEG_MAX)) {
        return NULL;
    }

    return (OsGVmPhysSegGet() + page->segID);//等用于OsGVmPhysSegGet()[page->segID]
}
//获取参数nPages对应的块组,例如 7 -> 2^3 返回 3
UINT32 OsVmPagesToOrder(size_t nPages)
{
    UINT32 order;

    for (order = 0; VM_ORDER_TO_PAGES(order) < nPages; order++);

    return order;
}
//释放双链表中的所有节点内存,本质是回归到伙伴orderlist中
size_t LOS_PhysPagesFree(LOS_DL_LIST *list)
{
    UINT32 intSave;
    LosVmPage *page = NULL;
    LosVmPage *nPage = NULL;
    LosVmPhysSeg *seg = NULL;
    size_t count = 0;

    if (list == NULL) {
        return 0;
    }

    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(page, nPage, list, LosVmPage, node) {//宏循环
        LOS_ListDelete(&page->node);//先把自己摘出去
        if (LOS_AtomicDecRet(&page->refCounts) <= 0) {//无引用
            seg = &g_vmPhysSeg[page->segID];//获取物理段
            LOS_SpinLockSave(&seg->freeListLock, &intSave);//锁住freeList
            OsVmPhysPagesFreeContiguous(page, ONE_PAGE);//连续释放,注意这里的ONE_PAGE其实有误导,让人以为是释放4K,其实是指连续的物理页框,如果3页连在一块是一起释放的.
            LOS_AtomicSet(&page->refCounts, 0);//引用重置为0
            LOS_SpinUnlockRestore(&seg->freeListLock, intSave);//恢复锁
        }
        count++;//继续取下一个node
    }

    return count;
}
#else
VADDR_T *LOS_PaddrToKVaddr(PADDR_T paddr)
{
    if ((paddr < DDR_MEM_ADDR) || (paddr >= (DDR_MEM_ADDR + DDR_MEM_SIZE))) {
        return NULL;
    }

    return (VADDR_T *)DMA_TO_VMM_ADDR(paddr);
}
#endif

