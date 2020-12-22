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
 * @defgroup los_vm_filemap vm filemap definition
 * @ingroup kernel
 */

#ifndef __LOS_VM_FILEMAP_H__
#define __LOS_VM_FILEMAP_H__

#include "fs/file.h"
#include "los_vm_map.h"
#include "los_vm_page.h"
#include "los_vm_common.h"
#include "los_vm_phys.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
//文件页结构体
typedef struct FilePage {
    LOS_DL_LIST             node;		//节点		
    LOS_DL_LIST             lru;		//lru节点, 结合 LosVmPhysSeg: LOS_DL_LIST lruList[VM_NR_LRU_LISTS] 理解
    LOS_DL_LIST             i_mmap;       /* list of mappings */
    UINT32                  n_maps;       /* num of mapping */
    struct VmPhysSeg        *physSeg;      /* physical memory that file page belongs to */
    struct VmPage           *vmPage;	//物理页框
    struct page_mapping     *mapping;	//此结构由文件系统提供，用于描述装入点 见于 ..\third_party\NuttX\include\nuttx\fs\fs.h
    VM_OFFSET_T             pgoff;		//页偏移地址，主要用于seek时使用
    UINT32                  flags;		//标签
    UINT16                  dirtyOff;	//脏页的页内偏移地址
    UINT16                  dirtyEnd;	//脏页的结束位置
} LosFilePage;
//虚拟地址和文件页的映射信息
typedef struct MapInfo {
    LOS_DL_LIST             node;	//节点
    VADDR_T                 vaddr;	//虚拟地址
    LosFilePage             *page;	//文件页
    LosArchMmu              *archMmu;//mmu
} LosMapInfo;
//Flags由 bitmap 管理
enum OsPageFlags {
    FILE_PAGE_FREE,			//空闲页	
    FILE_PAGE_LOCKED,		//被锁页
    FILE_PAGE_REFERENCED,   //被引用页
    FILE_PAGE_DIRTY,		//脏页
    FILE_PAGE_LRU,			//LRU置换页
    FILE_PAGE_ACTIVE,		//活动页
    FILE_PAGE_SHARED,		//共享页
};

#define PGOFF_MAX                       2000
#define MAX_SHRINK_PAGECACHE_TRY        2
#define VM_FILEMAP_MAX_SCAN             (SYS_MEM_SIZE_DEFAULT >> PAGE_SHIFT)
#define VM_FILEMAP_MIN_SCAN             32
//给页面贴上被锁的标签
STATIC INLINE VOID OsSetPageLocked(LosVmPage *page)
{
    LOS_BitmapSet(&page->flags, FILE_PAGE_LOCKED);
}
//给页面撕掉被锁的标签
STATIC INLINE VOID OsCleanPageLocked(LosVmPage *page)
{
    LOS_BitmapClr(&page->flags, FILE_PAGE_LOCKED);
}
//给页面贴上数据被修改的标签
STATIC INLINE VOID OsSetPageDirty(LosVmPage *page)
{
    LOS_BitmapSet(&page->flags, FILE_PAGE_DIRTY);
}
//给页面撕掉数据被修改的标签
STATIC INLINE VOID OsCleanPageDirty(LosVmPage *page)
{
    LOS_BitmapClr(&page->flags, FILE_PAGE_DIRTY);
}
//给页面贴上活动的标签
STATIC INLINE VOID OsSetPageActive(LosVmPage *page)
{
    LOS_BitmapSet(&page->flags, FILE_PAGE_ACTIVE);
}
//给页面撕掉活动的标签
STATIC INLINE VOID OsCleanPageActive(LosVmPage *page)
{
    LOS_BitmapClr(&page->flags, FILE_PAGE_ACTIVE);
}
//给页面贴上置换页的标签
STATIC INLINE VOID OsSetPageLRU(LosVmPage *page)
{
    LOS_BitmapSet(&page->flags, FILE_PAGE_LRU);
}
//给页面贴上被释放的标签
STATIC INLINE VOID OsSetPageFree(LosVmPage *page)
{
    LOS_BitmapSet(&page->flags, FILE_PAGE_FREE);
}
//给页面撕掉被释放的标签
STATIC INLINE VOID OsCleanPageFree(LosVmPage *page)
{
    LOS_BitmapClr(&page->flags, FILE_PAGE_FREE);
}
//给页面贴上被引用的标签
STATIC INLINE VOID OsSetPageReferenced(LosVmPage *page)
{
    LOS_BitmapSet(&page->flags, FILE_PAGE_REFERENCED);
}
//给页面撕掉被引用的标签
STATIC INLINE VOID OsCleanPageReferenced(LosVmPage *page)
{
    LOS_BitmapClr(&page->flags, FILE_PAGE_REFERENCED);
}
//页面是否活动
STATIC INLINE BOOL OsIsPageActive(LosVmPage *page)
{
    return BIT_GET(page->flags, FILE_PAGE_ACTIVE);
}
//页面是否被锁
STATIC INLINE BOOL OsIsPageLocked(LosVmPage *page)
{
    return BIT_GET(page->flags, FILE_PAGE_LOCKED);
}
//页面是否被引用，只被一个进程引用的页叫私有页，多个进程引用就是共享页，此为共享内存的本质所在
STATIC INLINE BOOL OsIsPageReferenced(LosVmPage *page)
{
    return BIT_GET(page->flags, FILE_PAGE_REFERENCED);
}
//页面是否为脏页，所谓脏页就是页内数据是否被更新过，只有脏页才会有写时拷贝
STATIC INLINE BOOL OsIsPageDirty(LosVmPage *page)
{
    return BIT_GET(page->flags, FILE_PAGE_DIRTY);
}
//文件页是否映射过了
STATIC INLINE BOOL OsIsPageMapped(LosFilePage *page)
{
    return (page->n_maps != 0);//由映射的次数来判断
}

/* The follow three functions is used to SHM module */
STATIC INLINE VOID OsSetPageShared(LosVmPage *page)//给页面贴上共享页标签
{
    LOS_BitmapSet(&page->flags, FILE_PAGE_SHARED);//设为共享页面,共享页位 置0
}
//给页面撕掉共享页标签
STATIC INLINE VOID OsCleanPageShared(LosVmPage *page)
{
    LOS_BitmapClr(&page->flags, FILE_PAGE_SHARED);//共享页位 置0
}
//是否为共享页
STATIC INLINE BOOL OsIsPageShared(LosVmPage *page)
{
    return BIT_GET(page->flags, FILE_PAGE_SHARED);
}

INT32 OsVfsFileMmap(struct file *filep, LosVmMapRegion *region);
LosFilePage *OsPageCacheAlloc(struct page_mapping *mapping, VM_OFFSET_T pgoff);
LosFilePage *OsFindGetEntry(struct page_mapping *mapping, VM_OFFSET_T pgoff);
LosMapInfo *OsGetMapInfo(LosFilePage *page, LosArchMmu *archMmu, VADDR_T vaddr);
VOID OsAddMapInfo(LosFilePage *page, LosArchMmu *archMmu, VADDR_T vaddr);
VOID OsDelMapInfo(LosVmMapRegion *region, LosVmPgFault *pgFault, BOOL cleanDirty);
VOID OsFileCacheFlush(struct page_mapping *mapping);
VOID OsFileCacheRemove(struct page_mapping *mapping);
VOID OsUnmapPageLocked(LosFilePage *page, LosMapInfo *info);
VOID OsUnmapAllLocked(LosFilePage *page);
VOID OsLruCacheAdd(LosFilePage *fpage, enum OsLruList lruType);
VOID OsLruCacheDel(LosFilePage *fpage);
LosFilePage *OsDumpDirtyPage(LosFilePage *oldPage);
VOID OsDoFlushDirtyPage(LosFilePage *fpage);
VOID OsDeletePageCacheLru(LosFilePage *page);
STATUS_T OsNamedMMap(struct file *filep, LosVmMapRegion *region);
VOID OsPageRefDecNoLock(LosFilePage *page);
VOID OsPageRefIncLocked(LosFilePage *page);
int OsTryShrinkMemory(size_t nPage);
VOID OsMarkPageDirty(LosFilePage *fpage, LosVmMapRegion *region, int off, int len);

typedef struct ProcessCB LosProcessCB;
VOID OsVmmFileRegionFree(struct file *filep, LosProcessCB *processCB);
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* __LOS_VM_FILEMAP_H__ */

