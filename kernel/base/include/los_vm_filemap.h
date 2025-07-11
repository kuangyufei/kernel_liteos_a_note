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
/*!
 * @file    los_vm_filemap.h
 * @brief
 * @link
   @verbatim
    磁盘高速缓存是一种软件机制，它允许系统把通常存放在磁盘上的一些数据保留在 RAM 中，以便对那些数据的
    进一步访问不用再访问磁盘而能尽快得到满足。
    页高速缓存中的信息单位是一个完整的页。
    一个页包含的磁盘块在物理上不一定相邻，所以不能用设备号和块号标识，而是通过页的所有者和所有者数据中的索引来识别。
    页高速缓存可以缓存以下内容
	    A.普通文件数据
	    B.含有目录的页 
	    C.直接从快设备读取的页 
	    D.用户进程数据的页
	    E.特殊文件系统的文件页
   @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2025-07-11
 */
/**
 * @defgroup los_vm_filemap vm filemap definition
 * @ingroup kernel
 */

#ifndef __LOS_VM_FILEMAP_H__
#define __LOS_VM_FILEMAP_H__

#ifdef LOSCFG_FS_VFS
#include "fs/file.h"
#endif
#include "los_vm_map.h"
#include "los_vm_page.h"
#include "los_vm_common.h"
#include "los_vm_phys.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#if 0 //@note_#if0 
//page_mapping描述的是一个文件在内存中被映射了多少页,<文件,文件页的关系>
/* file mapped in VMM pages */
struct page_mapping {//记录文件页和文件关系的结构体,叫文件页映射
  LOS_DL_LIST                           page_list;    /* all pages | 链表上挂的是属于该文件的所有FilePage，这些页的内容都来源同一个文件*/
  SPIN_LOCK_S                           list_lock;    /* lock protecting it | 操作page_list的自旋锁*/
  LosMux                                mux_lock;     /* mutex lock | 操作page_mapping的互斥量*/
  unsigned long                         nrpages;      /* number of total pages |page_list的节点数量 */
  unsigned long                         flags;		  ///< @note_why 全量代码中也没查到源码中对其操作
  Atomic                                ref;          /* reference counting | 引用次数(自增/自减),对应add_mapping/dec_mapping*/
  struct Vnode                          *host;        /* owner of this mapping | 属于哪个文件的映射*/
};


/* map: full_path(owner) <-> mapping */ //叫文件映射
struct file_map { //为在内核层面文件在内存的身份证,每个需映射到内存的文件必须创建一个file_map，都挂到全局g_file_mapping链表上
  LOS_DL_LIST           head;		///< 链表节点,用于挂到g_file_mapping上
  LosMux                lock;         /* lock to protect this mapping */
  struct page_mapping   mapping;	///< 每个文件都有唯一的page_mapping标识其在内存的身份
  char                  *owner;     /* owner: full path of file | 文件全路径来标识唯一性*/
};

#endif

/**
 * @brief 文件页结构体，用于管理文件映射的物理页面
 * @core 维护页面映射关系、物理内存信息及状态标志，支持LRU页面置换算法
 * @attention 所有链表操作需通过LOS_DL_LIST接口进行，确保线程安全
 */
typedef struct FilePage {
    LOS_DL_LIST             node;           ///< 文件页双向链表节点，用于页面管理
    LOS_DL_LIST             lru;            ///< LRU(最近最少使用)链表节点，用于页面置换
    LOS_DL_LIST             i_mmap;         ///< 映射关系链表，挂载使用此页面的MapInfo
    UINT32                  n_maps;         ///< 映射计数，表示当前页面被多少进程映射
    struct VmPhysSeg        *physSeg;       ///< 物理内存段指针，指向页面所属的物理内存区域
    struct VmPage           *vmPage;        ///< 虚拟内存页面指针，关联对应的虚拟页表项
    struct page_mapping     *mapping;       ///< 页面映射结构指针，指向文件与内存的映射关系
    VM_OFFSET_T             pgoff;          ///< 页面偏移量，相对于文件起始位置的页偏移
    UINT32                  flags;          ///< 页面状态标志，组合OsPageFlags枚举值
    UINT16                  dirtyOff;       ///< 脏数据起始偏移，单位：字节（相对于页面起始）
    UINT16                  dirtyEnd;       ///< 脏数据结束偏移，单位：字节（相对于页面起始）
} LosFilePage;

/**
 * @brief 映射信息结构体，描述虚拟地址与物理页面的映射关系
 * @core 建立用户虚拟地址到文件物理页面的映射桥梁
 */
typedef struct MapInfo {
    LOS_DL_LIST             node;           ///< 映射信息链表节点，挂载到FilePage.i_mmap
    VADDR_T                 vaddr;          ///< 用户空间虚拟地址，映射的起始地址
    LosFilePage             *page;          ///< 文件页指针，指向对应的物理页面
    LosArchMmu              *archMmu;       ///< 架构相关MMU信息，包含页表项等硬件相关配置
} LosMapInfo;

/**
 * @brief 页面状态标志枚举
 * @note 可通过位或运算组合多个状态
 */
enum OsPageFlags {
    FILE_PAGE_FREE,          ///< 页面空闲，可分配
    FILE_PAGE_LOCKED,        ///< 页面锁定，禁止被置换
    FILE_PAGE_REFERENCED,    ///< 页面被引用，LRU算法中提升优先级
    FILE_PAGE_DIRTY,         ///< 页面脏标记，内容已修改需写回
    FILE_PAGE_LRU,           ///< 页面在LRU链表中
    FILE_PAGE_ACTIVE,        ///< 页面活跃，近期被访问过
    FILE_PAGE_SHARED,        ///< 页面共享，被多个进程映射
};

/**
 * @defgroup vm_filemap_macros 文件内存映射配置宏
 * @brief 控制文件页缓存行为的关键参数定义
 * @{
 */
#define PGOFF_MAX                       2000U   ///< 最大页面偏移量，限制单个文件映射的最大页数
#define MAX_SHRINK_PAGECACHE_TRY        2U      ///< 页缓存收缩最大尝试次数，防止过度回收
#define VM_FILEMAP_MAX_SCAN             (SYS_MEM_SIZE_DEFAULT >> PAGE_SHIFT)  ///< 最大扫描页面数 = 默认内存大小 / 页面大小
#define VM_FILEMAP_MIN_SCAN             32U     ///< 最小扫描页面数，即使内存充足也至少扫描32页
/** @} */
/**
 * @brief 设置页面锁定标志
 * @param page 指向LosVmPage结构体的指针，代表要操作的物理页面
 * @note 锁定的页面将被禁止换出，通常用于内核关键数据或正在IO操作的页面
 */
STATIC INLINE VOID OsSetPageLocked(LosVmPage *page)
{
    LOS_BitmapSet(&page->flags, FILE_PAGE_LOCKED); // 设置FILE_PAGE_LOCKED标志位(bit位置由宏定义)
}

/**
 * @brief 清除页面锁定标志
 * @param page 指向LosVmPage结构体的指针，代表要操作的物理页面
 * @note 解锁后页面可参与内存回收机制
 */
STATIC INLINE VOID OsCleanPageLocked(LosVmPage *page)
{
    LOS_BitmapClr(&page->flags, FILE_PAGE_LOCKED); // 清除FILE_PAGE_LOCKED标志位
}

/**
 * @brief 设置页面脏页标志
 * @param page 指向LosVmPage结构体的指针，代表要操作的物理页面
 * @note 脏页标志表示页面内容已被修改，需要写回后备存储
 */
STATIC INLINE VOID OsSetPageDirty(LosVmPage *page)
{
    LOS_BitmapSet(&page->flags, FILE_PAGE_DIRTY); // 设置FILE_PAGE_DIRTY标志位
}

/**
 * @brief 清除页面脏页标志
 * @param page 指向LosVmPage结构体的指针，代表要操作的物理页面
 * @note 通常在页面内容成功写回存储设备后调用
 */
STATIC INLINE VOID OsCleanPageDirty(LosVmPage *page)
{
    LOS_BitmapClr(&page->flags, FILE_PAGE_DIRTY); // 清除FILE_PAGE_DIRTY标志位
}

/**
 * @brief 设置页面活跃标志
 * @param page 指向LosVmPage结构体的指针，代表要操作的物理页面
 * @note 活跃页面在LRU回收算法中具有较低的被换出优先级
 */
STATIC INLINE VOID OsSetPageActive(LosVmPage *page)
{
    LOS_BitmapSet(&page->flags, FILE_PAGE_ACTIVE); // 设置FILE_PAGE_ACTIVE标志位
}

/**
 * @brief 清除页面活跃标志
 * @param page 指向LosVmPage结构体的指针，代表要操作的物理页面
 * @note 非活跃页面更容易被内存回收机制选中
 */
STATIC INLINE VOID OsCleanPageActive(LosVmPage *page)
{
    LOS_BitmapClr(&page->flags, FILE_PAGE_ACTIVE); // 清除FILE_PAGE_ACTIVE标志位
}

/**
 * @brief 设置页面LRU标志
 * @param page 指向LosVmPage结构体的指针，代表要操作的物理页面
 * @note 标记页面已加入LRU链表，参与页面置换算法
 */
STATIC INLINE VOID OsSetPageLRU(LosVmPage *page)
{
    LOS_BitmapSet(&page->flags, FILE_PAGE_LRU); // 设置FILE_PAGE_LRU标志位
}

/**
 * @brief 设置页面空闲标志
 * @param page 指向LosVmPage结构体的指针，代表要操作的物理页面
 * @note 空闲页面可被分配器用于满足新的内存分配请求
 */
STATIC INLINE VOID OsSetPageFree(LosVmPage *page)
{
    LOS_BitmapSet(&page->flags, FILE_PAGE_FREE); // 设置FILE_PAGE_FREE标志位
}

/**
 * @brief 清除页面空闲标志
 * @param page 指向LosVmPage结构体的指针，代表要操作的物理页面
 * @note 通常在页面被分配给进程或内核使用时调用
 */
STATIC INLINE VOID OsCleanPageFree(LosVmPage *page)
{
    LOS_BitmapClr(&page->flags, FILE_PAGE_FREE); // 清除FILE_PAGE_FREE标志位
}

/**
 * @brief 设置页面引用标志
 * @param page 指向LosVmPage结构体的指针，代表要操作的物理页面
 * @note 引用标志用于页面置换算法，记录页面最近被访问的情况
 */
STATIC INLINE VOID OsSetPageReferenced(LosVmPage *page)
{
    LOS_BitmapSet(&page->flags, FILE_PAGE_REFERENCED); // 设置FILE_PAGE_REFERENCED标志位
}

/**
 * @brief 清除页面引用标志
 * @param page 指向LosVmPage结构体的指针，代表要操作的物理页面
 * @note 通常在页面老化(ageing)过程中周期性清除
 */
STATIC INLINE VOID OsCleanPageReferenced(LosVmPage *page)
{
    LOS_BitmapClr(&page->flags, FILE_PAGE_REFERENCED); // 清除FILE_PAGE_REFERENCED标志位
}

/**
 * @brief 检查页面是否处于活跃状态
 * @param page 指向LosVmPage结构体的指针，代表要检查的物理页面
 * @return BOOL 页面活跃返回TRUE，否则返回FALSE
 */
STATIC INLINE BOOL OsIsPageActive(LosVmPage *page)
{
    return BIT_GET(page->flags, FILE_PAGE_ACTIVE); // 获取FILE_PAGE_ACTIVE标志位状态
}

/**
 * @brief 检查页面是否被锁定
 * @param page 指向LosVmPage结构体的指针，代表要检查的物理页面
 * @return BOOL 页面锁定返回TRUE，否则返回FALSE
 */
STATIC INLINE BOOL OsIsPageLocked(LosVmPage *page)
{
    return BIT_GET(page->flags, FILE_PAGE_LOCKED); // 获取FILE_PAGE_LOCKED标志位状态
}

/**
 * @brief 检查页面是否被引用
 * @param page 指向LosVmPage结构体的指针，代表要检查的物理页面
 * @return BOOL 页面被引用返回TRUE，否则返回FALSE
 */
STATIC INLINE BOOL OsIsPageReferenced(LosVmPage *page)
{
    return BIT_GET(page->flags, FILE_PAGE_REFERENCED); // 获取FILE_PAGE_REFERENCED标志位状态
}

/**
 * @brief 检查页面是否为脏页
 * @param page 指向LosVmPage结构体的指针，代表要检查的物理页面
 * @return BOOL 页面为脏页返回TRUE，否则返回FALSE
 */
STATIC INLINE BOOL OsIsPageDirty(LosVmPage *page)
{
    return BIT_GET(page->flags, FILE_PAGE_DIRTY); // 获取FILE_PAGE_DIRTY标志位状态
}

/**
 * @brief 检查文件页是否被映射
 * @param page 指向LosFilePage结构体的指针，代表要检查的文件页
 * @return BOOL 页面被映射返回TRUE(n_maps>0)，否则返回FALSE
 * @note 该函数通过检查映射计数判断，而非标志位
 */
STATIC INLINE BOOL OsIsPageMapped(LosFilePage *page)
{
    return (page->n_maps != 0); // 检查映射计数是否大于0
}

/* 以下三个函数用于共享内存(SHM)模块 */
/**
 * @brief 设置页面共享标志
 * @param page 指向LosVmPage结构体的指针，代表要操作的物理页面
 * @note 共享页面可被多个进程映射，用于进程间通信
 */
STATIC INLINE VOID OsSetPageShared(LosVmPage *page)
{
    LOS_BitmapSet(&page->flags, FILE_PAGE_SHARED); // 设置FILE_PAGE_SHARED标志位
}

/**
 * @brief 清除页面共享标志
 * @param page 指向LosVmPage结构体的指针，代表要操作的物理页面
 * @note 通常在最后一个共享进程解除映射时调用
 */
STATIC INLINE VOID OsCleanPageShared(LosVmPage *page)
{
    LOS_BitmapClr(&page->flags, FILE_PAGE_SHARED); // 清除FILE_PAGE_SHARED标志位
}

/**
 * @brief 检查页面是否为共享页面
 * @param page 指向LosVmPage结构体的指针，代表要检查的物理页面
 * @return BOOL 页面共享返回TRUE，否则返回FALSE
 */
STATIC INLINE BOOL OsIsPageShared(LosVmPage *page)
{
    return BIT_GET(page->flags, FILE_PAGE_SHARED); // 获取FILE_PAGE_SHARED标志位状态
}
typedef struct ProcessCB LosProcessCB;

#ifdef LOSCFG_FS_VFS
INT32 OsVfsFileMmap(struct file *filep, LosVmMapRegion *region);
STATUS_T OsNamedMMap(struct file *filep, LosVmMapRegion *region);
VOID OsVmmFileRegionFree(struct file *filep, LosProcessCB *processCB);
#endif

LosFilePage *OsPageCacheAlloc(struct page_mapping *mapping, VM_OFFSET_T pgoff);
LosFilePage *OsFindGetEntry(struct page_mapping *mapping, VM_OFFSET_T pgoff);
LosMapInfo *OsGetMapInfo(const LosFilePage *page, const LosArchMmu *archMmu, VADDR_T vaddr);
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
VOID OsPageRefDecNoLock(LosFilePage *page);
VOID OsPageRefIncLocked(LosFilePage *page);
int OsTryShrinkMemory(size_t nPage);
VOID OsMarkPageDirty(LosFilePage *fpage, const LosVmMapRegion *region, int off, int len);

#ifdef LOSCFG_DEBUG_VERSION
VOID ResetPageCacheHitInfo(int *try, int *hit);
struct file_map* GetFileMappingList(void);
#endif
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* __LOS_VM_FILEMAP_H__ */

