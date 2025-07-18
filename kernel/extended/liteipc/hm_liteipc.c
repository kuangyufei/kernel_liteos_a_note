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
 * @file    hm_liteipc.c
 * @brief
 * @link
   @verbatim
   @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2022-1-29
 */
/*!
 * @file    hm_liteipc.c
 * @brief 轻量级进程间通信
 * @link LiteIPC http://weharmonyos.com/openharmony/zh-cn/device-dev/kernel/kernel-small-bundles-ipc.html @endlink
   @verbatim
   基本概念
	   LiteIPC是OpenHarmony LiteOS-A内核提供的一种新型IPC（Inter-Process Communication，即进程间通信）机制，
	   不同于传统的System V IPC机制，LiteIPC主要是为RPC（Remote Procedure Call，即远程过程调用）而设计的，
	   而且是通过设备文件的方式对上层提供接口的，而非传统的API函数方式。
	   
	   LiteIPC中有两个主要概念，一个是ServiceManager，另一个是Service。整个系统只能有一个ServiceManager，
	   而Service可以有多个。ServiceManager有两个主要功能：一是负责Service的注册和注销，二是负责管理Service的
	   访问权限（只有有权限的任务（Task）可以向对应的Service发送IPC消息）。
   
   运行机制
	   首先将需要接收IPC消息的任务通过ServiceManager注册成为一个Service，然后通过ServiceManager为该Service
	   任务配置访问权限，即指定哪些任务可以向该Service任务发送IPC消息。LiteIPC的核心思想就是在内核态为
	   每个Service任务维护一个IPC消息队列，该消息队列通过LiteIPC设备文件向上层用户态程序分别提供代表收取
	   IPC消息的读操作和代表发送IPC消息的写操作。
   @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2025-07-18
 */

#include "hm_liteipc.h"
#include "linux/kernel.h"
#include "fs/file.h"
#include "fs/driver.h"
#include "los_init.h"
#include "los_mp.h"
#include "los_mux.h"
#include "los_process_pri.h"
#include "los_sched_pri.h"
#include "los_spinlock.h"
#include "los_task_pri.h"
#include "los_vm_lock.h"
#include "los_vm_map.h"
#include "los_vm_page.h"
#include "los_vm_phys.h"
#include "los_hook.h"


#define USE_TASKID_AS_HANDLE 1          // 是否使用任务ID作为句柄：1-启用，0-禁用
#define USE_MMAP 1                      // 是否启用内存映射：1-启用，0-禁用
#define IPC_IO_DATA_MAX 8192UL          // IPC IO数据最大长度（字节）
#define IPC_MSG_DATA_SZ_MAX (IPC_IO_DATA_MAX * sizeof(SpecialObj) / (sizeof(SpecialObj) + sizeof(size_t)))  // IPC消息数据区最大大小
#define IPC_MSG_OBJECT_NUM_MAX (IPC_MSG_DATA_SZ_MAX / sizeof(SpecialObj))  // IPC消息最大对象数量

#define LITE_IPC_POOL_NAME "liteipc"    // IPC内存池名称
#define LITE_IPC_POOL_PAGE_MAX_NUM 64 /* 256KB */  // 内存池最大页数（256KB）
#define LITE_IPC_POOL_PAGE_DEFAULT_NUM 16 /* 64KB */  // 内存池默认页数（64KB）
#define LITE_IPC_POOL_MAX_SIZE (LITE_IPC_POOL_PAGE_MAX_NUM << PAGE_SHIFT)  // 内存池最大大小
#define LITE_IPC_POOL_DEFAULT_SIZE (LITE_IPC_POOL_PAGE_DEFAULT_NUM << PAGE_SHIFT)  // 内存池默认大小
#define LITE_IPC_POOL_UVADDR 0x10000000  // 内存池用户空间虚拟地址
#define INVAILD_ID (-1)                 // 无效ID值

#define LITEIPC_TIMEOUT_MS 5000UL       // IPC超时时间（毫秒）
#define LITEIPC_TIMEOUT_NS 5000000000ULL  // IPC超时时间（纳秒）

#define MAJOR_VERSION (2)               // 主版本号
#define MINOR_VERSION (0)               // 次版本号
#define DRIVER_VERSION (MAJOR_VERSION | MINOR_VERSION << 16)  // 驱动版本号（主版本号低16位，次版本号高16位）

/**
 * @brief IPC已使用节点结构体
 * @details 用于管理IPC内存池中已分配的内存节点
 */
typedef struct {
    LOS_DL_LIST list;                   // 双向链表节点
    VOID *ptr;                          // 内存块指针
} IpcUsedNode;

STATIC LosMux g_serviceHandleMapMux;    // 服务句柄映射互斥锁
#if (USE_TASKID_AS_HANDLE == 1)        // 如果使用任务ID作为句柄
STATIC HandleInfo g_cmsTask;           // CMS任务句柄信息
#else
STATIC HandleInfo g_serviceHandleMap[MAX_SERVICE_NUM];  // 服务句柄映射表
#endif
STATIC LOS_DL_LIST g_ipcPendlist;      // IPC等待链表

/* ipc lock */
SPIN_LOCK_INIT(g_ipcSpin);             // IPC自旋锁初始化
#define IPC_LOCK(state)       LOS_SpinLockSave(&g_ipcSpin, &(state))  // 获取IPC自旋锁并保存中断状态
#define IPC_UNLOCK(state)     LOS_SpinUnlockRestore(&g_ipcSpin, state)  // 恢复中断状态并释放IPC自旋锁

/**
 * @brief 打开IPC设备
 * @param[in] filep 文件操作结构体指针
 * @return 0-成功，非0-错误码
 */
STATIC int LiteIpcOpen(struct file *filep);
/**
 * @brief 关闭IPC设备
 * @param[in] filep 文件操作结构体指针
 * @return 0-成功，非0-错误码
 */
STATIC int LiteIpcClose(struct file *filep);
/**
 * @brief IPC设备控制命令
 * @param[in] filep 文件操作结构体指针
 * @param[in] cmd 命令码
 * @param[in] arg 命令参数
 * @return 0-成功，非0-错误码
 */
STATIC int LiteIpcIoctl(struct file *filep, int cmd, unsigned long arg);
/**
 * @brief IPC内存映射
 * @param[in] filep 文件操作结构体指针
 * @param[in] region 内存映射区域结构体
 * @return 0-成功，非0-错误码
 */
STATIC int LiteIpcMmap(struct file* filep, LosVmMapRegion *region);
/**
 * @brief IPC消息写入
 * @param[in] content IPC内容结构体指针
 * @return 0-成功，非0-错误码
 */
STATIC UINT32 LiteIpcWrite(IpcContent *content);
/**
 * @brief 获取任务ID
 * @param[in] serviceHandle 服务句柄
 * @param[out] taskID 任务ID输出指针
 * @return 0-成功，非0-错误码
 */
STATIC UINT32 GetTid(UINT32 serviceHandle, UINT32 *taskID);
/**
 * @brief 处理特殊对象
 * @param[in] dstTid 目标任务ID
 * @param[in] node IPC链表节点指针
 * @param[in] isRollback 是否回滚操作
 * @return 0-成功，非0-错误码
 */
STATIC UINT32 HandleSpecialObjects(UINT32 dstTid, IpcListNode *node, BOOL isRollback);
/**
 * @brief 创建IPC内存池
 * @return 成功返回ProcIpcInfo指针，失败返回NULL
 */
STATIC ProcIpcInfo *LiteIpcPoolCreate(VOID);

/**
 * @brief IPC文件操作结构体
 * @details 定义LiteIPC设备的文件操作接口
 */
STATIC const struct file_operations_vfs g_liteIpcFops = {
    .open = LiteIpcOpen,   /* 打开设备 */
    .close = LiteIpcClose,  /* 关闭设备 */
    .ioctl = LiteIpcIoctl,  /* 控制命令 */
    .mmap = LiteIpcMmap,   /* 内存映射 */
};

/**
 * @brief LiteIPC模块初始化
 * @details 初始化句柄映射表、互斥锁，注册设备驱动
 * @return 0-成功，非0-错误码
 */
LITE_OS_SEC_TEXT_INIT UINT32 OsLiteIpcInit(VOID)
{
    UINT32 ret;
#if (USE_TASKID_AS_HANDLE == 1)        // 如果使用任务ID作为句柄
    g_cmsTask.status = HANDLE_NOT_USED;  // 初始化CMS任务状态为未使用
#else
    (void)memset_s(g_serviceHandleMap, sizeof(g_serviceHandleMap), 0, sizeof(g_serviceHandleMap));  // 初始化服务句柄映射表
#endif
    ret = LOS_MuxInit(&g_serviceHandleMapMux, NULL);  // 初始化服务句柄映射互斥锁
    if (ret != LOS_OK) {
        return ret;                     // 互斥锁初始化失败，返回错误码
    }
    ret = (UINT32)register_driver(LITEIPC_DRIVER, &g_liteIpcFops, LITEIPC_DRIVER_MODE, NULL);  // 注册IPC驱动
    if (ret != LOS_OK) {
        PRINT_ERR("register lite_ipc driver failed:%d\n", ret);  // 驱动注册失败，打印错误信息
    }
    LOS_ListInit(&(g_ipcPendlist));     // 初始化IPC等待链表

    return ret;                         // 返回初始化结果
}

LOS_MODULE_INIT(OsLiteIpcInit, LOS_INIT_LEVEL_KMOD_EXTENDED);  // 注册模块初始化函数，扩展内核模块级别

/**
 * @brief 打开IPC设备
 * @details 为当前进程创建IPC内存池
 * @param[in] filep 文件操作结构体指针
 * @return 0-成功，-ENOMEM-内存分配失败
 */
LITE_OS_SEC_TEXT STATIC int LiteIpcOpen(struct file *filep)
{
    LosProcessCB *pcb = OsCurrProcessGet();  // 获取当前进程控制块
    if (pcb->ipcInfo != NULL) {
        return 0;                       // 已存在IPC信息，直接返回成功
    }

    pcb->ipcInfo = LiteIpcPoolCreate();  // 创建IPC内存池
    if (pcb->ipcInfo == NULL) {
        return -ENOMEM;                 // 内存池创建失败，返回内存不足错误
    }

    return 0;                           // 打开成功
}

/**
 * @brief 关闭IPC设备
 * @details 目前为空实现
 * @param[in] filep 文件操作结构体指针
 * @return 0-成功
 */
LITE_OS_SEC_TEXT STATIC int LiteIpcClose(struct file *filep)
{
    return 0;                           // 关闭成功
}

/**
 * @brief 检查内存池是否已映射
 * @param[in] ipcInfo 进程IPC信息结构体指针
 * @return TRUE-已映射，FALSE-未映射
 */
LITE_OS_SEC_TEXT STATIC BOOL IsPoolMapped(ProcIpcInfo *ipcInfo)
{
    return (ipcInfo->pool.uvaddr != NULL) && (ipcInfo->pool.kvaddr != NULL) &&
        (ipcInfo->pool.poolSize != 0);  // 检查用户地址、内核地址和大小是否有效
}

/**
 * @brief 执行IPC内存映射
 * @details 将内核空间内存池映射到用户空间
 * @param[in] pcb 进程控制块指针
 * @param[in] region 内存映射区域结构体
 * @return 0-成功，非0-错误码
 */
LITE_OS_SEC_TEXT STATIC INT32 DoIpcMmap(LosProcessCB *pcb, LosVmMapRegion *region)
{
    UINT32 i;                           // 循环计数器
    INT32 ret = 0;                      // 返回值
    PADDR_T pa;                         // 物理地址
    UINT32 uflags = VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_USER;  // 用户空间读写权限
    LosVmPage *vmPage = NULL;           // 虚拟内存页指针
    VADDR_T uva = (VADDR_T)(UINTPTR)pcb->ipcInfo->pool.uvaddr;  // 用户虚拟地址
    VADDR_T kva = (VADDR_T)(UINTPTR)pcb->ipcInfo->pool.kvaddr;  // 内核虚拟地址

    (VOID)LOS_MuxAcquire(&pcb->vmSpace->regionMux);  // 获取虚拟内存区域互斥锁

    for (i = 0; i < (region->range.size >> PAGE_SHIFT); i++) {  // 按页循环映射
        pa = LOS_PaddrQuery((VOID *)(UINTPTR)(kva + (i << PAGE_SHIFT)));  // 查询内核地址对应的物理地址
        if (pa == 0) {
            PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);  // 物理地址查询失败
            ret = -EINVAL;             // 设置无效参数错误
            break;                     // 跳出循环
        }
        vmPage = LOS_VmPageGet(pa);     // 获取虚拟内存页
        if (vmPage == NULL) {
            PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);  // 虚拟内存页获取失败
            ret = -EINVAL;             // 设置无效参数错误
            break;                     // 跳出循环
        }
        STATUS_T err = LOS_ArchMmuMap(&pcb->vmSpace->archMmu, uva + (i << PAGE_SHIFT), pa, 1, uflags);  // 执行MMU映射
        if (err < 0) {
            ret = err;                  // 设置映射错误
            PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);  // 映射失败
            break;                     // 跳出循环
        }
    }
    /* 如果发生任何失败，执行回滚操作 */
    if (i != (region->range.size >> PAGE_SHIFT)) {
        while (i--) {                   // 回滚已映射的页面
            pa = LOS_PaddrQuery((VOID *)(UINTPTR)(kva + (i << PAGE_SHIFT)));  // 查询物理地址
            vmPage = LOS_VmPageGet(pa); // 获取虚拟内存页
            (VOID)LOS_ArchMmuUnmap(&pcb->vmSpace->archMmu, uva + (i << PAGE_SHIFT), 1);  // 解除映射
            LOS_PhysPageFree(vmPage);   // 释放物理页面
        }
    }

    (VOID)LOS_MuxRelease(&pcb->vmSpace->regionMux);  // 释放虚拟内存区域互斥锁
    return ret;                         // 返回映射结果
}

/**
 * @brief IPC内存映射实现
 * @details 将内核空间IPC内存池映射到用户空间，建立进程间通信通道
 * @param[in] filep 文件操作结构体指针
 * @param[in] region 内存映射区域结构体
 * @return 0-成功，非0-错误码（-EINVAL参数无效，-EEXIST已映射，-ENOMEM内存不足）
 */
LITE_OS_SEC_TEXT STATIC int LiteIpcMmap(struct file *filep, LosVmMapRegion *region)
{
    int ret = 0;                         // 返回值
    LosVmMapRegion *regionTemp = NULL;   // 临时内存区域指针
    LosProcessCB *pcb = OsCurrProcessGet();  // 当前进程控制块
    ProcIpcInfo *ipcInfo = pcb->ipcInfo;  // 当前进程IPC信息

    // 参数合法性检查：IPC信息、映射区域、区域大小、权限标志
    if ((ipcInfo == NULL) || (region == NULL) || (region->range.size > LITE_IPC_POOL_MAX_SIZE) ||
        (!LOS_IsRegionPermUserReadOnly(region)) || (!LOS_IsRegionFlagPrivateOnly(region))) {
        ret = -EINVAL;                   // 设置无效参数错误码
        goto ERROR_REGION_OUT;           // 跳转到错误处理
    }
    if (IsPoolMapped(ipcInfo)) {         // 检查内存池是否已映射
        return -EEXIST;                  // 返回已存在错误
    }
    if (ipcInfo->pool.uvaddr != NULL) {  // 如果用户空间地址已存在
        // 查找已有映射区域
        regionTemp = LOS_RegionFind(pcb->vmSpace, (VADDR_T)(UINTPTR)ipcInfo->pool.uvaddr);
        if ((regionTemp != NULL) && (regionTemp != region)) {
            (VOID)LOS_RegionFree(pcb->vmSpace, regionTemp);  // 释放冲突区域
        }
    }
    ipcInfo->pool.uvaddr = (VOID *)(UINTPTR)region->range.base;  // 设置用户空间基地址
    if (ipcInfo->pool.kvaddr != NULL) {  // 如果内核空间地址已存在
        LOS_VFree(ipcInfo->pool.kvaddr);  // 释放内核空间内存
        ipcInfo->pool.kvaddr = NULL;     // 重置内核空间地址
    }
    /* 使用vmalloc分配物理内存 */
    ipcInfo->pool.kvaddr = LOS_VMalloc(region->range.size);  // 分配内核空间内存
    if (ipcInfo->pool.kvaddr == NULL) {  // 检查分配结果
        ret = -ENOMEM;                   // 设置内存不足错误码
        goto ERROR_REGION_OUT;           // 跳转到错误处理
    }
    /* 执行内存映射 */
    ret = DoIpcMmap(pcb, region);        // 调用实际映射函数
    if (ret) {                           // 检查映射结果
        goto ERROR_MAP_OUT;              // 跳转到映射错误处理
    }
    /* 初始化IPC内存池 */
    if (LOS_MemInit(ipcInfo->pool.kvaddr, region->range.size) != LOS_OK) {  // 初始化内存池
        ret = -EINVAL;                   // 设置初始化失败错误码
        goto ERROR_MAP_OUT;              // 跳转到映射错误处理
    }
    ipcInfo->pool.poolSize = region->range.size;  // 设置内存池大小
    region->regionFlags |= VM_MAP_REGION_FLAG_LITEIPC;  // 设置LITEIPC区域标志
    return 0;                            // 返回成功
ERROR_MAP_OUT:                          // 映射错误标签
    LOS_VFree(ipcInfo->pool.kvaddr);     // 释放内核空间内存
ERROR_REGION_OUT:                       // 区域错误标签
    if (ipcInfo != NULL) {               // 避免空指针访问
        ipcInfo->pool.uvaddr = NULL;     // 重置用户空间地址
        ipcInfo->pool.kvaddr = NULL;     // 重置内核空间地址
    }
    return ret;                          // 返回错误码
}

/**
 * @brief IPC内存池初始化
 * @details 初始化IPC内存池结构体成员变量
 * @param[in] ipcInfo 进程IPC信息结构体指针
 * @return LOS_OK-成功
 */
LITE_OS_SEC_TEXT_INIT STATIC UINT32 LiteIpcPoolInit(ProcIpcInfo *ipcInfo)
{
    ipcInfo->pool.uvaddr = NULL;         // 初始化用户空间地址
    ipcInfo->pool.kvaddr = NULL;         // 初始化内核空间地址
    ipcInfo->pool.poolSize = 0;          // 初始化内存池大小
    ipcInfo->ipcTaskID = INVAILD_ID;     // 初始化IPC任务ID为无效值
    LOS_ListInit(&ipcInfo->ipcUsedNodelist);  // 初始化已使用节点链表
    return LOS_OK;                       // 返回成功
}

/**
 * @brief 创建IPC内存池
 * @details 分配并初始化进程IPC信息结构体
 * @return 成功返回ProcIpcInfo指针，失败返回NULL
 */
LITE_OS_SEC_TEXT_INIT STATIC ProcIpcInfo *LiteIpcPoolCreate(VOID)
{
    // 分配进程IPC信息结构体内存
    ProcIpcInfo *ipcInfo = LOS_MemAlloc(m_aucSysMem1, sizeof(ProcIpcInfo));
    if (ipcInfo == NULL) {               // 检查内存分配结果
        return NULL;                     // 分配失败返回NULL
    }

    (VOID)memset_s(ipcInfo, sizeof(ProcIpcInfo), 0, sizeof(ProcIpcInfo));  // 清零结构体

    (VOID)LiteIpcPoolInit(ipcInfo);      // 初始化IPC内存池
    return ipcInfo;                      // 返回创建的IPC信息
}

/**
 * @brief 重新初始化IPC内存池
 * @details 基于父进程IPC信息创建新的IPC内存池
 * @param[in] parent 父进程IPC信息结构体指针
 * @return 成功返回ProcIpcInfo指针，失败返回NULL
 */
LITE_OS_SEC_TEXT ProcIpcInfo *LiteIpcPoolReInit(const ProcIpcInfo *parent)
{
    ProcIpcInfo *ipcInfo = LiteIpcPoolCreate();  // 创建新的IPC内存池
    if (ipcInfo == NULL) {               // 检查创建结果
        return NULL;                     // 创建失败返回NULL
    }

    ipcInfo->pool.uvaddr = parent->pool.uvaddr;  // 继承父进程用户空间地址
    ipcInfo->pool.kvaddr = NULL;         // 重置内核空间地址
    ipcInfo->pool.poolSize = 0;          // 重置内存池大小
    ipcInfo->ipcTaskID = INVAILD_ID;     // 重置IPC任务ID
    return ipcInfo;                      // 返回重新初始化的IPC信息
}

/**
 * @brief 删除IPC内存池
 * @details 释放IPC内存池资源并清理访问权限
 * @param[in] ipcInfo 进程IPC信息结构体指针
 * @param[in] processID 进程ID
 */
STATIC VOID LiteIpcPoolDelete(ProcIpcInfo *ipcInfo, UINT32 processID)
{
    UINT32 intSave;                      // 中断状态保存变量
    IpcUsedNode *node = NULL;            // IPC已使用节点指针
    if (ipcInfo->pool.kvaddr != NULL) {  // 如果内核空间地址有效
        LOS_VFree(ipcInfo->pool.kvaddr);  // 释放内核空间内存
        ipcInfo->pool.kvaddr = NULL;     // 重置内核空间地址
        IPC_LOCK(intSave);               // 获取IPC自旋锁
        // 遍历并释放所有已使用节点
        while (!LOS_ListEmpty(&ipcInfo->ipcUsedNodelist)) {
            // 获取链表第一个节点
            node = LOS_DL_LIST_ENTRY(ipcInfo->ipcUsedNodelist.pstNext, IpcUsedNode, list);
            LOS_ListDelete(&node->list); // 从链表删除节点
            free(node);                  // 释放节点内存
        }
        IPC_UNLOCK(intSave);             // 释放IPC自旋锁
    }
    /* 移除进程对服务的访问权限 */
    for (UINT32 i = 0; i < MAX_SERVICE_NUM; i++) {  // 遍历所有服务
        if (ipcInfo->access[i] == TRUE) {  // 如果进程有访问权限
            ipcInfo->access[i] = FALSE;   // 清除访问权限
            // 如果服务任务存在且有IPC任务信息
            if (OS_TCB_FROM_TID(i)->ipcTaskInfo != NULL) {
                // 清除服务对该进程的访问映射
                OS_TCB_FROM_TID(i)->ipcTaskInfo->accessMap[processID] = FALSE;
            }
        }
    }
}

/**
 * @brief 销毁IPC内存池
 * @details 删除指定进程的IPC内存池并释放资源
 * @param[in] processID 进程ID
 * @return LOS_OK-成功，LOS_NOK-失败
 */
LITE_OS_SEC_TEXT UINT32 LiteIpcPoolDestroy(UINT32 processID)
{
    LosProcessCB *pcb = OS_PCB_FROM_PID(processID);  // 获取进程控制块
    if (pcb->ipcInfo == NULL) {          // 检查IPC信息是否存在
        return LOS_NOK;                  // 不存在返回失败
    }

    LiteIpcPoolDelete(pcb->ipcInfo, pcb->processID);  // 删除IPC内存池
    LOS_MemFree(m_aucSysMem1, pcb->ipcInfo);  // 释放IPC信息内存
    pcb->ipcInfo = NULL;                 // 重置IPC信息指针
    return LOS_OK;                       // 返回成功
}

/**
 * @brief 初始化IPC任务信息
 * @details 分配并初始化任务IPC信息结构体
 * @return 成功返回IpcTaskInfo指针，失败返回NULL
 */
LITE_OS_SEC_TEXT_INIT STATIC IpcTaskInfo *LiteIpcTaskInit(VOID)
{
    // 分配任务IPC信息结构体内存
    IpcTaskInfo *taskInfo = LOS_MemAlloc((VOID *)m_aucSysMem1, sizeof(IpcTaskInfo));
    if (taskInfo == NULL) {              // 检查内存分配结果
        return NULL;                     // 分配失败返回NULL
    }

    (VOID)memset_s(taskInfo, sizeof(IpcTaskInfo), 0, sizeof(IpcTaskInfo));  // 清零结构体
    LOS_ListInit(&taskInfo->msgListHead);  // 初始化消息链表头
    return taskInfo;                     // 返回任务IPC信息
}

/* 仅当内核不再访问IPC节点内容时，用户才能释放IPC节点 */
/**
 * @brief 允许用户释放IPC节点
 * @details 将IPC节点添加到已使用链表，标记为可被用户释放
 * @param[in] pcb 进程控制块指针
 * @param[in] buf 要释放的缓冲区指针
 */
LITE_OS_SEC_TEXT STATIC VOID EnableIpcNodeFreeByUser(LosProcessCB *pcb, VOID *buf)
{
    UINT32 intSave;                      // 中断状态保存变量
    ProcIpcInfo *ipcInfo = pcb->ipcInfo;  // 进程IPC信息
    // 分配IPC已使用节点内存
    IpcUsedNode *node = (IpcUsedNode *)malloc(sizeof(IpcUsedNode));
    if (node != NULL) {                  // 如果节点分配成功
        node->ptr = buf;                 // 设置节点缓冲区指针
        IPC_LOCK(intSave);               // 获取IPC自旋锁
        LOS_ListAdd(&ipcInfo->ipcUsedNodelist, &node->list);  // 添加节点到链表
        IPC_UNLOCK(intSave);             // 释放IPC自旋锁
    }
}

/**
 * @brief 分配IPC节点内存
 * @details 从进程IPC内存池分配指定大小的内存
 * @param[in] pcb 进程控制块指针
 * @param[in] size 分配大小（字节）
 * @return 成功返回内存指针，失败返回NULL
 */
LITE_OS_SEC_TEXT STATIC VOID *LiteIpcNodeAlloc(LosProcessCB *pcb, UINT32 size)
{
    // 从IPC内存池分配内存
    VOID *ptr = LOS_MemAlloc(pcb->ipcInfo->pool.kvaddr, size);
    PRINT_INFO("LiteIpcNodeAlloc pid:%d, pool:%x buf:%x size:%d\n",
               pcb->processID, pcb->ipcInfo->pool.kvaddr, ptr, size);  // 打印分配信息
    return ptr;                          // 返回分配的内存指针
}

/**
 * @brief 释放IPC节点内存
 * @details 释放进程IPC内存池中的指定内存
 * @param[in] pcb 进程控制块指针
 * @param[in] buf 要释放的内存指针
 * @return LOS_OK-成功，非0-错误码
 */
LITE_OS_SEC_TEXT STATIC UINT32 LiteIpcNodeFree(LosProcessCB *pcb, VOID *buf)
{
    PRINT_INFO("LiteIpcNodeFree pid:%d, pool:%x buf:%x\n",
               pcb->processID, pcb->ipcInfo->pool.kvaddr, buf);  // 打印释放信息
    return LOS_MemFree(pcb->ipcInfo->pool.kvaddr, buf);  // 释放内存并返回结果
}

/**
 * @brief 检查是否为有效IPC节点
 * @details 验证缓冲区是否为已分配的IPC节点并从链表移除
 * @param[in] pcb 进程控制块指针
 * @param[in] buf 要检查的缓冲区指针
 * @return TRUE-有效节点，FALSE-无效节点
 */
LITE_OS_SEC_TEXT STATIC BOOL IsIpcNode(LosProcessCB *pcb, const VOID *buf)
{
    IpcUsedNode *node = NULL;            // IPC已使用节点指针
    UINT32 intSave;                      // 中断状态保存变量
    ProcIpcInfo *ipcInfo = pcb->ipcInfo;  // 进程IPC信息
    IPC_LOCK(intSave);                   // 获取IPC自旋锁
    // 遍历已使用节点链表
    LOS_DL_LIST_FOR_EACH_ENTRY(node, &ipcInfo->ipcUsedNodelist, IpcUsedNode, list) {
        if (node->ptr == buf) {          // 如果找到匹配的缓冲区
            LOS_ListDelete(&node->list); // 从链表删除节点
            IPC_UNLOCK(intSave);         // 释放IPC自旋锁
            free(node);                  // 释放节点内存
            return TRUE;                 // 返回有效
        }
    }
    IPC_UNLOCK(intSave);                 // 释放IPC自旋锁
    return FALSE;                        // 返回无效
}

/**
 * @brief 转换内核地址到用户地址
 * @details 将内核空间地址转换为用户空间可见地址
 * @param[in] pcb 进程控制块指针
 * @param[in] kernelAddr 内核空间地址
 * @return 转换后的用户空间地址
 */
LITE_OS_SEC_TEXT STATIC INTPTR GetIpcUserAddr(const LosProcessCB *pcb, INTPTR kernelAddr)
{
    IpcPool pool = pcb->ipcInfo->pool;   // 获取IPC内存池信息
    INTPTR offset = (INTPTR)(pool.uvaddr) - (INTPTR)(pool.kvaddr);  // 计算地址偏移量
    return kernelAddr + offset;          // 返回用户空间地址
}

/**
 * @brief 转换用户地址到内核地址
 * @details 将用户空间地址转换为内核空间可见地址
 * @param[in] pcb 进程控制块指针
 * @param[in] userAddr 用户空间地址
 * @return 转换后的内核空间地址
 */
LITE_OS_SEC_TEXT STATIC INTPTR GetIpcKernelAddr(const LosProcessCB *pcb, INTPTR userAddr)
{
    IpcPool pool = pcb->ipcInfo->pool;   // 获取IPC内存池信息
    INTPTR offset = (INTPTR)(pool.uvaddr) - (INTPTR)(pool.kvaddr);  // 计算地址偏移量
    return userAddr - offset;            // 返回内核空间地址
}

/**
 * @brief 检查缓冲区是否为已使用IPC节点
 * @details 验证缓冲区地址有效性并转换为内核空间节点指针
 * @param[in] node 用户空间节点指针
 * @param[out] outPtr 输出内核空间节点指针
 * @return LOS_OK-成功，-EINVAL无效地址，-EFAULT非IPC节点
 */
LITE_OS_SEC_TEXT STATIC UINT32 CheckUsedBuffer(const VOID *node, IpcListNode **outPtr)
{
    VOID *ptr = NULL;                    // 临时指针
    LosProcessCB *pcb = OsCurrProcessGet();  // 当前进程控制块
    IpcPool pool = pcb->ipcInfo->pool;   // IPC内存池信息
    // 检查节点地址是否在IPC内存池范围内
    if ((node == NULL) || ((INTPTR)node < (INTPTR)(pool.uvaddr)) ||
        ((INTPTR)node > (INTPTR)(pool.uvaddr) + pool.poolSize)) {
        return -EINVAL;                  // 地址无效返回错误
    }
    ptr = (VOID *)GetIpcKernelAddr(pcb, (INTPTR)(node));  // 转换为内核地址
    if (IsIpcNode(pcb, ptr) != TRUE) {   // 检查是否为有效IPC节点
        return -EFAULT;                  // 非IPC节点返回错误
    }
    *outPtr = (IpcListNode *)ptr;        // 设置输出节点指针
    return LOS_OK;                       // 返回成功
}

/**
 * @brief 获取服务句柄对应的任务ID
 * @details 根据服务句柄查询并返回对应的任务ID
 * @param[in] serviceHandle 服务句柄
 * @param[out] taskID 输出任务ID
 * @return LOS_OK-成功，-EINVAL无效句柄或未注册
 */
LITE_OS_SEC_TEXT STATIC UINT32 GetTid(UINT32 serviceHandle, UINT32 *taskID)
{
    if (serviceHandle >= MAX_SERVICE_NUM) {  // 检查服务句柄范围
        return -EINVAL;                  // 句柄超出范围返回错误
    }
    (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);  // 获取互斥锁
#if (USE_TASKID_AS_HANDLE == 1)         // 如果使用任务ID作为句柄
    *taskID = serviceHandle ? serviceHandle : g_cmsTask.taskID;  // 直接使用服务句柄或CMS任务ID
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);  // 释放互斥锁
    return LOS_OK;                       // 返回成功
#else                                   // 否则使用句柄映射表
    // 检查服务句柄是否已注册
    if (g_serviceHandleMap[serviceHandle].status == HANDLE_REGISTED) {
        *taskID = g_serviceHandleMap[serviceHandle].taskID;  // 获取映射的任务ID
        (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);  // 释放互斥锁
        return LOS_OK;                   // 返回成功
    }
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);  // 释放互斥锁
    return -EINVAL;                      // 服务未注册返回错误
#endif
}
/**
 * @brief 生成服务句柄
 * @param taskID 任务ID，如果为0则使用当前任务ID
 * @param status 服务句柄状态
 * @param serviceHandle 输出参数，生成的服务句柄
 * @return 成功返回LOS_OK，失败返回-EINVAL
 * @note 该函数使用互斥锁保证线程安全
 */
LITE_OS_SEC_TEXT STATIC UINT32 GenerateServiceHandle(UINT32 taskID, HandleStatus status, UINT32 *serviceHandle)
{
    (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);  // 获取服务句柄映射互斥锁
#if (USE_TASKID_AS_HANDLE == 1)
    *serviceHandle = taskID ? taskID : LOS_CurTaskIDGet(); /* 如果taskID为0，返回当前任务ID */
    if (*serviceHandle != g_cmsTask.taskID) {  // 检查服务句柄是否与CMS任务ID冲突
        (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);  // 释放互斥锁
        return LOS_OK;
    }
#else
    for (UINT32 i = 1; i < MAX_SERVICE_NUM; i++) {  // 遍历服务句柄映射表查找可用项
        if (g_serviceHandleMap[i].status == HANDLE_NOT_USED) {  // 找到未使用的服务句柄
            g_serviceHandleMap[i].taskID = taskID;  // 设置任务ID
            g_serviceHandleMap[i].status = status;  // 设置状态
            *serviceHandle = i;  // 输出服务句柄
            (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);  // 释放互斥锁
            return LOS_OK;
        }
    }
#endif
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);  // 释放互斥锁
    return -EINVAL;  // 没有可用的服务句柄
}

/**
 * @brief 刷新服务句柄状态
 * @param serviceHandle 服务句柄
 * @param result 操作结果，LOS_OK表示成功
 * @return 无
 * @note 仅当USE_TASKID_AS_HANDLE为0时有效
 */
LITE_OS_SEC_TEXT STATIC VOID RefreshServiceHandle(UINT32 serviceHandle, UINT32 result)
{
#if (USE_TASKID_AS_HANDLE == 0)
    (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);  // 获取服务句柄映射互斥锁
    if ((result == LOS_OK) && (g_serviceHandleMap[serviceHandle].status == HANDLE_REGISTING)) {  // 注册成功且当前状态为注册中
        g_serviceHandleMap[serviceHandle].status = HANDLE_REGISTED;  // 更新状态为已注册
    } else {
        g_serviceHandleMap[serviceHandle].status = HANDLE_NOT_USED;  // 注册失败，标记为未使用
    }
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);  // 释放互斥锁
#endif
}

/**
 * @brief 添加服务访问权限
 * @param taskID 请求访问的任务ID
 * @param serviceHandle 服务句柄
 * @return 成功返回LOS_OK，失败返回错误码
 * @note 该函数会更新服务提供者和请求者的访问映射表
 */
LITE_OS_SEC_TEXT STATIC UINT32 AddServiceAccess(UINT32 taskID, UINT32 serviceHandle)
{
    UINT32 serviceTid = 0;
    UINT32 ret = GetTid(serviceHandle, &serviceTid);  // 获取服务对应的任务ID
    if (ret != LOS_OK) {
        PRINT_ERR("Liteipc AddServiceAccess GetTid failed\n");
        return ret;
    }

    LosTaskCB *tcb = OS_TCB_FROM_TID(serviceTid);  // 获取服务任务控制块
    LosProcessCB *pcb = OS_PCB_FROM_TID(taskID);  // 获取请求任务的进程控制块
    if ((tcb->ipcTaskInfo == NULL) || (pcb->ipcInfo == NULL)) {  // 检查IPC信息是否初始化
        PRINT_ERR("Liteipc AddServiceAccess ipc not create! pid %u tid %u\n", pcb->processID, tcb->taskID);
        return -EINVAL;
    }
    tcb->ipcTaskInfo->accessMap[pcb->processID] = TRUE;  // 允许该进程访问服务
    pcb->ipcInfo->access[serviceTid] = TRUE;  // 记录该服务可被当前进程访问
    return LOS_OK;
}

/**
 * @brief 检查是否有服务访问权限
 * @param serviceHandle 服务句柄
 * @return 有权限返回TRUE，否则返回FALSE
 * @note 同一进程内的任务默认具有访问权限
 */
LITE_OS_SEC_TEXT STATIC BOOL HasServiceAccess(UINT32 serviceHandle)
{
    UINT32 serviceTid = 0;
    LosProcessCB *curr = OsCurrProcessGet();  // 获取当前进程控制块
    UINT32 ret;
    if (serviceHandle >= MAX_SERVICE_NUM) {  // 服务句柄越界检查
        return FALSE;
    }
    if (serviceHandle == 0) {  // 句柄0为特殊值，允许访问
        return TRUE;
    }
    ret = GetTid(serviceHandle, &serviceTid);  // 获取服务对应的任务ID
    if (ret != LOS_OK) {
        PRINT_ERR("Liteipc HasServiceAccess GetTid failed\n");
        return FALSE;
    }
    LosTaskCB *taskCB = OS_TCB_FROM_TID(serviceTid);  // 获取服务任务控制块
    if (taskCB->processCB == (UINTPTR)curr) {  // 同一进程内的任务直接允许访问
        return TRUE;
    }

    if (taskCB->ipcTaskInfo == NULL) {  // 服务任务未初始化IPC信息
        return FALSE;
    }

    return taskCB->ipcTaskInfo->accessMap[curr->processID];  // 检查访问映射表
}

/**
 * @brief 设置当前进程的IPC任务
 * @param 无
 * @return 成功返回IPC任务ID，失败返回-EINVAL
 * @note 每个进程只能设置一个IPC任务
 */
LITE_OS_SEC_TEXT STATIC UINT32 SetIpcTask(VOID)
{
    if (OsCurrProcessGet()->ipcInfo->ipcTaskID == INVAILD_ID) {  // 检查IPC任务是否未设置
        OsCurrProcessGet()->ipcInfo->ipcTaskID = LOS_CurTaskIDGet();  // 设置当前任务为IPC任务
        return OsCurrProcessGet()->ipcInfo->ipcTaskID;
    }
    PRINT_ERR("Liteipc curprocess %d IpcTask already set!\n", OsCurrProcessGet()->processID);  // 已设置IPC任务，打印错误
    return -EINVAL;
}

/**
 * @brief 检查当前进程是否已设置IPC任务
 * @param 无
 * @return 已设置返回TRUE，否则返回FALSE
 */
LITE_OS_SEC_TEXT BOOL IsIpcTaskSet(VOID)
{
    if (OsCurrProcessGet()->ipcInfo->ipcTaskID == INVAILD_ID) {  // 检查IPC任务ID是否有效
        return FALSE;
    }
    return TRUE;
}

/**
 * @brief 获取进程的IPC任务ID
 * @param pcb 进程控制块指针
 * @param ipcTaskID 输出参数，IPC任务ID
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
LITE_OS_SEC_TEXT STATIC UINT32 GetIpcTaskID(LosProcessCB *pcb, UINT32 *ipcTaskID)
{
    if (pcb->ipcInfo->ipcTaskID == INVAILD_ID) {  // IPC任务未设置
        return LOS_NOK;
    }
    *ipcTaskID = pcb->ipcInfo->ipcTaskID;  // 输出IPC任务ID
    return LOS_OK;
}

/**
 * @brief 发送服务死亡通知消息
 * @param processID 接收通知的进程ID
 * @param serviceHandle 死亡的服务句柄
 * @return 成功返回LOS_OK，失败返回错误码
 * @note 通知消息类型为MT_DEATH_NOTIFY
 */
LITE_OS_SEC_TEXT STATIC UINT32 SendDeathMsg(UINT32 processID, UINT32 serviceHandle)
{
    UINT32 ipcTaskID;
    UINT32 ret;
    IpcContent content;
    IpcMsg msg;
    LosProcessCB *pcb = OS_PCB_FROM_PID(processID);  // 获取接收进程控制块

    if (pcb->ipcInfo == NULL) {  // 进程IPC信息未初始化
        return -EINVAL;
    }

    pcb->ipcInfo->access[serviceHandle] = FALSE;  // 清除该服务的访问权限

    ret = GetIpcTaskID(pcb, &ipcTaskID);  // 获取接收进程的IPC任务ID
    if (ret != LOS_OK) {
        return -EINVAL;
    }
    content.flag = SEND;  // 设置为发送模式
    content.outMsg = &msg;  // 绑定输出消息结构
    (void)memset_s(content.outMsg, sizeof(IpcMsg), 0, sizeof(IpcMsg));  // 初始化消息结构
    content.outMsg->type = MT_DEATH_NOTIFY;  // 设置消息类型为死亡通知
    content.outMsg->target.handle = ipcTaskID;  // 设置目标IPC任务句柄
    content.outMsg->target.token = serviceHandle;  // 设置服务句柄
    content.outMsg->code = 0;  // 消息代码设为0
    return LiteIpcWrite(&content);  // 发送消息
}

/**
 * @brief 移除服务句柄并清理相关资源
 * @param taskID 服务对应的任务ID
 * @return 无
 * @note 该函数会释放IPC任务信息并通知相关进程
 */
LITE_OS_SEC_TEXT VOID LiteIpcRemoveServiceHandle(UINT32 taskID)
{
    UINT32 j;
    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);  // 获取任务控制块
    IpcTaskInfo *ipcTaskInfo = taskCB->ipcTaskInfo;
    if (ipcTaskInfo == NULL) {  // IPC任务信息未初始化，直接返回
        return;
    }

#if (USE_TASKID_AS_HANDLE == 1)

    UINT32 intSave;
    LOS_DL_LIST *listHead = NULL;
    LOS_DL_LIST *listNode = NULL;
    IpcListNode *node = NULL;
    LosProcessCB *pcb = OS_PCB_FROM_TCB(taskCB);  // 获取进程控制块

    listHead = &(ipcTaskInfo->msgListHead);  // 获取消息链表头
    do {
        SCHEDULER_LOCK(intSave);  // 关调度器
        if (LOS_ListEmpty(listHead)) {  // 链表为空
            SCHEDULER_UNLOCK(intSave);  // 开调度器
            break;
        } else {
            listNode = LOS_DL_LIST_FIRST(listHead);  // 获取第一个节点
            LOS_ListDelete(listNode);  // 从链表中删除节点
            node = LOS_DL_LIST_ENTRY(listNode, IpcListNode, listNode);  // 获取节点数据
            SCHEDULER_UNLOCK(intSave);  // 开调度器
            (VOID)HandleSpecialObjects(taskCB->taskID, node, TRUE);  // 处理特殊对象
            (VOID)LiteIpcNodeFree(pcb, (VOID *)node);  // 释放节点内存
        }
    } while (1);  // 循环处理所有节点

    ipcTaskInfo->accessMap[pcb->processID] = FALSE;  // 清除自身进程访问权限
    for (j = 0; j < LOSCFG_BASE_CORE_PROCESS_LIMIT; j++) {  // 遍历所有进程
        if (ipcTaskInfo->accessMap[j] == TRUE) {  // 找到有访问权限的进程
            ipcTaskInfo->accessMap[j] = FALSE;  // 清除访问权限
            (VOID)SendDeathMsg(j, taskCB->taskID);  // 发送死亡通知
        }
    }
#else
    (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);  // 获取服务句柄映射互斥锁
    for (UINT32 i = 1; i < MAX_SERVICE_NUM; i++) {  // 遍历服务句柄映射表
        if ((g_serviceHandleMap[i].status != HANDLE_NOT_USED) && (g_serviceHandleMap[i].taskID == taskCB->taskID)) {  // 找到匹配的服务句柄
            g_serviceHandleMap[i].status = HANDLE_NOT_USED;  // 标记为未使用
            g_serviceHandleMap[i].taskID = INVAILD_ID;  // 清除任务ID
            break;
        }
    }
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);  // 释放互斥锁
    /* 执行死亡处理 */
    if (i < MAX_SERVICE_NUM) {  // 找到服务句柄
        for (j = 0; j < LOSCFG_BASE_CORE_PROCESS_LIMIT; j++) {  // 遍历所有进程
            if (ipcTaskInfo->accessMap[j] == TRUE) {  // 找到有访问权限的进程
                (VOID)SendDeathMsg(j, i);  // 发送死亡通知
            }
        }
    }
#endif

    (VOID)LOS_MemFree(m_aucSysMem1, ipcTaskInfo);  // 释放IPC任务信息内存
    taskCB->ipcTaskInfo = NULL;  // 清除IPC任务信息指针
}

/**
 * @brief 设置CMS（核心消息服务）任务
 * @param maxMsgSize 最大消息大小
 * @return 成功返回LOS_OK，失败返回-EINVAL或-EEXIST
 * @note CMS任务只能设置一次
 */
LITE_OS_SEC_TEXT STATIC UINT32 SetCms(UINTPTR maxMsgSize)
{
    if (maxMsgSize < sizeof(IpcMsg)) {  // 检查消息大小是否小于最小要求
        return -EINVAL;
    }
    (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);  // 获取服务句柄映射互斥锁
#if (USE_TASKID_AS_HANDLE == 1)
    if (g_cmsTask.status == HANDLE_NOT_USED) {  // CMS任务未设置
        g_cmsTask.status = HANDLE_REGISTED;  // 设置状态为已注册
        g_cmsTask.taskID = LOS_CurTaskIDGet();  // 设置当前任务为CMS任务
        g_cmsTask.maxMsgSize = maxMsgSize;  // 设置最大消息大小
        (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);  // 释放互斥锁
        return LOS_OK;
    }
#else
    if (g_serviceHandleMap[0].status == HANDLE_NOT_USED) {  // 服务句柄0未使用
        g_serviceHandleMap[0].status = HANDLE_REGISTED;  // 设置状态为已注册
        g_serviceHandleMap[0].taskID = LOS_CurTaskIDGet();  // 设置当前任务为CMS任务
        (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);  // 释放互斥锁
        return LOS_OK;
    }
#endif
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);  // 释放互斥锁
    return -EEXIST;  // CMS任务已存在
}

/**
 * @brief 检查CMS任务是否已设置
 * @param 无
 * @return 已设置返回TRUE，否则返回FALSE
 */
LITE_OS_SEC_TEXT STATIC BOOL IsCmsSet(VOID)
{
    BOOL ret;
    (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);  // 获取服务句柄映射互斥锁
#if (USE_TASKID_AS_HANDLE == 1)
    ret = g_cmsTask.status == HANDLE_REGISTED;  // 检查CMS任务状态
#else
    ret = g_serviceHandleMap[0].status == HANDLE_REGISTED;  // 检查服务句柄0状态
#endif
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);  // 释放互斥锁
    return ret;
}

/**
 * @brief 检查任务是否为CMS任务
 * @param taskID 任务ID
 * @return 是CMS任务返回TRUE，否则返回FALSE
 * @note CMS任务所在进程的所有任务都被视为CMS任务
 */
LITE_OS_SEC_TEXT STATIC BOOL IsCmsTask(UINT32 taskID)
{
    BOOL ret;
    (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);  // 获取服务句柄映射互斥锁
#if (USE_TASKID_AS_HANDLE == 1)
    ret = IsCmsSet() ? (OS_TCB_FROM_TID(taskID)->processCB == OS_TCB_FROM_TID(g_cmsTask.taskID)->processCB) : FALSE;  // 检查是否与CMS任务同进程
#else
    ret = IsCmsSet() ? (OS_TCB_FROM_TID(taskID)->processCB ==
        OS_TCB_FROM_TID(g_serviceHandleMap[0].taskID)->processCB) : FALSE;  // 检查是否与CMS任务同进程
#endif
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);  // 释放互斥锁
    return ret;
}

/**
 * @brief 检查任务是否存活
 * @param taskID 任务ID
 * @return 存活返回TRUE，否则返回FALSE
 * @note 存活条件：任务ID有效、未被标记为未使用、处于活动状态且为用户模式
 */
LITE_OS_SEC_TEXT STATIC BOOL IsTaskAlive(UINT32 taskID)
{
    LosTaskCB *tcb = NULL;
    if (OS_TID_CHECK_INVALID(taskID)) {  // 任务ID无效
        return FALSE;
    }
    tcb = OS_TCB_FROM_TID(taskID);  // 获取任务控制块
    if (OsTaskIsUnused(tcb)) {  // 任务已被标记为未使用
        return FALSE;
    }
    if (OsTaskIsInactive(tcb)) {  // 任务处于非活动状态
        return FALSE;
    }
    if (!OsTaskIsUserMode(tcb)) {  // 不是用户模式任务
        return FALSE;
    }
    return TRUE;
}

/**
 * @brief 处理文件描述符特殊对象
 * @param pcb 进程控制块指针
 * @param obj 特殊对象指针
 * @param isRollback 是否为回滚操作
 * @return 成功返回LOS_OK，失败返回错误码
 * @note 非回滚时复制文件描述符，回滚时关闭文件描述符
 */
LITE_OS_SEC_TEXT STATIC UINT32 HandleFd(const LosProcessCB *pcb, SpecialObj *obj, BOOL isRollback)
{
    int ret;
    if (isRollback == FALSE) {  // 非回滚操作：复制文件描述符到目标进程
        ret = CopyFdToProc(obj->content.fd, pcb->processID);  // 复制文件描述符
        if (ret < 0) {
            return ret;
        }
        obj->content.fd = ret;  // 更新为新的文件描述符
    } else {  // 回滚操作：关闭文件描述符
        ret = CloseProcFd(obj->content.fd, pcb->processID);  // 关闭文件描述符
        if (ret < 0) {
            return ret;
        }
    }

    return LOS_OK;
}

/**
 * @brief 处理指针特殊对象
 * @param pcb 进程控制块指针
 * @param obj 特殊对象指针
 * @param isRollback 是否为回滚操作
 * @return 成功返回LOS_OK，失败返回错误码
 * @note 非回滚时复制用户空间数据到IPC节点，回滚时释放IPC节点
 */
LITE_OS_SEC_TEXT STATIC UINT32 HandlePtr(LosProcessCB *pcb, SpecialObj *obj, BOOL isRollback)
{
    VOID *buf = NULL;
    UINT32 ret;
    if ((obj->content.ptr.buff == NULL) || (obj->content.ptr.buffSz == 0)) {  // 指针或大小无效
        return -EINVAL;
    }
    if (isRollback == FALSE) {  // 非回滚操作：复制用户数据
        if (LOS_IsUserAddress((vaddr_t)(UINTPTR)(obj->content.ptr.buff)) == FALSE) {  // 检查是否为用户空间地址
            PRINT_ERR("Liteipc Bad ptr address\n");
            return -EINVAL;
        }
        buf = LiteIpcNodeAlloc(pcb, obj->content.ptr.buffSz);  // 分配IPC节点内存
        if (buf == NULL) {
            PRINT_ERR("Liteipc DealPtr alloc mem failed\n");
            return -EINVAL;
        }
        ret = copy_from_user(buf, obj->content.ptr.buff, obj->content.ptr.buffSz);  // 从用户空间复制数据
        if (ret != LOS_OK) {
            LiteIpcNodeFree(pcb, buf);  // 复制失败，释放内存
            return ret;
        }
        obj->content.ptr.buff = (VOID *)GetIpcUserAddr(pcb, (INTPTR)buf);  // 转换为用户空间地址
        EnableIpcNodeFreeByUser(pcb, (VOID *)buf);  // 允许用户空间释放该节点
    } else {  // 回滚操作：释放IPC节点
        buf = (VOID *)GetIpcKernelAddr(pcb, (INTPTR)obj->content.ptr.buff);  // 转换为内核空间地址
        if (IsIpcNode(pcb, buf) == TRUE) {  // 检查是否为有效的IPC节点
            (VOID)LiteIpcNodeFree(pcb, buf);  // 释放节点内存
        }
    }
    return LOS_OK;
}
/**
 * @brief 处理服务类型特殊对象
 * @param dstTid 目标任务ID
 * @param obj 特殊对象指针
 * @param isRollback 是否为回滚操作
 * @return 成功返回LOS_OK，失败返回错误码
 * @note 负责服务句柄的生成、访问权限控制和有效性检查
 */
LITE_OS_SEC_TEXT STATIC UINT32 HandleSvc(UINT32 dstTid, SpecialObj *obj, BOOL isRollback)
{
    UINT32 taskID = 0;
    if (isRollback == FALSE) {  // 非回滚操作：处理服务注册和访问
        if (obj->content.svc.handle == -1) {  // 需要生成新的服务句柄
            if (obj->content.svc.token != 1) {  // 验证服务令牌
                PRINT_ERR("Liteipc HandleSvc wrong svc token\n");
                return -EINVAL;
            }
            UINT32 selfTid = LOS_CurTaskIDGet();  // 获取当前任务ID
            LosTaskCB *tcb = OS_TCB_FROM_TID(selfTid);  // 获取当前任务控制块
            if (tcb->ipcTaskInfo == NULL) {  // 初始化IPC任务信息（如未初始化）
                tcb->ipcTaskInfo = LiteIpcTaskInit();
            }
            uint32_t serviceHandle = 0;
            UINT32 ret = GenerateServiceHandle(selfTid, HANDLE_REGISTED, &serviceHandle);  // 生成服务句柄
            if (ret != LOS_OK) {
                PRINT_ERR("Liteipc GenerateServiceHandle failed.\n");
                return ret;
            }
            obj->content.svc.handle = serviceHandle;  // 更新服务句柄
            (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);  // 获取服务句柄映射互斥锁
            AddServiceAccess(dstTid, serviceHandle);  // 添加目标任务对服务的访问权限
            (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);  // 释放互斥锁
        }
        if (IsTaskAlive(obj->content.svc.handle) == FALSE) {  // 检查服务对应的任务是否存活
            PRINT_ERR("Liteipc HandleSvc wrong svctid\n");
            return -EINVAL;
        }
        if (HasServiceAccess(obj->content.svc.handle) == FALSE) {  // 检查是否有服务访问权限
            PRINT_ERR("Liteipc %s, %d, svchandle:%d, tid:%d\n", __FUNCTION__, __LINE__, obj->content.svc.handle, LOS_CurTaskIDGet());
            return -EACCES;
        }
        LosTaskCB *taskCb = OS_TCB_FROM_TID(obj->content.svc.handle);  // 获取服务任务控制块
        if (taskCb->ipcTaskInfo == NULL) {  // 初始化服务任务的IPC信息（如未初始化）
            taskCb->ipcTaskInfo = LiteIpcTaskInit();
        }
        if (GetTid(obj->content.svc.handle, &taskID) == 0) {  // 获取服务对应的任务ID
            AddServiceAccess(dstTid, obj->content.svc.handle);  // 添加访问权限
        }
    }
    return LOS_OK;
}

/**
 * @brief 根据对象类型分发处理特殊对象
 * @param dstTid 目标任务ID
 * @param obj 特殊对象指针
 * @param isRollback 是否为回滚操作
 * @return 成功返回LOS_OK，失败返回错误码
 * @note 支持FD、PTR和SVC三种类型的特殊对象处理
 */
LITE_OS_SEC_TEXT STATIC UINT32 HandleObj(UINT32 dstTid, SpecialObj *obj, BOOL isRollback)
{
    UINT32 ret;
    LosProcessCB *pcb = OS_PCB_FROM_TID(dstTid);  // 获取目标任务的进程控制块
    switch (obj->type) {  // 根据对象类型选择处理函数
        case OBJ_FD:  // 文件描述符类型
            ret = HandleFd(pcb, obj, isRollback);
            break;
        case OBJ_PTR:  // 指针类型
            ret = HandlePtr(pcb, obj, isRollback);
            break;
        case OBJ_SVC:  // 服务类型
            ret = HandleSvc(dstTid, (SpecialObj *)obj, isRollback);
            break;
        default:  // 未知类型
            ret = -EINVAL;
            break;
    }
    return ret;
}

/**
 * @brief 批量处理消息中的所有特殊对象
 * @param dstTid 目标任务ID
 * @param node IPC消息节点指针
 * @param isRollback 是否为回滚操作
 * @return 成功返回LOS_OK，失败返回错误码
 * @note 处理失败时会对已处理的对象执行回滚操作
 */
LITE_OS_SEC_TEXT STATIC UINT32 HandleSpecialObjects(UINT32 dstTid, IpcListNode *node, BOOL isRollback)
{
    UINT32 ret = LOS_OK;
    IpcMsg *msg = &(node->msg);  // 获取消息结构
    INT32 i;
    SpecialObj *obj = NULL;
    UINT32 *offset = (UINT32 *)(UINTPTR)(msg->offsets);  // 获取特殊对象偏移数组
    if (isRollback) {  // 回滚操作：从最后一个对象开始回滚
        i = msg->spObjNum;
        goto EXIT;
    }
    for (i = 0; i < msg->spObjNum; i++) {  // 遍历所有特殊对象
        if (offset[i] > msg->dataSz - sizeof(SpecialObj)) {  // 检查偏移是否超出数据区
            ret = -EINVAL;
            goto EXIT;
        }
        if ((i > 0) && (offset[i] < offset[i - 1] + sizeof(SpecialObj))) {  // 检查对象是否重叠
            ret = -EINVAL;
            goto EXIT;
        }
        obj = (SpecialObj *)((UINTPTR)msg->data + offset[i]);  // 获取特殊对象
        if (obj == NULL) {  // 检查对象有效性
            ret = -EINVAL;
            goto EXIT;
        }
        ret = HandleObj(dstTid, obj, FALSE);  // 处理特殊对象
        if (ret != LOS_OK) {
            goto EXIT;
        }
    }
    return LOS_OK;
EXIT:  // 错误处理：回滚已处理的对象
    for (i--; i >= 0; i--) {  // 逆序回滚
        obj = (SpecialObj *)((UINTPTR)msg->data + offset[i]);
        (VOID)HandleObj(dstTid, obj, TRUE);  // 执行回滚操作
    }
    return ret;
}

/**
 * @brief 检查CMS消息的总大小是否超出限制
 * @param msg IPC消息指针
 * @return 成功返回LOS_OK，超出限制返回-EINVAL
 * @note 仅对发送给CMS的消息进行大小检查
 */
LITE_OS_SEC_TEXT STATIC UINT32 CheckMsgSize(IpcMsg *msg)
{
    UINT64 totalSize;
    UINT32 i;
    UINT32 *offset = (UINT32 *)(UINTPTR)(msg->offsets);  // 获取特殊对象偏移数组
    SpecialObj *obj = NULL;
    if (msg->target.handle != 0) {  // 非CMS消息不需要检查大小
        return LOS_OK;
    }
    /* 发送给CMS的消息需要检查大小 */
    totalSize = (UINT64)sizeof(IpcMsg) + msg->dataSz + msg->spObjNum * sizeof(UINT32);  // 基础大小
    for (i = 0; i < msg->spObjNum; i++) {  // 累加特殊对象大小
        if (offset[i] > msg->dataSz - sizeof(SpecialObj)) {  // 检查偏移有效性
            return -EINVAL;
        }
        if ((i > 0) && (offset[i] < offset[i - 1] + sizeof(SpecialObj))) {  // 检查对象是否重叠
            return -EINVAL;
        }
        obj = (SpecialObj *)((UINTPTR)msg->data + offset[i]);  // 获取特殊对象
        if (obj == NULL) {  // 检查对象有效性
            return -EINVAL;
        }
        if (obj->type == OBJ_PTR) {  // 指针类型对象需要累加缓冲区大小
            totalSize += obj->content.ptr.buffSz;
        }
    }
    (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);  // 获取互斥锁
    if (totalSize > g_cmsTask.maxMsgSize) {  // 检查总大小是否超出CMS最大消息限制
        (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
        return -EINVAL;
    }
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);  // 释放互斥锁
    return LOS_OK;
}

/**
 * @brief 将用户空间的消息数据复制到内核空间
 * @param node IPC消息节点指针
 * @param bufSz 缓冲区大小
 * @param msg 用户空间的消息指针
 * @return 成功返回LOS_OK，失败返回错误码
 * @note 包含消息头、数据区和特殊对象偏移数组的复制
 */
LITE_OS_SEC_TEXT STATIC UINT32 CopyDataFromUser(IpcListNode *node, UINT32 bufSz, const IpcMsg *msg)
{
    UINT32 ret;
    ret = (UINT32)memcpy_s((VOID *)(&node->msg), bufSz - sizeof(LOS_DL_LIST), (const VOID *)msg, sizeof(IpcMsg));  // 复制消息头
    if (ret != LOS_OK) {
        PRINT_DEBUG("%s, %d, %u\n", __FUNCTION__, __LINE__, ret);
        return ret;
    }

    if (msg->dataSz) {  // 复制数据区（如果有）
        node->msg.data = (VOID *)((UINTPTR)node + sizeof(IpcListNode));  // 设置数据区指针
        ret = copy_from_user((VOID *)(node->msg.data), msg->data, msg->dataSz);  // 从用户空间复制数据
        if (ret != LOS_OK) {
            PRINT_DEBUG("%s, %d\n", __FUNCTION__, __LINE__);
            return ret;
        }
    } else {
        node->msg.data = NULL;  // 无数据区
    }

    if (msg->spObjNum) {  // 复制特殊对象偏移数组（如果有）
        node->msg.offsets = (VOID *)((UINTPTR)node + sizeof(IpcListNode) + msg->dataSz);  // 设置偏移数组指针
        ret = copy_from_user((VOID *)(node->msg.offsets), msg->offsets, msg->spObjNum * sizeof(UINT32));  // 复制偏移数组
        if (ret != LOS_OK) {
            PRINT_DEBUG("%s, %d, %x, %x, %d\n", __FUNCTION__, __LINE__, node->msg.offsets, msg->offsets, msg->spObjNum);
            return ret;
        }
    } else {
        node->msg.offsets = NULL;  // 无特殊对象
    }
    ret = CheckMsgSize(&node->msg);  // 检查消息大小
    if (ret != LOS_OK) {
        PRINT_DEBUG("%s, %d\n", __FUNCTION__, __LINE__);
        return ret;
    }
    node->msg.taskID = LOS_CurTaskIDGet();  // 设置发送任务ID
    node->msg.processID = OsCurrProcessGet()->processID;  // 设置发送进程ID
#ifdef LOSCFG_SECURITY_CAPABILITY
    node->msg.userID = OsCurrProcessGet()->user->userID;  // 设置用户ID（使能安全能力时）
    node->msg.gid = OsCurrProcessGet()->user->gid;  // 设置组ID（使能安全能力时）
#endif
    return LOS_OK;
}

/**
 * @brief 验证回复消息是否有效
 * @param content IPC内容结构体指针
 * @return 有效返回TRUE，否则返回FALSE
 * @note 检查回复是否与请求匹配，包括时间戳、目标句柄等
 */
LITE_OS_SEC_TEXT STATIC BOOL IsValidReply(const IpcContent *content)
{
    LosProcessCB *curr = OsCurrProcessGet();  // 获取当前进程控制块
    IpcListNode *node = (IpcListNode *)GetIpcKernelAddr(curr, (INTPTR)(content->buffToFree));  // 获取请求消息节点
    IpcMsg *requestMsg = &node->msg;  // 获取请求消息
    IpcMsg *replyMsg = content->outMsg;  // 获取回复消息
    UINT32 reqDstTid = 0;
    /* 检查回复是否与请求匹配 */
    if ((requestMsg->type != MT_REQUEST)  ||  // 请求类型检查
        (requestMsg->flag == LITEIPC_FLAG_ONEWAY) ||  // 排除单向请求
        (replyMsg->timestamp != requestMsg->timestamp) ||  // 时间戳匹配检查
        (replyMsg->target.handle != requestMsg->taskID) ||  // 目标句柄检查
        (GetTid(requestMsg->target.handle, &reqDstTid) != 0) ||  // 获取请求目标任务ID
        (OS_TCB_FROM_TID(reqDstTid)->processCB != (UINTPTR)curr)) {  // 进程匹配检查
        return FALSE;
    }
    return TRUE;
}

/**
 * @brief 检查IPC消息参数的有效性
 * @param content IPC内容结构体指针
 * @param dstTid 输出参数，目标任务ID
 * @return 成功返回LOS_OK，失败返回错误码
 * @note 根据消息类型执行不同的验证逻辑
 */
LITE_OS_SEC_TEXT STATIC UINT32 CheckPara(IpcContent *content, UINT32 *dstTid)
{
    UINT32 ret;
    IpcMsg *msg = content->outMsg;  // 获取消息结构
    UINT32 flag = content->flag;  // 获取操作标志
#if (USE_TIMESTAMP == 1)
    UINT64 now = LOS_CurrNanosec();  // 获取当前时间戳（使能时间戳时）
#endif
    // 基本参数检查
    if (((msg->dataSz > 0) && (msg->data == NULL)) ||  // 数据区非空但指针为空
        ((msg->spObjNum > 0) && (msg->offsets == NULL)) ||  // 特殊对象非空但偏移数组为空
        (msg->dataSz > IPC_MSG_DATA_SZ_MAX) ||  // 数据区大小超限
        (msg->spObjNum > IPC_MSG_OBJECT_NUM_MAX) ||  // 特殊对象数量超限
        (msg->dataSz < msg->spObjNum * sizeof(SpecialObj))) {  // 数据区不足以容纳所有特殊对象
        return -EINVAL;
    }
    switch (msg->type) {  // 根据消息类型执行特定检查
        case MT_REQUEST:  // 请求消息
            if (HasServiceAccess(msg->target.handle)) {  // 检查是否有服务访问权限
                ret = GetTid(msg->target.handle, dstTid);  // 获取目标任务ID
                if (ret != LOS_OK) {
                    return -EINVAL;
                }
            } else {
                PRINT_ERR("Liteipc %s, %d\n", __FUNCTION__, __LINE__);
                return -EACCES;
            }
#if (USE_TIMESTAMP == 1)
            msg->timestamp = now;  // 设置请求时间戳
#endif
            break;
        case MT_REPLY:  // 回复消息
        case MT_FAILED_REPLY:  // 失败回复消息
            if ((flag & BUFF_FREE) != BUFF_FREE) {  // 检查是否设置了缓冲区释放标志
                return -EINVAL;
            }
            if (!IsValidReply(content)) {  // 验证回复的有效性
                return -EINVAL;
            }
#if (USE_TIMESTAMP == 1)
            if (now > msg->timestamp + LITEIPC_TIMEOUT_NS) {  // 检查回复是否超时
#ifdef LOSCFG_KERNEL_HOOK
                ret = GetTid(msg->target.handle, dstTid);
                if (ret != LOS_OK) {
                    *dstTid = INVAILD_ID;
                }
#endif
                OsHookCall(LOS_HOOK_TYPE_IPC_WRITE_DROP, msg, *dstTid,  // 调用钩子函数
                           (*dstTid == INVAILD_ID) ? INVAILD_ID : OS_PCB_FROM_TID(*dstTid)->processID, 0);
                PRINT_ERR("Liteipc A timeout reply, request timestamp:%lld, now:%lld\n", msg->timestamp, now);
                return -ETIME;
            }
#endif
            *dstTid = msg->target.handle;  // 设置目标任务ID
            break;
        case MT_DEATH_NOTIFY:  // 死亡通知消息
            *dstTid = msg->target.handle;  // 设置目标任务ID
#if (USE_TIMESTAMP == 1)
            msg->timestamp = now;  // 设置时间戳
#endif
            break;
        default:  // 未知消息类型
            PRINT_DEBUG("Unknown msg type:%d\n", msg->type);
            return -EINVAL;
    }

    if (IsTaskAlive(*dstTid) == FALSE) {  // 检查目标任务是否存活
        return -EINVAL;
    }
    return LOS_OK;
}

/**
 * @brief IPC消息写入的主函数
 * @param content IPC内容结构体指针
 * @return 成功返回LOS_OK，失败返回错误码
 * @note 完成消息验证、数据复制、特殊对象处理和消息入队
 */
LITE_OS_SEC_TEXT STATIC UINT32 LiteIpcWrite(IpcContent *content)
{
    UINT32 ret, intSave;
    UINT32 dstTid;  // 目标任务ID

    IpcMsg *msg = content->outMsg;  // 获取消息结构

    ret = CheckPara(content, &dstTid);  // 检查消息参数
    if (ret != LOS_OK) {
        return ret;
    }

    LosTaskCB *tcb = OS_TCB_FROM_TID(dstTid);  // 获取目标任务控制块
    LosProcessCB *pcb = OS_PCB_FROM_TCB(tcb);  // 获取目标进程控制块
    if (pcb->ipcInfo == NULL) {  // 检查进程IPC信息是否初始化
        PRINT_ERR("pid %u Liteipc not create\n", pcb->processID);
        return -EINVAL;
    }

    // 计算所需缓冲区大小：消息节点大小 + 数据区大小 + 特殊对象偏移数组大小
    UINT32 bufSz = sizeof(IpcListNode) + msg->dataSz + msg->spObjNum * sizeof(UINT32);
    IpcListNode *buf = (IpcListNode *)LiteIpcNodeAlloc(pcb, bufSz);  // 分配消息节点内存
    if (buf == NULL) {
        PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
        return -ENOMEM;
    }
    ret = CopyDataFromUser(buf, bufSz, (const IpcMsg *)msg);  // 从用户空间复制数据
    if (ret != LOS_OK) {
        PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
        goto ERROR_COPY;  // 复制失败，跳转到错误处理
    }

    if (tcb->ipcTaskInfo == NULL) {  // 初始化目标任务的IPC信息（如未初始化）
        tcb->ipcTaskInfo = LiteIpcTaskInit();
    }

    ret = HandleSpecialObjects(dstTid, buf, FALSE);  // 处理特殊对象
    if (ret != LOS_OK) {
        PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
        goto ERROR_COPY;  // 处理失败，跳转到错误处理
    }
    /* 将消息添加到链表并唤醒目标任务 */
    SCHEDULER_LOCK(intSave);  // 关调度器
    LOS_ListTailInsert(&(tcb->ipcTaskInfo->msgListHead), &(buf->listNode));  // 将消息节点插入链表尾部
    OsHookCall(LOS_HOOK_TYPE_IPC_WRITE, &buf->msg, dstTid, pcb->processID, tcb->waitFlag);  // 调用钩子函数
    if (tcb->waitFlag == OS_TASK_WAIT_LITEIPC) {  // 如果目标任务正在等待IPC消息
        OsTaskWakeClearPendMask(tcb);  // 唤醒任务并清除等待标志
        tcb->ops->wake(tcb);
        SCHEDULER_UNLOCK(intSave);  // 开调度器
        LOS_MpSchedule(OS_MP_CPU_ALL);  // 多核调度
        LOS_Schedule();  // 触发调度
    } else {
        SCHEDULER_UNLOCK(intSave);  // 开调度器
    }
    return LOS_OK;
ERROR_COPY:  // 错误处理：释放已分配的内存
    LiteIpcNodeFree(pcb, buf);  // 释放消息节点
    return ret;
}
/**
 * @brief 检查接收到的IPC消息有效性
 * @param node 指向IpcListNode结构体的指针，包含待检查的消息
 * @param content 指向IpcContent结构体的指针，包含IPC操作相关内容
 * @param tcb 指向LosTaskCB结构体的指针，当前任务控制块
 * @return UINT32 成功返回LOS_OK，失败返回错误码
 */
LITE_OS_SEC_TEXT STATIC UINT32 CheckReceivedMsg(IpcListNode *node, IpcContent *content, LosTaskCB *tcb)
{
    UINT32 ret = LOS_OK;  // 初始化返回值为成功
    if (node == NULL) {   // 检查消息节点是否为空
        return -EINVAL;   // 参数无效，返回错误码
    }
    // 根据消息类型进行不同处理
    switch (node->msg.type) {
        case MT_REQUEST:  // 请求类型消息
            // 如果内容标志包含发送标志，则参数无效
            if ((content->flag & SEND) == SEND) {  
                PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);  // 打印错误信息
                ret = -EINVAL;  // 设置返回值为参数无效
            }
            break;
        case MT_FAILED_REPLY:  // 失败回复类型消息
            ret = -ENOENT;      // 设置返回值为未找到
            /* fall-through */  // 继续执行下一个case
        case MT_REPLY:  // 回复类型消息
            // 如果内容标志不包含发送标志，则参数无效
            if ((content->flag & SEND) != SEND) {  
                PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);  // 打印错误信息
                ret = -EINVAL;  // 设置返回值为参数无效
            }
#if (USE_TIMESTAMP == 1)  // 如果启用时间戳功能
            // 检查消息时间戳是否匹配
            if (node->msg.timestamp != content->outMsg->timestamp) {  
                PRINT_ERR("Receive a unmatch reply, drop it\n");  // 打印不匹配错误
                ret = -EINVAL;  // 设置返回值为参数无效
            }
#else  // 未启用时间戳功能
            // 检查消息代码和目标令牌是否匹配
            if ((node->msg.code != content->outMsg->code) ||  
                (node->msg.target.token != content->outMsg->target.token)) {  
                PRINT_ERR("Receive a unmatch reply, drop it\n");  // 打印不匹配错误
                ret = -EINVAL;  // 设置返回值为参数无效
            }
#endif
            break;
        case MT_DEATH_NOTIFY:  // 死亡通知类型消息
            break;  // 无需特殊处理
        default:  // 未知消息类型
            PRINT_ERR("Unknown msg type:%d\n", node->msg.type);  // 打印未知类型错误
            ret =  -EINVAL;  // 设置返回值为参数无效
    }
    if (ret != LOS_OK) {  // 如果检查失败
        // 调用钩子函数，通知IPC消息被丢弃
        OsHookCall(LOS_HOOK_TYPE_IPC_READ_DROP, &node->msg, tcb->waitFlag);  
        // 处理特殊对象
        (VOID)HandleSpecialObjects(LOS_CurTaskIDGet(), node, TRUE);  
        // 释放IPC节点
        (VOID)LiteIpcNodeFree(OsCurrProcessGet(), (VOID *)node);  
    } else {  // 如果检查成功
        // 调用钩子函数，通知IPC消息已读取
        OsHookCall(LOS_HOOK_TYPE_IPC_READ, &node->msg, tcb->waitFlag);  
    }
    return ret;  // 返回检查结果
}

/**
 * @brief 读取IPC消息
 * @param content 指向IpcContent结构体的指针，包含IPC操作相关内容
 * @return UINT32 成功返回LOS_OK，失败返回错误码
 */
LITE_OS_SEC_TEXT STATIC UINT32 LiteIpcRead(IpcContent *content)
{
    UINT32 intSave, ret;  // 中断保存变量和返回值
    UINT32 selfTid = LOS_CurTaskIDGet();  // 获取当前任务ID
    LOS_DL_LIST *listHead = NULL;  // 消息链表头指针
    LOS_DL_LIST *listNode = NULL;  // 消息链表节点指针
    IpcListNode *node = NULL;  // IPC消息节点指针
    // 判断是否为同步模式（同时设置发送和接收标志）
    UINT32 syncFlag = (content->flag & SEND) && (content->flag & RECV);  
    // 同步模式下设置超时时间，异步模式下永久等待
    UINT32 timeout = syncFlag ? LOS_MS2Tick(LITEIPC_TIMEOUT_MS) : LOS_WAIT_FOREVER;  

    LosTaskCB *tcb = OS_TCB_FROM_TID(selfTid);  // 获取当前任务控制块
    if (tcb->ipcTaskInfo == NULL) {  // 如果IPC任务信息未初始化
        tcb->ipcTaskInfo = LiteIpcTaskInit();  // 初始化IPC任务信息
    }

    listHead = &(tcb->ipcTaskInfo->msgListHead);  // 获取消息链表头
    do {  // 循环等待消息
        SCHEDULER_LOCK(intSave);  // 关闭调度器
        if (LOS_ListEmpty(listHead)) {  // 如果消息链表为空
            // 设置任务等待掩码和超时时间
            OsTaskWaitSetPendMask(OS_TASK_WAIT_LITEIPC, OS_INVALID_VALUE, timeout);  
            // 调用钩子函数，通知尝试读取IPC消息
            OsHookCall(LOS_HOOK_TYPE_IPC_TRY_READ, syncFlag ? MT_REPLY : MT_REQUEST, tcb->waitFlag);  
            // 等待IPC消息
            ret = tcb->ops->wait(tcb, &g_ipcPendlist, timeout);  
            if (ret == LOS_ERRNO_TSK_TIMEOUT) {  // 如果等待超时
                // 调用钩子函数，通知IPC读取超时
                OsHookCall(LOS_HOOK_TYPE_IPC_READ_TIMEOUT, syncFlag ? MT_REPLY : MT_REQUEST, tcb->waitFlag);  
                SCHEDULER_UNLOCK(intSave);  // 开启调度器
                return -ETIME;  // 返回超时错误
            }

            if (OsTaskIsKilled(tcb)) {  // 如果任务已被杀死
                // 调用钩子函数，通知IPC任务被杀死
                OsHookCall(LOS_HOOK_TYPE_IPC_KILL, syncFlag ? MT_REPLY : MT_REQUEST, tcb->waitFlag);  
                SCHEDULER_UNLOCK(intSave);  // 开启调度器
                return -ERFKILL;  // 返回任务被杀死错误
            }

            SCHEDULER_UNLOCK(intSave);  // 开启调度器
        } else {  // 如果消息链表非空
            listNode = LOS_DL_LIST_FIRST(listHead);  // 获取第一个链表节点
            LOS_ListDelete(listNode);  // 从链表中删除节点
            // 将链表节点转换为IpcListNode类型
            node = LOS_DL_LIST_ENTRY(listNode, IpcListNode, listNode);  
            SCHEDULER_UNLOCK(intSave);  // 开启调度器
            ret = CheckReceivedMsg(node, content, tcb);  // 检查消息有效性
            if (ret == LOS_OK) {  // 如果消息有效
                break;  // 跳出循环
            }
            if (ret == -ENOENT) {  // 如果收到失败回复
                return ret;  // 返回错误码
            }
        }
    } while (1);  // 无限循环，直到获取有效消息
    // 将内核空间地址转换为用户空间地址
    node->msg.data = (VOID *)GetIpcUserAddr(OsCurrProcessGet(), (INTPTR)(node->msg.data));  
    node->msg.offsets = (VOID *)GetIpcUserAddr(OsCurrProcessGet(), (INTPTR)(node->msg.offsets));  
    content->inMsg = (VOID *)GetIpcUserAddr(OsCurrProcessGet(), (INTPTR)(&(node->msg)));  
    // 允许用户空间释放IPC节点
    EnableIpcNodeFreeByUser(OsCurrProcessGet(), (VOID *)node);  
    return LOS_OK;  // 返回成功
}

/**
 * @brief IPC消息处理主函数
 * @param con 指向IpcContent结构体的指针，用户空间传入的IPC内容
 * @return UINT32 成功返回LOS_OK，失败返回错误码
 */
LITE_OS_SEC_TEXT STATIC UINT32 LiteIpcMsgHandle(IpcContent *con)
{
    UINT32 ret = LOS_OK;  // 初始化返回值为成功
    IpcContent localContent;  // 本地IPC内容结构体
    IpcContent *content = &localContent;  // 指向本地IPC内容的指针
    IpcMsg localMsg;  // 本地消息结构体
    IpcMsg *msg = &localMsg;  // 指向本地消息的指针
    IpcListNode *nodeNeedFree = NULL;  // 需要释放的IPC节点指针

    // 从用户空间复制IPC内容到内核空间
    if (copy_from_user((void *)content, (const void *)con, sizeof(IpcContent)) != LOS_OK) {  
        PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);  // 打印复制错误
        return -EINVAL;  // 返回参数无效错误
    }

    if ((content->flag & BUFF_FREE) == BUFF_FREE) {  // 如果需要释放缓冲区
        // 检查并获取需要释放的节点
        ret = CheckUsedBuffer(content->buffToFree, &nodeNeedFree);  
        if (ret != LOS_OK) {  // 如果检查失败
            PRINT_ERR("CheckUsedBuffer failed:%d\n", ret);  // 打印检查失败错误
            return ret;  // 返回错误码
        }
    }

    if ((content->flag & SEND) == SEND) {  // 如果需要发送消息
        if (content->outMsg == NULL) {  // 如果输出消息为空
            PRINT_ERR("content->outmsg is null\n");  // 打印空指针错误
            ret = -EINVAL;  // 设置返回值为参数无效
            goto BUFFER_FREE;  // 跳转到缓冲区释放
        }
        // 从用户空间复制消息到内核空间
        if (copy_from_user((void *)msg, (const void *)content->outMsg, sizeof(IpcMsg)) != LOS_OK) {  
            PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);  // 打印复制错误
            ret = -EINVAL;  // 设置返回值为参数无效
            goto BUFFER_FREE;  // 跳转到缓冲区释放
        }
        content->outMsg = msg;  // 更新输出消息指针
        // 检查消息类型是否有效
        if ((content->outMsg->type < 0) || (content->outMsg->type >= MT_DEATH_NOTIFY)) {  
            PRINT_ERR("LiteIpc unknown msg type:%d\n", content->outMsg->type);  // 打印未知类型错误
            ret = -EINVAL;  // 设置返回值为参数无效
            goto BUFFER_FREE;  // 跳转到缓冲区释放
        }
        ret = LiteIpcWrite(content);  // 调用LiteIpcWrite发送消息
        if (ret != LOS_OK) {  // 如果发送失败
            PRINT_ERR("LiteIpcWrite failed\n");  // 打印发送失败错误
            goto BUFFER_FREE;  // 跳转到缓冲区释放
        }
    }
BUFFER_FREE:  // 缓冲区释放标签
    if (nodeNeedFree != NULL) {  // 如果存在需要释放的节点
        // 释放IPC节点
        UINT32 freeRet = LiteIpcNodeFree(OsCurrProcessGet(), nodeNeedFree);  
        // 更新返回值（如果释放失败，优先返回释放错误）
        ret = (freeRet == LOS_OK) ? ret : freeRet;  
    }
    if (ret != LOS_OK) {  // 如果处理失败
        return ret;  // 返回错误码
    }

    if ((content->flag & RECV) == RECV) {  // 如果需要接收消息
        ret = LiteIpcRead(content);  // 调用LiteIpcRead接收消息
        if (ret != LOS_OK) {  // 如果接收失败
            PRINT_ERR("LiteIpcRead failed ERROR: %d\n", (INT32)ret);  // 打印接收失败错误
            return ret;  // 返回错误码
        }
        // 计算inMsg成员在IpcContent结构体中的偏移量
        UINT32 offset = LOS_OFF_SET_OF(IpcContent, inMsg);  
        // 将接收到的消息地址复制回用户空间
        ret = copy_to_user((char*)con + offset, (char*)content + offset, sizeof(IpcMsg *));  
        if (ret != LOS_OK) {  // 如果复制失败
            PRINT_ERR("%s, %d, %d\n", __FUNCTION__, __LINE__, ret);  // 打印复制错误
            return -EINVAL;  // 返回参数无效错误
        }
    }
    return ret;  // 返回处理结果
}

/**
 * @brief 处理CMS（通信管理服务）命令
 * @param content 指向CmsCmdContent结构体的指针，包含CMS命令内容
 * @return UINT32 成功返回LOS_OK，失败返回错误码
 */
LITE_OS_SEC_TEXT STATIC UINT32 HandleCmsCmd(CmsCmdContent *content)
{
    UINT32 ret = LOS_OK;  // 初始化返回值为成功
    CmsCmdContent localContent;  // 本地CMS命令内容结构体
    if (content == NULL) {  // 如果命令内容为空
        return -EINVAL;  // 返回参数无效错误
    }
    // 检查当前任务是否为CMS任务
    if (IsCmsTask(LOS_CurTaskIDGet()) == FALSE) {  
        return -EACCES;  // 返回权限拒绝错误
    }
    // 从用户空间复制命令内容到内核空间
    if (copy_from_user((void *)(&localContent), (const void *)content, sizeof(CmsCmdContent)) != LOS_OK) {  
        PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);  // 打印复制错误
        return -EINVAL;  // 返回参数无效错误
    }
    // 根据命令类型进行不同处理
    switch (localContent.cmd) {
        case CMS_GEN_HANDLE:  // 生成服务句柄命令
            // 如果指定了任务ID且任务不存在
            if ((localContent.taskID != 0) && (IsTaskAlive(localContent.taskID) == FALSE)) {  
                return -EINVAL;  // 返回参数无效错误
            }
            // 生成服务句柄
            ret = GenerateServiceHandle(localContent.taskID, HANDLE_REGISTED, &(localContent.serviceHandle));  
            if (ret == LOS_OK) {  // 如果生成成功
                // 将生成的句柄复制回用户空间
                ret = copy_to_user((void *)content, (const void *)(&localContent), sizeof(CmsCmdContent));  
            }
            // 加锁保护服务句柄映射
            (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);  
            // 添加CMS任务对服务句柄的访问权限
            AddServiceAccess(g_cmsTask.taskID, localContent.serviceHandle);  
            (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);  // 解锁
            break;
        case CMS_REMOVE_HANDLE:  // 移除服务句柄命令
            // 检查服务句柄是否超出范围
            if (localContent.serviceHandle >= MAX_SERVICE_NUM) {  
                return -EINVAL;  // 返回参数无效错误
            }
            // 刷新服务句柄（设置为无效）
            RefreshServiceHandle(localContent.serviceHandle, -1);  
            break;
        case CMS_ADD_ACCESS:  // 添加访问权限命令
            // 检查任务是否存活
            if (IsTaskAlive(localContent.taskID) == FALSE) {  
                return -EINVAL;  // 返回参数无效错误
            }
            // 添加任务对服务句柄的访问权限
            return AddServiceAccess(localContent.taskID, localContent.serviceHandle);  
        default:  // 未知命令
            PRINT_DEBUG("Unknown cmd cmd:%d\n", localContent.cmd);  // 打印未知命令调试信息
            return -EINVAL;  // 返回参数无效错误
    }
    return ret;  // 返回处理结果
}
/**
 * @brief 处理获取IPC版本信息请求
 * @param version 指向IpcVersion结构体的指针，用于存储版本信息
 * @return UINT32 成功返回LOS_OK，失败返回错误码
 */
LITE_OS_SEC_TEXT STATIC UINT32 HandleGetVersion(IpcVersion *version)
{
    UINT32 ret = LOS_OK;  // 初始化返回值为成功
    IpcVersion localIpcVersion;  // 本地版本信息结构体
    if (version == NULL) {  // 检查输入参数是否为空
        return -EINVAL;  // 参数无效，返回错误码
    }

    localIpcVersion.driverVersion = DRIVER_VERSION;  // 设置驱动版本号
    // 将本地版本信息复制到用户空间
    ret = copy_to_user((void *)version, (const void *)(&localIpcVersion), sizeof(IpcVersion));  
    if (ret != LOS_OK) {  // 检查复制操作是否成功
        PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);  // 打印复制失败错误
    }
    return ret;  // 返回操作结果
}

/**
 * @brief IPC设备控制函数，处理各类IPC相关命令
 * @param filep 文件指针，指向打开的IPC设备文件
 * @param cmd 控制命令，指定要执行的IPC操作类型
 * @param arg 命令参数，根据不同命令类型指向不同的数据结构
 * @return int 成功返回0或正整数，失败返回负错误码
 */
LITE_OS_SEC_TEXT int LiteIpcIoctl(struct file *filep, int cmd, unsigned long arg)
{
    UINT32 ret = LOS_OK;  // 初始化返回值为成功
    LosProcessCB *pcb = OsCurrProcessGet();  // 获取当前进程控制块
    ProcIpcInfo *ipcInfo = pcb->ipcInfo;  // 获取当前进程的IPC信息

    if (ipcInfo == NULL) {  // 检查IPC信息是否初始化
        PRINT_ERR("Liteipc pool not create!\n");  // 打印IPC池未创建错误
        return -EINVAL;  // 返回参数无效错误
    }

    if (IsPoolMapped(ipcInfo) == FALSE) {  // 检查IPC池是否已映射
        PRINT_ERR("Liteipc Ipc pool not init, need to mmap first!\n");  // 打印IPC池未初始化错误
        return -ENOMEM;  // 返回内存不足错误
    }

    // 根据命令类型执行不同操作
    switch (cmd) {
        case IPC_SET_CMS:  // 设置CMS（通信管理服务）任务
            return (INT32)SetCms(arg);  // 调用SetCms函数并返回结果
        case IPC_CMS_CMD:  // 处理CMS命令
            // 调用HandleCmsCmd函数处理CMS命令并返回结果
            return (INT32)HandleCmsCmd((CmsCmdContent *)(UINTPTR)arg);  
        case IPC_GET_VERSION:  // 获取IPC版本信息
            // 调用HandleGetVersion函数获取版本信息并返回结果
            return (INT32)HandleGetVersion((IpcVersion *)(UINTPTR)arg);  
        case IPC_SET_IPC_THREAD:  // 设置IPC线程
            if (IsCmsSet() == FALSE) {  // 检查CMS是否已设置
                PRINT_ERR("Liteipc ServiceManager not set!\n");  // 打印服务管理器未设置错误
                return -EINVAL;  // 返回参数无效错误
            }
            return (INT32)SetIpcTask();  // 调用SetIpcTask函数并返回结果
        case IPC_SEND_RECV_MSG:  // 发送和接收IPC消息
            if (arg == 0) {  // 检查参数是否为空
                return -EINVAL;  // 返回参数无效错误
            }
            if (IsCmsSet() == FALSE) {  // 检查CMS是否已设置
                PRINT_ERR("Liteipc ServiceManager not set!\n");  // 打印服务管理器未设置错误
                return -EINVAL;  // 返回参数无效错误
            }
            // 调用LiteIpcMsgHandle函数处理消息发送和接收
            ret = LiteIpcMsgHandle((IpcContent *)(UINTPTR)arg);  
            if (ret != LOS_OK) {  // 检查消息处理是否成功
                return (INT32)ret;  // 返回错误码
            }
            break;  // 跳出switch语句
        default:  // 未知命令
            PRINT_ERR("Unknown liteipc ioctl cmd:%d\n", cmd);  // 打印未知命令错误
            return -EINVAL;  // 返回参数无效错误
    }
    return (INT32)ret;  // 返回操作结果
}