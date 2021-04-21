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


#ifdef LOSCFG_KERNEL_VM

/**************************************************************************************************
增加文件页到页高速缓存(page cache)
LosFilePage将一个文件切成了一页一页,因为读文件过程随机seek,所以文件页也不会是连续的,
pgoff记录文件的位置,并确保在cache的文件数据是按顺序排列的.
**************************************************************************************************/ 
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
//将页面加到活动文件页LRU链表上
VOID OsAddToPageacheLru(LosFilePage *page, struct page_mapping *mapping, VM_OFFSET_T pgoff)
{
    OsPageCacheAdd(page, mapping, pgoff);
    OsLruCacheAdd(page, VM_LRU_ACTIVE_FILE);
}
//从页高速缓存上删除页
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
//通过虚拟地址获取文件页映射信息,archMmu每个进程都有属于自己的mmu
LosMapInfo *OsGetMapInfo(LosFilePage *page, LosArchMmu *archMmu, VADDR_T vaddr)
{
    LosMapInfo *info = NULL;
    LOS_DL_LIST *immap = &page->i_mmap;//一个文件页被多个进程映射

    LOS_DL_LIST_FOR_EACH_ENTRY(info, immap, LosMapInfo, node) {//遍历每个节点
        if ((info->archMmu == archMmu) && (info->vaddr == vaddr) && (info->page == page)) {//全等时返回
            return info;
        }
    }
    return NULL;
}
//删除页高速缓存和LRU,对应 OsAddToPageacheLru
VOID OsDeletePageCacheLru(LosFilePage *page)
{
    /* delete form lru list */
    OsLruCacheDel(page);	//将页面从lru列表中删除
    /* delete from cache lits and free pmm if need */
    OsPageCacheDel(page); //从page缓存中删除
}
/**************************************************************************************************
 找到文件filp的pgOff位置在内核空间的虚拟地址,并填充文件数据,参数带回 kvaddr 和 readSize
**************************************************************************************************/
#if VFS_IMPL_LATER
STATIC LosFilePage *OsPagecacheGetPageAndFill(struct file *filp, VM_OFFSET_T pgOff, size_t *readSize, VADDR_T *kvaddr)
{
    LosFilePage *page = NULL;
    struct page_mapping *mapping = filp->f_mapping;//找到文件和文件页的映射关系结构体

    page = OsFindGetEntry(mapping, pgOff);//通过位置找到对应的文件页
    if (page != NULL) {//找到了
        OsSetPageLocked(page->vmPage);//因为一个文件页可能多个进程同时访问,所以操作之前必须先锁
        OsPageRefIncLocked(page);
        *kvaddr = (VADDR_T)(UINTPTR)OsVmPageToVaddr(page->vmPage);//找到物理地址在内核空间上的虚拟地址
        *readSize = PAGE_SIZE;//读一页数据
    } else {//没找到
        page = OsPageCacheAlloc(mapping, pgOff);//分配一个文件页
        if (page == NULL) {
            VM_ERR("Failed to alloc a page frame");
            return page;
        }
        OsSetPageLocked(page->vmPage);//锁页
        *kvaddr = (VADDR_T)(UINTPTR)OsVmPageToVaddr(page->vmPage);//获得内核虚拟地址

        file_seek(filp, pgOff << PAGE_SHIFT, SEEK_SET);//以4K为单位读数据
        /* "ReadPage" func exists definitely in this procedure */ //i_mops为 mount驱动程序,从文件中读一页数据到内核地址空间
        *readSize = filp->f_vnode->u.i_mops->readpage(filp, (char *)(UINTPTR)*kvaddr, PAGE_SIZE);
        if (*readSize == 0) {//读取失败怎么处理?
            VM_ERR("read 0 bytes");
            OsCleanPageLocked(page->vmPage);//释放页锁
        }
        OsAddToPageacheLru(page, mapping, pgOff);
    }

    return page;
}
/******************************************************************************
 从文件filp中读 size 数据到buf中
******************************************************************************/
ssize_t OsMappingRead(struct file *filp, char *buf, size_t size)
{
    INT32 ret;
    vaddr_t kvaddr = 0;
    UINT32 intSave;
    struct stat bufStat;
    size_t readSize = 0;
    size_t readTotal = 0;
    size_t readLeft = size;
    LosFilePage *page = NULL;
    VM_OFFSET_T pos = file_seek(filp, 0, SEEK_CUR);//从哪个位置开始读文件
    VM_OFFSET_T pgOff = pos >> PAGE_SHIFT;
    INT32 offInPage = pos % PAGE_SIZE;
    struct page_mapping *mapping = filp->f_mapping;
    INT32 nPages = (ROUNDUP(pos + size, PAGE_SIZE) - ROUNDDOWN(pos, PAGE_SIZE)) >> PAGE_SHIFT;//计算出要读多少页数据

    ret = stat(filp->f_path, &bufStat);//先查下文件状态是否正常,比如文件是否还存在
    if (ret != OK) {
        VM_ERR("Get file size failed. (filepath=%s)", filp->f_path);
        return 0;
    }

    if (pos >= bufStat.st_size) {//要读取的位置不能超过文件总大小
        PRINT_INFO("%s filp->f_pos >= bufStat.st_size (pos=%ld, fileSize=%ld)\n", filp->f_path, pos, bufStat.st_size);
        return 0;
    }

    LOS_SpinLockSave(&mapping->list_lock, &intSave);//一定要用自旋锁,因为多CPU可能同时操作

    for (INT32 i = 0; (i < nPages) && readLeft; i++, pgOff++) {//一页一页读
        page = OsPagecacheGetPageAndFill(filp, pgOff, &readSize, &kvaddr);//按页读
        if ((page == NULL) || (readSize == 0)) {
            break;
        }
        if (readSize < PAGE_SIZE) {//当读的数据还不要一页时
            readLeft = readSize;
        }

        readSize = MIN2((PAGE_SIZE - offInPage), readLeft);

        (VOID)memcpy_s((VOID *)buf, readLeft, (char *)(UINTPTR)kvaddr + offInPage, readSize);
        buf += readSize;
        readLeft -= readSize;
        readTotal += readSize;

        offInPage = 0;

        OsCleanPageLocked(page->vmPage);
    }

    LOS_SpinUnlockRestore(&mapping->list_lock, intSave);
    file_seek(filp, pos + readTotal, SEEK_SET);//文件要跳到读取后的位置

    return readTotal;
}
#endif
/******************************************************************************
 向页高速缓存中写入数据
******************************************************************************/
ssize_t OsMappingWrite(struct file *filp, const char *buf, size_t size)
{
    VADDR_T kvaddr;
    UINT32 intSave;
    INT32 writeSize = 0;
    size_t writeLeft = size;
    VM_OFFSET_T pos = file_seek(filp, 0, SEEK_CUR);//获得要从文件那个位置写入
    VM_OFFSET_T pgOff = pos >> PAGE_SHIFT;	//计算出文件页的序号
    INT32 offInPage = pos % PAGE_SIZE;		//计算出文件页内偏移量
    LosFilePage *page = NULL;
    struct page_mapping *mapping = filp->f_mapping;//得到文件页映射
    INT32 nPages = (ROUNDUP(pos + size, PAGE_SIZE) - ROUNDDOWN(pos, PAGE_SIZE)) >> PAGE_SHIFT;

    LOS_SpinLockSave(&mapping->list_lock, &intSave);

    for (INT32 i = 0; i < nPages; i++, pgOff++) {//遍历pgOff,pgOff表示页序号
        page = OsFindGetEntry(mapping, pgOff);//通过页序号拿到页高速缓存中的文件页
        if (page) {//页高速缓存中有该页
            kvaddr = (VADDR_T)(UINTPTR)OsVmPageToVaddr(page->vmPage);//通过文件页获得虚拟地址(即数据写入地址)
            OsSetPageLocked(page->vmPage);//写操作要上锁
            OsPageRefIncLocked(page);//因为页可能被其他进程所引用,所以也要上锁	 
        } else {//页高速缓存中没有该页
            page = OsPageCacheAlloc(mapping, pgOff);//分配一个文件页
            if (page == NULL) {
                VM_ERR("Failed to alloc a page frame");
                break;
            }
            kvaddr = (VADDR_T)(UINTPTR)OsVmPageToVaddr(page->vmPage);//通过文件页获得虚拟地址(即数据写入地址)
            OsAddToPageacheLru(page, mapping, pgOff);//将页加入页高速缓存中
            OsSetPageLocked(page->vmPage);//写操作要上锁,因为是新页,所以不要 OsPageRefIncLocked
        }
		//准备工作做好了,才是写入页高速缓存,注意这里只是写缓存,而不是写最终的文件
        writeSize = MIN2((PAGE_SIZE - offInPage), writeLeft);//计算出要写入的大小

        (VOID)memcpy_s((char *)(UINTPTR)kvaddr + offInPage, writeLeft, buf, writeSize);//将内容拷贝到页高速缓存
        buf += writeSize;		//计算出剩余未写完buf位置
        writeLeft -= writeSize;	//计算出剩余大小

        OsMarkPageDirty(page, NULL, offInPage, writeSize);//标记文件页为脏页,从offInPage位置开始脏的.
		//注意这里只是做标记,内核并不会马上将数据写入磁盘文件,而是在缺页时写入磁盘,就是著名的 写时拷贝技术.
        offInPage = 0;//脏位置复位

        OsCleanPageLocked(page->vmPage);//释放文件页锁
    }

    LOS_SpinUnlockRestore(&mapping->list_lock, intSave);

    file_seek(filp, pos + size - writeLeft, SEEK_SET);//最后文件要seek到指定的位置
    return (size - writeLeft);
}
//解除文件页和进程的映射关系
STATIC VOID OsPageCacheUnmap(LosFilePage *fpage, LosArchMmu *archMmu, VADDR_T vaddr)
{
    UINT32 intSave;
    LosMapInfo *info = NULL;

    LOS_SpinLockSave(&fpage->physSeg->lruLock, &intSave);
    info = OsGetMapInfo(fpage, archMmu, vaddr);//获取文件页在进程的映射信息
    if (info == NULL) {
        VM_ERR("OsPageCacheUnmap get map info fail!");
    } else {
        OsUnmapPageLocked(fpage, info);//解除进程和文件页映射关系
    }
    if (!(OsIsPageMapped(fpage) && ((fpage->flags & VM_MAP_REGION_FLAG_PERM_EXECUTE) ||
        OsIsPageDirty(fpage->vmPage)))) {
        OsPageRefDecNoLock(fpage);
    }

    LOS_SpinUnlockRestore(&fpage->physSeg->lruLock, intSave);
}
//删除文件
VOID OsVmmFileRemove(LosVmMapRegion *region, LosArchMmu *archMmu, VM_OFFSET_T pgoff)
{
    UINT32 intSave;
    vaddr_t vaddr;
    paddr_t paddr = 0;
    struct file *file = NULL;
    struct page_mapping *mapping = NULL;
    LosFilePage *fpage = NULL;
    LosFilePage *tmpPage = NULL;
    LosVmPage *mapPage = NULL;

    if (!LOS_IsRegionFileValid(region) || (region->unTypeData.rf.file->f_mapping == NULL)) {
        return;//判断是否为文件映射，是否已map
    }
    file = region->unTypeData.rf.file;
    mapping = file->f_mapping;
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
//标记page为脏页 进程修改了高速缓存里的数据时，该页就被内核标记为脏页
VOID OsMarkPageDirty(LosFilePage *fpage, LosVmMapRegion *region, INT32 off, INT32 len)
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

STATIC UINT32 GetDirtySize(LosFilePage *fpage, struct file *file)
{
    UINT32 fileSize;
    UINT32 dirtyBegin;
    UINT32 dirtyEnd;
    struct stat buf_stat;

    if (stat(file->f_path, &buf_stat) != OK) {
        VM_ERR("FlushDirtyPage get file size failed. (filepath=%s)", file->f_path);
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
//冲洗脏页，回写磁盘
STATIC INT32 OsFlushDirtyPage(LosFilePage *fpage)
{
    UINT32 ret;
    size_t len;
    char *buff = NULL;
    VM_OFFSET_T oldPos;
    struct file *file = fpage->mapping->host;/* owner of this mapping */ //此映射属于哪个文件,注意<file,page_mapping>是1:1的关系.
    if ((file == NULL) || (file->f_vnode == NULL)) {
        VM_ERR("page cache file error");
        return LOS_NOK;
    }

    oldPos = file_seek(file, 0, SEEK_CUR);////先记录老位置,因为写完页数据后要seek回老位置
    buff = (char *)OsVmPageToVaddr(fpage->vmPage);//获取页面的虚拟地址
    file_seek(file, (((UINT32)fpage->pgoff << PAGE_SHIFT) + fpage->dirtyOff), SEEK_SET);//移到页面脏数据位置，注意不是整个页面都脏了，可能只脏了一部分
    len = fpage->dirtyEnd - fpage->dirtyOff;//计算出脏数据长度
    len = (len == 0) ? GetDirtySize(fpage, file) : len;
    if (len == 0) {//没有脏数据
        OsCleanPageDirty(fpage->vmPage);//页面取消脏标签
        (VOID)file_seek(file, oldPos, SEEK_SET);//移回老位置
        return LOS_OK;
    }
	//
    ret = file_write(file, (VOID *)buff, len);
    if (ret <= 0) {
        VM_ERR("WritePage error ret %d", ret);
    }
    ret = (ret <= 0) ? LOS_NOK : LOS_OK;
    OsCleanPageDirty(fpage->vmPage);//撕掉脏页标签，还清白之身,哈哈,臣妾又干净了.
    (VOID)file_seek(file, oldPos, SEEK_SET);//不管是否写正常,一定要切回到原来位置,很重要!!! 写页是内核的操作,不是用户的操作行为

    return ret;
}
//备份脏页,老脏页撕掉脏页标签
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
//冲洗脏页数据,将脏页数据回写磁盘
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
//删除映射信息
VOID OsDelMapInfo(LosVmMapRegion *region, LosVmPgFault *vmf, BOOL cleanDirty)
{
    UINT32 intSave;
    LosMapInfo *info = NULL;
    LosFilePage *fpage = NULL;

    if (!LOS_IsRegionFileValid(region) || (region->unTypeData.rf.file->f_mapping == NULL) || (vmf == NULL)) {//参数检查
        return;
    }

    LOS_SpinLockSave(&region->unTypeData.rf.file->f_mapping->list_lock, &intSave);
    fpage = OsFindGetEntry(region->unTypeData.rf.file->f_mapping, vmf->pgoff);//找到文件页
    if (fpage == NULL) {
        LOS_SpinUnlockRestore(&region->unTypeData.rf.file->f_mapping->list_lock, intSave);
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
        LOS_SpinUnlockRestore(&region->unTypeData.rf.file->f_mapping->list_lock, intSave);
        LOS_MemFree(m_aucSysMem0, info);
        return;
    }
    LOS_SpinUnlockRestore(&region->unTypeData.rf.file->f_mapping->list_lock, intSave);
}
/**************************************************************************************************
文件缺页时的处理,先读入磁盘数据，再重新读页数据
被 OsDoReadFault(...),OsDoCowFault(...),OsDoSharedFault(...) 等调用
**************************************************************************************************/
INT32 OsVmmFileFault(LosVmMapRegion *region, LosVmPgFault *vmf)
{
    INT32 ret;
    VM_OFFSET_T oldPos;
    VOID *kvaddr = NULL;

    UINT32 intSave;
    bool newCache = false;
    struct file *file = NULL;
    struct page_mapping *mapping = NULL;
    LosFilePage *fpage = NULL;

    if (!LOS_IsRegionFileValid(region) || (region->unTypeData.rf.file->f_mapping == NULL) || (vmf == NULL)) {//文件是否映射到了内存
        VM_ERR("Input param is NULL");
        return LOS_NOK;
    }
    file = region->unTypeData.rf.file;
    mapping = file->f_mapping;

    /* get or create a new cache node */
    LOS_SpinLockSave(&mapping->list_lock, &intSave);
    fpage = OsFindGetEntry(mapping, vmf->pgoff);//获取文件页
    if (fpage != NULL) {//找到了,说明该页已经在页高速缓存中
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
        oldPos = file_seek(file, 0, SEEK_CUR);//先记录老位置,因为读完页数据后要seek回老位置
        file_seek(file, fpage->pgoff << PAGE_SHIFT, SEEK_SET);//定位到要开始读的位置,从fpage->pgoff << PAGE_SHIFT 文件位置开始读.
        ret = file_read(file, kvaddr, PAGE_SIZE);//将磁盘4K数据读到物理页框的kvaddr地址中,真正写到了fpage->vmPage->physAddr中.
        file_seek(file, oldPos, SEEK_SET);//不管是否读写正常,一定要切回到原来位置,很重要!!! 因为缺页->读页是内核的操作,不是用户的操作行为
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
//文件缓存冲洗,把所有fpage冲洗一边，把脏页洗到dirtyList中,配合OsFileCacheRemove理解 
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

LosVmFileOps g_commVmOps = {//
    .open = NULL,
    .close = NULL,
    .fault = OsVmmFileFault, //缺页中断处理
    .remove = OsVmmFileRemove,//删除页
};

INT32 OsVfsFileMmap(struct file *filep, LosVmMapRegion *region)
{
    region->unTypeData.rf.vmFOps = &g_commVmOps;//文件操作
    region->unTypeData.rf.file = filep; //文件描述信息
    region->unTypeData.rf.fileMagic = filep->f_magicnum;//magic数
    return ENOERR;
}
/******************************************************************************
 有名映射,跟匿名映射相对应
 参数filep是广义的文件,在鸿蒙内核,目录/普通文件/字符设备/块设备/网络套接字/管道/链接 都是文件
******************************************************************************/
STATUS_T OsNamedMMap(struct file *filep, LosVmMapRegion *region)
{
    struct Vnode *vnode = NULL;
    if (filep == NULL) {
        return LOS_ERRNO_VM_MAP_FAILED;
    }
    vnode = filep->f_vnode;
    if (vnode == NULL) {
        return LOS_ERRNO_VM_MAP_FAILED;
    }

    if (filep->ops != NULL && filep->ops->mmap != NULL) {
        if (vnode->type == VNODE_TYPE_CHR || vnode->type == VNODE_TYPE_BLK) {
            LOS_SetRegionTypeDev(region);
        } else {
            LOS_SetRegionTypeFile(region);
        }
        int ret = filep->ops->mmap(filep, region);
        if (ret != LOS_OK) {
            return LOS_ERRNO_VM_MAP_FAILED;
        }
    } else {
        VM_ERR("mmap file type unknown");
        return LOS_ERRNO_VM_MAP_FAILED;
    }
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
/**************************************************************************************************
以页高速缓存方式分配一个文件页 LosFilePage
 Direct Memory Access（存储器直接访问）指一种高速的数据传输操作，允许在外部设备和存储器之间直接读写数据。
 整个数据传输操作在一个称为"DMA控制器"的控制下进行的。CPU只需在数据传输开始和结束时做一点处理（开始和结束时候要做中断处理）
**************************************************************************************************/
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

#ifdef LOSCFG_FS_VFS
VOID OsVmmFileRegionFree(struct file *filep, LosProcessCB *processCB)//释放进程某个文件的线性区
{
    int ret;
    LosVmSpace *space = NULL;
    LosVmMapRegion *region = NULL;
    LosRbNode *pstRbNode = NULL;
    LosRbNode *pstRbNodeTmp = NULL;

    if (processCB == NULL) {
        processCB = OsCurrProcessGet();
    }

    space = processCB->vmSpace;//拿到进程空间
    if (space != NULL) {
        (VOID)LOS_MuxAcquire(&space->regionMux);
        /* free the regions associated with filep */ //释放与filep关联的区域
        RB_SCAN_SAFE(&space->regionRbTree, pstRbNode, pstRbNodeTmp) //红黑树遍历线性区
            region = (LosVmMapRegion *)pstRbNode;
            if (LOS_IsRegionFileValid(region)) {//是否为一个有效的文件线性区
                if (region->unTypeData.rf.file != filep) {//一个文件对应一个线性区
                    continue;
                }
                ret = LOS_RegionFree(space, region);//释放线性区
                if (ret != LOS_OK) {
                    VM_ERR("free region error, space %p, region %p", space, region);
                }
            }
        RB_SCAN_SAFE_END(&space->regionRbTree, pstRbNode, pstRbNodeTmp)//遍历结束
        (VOID)LOS_MuxRelease(&space->regionMux);
    }
}
#endif
#else
INT32 OsVfsFileMmap(struct file *filep, LosVmMapRegion *region)
{
    UNUSED(filep);
    UNUSED(region);
    return ENOERR;
}
#endif

