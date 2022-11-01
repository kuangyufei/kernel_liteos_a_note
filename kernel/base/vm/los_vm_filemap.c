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
 * @defgroup los_vm_filemap vm filemap definition
 * @ingroup kernel
 */

#include "los_vm_filemap.h"
#include "los_vm_page.h"
#include "los_vm_phys.h"
#include "los_vm_common.h"
#include "los_vm_fault.h"
#include "los_process_pri.h"
#include "los_vm_lock.h"
#ifdef LOSCFG_FS_VFS
#include "vnode.h"
#endif

#ifndef UNUSED
#define UNUSED(x)          (VOID)(x)
#endif

#ifdef LOSCFG_DEBUG_VERSION
static int g_totalPageCacheTry = 0;
static int g_totalPageCacheHit = 0;
#define TRACE_TRY_CACHE() do { g_totalPageCacheTry++; } while (0)
#define TRACE_HIT_CACHE() do { g_totalPageCacheHit++; } while (0)

VOID ResetPageCacheHitInfo(int *try, int *hit)
{
    *try = g_totalPageCacheTry;
    *hit = g_totalPageCacheHit;
    g_totalPageCacheHit = 0;
    g_totalPageCacheTry = 0;
}
#else
#define TRACE_TRY_CACHE()
#define TRACE_HIT_CACHE()
#endif
#ifdef LOSCFG_KERNEL_VM

/**
 * @brief 
    @verbatim
    增加文件页到页高速缓存(page cache)
    LosFilePage将一个文件切成了一页一页,因为读文件过程随机seek,所以文件页也不会是连续的,
    pgoff记录文件的位置,并确保在cache的文件数据是按顺序排列的.
    @endverbatim
 * @param page 
 * @param mapping 
 * @param pgoff 
 * @return STATIC 
 */
STATIC VOID OsPageCacheAdd(LosFilePage *page, struct page_mapping *mapping, VM_OFFSET_T pgoff)
{
    LosFilePage *fpage = NULL;

    LOS_DL_LIST_FOR_EACH_ENTRY(fpage, &mapping->page_list, LosFilePage, node) {//遍历page_list链表
        if (fpage->pgoff > pgoff) {//插入的条件,这样插入保证了按pgoff 从小到大排序
            LOS_ListTailInsert(&fpage->node, &page->node);//等于挂到fpage节点的前面了
            goto done_add;
        }
    }

    LOS_ListTailInsert(&mapping->page_list, &page->node);//将页挂到文件映射的链表上,相当于挂到了最后

done_add:
    mapping->nrpages++;	//文件在缓存中多了一个 文件页
}
///将页面加到活动文件页LRU链表上
VOID OsAddToPageacheLru(LosFilePage *page, struct page_mapping *mapping, VM_OFFSET_T pgoff)
{
    OsPageCacheAdd(page, mapping, pgoff);
    OsLruCacheAdd(page, VM_LRU_ACTIVE_FILE);
}
///从页高速缓存上删除页
VOID OsPageCacheDel(LosFilePage *fpage)
{
    /* delete from file cache list */
    LOS_ListDelete(&fpage->node);//将自己从链表上摘除
    fpage->mapping->nrpages--;//文件映射的页总数减少

    /* unmap and remove map info */
    if (OsIsPageMapped(fpage)) {//是否映射过
        OsUnmapAllLocked(fpage);
    }

    LOS_PhysPageFree(fpage->vmPage);//释放物理内存

    LOS_MemFree(m_aucSysMem0, fpage);//释放文件页结构体内存
}
/**************************************************************************************************
每个进程都有自己的地址空间, 多个进程可以访问同一个LosFilePage,每个进程使用的虚拟地址都需要单独映射
所以同一个LosFilePage会映射到多个进程空间.本函数记录页面被哪些进程映射过
在两个地方会被被空间映射
1.缺页中断 2.克隆地址空间
**************************************************************************************************/
VOID OsAddMapInfo(LosFilePage *page, LosArchMmu *archMmu, VADDR_T vaddr)
{
    LosMapInfo *info = NULL;

    info = (LosMapInfo *)LOS_MemAlloc(m_aucSysMem0, sizeof(LosMapInfo));//分配一个映射信息
    if (info == NULL) {
        VM_ERR("OsAddMapInfo alloc memory failed!");
        return;
    }
    info->page = page;		//文件页
    info->archMmu = archMmu;//进程MMU,完成虚实地址转换 
    info->vaddr = vaddr;	//虚拟地址

    LOS_ListAdd(&page->i_mmap, &info->node);//将 LosMapInfo 节点挂入链表
    page->n_maps++;//映射总数++
}
///通过虚拟地址获取文件页映射信息,archMmu每个进程都有属于自己的mmu
LosMapInfo *OsGetMapInfo(const LosFilePage *page, const LosArchMmu *archMmu, VADDR_T vaddr)
{
    LosMapInfo *info = NULL;
    const LOS_DL_LIST *immap = &page->i_mmap;//一个文件页被多个进程映射

    LOS_DL_LIST_FOR_EACH_ENTRY(info, immap, LosMapInfo, node) {//遍历每个节点
        if ((info->archMmu == archMmu) && (info->vaddr == vaddr) && (info->page == page)) {//全等时返回
            return info;
        }
    }
    return NULL;
}
///删除页高速缓存和LRU,对应 OsAddToPageacheLru
VOID OsDeletePageCacheLru(LosFilePage *page)
{
    /* delete form lru list */
    OsLruCacheDel(page);	//将页面从lru列表中删除
    /* delete from cache lits and free pmm if need */
    OsPageCacheDel(page); //从page缓存中删除
}

//解除文件页和进程的映射关系
STATIC VOID OsPageCacheUnmap(LosFilePage *fpage, LosArchMmu *archMmu, VADDR_T vaddr)
{
    UINT32 intSave;
    LosMapInfo *info = NULL;

    LOS_SpinLockSave(&fpage->physSeg->lruLock, &intSave);
    info = OsGetMapInfo(fpage, archMmu, vaddr);//获取文件页在进程的映射信息
    if (info == NULL) {
        VM_ERR("OsPageCacheUnmap get map info failed!");
    } else {
        OsUnmapPageLocked(fpage, info);//解除进程和文件页映射关系
    }
    if (!(OsIsPageMapped(fpage) && ((fpage->flags & VM_MAP_REGION_FLAG_PERM_EXECUTE) ||
        OsIsPageDirty(fpage->vmPage)))) {
        OsPageRefDecNoLock(fpage);
    }

    LOS_SpinUnlockRestore(&fpage->physSeg->lruLock, intSave);
}
///删除文件
VOID OsVmmFileRemove(LosVmMapRegion *region, LosArchMmu *archMmu, VM_OFFSET_T pgoff)
{
    UINT32 intSave;
    vaddr_t vaddr;
    paddr_t paddr = 0;
    struct Vnode *vnode = NULL;
    struct page_mapping *mapping = NULL;
    LosFilePage *fpage = NULL;
    LosFilePage *tmpPage = NULL;
    LosVmPage *mapPage = NULL;

    if (!LOS_IsRegionFileValid(region) || (region->unTypeData.rf.vnode == NULL)) {
        return;//判断是否为文件映射，是否已map
    }
    vnode = region->unTypeData.rf.vnode;
    mapping = &vnode->mapping;
    vaddr = region->range.base + ((UINT32)(pgoff - region->pgOff) << PAGE_SHIFT);//得到虚拟地址

    status_t status = LOS_ArchMmuQuery(archMmu, vaddr, &paddr, NULL);//获取物理地址
    if (status != LOS_OK) {
        return;
    }

    mapPage = LOS_VmPageGet(paddr);//获取物理页框

    /* is page is in cache list */
    LOS_SpinLockSave(&mapping->list_lock, &intSave);
    fpage = OsFindGetEntry(mapping, pgoff);//获取fpage
    /* no cache or have cache but not map(cow), free it direct */
    if ((fpage == NULL) || (fpage->vmPage != mapPage)) {//没有缓存或有缓存但没有映射（cow），直接释放它
        LOS_PhysPageFree(mapPage);//释放物理页框
        LOS_ArchMmuUnmap(archMmu, vaddr, 1);//取消虚拟地址的映射
    /* this is a page cache map! */
    } else {
        OsPageCacheUnmap(fpage, archMmu, vaddr);////取消缓存中的映射
        if (OsIsPageDirty(fpage->vmPage)) {//脏页处理
            tmpPage = OsDumpDirtyPage(fpage);//dump 脏页
        }
    }
    LOS_SpinUnlockRestore(&mapping->list_lock, intSave);

    if (tmpPage) {
        OsDoFlushDirtyPage(tmpPage);
    }
    return;
}
///标记page为脏页 进程修改了高速缓存里的数据时，该页就被内核标记为脏页
VOID OsMarkPageDirty(LosFilePage *fpage, const LosVmMapRegion *region, INT32 off, INT32 len)
{
    if (region != NULL) {
        OsSetPageDirty(fpage->vmPage);//设置为脏页
        fpage->dirtyOff = off;//脏页偏移位置
        fpage->dirtyEnd = len;//脏页结束位置
    } else {
        OsSetPageDirty(fpage->vmPage);//设置为脏页
        if ((off + len) > fpage->dirtyEnd) {
            fpage->dirtyEnd = off + len;
        }

        if (off < fpage->dirtyOff) {
            fpage->dirtyOff = off;
        }
    }
}

STATIC UINT32 GetDirtySize(LosFilePage *fpage, struct Vnode *vnode)
{
    UINT32 fileSize;
    UINT32 dirtyBegin;
    UINT32 dirtyEnd;
    struct stat buf_stat;

    if (stat(vnode->filePath, &buf_stat) != OK) {
        VM_ERR("FlushDirtyPage get file size failed. (filePath=%s)", vnode->filePath);
        return 0;
    }

    fileSize = buf_stat.st_size;
    dirtyBegin = ((UINT32)fpage->pgoff << PAGE_SHIFT);
    dirtyEnd = dirtyBegin + PAGE_SIZE;

    if (dirtyBegin >= fileSize) {
        return 0;
    }

    if (dirtyEnd >= fileSize) {
        return fileSize - dirtyBegin;
    }

    return PAGE_SIZE;
}
///冲洗脏页，回写磁盘
STATIC INT32 OsFlushDirtyPage(LosFilePage *fpage)
{
    UINT32 ret;
    size_t len;
    char *buff = NULL;
    struct Vnode *vnode = fpage->mapping->host;/* owner of this mapping */ //此映射属于哪个文件,注意<file,page_mapping>是1:1的关系.
    if (vnode == NULL) {
        VM_ERR("page cache vnode error");
        return LOS_NOK;
    }

    len = fpage->dirtyEnd - fpage->dirtyOff;//计算出脏数据长度
    len = (len == 0) ? GetDirtySize(fpage, vnode) : len;
    if (len == 0) {//没有脏数据
        OsCleanPageDirty(fpage->vmPage);//页面取消脏标签
        return LOS_OK;
    }

    buff = (char *)OsVmPageToVaddr(fpage->vmPage);

    /* actually, we did not update the fpage->dirtyOff */
    ret = vnode->vop->WritePage(vnode, (VOID *)buff, fpage->pgoff, len);
    if (ret <= 0) {
        VM_ERR("WritePage error ret %d", ret);
    } else {
        OsCleanPageDirty(fpage->vmPage);
    }
    ret = (ret <= 0) ? LOS_NOK : LOS_OK;

    return ret;
}
///备份脏页,老脏页撕掉脏页标签
LosFilePage *OsDumpDirtyPage(LosFilePage *oldFPage)
{
    LosFilePage *newFPage = NULL;

    newFPage = (LosFilePage *)LOS_MemAlloc(m_aucSysMem0, sizeof(LosFilePage));
    if (newFPage == NULL) {
        VM_ERR("Failed to allocate for temp page!");
        return NULL;
    }

    OsCleanPageDirty(oldFPage->vmPage);
    (VOID)memcpy_s(newFPage, sizeof(LosFilePage), oldFPage, sizeof(LosFilePage));//直接内存拷贝

    return newFPage;
}
///冲洗脏页数据,将脏页数据回写磁盘
VOID OsDoFlushDirtyPage(LosFilePage *fpage)
{
    if (fpage == NULL) {
        return;
    }
    (VOID)OsFlushDirtyPage(fpage);
    LOS_MemFree(m_aucSysMem0, fpage);
}

STATIC VOID OsReleaseFpage(struct page_mapping *mapping, LosFilePage *fpage)
{
    UINT32 intSave;
    UINT32 lruSave;
    SPIN_LOCK_S *lruLock = &fpage->physSeg->lruLock;
    LOS_SpinLockSave(&mapping->list_lock, &intSave);
    LOS_SpinLockSave(lruLock, &lruSave);
    OsCleanPageLocked(fpage->vmPage);
    OsDeletePageCacheLru(fpage);
    LOS_SpinUnlockRestore(lruLock, lruSave);
    LOS_SpinUnlockRestore(&mapping->list_lock, intSave);
}
///删除映射信息
VOID OsDelMapInfo(LosVmMapRegion *region, LosVmPgFault *vmf, BOOL cleanDirty)
{
    UINT32 intSave;
    LosMapInfo *info = NULL;
    LosFilePage *fpage = NULL;
    struct page_mapping *mapping = NULL;

    if (!LOS_IsRegionFileValid(region) || (region->unTypeData.rf.vnode == NULL) || (vmf == NULL)) {
        return;
    }

    mapping = &region->unTypeData.rf.vnode->mapping;
    LOS_SpinLockSave(&mapping->list_lock, &intSave);
    fpage = OsFindGetEntry(mapping, vmf->pgoff);
    if (fpage == NULL) {
        LOS_SpinUnlockRestore(&mapping->list_lock, intSave);
        return;
    }

    if (cleanDirty) {
        OsCleanPageDirty(fpage->vmPage);//恢复干净页
    }
    info = OsGetMapInfo(fpage, &region->space->archMmu, (vaddr_t)vmf->vaddr);//通过虚拟地址获取映射信息
    if (info != NULL) {
        fpage->n_maps--;
        LOS_ListDelete(&info->node);
        LOS_AtomicDec(&fpage->vmPage->refCounts);
        LOS_SpinUnlockRestore(&mapping->list_lock, intSave);
        LOS_MemFree(m_aucSysMem0, info);
        return;
    }
    LOS_SpinUnlockRestore(&mapping->list_lock, intSave);
}
/*!
文件缺页时的处理,先读入磁盘数据，再重新读页数据
被 OsDoReadFault(...),OsDoCowFault(...),OsDoSharedFault(...) 等调用
*/
INT32 OsVmmFileFault(LosVmMapRegion *region, LosVmPgFault *vmf)
{
    INT32 ret;
    VOID *kvaddr = NULL;

    UINT32 intSave;
    bool newCache = false;
    struct Vnode *vnode = NULL;
    struct page_mapping *mapping = NULL;
    LosFilePage *fpage = NULL;

    if (!LOS_IsRegionFileValid(region) || (region->unTypeData.rf.vnode == NULL) || (vmf == NULL)) {//文件是否映射到了内存
        VM_ERR("Input param is NULL");
        return LOS_NOK;
    }
    vnode = region->unTypeData.rf.vnode;
    mapping = &vnode->mapping;

    /* get or create a new cache node */
    LOS_SpinLockSave(&mapping->list_lock, &intSave);
    fpage = OsFindGetEntry(mapping, vmf->pgoff);//获取文件页
    TRACE_TRY_CACHE();
    if (fpage != NULL) {//找到了,说明该页已经在页高速缓存中
        TRACE_HIT_CACHE();
        OsPageRefIncLocked(fpage);
    } else {//真的缺页了,页高速缓存中没找到
        fpage = OsPageCacheAlloc(mapping, vmf->pgoff);//分配一个文件页，将数据初始化好，包括vmpage(物理页框)
        if (fpage == NULL) {
            LOS_SpinUnlockRestore(&mapping->list_lock, intSave);
            VM_ERR("Failed to alloc a page frame");
            return LOS_NOK;
        }
        newCache = true;//分配了新文件页
    }
    OsSetPageLocked(fpage->vmPage);//对vmpage上锁
    LOS_SpinUnlockRestore(&mapping->list_lock, intSave);
    kvaddr = OsVmPageToVaddr(fpage->vmPage);//获取该页框在内核空间的虚拟地址,因为 page cache本身就是在内核空间,

    /* read file to new page cache */
    if (newCache) {//新cache
        ret = vnode->vop->ReadPage(vnode, kvaddr, fpage->pgoff << PAGE_SHIFT);
        if (ret == 0) {
            VM_ERR("Failed to read from file!");
            OsReleaseFpage(mapping, fpage);
            return LOS_NOK;
        }
        LOS_SpinLockSave(&mapping->list_lock, &intSave);
        OsAddToPageacheLru(fpage, mapping, vmf->pgoff);//将fpage挂入pageCache 和 LruCache
        LOS_SpinUnlockRestore(&mapping->list_lock, intSave);
    }

    LOS_SpinLockSave(&mapping->list_lock, &intSave);
    /* cow fault case no need to save mapinfo */
    if (!((vmf->flags & VM_MAP_PF_FLAG_WRITE) && !(region->regionFlags & VM_MAP_REGION_FLAG_SHARED))) {
        OsAddMapInfo(fpage, &region->space->archMmu, (vaddr_t)vmf->vaddr);//添加<虚拟地址,文件页>的映射关系,如此进程以后就能通过虚拟地址操作文件页了.
        fpage->flags = region->regionFlags;
    }

    /* share page fault, mark the page dirty */
    if ((vmf->flags & VM_MAP_PF_FLAG_WRITE) && (region->regionFlags & VM_MAP_REGION_FLAG_SHARED)) {//有过写操作或者为共享线性区
        OsMarkPageDirty(fpage, region, 0, 0);//标记为脏页,要回写磁盘,内核会在适当的时候回写磁盘
    }

    vmf->pageKVaddr = kvaddr;//缺陷页记录文件页的虚拟地址
    LOS_SpinUnlockRestore(&mapping->list_lock, intSave);
    return LOS_OK;
}
///文件缓存冲洗,把所有fpage冲洗一边，把脏页洗到dirtyList中,配合OsFileCacheRemove理解 
VOID OsFileCacheFlush(struct page_mapping *mapping)
{
    UINT32 intSave;
    UINT32 lruLock;
    LOS_DL_LIST_HEAD(dirtyList);//LOS_DL_LIST list = { &(list), &(list) };
    LosFilePage *ftemp = NULL;
    LosFilePage *fpage = NULL;

    if (mapping == NULL) {
        return;
    }
    LOS_SpinLockSave(&mapping->list_lock, &intSave);
    LOS_DL_LIST_FOR_EACH_ENTRY(fpage, &mapping->page_list, LosFilePage, node) {//循环从page_list中取node给fpage
        LOS_SpinLockSave(&fpage->physSeg->lruLock, &lruLock);
        if (OsIsPageDirty(fpage->vmPage)) {//是否为脏页
            ftemp = OsDumpDirtyPage(fpage);//这里挺妙的，copy出一份新页，老页变成了非脏页继续用
            if (ftemp != NULL) {
                LOS_ListTailInsert(&dirtyList, &ftemp->node);//将新页插入脏页List,等待回写磁盘
            }
        }
        LOS_SpinUnlockRestore(&fpage->physSeg->lruLock, lruLock);
    }
    LOS_SpinUnlockRestore(&mapping->list_lock, intSave);

    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(fpage, ftemp, &dirtyList, LosFilePage, node) {//仔细看这个宏，关键在 &(item)->member != (list);
        OsDoFlushDirtyPage(fpage);//立马洗掉，所以dirtyList可以不是全局变量
    }
}

/******************************************************************************
 删除文件缓存,清空文件在page cache的痕迹
 参数 mapping 可理解为文件在内存的身份证
******************************************************************************/
VOID OsFileCacheRemove(struct page_mapping *mapping)
{
    UINT32 intSave;
    UINT32 lruSave;
    SPIN_LOCK_S *lruLock = NULL;
    LOS_DL_LIST_HEAD(dirtyList);//定义一个叫dirtyList的双循环链表并初始化,用于挂脏页
    LosFilePage *ftemp = NULL;
    LosFilePage *fpage = NULL;
    LosFilePage *fnext = NULL;

    LOS_SpinLockSave(&mapping->list_lock, &intSave);//多进程操作,必须上锁.
    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(fpage, fnext, &mapping->page_list, LosFilePage, node) {//遍历文件在内存中产生的所有文件页(例如1,4,8页)不一定连续,取决于用户的读取顺序
        lruLock = &fpage->physSeg->lruLock;
        LOS_SpinLockSave(lruLock, &lruSave);//@note_why 自旋锁有必要从这里开始上锁吗?
        if (OsIsPageDirty(fpage->vmPage)) {//数据是脏页吗,脏页就是被修改过数据的页
            ftemp = OsDumpDirtyPage(fpage);//做这个拷贝动作是为了fpage的统一下线,因为数据回写磁盘的速度是很慢的,如果直接在这里处理脏数据
            if (ftemp != NULL) {//会导致函数持有mapping->list_lock自旋锁的时间太长了,影响其他CPU的处理效率 
                LOS_ListTailInsert(&dirtyList, &ftemp->node);//将临时脏页挂到记录脏页链表上
            }
        }

        OsDeletePageCacheLru(fpage);//删除高速缓存和从置换链表中下线
        LOS_SpinUnlockRestore(lruLock, lruSave);
    }
    LOS_SpinUnlockRestore(&mapping->list_lock, intSave);//恢复自旋锁,不能让别的CPU等太久

    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(fpage, fnext, &dirtyList, LosFilePage, node) {//到这里,再来慢慢的统一处理脏页数据
        OsDoFlushDirtyPage(fpage);//遍历脏页链表,一页一页处理
    }
}
///虚拟内存文件操作实现类
LosVmFileOps g_commVmOps = {//
    .open = NULL,
    .close = NULL,
    .fault = OsVmmFileFault, //缺页中断处理
    .remove = OsVmmFileRemove,//删除页
};
//文件映射
INT32 OsVfsFileMmap(struct file *filep, LosVmMapRegion *region)
{
    region->unTypeData.rf.vmFOps = &g_commVmOps;//文件操作
    region->unTypeData.rf.vnode = filep->f_vnode;
    region->unTypeData.rf.f_oflags = filep->f_oflags;

    return ENOERR;
}
/*!
 有名映射,可理解为文件映射,跟匿名映射相对应
 参数filep是广义的文件,在鸿蒙内核,目录/普通文件/字符设备/块设备/网络套接字/管道/链接 都是文件
*/
STATUS_T OsNamedMMap(struct file *filep, LosVmMapRegion *region)
{
    struct Vnode *vnode = NULL;
    if (filep == NULL) {
        return LOS_ERRNO_VM_MAP_FAILED;
    }
    file_hold(filep);
    vnode = filep->f_vnode;
    VnodeHold();
    vnode->useCount++;
    VnodeDrop();
    if (filep->ops != NULL && filep->ops->mmap != NULL) {
        if (vnode->type == VNODE_TYPE_CHR || vnode->type == VNODE_TYPE_BLK) {//块设备或者字符设备 /dev/..
            LOS_SetRegionTypeDev(region);//设置为设备类型 
        } else {
            LOS_SetRegionTypeFile(region);//设置为文件类型
        }
        int ret = filep->ops->mmap(filep, region);
        if (ret != LOS_OK) {
            file_release(filep);
            return LOS_ERRNO_VM_MAP_FAILED;
        }
    } else {
        VM_ERR("mmap file type unknown");
        file_release(filep);
        return LOS_ERRNO_VM_MAP_FAILED;
    }
    file_release(filep);
    return LOS_OK;
}

/**************************************************************************************************
 通过位置从文件映射页中找到的指定的文件页
 举例:mapping->page_list上节点的数据可能只有是文件 1,3,4,6 页的数据,此时来找文件第5页的数据就会没有
**************************************************************************************************/
LosFilePage *OsFindGetEntry(struct page_mapping *mapping, VM_OFFSET_T pgoff)
{
    LosFilePage *fpage = NULL;

    LOS_DL_LIST_FOR_EACH_ENTRY(fpage, &mapping->page_list, LosFilePage, node) {//遍历文件页
        if (fpage->pgoff == pgoff) {//找到指定的页,
            return fpage;
        }

        if (fpage->pgoff > pgoff) {//大于之前还没有找到,说明不在链表中,往后的也不用找了,
            break;//因为 mapping->page_list节点上的数据都是按 fpage->pgoff 从小到大的顺序排列的.
        }
    }

    return NULL;
}

/* need mutex & change memory to dma zone. */
/*!
以页高速缓存方式分配一个文件页 LosFilePage
 Direct Memory Access（存储器直接访问）指一种高速的数据传输操作，允许在外部设备和存储器之间直接读写数据。
 整个数据传输操作在一个称为"DMA控制器"的控制下进行的。CPU只需在数据传输开始和结束时做一点处理（开始和结束时候要做中断处理）
*/
LosFilePage *OsPageCacheAlloc(struct page_mapping *mapping, VM_OFFSET_T pgoff)
{
    VOID *kvaddr = NULL;
    LosVmPhysSeg *physSeg = NULL;
    LosVmPage *vmPage = NULL;
    LosFilePage *fpage = NULL;

    vmPage = LOS_PhysPageAlloc();	//先分配一个物理页
    if (vmPage == NULL) {
        VM_ERR("alloc vm page failed");
        return NULL;
    }
    physSeg = OsVmPhysSegGet(vmPage);//通过页获取所在seg
    kvaddr = OsVmPageToVaddr(vmPage);//获取内核空间的虚拟地址,具体点进去看函数说明，这里一定要理解透彻！
    if ((physSeg == NULL) || (kvaddr == NULL)) {
        LOS_PhysPageFree(vmPage);	//异常情况要释放vmPage
        VM_ERR("alloc vm page failed!");
        return NULL;
    }

    fpage = (LosFilePage *)LOS_MemAlloc(m_aucSysMem0, sizeof(LosFilePage));//从内存池中分配一个filePage
    if (fpage == NULL) {
        LOS_PhysPageFree(vmPage);	//异常情况要释放vmPage
        VM_ERR("Failed to allocate for page!");
        return NULL;
    }

    (VOID)memset_s((VOID *)fpage, sizeof(LosFilePage), 0, sizeof(LosFilePage));//调标准库函数 置0

    LOS_ListInit(&fpage->i_mmap);	//初始化映射,链表上挂 MapInfo
    LOS_ListInit(&fpage->node);		//节点初始化
    LOS_ListInit(&fpage->lru);		//LRU初始化
    fpage->n_maps = 0;				//映射次数
    fpage->dirtyOff = PAGE_SIZE;	//默认页尾部,相当于没有脏数据
    fpage->dirtyEnd = 0;			//脏页结束位置
    fpage->physSeg = physSeg;		//页框所属段.其中包含了 LRU LIST ==
    fpage->vmPage = vmPage;			//物理页框
    fpage->mapping = mapping;		//记录所有文件页映射
    fpage->pgoff = pgoff;			//将文件切成一页页，页标
    (VOID)memset_s(kvaddr, PAGE_SIZE, 0, PAGE_SIZE);//页内数据清0

    return fpage;
}

#ifndef LOSCFG_FS_VFS
INT32 OsVfsFileMmap(struct file *filep, LosVmMapRegion *region)
{
    UNUSED(filep);
    UNUSED(region);
    return ENOERR;
}
#endif

#endif
