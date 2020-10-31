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

#include "los_vm_phys.h"
#include "los_vm_boot.h"
#include "los_vm_common.h"
#include "los_vm_map.h"
#include "los_vm_dump.h"
#include "los_process_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define ONE_PAGE    1

/* Physical memory area array */
STATIC struct VmPhysArea g_physArea[] = {//这里只整了一个区域,即只生成一个段,鸿蒙物理内存采用段页式管理
    {
        .start = SYS_MEM_BASE, //整个物理内存基地址
        .size = SYS_MEM_SIZE_DEFAULT,//整个物理内存总大小
    },
};

struct VmPhysSeg g_vmPhysSeg[VM_PHYS_SEG_MAX];//最大32段
INT32 g_vmPhysSegNum = 0;	//段数
//获取段数组,全局变量,变量放在 .bbs 区
LosVmPhysSeg *OsGVmPhysSegGet()
{
    return g_vmPhysSeg;
}
//初始化Lru置换 LRU list 在 VmPhysSeg中管理
STATIC VOID OsVmPhysLruInit(struct VmPhysSeg *seg)
{
    INT32 i;
    UINT32 intSave;
    LOS_SpinInit(&seg->lruLock);//自旋锁,自旋锁用户CPU多核同步

    LOS_SpinLockSave(&seg->lruLock, &intSave);
    for (i = 0; i < VM_NR_LRU_LISTS; i++) { //五个双循环链表
        seg->lruSize[i] = 0;	//记录链表节点数
        LOS_ListInit(&seg->lruList[i]);//初始化LRU链表
    }
    LOS_SpinUnlockRestore(&seg->lruLock, intSave);
}
//创建物理段
STATIC INT32 OsVmPhysSegCreate(paddr_t start, size_t size)
{
    struct VmPhysSeg *seg = NULL;

    if (g_vmPhysSegNum >= VM_PHYS_SEG_MAX) {
        return -1;
    }

    seg = &g_vmPhysSeg[g_vmPhysSegNum++];
    for (; (seg > g_vmPhysSeg) && ((seg - 1)->start > (start + size)); seg--) {
        *seg = *(seg - 1);
    }
    seg->start = start;
    seg->size = size;

    return 0;
}

VOID OsVmPhysSegAdd(VOID)
{
    INT32 i, ret;

    LOS_ASSERT(g_vmPhysSegNum <= VM_PHYS_SEG_MAX);
	
    for (i = 0; i < (sizeof(g_physArea) / sizeof(g_physArea[0])); i++) {//将g_physArea转化成段
        ret = OsVmPhysSegCreate(g_physArea[i].start, g_physArea[i].size);//一个区对应一个段
        if (ret != 0) {
            VM_ERR("create phys seg failed");
        }
    }
}
//段区域大小调整
VOID OsVmPhysAreaSizeAdjust(size_t size)
{
    INT32 i;

    for (i = 0; i < (sizeof(g_physArea) / sizeof(g_physArea[0])); i++) {
        g_physArea[i].start += size;
        g_physArea[i].size -= size;
    }
}

UINT32 OsVmPhysPageNumGet(VOID)
{
    UINT32 nPages = 0;
    INT32 i;

    for (i = 0; i < (sizeof(g_physArea) / sizeof(g_physArea[0])); i++) {
        nPages += g_physArea[i].size >> PAGE_SHIFT;//右移12位，相当于除以4K,得出总页数
    }

    return nPages;
}
//初始化空闲链表,分配页框使用伙伴算法
STATIC INLINE VOID OsVmPhysFreeListInit(struct VmPhysSeg *seg)
{
    int i;
    UINT32 intSave;
    struct VmFreeList *list = NULL;

    LOS_SpinInit(&seg->freeListLock);//自旋锁

    LOS_SpinLockSave(&seg->freeListLock, &intSave);
    for (i = 0; i < VM_LIST_ORDER_MAX; i++) {
        list = &seg->freeList[i];
        LOS_ListInit(&list->node);//初始化9个链表 2^0,2^1,2^2 分配页框 ^代表次方的意思
        list->listCnt = 0;
    }
    LOS_SpinUnlockRestore(&seg->freeListLock, intSave);
}
//段初始化
VOID OsVmPhysInit(VOID)
{
    struct VmPhysSeg *seg = NULL;
    UINT32 nPages = 0;
    int i;

    for (i = 0; i < g_vmPhysSegNum; i++) {
        seg = &g_vmPhysSeg[i];
        seg->pageBase = &g_vmPageArray[nPages];//
        nPages += seg->size >> PAGE_SHIFT;//偏移12位,因以页管理,本段总页数
        OsVmPhysFreeListInit(seg);	//初始化空闲链表,分配页框使用伙伴算法
        OsVmPhysLruInit(seg);		//初始化LRU置换链表
    }
}
//将页框挂入空闲链表,供分配
STATIC VOID OsVmPhysFreeListAdd(LosVmPage *page, UINT8 order)
{
    struct VmPhysSeg *seg = NULL;
    struct VmFreeList *list = NULL;

    if (page->segID >= VM_PHYS_SEG_MAX) {
        LOS_Panic("The page segment id(%d) is invalid\n", page->segID);
    }

    page->order = order;// page记录 块组号
    seg = &g_vmPhysSeg[page->segID];//先找到所属段

    list = &seg->freeList[order];//找到对应List
    LOS_ListTailInsert(&list->node, &page->node);//将page节点挂入链表
    list->listCnt++;//链表内的节点总数++
}

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

STATIC VOID OsVmPhysFreeListDelUnsafe(LosVmPage *page)
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

STATIC VOID OsVmPhysPagesSpiltUnsafe(LosVmPage *page, UINT8 oldOrder, UINT8 newOrder)
{
    UINT32 order;
    LosVmPage *buddyPage = NULL;

    for (order = newOrder; order > oldOrder;) {
        order--;
        buddyPage = &page[VM_ORDER_TO_PAGES(order)];
        LOS_ASSERT(buddyPage->order == VM_LIST_ORDER_MAX);
        OsVmPhysFreeListAddUnsafe(buddyPage, order);
    }
}
//通过物理地址获取所属页面
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

    offset = pa - seg->start;//得到偏移地址
    return (seg->pageBase + (offset >> PAGE_SHIFT));//得到第n页page
}
//通过page获取空间的虚拟地址 参考OsArchMmuInit
VOID *OsVmPageToVaddr(LosVmPage *page)//
{
    VADDR_T vaddr;
    vaddr = KERNEL_ASPACE_BASE + page->physAddr - SYS_MEM_BASE;//page->physAddr - SYS_MEM_BASE 得到物理地址上的偏移量
	//因在整个虚拟内存中内核空间和用户空间是通过地址隔离的，如此很巧妙的就把该物理页映射到了内核空间
	//内核空间的vmPage是不会被置换的，所以是常驻内存，内核空间初始化mmu时就映射好了L1表
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
//从段中分配指定页框数
LosVmPage *OsVmPhysPagesAlloc(struct VmPhysSeg *seg, size_t nPages)
{
    struct VmFreeList *list = NULL;
    LosVmPage *page = NULL;
    UINT32 order;
    UINT32 newOrder;

    if ((seg == NULL) || (nPages == 0)) {
        return NULL;
    }

    order = OsVmPagesToOrder(nPages);//根据页数计算出用哪个块组
    if (order < VM_LIST_ORDER_MAX) {//order不能大于9 即:256*4K = 1M 可理解为向内核堆申请内存一次不能超过1M
        for (newOrder = order; newOrder < VM_LIST_ORDER_MAX; newOrder++) {//没有就找更大块
            list = &seg->freeList[newOrder];//从最合适的块处开始找
            if (LOS_ListEmpty(&list->node)) {//没找到
                continue;//继续找更大块的
            }
            page = LOS_DL_LIST_ENTRY(LOS_DL_LIST_FIRST(&list->node), LosVmPage, node);//找到
            goto DONE;
        }
    }
    return NULL;
DONE:
    OsVmPhysFreeListDelUnsafe(page);
    OsVmPhysPagesSpiltUnsafe(page, order, newOrder);
    return page;
}
//释放物理页,所谓释放物理页就是把页挂到空闲链表中
VOID OsVmPhysPagesFree(LosVmPage *page, UINT8 order)
{
    paddr_t pa;
    LosVmPage *buddyPage = NULL;

    if ((page == NULL) || (order >= VM_LIST_ORDER_MAX)) {
        return;
    }

    if (order < VM_LIST_ORDER_MAX - 1) {//order[0,7]
        pa = VM_PAGE_TO_PHYS(page);//获取物理地址
        do {
            pa ^= VM_ORDER_TO_PHYS(order);//注意这里是高位和低位的 ^= ,也就是说跳到 order块组 物理地址处,此处处理甚妙!
            buddyPage = OsVmPhysToPage(pa, page->segID);//如此就能拿到以2^order次方跳的buddyPage
            if ((buddyPage == NULL) || (buddyPage->order != order)) {
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
//连续的释放物理页框, 如果8页连在一块是一起释放的,取决于使用了伙伴算法的order
VOID OsVmPhysPagesFreeContiguous(LosVmPage *page, size_t nPages)
{
    paddr_t pa;
    UINT32 order;
    size_t count;
    size_t n;

    while (TRUE) {
        pa = VM_PAGE_TO_PHYS(page);//获取页面物理地址
        order = VM_PHYS_TO_ORDER(pa);//通过物理地址找到伙伴算法的级别
        n = VM_ORDER_TO_PAGES(order);//通过级别找到物理页块 (1<<order),意思是如果order=3，就是有8个页块
        if (n > nPages) {//只剩小于 2的order时，退出循环
            break;
        }
        OsVmPhysPagesFree(page, order);//释放伙伴算法对应块组
        nPages -= n;//总页数减少
        page += n;//释放的页数增多
    }
	//举例剩下 7个页框时，依次用 2^2 2^1 2^0 方式释放
    for (count = 0; count < nPages; count += n) {
        order = LOS_HighBitGet(nPages);//从高到低块组释放
        n = VM_ORDER_TO_PAGES(order);//2^order次方
        OsVmPhysPagesFree(page, order);//释放块组
        page += n;
    }
}
//获取一定数量的页框
STATIC LosVmPage *OsVmPhysPagesGet(size_t nPages)
{
    UINT32 intSave;
    struct VmPhysSeg *seg = NULL;
    LosVmPage *page = NULL;
    UINT32 segID;

    if (nPages == 0) {
        return NULL;
    }

    for (segID = 0; segID < g_vmPhysSegNum; segID++) {
        seg = &g_vmPhysSeg[segID];
        LOS_SpinLockSave(&seg->freeListLock, &intSave);
        page = OsVmPhysPagesAlloc(seg, nPages);//分配指定页数的物理页, 可分析出 nPages 不能大于 256个
        if (page != NULL) {
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

    page = OsVmVaddrToPage(ptr);//通过虚拟地址找page
    if (page == NULL) {
        VM_ERR("vm page of ptr(%#x) is null", ptr);
        return;
    }
    page->nPages = 0;

    seg = &g_vmPhysSeg[page->segID];
    LOS_SpinLockSave(&seg->freeListLock, &intSave);

    OsVmPhysPagesFreeContiguous(page, nPages);//Os层具体释放实现

    LOS_SpinUnlockRestore(&seg->freeListLock, intSave);
}
//通过物理地址获取内核虚拟地址
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

    return (VADDR_T *)(UINTPTR)(paddr - SYS_MEM_BASE + KERNEL_ASPACE_BASE);
}

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

LosVmPage *LOS_PhysPageAlloc(VOID)
{
    return OsVmPhysPagesGet(ONE_PAGE);//分配一页物理页
}

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
        LOS_ListTailInsert(list, &page->node);//从参数链表list尾部插入新页面结点
        count++;
    }

    return count;
}
//共享页面拷贝实现
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

    oldPage = LOS_VmPageGet(oldPaddr);//由物理地址得到 page信息页
    if (oldPage == NULL) {
        VM_ERR("invalid paddr %p", oldPaddr);
        return;
    }

    seg = &g_vmPhysSeg[oldPage->segID];//拿到段
    LOS_SpinLockSave(&seg->freeListLock, &intSave);
    if (LOS_AtomicRead(&oldPage->refCounts) == 1) {//页面引用次数仅一次,说明还没有被其他进程所使用过
        *newPaddr = oldPaddr;
    } else {
        newMem = LOS_PaddrToKVaddr(*newPaddr);	//新页虚拟地址
        oldMem = LOS_PaddrToKVaddr(oldPaddr);	//老页虚拟地址
        if ((newMem == NULL) || (oldMem == NULL)) {
            LOS_SpinUnlockRestore(&seg->freeListLock, intSave);
            return;
        }
        if (memcpy_s(newMem, PAGE_SIZE, oldMem, PAGE_SIZE) != EOK) {//老页内容复制给新页
            VM_ERR("memcpy_s failed");
        }

        LOS_AtomicInc(&newPage->refCounts);//新页ref++
        LOS_AtomicDec(&oldPage->refCounts);//老页ref--
    }
    LOS_SpinUnlockRestore(&seg->freeListLock, intSave);
    return;
}

struct VmPhysSeg *OsVmPhysSegGet(LosVmPage *page)
{
    if ((page == NULL) || (page->segID >= VM_PHYS_SEG_MAX)) {
        return NULL;
    }

    return (OsGVmPhysSegGet() + page->segID);
}
//通过总页数 ,获取块组 ,例如需要分配 8个页,返回就是 3 ,例如 1023个 返回就是 10
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

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
