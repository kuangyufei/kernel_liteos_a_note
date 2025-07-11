/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2023 Huawei Device Co., Ltd. All rights reserved.
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
/**
 * @defgroup vm_map_macro 虚拟内存映射宏定义
 * @ingroup kernel_vm
 * @{
 */
/**
 * @brief 内核大内存分配阈值
 * @details 当内核内存分配大小小于此值时使用堆分配，否则使用物理页面分配
 * @value (PAGE_SIZE << 2) 通常为4页大小(16KB，假设PAGE_SIZE=4KB)
 */
#define KMALLOC_LARGE_SIZE    (PAGE_SIZE << 2)
/** @} */

/**
 * @brief 虚拟内存区域范围结构
 * @details 描述一段连续虚拟内存区域的基地址和大小
 * @ingroup kernel_vm
 */
typedef struct VmMapRange {
    VADDR_T             base;           /**< 虚拟内存区域基地址 */
    UINT32              size;           /**< 虚拟内存区域大小(字节) */
} LosVmMapRange;

/* 前向声明：避免循环依赖并减少编译开销 */
struct VmMapRegion;
typedef struct VmMapRegion LosVmMapRegion;
struct VmFileOps;
typedef struct VmFileOps LosVmFileOps;
struct VmSpace;
typedef struct VmSpace LosVmSpace;

/**
 * @brief 页面错误信息结构
 * @details 存储页面错误发生时的相关上下文信息
 * @ingroup kernel_vm
 */
typedef struct VmFault {
    UINT32          flags;              /* FAULT_FLAG_xxx 标志位，描述页面错误类型 */
    unsigned long   pgoff;              /* 基于区域的逻辑页面偏移量 */
    VADDR_T         vaddr;              /* 触发错误的虚拟地址 */
    VADDR_T         *pageKVaddr;        /* 页面错误对应物理页的内核虚拟地址指针 */
} LosVmPgFault;

/**
 * @brief 虚拟内存文件操作接口
 * @details 定义虚拟内存区域关联文件的操作方法集合
 * @ingroup kernel_vm
 */
struct VmFileOps {
    /**
     * @brief 打开虚拟内存区域关联的文件
     * @param region 虚拟内存区域指针
     */
    void (*open)(struct VmMapRegion *region);
    /**
     * @brief 关闭虚拟内存区域关联的文件
     * @param region 虚拟内存区域指针
     */
    void (*close)(struct VmMapRegion *region);
    /**
     * @brief 处理与文件关联的页面错误
     * @param region 虚拟内存区域指针
     * @param pageFault 页面错误信息结构指针
     * @return 0表示成功，非0表示错误码
     */
    int  (*fault)(struct VmMapRegion *region, LosVmPgFault *pageFault);
    /**
     * @brief 移除虚拟内存区域中与文件关联的页面
     * @param region 虚拟内存区域指针
     * @param archMmu 体系结构相关的MMU操作接口
     * @param offset 相对于区域起始的偏移量
     */
    void (*remove)(struct VmMapRegion *region, LosArchMmu *archMmu, VM_OFFSET_T offset);
};

/**
 * @brief 虚拟内存区域结构
 * @details 描述进程虚拟地址空间中的一个连续内存区域及其属性
 * @ingroup kernel_vm
 */
struct VmMapRegion {
    LosRbNode           rbNode;         /**< 红黑树节点，用于区域在地址空间中的快速查找 */
    LosVmSpace          *space;          /**< 指向该区域所属的虚拟地址空间 */
    LOS_DL_LIST         node;           /**< 双向链表节点，用于区域遍历 */
    LosVmMapRange       range;          /**< 区域地址范围(基地址和大小) */
    VM_OFFSET_T         pgOff;          /**< 区域相对于文件的页面偏移量 */
    UINT32              regionFlags;    /**< 区域标志：如写时复制(COW)、用户锁定等属性 */
    UINT32              shmid;          /**< 共享内存ID，用于标识共享区域 */
    UINT8               forkFlags;      /**< 虚拟空间fork标志：COPY(复制)、ZERO(置零)等 */
    UINT8               regionType;     /**< 虚拟区域类型：匿名(ANON)、文件(FILE)、设备(DEV) */
    /**
     * @brief 区域类型专用数据联合体
     * @details 根据regionType的不同，使用不同的成员结构
     */
    union {
        /** @brief 文件类型区域数据 */
        struct VmRegionFile {
            int f_oflags;               /**< 文件打开标志 */
            struct Vnode *vnode;        /**< 文件vnode指针 */
            const LosVmFileOps *vmFOps; /**< 文件操作接口指针 */
        } rf;
        /** @brief 匿名类型区域数据 */
        struct VmRegionAnon {
            LOS_DL_LIST  node;          /**< 区域关联的LosVmPage页面链表 */
        } ra;
        /** @brief 设备类型区域数据 */
        struct VmRegionDev {
            LOS_DL_LIST  node;          /**< 区域关联的LosVmPage页面链表 */
            const LosVmFileOps *vmFOps; /**< 设备操作接口指针 */
        } rd;
    } unTypeData;
};

/**
 * @brief 虚拟地址空间结构
 * @details 描述一个进程的完整虚拟地址空间及其管理信息
 * @ingroup kernel_vm
 */
typedef struct VmSpace {
    LOS_DL_LIST         node;           /**< 双向链表节点，用于进程间地址空间遍历 */
    LosRbTree           regionRbTree;   /**< 区域红黑树的根节点，用于区域快速查找 */
    LosMux              regionMux;      /**< 区域红黑树互斥锁，保护并发访问 */
    VADDR_T             base;           /**< 虚拟地址空间基地址 */
    UINT32              size;           /**< 虚拟地址空间大小(字节) */
    VADDR_T             heapBase;       /**< 堆区域基地址 */
    VADDR_T             heapNow;        /**< 当前堆指针位置(已分配到的地址) */
    LosVmMapRegion      *heap;          /**< 堆区域指针 */
    VADDR_T             mapBase;        /**< 内存映射区域基地址 */
    UINT32              mapSize;        /**< 内存映射区域大小(字节) */
    LosArchMmu          archMmu;        /**< 体系结构相关的MMU数据 */
#ifdef LOSCFG_DRIVERS_TZDRIVER
    VADDR_T             codeStart;      /**< 用户进程代码段起始地址 */
    VADDR_T             codeEnd;        /**< 用户进程代码段结束地址 */
#endif
} LosVmSpace;

/**
 * @defgroup vm_region_type 虚拟内存区域类型
 * @ingroup vm_map_macro
 * @{
 */
#define     VM_MAP_REGION_TYPE_NONE                 (0x0)   /**< 无效区域类型 */
#define     VM_MAP_REGION_TYPE_ANON                 (0x1)   /**< 匿名区域(无关联文件) */
#define     VM_MAP_REGION_TYPE_FILE                 (0x2)   /**< 文件映射区域 */
#define     VM_MAP_REGION_TYPE_DEV                  (0x4)   /**< 设备映射区域 */
#define     VM_MAP_REGION_TYPE_MASK                 (0x7)   /**< 区域类型掩码(低3位) */
/** @} */

/**
 * @defgroup vm_region_flags 虚拟内存区域标志
 * @ingroup vm_map_macro
 * @note 高8位(24~31)保留，共享内存将使用
 * @{
 */
/* 缓存属性标志(位0~1) */
#define     VM_MAP_REGION_FLAG_CACHED               (0<<0)  /**< 缓存使能 */
#define     VM_MAP_REGION_FLAG_UNCACHED             (1<<0)  /**< 无缓存 */
#define     VM_MAP_REGION_FLAG_UNCACHED_DEVICE      (2<<0)  /**< 设备无缓存(仅部分架构支持，否则等同于UNCACHED) */
#define     VM_MAP_REGION_FLAG_STRONGLY_ORDERED     (3<<0)  /**< 强序内存(仅部分架构支持，否则等同于UNCACHED) */
#define     VM_MAP_REGION_FLAG_CACHE_MASK           (3<<0)  /**< 缓存属性掩码(位0~1) */

/* 权限控制标志(位2~5) */
#define     VM_MAP_REGION_FLAG_PERM_USER            (1<<2)  /**< 用户态可访问 */
#define     VM_MAP_REGION_FLAG_PERM_READ            (1<<3)  /**< 读权限 */
#define     VM_MAP_REGION_FLAG_PERM_WRITE           (1<<4)  /**< 写权限 */
#define     VM_MAP_REGION_FLAG_PERM_EXECUTE         (1<<5)  /**< 执行权限 */
#define     VM_MAP_REGION_FLAG_PROT_MASK            (0xF<<2) /**< 权限标志掩码(位2~5) */

/* 安全与共享标志(位6~8) */
#define     VM_MAP_REGION_FLAG_NS                   (1<<6)  /**< 非安全模式(NON-SECURE) */
#define     VM_MAP_REGION_FLAG_SHARED               (1<<7)  /**< 共享映射 */
#define     VM_MAP_REGION_FLAG_PRIVATE              (1<<8)  /**< 私有映射 */
#define     VM_MAP_REGION_FLAG_FLAG_MASK            (3<<7)  /**< 共享/私有标志掩码(位7~8) */

/* 内存区域用途标志(位9~15) */
#define     VM_MAP_REGION_FLAG_STACK                (1<<9)  /**< 栈区域 */
#define     VM_MAP_REGION_FLAG_HEAP                 (1<<10) /**< 堆区域 */
#define     VM_MAP_REGION_FLAG_DATA                 (1<<11) /**< 数据段 */
#define     VM_MAP_REGION_FLAG_TEXT                 (1<<12) /**< 代码段 */
#define     VM_MAP_REGION_FLAG_BSS                  (1<<13) /**< BSS段(未初始化数据) */
#define     VM_MAP_REGION_FLAG_VDSO                 (1<<14) /**< 虚拟动态共享对象 */
#define     VM_MAP_REGION_FLAG_MMAP                 (1<<15) /**< mmap系统调用创建的区域 */

/* 特殊用途标志(位16~20) */
#define     VM_MAP_REGION_FLAG_SHM                  (1<<16) /**< 共享内存区域 */
#define     VM_MAP_REGION_FLAG_FIXED                (1<<17) /**< 固定地址映射，不自动选择地址 */
#define     VM_MAP_REGION_FLAG_FIXED_NOREPLACE      (1<<18) /**< 固定地址且不替换已有映射 */
#define     VM_MAP_REGION_FLAG_LITEIPC              (1<<19) /**< LiteIPC专用区域 */
#define     VM_MAP_REGION_FLAG_INVALID              (1<<20) /**< 无效标志，表示未指定有效的区域标志 */
/** @} */
/**
 * @defgroup vm_region_utils 虚拟内存区域工具函数
 * @ingroup kernel_vm
 * @{
 */
/**
 * @brief 将保护标志和映射标志转换为区域标志
 * @details 将POSIX标准的内存保护标志和映射标志转换为内核内部使用的区域标志
 * @param[in] prot 内存保护标志，取值为PROT_READ/PROT_WRITE/PROT_EXEC等的组合
 * @param[in] flags 映射标志，取值为MAP_SHARED/MAP_PRIVATE/MAP_FIXED等的组合
 * @return UINT32 转换后的区域标志，为VM_MAP_REGION_FLAG_xxx系列标志的组合
 * @note 写权限自动包含读权限，执行权限自动包含读权限
 */
STATIC INLINE UINT32 OsCvtProtFlagsToRegionFlags(unsigned long prot, unsigned long flags)
{
    UINT32 regionFlags = 0;

    regionFlags |= VM_MAP_REGION_FLAG_PERM_USER;                                  /* 默认添加用户态访问权限 */
    regionFlags |= (prot & PROT_READ) ? VM_MAP_REGION_FLAG_PERM_READ : 0;         /* 读权限转换 */
    regionFlags |= (prot & PROT_WRITE) ? (VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE) : 0; /* 写权限隐含读权限 */
    regionFlags |= (prot & PROT_EXEC) ? (VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_EXECUTE) : 0; /* 执行权限隐含读权限 */
    regionFlags |= (flags & MAP_SHARED) ? VM_MAP_REGION_FLAG_SHARED : 0;         /* 共享映射标志转换 */
    regionFlags |= (flags & MAP_PRIVATE) ? VM_MAP_REGION_FLAG_PRIVATE : 0;       /* 私有映射标志转换 */
    regionFlags |= (flags & MAP_FIXED) ? VM_MAP_REGION_FLAG_FIXED : 0;           /* 固定地址标志转换 */
    regionFlags |= (flags & MAP_FIXED_NOREPLACE) ? VM_MAP_REGION_FLAG_FIXED_NOREPLACE : 0; /* 不替换已有映射的固定地址标志 */

    return regionFlags;
}

/**
 * @brief 判断虚拟地址是否属于内核地址空间
 * @param[in] vaddr 待判断的虚拟地址
 * @return BOOL TRUE表示是内核地址，FALSE表示不是
 * @note 内核地址空间范围由KERNEL_ASPACE_BASE和KERNEL_ASPACE_SIZE定义
 */
STATIC INLINE BOOL LOS_IsKernelAddress(VADDR_T vaddr)
{
    return ((vaddr >= (VADDR_T)KERNEL_ASPACE_BASE) &&
            (vaddr <= ((VADDR_T)KERNEL_ASPACE_BASE + ((VADDR_T)KERNEL_ASPACE_SIZE - 1))));
}

/**
 * @brief 判断虚拟地址范围是否完全属于内核地址空间
 * @param[in] vaddr 地址范围起始虚拟地址
 * @param[in] len 地址范围长度(字节)
 * @return BOOL TRUE表示整个范围都是内核地址，FALSE表示不是
 * @note 会自动处理地址溢出情况(vaddr + len > vaddr)
 */
STATIC INLINE BOOL LOS_IsKernelAddressRange(VADDR_T vaddr, size_t len)
{
    return (vaddr + len > vaddr) && LOS_IsKernelAddress(vaddr) && (LOS_IsKernelAddress(vaddr + len - 1));
}

/**
 * @brief 计算虚拟内存区域的结束地址
 * @param[in] region 虚拟内存区域指针
 * @return VADDR_T 区域结束地址(包含该地址)
 * @note 计算公式：base + size - 1
 */
STATIC INLINE VADDR_T LOS_RegionEndAddr(LosVmMapRegion *region)
{
    return (region->range.base + region->range.size - 1);
}

/**
 * @brief 计算虚拟地址范围的大小
 * @param[in] start 起始地址(包含)
 * @param[in] end 结束地址(包含)
 * @return size_t 地址范围大小(字节)
 * @note 计算公式：end - start + 1
 */
STATIC INLINE size_t LOS_RegionSize(VADDR_T start, VADDR_T end)
{
    return (end - start + 1);
}

/**
 * @brief 判断区域是否为文件映射类型
 * @param[in] region 虚拟内存区域指针
 * @return BOOL TRUE表示是文件映射区域，FALSE表示不是
 */
STATIC INLINE BOOL LOS_IsRegionTypeFile(LosVmMapRegion* region)
{
    return region->regionType == VM_MAP_REGION_TYPE_FILE;
}

/**
 * @brief 判断区域是否为用户态只读权限
 * @param[in] region 虚拟内存区域指针
 * @return BOOL TRUE表示仅用户态读权限，FALSE表示其他权限组合
 * @note 权限组合为VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_READ
 */
STATIC INLINE BOOL LOS_IsRegionPermUserReadOnly(LosVmMapRegion* region)
{
    return ((region->regionFlags & VM_MAP_REGION_FLAG_PROT_MASK) ==
            (VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_READ));
}

/**
 * @brief 判断区域是否为纯私有映射
 * @param[in] region 虚拟内存区域指针
 * @return BOOL TRUE表示仅私有映射标志，FALSE表示其他共享属性
 * @note 会屏蔽其他标志位，仅判断共享/私有标志组合
 */
STATIC INLINE BOOL LOS_IsRegionFlagPrivateOnly(LosVmMapRegion* region)
{
    return ((region->regionFlags & VM_MAP_REGION_FLAG_FLAG_MASK) == VM_MAP_REGION_FLAG_PRIVATE);
}

/**
 * @brief 设置区域类型为文件映射
 * @param[in,out] region 虚拟内存区域指针
 */
STATIC INLINE VOID LOS_SetRegionTypeFile(LosVmMapRegion* region)
{
    region->regionType = VM_MAP_REGION_TYPE_FILE;
}

/**
 * @brief 判断区域是否为设备映射类型
 * @param[in] region 虚拟内存区域指针
 * @return BOOL TRUE表示是设备映射区域，FALSE表示不是
 */
STATIC INLINE BOOL LOS_IsRegionTypeDev(LosVmMapRegion* region)
{
    return region->regionType == VM_MAP_REGION_TYPE_DEV;
}

/**
 * @brief 设置区域类型为设备映射
 * @param[in,out] region 虚拟内存区域指针
 */
STATIC INLINE VOID LOS_SetRegionTypeDev(LosVmMapRegion* region)
{
    region->regionType = VM_MAP_REGION_TYPE_DEV;
}

/**
 * @brief 判断区域是否为匿名映射类型
 * @param[in] region 虚拟内存区域指针
 * @return BOOL TRUE表示是匿名映射区域，FALSE表示不是
 */
STATIC INLINE BOOL LOS_IsRegionTypeAnon(LosVmMapRegion* region)
{
    return region->regionType == VM_MAP_REGION_TYPE_ANON;
}

/**
 * @brief 设置区域类型为匿名映射
 * @param[in,out] region 虚拟内存区域指针
 */
STATIC INLINE VOID LOS_SetRegionTypeAnon(LosVmMapRegion* region)
{
    region->regionType = VM_MAP_REGION_TYPE_ANON;
}

/**
 * @brief 判断虚拟地址是否属于用户地址空间
 * @param[in] vaddr 待判断的虚拟地址
 * @return BOOL TRUE表示是用户地址，FALSE表示不是
 * @note 用户地址空间范围由USER_ASPACE_BASE和USER_ASPACE_SIZE定义
 */
STATIC INLINE BOOL LOS_IsUserAddress(VADDR_T vaddr)
{
    return ((vaddr >= USER_ASPACE_BASE) &&
            (vaddr <= (USER_ASPACE_BASE + (USER_ASPACE_SIZE - 1))));
}

/**
 * @brief 判断虚拟地址范围是否完全属于用户地址空间
 * @param[in] vaddr 地址范围起始虚拟地址
 * @param[in] len 地址范围长度(字节)
 * @return BOOL TRUE表示整个范围都是用户地址，FALSE表示不是
 * @note 会自动处理地址溢出情况(vaddr + len > vaddr)
 */
STATIC INLINE BOOL LOS_IsUserAddressRange(VADDR_T vaddr, size_t len)
{
    return (vaddr + len > vaddr) && LOS_IsUserAddress(vaddr) && (LOS_IsUserAddress(vaddr + len - 1));
}

/**
 * @brief 判断虚拟地址是否属于动态虚拟内存区域
 * @param[in] vaddr 待判断的虚拟地址
 * @return BOOL TRUE表示是vmalloc区域地址，FALSE表示不是
 * @note vmalloc区域范围由VMALLOC_START和VMALLOC_SIZE定义
 */
STATIC INLINE BOOL LOS_IsVmallocAddress(VADDR_T vaddr)
{
    return ((vaddr >= VMALLOC_START) &&
            (vaddr <= (VMALLOC_START + (VMALLOC_SIZE - 1))));
}

/**
 * @brief 判断虚拟地址空间是否为空(无任何区域)
 * @param[in] vmSpace 虚拟地址空间指针
 * @return BOOL TRUE表示地址空间为空，FALSE表示已包含区域
 * @note 通过判断区域红黑树的节点数量是否为0实现
 */
STATIC INLINE BOOL OsIsVmRegionEmpty(LosVmSpace *vmSpace)
{
    if (vmSpace->regionRbTree.ulNodes == 0) {
        return TRUE;
    }
    return FALSE;
}
/** @} */

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
STATUS_T LOS_VmSpaceClone(UINT32 cloneFlags, LosVmSpace *oldVmSpace, LosVmSpace *newVmSpace);
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

