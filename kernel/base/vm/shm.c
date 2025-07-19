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
/*!
 * @file    shm.c
 * @brief
 * @link
   @verbatim
    什么是共享内存
	顾名思义，共享内存就是允许两个不相关的进程访问同一个物理内存。共享内存是在两个正在运行的进程之间
	共享和传递数据的一种非常有效的方式。不同进程之间共享的内存通常安排为同一段物理内存。进程可以将同
	一段共享内存连接到它们自己的地址空间中，所有进程都可以访问共享内存中的地址，就好像它们是由用C语言
	函数malloc()分配的内存一样。而如果某个进程向共享内存写入数据，所做的改动将立即影响到可以访问同一段
	共享内存的任何其他进程。

	特别提醒：共享内存并未提供同步机制，也就是说，在第一个进程结束对共享内存的写操作之前，并无自动机制
	可以阻止第二个进程开始对它进行读取。所以我们通常需要用其他的机制来同步对共享内存的访问

	共享线性区可以由任意的进程创建,每个使用共享线性区都必须经过映射.
   @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2025-07-19
 */

#include "stdio.h"
#include "string.h"
#include "time.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "los_config.h"
#include "los_init.h"
#include "los_vm_map.h"
#include "los_vm_filemap.h"
#include "los_vm_phys.h"
#include "los_arch_mmu.h"
#include "los_vm_page.h"
#include "los_vm_lock.h"
#include "los_process.h"
#include "los_process_pri.h"
#include "user_copy.h"
#include "los_vm_shm_pri.h"
#include "sys/shm.h"
#ifdef LOSCFG_SHELL
#include "shcmd.h"
#include "shell.h"
#endif

#ifdef LOSCFG_KERNEL_SHM

/**
 * @defgroup shm_macro SHM核心宏定义
 * @brief 共享内存（System V IPC）模块的核心常量与控制宏
 * @{ 
 */

/**
 * @brief 共享内存段状态：空闲
 * @details 标识共享内存段当前处于未分配状态，可被新的shmget请求分配
 * @note 该标志位与其他状态标志（USED/REMOVE）互斥
 */
#define SHM_SEG_FREE    0x2000  /* 共享内存段空闲状态标志 */
/**
 * @brief 共享内存段状态：已使用
 * @details 标识共享内存段已被进程分配，正在使用中
 * @note 进程通过shmget获取并关联该段后设置此标志
 */
#define SHM_SEG_USED    0x4000  /* 共享内存段已使用状态标志 */
/**
 * @brief 共享内存段状态：待删除
 * @details 标识共享内存段已被标记删除，当最后一个关联进程 detach 后将被实际释放
 * @see shmctl(IPC_RMID)
 */
#define SHM_SEG_REMOVE  0x8000  /* 共享内存段待删除状态标志 */

/**
 * @brief SHM创建模式标志
 * @details 用于shmget的flag参数，标识创建新的共享内存段
 * @note 需与IPC_CREAT配合使用以创建新段
 */
#ifndef SHM_M
#define SHM_M   010000           /* 创建模式标志（八进制） */
#endif

/**
 * @brief SHM执行权限标志
 * @details 用于共享内存段的权限控制，允许执行操作
 * @note 该标志在LiteOS-A中未实际使用，为POSIX兼容性定义
 */
#ifndef SHM_X
#define SHM_X   0100             /* 执行权限标志（八进制） */
#endif

/**
 * @brief 默认访问权限掩码
 * @details 定义共享内存段的默认访问权限，组合用户、组和其他用户的读写执行权限
 * @note 实际权限会与进程的umask进行按位与操作
 */
#ifndef ACCESSPERMS
#define ACCESSPERMS (S_IRWXU | S_IRWXG | S_IRWXO)  /* 默认访问权限掩码 */
#endif

/**
 * @brief 共享内存读权限掩码（所有用户）
 * @details 组合用户、组和其他用户的读权限位
 * @note 对应POSIX权限位：0444（八进制）
 */
#define SHM_S_IRUGO (S_IRUSR | S_IRGRP | S_IROTH)  /* 读权限掩码 */
/**
 * @brief 共享内存写权限掩码（所有用户）
 * @details 组合用户、组和其他用户的写权限位
 * @note 对应POSIX权限位：0222（八进制）
 */
#define SHM_S_IWUGO (S_IWUSR | S_IWGRP | S_IWOTH)  /* 写权限掩码 */
/**
 * @brief 共享内存执行权限掩码（所有用户）
 * @details 组合用户、组和其他用户的执行权限位
 * @note 对应POSIX权限位：0111（八进制）
 */
#define SHM_S_IXUGO (S_IXUSR | S_IXGRP | S_IXOTH)  /* 执行权限掩码 */

/**
 * @brief 组权限位移量
 * @details 将组权限位转换为用户权限位的位移量（3位）
 * @note 用于权限检查时的位运算转换
 */
#define SHM_GROUPE_TO_USER  3   /* 组权限到用户权限的位移量 */
/**
 * @brief 其他用户权限位移量
 * @details 将其他用户权限位转换为用户权限位的位移量（6位）
 * @note 用于权限检查时的位运算转换
 */
#define SHM_OTHER_TO_USER   6   /* 其他用户权限到用户权限的位移量 */

/**
 * @brief 非容器模式下的共享内存全局变量
 * @details 当未启用IPC容器（LOSCFG_IPC_CONTAINER）时，使用全局变量管理共享内存
 * @note 容器模式下会使用每个容器独立的SHM管理结构
 */
#ifndef LOSCFG_IPC_CONTAINER
/**
 * @brief 共享内存全局互斥锁
 * @details 保护共享内存元数据操作的线程安全，所有SHM系统调用必须持有此锁
 * @warning 必须通过SYSV_SHM_LOCK/SYSV_SHM_UNLOCK宏操作，禁止直接调用LOS_Muxxxx接口
 */
STATIC LosMux g_sysvShmMux;

/* private data - 共享内存核心管理数据 */
/**
 * @brief 共享内存系统信息结构体
 * @details 记录系统级共享内存资源使用情况，如最大段数、最大段大小等
 * @see struct shminfo定义
 */
STATIC struct shminfo g_shmInfo;
/**
 * @brief 共享内存段数组指针
 * @details 指向所有共享内存段的元数据数组，每个元素描述一个共享内存段的状态
 * @note 数组大小由SHMMNI配置项决定
 */
STATIC struct shmIDSource *g_shmSegs = NULL;
/**
 * @brief 已使用共享内存页面计数
 * @details 记录系统当前分配给共享内存的物理页面总数
 * @note 用于内存资源统计和限制检查
 */
STATIC UINT32 g_shmUsedPageCount;

/**
 * @brief 共享内存信息访问宏
 * @details 统一访问共享内存系统信息的接口，便于后续扩展为容器模式
 */
#define IPC_SHM_INFO            g_shmInfo
/**
 * @brief 共享内存互斥锁访问宏
 * @details 统一访问共享内存全局锁的接口，隐藏实现细节
 */
#define IPC_SHM_SYS_VSHM_MUTEX  g_sysvShmMux
/**
 * @brief 共享内存段数组访问宏
 * @details 统一访问共享内存段数组的接口，便于后续扩展
 */
#define IPC_SHM_SEGS            g_shmSegs
/**
 * @brief 已使用页面计数访问宏
 * @details 统一访问共享内存页面计数的接口
 */
#define IPC_SHM_USED_PAGE_COUNT g_shmUsedPageCount
#endif

/* private macro - 共享内存内部操作宏 */
/**
 * @brief 获取共享内存全局锁
 * @details 以永久等待方式获取共享内存互斥锁，确保线程安全
 * @note 所有操作共享内存元数据的函数必须先调用此宏
 * @warning 必须与SYSV_SHM_UNLOCK配对使用，防止死锁
 */
#define SYSV_SHM_LOCK()     (VOID)LOS_MuxLock(&IPC_SHM_SYS_VSHM_MUTEX, LOS_WAIT_FOREVER)
/**
 * @brief 释放共享内存全局锁
 * @details 释放之前通过SYSV_SHM_LOCK获取的互斥锁
 * @note 必须在操作完成后立即调用，避免长时间持有锁导致性能问题
 */
#define SYSV_SHM_UNLOCK()   (VOID)LOS_MuxUnlock(&IPC_SHM_SYS_VSHM_MUTEX)

#if 0 // @note_#if0

//内核为每一个IPC对象保存一个ipc_perm结构体，该结构说明了IPC对象的权限和所有者
struct ipc_perm {
	key_t __ipc_perm_key;	//调用shmget()时给出的关键字
	uid_t uid;				//共享内存所有者的有效用户ID
	gid_t gid;				//共享内存所有者所属组的有效组ID
	uid_t cuid;				//共享内存创建 者的有效用户ID
	gid_t cgid;				//共享内存创建者所属组的有效组ID
	mode_t mode;			//权限 + SHM_DEST / SHM_LOCKED /SHM_HUGETLB 标志位
	int __ipc_perm_seq;		//序列号
	long __pad1;			//保留扩展用
	long __pad2;
};
//每个共享内存段在内核中维护着一个内部结构shmid_ds
struct shmid_ds {
	struct ipc_perm shm_perm;///< 操作许可，里面包含共享内存的用户ID、组ID等信息
	size_t shm_segsz;	///< 共享内存段的大小，单位为字节
	time_t shm_atime;	///< 最后一个进程访问共享内存的时间	
	time_t shm_dtime; 	///< 最后一个进程离开共享内存的时间
	time_t shm_ctime; 	///< 创建时间
	pid_t shm_cpid;		///< 创建共享内存的进程ID
	pid_t shm_lpid;		///< 最后操作共享内存的进程ID
	unsigned long shm_nattch;	///< 当前使用该共享内存段的进程数量
	unsigned long __pad1;	//保留扩展用
	unsigned long __pad2;
};
// 共享内存模块设置信息
struct shminfo {
	unsigned long shmmax, shmmin, shmmni, shmseg, shmall, __unused[4];
};
struct shmIDSource {//共享内存描述符
    struct shmid_ds ds; //是内核为每一个共享内存段维护的数据结构
    UINT32 status;	//状态 SHM_SEG_FREE ...
    LOS_DL_LIST node; //节点,挂VmPage
#ifdef LOSCFG_SHELL
    CHAR ownerName[OS_PCB_NAME_LEN];
#endif
};

/* private data */
STATIC struct shminfo g_shmInfo = { //描述共享内存范围的全局变量
    .shmmax = SHM_MAX,//共享内存单个上限 4096页 即 16M
    .shmmin = SHM_MIN,//共享内存单个下限 1页 即:4K
    .shmmni = SHM_MNI,//共享内存总数 默认192 
    .shmseg = SHM_SEG,//每个用户进程可以使用的最多的共享内存段的数目 128
    .shmall = SHM_ALL,//系统范围内共享内存的总页数, 4096页 
};

STATIC struct shmIDSource *g_shmSegs = NULL;
STATIC UINT32 g_shmUsedPageCount;
#endif
/**
 * @brief 初始化共享内存控制块及系统参数
 * @details 完成共享内存互斥锁初始化、系统限制参数设置、共享内存段数组分配及初始化
 * @param[in] sysvShmMux 共享内存操作互斥锁指针
 * @param[in,out] shmInfo 共享内存系统信息结构体指针，用于存储系统限制参数
 * @param[out] shmUsedPageCount 已使用共享内存页数指针
 * @return 成功返回共享内存段数组指针，失败返回NULL
 * @note 必须在系统启动阶段调用，为共享内存功能提供基础运行环境
 */
struct shmIDSource *OsShmCBInit(LosMux *sysvShmMux, struct shminfo *shmInfo, UINT32 *shmUsedPageCount)
{
    UINT32 ret;                  // 函数返回值
    UINT32 i;                    // 循环计数器

    // 参数合法性检查，防止空指针访问导致系统异常
    if ((sysvShmMux == NULL) || (shmInfo == NULL) || (shmUsedPageCount == NULL)) {
        return NULL;  // 参数无效，返回NULL
    }

    // 初始化共享内存操作互斥锁，保证多线程安全访问
    ret = LOS_MuxInit(sysvShmMux, NULL);
    if (ret != LOS_OK) {
        goto ERROR;  // 互斥锁初始化失败，跳转到错误处理
    }

    // 设置共享内存系统限制参数
    shmInfo->shmmax = SHM_MAX;    // 最大共享内存段大小
    shmInfo->shmmin = SHM_MIN;    // 最小共享内存段大小
    shmInfo->shmmni = SHM_MNI;    // 最大共享内存段数量
    shmInfo->shmseg = SHM_SEG;    // 每个进程可附加的最大共享内存段数
    shmInfo->shmall = SHM_ALL;    // 系统所有共享内存的总大小（页）

    // 从系统内存中分配共享内存段控制结构数组
    struct shmIDSource *shmSegs = LOS_MemAlloc((VOID *)OS_SYS_MEM_ADDR, sizeof(struct shmIDSource) * shmInfo->shmmni);
    if (shmSegs == NULL) {
        (VOID)LOS_MuxDestroy(sysvShmMux);  // 内存分配失败，销毁已初始化的互斥锁
        goto ERROR;                        // 跳转到错误处理
    }
    // 初始化共享内存段数组，清零所有字段
    (VOID)memset_s(shmSegs, (sizeof(struct shmIDSource) * shmInfo->shmmni),
                   0, (sizeof(struct shmIDSource) * shmInfo->shmmni));

    // 初始化每个共享内存段的状态和链表
    for (i = 0; i < shmInfo->shmmni; i++) {
        shmSegs[i].status = SHM_SEG_FREE;       // 标记段为空闲状态
        shmSegs[i].ds.shm_perm.seq = i + 1;     // 设置段序列号，用于标识段唯一性
        LOS_ListInit(&shmSegs[i].node);         // 初始化段的物理页链表
    }
    *shmUsedPageCount = 0;  // 初始化已使用页数为0

    return shmSegs;         // 成功返回共享内存段数组指针

ERROR:
    VM_ERR("ShmInit fail\n");  // 输出错误信息
    return NULL;               // 返回NULL表示初始化失败
}

/**
 * @brief 共享内存模块初始化入口
 * @details 调用控制块初始化函数，完成共享内存系统的初始化
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 * @note 依赖LOS_MODULE_INIT机制在系统启动时自动调用
 */
UINT32 ShmInit(VOID)
{
#ifndef LOSCFG_IPC_CONTAINER  // 当未启用IPC容器功能时
    // 初始化共享内存控制块，获取共享内存段数组
    g_shmSegs = OsShmCBInit(&IPC_SHM_SYS_VSHM_MUTEX, &IPC_SHM_INFO, &IPC_SHM_USED_PAGE_COUNT);
    if (g_shmSegs == NULL) {
        return LOS_NOK;  // 初始化失败返回错误码
    }
#endif
    return LOS_OK;  // 初始化成功
}

// 模块初始化宏，指定在VM_COMPLETE阶段初始化共享内存模块
LOS_MODULE_INIT(ShmInit, LOS_INIT_LEVEL_VM_COMPLETE);

/**
 * @brief 共享内存模块去初始化
 * @details 释放共享内存段数组，销毁互斥锁
 * @return 成功返回0，失败返回-1
 * @warning 仅在系统关闭时调用，运行时调用可能导致严重错误
 */
UINT32 ShmDeinit(VOID)
{
    UINT32 ret;

    // 释放共享内存段数组占用的系统内存
    (VOID)LOS_MemFree((VOID *)OS_SYS_MEM_ADDR, IPC_SHM_SEGS);
    IPC_SHM_SEGS = NULL;  // 置空指针，防止野指针访问

    // 销毁共享内存操作互斥锁
    ret = LOS_MuxDestroy(&IPC_SHM_SYS_VSHM_MUTEX);
    if (ret != LOS_OK) {
        return -1;  // 互斥锁销毁失败
    }

    return 0;  // 去初始化成功
}

/**
 * @brief 设置共享内存段物理页的共享标志
 * @param[in] seg 共享内存段控制结构指针
 * @note 用于标记物理页为共享状态，实现进程间内存共享
 */
STATIC inline VOID ShmSetSharedFlag(struct shmIDSource *seg)
{
    LosVmPage *page = NULL;  // 物理页结构指针

    // 遍历段的所有物理页，设置共享标志
    LOS_DL_LIST_FOR_EACH_ENTRY(page, &seg->node, LosVmPage, node) {
        OsSetPageShared(page);  // 设置物理页共享属性
    }
}

/**
 * @brief 清除共享内存段物理页的共享标志
 * @param[in] seg 共享内存段控制结构指针
 * @note 用于取消物理页的共享状态，释放物理内存资源
 */
STATIC inline VOID ShmClearSharedFlag(struct shmIDSource *seg)
{
    LosVmPage *page = NULL;

    // 遍历段的所有物理页，清除共享标志
    LOS_DL_LIST_FOR_EACH_ENTRY(page, &seg->node, LosVmPage, node) {
        OsCleanPageShared(page);  // 清除物理页共享属性
    }
}

/**
 * @brief 减少共享内存段物理页的引用计数
 * @param[in] seg 共享内存段控制结构指针
 * @note 当进程 detach 共享内存时调用，引用计数为0时可释放物理页
 */
STATIC VOID ShmPagesRefDec(struct shmIDSource *seg)
{
    LosVmPage *page = NULL;

    // 遍历段的所有物理页，原子减少引用计数
    LOS_DL_LIST_FOR_EACH_ENTRY(page, &seg->node, LosVmPage, node) {
        LOS_AtomicDec(&page->refCounts);  // 原子操作，防止并发计数错误
    }
}

/**
 * @brief 共享内存段分配前的合法性检查
 * @param[in] key 共享内存键值
 * @param[in,out] size 请求的共享内存大小，返回页对齐后的大小
 * @param[out] segNum 可用的共享内存段索引
 * @return 成功返回0，失败返回错误码（-EINVAL/-ENOMEM/-ENOSPC）
 * @note 检查包括大小范围、系统资源限制和段可用性
 */
STATIC INT32 ShmAllocSegCheck(key_t key, size_t *size, INT32 *segNum)
{
    INT32 i;
    // 检查请求大小是否在合法范围内
    if ((*size == 0) || (*size < IPC_SHM_INFO.shmmin) ||
        (*size > IPC_SHM_INFO.shmmax)) {
        return -EINVAL;  // 大小无效
    }

    *size = LOS_Align(*size, PAGE_SIZE);  // 将大小页对齐
    // 检查系统剩余共享内存是否充足
    if ((IPC_SHM_USED_PAGE_COUNT + (*size >> PAGE_SHIFT)) > IPC_SHM_INFO.shmall) {
        return -ENOMEM;  // 内存不足
    }

#ifdef LOSCFG_KERNEL_IPC_PLIMIT  // 启用IPC资源限制时
    // 检查进程IPC资源限制
    if (OsIPCLimitShmAlloc(*size) != LOS_OK) {
        return -ENOMEM;  // 超出进程资源限制
    }
#endif

    // 查找空闲的共享内存段
    for (i = 0; i < IPC_SHM_INFO.shmmni; i++) {
        if (IPC_SHM_SEGS[i].status & SHM_SEG_FREE) {
            IPC_SHM_SEGS[i].status &= ~SHM_SEG_FREE;  // 标记段为已使用
            *segNum = i;                             // 返回找到的段索引
            break;
        }
    }

    if (*segNum < 0) {
        return -ENOSPC;  // 没有可用的共享内存段
    }
    return 0;  // 检查通过
}

/**
 * @brief 分配共享内存段
 * @param[in] key 共享内存键值
 * @param[in] size 请求的共享内存大小
 * @param[in] shmflg 共享内存创建标志（包含权限信息）
 * @return 成功返回共享内存段ID，失败返回错误码
 * @note 完成物理内存分配、段元数据初始化和权限设置
 */
STATIC INT32 ShmAllocSeg(key_t key, size_t size, INT32 shmflg)
{
    INT32 segNum = -1;  // 共享内存段索引
    struct shmIDSource *seg = NULL;  // 段控制结构指针
    size_t count;       // 实际分配的物理页数

    // 分配前检查（大小合法性、资源可用性等）
    INT32 ret = ShmAllocSegCheck(key, &size, &segNum);
    if (ret < 0) {
        return ret;  // 检查失败，返回错误码
    }

    seg = &IPC_SHM_SEGS[segNum];  // 获取段控制结构指针
    // 分配物理内存页
    count = LOS_PhysPagesAlloc(size >> PAGE_SHIFT, &seg->node);
    if (count != (size >> PAGE_SHIFT)) {
        (VOID)LOS_PhysPagesFree(&seg->node);  // 分配失败，释放已分配的页
        seg->status = SHM_SEG_FREE;           // 重置段状态为空闲
#ifdef LOSCFG_KERNEL_IPC_PLIMIT
        OsIPCLimitShmFree(size);  // 释放IPC资源限制计数
#endif
        return -ENOMEM;  // 返回内存不足错误
    }

    ShmSetSharedFlag(seg);  // 设置物理页共享标志
    // 更新系统已使用共享内存页数
    IPC_SHM_USED_PAGE_COUNT += size >> PAGE_SHIFT;

    // 初始化段状态和元数据
    seg->status |= SHM_SEG_USED;  // 标记段为已使用
    seg->ds.shm_perm.mode = (UINT32)shmflg & ACCESSPERMS;  // 设置访问权限
    seg->ds.shm_perm.key = key;                            // 共享内存键值
    seg->ds.shm_segsz = size;                              // 段大小（页对齐后）
    seg->ds.shm_perm.cuid = LOS_GetUserID();               // 创建者用户ID
    seg->ds.shm_perm.uid = LOS_GetUserID();                // 所有者用户ID
    seg->ds.shm_perm.cgid = LOS_GetGroupID();              // 创建者组ID
    seg->ds.shm_perm.gid = LOS_GetGroupID();               // 所有者组ID
    seg->ds.shm_lpid = 0;                                  // 最后操作进程ID
    seg->ds.shm_nattch = 0;                                // 附加计数（当前无进程附加）
    seg->ds.shm_cpid = LOS_GetCurrProcessID();             // 创建进程ID
    seg->ds.shm_atime = 0;                                 // 最后附加时间（未附加）
    seg->ds.shm_dtime = 0;                                 // 最后分离时间（未分离）
    seg->ds.shm_ctime = time(NULL);                        // 创建时间
#ifdef LOSCFG_SHELL
    // 记录所有者进程名称（用于Shell命令显示）
    (VOID)memcpy_s(seg->ownerName, OS_PCB_NAME_LEN, OsCurrProcessGet()->processName, OS_PCB_NAME_LEN);
#endif

    return segNum;  // 返回共享内存段ID
}

/**
 * @brief 释放共享内存段及其物理内存
 * @param[in] seg 共享内存段控制结构指针
 * @param[in,out] shmUsedPageCount 已使用共享内存页数指针
 * @note 清除物理页共享标志，释放物理内存，并更新系统资源计数
 */
STATIC INLINE VOID ShmFreeSeg(struct shmIDSource *seg, UINT32 *shmUsedPageCount)
{
    UINT32 count;  // 实际释放的物理页数

    ShmClearSharedFlag(seg);  // 清除物理页共享标志
    // 释放物理内存页
    count = LOS_PhysPagesFree(&seg->node);
    // 检查释放的页数是否与段大小匹配
    if (count != (seg->ds.shm_segsz >> PAGE_SHIFT)) {
        VM_ERR("free physical pages failed, count = %d, size = %d", count, seg->ds.shm_segsz >> PAGE_SHIFT);
        return;  // 释放失败，输出错误信息
    }
#ifdef LOSCFG_KERNEL_IPC_PLIMIT
    OsIPCLimitShmFree(seg->ds.shm_segsz);  // 释放IPC资源限制计数
#endif
    // 更新系统已使用共享内存页数
    if (shmUsedPageCount != NULL) {
        (*shmUsedPageCount) -= seg->ds.shm_segsz >> PAGE_SHIFT;
    }
    seg->status = SHM_SEG_FREE;  // 重置段状态为空闲
    LOS_ListInit(&seg->node);    // 初始化物理页链表
}

/**
 * @brief 根据键值查找共享内存段
 * @param[in] key 共享内存键值
 * @return 成功返回段索引，失败返回-1
 * @note 遍历所有段，匹配键值和段状态（已使用）
 */
STATIC INT32 ShmFindSegByKey(key_t key)
{
    INT32 i;
    struct shmIDSource *seg = NULL;

    // 遍历所有共享内存段
    for (i = 0; i < IPC_SHM_INFO.shmmni; i++) {
        seg = &IPC_SHM_SEGS[i];
        // 检查段是否已使用且键值匹配
        if ((seg->status & SHM_SEG_USED) &&
            (seg->ds.shm_perm.key == key)) {
            return i;  // 返回找到的段索引
        }
    }

    return -1;  // 未找到匹配的段
}
/**
 * @brief 共享内存段有效性检查
 * @param segNum 要检查的共享内存段编号
 * @param size 请求的内存大小
 * @param shmFlg 创建/访问标志
 * @return 成功返回段编号，失败返回错误码
 * @retval -EINVAL 请求大小超过段实际大小
 * @retval -EEXIST 同时指定IPC_CREAT和IPC_EXCL但段已存在
 * @note 该函数不修改任何全局状态，仅进行参数验证
 */
STATIC INT32 ShmSegValidCheck(INT32 segNum, size_t size, INT32 shmFlg)
{
    struct shmIDSource *seg = &IPC_SHM_SEGS[segNum];  /* 获取共享内存段控制块 */

    /* 检查请求大小是否超过段实际大小 */
    if (size > seg->ds.shm_segsz) {
        return -EINVAL;  /* 无效参数：请求大小超限 */
    }

    /* 检查是否同时指定IPC_CREAT和IPC_EXCL标志 */
    if (((UINT32)shmFlg & (IPC_CREAT | IPC_EXCL)) == (IPC_CREAT | IPC_EXCL)) {
        return -EEXIST;  /* 文件已存在：段已存在且要求创建新段 */
    }

    return segNum;  /* 验证通过，返回段编号 */
}

/**
 * @brief 通过ID查找共享内存段
 * @param shmid 共享内存段ID
 * @return 成功返回段控制块指针，失败返回NULL并设置errno
 * @errno EINVAL shmid无效（超出范围）
 * @errno EIDRM 段已被标记删除或已释放
 * @note 该函数是共享内存系统调用的基础查找函数，需持有SYSV_SHM_LOCK锁调用
 */
STATIC struct shmIDSource *ShmFindSeg(int shmid)
{
    struct shmIDSource *seg = NULL;

    /* 检查shmid是否在有效范围内 */
    if ((shmid < 0) || (shmid >= IPC_SHM_INFO.shmmni)) {
        set_errno(EINVAL);  /* 设置错误号：无效的段ID */
        return NULL;
    }

    seg = &IPC_SHM_SEGS[shmid];  /* 获取段控制块 */
    /* 检查段状态：是否空闲或已标记删除且无进程连接 */
    if ((seg->status & SHM_SEG_FREE) || ((seg->ds.shm_nattch == 0) && (seg->status & SHM_SEG_REMOVE))) {
        set_errno(EIDRM);  /* 设置错误号：段已被移除 */
        return NULL;
    }

    return seg;  /* 返回有效的段控制块 */
}

/**
 * @brief 将物理页面映射到进程虚拟地址空间
 * @param space 目标进程虚拟地址空间
 * @param pageList 物理页面链表
 * @param vaddr 虚拟地址起始位置
 * @param regionFlags 内存区域标志（权限等）
 * @note 该函数会增加物理页面的引用计数
 * @warning 必须在持有相关锁的情况下调用，防止页面被并发修改
 * @performance 循环单次映射一页，大内存块会有性能开销
 */
STATIC VOID ShmVmmMapping(LosVmSpace *space, LOS_DL_LIST *pageList, VADDR_T vaddr, UINT32 regionFlags)
{
    LosVmPage *vmPage = NULL;
    VADDR_T va = vaddr;  /* 当前虚拟地址 */
    PADDR_T pa;          /* 物理地址 */
    STATUS_T ret;

    /* 遍历物理页面链表 */
    LOS_DL_LIST_FOR_EACH_ENTRY(vmPage, pageList, LosVmPage, node) {
        pa = VM_PAGE_TO_PHYS(vmPage);  /* 将页面结构转换为物理地址 */
        LOS_AtomicInc(&vmPage->refCounts);  /* 增加页面引用计数 */
        /* 执行MMU映射：1个页面，使用指定权限 */
        ret = LOS_ArchMmuMap(&space->archMmu, va, pa, 1, regionFlags);
        if (ret != 1) {
            VM_ERR("LOS_ArchMmuMap failed, ret = %d", ret);  /* 映射失败记录错误日志 */
        }
        va += PAGE_SIZE;  /* 移动到下一页虚拟地址 */
    }
}

/**
 * @brief 进程fork时处理共享内存映射
 * @param space 新进程虚拟地址空间
 * @param oldRegion 父进程共享内存区域
 * @param newRegion 子进程共享内存区域
 * @note 共享内存会被子进程继承，保持相同的物理页面映射
 * @synchronization 使用SYSV_SHM_LOCK确保段元数据操作原子性
 * @see ShmVmmMapping 实际执行映射的函数
 */
VOID OsShmFork(LosVmSpace *space, LosVmMapRegion *oldRegion, LosVmMapRegion *newRegion)
{
    struct shmIDSource *seg = NULL;

    SYSV_SHM_LOCK();  /* 获取共享内存系统锁 */
    seg = ShmFindSeg(oldRegion->shmid);  /* 查找共享内存段 */
    if (seg == NULL) {
        SYSV_SHM_UNLOCK();  /* 未找到段，释放锁 */
        VM_ERR("shm fork failed!");  /* 记录错误日志 */
        return;
    }

    /* 复制段ID和标志 */
    newRegion->shmid = oldRegion->shmid;
    newRegion->forkFlags = oldRegion->forkFlags;
    /* 为子进程映射共享内存页面 */
    ShmVmmMapping(space, &seg->node, newRegion->range.base, newRegion->regionFlags);
    seg->ds.shm_nattch++;  /* 增加段连接计数 */
    SYSV_SHM_UNLOCK();  /* 释放共享内存系统锁 */
}

/**
 * @brief 释放共享内存区域
 * @param space 进程虚拟地址空间
 * @param region 要释放的共享内存区域
 * @note 仅当最后一个进程释放且段被标记删除时才真正释放物理内存
 * @synchronization 使用SYSV_SHM_LOCK确保操作原子性
 * @see ShmFreeSeg 实际释放段资源的函数
 */
VOID OsShmRegionFree(LosVmSpace *space, LosVmMapRegion *region)
{
    struct shmIDSource *seg = NULL;

    SYSV_SHM_LOCK();  /* 获取共享内存系统锁 */
    seg = ShmFindSeg(region->shmid);  /* 查找共享内存段 */
    if (seg == NULL) {
        SYSV_SHM_UNLOCK();  /* 未找到段，释放锁 */
        return;
    }

    /* 解除虚拟地址映射 */
    LOS_ArchMmuUnmap(&space->archMmu, region->range.base, region->range.size >> PAGE_SHIFT);
    ShmPagesRefDec(seg);  /* 减少物理页面引用计数 */
    seg->ds.shm_nattch--;  /* 减少段连接计数 */
    /* 检查是否需要释放段：无连接且已标记删除 */
    if (seg->ds.shm_nattch <= 0 && (seg->status & SHM_SEG_REMOVE)) {
        ShmFreeSeg(seg, &IPC_SHM_USED_PAGE_COUNT);  /* 释放段资源 */
    } else {
        seg->ds.shm_dtime = time(NULL);  /* 更新最后分离时间 */
        seg->ds.shm_lpid = LOS_GetCurrProcessID();  /* 记录最后操作进程ID */
    }
    SYSV_SHM_UNLOCK();  /* 释放共享内存系统锁 */
}

/**
 * @brief 检查内存区域是否为共享内存
 * @param region 内存区域
 * @return 是共享内存返回TRUE，否则返回FALSE
 * @note 通过区域标志判断，是轻量级检查
 * @performance O(1)时间复杂度，仅检查标志位
 */
BOOL OsIsShmRegion(LosVmMapRegion *region)
{
    /* 检查区域标志中是否设置了共享内存标志 */
    return (region->regionFlags & VM_MAP_REGION_FLAG_SHM) ? TRUE : FALSE;
}

/**
 * @brief 统计已使用的共享内存段数量
 * @return 已使用的段数量
 * @note 遍历所有段控制块，统计状态为使用中的段
 * @performance O(n)时间复杂度，n为最大段数(SHM_INFO.shmmni)
 */
STATIC INT32 ShmSegUsedCount(VOID)
{
    INT32 i;
    INT32 count = 0;  /* 计数器 */
    struct shmIDSource *seg = NULL;

    /* 遍历所有可能的段 */
    for (i = 0; i < IPC_SHM_INFO.shmmni; i++) {
        seg = &IPC_SHM_SEGS[i];
        if (seg->status & SHM_SEG_USED) {  /* 检查段是否处于使用状态 */
            count++;
        }
    }
    return count;
}

/**
 * @brief 共享内存权限检查
 * @param seg 共享内存段控制块
 * @param mode 请求的访问模式
 * @return 权限检查通过返回0，否则返回错误码
 * @retval EACCES 权限不足
 * @note 实现了POSIX风格的UGO权限模型
 * @security 严格的权限检查可防止未授权访问
 */
STATIC INT32 ShmPermCheck(struct shmIDSource *seg, mode_t mode)
{
    INT32 uid = LOS_GetUserID();  /* 当前用户ID */
    UINT32 tmpMode = 0;           /* 计算得到的有效权限 */
    mode_t privMode = seg->ds.shm_perm.mode;  /* 段的权限位 */
    mode_t accMode;               /* 请求的访问模式（按用户类型转换后） */

    /* 根据用户关系确定权限检查基础 */
    if ((uid == seg->ds.shm_perm.uid) || (uid == seg->ds.shm_perm.cuid)) {
        tmpMode |= SHM_M;  /* 所有者有管理权限 */
        accMode = mode & S_IRWXU;  /* 提取用户权限位 */
    } else if (LOS_CheckInGroups(seg->ds.shm_perm.gid) ||
               LOS_CheckInGroups(seg->ds.shm_perm.cgid)) {
        privMode <<= SHM_GROUPE_TO_USER;  /* 组权限位移到用户位 */
        accMode = (mode & S_IRWXG) << SHM_GROUPE_TO_USER;  /* 提取组权限位并移位 */
    } else {
        privMode <<= SHM_OTHER_TO_USER;  /* 其他用户权限位移到用户位 */
        accMode = (mode & S_IRWXO) << SHM_OTHER_TO_USER;  /* 提取其他用户权限位并移位 */
    }

    /* 构建有效权限掩码 */
    if (privMode & SHM_R) {
        tmpMode |= SHM_R;  /* 读权限 */
    }
    if (privMode & SHM_W) {
        tmpMode |= SHM_W;  /* 写权限 */
    }
    if (privMode & SHM_X) {
        tmpMode |= SHM_X;  /* 执行权限（共享内存较少使用） */
    }

    /* 检查管理权限 */
    if ((mode == SHM_M) && (tmpMode & SHM_M)) {
        return 0;  /* 有管理权限 */
    } else if (mode == SHM_M) {
        return EACCES;  /* 无管理权限 */
    }

    /* 检查访问权限：请求的权限必须完全被允许 */
    if ((tmpMode & accMode) == accMode) {
        return 0;  /* 权限检查通过 */
    } else {
        return EACCES;  /* 权限不足 */
    }
}
/**
 * @brief 创建或获取共享内存段（系统调用实现）
 * @param key 共享内存键值（IPC_PRIVATE表示创建私有段）
 * @param size 请求的内存大小（字节）
 * @param shmflg 操作标志（权限位+控制标志）
 * @return 成功返回共享内存段ID，失败返回-1并设置errno
 * @errno ENOENT 段不存在且未指定IPC_CREAT
 * @errno EEXIST 同时指定IPC_CREAT和IPC_EXCL但段已存在
 * @errno EINVAL 段大小无效或参数非法
 * @errno EACCES 权限不足
 * @note 核心系统调用，对应POSIX标准的shmget()
 * @synchronization 使用SYSV_SHM_LOCK确保操作原子性
 */
INT32 ShmGet(key_t key, size_t size, INT32 shmflg)
{
    INT32 ret;                  /* 函数返回值 */
    INT32 shmid;                /* 共享内存段ID */

    SYSV_SHM_LOCK();            /* 获取共享内存系统锁，确保多进程安全 */

    /* 处理私有共享内存（IPC_PRIVATE） */
    if (key == IPC_PRIVATE) {
        ret = ShmAllocSeg(key, size, shmflg);  /* 分配新的私有共享内存段 */
    } else {
        /* 按键查找已存在的共享内存段 */
        ret = ShmFindSegByKey(key);
        if (ret < 0) {  /* 未找到对应段 */
            /* 检查是否需要创建新段 */
            if (((UINT32)shmflg & IPC_CREAT) == 0) {
                ret = -ENOENT;  /* 未找到且未指定创建标志，返回不存在错误 */
                goto ERROR;     /* 跳转到错误处理 */
            } else {
                ret = ShmAllocSeg(key, size, shmflg);  /* 创建新段 */
            }
        } else {  /* 找到已存在的段 */
            shmid = ret;
            /* 检查是否同时指定了创建和排他标志 */
            if (((UINT32)shmflg & IPC_CREAT) && ((UINT32)shmflg & IPC_EXCL)) {
                ret = -EEXIST;  /* 段已存在，返回文件已存在错误 */
                goto ERROR;
            }
            /* 检查访问权限 */
            ret = ShmPermCheck(ShmFindSeg(shmid), (UINT32)shmflg & ACCESSPERMS);
            if (ret != 0) {
                ret = -ret;     /* 将权限检查结果转换为错误码 */
                goto ERROR;
            }
            /* 验证段大小是否匹配请求 */
            ret = ShmSegValidCheck(shmid, size, shmflg);
        }
    }
    if (ret < 0) {  /* 所有路径的错误汇总检查 */
        goto ERROR;
    }

    SYSV_SHM_UNLOCK();  /* 释放共享内存系统锁 */
    return ret;         /* 返回段ID */

ERROR:                  /* 错误处理标签 */
    set_errno(-ret);    /* 设置全局错误号 */
    SYSV_SHM_UNLOCK();  /* 释放共享内存系统锁 */
    PRINT_DEBUG("%s %d, ret = %d\n", __FUNCTION__, __LINE__, ret);  /* 调试日志输出 */
    return -1;          /* 返回错误 */
}

/**
 * @brief 验证shmat系统调用的参数合法性
 * @param shmaddr 请求附加的虚拟地址（可为NULL）
 * @param shmflg 附加标志
 * @return 成功返回0，失败返回错误码
 * @retval EINVAL 地址未对齐且未指定SHM_RND，或SHM_REMAP标志与NULL地址同时使用
 * @note 该函数仅进行参数验证，不修改系统状态
 * @security 严格的参数检查可防止恶意进程进行地址空间攻击
 */
INT32 ShmatParamCheck(const VOID *shmaddr, INT32 shmflg)
{
    /* 检查SHM_REMAP标志与NULL地址的非法组合 */
    if (((UINT32)shmflg & SHM_REMAP) && (shmaddr == NULL)) {
        return EINVAL;
    }

    /* 检查地址对齐：非NULL地址且未指定SHM_RND时必须页对齐 */
    if ((shmaddr != NULL) && !IS_PAGE_ALIGNED(shmaddr) && (((UINT32)shmflg & SHM_RND) == 0)) {
        return EINVAL;
    }

    return 0;   /* 参数验证通过 */
}

/**
 * @brief 为共享内存分配虚拟地址区域并建立映射
 * @param seg 共享内存段控制块
 * @param shmaddr 请求的附加地址（可为NULL，表示自动分配）
 * @param shmflg 附加标志
 * @param prot 内存保护位（PROT_READ/PROT_WRITE/PROT_EXEC）
 * @return 成功返回虚拟内存区域指针，失败返回NULL并设置errno
 * @errno EINVAL 地址范围冲突或参数无效
 * @errno ENOMEM 虚拟地址空间不足
 * @note 内部辅助函数，被ShmAt()调用
 * @performance 采用区域预分配策略减少内存碎片
 */
LosVmMapRegion *ShmatVmmAlloc(struct shmIDSource *seg, const VOID *shmaddr,
                              INT32 shmflg, UINT32 prot)
{
    LosVmSpace *space = OsCurrProcessGet()->vmSpace;  /* 当前进程虚拟地址空间 */
    LosVmMapRegion *region = NULL;                    /* 虚拟内存区域指针 */
    UINT32 flags = MAP_ANONYMOUS | MAP_SHARED;        /* 映射标志：匿名共享 */
    UINT32 mapFlags = flags | MAP_FIXED;              /* 固定地址映射标志 */
    VADDR_T vaddr;                                    /* 计算后的虚拟地址 */
    UINT32 regionFlags;                               /* 区域标志（含权限） */
    INT32 ret;                                        /* 临时返回值 */

    /* 如果指定了地址，设置不可替换标志防止覆盖现有映射 */
    if (shmaddr != NULL) {
        flags |= MAP_FIXED_NOREPLACE;
    }
    /* 将保护位和标志转换为区域标志 */
    regionFlags = OsCvtProtFlagsToRegionFlags(prot, flags);
    (VOID)LOS_MuxAcquire(&space->regionMux);  /* 获取地址空间区域锁 */

    if (shmaddr == NULL) {  /* 自动分配虚拟地址 */
        /* 分配连续虚拟地址区域 */
        region = LOS_RegionAlloc(space, 0, seg->ds.shm_segsz, regionFlags, 0);
    } else {  /* 使用指定地址 */
        /* 如果指定SHM_RND标志，将地址向下对齐到SHMLBA边界 */
        if ((UINT32)shmflg & SHM_RND) {
            vaddr = ROUNDDOWN((VADDR_T)(UINTPTR)shmaddr, SHMLBA);
        } else {
            vaddr = (VADDR_T)(UINTPTR)shmaddr;  /* 使用原始地址 */
        }

        /* 检查地址范围是否已被占用（非重映射模式下） */
        if (!((UINT32)shmflg & SHM_REMAP) && (LOS_RegionFind(space, vaddr) ||
            LOS_RegionFind(space, vaddr + seg->ds.shm_segsz - 1) ||
            LOS_RegionRangeFind(space, vaddr, seg->ds.shm_segsz - 1))) {
            ret = EINVAL;  /* 地址冲突，返回无效参数错误 */
            goto ERROR;
        }

        /* 执行内存映射 */
        vaddr = (VADDR_T)LOS_MMap(vaddr, seg->ds.shm_segsz, prot, mapFlags, -1, 0);
        region = LOS_RegionFind(space, vaddr);  /* 查找映射后的区域 */
    }

    if (region == NULL) {  /* 区域分配失败 */
        ret = ENOMEM;  /* 内存不足错误 */
        goto ERROR;
    }

    /* 将物理页面映射到虚拟地址 */
    ShmVmmMapping(space, &seg->node, region->range.base, regionFlags);
    (VOID)LOS_MuxRelease(&space->regionMux);  /* 释放地址空间区域锁 */
    return region;                            /* 返回成功分配的区域 */

ERROR:                          /* 错误处理标签 */
    set_errno(ret);              /* 设置错误号 */
    (VOID)LOS_MuxRelease(&space->regionMux);  /* 释放地址空间区域锁 */
    return NULL;                 /* 返回错误 */
}

/**
 * @brief 将共享内存段附加到进程地址空间（系统调用实现）
 * @param shmid 共享内存段ID
 * @param shmaddr 请求附加的地址（可为NULL）
 * @param shmflg 附加标志
 * @return 成功返回附加的虚拟地址，失败返回(void *)-1并设置errno
 * @errno EINVAL 参数无效或段不存在
 * @errno EACCES 权限不足
 * @errno ENOMEM 虚拟地址空间不足
 * @note 对应POSIX标准的shmat()系统调用
 * @synchronization 使用SYSV_SHM_LOCK确保操作原子性
 */
VOID *ShmAt(INT32 shmid, const VOID *shmaddr, INT32 shmflg)
{
    INT32 ret;                          /* 临时返回值 */
    UINT32 prot = PROT_READ;            /* 内存保护位，默认只读 */
    mode_t acc_mode = SHM_S_IRUGO;      /* 访问模式，默认只读 */
    struct shmIDSource *seg = NULL;     /* 共享内存段控制块 */
    LosVmMapRegion *r = NULL;           /* 虚拟内存区域 */

    /* 验证附加参数合法性 */
    ret = ShmatParamCheck(shmaddr, shmflg);
    if (ret != 0) {
        set_errno(ret);
        return (VOID *)-1;
    }

    /* 根据标志设置内存保护位和访问模式 */
    if ((UINT32)shmflg & SHM_EXEC) {
        prot |= PROT_EXEC;              /* 可执行权限 */
        acc_mode |= SHM_S_IXUGO;
    } else if (((UINT32)shmflg & SHM_RDONLY) == 0) {
        prot |= PROT_WRITE;             /* 可写权限（非只读模式） */
        acc_mode |= SHM_S_IWUGO;
    }

    SYSV_SHM_LOCK();                    /* 获取共享内存系统锁 */
    seg = ShmFindSeg(shmid);            /* 查找共享内存段 */
    if (seg == NULL) {                  /* 段不存在或已被删除 */
        SYSV_SHM_UNLOCK();
        return (VOID *)-1;
    }

    /* 检查访问权限 */
    ret = ShmPermCheck(seg, acc_mode);
    if (ret != 0) {
        goto ERROR;
    }

    seg->ds.shm_nattch++;               /* 增加段连接计数 */
    /* 分配虚拟地址并建立映射 */
    r = ShmatVmmAlloc(seg, shmaddr, shmflg, prot);
    if (r == NULL) {                    /* 映射失败 */
        seg->ds.shm_nattch--;           /* 恢复连接计数 */
        SYSV_SHM_UNLOCK();
        return (VOID *)-1;
    }

    /* 设置区域属性 */
    r->shmid = shmid;
    r->regionFlags |= VM_MAP_REGION_FLAG_SHM;  /* 标记为共享内存区域 */
    seg->ds.shm_atime = time(NULL);           /* 更新最后附加时间 */
    seg->ds.shm_lpid = LOS_GetCurrProcessID(); /* 记录最后操作进程ID */
    SYSV_SHM_UNLOCK();                        /* 释放共享内存系统锁 */

    return (VOID *)(UINTPTR)r->range.base;     /* 返回附加的虚拟地址 */

ERROR:                          /* 错误处理标签 */
    set_errno(ret);              /* 设置错误号 */
    SYSV_SHM_UNLOCK();           /* 释放共享内存系统锁 */
    PRINT_DEBUG("%s %d, ret = %d\n", __FUNCTION__, __LINE__, ret);
    return (VOID *)-1;           /* 返回错误 */
}

/**
 * @brief 控制共享内存段属性（系统调用实现）
 * @param shmid 共享内存段ID
 * @param cmd 控制命令
 * @param buf 命令参数缓冲区（用户空间）
 * @return 成功返回0或命令特定值，失败返回-1并设置errno
 * @errno EINVAL 无效命令或参数
 * @errno EACCES 权限不足
 * @errno EFAULT 用户缓冲区访问失败
 * @errno EPERM 操作不允许
 * @note 对应POSIX标准的shmctl()系统调用，支持多种控制命令
 * @synchronization 使用SYSV_SHM_LOCK确保操作原子性
 */
INT32 ShmCtl(INT32 shmid, INT32 cmd, struct shmid_ds *buf)
{
    struct shmIDSource *seg = NULL;     /* 共享内存段控制块 */
    INT32 ret = 0;                      /* 函数返回值 */
    struct shm_info shmInfo = { 0 };    /* SHM_INFO命令返回的信息结构体 */
    struct ipc_perm shm_perm;           /* IPC_SET命令使用的权限结构体 */

    cmd = ((UINT32)cmd & ~IPC_64);      /* 清除64位标志，兼容32位接口 */

    SYSV_SHM_LOCK();                    /* 获取共享内存系统锁 */

    /* 除IPC_INFO和SHM_INFO外，其他命令需要查找段 */
    if ((cmd != IPC_INFO) && (cmd != SHM_INFO)) {
        seg = ShmFindSeg(shmid);        /* 查找共享内存段 */
        if (seg == NULL) {              /* 段不存在或已被删除 */
            SYSV_SHM_UNLOCK();
            return -1;
        }
    }

    /* 检查缓冲区是否为NULL（IPC_RMID命令不需要缓冲区） */
    if ((buf == NULL) && (cmd != IPC_RMID)) {
        ret = EINVAL;
        goto ERROR;
    }

    /* 根据命令类型执行不同操作 */
    switch (cmd) {
        case IPC_STAT:                  /* 获取段状态 */
        case SHM_STAT:                  /* 获取段状态（带索引） */
            /* 检查读权限 */
            ret = ShmPermCheck(seg, SHM_S_IRUGO);
            if (ret != 0) {
                goto ERROR;
            }

            /* 将内核空间的段信息复制到用户缓冲区 */
            ret = LOS_ArchCopyToUser(buf, &seg->ds, sizeof(struct shmid_ds));
            if (ret != 0) {
                ret = EFAULT;           /* 用户缓冲区访问失败 */
                goto ERROR;
            }
            /* SHM_STAT命令返回段ID和序列号的组合值 */
            if (cmd == SHM_STAT) {
                ret = (unsigned int)((unsigned int)seg->ds.shm_perm.seq << 16) | 
                      (unsigned int)((unsigned int)shmid & 0xffff); /* 16: 序列号作为高16位 */
            }
            break;

        case IPC_SET:                   /* 设置段属性 */
            /* 检查管理权限 */
            ret = ShmPermCheck(seg, SHM_M);
            if (ret != 0) {
                ret = EPERM;            /* 权限不足 */
                goto ERROR;
            }

            /* 从用户空间复制新的权限设置 */
            ret = LOS_ArchCopyFromUser(&shm_perm, &buf->shm_perm, sizeof(struct ipc_perm));
            if (ret != 0) {
                ret = EFAULT;           /* 用户缓冲区访问失败 */
                goto ERROR;
            }

            /* 更新段的UID、GID和权限位 */
            seg->ds.shm_perm.uid = shm_perm.uid;
            seg->ds.shm_perm.gid = shm_perm.gid;
            seg->ds.shm_perm.mode = (seg->ds.shm_perm.mode & ~ACCESSPERMS) | 
                                   (shm_perm.mode & ACCESSPERMS);
            seg->ds.shm_ctime = time(NULL);  /* 更新最后修改时间 */

#ifdef LOSCFG_SHELL                       /* 条件编译：如果启用Shell */
            /* 检查UID有效性 */
            if (OsProcessIDUserCheckInvalid(shm_perm.uid)) {
                break;
            }
            /* 更新所有者名称 */
            (VOID)memcpy_s(seg->ownerName, OS_PCB_NAME_LEN, 
                           OS_PCB_FROM_PID(shm_perm.uid)->processName, OS_PCB_NAME_LEN);
#endif
            break;

        case IPC_RMID:                  /* 标记删除段 */
            /* 检查管理权限 */
            ret = ShmPermCheck(seg, SHM_M);
            if (ret != 0) {
                ret = EPERM;            /* 权限不足 */
                goto ERROR;
            }

            seg->status |= SHM_SEG_REMOVE;  /* 设置删除标记 */
            /* 如果没有进程连接，立即释放段 */
            if (seg->ds.shm_nattch <= 0) {
                ShmFreeSeg(seg, &IPC_SHM_USED_PAGE_COUNT);
            }
            break;

        case IPC_INFO:                  /* 获取系统共享内存信息 */
            /* 将系统信息复制到用户缓冲区 */
            ret = LOS_ArchCopyToUser(buf, &IPC_SHM_INFO, sizeof(struct shminfo));
            if (ret != 0) {
                ret = EFAULT;           /* 用户缓冲区访问失败 */
                goto ERROR;
            }
            ret = IPC_SHM_INFO.shmmni;  /* 返回最大段数 */
            break;

        case SHM_INFO:                  /* 获取共享内存统计信息 */
            /* 初始化统计信息结构体（当前版本未实现交换相关统计） */
            shmInfo.shm_rss = 0;        /* 驻留页计数 */
            shmInfo.shm_swp = 0;        /* 交换页计数 */
            shmInfo.shm_tot = 0;        /* 总内存使用 */
            shmInfo.swap_attempts = 0;  /* 交换尝试次数 */
            shmInfo.swap_successes = 0; /* 交换成功次数 */
            shmInfo.used_ids = ShmSegUsedCount(); /* 当前使用的段数量 */

            /* 将统计信息复制到用户缓冲区 */
            ret = LOS_ArchCopyToUser(buf, &shmInfo, sizeof(struct shm_info));
            if (ret != 0) {
                ret = EFAULT;           /* 用户缓冲区访问失败 */
                goto ERROR;
            }
            ret = IPC_SHM_INFO.shmmni;  /* 返回最大段数 */
            break;

        default:                        /* 不支持的命令 */
            VM_ERR("the cmd(%d) is not supported!", cmd);
            ret = EINVAL;
            goto ERROR;
    }

    SYSV_SHM_UNLOCK();  /* 释放共享内存系统锁 */
    return ret;         /* 返回命令执行结果 */

ERROR:                  /* 错误处理标签 */
    set_errno(ret);     /* 设置错误号 */
    SYSV_SHM_UNLOCK();  /* 释放共享内存系统锁 */
    PRINT_DEBUG("%s %d, ret = %d\n", __FUNCTION__, __LINE__, ret);
    return -1;          /* 返回错误 */
}
/**
 * @brief 分离共享内存段与进程地址空间
 * @details 从当前进程的虚拟地址空间中移除共享内存映射，更新段引用计数，必要时释放物理内存
 * @param[in] shmaddr 要分离的共享内存起始地址（必须页对齐）
 * @retval 0 分离成功
 * @retval -1 分离失败，具体错误见errno
 * @note 调用者需确保地址有效且属于共享内存区域
 * @attention 必须持有进程地址空间锁进行区域操作
 */
INT32 ShmDt(const VOID *shmaddr)
{
    LosVmSpace *space = OsCurrProcessGet()->vmSpace;  /* 获取当前进程虚拟地址空间 */
    struct shmIDSource *seg = NULL;                   /* 共享内存段控制块指针 */
    LosVmMapRegion *region = NULL;                    /* 虚拟内存映射区域结构 */
    INT32 shmid;                                      /* 共享内存段ID */
    INT32 ret;                                        /* 返回值 */

    /* 检查共享内存地址是否页对齐 - 内存安全检查 */
    if (IS_PAGE_ALIGNED(shmaddr) == 0) {
        ret = EINVAL;
        goto ERROR;  /* 地址未对齐，跳转至错误处理 */
    }

    (VOID)LOS_MuxAcquire(&space->regionMux);          /* 获取地址空间区域锁 - 并发控制 */
    /* 查找虚拟地址对应的映射区域 */
    region = LOS_RegionFind(space, (VADDR_T)(UINTPTR)shmaddr);
    /* 区域不存在或非共享内存区域 - 参数有效性检查 */
    if ((region == NULL) || !OsIsShmRegion(region)) {
        ret = EINVAL;
        goto ERROR_WITH_LOCK;  /* 区域无效，带锁跳转至错误处理 */
    }
    shmid = region->shmid;  /* 获取共享内存段ID */

    /* 验证找到的区域是否与请求地址完全匹配 */
    if (region->range.base != (VADDR_T)(UINTPTR)shmaddr) {
        ret = EINVAL;
        goto ERROR_WITH_LOCK;  /* 地址不匹配，带锁跳转至错误处理 */
    }

    /* 从地址空间移除区域 - 内存管理核心操作 */
    LOS_RbDelNode(&space->regionRbTree, &region->rbNode);
    /* 解除MMU映射 - 硬件层面内存解除 */
    LOS_ArchMmuUnmap(&space->archMmu, region->range.base, region->range.size >> PAGE_SHIFT);
    (VOID)LOS_MuxRelease(&space->regionMux);          /* 释放地址空间区域锁 */
    free(region);  /* 释放区域结构内存 - 防止内存泄漏 */

    SYSV_SHM_LOCK();  /* 获取共享内存系统锁 - 跨进程同步 */
    seg = ShmFindSeg(shmid);  /* 通过ID查找共享内存段 */
    if (seg == NULL) {  /* 段不存在检查 */
        ret = EINVAL;
        SYSV_SHM_UNLOCK();
        goto ERROR;
    }

    ShmPagesRefDec(seg);  /* 减少物理页面引用计数 - COW机制支持 */
    seg->ds.shm_nattch--; /* 减少连接进程数 - 资源跟踪 */
    /* 最后一个进程分离且标记为删除 - 延迟释放机制 */
    if ((seg->ds.shm_nattch <= 0) && (seg->status & SHM_SEG_REMOVE)) {
        ShmFreeSeg(seg, &IPC_SHM_USED_PAGE_COUNT);  /* 释放共享内存段 */
    } else {
        seg->ds.shm_dtime = time(NULL);  /* 更新最后分离时间戳 */
        seg->ds.shm_lpid = LOS_GetCurrProcessID(); /* 记录最后分离进程ID */
    }
    SYSV_SHM_UNLOCK();  /* 释放共享内存系统锁 */

    return 0;  /* 成功返回 */

ERROR_WITH_LOCK:  /* 带锁错误处理标签 */
    (VOID)LOS_MuxRelease(&space->regionMux);
ERROR:  /* 错误处理标签 */
    set_errno(ret);  /* 设置错误码 */
    PRINT_DEBUG("%s %d, ret = %d\n", __FUNCTION__, __LINE__, ret);  /* 调试信息输出 */
    return -1;
}

/**
 * @brief 共享内存段销毁回调函数
 * @details 遍历并释放所有活动共享内存段，清理控制结构和同步原语
 * @param[in] shmSegs 共享内存段数组
 * @param[in] shmInfo 共享内存系统信息
 * @param[in] sysvShmMux 共享内存系统锁
 * @note 系统关闭或共享内存模块卸载时调用
 * @warning 必须确保所有使用中的共享内存已被分离
 */
VOID OsShmCBDestroy(struct shmIDSource *shmSegs, struct shminfo *shmInfo, LosMux *sysvShmMux)
{
    if ((shmSegs == NULL) || (shmInfo == NULL) || (sysvShmMux == NULL)) {
        return;  /* 参数合法性检查 - 防止空指针解引用 */
    }

    /* 遍历所有可能的共享内存段 */
    for (UINT32 index = 0; index < shmInfo->shmmni; index++) {
        struct shmIDSource *seg = &shmSegs[index];
        if (seg->status == SHM_SEG_FREE) {
            continue;  /* 跳过空闲段 */
        }

        (VOID)LOS_MuxLock(sysvShmMux, LOS_WAIT_FOREVER);  /* 获取系统锁 */
        ShmFreeSeg(seg, NULL);  /* 释放共享内存段 - 不跟踪页面计数 */
        (VOID)LOS_MuxUnlock(sysvShmMux);  /* 释放系统锁 */
    }

    (VOID)LOS_MemFree((VOID *)OS_SYS_MEM_ADDR, shmSegs);  /* 释放段数组内存 */
    (VOID)LOS_MuxDestroy(sysvShmMux);  /* 销毁系统锁 - 资源清理 */
}

#ifdef LOSCFG_SHELL  /* 条件编译：Shell命令支持 */
/**
 * @brief 显示共享内存段信息命令实现
 * @details 遍历所有活动共享内存段，格式化输出段状态、权限、大小等信息
 * @note 仅当配置LOSCFG_SHELL时可用
 * @attention 需要持有共享内存系统锁以保证数据一致性
 */
STATIC VOID OsShmInfoCmd(VOID)
{
    INT32 i;
    struct shmIDSource *seg = NULL;

    PRINTK("\r\n------- Shared Memory Segments -------\n");
    /* 表头输出 - 格式化对齐 */
    PRINTK("key      shmid    perms      bytes      nattch     status     owner\n");
    SYSV_SHM_LOCK();  /* 加锁防止并发修改 */
    for (i = 0; i < IPC_SHM_INFO.shmmni; i++) {  /* 遍历所有可能的段ID */
        seg = &IPC_SHM_SEGS[i];
        if (!(seg->status & SHM_SEG_USED)) {  /* 跳过未使用段 */
            continue;
        }
        /* 格式化输出段信息 - 包含关键元数据 */
        PRINTK("%08x %-8d %-10o %-10u %-10u %-10x %s\n",
               seg->ds.shm_perm.key,    /* 共享内存键 */
               i,                       /* 段ID */
               seg->ds.shm_perm.mode,   /* 权限位 */
               seg->ds.shm_segsz,       /* 段大小(字节) */
               seg->ds.shm_nattch,      /* 连接进程数 */
               seg->status,             /* 段状态标志 */
               seg->ownerName);         /* 所有者名称 */
    }
    SYSV_SHM_UNLOCK();  /* 释放系统锁 */
}

/**
 * @brief 删除共享内存段命令实现
 * @details 根据shmid查找并释放共享内存段，仅允许删除无连接进程的段
 * @param[in] shmid 要删除的共享内存段ID
 * @note 实际释放操作受ShmFreeSeg控制
 * @warning 强制删除可能导致使用中进程崩溃
 */
STATIC VOID OsShmDeleteCmd(INT32 shmid)
{
    struct shmIDSource *seg = NULL;

    /* 边界检查 - 输入验证 */
    if ((shmid < 0) || (shmid >= IPC_SHM_INFO.shmmni)) {
        PRINT_ERR("shmid is invalid: %d\n", shmid);
        return;
    }

    SYSV_SHM_LOCK();
    seg = ShmFindSeg(shmid);  /* 查找共享内存段 */
    if (seg == NULL) {  /* 段不存在检查 */
        SYSV_SHM_UNLOCK();
        return;
    }

    /* 仅当无连接进程时才释放 - 防止数据不一致 */
    if (seg->ds.shm_nattch <= 0) {
        ShmFreeSeg(seg, &IPC_SHM_USED_PAGE_COUNT);
    }
    SYSV_SHM_UNLOCK();
}

/**
 * @brief 共享内存Shell命令用法说明
 * @details 输出shm命令的选项、参数和功能说明
 * @note 遵循LiteOS Shell命令规范
 */
STATIC VOID OsShmCmdUsage(VOID)
{
    PRINTK("\tnone option,   print shm usage info\n"
           "\t-r [shmid],    Recycle the specified shared memory about shmid\n"
           "\t-h | --help,   print shm command usage\n");
}

/**
 * @brief 共享内存Shell命令处理函数
 * @details 解析命令行参数，调度执行对应的共享内存操作
 * @param[in] argc 参数数量
 * @param[in] argv 参数数组
 * @retval 0 成功
 * @retval -1 失败
 * @note 支持"shm"、"shm -h"和"shm -r <shmid>"三种用法
 * @attention 参数解析需严格验证防止注入攻击
 */
UINT32 OsShellCmdShm(INT32 argc, const CHAR *argv[])
{
    INT32 shmid;
    CHAR *endPtr = NULL;

    if (argc == 0) {  /* 无参数 - 显示共享内存信息 */
        OsShmInfoCmd();
    } else if (argc == 1) {  /* 一个参数 - 帮助命令 */
        if ((strcmp(argv[0], "-h") != 0) && (strcmp(argv[0], "--help") != 0)) {
            PRINTK("Invalid option: %s\n", argv[0]);
        }
        OsShmCmdUsage();
    } else if (argc == 2) {  /* 两个参数 - 删除命令 */
        if (strcmp(argv[0], "-r") != 0) {  /* 验证选项 */
            PRINTK("Invalid option: %s\n", argv[0]);
            goto DONE;
        }
        /* 字符串转整数 - 输入安全检查 */
        shmid = strtoul((CHAR *)argv[1], &endPtr, 0);
        if ((endPtr == NULL) || (*endPtr != 0)) {  /* 验证转换完整性 */
            PRINTK("check shmid %s(us) invalid\n", argv[1]);
            goto DONE;
        }
        OsShmDeleteCmd(shmid);  /* 执行删除操作 */
    }
    return 0;
DONE:  /* 命令错误处理出口 */
    OsShmCmdUsage();
    return -1;
}

/* Shell命令注册 - 将shm命令添加到系统命令表 */
SHELLCMD_ENTRY(shm_shellcmd, CMD_TYPE_SHOW, "shm", 2, (CmdCallBackFunc)OsShellCmdShm);
#endif
#endif

