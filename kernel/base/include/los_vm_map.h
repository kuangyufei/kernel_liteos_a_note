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
 * @defgroup los_vm_map vm mapping management
 * @ingroup kernel
 */

#ifndef __LOS_VM_MAP_H__
#define __LOS_VM_MAP_H__

#include "los_typedef.h"
#include "los_arch_mmu.h"
#include "los_mux.h"
#include "los_rbtree.h"
#include "los_vm_syscall.h"
#include "los_vm_zone.h"
#include "los_vm_common.h"

struct Vnode;
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
#if 0 // @note_#if0
file结构体来自 ..\third_party\NuttX\include\nuttx\fs\fs.h
struct file //打开文件的基本表示形式
{
  unsigned int         f_magicnum;  /* file magic number */
  int                  f_oflags;    /* Open mode flags */
  FAR struct inode     *f_inode;    /* Driver interface */
  loff_t               f_pos;       /* File position */
  unsigned long        f_refcount;  /* reference count */
  char                 *f_path;     /* File fullpath */
  void                 *f_priv;     /* Per file driver private data */
  const char           *f_relpath;  /* realpath */
  struct page_mapping  *f_mapping;  /* mapping file to memory */
  void                 *f_dir;      /* DIR struct for iterate the directory if open a directory */
};

struct page_mapping {
  LOS_DL_LIST                           page_list;    /* all pages */
  SPIN_LOCK_S                           list_lock;    /* lock protecting it */
  LosMux                                mux_lock;     /* mutex lock */
  unsigned long                         nrpages;      /* number of total pages */
  unsigned long                         flags;
  Atomic                                ref;          /* reference counting */
  struct file                           *host;        /* owner of this mapping */
};
#endif
/* If the kernel malloc size is less than 16k, use heap, otherwise use physical pages */
#define KMALLOC_LARGE_SIZE    (PAGE_SIZE << 2)
typedef struct VmMapRange {
    VADDR_T             base;           /**< vm region base addr | 线性区基地址*/
    UINT32              size;           /**< vm region size | 线性区大小*/
} LosVmMapRange;

struct VmMapRegion;
typedef struct VmMapRegion LosVmMapRegion;
struct VmFileOps;
typedef struct VmFileOps LosVmFileOps;
struct VmSpace;
typedef struct VmSpace LosVmSpace;
/// 缺页结构信息体 
typedef struct VmFault {
    UINT32          flags;              /*! FAULT_FLAG_xxx flags | 缺页标识*/
    unsigned long   pgoff;              /*! Logical page offset based on region | 基于线性区的逻辑页偏移量*/
    VADDR_T         vaddr;              /*! Faulting virtual address | 产生缺页的虚拟地址*/
    VADDR_T         *pageKVaddr;        /*! KVaddr of pagefault's vm page's paddr | page cache中的虚拟地址*/
} LosVmPgFault;
/// 虚拟内存文件操作函数指针,上层开发可理解为 class 里的方法，注意是对线性区的操作 , 文件操作 见于g_commVmOps
struct VmFileOps {
    void (*open)(struct VmMapRegion *region); ///< 打开
    void (*close)(struct VmMapRegion *region); ///< 关闭
    int  (*fault)(struct VmMapRegion *region, LosVmPgFault *pageFault); ///< 缺页,OsVmmFileFault
    void (*remove)(struct VmMapRegion *region, LosArchMmu *archMmu, VM_OFFSET_T offset); ///< 删除 OsVmmFileRemove
};

/*!
	线性区描述符,内核通过线性区管理虚拟地址,而线性地址就是虚拟地址
	在内核里,用户空间的进程要访问内存或磁盘里的数据要通过映射的方式将内存的物理地址和用户空间的虚拟地址联系起来. 
	用户通过访问这样的虚拟地址就可以访问到实际的物理地址,也就是实际的物理内存. 映射在实现虚拟地址到物理地址中扮演
	重要角色. 内核中映射分为文件映射和匿名映射. 文件映射就是磁盘中的数据通过文件系统映射到内存再通过文件映射映射到
	虚拟空间.这样,用户就可以在用户空间通过 open ,read, write 等函数区操作文件内容. 匿名映射就是用户空间需要分配一定
	的物理内存来存储数据,这部分内存不属于任何文件,内核就使用匿名映射将内存中的 某段物理地址与用户空间一一映射,
	这样用户就可用直接操作虚拟地址来范围这段物理内存. 至于实际的代码,文件映射的操作就是: open,read,write,close,mmap...
	操作的虚拟地址都属于文件映射. malloc 分配的虚拟地址属于匿名映射. 
*/
struct VmMapRegion {
    LosRbNode           rbNode;         /**< region red-black tree node | 红黑树节点,通过它将本线性区挂在VmSpace.regionRbTree*/
    LosVmSpace          *space;			///< 所属虚拟空间,虚拟空间由多个线性区组成
    LOS_DL_LIST         node;           /**< region dl list | 链表节点,通过它将本线性区挂在VmSpace.regions上*/				
    LosVmMapRange       range;          /**< region address range | 记录线性区的范围*/
    VM_OFFSET_T         pgOff;          /**< region page offset to file | 以文件开始处的偏移量, 必须是分页大小的整数倍, 通常为0, 表示从文件头开始映射。*/
    UINT32              regionFlags;   /**< region flags: cow, user_wired | 线性区标签*/
    UINT32              shmid;          /**< shmid about shared region | shmid为共享线性区id,id背后就是共享线性区*/
    UINT8               forkFlags;      /**< vm space fork flags: COPY, ZERO, | 线性区标记方式*/
    UINT8               regionType;     /**< vm region type: ANON, FILE, DEV | 映射类型是匿名,文件,还是设备,所谓匿名可理解为内存映射*/
	union {
        struct VmRegionFile {// <磁盘文件 , 物理内存, 用户进程虚拟地址空间 > 
            int f_oflags;
            struct Vnode *vnode;
            const LosVmFileOps *vmFOps;///< 文件处理各操作接口,open,read,write,close,mmap
        } rf;
		//匿名映射是指那些没有关联到文件页，如进程堆、栈、数据段和任务已修改的共享库等与物理内存的映射
        struct VmRegionAnon {//<swap区 , 物理内存, 用户进程虚拟地址空间 > 
            LOS_DL_LIST  node;          /**< region LosVmPage list | 线性区虚拟页链表*/
        } ra;
        struct VmRegionDev {//设备映射,也是一种文件
            LOS_DL_LIST  node;          /**< region LosVmPage list | 线性区虚拟页链表*/
            const LosVmFileOps *vmFOps; ///< 操作设备像操作文件一样方便.
        } rd;
    } unTypeData;
};
/// 进程空间,每个进程都有一个属于自己的虚拟内存地址空间
typedef struct VmSpace {
    LOS_DL_LIST         node;           /**< vm space dl list | 节点,通过它挂到全局虚拟空间 g_vmSpaceList 链表上*/
    LosRbTree           regionRbTree;   /**< region red-black tree root | 采用红黑树方式管理本空间各个线性区*/
    LosMux              regionMux;      /**< region list mutex lock | 虚拟空间的互斥锁*/
    VADDR_T             base;           /**< vm space base addr | 虚拟空间的基地址,常用于判断地址是否在内核还是用户空间*/
    UINT32              size;           /**< vm space size | 虚拟空间大小*/
    VADDR_T             heapBase;       /**< vm space heap base address | 堆区基地址，表堆区范围起点*/
    VADDR_T             heapNow;        /**< vm space heap base now | 堆区现地址，表堆区范围终点，do_brk()直接修改堆的大小返回新的堆区结束地址， heapNow >= heapBase*/
    LosVmMapRegion      *heap;          /**< heap region | 堆区是个特殊的线性区，用于满足进程的动态内存需求，大家熟知的malloc,realloc,free其实就是在操作这个区*/				
    VADDR_T             mapBase;        /**< vm space mapping area base | 虚拟空间映射区基地址,L1，L2表存放在这个区 */
    UINT32              mapSize;        /**< vm space mapping area size | 虚拟空间映射区大小，映射区是个很大的区。*/
    LosArchMmu          archMmu;        /**< vm mapping physical memory | MMU记录<虚拟地址,物理地址>的映射情况 */
#ifdef LOSCFG_DRIVERS_TZDRIVER
    VADDR_T             codeStart;      /**< user process code area start | 代码区开始位置 */
    VADDR_T             codeEnd;        /**< user process code area end | 代码区结束位置 */
#endif
} LosVmSpace;

#define     VM_MAP_REGION_TYPE_NONE                 (0x0) ///< 初始化使用
#define     VM_MAP_REGION_TYPE_ANON                 (0x1) ///< 匿名映射线性区
#define     VM_MAP_REGION_TYPE_FILE                 (0x2) ///< 文件映射线性区
#define     VM_MAP_REGION_TYPE_DEV                  (0x4) ///< 设备映射线性区
#define     VM_MAP_REGION_TYPE_MASK                 (0x7) ///< 映射线性区掩码

/* the high 8 bits(24~31) should reserved, shm will use it */
#define     VM_MAP_REGION_FLAG_CACHED               (0<<0)		///< 缓冲区
#define     VM_MAP_REGION_FLAG_UNCACHED             (1<<0)		///< 非缓冲区
#define     VM_MAP_REGION_FLAG_UNCACHED_DEVICE      (2<<0) /* only exists on some arches, otherwise UNCACHED */
#define     VM_MAP_REGION_FLAG_STRONGLY_ORDERED     (3<<0) /* only exists on some arches, otherwise UNCACHED */
#define     VM_MAP_REGION_FLAG_CACHE_MASK           (3<<0)		///< 缓冲区掩码
#define     VM_MAP_REGION_FLAG_PERM_USER            (1<<2)		///< 用户空间区
#define     VM_MAP_REGION_FLAG_PERM_READ            (1<<3)		///< 可读取区
#define     VM_MAP_REGION_FLAG_PERM_WRITE           (1<<4)		///< 可写入区
#define     VM_MAP_REGION_FLAG_PERM_EXECUTE         (1<<5)		///< 可被执行区
#define     VM_MAP_REGION_FLAG_PROT_MASK            (0xF<<2)	///< 访问权限掩码
#define     VM_MAP_REGION_FLAG_NS                   (1<<6) 		/* NON-SECURE */
#define     VM_MAP_REGION_FLAG_SHARED               (1<<7)		///< MAP_SHARED：把对该内存段的修改保存到磁盘文件中 详见 OsCvtProtFlagsToRegionFlags ,要和 VM_MAP_REGION_FLAG_SHM区别理解
#define     VM_MAP_REGION_FLAG_PRIVATE              (1<<8)		///< MAP_PRIVATE：内存段私有，对它的修改值仅对本进程有效,详见 OsCvtProtFlagsToRegionFlags。
#define     VM_MAP_REGION_FLAG_FLAG_MASK            (3<<7)		///< 掩码
#define     VM_MAP_REGION_FLAG_STACK                (1<<9)		///< 线性区的类型:栈区
#define     VM_MAP_REGION_FLAG_HEAP                 (1<<10)		///< 线性区的类型:堆区
#define     VM_MAP_REGION_FLAG_DATA                 (1<<11)		///< data数据区 编译在ELF中
#define     VM_MAP_REGION_FLAG_TEXT                 (1<<12)		///< 代码区
#define     VM_MAP_REGION_FLAG_BSS                  (1<<13)		///< bbs数据区 由运行时动态分配
#define     VM_MAP_REGION_FLAG_VDSO                 (1<<14)		///< VDSO（Virtual Dynamic Shared Object，虚拟动态共享库）由内核提供的虚拟.so文件，它不在磁盘上，而在内核里，内核将其映射到一个地址空间中，被所有程序共享，正文段大小为一个页面。
#define     VM_MAP_REGION_FLAG_MMAP                 (1<<15)		///< 映射区,虚拟空间内有专门用来存储<虚拟地址-物理地址>映射的区域
#define     VM_MAP_REGION_FLAG_SHM                  (1<<16) 	///< 共享内存区,和代码区同级概念,意思是整个线性区被贴上共享标签
#define     VM_MAP_REGION_FLAG_FIXED                (1<<17)
#define     VM_MAP_REGION_FLAG_FIXED_NOREPLACE      (1<<18)
#define     VM_MAP_REGION_FLAG_INVALID              (1<<19) /* indicates that flags are not specified */
/// 从外部权限标签转化为线性区权限标签
STATIC INLINE UINT32 OsCvtProtFlagsToRegionFlags(unsigned long prot, unsigned long flags)
{
    UINT32 regionFlags = 0;

    regionFlags |= VM_MAP_REGION_FLAG_PERM_USER;								//必须是可用区
    regionFlags |= (prot & PROT_READ) ? VM_MAP_REGION_FLAG_PERM_READ : 0; 		//映射区可被读
    regionFlags |= (prot & PROT_WRITE) ? (VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE) : 0;
    regionFlags |= (prot & PROT_EXEC) ? (VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_EXECUTE) : 0;
    regionFlags |= (flags & MAP_SHARED) ? VM_MAP_REGION_FLAG_SHARED : 0;
    regionFlags |= (flags & MAP_PRIVATE) ? VM_MAP_REGION_FLAG_PRIVATE : 0;
    regionFlags |= (flags & MAP_FIXED) ? VM_MAP_REGION_FLAG_FIXED : 0;
    regionFlags |= (flags & MAP_FIXED_NOREPLACE) ? VM_MAP_REGION_FLAG_FIXED_NOREPLACE : 0;

    return regionFlags;
}
/// 是否为内核空间的地址
STATIC INLINE BOOL LOS_IsKernelAddress(VADDR_T vaddr)
{
    return ((vaddr >= (VADDR_T)KERNEL_ASPACE_BASE) &&
            (vaddr <= ((VADDR_T)KERNEL_ASPACE_BASE + ((VADDR_T)KERNEL_ASPACE_SIZE - 1))));
}
/// 给定地址范围是否都在内核空间中
STATIC INLINE BOOL LOS_IsKernelAddressRange(VADDR_T vaddr, size_t len)
{
    return (vaddr + len > vaddr) && LOS_IsKernelAddress(vaddr) && (LOS_IsKernelAddress(vaddr + len - 1));
}
/// 获取区的结束地址
STATIC INLINE VADDR_T LOS_RegionEndAddr(LosVmMapRegion *region)
{
    return (region->range.base + region->range.size - 1);
}
/// 获取线性区大小
STATIC INLINE size_t LOS_RegionSize(VADDR_T start, VADDR_T end)
{
    return (end - start + 1);
}
/// 是否为文件映射区
STATIC INLINE BOOL LOS_IsRegionTypeFile(LosVmMapRegion* region)
{
    return region->regionType == VM_MAP_REGION_TYPE_FILE;
}
/// permanent 用户进程常量区
STATIC INLINE BOOL LOS_IsRegionPermUserReadOnly(LosVmMapRegion* region)
{
    return ((region->regionFlags & VM_MAP_REGION_FLAG_PROT_MASK) ==
            (VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_READ));
}
/// 是否为私有线性区
STATIC INLINE BOOL LOS_IsRegionFlagPrivateOnly(LosVmMapRegion* region)
{
    return ((region->regionFlags & VM_MAP_REGION_FLAG_FLAG_MASK) == VM_MAP_REGION_FLAG_PRIVATE);
}
/// 设置线性区为文件映射
STATIC INLINE VOID LOS_SetRegionTypeFile(LosVmMapRegion* region)
{
    region->regionType = VM_MAP_REGION_TYPE_FILE;
}
/// 是否为设备映射线性区 /dev/...
STATIC INLINE BOOL LOS_IsRegionTypeDev(LosVmMapRegion* region)
{
    return region->regionType == VM_MAP_REGION_TYPE_DEV;
}
/// 设为设备映射线性区
STATIC INLINE VOID LOS_SetRegionTypeDev(LosVmMapRegion* region)
{
    region->regionType = VM_MAP_REGION_TYPE_DEV;
}
/// 是否为匿名swap映射线性区
STATIC INLINE BOOL LOS_IsRegionTypeAnon(LosVmMapRegion* region)
{
    return region->regionType == VM_MAP_REGION_TYPE_ANON;
}
/// 设为匿名swap映射线性区
STATIC INLINE VOID LOS_SetRegionTypeAnon(LosVmMapRegion* region)
{
    region->regionType = VM_MAP_REGION_TYPE_ANON;
}
/// 虚拟地址是否在用户空间, 真为在用户空间
STATIC INLINE BOOL LOS_IsUserAddress(VADDR_T vaddr)
{
    return ((vaddr >= USER_ASPACE_BASE) &&
            (vaddr <= (USER_ASPACE_BASE + (USER_ASPACE_SIZE - 1))));
}
/// 虚拟地址[vaddr,vaddr + len]是否在用户空间
STATIC INLINE BOOL LOS_IsUserAddressRange(VADDR_T vaddr, size_t len)
{
    return (vaddr + len > vaddr) && LOS_IsUserAddress(vaddr) && (LOS_IsUserAddress(vaddr + len - 1));
}

//是否是一个动态分配的地址(通过vmalloc申请的)
STATIC INLINE BOOL LOS_IsVmallocAddress(VADDR_T vaddr)
{
    return ((vaddr >= VMALLOC_START) &&
            (vaddr <= (VMALLOC_START + (VMALLOC_SIZE - 1))));
}
/// 是否为一个空线性区
STATIC INLINE BOOL OsIsVmRegionEmpty(LosVmSpace *vmSpace)
{
    if (vmSpace->regionRbTree.ulNodes == 0) {
        return TRUE;
    }
    return FALSE;
}

LosVmSpace *LOS_GetKVmSpace(VOID);
LOS_DL_LIST *LOS_GetVmSpaceList(VOID);
LosVmSpace *LOS_GetVmallocSpace(VOID);
VOID OsInitMappingStartUp(VOID);
VOID OsKSpaceInit(VOID);
BOOL LOS_IsRangeInSpace(const LosVmSpace *space, VADDR_T vaddr, size_t size);
STATUS_T LOS_VmSpaceReserve(LosVmSpace *space, size_t size, VADDR_T vaddr);
INT32 OsUserHeapFree(LosVmSpace *vmSpace, VADDR_T addr, size_t len);
VADDR_T OsAllocRange(LosVmSpace *vmSpace, size_t len);
VADDR_T OsAllocSpecificRange(LosVmSpace *vmSpace, VADDR_T vaddr, size_t len, UINT32 regionFlags);
LosVmMapRegion *OsCreateRegion(VADDR_T vaddr, size_t len, UINT32 regionFlags, unsigned long offset);
BOOL OsInsertRegion(LosRbTree *regionRbTree, LosVmMapRegion *region);
LosVmSpace *LOS_SpaceGet(VADDR_T vaddr);
LosVmSpace *LOS_CurrSpaceGet(VOID);
BOOL LOS_IsRegionFileValid(LosVmMapRegion *region);
LosVmMapRegion *LOS_RegionRangeFind(LosVmSpace *vmSpace, VADDR_T addr, size_t len);
LosVmMapRegion *LOS_RegionFind(LosVmSpace *vmSpace, VADDR_T addr);
PADDR_T LOS_PaddrQuery(VOID *vaddr);
LosVmMapRegion *LOS_RegionAlloc(LosVmSpace *vmSpace, VADDR_T vaddr, size_t len, UINT32 regionFlags, VM_OFFSET_T pgoff);
STATUS_T OsRegionsRemove(LosVmSpace *space, VADDR_T vaddr, size_t size);
STATUS_T OsVmRegionAdjust(LosVmSpace *space, VADDR_T vaddr, size_t size);
LosVmMapRegion *OsVmRegionDup(LosVmSpace *space, LosVmMapRegion *oldRegion, VADDR_T vaddr, size_t size);
STATUS_T OsIsRegionCanExpand(LosVmSpace *space, LosVmMapRegion *region, size_t size);
STATUS_T LOS_RegionFree(LosVmSpace *space, LosVmMapRegion *region);
STATUS_T LOS_VmSpaceFree(LosVmSpace *space);
STATUS_T LOS_VaddrToPaddrMmap(LosVmSpace *space, VADDR_T vaddr, PADDR_T paddr, size_t len, UINT32 flags);
BOOL OsUserVmSpaceInit(LosVmSpace *vmSpace, VADDR_T *virtTtb);
LosVmSpace *OsCreateUserVmSpace(VOID);
STATUS_T LOS_VmSpaceClone(LosVmSpace *oldVmSpace, LosVmSpace *newVmSpace);
LosMux *OsGVmSpaceMuxGet(VOID);
STATUS_T OsUnMMap(LosVmSpace *space, VADDR_T addr, size_t size);
STATUS_T OsVmSpaceRegionFree(LosVmSpace *space);
/**
 * thread safety
 * it is used to malloc continuous virtual memory, no sure for continuous physical memory.
 */
VOID *LOS_VMalloc(size_t size);
VOID LOS_VFree(const VOID *addr);

/**
 * thread safety
 * these is used to malloc or free kernel memory.
 * when the size is large and close to multiples of pages,
 * will alloc pmm pages, otherwise alloc bestfit memory.
 */
VOID *LOS_KernelMalloc(UINT32 size);
VOID *LOS_KernelMallocAlign(UINT32 size, UINT32 boundary);
VOID *LOS_KernelRealloc(VOID *ptr, UINT32 size);
VOID LOS_KernelFree(VOID *ptr);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* __LOS_VM_MAP_H__ */

