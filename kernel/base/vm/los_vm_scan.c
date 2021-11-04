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

#ifdef LOSCFG_FS_VFS

#include "fs/file.h"
#include "los_vm_filemap.h"

#ifdef LOSCFG_KERNEL_VM

/* unmap a lru page by map record info caller need lru lock */
/**************************************************************************************************
 解除文件页和进程(mmu)的映射关系
 参数info记录了进程的MMU
**************************************************************************************************/
VOID OsUnmapPageLocked(LosFilePage *page, LosMapInfo *info)
{
    if (page == NULL || info == NULL) {
        VM_ERR("UnmapPage error input null!");
        return;
    }
    page->n_maps--;
    LOS_ListDelete(&info->node);
    LOS_AtomicDec(&page->vmPage->refCounts);
    LOS_ArchMmuUnmap(info->archMmu, info->vaddr, 1);
    LOS_MemFree(m_aucSysMem0, info);//释放虚拟
}
///解除文件页在所有进程的映射
VOID OsUnmapAllLocked(LosFilePage *page)
{
    LosMapInfo *info = NULL;
    LosMapInfo *next = NULL;
    LOS_DL_LIST *immap = &page->i_mmap;
	
    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(info, next, immap, LosMapInfo, node) {//遍历 immap->info 链表
        OsUnmapPageLocked(page, info);
    }
}

/* add a new lru node to lru list, lruType can be file or anon */
VOID OsLruCacheAdd(LosFilePage *fpage, enum OsLruList lruType)//在lru列表中添加一个新的lru节点，lruType可以是文件或匿名
{
    UINT32 intSave;
    LosVmPhysSeg *physSeg = fpage->physSeg;	//得到页面对应段
    LosVmPage *page = fpage->vmPage;		//得到物理页面

    LOS_SpinLockSave(&physSeg->lruLock, &intSave);//自旋锁:最多只能被一个内核持有，CPU内核 互斥锁
    OsSetPageActive(page);	//设置页面为活动页
    OsCleanPageReferenced(page);//清除页面被引用位
    physSeg->lruSize[lruType]++;	//lruType页总size++
    LOS_ListTailInsert(&physSeg->lruList[lruType], &fpage->lru);//加入lruType页双循环链表中

    LOS_SpinUnlockRestore(&physSeg->lruLock, intSave);//解锁
}

/* dellete a lru node, caller need hold lru_lock */
VOID OsLruCacheDel(LosFilePage *fpage)//删除lru节点，调用者需要拿到lru锁
{
    LosVmPhysSeg *physSeg = fpage->physSeg;	//得到页面对应段
    int type = OsIsPageActive(fpage->vmPage) ? VM_LRU_ACTIVE_FILE : VM_LRU_INACTIVE_FILE;//得到页面LRU类型

    physSeg->lruSize[type]--;	//type页总size--
    LOS_ListDelete(&fpage->lru);//将自己从lru链表中摘出来
}
///非活动文件页低于活动文件页吗
BOOL OsInactiveListIsLow(LosVmPhysSeg *physSeg)
{
    return (physSeg->lruSize[VM_LRU_ACTIVE_FILE] >
            physSeg->lruSize[VM_LRU_INACTIVE_FILE]) ? TRUE : FALSE;//直接对比size，效率杠杠的
}

/* move a page from inactive list to active list  head */
STATIC INLINE VOID OsMoveToActiveList(LosFilePage *fpage)//将页面从非活动列表移动到活动列表
{
    LosVmPhysSeg *physSeg = fpage->physSeg;		//得到页面对应段

    physSeg->lruSize[VM_LRU_ACTIVE_FILE]++; 	//活动页总size++
    physSeg->lruSize[VM_LRU_INACTIVE_FILE]--;	//不活动页总size--
    LOS_ListDelete(&fpage->lru);				//将自己从lru链表中摘出来
    LOS_ListTailInsert(&physSeg->lruList[VM_LRU_ACTIVE_FILE], &fpage->lru);//加入活动页双循环链表中
}

/* move a page from active list to inactive list  head */
STATIC INLINE VOID OsMoveToInactiveList(LosFilePage *fpage)//将页面从活动列表移动到非活动列表
{
    LosVmPhysSeg *physSeg = fpage->physSeg;		//得到页面对应段

    physSeg->lruSize[VM_LRU_ACTIVE_FILE]--;		//活动页总size--
    physSeg->lruSize[VM_LRU_INACTIVE_FILE]++;	//不活动页总size++
    LOS_ListDelete(&fpage->lru);				//将自己从lru链表中摘出来
    LOS_ListTailInsert(&physSeg->lruList[VM_LRU_INACTIVE_FILE], &fpage->lru);//加入不活动页双循环链表中
}

/* move a page to the most active pos in lru list(active head) *///将页面移至lru列表中最活跃的位置
STATIC INLINE VOID OsMoveToActiveHead(LosFilePage *fpage)
{
    LosVmPhysSeg *physSeg = fpage->physSeg;	//得到页面对应段
    LOS_ListDelete(&fpage->lru);			//将自己从lru链表中摘出来
    LOS_ListTailInsert(&physSeg->lruList[VM_LRU_ACTIVE_FILE], &fpage->lru);//加入活动页双循环链表中
}

/* move a page to the most active pos in lru list(inactive head) */
STATIC INLINE VOID OsMoveToInactiveHead(LosFilePage *fpage)//鸿蒙会从inactive链表的尾部开始进行回收,跟linux一样
{
    LosVmPhysSeg *physSeg = fpage->physSeg;	//得到页面对应段
    LOS_ListDelete(&fpage->lru);			//将自己从lru链表中摘出来
    LOS_ListTailInsert(&physSeg->lruList[VM_LRU_INACTIVE_FILE], &fpage->lru);//加入不活动页双循环链表中
}

/* page referced add: (call by page cache get)
----------inactive----------|----------active------------
[ref:0,act:0], [ref:1,act:0]|[ref:0,act:1], [ref:1,act:1]
ref:0, act:0 --> ref:1, act:0
ref:1, act:0 --> ref:0, act:1
ref:0, act:1 --> ref:1, act:1
*/
VOID OsPageRefIncLocked(LosFilePage *fpage)// ref ,act 标签转换功能
{
    BOOL isOrgActive;
    UINT32 intSave;
    LosVmPage *page = NULL;

    if (fpage == NULL) {
        return;
    }

    LOS_SpinLockSave(&fpage->physSeg->lruLock, &intSave);//要处理lruList,先拿锁

    page = fpage->vmPage;//拿到物理页框
    isOrgActive = OsIsPageActive(page);//页面是否在活动

    if (OsIsPageReferenced(page) && !OsIsPageActive(page)) {//身兼 不活动和引用标签
        OsCleanPageReferenced(page);//撕掉引用标签  ref:1, act:0 --> ref:0, act:1
        OsSetPageActive(page);		//贴上活动标签
    } else if (!OsIsPageReferenced(page)) {
        OsSetPageReferenced(page);//ref:0, act:0 --> ref:1, act:0
    }

    if (!isOrgActive && OsIsPageActive(page)) {
        /* move inactive to active */
        OsMoveToActiveList(fpage);
    /* no change, move head */
    } else {
        if (OsIsPageActive(page)) {
            OsMoveToActiveHead(fpage);
        } else {
            OsMoveToInactiveHead(fpage);
        }
    }

    LOS_SpinUnlockRestore(&fpage->physSeg->lruLock, intSave);
}

/* page referced dec: (call by thrinker)
----------inactive----------|----------active------------
[ref:0,act:0], [ref:1,act:0]|[ref:0,act:1], [ref:1,act:1]
ref:1, act:1 --> ref:0, act:1
ref:0, act:1 --> ref:1, act:0
ref:1, act:0 --> ref:0, act:0
*/
VOID OsPageRefDecNoLock(LosFilePage *fpage) // ref ,act 标签转换功能
{
    BOOL isOrgActive;
    LosVmPage *page = NULL;

    if (fpage == NULL) {
        return;
    }

    page = fpage->vmPage;
    isOrgActive = OsIsPageActive(page);

    if (!OsIsPageReferenced(page) && OsIsPageActive(page)) {//[ref:0,act:1]的情况
        OsCleanPageActive(page);
        OsSetPageReferenced(page);
    } else if (OsIsPageReferenced(page)) {
        OsCleanPageReferenced(page);
    }

    if (isOrgActive && !OsIsPageActive(page)) {
        OsMoveToInactiveList(fpage);
    }
}
///缩小活动页链表
VOID OsShrinkActiveList(LosVmPhysSeg *physSeg, int nScan)
{
    LosFilePage *fpage = NULL;
    LosFilePage *fnext = NULL;
    LOS_DL_LIST *activeFile = &physSeg->lruList[VM_LRU_ACTIVE_FILE];

    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(fpage, fnext, activeFile, LosFilePage, lru) {//一页一页处理
        if (LOS_SpinTrylock(&fpage->mapping->list_lock) != LOS_OK) {//尝试获取文件页所在的page_mapping锁
            continue;//接着处理下一文件页
        }

        /* happend when caller hold cache lock and try reclaim this page *///调用方持有缓存锁并尝试回收此页时发生
        if (OsIsPageLocked(fpage->vmPage)) {//页面是否被锁
            LOS_SpinUnlock(&fpage->mapping->list_lock);//失败时,一定要释放page_mapping锁.
            continue;//接着处理下一文件页
        }

        if (OsIsPageMapped(fpage) && (fpage->flags & VM_MAP_REGION_FLAG_PERM_EXECUTE)) {//文件页是否被映射而且是个可执行文件 ?
            LOS_SpinUnlock(&fpage->mapping->list_lock);//是时,一定要释放page_mapping锁.
            continue;//接着处理下一文件页
        }
		//找了可以收缩的文件页
        OsPageRefDecNoLock(fpage); //将页面移到未活动文件链表

        LOS_SpinUnlock(&fpage->mapping->list_lock);	//释放page_mapping锁.

        if (--nScan <= 0) {
            break;
        }
    }
}
///缩小未活动页链表
int OsShrinkInactiveList(LosVmPhysSeg *physSeg, int nScan, LOS_DL_LIST *list)
{
    UINT32 nrReclaimed = 0;
    LosVmPage *page = NULL;
    SPIN_LOCK_S *flock = NULL;
    LosFilePage *fpage = NULL;
    LosFilePage *fnext = NULL;
    LosFilePage *ftemp = NULL;
    LOS_DL_LIST *inactive_file = &physSeg->lruList[VM_LRU_INACTIVE_FILE];

    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(fpage, fnext, inactive_file, LosFilePage, lru) {//遍历链表一页一页处理
        flock = &fpage->mapping->list_lock;

        if (LOS_SpinTrylock(flock) != LOS_OK) {//尝试获取文件页所在的page_mapping锁
            continue;//接着处理下一文件页
        }

        page = fpage->vmPage;//获取物理页框
        if (OsIsPageLocked(page)) {//页面是否被锁
            LOS_SpinUnlock(flock);
            continue;//接着处理下一文件页
        }

        if (OsIsPageMapped(fpage) && (OsIsPageDirty(page) || (fpage->flags & VM_MAP_REGION_FLAG_PERM_EXECUTE))) {
            LOS_SpinUnlock(flock);//文件页是否被映射而且是个脏页获取是个可执行文件 ?
            continue;//接着处理下一文件页
        }

        if (OsIsPageDirty(page)) {//是脏页
            ftemp = OsDumpDirtyPage(fpage);//备份脏页
            if (ftemp != NULL) {//备份成功了
                LOS_ListTailInsert(list, &ftemp->node);//将脏页挂到参数链表上带走
            }
        }

        OsDeletePageCacheLru(fpage);//将文件页从LRU和pagecache上摘除
        LOS_SpinUnlock(flock);
        nrReclaimed++;//成功回收了一页

        if (--nScan <= 0) {//继续回收
            break;
        }
    }

    return nrReclaimed;
}
///是否 未活动文件页少于活动文件页
bool InactiveListIsLow(LosVmPhysSeg *physSeg)
{
    return (physSeg->lruSize[VM_LRU_ACTIVE_FILE] > physSeg->lruSize[VM_LRU_INACTIVE_FILE]) ? TRUE : FALSE;
}

#ifdef LOSCFG_FS_VFS
int OsTryShrinkMemory(size_t nPage)//尝试收缩文件页
{
    UINT32 intSave;
    size_t totalPages;
    size_t nReclaimed = 0;
    LosVmPhysSeg *physSeg = NULL;
    UINT32 index;
    LOS_DL_LIST_HEAD(dirtyList);//初始化脏页链表,上面将挂所有脏页用于同步到磁盘后回收
    LosFilePage *fpage = NULL;
    LosFilePage *fnext = NULL;

    if (nPage <= 0) {
        nPage = VM_FILEMAP_MIN_SCAN;//
    }

    if (nPage > VM_FILEMAP_MAX_SCAN) {
        nPage = VM_FILEMAP_MAX_SCAN;
    }

    for (index = 0; index < g_vmPhysSegNum; index++) {//遍历整个物理段组
        physSeg = &g_vmPhysSeg[index];//一段段来
        LOS_SpinLockSave(&physSeg->lruLock, &intSave);
        totalPages = physSeg->lruSize[VM_LRU_ACTIVE_FILE] + physSeg->lruSize[VM_LRU_INACTIVE_FILE];//统计所有文件页
        if (totalPages < VM_FILEMAP_MIN_SCAN) {//文件页占用内存不多的情况下,怎么处理?
            LOS_SpinUnlockRestore(&physSeg->lruLock, intSave);
            continue;//放过这一段,找下一段
        }

        if (InactiveListIsLow(physSeg)) {//未活动页少于活动页的情况
            OsShrinkActiveList(physSeg, (nPage < VM_FILEMAP_MIN_SCAN) ? VM_FILEMAP_MIN_SCAN : nPage);//缩小活动页
        }

        nReclaimed += OsShrinkInactiveList(physSeg, nPage, &dirtyList);//缩小未活动页,带出脏页链表
        LOS_SpinUnlockRestore(&physSeg->lruLock, intSave);

        if (nReclaimed >= nPage) {//够了,够了,达到目的了.
            break;//退出收缩
        }
    }

    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(fpage, fnext, &dirtyList, LosFilePage, node) {//遍历处理脏页数据
        OsDoFlushDirtyPage(fpage);//冲洗脏页数据,将脏页数据回写磁盘
    }

    return nReclaimed;
}
#else
int OsTryShrinkMemory(size_t nPage)
{
    return 0;
}
#endif
#endif

#endif
