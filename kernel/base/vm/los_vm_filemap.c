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
#define UNUSED(x)          (VOID)(x)  // 宏定义：将未使用的变量标记为已使用，避免编译警告
#endif

#ifdef LOSCFG_DEBUG_VERSION
static int g_totalPageCacheTry = 0;    // 页面缓存尝试次数统计
static int g_totalPageCacheHit = 0;    // 页面缓存命中次数统计
#define TRACE_TRY_CACHE() do { g_totalPageCacheTry++; } while (0)  // 增加缓存尝试计数
#define TRACE_HIT_CACHE() do { g_totalPageCacheHit++; } while (0)  // 增加缓存命中计数

/**
 * @brief  重置页面缓存命中统计信息
 * @param  try [out] 输出之前的尝试次数
 * @param  hit [out] 输出之前的命中次数
 * @return 无
 */
VOID ResetPageCacheHitInfo(int *try, int *hit)
{
    *try = g_totalPageCacheTry;        // 保存尝试次数
    *hit = g_totalPageCacheHit;        // 保存命中次数
    g_totalPageCacheHit = 0;           // 重置命中次数
    g_totalPageCacheTry = 0;           // 重置尝试次数
}
#else
#define TRACE_TRY_CACHE()              // 调试版本未启用时为空宏
#define TRACE_HIT_CACHE()              // 调试版本未启用时为空宏
#endif
#ifdef LOSCFG_KERNEL_VM


/**
 * @brief  将文件页添加到页面缓存链表
 * @param  page [in] 要添加的文件页
 * @param  mapping [in] 文件映射结构
 * @param  pgoff [in] 页面偏移量
 * @return 无
 */
STATIC VOID OsPageCacheAdd(LosFilePage *page, struct page_mapping *mapping, VM_OFFSET_T pgoff)
{
    LosFilePage *fpage = NULL;         // 遍历用的文件页指针

    LOS_DL_LIST_FOR_EACH_ENTRY(fpage, &mapping->page_list, LosFilePage, node) {// 遍历page_list链表
        if (fpage->pgoff > pgoff) {    // 找到第一个pgoff大于当前页的节点
            LOS_ListTailInsert(&fpage->node, &page->node); // 插入到该节点前面
            goto done_add;             // 跳转至添加完成处理
        }
    }

    LOS_ListTailInsert(&mapping->page_list, &page->node); // 未找到更大pgoff的节点，添加到链表末尾

done_add:
    mapping->nrpages++;                // 文件在缓存中的页面计数加1
}

/**
 * @brief  将文件页添加到页面缓存和LRU链表
 * @param  page [in] 要添加的文件页
 * @param  mapping [in] 文件映射结构
 * @param  pgoff [in] 页面偏移量
 * @return 无
 */
VOID OsAddToPageacheLru(LosFilePage *page, struct page_mapping *mapping, VM_OFFSET_T pgoff)
{
    OsPageCacheAdd(page, mapping, pgoff);                  // 添加到页面缓存
    OsLruCacheAdd(page, VM_LRU_ACTIVE_FILE);               // 添加到活跃文件LRU链表
}

/**
 * @brief  从页面缓存中删除文件页
 * @param  fpage [in] 要删除的文件页
 * @return 无
 */
VOID OsPageCacheDel(LosFilePage *fpage)
{
    /* delete from file cache list */
    LOS_ListDelete(&fpage->node);      // 将文件页从链表中摘除
    fpage->mapping->nrpages--;         // 文件在缓存中的页面计数减1

    /* unmap and remove map info */
    if (OsIsPageMapped(fpage)) {       // 检查页面是否有映射
        OsUnmapAllLocked(fpage);       // 解除所有映射
    }

    LOS_PhysPageFree(fpage->vmPage);   // 释放物理内存
    LOS_MemFree(m_aucSysMem0, fpage);  // 释放文件页结构体内存
}

/**************************************************************************************************
每个进程都有自己的地址空间, 多个进程可以访问同一个LosFilePage,每个进程使用的虚拟地址都需要单独映射
所以同一个LosFilePage会映射到多个进程空间.本函数记录页面被哪些进程映射过
在两个地方会被被空间映射
1.缺页中断 2.克隆地址空间
**************************************************************************************************/
/**
 * @brief  添加文件页的映射信息
 * @param  page [in] 文件页
 * @param  archMmu [in] 进程MMU结构
 * @param  vaddr [in] 虚拟地址
 * @return 无
 */
VOID OsAddMapInfo(LosFilePage *page, LosArchMmu *archMmu, VADDR_T vaddr)
{
    LosMapInfo *info = NULL;           // 映射信息指针

    info = (LosMapInfo *)LOS_MemAlloc(m_aucSysMem0, sizeof(LosMapInfo)); // 分配映射信息内存
    if (info == NULL) {
        VM_ERR("OsAddMapInfo alloc memory failed!"); // 内存分配失败打印错误
        return;
    }
    info->page = page;                 // 设置映射的文件页
    info->archMmu = archMmu;           // 设置进程MMU
    info->vaddr = vaddr;               // 设置虚拟地址

    LOS_ListAdd(&page->i_mmap, &info->node); // 将映射信息添加到链表
    page->n_maps++;                    // 映射计数加1
}

/**
 * @brief  获取文件页的映射信息
 * @param  page [in] 文件页
 * @param  archMmu [in] 进程MMU结构
 * @param  vaddr [in] 虚拟地址
 * @return 成功返回映射信息指针，失败返回NULL
 */
LosMapInfo *OsGetMapInfo(const LosFilePage *page, const LosArchMmu *archMmu, VADDR_T vaddr)
{
    LosMapInfo *info = NULL;           // 映射信息指针
    const LOS_DL_LIST *immap = &page->i_mmap; // 获取映射信息链表

    LOS_DL_LIST_FOR_EACH_ENTRY(info, immap, LosMapInfo, node) { // 遍历映射信息链表
        // 匹配MMU、虚拟地址和文件页
        if ((info->archMmu == archMmu) && (info->vaddr == vaddr) && (info->page == page)) {
            return info;               // 找到匹配的映射信息并返回
        }
    }
    return NULL;                       // 未找到返回NULL
}

/**
 * @brief  从LRU和页面缓存中删除文件页
 * @param  page [in] 要删除的文件页
 * @return 无
 */
VOID OsDeletePageCacheLru(LosFilePage *page)
{
    /* delete form lru list */
    OsLruCacheDel(page);               // 从LRU列表中删除
    /* delete from cache lits and free pmm if need */
    OsPageCacheDel(page);              // 从页面缓存中删除
}


/**
 * @brief  解除文件页的映射
 * @param  fpage [in] 文件页
 * @param  archMmu [in] 进程MMU结构
 * @param  vaddr [in] 虚拟地址
 * @return 无
 */
STATIC VOID OsPageCacheUnmap(LosFilePage *fpage, LosArchMmu *archMmu, VADDR_T vaddr)
{
    UINT32 intSave;                    // 中断状态保存变量
    LosMapInfo *info = NULL;           // 映射信息指针

    LOS_SpinLockSave(&fpage->physSeg->lruLock, &intSave); // 获取自旋锁并禁止中断
    info = OsGetMapInfo(fpage, archMmu, vaddr); // 获取映射信息
    if (info == NULL) {
        VM_ERR("OsPageCacheUnmap get map info failed!"); // 获取映射信息失败
    } else {
        OsUnmapPageLocked(fpage, info); // 解除页面映射
    }
    // 检查页面是否还有映射或特殊标志
    if (!(OsIsPageMapped(fpage) && ((fpage->flags & VM_MAP_REGION_FLAG_PERM_EXECUTE) ||
        OsIsPageDirty(fpage->vmPage)))) {
        OsPageRefDecNoLock(fpage);     // 减少页面引用计数
    }

    LOS_SpinUnlockRestore(&fpage->physSeg->lruLock, intSave); // 释放自旋锁并恢复中断
}

/**
 * @brief  删除文件映射的页面
 * @param  region [in] 内存映射区域
 * @param  archMmu [in] 进程MMU结构
 * @param  pgoff [in] 页面偏移量
 * @return 无
 */
VOID OsVmmFileRemove(LosVmMapRegion *region, LosArchMmu *archMmu, VM_OFFSET_T pgoff)
{
    UINT32 intSave;                    // 中断状态保存变量
    vaddr_t vaddr;                     // 虚拟地址
    paddr_t paddr = 0;                 // 物理地址
    struct Vnode *vnode = NULL;        // Vnode结构指针
    struct page_mapping *mapping = NULL; // 页面映射结构指针
    LosFilePage *fpage = NULL;         // 文件页指针
    LosFilePage *tmpPage = NULL;       // 临时文件页指针
    LosVmPage *mapPage = NULL;         // 虚拟内存页指针

    if (!LOS_IsRegionFileValid(region) || (region->unTypeData.rf.vnode == NULL)) {
        return;                        // 检查区域是否有效
    }
    vnode = region->unTypeData.rf.vnode; // 获取Vnode
    mapping = &vnode->mapping;         // 获取页面映射
    // 计算虚拟地址
    vaddr = region->range.base + ((UINT32)(pgoff - region->pgOff) << PAGE_SHIFT);

    status_t status = LOS_ArchMmuQuery(archMmu, vaddr, &paddr, NULL); // 查询物理地址
    if (status != LOS_OK) {
        return;                        // 查询失败返回
    }

    mapPage = LOS_VmPageGet(paddr);    // 获取虚拟内存页

    /* is page is in cache list */
    LOS_SpinLockSave(&mapping->list_lock, &intSave); // 获取自旋锁
    fpage = OsFindGetEntry(mapping, pgoff); // 查找文件页
    /* no cache or have cache but not map(cow), free it direct */
    if ((fpage == NULL) || (fpage->vmPage != mapPage)) { // 无缓存或非映射页
        LOS_PhysPageFree(mapPage);     // 释放物理页
        LOS_ArchMmuUnmap(archMmu, vaddr, 1); // 解除虚拟地址映射
    /* this is a page cache map! */
    } else {
        OsPageCacheUnmap(fpage, archMmu, vaddr); // 解除缓存映射
        if (OsIsPageDirty(fpage->vmPage)) { // 检查是否为脏页
            tmpPage = OsDumpDirtyPage(fpage); // 转储脏页
        }
    }
    LOS_SpinUnlockRestore(&mapping->list_lock, intSave);

    if (tmpPage) {
        OsDoFlushDirtyPage(tmpPage);   // 刷新脏页
    }
    return;
}

/**
 * @brief  标记页面为脏页
 * @param  fpage [in] 文件页
 * @param  region [in] 内存映射区域
 * @param  off [in] 偏移量
 * @param  len [in] 长度
 * @return 无
 */
VOID OsMarkPageDirty(LosFilePage *fpage, const LosVmMapRegion *region, INT32 off, INT32 len)
{
    if (region != NULL) {
        OsSetPageDirty(fpage->vmPage); // 设置页面为脏页
        fpage->dirtyOff = off;         // 设置脏页偏移
        fpage->dirtyEnd = len;         // 设置脏页结束位置
    } else {
        OsSetPageDirty(fpage->vmPage); // 设置页面为脏页
        if ((off + len) > fpage->dirtyEnd) {
            fpage->dirtyEnd = off + len; // 更新脏页结束位置
        }

        if (off < fpage->dirtyOff) {
            fpage->dirtyOff = off;     // 更新脏页起始位置
        }
    }
}

/**
 * @brief  获取脏页大小
 * @param  fpage [in] 文件页
 * @param  vnode [in] Vnode结构
 * @return 脏页大小
 */
STATIC UINT32 GetDirtySize(LosFilePage *fpage, struct Vnode *vnode)
{
    UINT32 fileSize;                   // 文件大小
    UINT32 dirtyBegin;                 // 脏页起始位置
    UINT32 dirtyEnd;                   // 脏页结束位置
    struct stat buf_stat;              // 文件状态结构

    if (stat(vnode->filePath, &buf_stat) != OK) { // 获取文件状态
        VM_ERR("FlushDirtyPage get file size failed. (filePath=%s)", vnode->filePath);
        return 0;
    }

    fileSize = buf_stat.st_size;       // 获取文件大小
    dirtyBegin = ((UINT32)fpage->pgoff << PAGE_SHIFT); // 计算脏页起始偏移
    dirtyEnd = dirtyBegin + PAGE_SIZE; // 计算脏页结束偏移

    if (dirtyBegin >= fileSize) {
        return 0;                      // 脏页起始位置超出文件大小
    }

    if (dirtyEnd >= fileSize) {
        return fileSize - dirtyBegin;  // 脏页部分超出文件大小
    }

    return PAGE_SIZE;                  // 返回完整页大小
}

/**
 * @brief  刷新脏页到磁盘
 * @param  fpage [in] 文件页
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATIC INT32 OsFlushDirtyPage(LosFilePage *fpage)
{
    ssize_t ret;                       // 返回值
    size_t len;                        // 长度
    char *buff = NULL;                 // 缓冲区指针
    struct Vnode *vnode = fpage->mapping->host; /* owner of this mapping */
    if (vnode == NULL) {
        VM_ERR("page cache vnode error"); // Vnode为空错误
        return LOS_NOK;
    }

    len = fpage->dirtyEnd - fpage->dirtyOff; // 计算脏数据长度
    len = (len == 0) ? GetDirtySize(fpage, vnode) : len; // 处理长度为0的情况
    if (len == 0) {                    // 无脏数据
        OsCleanPageDirty(fpage->vmPage); // 清除脏页标记
        return LOS_OK;
    }

    buff = (char *)OsVmPageToVaddr(fpage->vmPage); // 获取页面虚拟地址

    /* actually, we did not update the fpage->dirtyOff */
    ret = vnode->vop->WritePage(vnode, (VOID *)buff, fpage->pgoff, len); // 写入页面
    if (ret <= 0) {
        VM_ERR("WritePage error ret %d", ret); // 写入失败
    } else {
        OsCleanPageDirty(fpage->vmPage); // 清除脏页标记
    }
    ret = (ret <= 0) ? LOS_NOK : LOS_OK; // 设置返回值

    return ret;
}

/**
 * @brief  转储脏页
 * @param  oldFPage [in] 原脏页
 * @return 新的文件页指针
 */
LosFilePage *OsDumpDirtyPage(LosFilePage *oldFPage)
{
    LosFilePage *newFPage = NULL;      // 新文件页指针

    newFPage = (LosFilePage *)LOS_MemAlloc(m_aucSysMem0, sizeof(LosFilePage)); // 分配内存
    if (newFPage == NULL) {
        VM_ERR("Failed to allocate for temp page!"); // 分配失败
        return NULL;
    }

    OsCleanPageDirty(oldFPage->vmPage); // 清除原页脏标记
    // 复制文件页内容
    (VOID)memcpy_s(newFPage, sizeof(LosFilePage), oldFPage, sizeof(LosFilePage));

    return newFPage;                   // 返回新文件页
}

/**
 * @brief  执行脏页刷新
 * @param  fpage [in] 文件页
 * @return 无
 */
VOID OsDoFlushDirtyPage(LosFilePage *fpage)
{
    if (fpage == NULL) {
        return;                        // 参数检查
    }
    (VOID)OsFlushDirtyPage(fpage);     // 刷新脏页
    LOS_MemFree(m_aucSysMem0, fpage);  // 释放文件页内存
}

/**
 * @brief  释放文件页
 * @param  mapping [in] 页面映射结构
 * @param  fpage [in] 文件页
 * @return 无
 */
STATIC VOID OsReleaseFpage(struct page_mapping *mapping, LosFilePage *fpage)
{
    UINT32 intSave;                    // 中断状态保存变量
    UINT32 lruSave;                    // LRU锁状态保存变量
    SPIN_LOCK_S *lruLock = &fpage->physSeg->lruLock; // LRU锁
    LOS_SpinLockSave(&mapping->list_lock, &intSave); // 获取映射锁
    LOS_SpinLockSave(lruLock, &lruSave); // 获取LRU锁
    OsCleanPageLocked(fpage->vmPage);  // 清除页面锁定
    OsDeletePageCacheLru(fpage);       // 删除页面缓存和LRU
    LOS_SpinUnlockRestore(lruLock, lruSave); // 释放LRU锁
    LOS_SpinUnlockRestore(&mapping->list_lock, intSave); // 释放映射锁
}

/**
 * @brief  删除映射信息
 * @param  region [in] 内存映射区域
 * @param  vmf [in] 缺页信息
 * @param  cleanDirty [in] 是否清除脏页标记
 * @return 无
 */
VOID OsDelMapInfo(LosVmMapRegion *region, LosVmPgFault *vmf, BOOL cleanDirty)
{
    UINT32 intSave;                    // 中断状态保存变量
    LosMapInfo *info = NULL;           // 映射信息指针
    LosFilePage *fpage = NULL;         // 文件页指针
    struct page_mapping *mapping = NULL; // 页面映射结构指针

    if (!LOS_IsRegionFileValid(region) || (region->unTypeData.rf.vnode == NULL) || (vmf == NULL)) {
        return;                        // 参数检查
    }

    mapping = &region->unTypeData.rf.vnode->mapping; // 获取映射
    LOS_SpinLockSave(&mapping->list_lock, &intSave); // 获取映射锁
    fpage = OsFindGetEntry(mapping, vmf->pgoff); // 查找文件页
    if (fpage == NULL) {
        LOS_SpinUnlockRestore(&mapping->list_lock, intSave); // 释放锁
        return;
    }

    if (cleanDirty) {
        OsCleanPageDirty(fpage->vmPage); // 清除脏页标记
    }
    // 获取映射信息
    info = OsGetMapInfo(fpage, &region->space->archMmu, (vaddr_t)vmf->vaddr);
    if (info != NULL) {
        fpage->n_maps--;               // 减少映射计数
        LOS_ListDelete(&info->node);   // 从链表删除
        LOS_AtomicDec(&fpage->vmPage->refCounts); // 减少引用计数
        LOS_SpinUnlockRestore(&mapping->list_lock, intSave); // 释放锁
        LOS_MemFree(m_aucSysMem0, info); // 释放映射信息
        return;
    }
    LOS_SpinUnlockRestore(&mapping->list_lock, intSave); // 释放锁
}

/**
 * @brief  文件映射缺页处理
 * @param  region [in] 内存映射区域
 * @param  vmf [in] 缺页信息
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
INT32 OsVmmFileFault(LosVmMapRegion *region, LosVmPgFault *vmf)
{
    INT32 ret;                         // 返回值
    VOID *kvaddr = NULL;               // 内核虚拟地址

    UINT32 intSave;                    // 中断状态保存变量
    bool newCache = false;             // 是否新缓存标志
    struct Vnode *vnode = NULL;        // Vnode结构指针
    struct page_mapping *mapping = NULL; // 页面映射结构指针
    LosFilePage *fpage = NULL;         // 文件页指针

    if (!LOS_IsRegionFileValid(region) || (region->unTypeData.rf.vnode == NULL) || (vmf == NULL)) {
        VM_ERR("Input param is NULL"); // 参数错误
        return LOS_NOK;
    }
    vnode = region->unTypeData.rf.vnode; // 获取Vnode
    mapping = &vnode->mapping;         // 获取映射

    /* get or create a new cache node */
    LOS_SpinLockSave(&mapping->list_lock, &intSave); // 获取映射锁
    fpage = OsFindGetEntry(mapping, vmf->pgoff); // 查找文件页
    TRACE_TRY_CACHE();                 // 增加缓存尝试计数
    if (fpage != NULL) {               // 找到缓存页
        TRACE_HIT_CACHE();             // 增加缓存命中计数
        OsPageRefIncLocked(fpage);     // 增加引用计数
    } else {                           // 未找到缓存页
        fpage = OsPageCacheAlloc(mapping, vmf->pgoff); // 分配新缓存页
        if (fpage == NULL) {
            LOS_SpinUnlockRestore(&mapping->list_lock, intSave); // 释放锁
            VM_ERR("Failed to alloc a page frame"); // 分配失败
            return LOS_NOK;
        }
        newCache = true;               // 设置新缓存标志
    }
    OsSetPageLocked(fpage->vmPage);    // 锁定页面
    LOS_SpinUnlockRestore(&mapping->list_lock, intSave); // 释放锁
    kvaddr = OsVmPageToVaddr(fpage->vmPage); // 获取内核虚拟地址

    /* read file to new page cache */
    if (newCache) {                    // 新缓存页需要读取文件
        ret = vnode->vop->ReadPage(vnode, kvaddr, fpage->pgoff << PAGE_SHIFT); // 读取页面
        if (ret == 0) {
            VM_ERR("Failed to read from file!"); // 读取失败
            OsReleaseFpage(mapping, fpage); // 释放页面
            return LOS_NOK;
        }
        LOS_SpinLockSave(&mapping->list_lock, &intSave); // 获取锁
        OsAddToPageacheLru(fpage, mapping, vmf->pgoff); // 添加到缓存和LRU
        LOS_SpinUnlockRestore(&mapping->list_lock, intSave); // 释放锁
    }

    LOS_SpinLockSave(&mapping->list_lock, &intSave); // 获取锁
    /* cow fault case no need to save mapinfo */
    // 检查是否为写时复制情况
    if (!((vmf->flags & VM_MAP_PF_FLAG_WRITE) && !(region->regionFlags & VM_MAP_REGION_FLAG_SHARED))) {
        // 添加映射信息
        OsAddMapInfo(fpage, &region->space->archMmu, (vaddr_t)vmf->vaddr);
        fpage->flags = region->regionFlags; // 设置标志
    }

    /* share page fault, mark the page dirty */
    // 共享页面写操作，标记为脏页
    if ((vmf->flags & VM_MAP_PF_FLAG_WRITE) && (region->regionFlags & VM_MAP_REGION_FLAG_SHARED)) {
        OsMarkPageDirty(fpage, region, 0, 0); // 标记脏页
    }

    vmf->pageKVaddr = kvaddr;          // 设置页面内核虚拟地址
    LOS_SpinUnlockRestore(&mapping->list_lock, intSave); // 释放锁
    return LOS_OK;
}

/**
 * @brief  刷新文件缓存
 * @param  mapping [in] 页面映射结构
 * @return 无
 */
VOID OsFileCacheFlush(struct page_mapping *mapping)
{
    UINT32 intSave;                    // 中断状态保存变量
    UINT32 lruLock;                    // LRU锁状态保存变量
    LOS_DL_LIST_HEAD(dirtyList);       // 脏页链表头
    LosFilePage *ftemp = NULL;         // 临时文件页指针
    LosFilePage *fpage = NULL;         // 文件页指针

    if (mapping == NULL) {
        return;                        // 参数检查
    }
    LOS_SpinLockSave(&mapping->list_lock, &intSave); // 获取映射锁
    // 遍历所有文件页
    LOS_DL_LIST_FOR_EACH_ENTRY(fpage, &mapping->page_list, LosFilePage, node) {
        LOS_SpinLockSave(&fpage->physSeg->lruLock, &lruLock); // 获取LRU锁
        if (OsIsPageDirty(fpage->vmPage)) { // 检查是否为脏页
            ftemp = OsDumpDirtyPage(fpage); // 转储脏页
            if (ftemp != NULL) {
                LOS_ListTailInsert(&dirtyList, &ftemp->node); // 添加到脏页链表
            }
        }
        LOS_SpinUnlockRestore(&fpage->physSeg->lruLock, lruLock); // 释放LRU锁
    }
    LOS_SpinUnlockRestore(&mapping->list_lock, intSave); // 释放映射锁

    // 遍历脏页链表并刷新
    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(fpage, ftemp, &dirtyList, LosFilePage, node) {
        OsDoFlushDirtyPage(fpage);     // 刷新脏页
    }
}


/**
 * @brief  删除文件缓存
 * @param  mapping [in] 页面映射结构
 * @return 无
 */
VOID OsFileCacheRemove(struct page_mapping *mapping)
{
    UINT32 intSave;                    // 中断状态保存变量
    UINT32 lruSave;                    // LRU锁状态保存变量
    SPIN_LOCK_S *lruLock = NULL;       // LRU锁指针
    LOS_DL_LIST_HEAD(dirtyList);       // 脏页链表头
    LosFilePage *ftemp = NULL;         // 临时文件页指针
    LosFilePage *fpage = NULL;         // 文件页指针
    LosFilePage *fnext = NULL;         // 下一个文件页指针

    LOS_SpinLockSave(&mapping->list_lock, &intSave); // 获取映射锁
    // 遍历所有文件页
    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(fpage, fnext, &mapping->page_list, LosFilePage, node) {
        lruLock = &fpage->physSeg->lruLock; // 获取LRU锁
        LOS_SpinLockSave(lruLock, &lruSave); // 加锁
        if (OsIsPageDirty(fpage->vmPage)) { // 检查脏页
            ftemp = OsDumpDirtyPage(fpage); // 转储脏页
            if (ftemp != NULL) {
                LOS_ListTailInsert(&dirtyList, &ftemp->node); // 添加到脏页链表
            }
        }

        OsDeletePageCacheLru(fpage);   // 删除缓存和LRU
        LOS_SpinUnlockRestore(lruLock, lruSave); // 释放LRU锁
    }
    LOS_SpinUnlockRestore(&mapping->list_lock, intSave); // 释放映射锁

    // 遍历脏页链表并刷新
    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(fpage, fnext, &dirtyList, LosFilePage, node) {
        OsDoFlushDirtyPage(fpage);     // 刷新脏页
    }
}

LosVmFileOps g_commVmOps = {          // 文件映射操作集
    .open = NULL,                     // 打开操作（未实现）
    .close = NULL,                    // 关闭操作（未实现）
    .fault = OsVmmFileFault,          // 缺页中断处理
    .remove = OsVmmFileRemove,        // 删除页操作
};

/**
 * @brief  文件映射设置
 * @param  filep [in] 文件指针
 * @param  region [in] 内存映射区域
 * @return 成功返回ENOERR，失败返回错误码
 */
INT32 OsVfsFileMmap(struct file *filep, LosVmMapRegion *region)
{
    region->unTypeData.rf.vmFOps = &g_commVmOps; // 设置文件操作集
    region->unTypeData.rf.vnode = filep->f_vnode; // 设置Vnode
    region->unTypeData.rf.f_oflags = filep->f_oflags; // 设置文件标志

    return ENOERR;                     // 返回成功
}

/**
 * @brief  有名映射（文件映射）
 * @param  filep [in] 文件指针
 * @param  region [in] 内存映射区域
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATUS_T OsNamedMMap(struct file *filep, LosVmMapRegion *region)
{
    struct Vnode *vnode = NULL;        // Vnode结构指针
    if (filep == NULL) {
        return LOS_ERRNO_VM_MAP_FAILED; // 参数错误
    }
    file_hold(filep);                  // 增加文件引用计数
    vnode = filep->f_vnode;            // 获取Vnode
    VnodeHold();                       // 持有Vnode
    vnode->useCount++;                 // 增加使用计数
    VnodeDrop();                       // 释放Vnode持有
    if (filep->ops != NULL && filep->ops->mmap != NULL) {
        // 检查是否为字符设备或块设备
        if (vnode->type == VNODE_TYPE_CHR || vnode->type == VNODE_TYPE_BLK) {
            LOS_SetRegionTypeDev(region); // 设置为设备类型
        } else {
            LOS_SetRegionTypeFile(region); // 设置为文件类型
        }
        int ret = filep->ops->mmap(filep, region); // 执行mmap操作
        if (ret != LOS_OK) {
            file_release(filep);       // 释放文件
            return LOS_ERRNO_VM_MAP_FAILED; // 返回失败
        }
    } else {
        VM_ERR("mmap file type unknown"); // 未知文件类型
        file_release(filep);           // 释放文件
        return LOS_ERRNO_VM_MAP_FAILED; // 返回失败
    }
    file_release(filep);               // 释放文件
    return LOS_OK;                     // 返回成功
}

/**************************************************************************************************
 通过位置从文件映射页中找到的指定的文件页
 举例:mapping->page_list上节点的数据可能只有是文件 1,3,4,6 页的数据,此时来找文件第5页的数据就会没有
**************************************************************************************************/
/**
 * @brief  查找文件页
 * @param  mapping [in] 页面映射结构
 * @param  pgoff [in] 页面偏移量
 * @return 成功返回文件页指针，失败返回NULL
 */
LosFilePage *OsFindGetEntry(struct page_mapping *mapping, VM_OFFSET_T pgoff)
{
    LosFilePage *fpage = NULL;         // 文件页指针

    LOS_DL_LIST_FOR_EACH_ENTRY(fpage, &mapping->page_list, LosFilePage, node) { // 遍历文件页
        if (fpage->pgoff == pgoff) {   // 找到指定偏移的页
            return fpage;              // 返回文件页
        }

        if (fpage->pgoff > pgoff) {    // 找到大于目标偏移的页
            break;                     // 跳出循环（链表按pgoff排序）
        }
    }

    return NULL;                       // 未找到返回NULL
}

/* need mutex & change memory to dma zone. */
/**
 * @brief  分配文件页
 * @param  mapping [in] 页面映射结构
 * @param  pgoff [in] 页面偏移量
 * @return 成功返回文件页指针，失败返回NULL
 */
LosFilePage *OsPageCacheAlloc(struct page_mapping *mapping, VM_OFFSET_T pgoff)
{
    VOID *kvaddr = NULL;               // 内核虚拟地址
    LosVmPhysSeg *physSeg = NULL;      // 物理内存段
    LosVmPage *vmPage = NULL;          // 虚拟内存页
    LosFilePage *fpage = NULL;         // 文件页

    vmPage = LOS_PhysPageAlloc();      // 分配物理页
    if (vmPage == NULL) {
        VM_ERR("alloc vm page failed"); // 分配失败
        return NULL;
    }
    physSeg = OsVmPhysSegGet(vmPage);  // 获取物理段
    kvaddr = OsVmPageToVaddr(vmPage);  // 获取内核虚拟地址
    if ((physSeg == NULL) || (kvaddr == NULL)) {
        LOS_PhysPageFree(vmPage);      // 释放物理页
        VM_ERR("alloc vm page failed!"); // 错误
        return NULL;
    }

    fpage = (LosFilePage *)LOS_MemAlloc(m_aucSysMem0, sizeof(LosFilePage)); // 分配文件页
    if (fpage == NULL) {
        LOS_PhysPageFree(vmPage);      // 释放物理页
        VM_ERR("Failed to allocate for page!"); // 分配失败
        return NULL;
    }

    (VOID)memset_s((VOID *)fpage, sizeof(LosFilePage), 0, sizeof(LosFilePage)); // 初始化

    LOS_ListInit(&fpage->i_mmap);      // 初始化映射链表
    LOS_ListInit(&fpage->node);        // 初始化节点
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
