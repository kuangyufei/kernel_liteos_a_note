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
 * @date    2021-11-22
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

#define USE_TASKID_AS_HANDLE 1
#define USE_MMAP 1
#define IPC_IO_DATA_MAX 8192UL
#define IPC_MSG_DATA_SZ_MAX (IPC_IO_DATA_MAX * sizeof(SpecialObj) / (sizeof(SpecialObj) + sizeof(size_t)))
#define IPC_MSG_OBJECT_NUM_MAX (IPC_MSG_DATA_SZ_MAX / sizeof(SpecialObj))

#define LITE_IPC_POOL_NAME "liteipc"
#define LITE_IPC_POOL_PAGE_MAX_NUM 64 /* 256KB */
#define LITE_IPC_POOL_PAGE_DEFAULT_NUM 16 /* 64KB */
#define LITE_IPC_POOL_MAX_SIZE (LITE_IPC_POOL_PAGE_MAX_NUM << PAGE_SHIFT)
#define LITE_IPC_POOL_DEFAULT_SIZE (LITE_IPC_POOL_PAGE_DEFAULT_NUM << PAGE_SHIFT)
#define LITE_IPC_POOL_UVADDR 0x10000000
#define INVAILD_ID (-1)

#define LITEIPC_TIMEOUT_MS 5000UL
#define LITEIPC_TIMEOUT_NS 5000000000ULL

#define MAJOR_VERSION (2)
#define MINOR_VERSION (0)
#define DRIVER_VERSION (MAJOR_VERSION | MINOR_VERSION << 16)

typedef struct {
    LOS_DL_LIST list;
    VOID *ptr;
} IpcUsedNode;

STATIC LosMux g_serviceHandleMapMux;
#if (USE_TASKID_AS_HANDLE == 1)
STATIC HandleInfo g_cmsTask;
#else
STATIC HandleInfo g_serviceHandleMap[MAX_SERVICE_NUM];
#endif
STATIC LOS_DL_LIST g_ipcPendlist;

/* ipc lock */
SPIN_LOCK_INIT(g_ipcSpin);
#define IPC_LOCK(state)       LOS_SpinLockSave(&g_ipcSpin, &(state))
#define IPC_UNLOCK(state)     LOS_SpinUnlockRestore(&g_ipcSpin, state)

STATIC int LiteIpcOpen(struct file *filep);
STATIC int LiteIpcClose(struct file *filep);
STATIC int LiteIpcIoctl(struct file *filep, int cmd, unsigned long arg);
STATIC int LiteIpcMmap(struct file* filep, LosVmMapRegion *region);
STATIC UINT32 LiteIpcWrite(IpcContent *content);
STATIC UINT32 GetTid(UINT32 serviceHandle, UINT32 *taskID);
STATIC UINT32 HandleSpecialObjects(UINT32 dstTid, IpcListNode *node, BOOL isRollback);
STATIC ProcIpcInfo *LiteIpcPoolCreate(VOID);

STATIC const struct file_operations_vfs g_liteIpcFops = {
    .open = LiteIpcOpen,   /* open */
    .close = LiteIpcClose,  /* close */
    .ioctl = LiteIpcIoctl,  /* ioctl */
    .mmap = LiteIpcMmap,   /* mmap */
};

LITE_OS_SEC_TEXT_INIT UINT32 OsLiteIpcInit(VOID)
{
    UINT32 ret;
#if (USE_TASKID_AS_HANDLE == 1)
    g_cmsTask.status = HANDLE_NOT_USED;
#else
    (void)memset_s(g_serviceHandleMap, sizeof(g_serviceHandleMap), 0, sizeof(g_serviceHandleMap));
#endif
    ret = LOS_MuxInit(&g_serviceHandleMapMux, NULL);
    if (ret != LOS_OK) {
        return ret;
    }
    ret = (UINT32)register_driver(LITEIPC_DRIVER, &g_liteIpcFops, LITEIPC_DRIVER_MODE, NULL);
    if (ret != LOS_OK) {
        PRINT_ERR("register lite_ipc driver failed:%d\n", ret);
    }
    LOS_ListInit(&(g_ipcPendlist));

    return ret;
}

LOS_MODULE_INIT(OsLiteIpcInit, LOS_INIT_LEVEL_KMOD_EXTENDED);

LITE_OS_SEC_TEXT STATIC int LiteIpcOpen(struct file *filep)
{
    LosProcessCB *pcb = OsCurrProcessGet();
    if (pcb->ipcInfo != NULL) {
        return 0;
    }

    pcb->ipcInfo = LiteIpcPoolCreate();
    if (pcb->ipcInfo == NULL) {
        return -ENOMEM;
    }

    return 0;
}

LITE_OS_SEC_TEXT STATIC int LiteIpcClose(struct file *filep)
{
    return 0;
}

LITE_OS_SEC_TEXT STATIC BOOL IsPoolMapped(ProcIpcInfo *ipcInfo)
{
    return (ipcInfo->pool.uvaddr != NULL) && (ipcInfo->pool.kvaddr != NULL) &&
        (ipcInfo->pool.poolSize != 0);
}

LITE_OS_SEC_TEXT STATIC INT32 DoIpcMmap(LosProcessCB *pcb, LosVmMapRegion *region)
{
    UINT32 i;
    INT32 ret = 0;
    PADDR_T pa;
    UINT32 uflags = VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_USER;
    LosVmPage *vmPage = NULL;
    VADDR_T uva = (VADDR_T)(UINTPTR)pcb->ipcInfo->pool.uvaddr;
    VADDR_T kva = (VADDR_T)(UINTPTR)pcb->ipcInfo->pool.kvaddr;

    (VOID)LOS_MuxAcquire(&pcb->vmSpace->regionMux);

    for (i = 0; i < (region->range.size >> PAGE_SHIFT); i++) {
        pa = LOS_PaddrQuery((VOID *)(UINTPTR)(kva + (i << PAGE_SHIFT)));
        if (pa == 0) {
            PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
            ret = -EINVAL;
            break;
        }
        vmPage = LOS_VmPageGet(pa);
        if (vmPage == NULL) {
            PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
            ret = -EINVAL;
            break;
        }
        STATUS_T err = LOS_ArchMmuMap(&pcb->vmSpace->archMmu, uva + (i << PAGE_SHIFT), pa, 1, uflags);
        if (err < 0) {
            ret = err;
            PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
            break;
        }
    }
    /* if any failure happened, rollback */
    if (i != (region->range.size >> PAGE_SHIFT)) {
        while (i--) {
            pa = LOS_PaddrQuery((VOID *)(UINTPTR)(kva + (i << PAGE_SHIFT)));
            vmPage = LOS_VmPageGet(pa);
            (VOID)LOS_ArchMmuUnmap(&pcb->vmSpace->archMmu, uva + (i << PAGE_SHIFT), 1);
            LOS_PhysPageFree(vmPage);
        }
    }

    (VOID)LOS_MuxRelease(&pcb->vmSpace->regionMux);
    return ret;
}

LITE_OS_SEC_TEXT STATIC int LiteIpcMmap(struct file *filep, LosVmMapRegion *region)
{
    int ret = 0;
    LosVmMapRegion *regionTemp = NULL;
    LosProcessCB *pcb = OsCurrProcessGet();
    ProcIpcInfo *ipcInfo = pcb->ipcInfo;

    if ((ipcInfo == NULL) || (region == NULL) || (region->range.size > LITE_IPC_POOL_MAX_SIZE) ||
        (!LOS_IsRegionPermUserReadOnly(region)) || (!LOS_IsRegionFlagPrivateOnly(region))) {
        ret = -EINVAL;
        goto ERROR_REGION_OUT;
    }
    if (IsPoolMapped(ipcInfo)) {
        return -EEXIST;
    }
    if (ipcInfo->pool.uvaddr != NULL) {
        regionTemp = LOS_RegionFind(pcb->vmSpace, (VADDR_T)(UINTPTR)ipcInfo->pool.uvaddr);
        if ((regionTemp != NULL) && (regionTemp != region)) {
            (VOID)LOS_RegionFree(pcb->vmSpace, regionTemp);
        }
    }
    ipcInfo->pool.uvaddr = (VOID *)(UINTPTR)region->range.base;
    if (ipcInfo->pool.kvaddr != NULL) {
        LOS_VFree(ipcInfo->pool.kvaddr);
        ipcInfo->pool.kvaddr = NULL;
    }
    /* use vmalloc to alloc phy mem */
    ipcInfo->pool.kvaddr = LOS_VMalloc(region->range.size);
    if (ipcInfo->pool.kvaddr == NULL) {
        ret = -ENOMEM;
        goto ERROR_REGION_OUT;
    }
    /* do mmap */
    ret = DoIpcMmap(pcb, region);
    if (ret) {
        goto ERROR_MAP_OUT;
    }
    /* ipc pool init */
    if (LOS_MemInit(ipcInfo->pool.kvaddr, region->range.size) != LOS_OK) {
        ret = -EINVAL;
        goto ERROR_MAP_OUT;
    }
    ipcInfo->pool.poolSize = region->range.size;
    region->regionFlags |= VM_MAP_REGION_FLAG_LITEIPC;
    return 0;
ERROR_MAP_OUT:
    LOS_VFree(ipcInfo->pool.kvaddr);
ERROR_REGION_OUT:
    if (ipcInfo != NULL) {
        ipcInfo->pool.uvaddr = NULL;
        ipcInfo->pool.kvaddr = NULL;
    }
    return ret;
}

LITE_OS_SEC_TEXT_INIT STATIC UINT32 LiteIpcPoolInit(ProcIpcInfo *ipcInfo)
{
    ipcInfo->pool.uvaddr = NULL;
    ipcInfo->pool.kvaddr = NULL;
    ipcInfo->pool.poolSize = 0;
    ipcInfo->ipcTaskID = INVAILD_ID;
    LOS_ListInit(&ipcInfo->ipcUsedNodelist);
    return LOS_OK;
}

LITE_OS_SEC_TEXT_INIT STATIC ProcIpcInfo *LiteIpcPoolCreate(VOID)
{
    ProcIpcInfo *ipcInfo = LOS_MemAlloc(m_aucSysMem1, sizeof(ProcIpcInfo));
    if (ipcInfo == NULL) {
        return NULL;
    }

    (VOID)memset_s(ipcInfo, sizeof(ProcIpcInfo), 0, sizeof(ProcIpcInfo));

    (VOID)LiteIpcPoolInit(ipcInfo);
    return ipcInfo;
}

LITE_OS_SEC_TEXT ProcIpcInfo *LiteIpcPoolReInit(const ProcIpcInfo *parent)
{
    ProcIpcInfo *ipcInfo = LiteIpcPoolCreate();
    if (ipcInfo == NULL) {
        return NULL;
    }

    ipcInfo->pool.uvaddr = parent->pool.uvaddr;
    ipcInfo->pool.kvaddr = NULL;
    ipcInfo->pool.poolSize = 0;
    ipcInfo->ipcTaskID = INVAILD_ID;
    return ipcInfo;
}

STATIC VOID LiteIpcPoolDelete(ProcIpcInfo *ipcInfo, UINT32 processID)
{
    UINT32 intSave;
    IpcUsedNode *node = NULL;
    if (ipcInfo->pool.kvaddr != NULL) {
        LOS_VFree(ipcInfo->pool.kvaddr);
        ipcInfo->pool.kvaddr = NULL;
        IPC_LOCK(intSave);
        while (!LOS_ListEmpty(&ipcInfo->ipcUsedNodelist)) {
            node = LOS_DL_LIST_ENTRY(ipcInfo->ipcUsedNodelist.pstNext, IpcUsedNode, list);
            LOS_ListDelete(&node->list);
            free(node);
        }
        IPC_UNLOCK(intSave);
    }
    /* remove process access to service */
    for (UINT32 i = 0; i < MAX_SERVICE_NUM; i++) {
        if (ipcInfo->access[i] == TRUE) {
            ipcInfo->access[i] = FALSE;
            if (OS_TCB_FROM_TID(i)->ipcTaskInfo != NULL) {
                OS_TCB_FROM_TID(i)->ipcTaskInfo->accessMap[processID] = FALSE;
            }
        }
    }
}

LITE_OS_SEC_TEXT UINT32 LiteIpcPoolDestroy(UINT32 processID)
{
    LosProcessCB *pcb = OS_PCB_FROM_PID(processID);
    if (pcb->ipcInfo == NULL) {
        return LOS_NOK;
    }

    LiteIpcPoolDelete(pcb->ipcInfo, pcb->processID);
    LOS_MemFree(m_aucSysMem1, pcb->ipcInfo);
    pcb->ipcInfo = NULL;
    return LOS_OK;
}

LITE_OS_SEC_TEXT_INIT STATIC IpcTaskInfo *LiteIpcTaskInit(VOID)
{
    IpcTaskInfo *taskInfo = LOS_MemAlloc((VOID *)m_aucSysMem1, sizeof(IpcTaskInfo));
    if (taskInfo == NULL) {
        return NULL;
    }

    (VOID)memset_s(taskInfo, sizeof(IpcTaskInfo), 0, sizeof(IpcTaskInfo));
    LOS_ListInit(&taskInfo->msgListHead);
    return taskInfo;
}

/* Only when kernenl no longer access ipc node content, can user free the ipc node */
LITE_OS_SEC_TEXT STATIC VOID EnableIpcNodeFreeByUser(LosProcessCB *pcb, VOID *buf)
{
    UINT32 intSave;
    ProcIpcInfo *ipcInfo = pcb->ipcInfo;
    IpcUsedNode *node = (IpcUsedNode *)malloc(sizeof(IpcUsedNode));
    if (node != NULL) {
        node->ptr = buf;
        IPC_LOCK(intSave);
        LOS_ListAdd(&ipcInfo->ipcUsedNodelist, &node->list);
        IPC_UNLOCK(intSave);
    }
}

LITE_OS_SEC_TEXT STATIC VOID *LiteIpcNodeAlloc(LosProcessCB *pcb, UINT32 size)
{
    VOID *ptr = LOS_MemAlloc(pcb->ipcInfo->pool.kvaddr, size);
    PRINT_INFO("LiteIpcNodeAlloc pid:%d, pool:%x buf:%x size:%d\n",
               pcb->processID, pcb->ipcInfo->pool.kvaddr, ptr, size);
    return ptr;
}

LITE_OS_SEC_TEXT STATIC UINT32 LiteIpcNodeFree(LosProcessCB *pcb, VOID *buf)
{
    PRINT_INFO("LiteIpcNodeFree pid:%d, pool:%x buf:%x\n",
               pcb->processID, pcb->ipcInfo->pool.kvaddr, buf);
    return LOS_MemFree(pcb->ipcInfo->pool.kvaddr, buf);
}

LITE_OS_SEC_TEXT STATIC BOOL IsIpcNode(LosProcessCB *pcb, const VOID *buf)
{
    IpcUsedNode *node = NULL;
    UINT32 intSave;
    ProcIpcInfo *ipcInfo = pcb->ipcInfo;
    IPC_LOCK(intSave);
    LOS_DL_LIST_FOR_EACH_ENTRY(node, &ipcInfo->ipcUsedNodelist, IpcUsedNode, list) {
        if (node->ptr == buf) {
            LOS_ListDelete(&node->list);
            IPC_UNLOCK(intSave);
            free(node);
            return TRUE;
        }
    }
    IPC_UNLOCK(intSave);
    return FALSE;
}

LITE_OS_SEC_TEXT STATIC INTPTR GetIpcUserAddr(const LosProcessCB *pcb, INTPTR kernelAddr)
{
    IpcPool pool = pcb->ipcInfo->pool;
    INTPTR offset = (INTPTR)(pool.uvaddr) - (INTPTR)(pool.kvaddr);
    return kernelAddr + offset;
}

LITE_OS_SEC_TEXT STATIC INTPTR GetIpcKernelAddr(const LosProcessCB *pcb, INTPTR userAddr)
{
    IpcPool pool = pcb->ipcInfo->pool;
    INTPTR offset = (INTPTR)(pool.uvaddr) - (INTPTR)(pool.kvaddr);
    return userAddr - offset;
}

LITE_OS_SEC_TEXT STATIC UINT32 CheckUsedBuffer(const VOID *node, IpcListNode **outPtr)
{
    VOID *ptr = NULL;
    LosProcessCB *pcb = OsCurrProcessGet();
    IpcPool pool = pcb->ipcInfo->pool;
    if ((node == NULL) || ((INTPTR)node < (INTPTR)(pool.uvaddr)) ||
        ((INTPTR)node > (INTPTR)(pool.uvaddr) + pool.poolSize)) {
        return -EINVAL;
    }
    ptr = (VOID *)GetIpcKernelAddr(pcb, (INTPTR)(node));
    if (IsIpcNode(pcb, ptr) != TRUE) {
        return -EFAULT;
    }
    *outPtr = (IpcListNode *)ptr;
    return LOS_OK;
}

LITE_OS_SEC_TEXT STATIC UINT32 GetTid(UINT32 serviceHandle, UINT32 *taskID)
{
    if (serviceHandle >= MAX_SERVICE_NUM) {
        return -EINVAL;
    }
    (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);
#if (USE_TASKID_AS_HANDLE == 1)
    *taskID = serviceHandle ? serviceHandle : g_cmsTask.taskID;
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
    return LOS_OK;
#else
    if (g_serviceHandleMap[serviceHandle].status == HANDLE_REGISTED) {
        *taskID = g_serviceHandleMap[serviceHandle].taskID;
        (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
        return LOS_OK;
    }
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
    return -EINVAL;
#endif
}

LITE_OS_SEC_TEXT STATIC UINT32 GenerateServiceHandle(UINT32 taskID, HandleStatus status, UINT32 *serviceHandle)
{
    (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);
#if (USE_TASKID_AS_HANDLE == 1)
    *serviceHandle = taskID ? taskID : LOS_CurTaskIDGet(); /* if taskID is 0, return curTaskID */
    if (*serviceHandle != g_cmsTask.taskID) {
        (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
        return LOS_OK;
    }
#else
    for (UINT32 i = 1; i < MAX_SERVICE_NUM; i++) {
        if (g_serviceHandleMap[i].status == HANDLE_NOT_USED) {
            g_serviceHandleMap[i].taskID = taskID;
            g_serviceHandleMap[i].status = status;
            *serviceHandle = i;
            (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
            return LOS_OK;
        }
    }
#endif
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
    return -EINVAL;
}

LITE_OS_SEC_TEXT STATIC VOID RefreshServiceHandle(UINT32 serviceHandle, UINT32 result)
{
#if (USE_TASKID_AS_HANDLE == 0)
    (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);
    if ((result == LOS_OK) && (g_serviceHandleMap[serviceHandle].status == HANDLE_REGISTING)) {
        g_serviceHandleMap[serviceHandle].status = HANDLE_REGISTED;
    } else {
        g_serviceHandleMap[serviceHandle].status = HANDLE_NOT_USED;
    }
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
#endif
}

LITE_OS_SEC_TEXT STATIC UINT32 AddServiceAccess(UINT32 taskID, UINT32 serviceHandle)
{
    UINT32 serviceTid = 0;
    UINT32 ret = GetTid(serviceHandle, &serviceTid);
    if (ret != LOS_OK) {
        PRINT_ERR("Liteipc AddServiceAccess GetTid failed\n");
        return ret;
    }

    LosTaskCB *tcb = OS_TCB_FROM_TID(serviceTid);
    LosProcessCB *pcb = OS_PCB_FROM_TID(taskID);
    if ((tcb->ipcTaskInfo == NULL) || (pcb->ipcInfo == NULL)) {
        PRINT_ERR("Liteipc AddServiceAccess ipc not create! pid %u tid %u\n", pcb->processID, tcb->taskID);
        return -EINVAL;
    }
    tcb->ipcTaskInfo->accessMap[pcb->processID] = TRUE;
    pcb->ipcInfo->access[serviceTid] = TRUE;
    return LOS_OK;
}

LITE_OS_SEC_TEXT STATIC BOOL HasServiceAccess(UINT32 serviceHandle)
{
    UINT32 serviceTid = 0;
    LosProcessCB *curr = OsCurrProcessGet();
    UINT32 ret;
    if (serviceHandle >= MAX_SERVICE_NUM) {
        return FALSE;
    }
    if (serviceHandle == 0) {
        return TRUE;
    }
    ret = GetTid(serviceHandle, &serviceTid);
    if (ret != LOS_OK) {
        PRINT_ERR("Liteipc HasServiceAccess GetTid failed\n");
        return FALSE;
    }
    LosTaskCB *taskCB = OS_TCB_FROM_TID(serviceTid);
    if (taskCB->processCB == (UINTPTR)curr) {
        return TRUE;
    }

    if (taskCB->ipcTaskInfo == NULL) {
        return FALSE;
    }

    return taskCB->ipcTaskInfo->accessMap[curr->processID];
}

LITE_OS_SEC_TEXT STATIC UINT32 SetIpcTask(VOID)
{
    if (OsCurrProcessGet()->ipcInfo->ipcTaskID == INVAILD_ID) {
        OsCurrProcessGet()->ipcInfo->ipcTaskID = LOS_CurTaskIDGet();
        return OsCurrProcessGet()->ipcInfo->ipcTaskID;
    }
    PRINT_ERR("Liteipc curprocess %d IpcTask already set!\n", OsCurrProcessGet()->processID);
    return -EINVAL;
}

LITE_OS_SEC_TEXT BOOL IsIpcTaskSet(VOID)
{
    if (OsCurrProcessGet()->ipcInfo->ipcTaskID == INVAILD_ID) {
        return FALSE;
    }
    return TRUE;
}

LITE_OS_SEC_TEXT STATIC UINT32 GetIpcTaskID(LosProcessCB *pcb, UINT32 *ipcTaskID)
{
    if (pcb->ipcInfo->ipcTaskID == INVAILD_ID) {
        return LOS_NOK;
    }
    *ipcTaskID = pcb->ipcInfo->ipcTaskID;
    return LOS_OK;
}

LITE_OS_SEC_TEXT STATIC UINT32 SendDeathMsg(UINT32 processID, UINT32 serviceHandle)
{
    UINT32 ipcTaskID;
    UINT32 ret;
    IpcContent content;
    IpcMsg msg;
    LosProcessCB *pcb = OS_PCB_FROM_PID(processID);

    if (pcb->ipcInfo == NULL) {
        return -EINVAL;
    }

    pcb->ipcInfo->access[serviceHandle] = FALSE;

    ret = GetIpcTaskID(pcb, &ipcTaskID);
    if (ret != LOS_OK) {
        return -EINVAL;
    }
    content.flag = SEND;
    content.outMsg = &msg;
    (void)memset_s(content.outMsg, sizeof(IpcMsg), 0, sizeof(IpcMsg));
    content.outMsg->type = MT_DEATH_NOTIFY;
    content.outMsg->target.handle = ipcTaskID;
    content.outMsg->target.token = serviceHandle;
    content.outMsg->code = 0;
    return LiteIpcWrite(&content);
}

LITE_OS_SEC_TEXT VOID LiteIpcRemoveServiceHandle(UINT32 taskID)
{
    UINT32 j;
    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);
    IpcTaskInfo *ipcTaskInfo = taskCB->ipcTaskInfo;
    if (ipcTaskInfo == NULL) {
        return;
    }

#if (USE_TASKID_AS_HANDLE == 1)

    UINT32 intSave;
    LOS_DL_LIST *listHead = NULL;
    LOS_DL_LIST *listNode = NULL;
    IpcListNode *node = NULL;
    LosProcessCB *pcb = OS_PCB_FROM_TCB(taskCB);

    listHead = &(ipcTaskInfo->msgListHead);
    do {
        SCHEDULER_LOCK(intSave);
        if (LOS_ListEmpty(listHead)) {
            SCHEDULER_UNLOCK(intSave);
            break;
        } else {
            listNode = LOS_DL_LIST_FIRST(listHead);
            LOS_ListDelete(listNode);
            node = LOS_DL_LIST_ENTRY(listNode, IpcListNode, listNode);
            SCHEDULER_UNLOCK(intSave);
            (VOID)HandleSpecialObjects(taskCB->taskID, node, TRUE);
            (VOID)LiteIpcNodeFree(pcb, (VOID *)node);
        }
    } while (1);

    ipcTaskInfo->accessMap[pcb->processID] = FALSE;
    for (j = 0; j < LOSCFG_BASE_CORE_PROCESS_LIMIT; j++) {
        if (ipcTaskInfo->accessMap[j] == TRUE) {
            ipcTaskInfo->accessMap[j] = FALSE;
            (VOID)SendDeathMsg(j, taskCB->taskID);
        }
    }
#else
    (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);
    for (UINT32 i = 1; i < MAX_SERVICE_NUM; i++) {
        if ((g_serviceHandleMap[i].status != HANDLE_NOT_USED) && (g_serviceHandleMap[i].taskID == taskCB->taskID)) {
            g_serviceHandleMap[i].status = HANDLE_NOT_USED;
            g_serviceHandleMap[i].taskID = INVAILD_ID;
            break;
        }
    }
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
    /* run deathHandler */
    if (i < MAX_SERVICE_NUM) {
        for (j = 0; j < LOSCFG_BASE_CORE_PROCESS_LIMIT; j++) {
            if (ipcTaskInfo->accessMap[j] == TRUE) {
                (VOID)SendDeathMsg(j, i);
            }
        }
    }
#endif

    (VOID)LOS_MemFree(m_aucSysMem1, ipcTaskInfo);
    taskCB->ipcTaskInfo = NULL;
}

LITE_OS_SEC_TEXT STATIC UINT32 SetCms(UINTPTR maxMsgSize)
{
    if (maxMsgSize < sizeof(IpcMsg)) {
        return -EINVAL;
    }
    (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);
#if (USE_TASKID_AS_HANDLE == 1)
    if (g_cmsTask.status == HANDLE_NOT_USED) {
        g_cmsTask.status = HANDLE_REGISTED;
        g_cmsTask.taskID = LOS_CurTaskIDGet();
        g_cmsTask.maxMsgSize = maxMsgSize;
        (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
        return LOS_OK;
    }
#else
    if (g_serviceHandleMap[0].status == HANDLE_NOT_USED) {
        g_serviceHandleMap[0].status = HANDLE_REGISTED;
        g_serviceHandleMap[0].taskID = LOS_CurTaskIDGet();
        (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
        return LOS_OK;
    }
#endif
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
    return -EEXIST;
}

LITE_OS_SEC_TEXT STATIC BOOL IsCmsSet(VOID)
{
    BOOL ret;
    (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);
#if (USE_TASKID_AS_HANDLE == 1)
    ret = g_cmsTask.status == HANDLE_REGISTED;
#else
    ret = g_serviceHandleMap[0].status == HANDLE_REGISTED;
#endif
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
    return ret;
}

LITE_OS_SEC_TEXT STATIC BOOL IsCmsTask(UINT32 taskID)
{
    BOOL ret;
    (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);
#if (USE_TASKID_AS_HANDLE == 1)
    ret = IsCmsSet() ? (OS_TCB_FROM_TID(taskID)->processCB == OS_TCB_FROM_TID(g_cmsTask.taskID)->processCB) : FALSE;
#else
    ret = IsCmsSet() ? (OS_TCB_FROM_TID(taskID)->processCB ==
        OS_TCB_FROM_TID(g_serviceHandleMap[0].taskID)->processCB) : FALSE;
#endif
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
    return ret;
}

LITE_OS_SEC_TEXT STATIC BOOL IsTaskAlive(UINT32 taskID)
{
    LosTaskCB *tcb = NULL;
    if (OS_TID_CHECK_INVALID(taskID)) {
        return FALSE;
    }
    tcb = OS_TCB_FROM_TID(taskID);
    if (OsTaskIsUnused(tcb)) {
        return FALSE;
    }
    if (OsTaskIsInactive(tcb)) {
        return FALSE;
    }
    if (!OsTaskIsUserMode(tcb)) {
        return FALSE;
    }
    return TRUE;
}

LITE_OS_SEC_TEXT STATIC UINT32 HandleFd(const LosProcessCB *pcb, SpecialObj *obj, BOOL isRollback)
{
    int ret;
    if (isRollback == FALSE) {
        ret = CopyFdToProc(obj->content.fd, pcb->processID);
        if (ret < 0) {
            return ret;
        }
        obj->content.fd = ret;
    } else {
        ret = CloseProcFd(obj->content.fd, pcb->processID);
        if (ret < 0) {
            return ret;
        }
    }

    return LOS_OK;
}

LITE_OS_SEC_TEXT STATIC UINT32 HandlePtr(LosProcessCB *pcb, SpecialObj *obj, BOOL isRollback)
{
    VOID *buf = NULL;
    UINT32 ret;
    if ((obj->content.ptr.buff == NULL) || (obj->content.ptr.buffSz == 0)) {
        return -EINVAL;
    }
    if (isRollback == FALSE) {
        if (LOS_IsUserAddress((vaddr_t)(UINTPTR)(obj->content.ptr.buff)) == FALSE) {
            PRINT_ERR("Liteipc Bad ptr address\n");
            return -EINVAL;
        }
        buf = LiteIpcNodeAlloc(pcb, obj->content.ptr.buffSz);
        if (buf == NULL) {
            PRINT_ERR("Liteipc DealPtr alloc mem failed\n");
            return -EINVAL;
        }
        ret = copy_from_user(buf, obj->content.ptr.buff, obj->content.ptr.buffSz);
        if (ret != LOS_OK) {
            LiteIpcNodeFree(pcb, buf);
            return ret;
        }
        obj->content.ptr.buff = (VOID *)GetIpcUserAddr(pcb, (INTPTR)buf);
        EnableIpcNodeFreeByUser(pcb, (VOID *)buf);
    } else {
        buf = (VOID *)GetIpcKernelAddr(pcb, (INTPTR)obj->content.ptr.buff);
        if (IsIpcNode(pcb, buf) == TRUE) {
            (VOID)LiteIpcNodeFree(pcb, buf);
        }
    }
    return LOS_OK;
}

LITE_OS_SEC_TEXT STATIC UINT32 HandleSvc(UINT32 dstTid, SpecialObj *obj, BOOL isRollback)
{
    UINT32 taskID = 0;
    if (isRollback == FALSE) {
        if (obj->content.svc.handle == -1) {
            if (obj->content.svc.token != 1) {
                PRINT_ERR("Liteipc HandleSvc wrong svc token\n");
                return -EINVAL;
            }
            UINT32 selfTid = LOS_CurTaskIDGet();
            LosTaskCB *tcb = OS_TCB_FROM_TID(selfTid);
            if (tcb->ipcTaskInfo == NULL) {
                tcb->ipcTaskInfo = LiteIpcTaskInit();
            }
            uint32_t serviceHandle = 0;
            UINT32 ret = GenerateServiceHandle(selfTid, HANDLE_REGISTED, &serviceHandle);
            if (ret != LOS_OK) {
                PRINT_ERR("Liteipc GenerateServiceHandle failed.\n");
                return ret;
            }
            obj->content.svc.handle = serviceHandle;
            (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);
            AddServiceAccess(dstTid, serviceHandle);
            (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
        }
        if (IsTaskAlive(obj->content.svc.handle) == FALSE) {
            PRINT_ERR("Liteipc HandleSvc wrong svctid\n");
            return -EINVAL;
        }
        if (HasServiceAccess(obj->content.svc.handle) == FALSE) {
            PRINT_ERR("Liteipc %s, %d, svchandle:%d, tid:%d\n", __FUNCTION__, __LINE__, obj->content.svc.handle, LOS_CurTaskIDGet());
            return -EACCES;
        }
        LosTaskCB *taskCb = OS_TCB_FROM_TID(obj->content.svc.handle);
        if (taskCb->ipcTaskInfo == NULL) {
            taskCb->ipcTaskInfo = LiteIpcTaskInit();
        }
        if (GetTid(obj->content.svc.handle, &taskID) == 0) {
            AddServiceAccess(dstTid, obj->content.svc.handle);
        }
    }
    return LOS_OK;
}

LITE_OS_SEC_TEXT STATIC UINT32 HandleObj(UINT32 dstTid, SpecialObj *obj, BOOL isRollback)
{
    UINT32 ret;
    LosProcessCB *pcb = OS_PCB_FROM_TID(dstTid);
    switch (obj->type) {
        case OBJ_FD:
            ret = HandleFd(pcb, obj, isRollback);
            break;
        case OBJ_PTR:
            ret = HandlePtr(pcb, obj, isRollback);
            break;
        case OBJ_SVC:
            ret = HandleSvc(dstTid, (SpecialObj *)obj, isRollback);
            break;
        default:
            ret = -EINVAL;
            break;
    }
    return ret;
}

LITE_OS_SEC_TEXT STATIC UINT32 HandleSpecialObjects(UINT32 dstTid, IpcListNode *node, BOOL isRollback)
{
    UINT32 ret = LOS_OK;
    IpcMsg *msg = &(node->msg);
    INT32 i;
    SpecialObj *obj = NULL;
    UINT32 *offset = (UINT32 *)(UINTPTR)(msg->offsets);
    if (isRollback) {
        i = msg->spObjNum;
        goto EXIT;
    }
    for (i = 0; i < msg->spObjNum; i++) {
        if (offset[i] > msg->dataSz - sizeof(SpecialObj)) {
            ret = -EINVAL;
            goto EXIT;
        }
        if ((i > 0) && (offset[i] < offset[i - 1] + sizeof(SpecialObj))) {
            ret = -EINVAL;
            goto EXIT;
        }
        obj = (SpecialObj *)((UINTPTR)msg->data + offset[i]);
        if (obj == NULL) {
            ret = -EINVAL;
            goto EXIT;
        }
        ret = HandleObj(dstTid, obj, FALSE);
        if (ret != LOS_OK) {
            goto EXIT;
        }
    }
    return LOS_OK;
EXIT:
    for (i--; i >= 0; i--) {
        obj = (SpecialObj *)((UINTPTR)msg->data + offset[i]);
        (VOID)HandleObj(dstTid, obj, TRUE);
    }
    return ret;
}

LITE_OS_SEC_TEXT STATIC UINT32 CheckMsgSize(IpcMsg *msg)
{
    UINT64 totalSize;
    UINT32 i;
    UINT32 *offset = (UINT32 *)(UINTPTR)(msg->offsets);
    SpecialObj *obj = NULL;
    if (msg->target.handle != 0) {
        return LOS_OK;
    }
    /* msg send to cms, check the msg size */
    totalSize = (UINT64)sizeof(IpcMsg) + msg->dataSz + msg->spObjNum * sizeof(UINT32);
    for (i = 0; i < msg->spObjNum; i++) {
        if (offset[i] > msg->dataSz - sizeof(SpecialObj)) {
            return -EINVAL;
        }
        if ((i > 0) && (offset[i] < offset[i - 1] + sizeof(SpecialObj))) {
            return -EINVAL;
        }
        obj = (SpecialObj *)((UINTPTR)msg->data + offset[i]);
        if (obj == NULL) {
            return -EINVAL;
        }
        if (obj->type == OBJ_PTR) {
            totalSize += obj->content.ptr.buffSz;
        }
    }
    (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);
    if (totalSize > g_cmsTask.maxMsgSize) {
        (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
        return -EINVAL;
    }
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
    return LOS_OK;
}

LITE_OS_SEC_TEXT STATIC UINT32 CopyDataFromUser(IpcListNode *node, UINT32 bufSz, const IpcMsg *msg)
{
    UINT32 ret;
    ret = (UINT32)memcpy_s((VOID *)(&node->msg), bufSz - sizeof(LOS_DL_LIST), (const VOID *)msg, sizeof(IpcMsg));
    if (ret != LOS_OK) {
        PRINT_DEBUG("%s, %d, %u\n", __FUNCTION__, __LINE__, ret);
        return ret;
    }

    if (msg->dataSz) {
        node->msg.data = (VOID *)((UINTPTR)node + sizeof(IpcListNode));
        ret = copy_from_user((VOID *)(node->msg.data), msg->data, msg->dataSz);
        if (ret != LOS_OK) {
            PRINT_DEBUG("%s, %d\n", __FUNCTION__, __LINE__);
            return ret;
        }
    } else {
        node->msg.data = NULL;
    }

    if (msg->spObjNum) {
        node->msg.offsets = (VOID *)((UINTPTR)node + sizeof(IpcListNode) + msg->dataSz);
        ret = copy_from_user((VOID *)(node->msg.offsets), msg->offsets, msg->spObjNum * sizeof(UINT32));
        if (ret != LOS_OK) {
            PRINT_DEBUG("%s, %d, %x, %x, %d\n", __FUNCTION__, __LINE__, node->msg.offsets, msg->offsets, msg->spObjNum);
            return ret;
        }
    } else {
        node->msg.offsets = NULL;
    }
    ret = CheckMsgSize(&node->msg);
    if (ret != LOS_OK) {
        PRINT_DEBUG("%s, %d\n", __FUNCTION__, __LINE__);
        return ret;
    }
    node->msg.taskID = LOS_CurTaskIDGet();
    node->msg.processID = OsCurrProcessGet()->processID;
#ifdef LOSCFG_SECURITY_CAPABILITY
    node->msg.userID = OsCurrProcessGet()->user->userID;
    node->msg.gid = OsCurrProcessGet()->user->gid;
#endif
    return LOS_OK;
}

LITE_OS_SEC_TEXT STATIC BOOL IsValidReply(const IpcContent *content)
{
    LosProcessCB *curr = OsCurrProcessGet();
    IpcListNode *node = (IpcListNode *)GetIpcKernelAddr(curr, (INTPTR)(content->buffToFree));
    IpcMsg *requestMsg = &node->msg;
    IpcMsg *replyMsg = content->outMsg;
    UINT32 reqDstTid = 0;
    /* Check whether the reply matches the request */
    if ((requestMsg->type != MT_REQUEST)  ||
        (requestMsg->flag == LITEIPC_FLAG_ONEWAY) ||
        (replyMsg->timestamp != requestMsg->timestamp) ||
        (replyMsg->target.handle != requestMsg->taskID) ||
        (GetTid(requestMsg->target.handle, &reqDstTid) != 0) ||
        (OS_TCB_FROM_TID(reqDstTid)->processCB != (UINTPTR)curr)) {
        return FALSE;
    }
    return TRUE;
}

LITE_OS_SEC_TEXT STATIC UINT32 CheckPara(IpcContent *content, UINT32 *dstTid)
{
    UINT32 ret;
    IpcMsg *msg = content->outMsg;
    UINT32 flag = content->flag;
#if (USE_TIMESTAMP == 1)
    UINT64 now = LOS_CurrNanosec();
#endif
    if (((msg->dataSz > 0) && (msg->data == NULL)) ||
        ((msg->spObjNum > 0) && (msg->offsets == NULL)) ||
        (msg->dataSz > IPC_MSG_DATA_SZ_MAX) ||
        (msg->spObjNum > IPC_MSG_OBJECT_NUM_MAX) ||
        (msg->dataSz < msg->spObjNum * sizeof(SpecialObj))) {
        return -EINVAL;
    }
    switch (msg->type) {
        case MT_REQUEST:
            if (HasServiceAccess(msg->target.handle)) {
                ret = GetTid(msg->target.handle, dstTid);
                if (ret != LOS_OK) {
                    return -EINVAL;
                }
            } else {
                PRINT_ERR("Liteipc %s, %d\n", __FUNCTION__, __LINE__);
                return -EACCES;
            }
#if (USE_TIMESTAMP == 1)
            msg->timestamp = now;
#endif
            break;
        case MT_REPLY:
        case MT_FAILED_REPLY:
            if ((flag & BUFF_FREE) != BUFF_FREE) {
                return -EINVAL;
            }
            if (!IsValidReply(content)) {
                return -EINVAL;
            }
#if (USE_TIMESTAMP == 1)
            if (now > msg->timestamp + LITEIPC_TIMEOUT_NS) {
#ifdef LOSCFG_KERNEL_HOOK
                ret = GetTid(msg->target.handle, dstTid);
                if (ret != LOS_OK) {
                    *dstTid = INVAILD_ID;
                }
#endif
                OsHookCall(LOS_HOOK_TYPE_IPC_WRITE_DROP, msg, *dstTid,
                           (*dstTid == INVAILD_ID) ? INVAILD_ID : OS_PCB_FROM_TID(*dstTid)->processID, 0);
                PRINT_ERR("Liteipc A timeout reply, request timestamp:%lld, now:%lld\n", msg->timestamp, now);
                return -ETIME;
            }
#endif
            *dstTid = msg->target.handle;
            break;
        case MT_DEATH_NOTIFY:
            *dstTid = msg->target.handle;
#if (USE_TIMESTAMP == 1)
            msg->timestamp = now;
#endif
            break;
        default:
            PRINT_DEBUG("Unknown msg type:%d\n", msg->type);
            return -EINVAL;
    }

    if (IsTaskAlive(*dstTid) == FALSE) {
        return -EINVAL;
    }
    return LOS_OK;
}

LITE_OS_SEC_TEXT STATIC UINT32 LiteIpcWrite(IpcContent *content)
{
    UINT32 ret, intSave;
    UINT32 dstTid;

    IpcMsg *msg = content->outMsg;

    ret = CheckPara(content, &dstTid);
    if (ret != LOS_OK) {
        return ret;
    }

    LosTaskCB *tcb = OS_TCB_FROM_TID(dstTid);
    LosProcessCB *pcb = OS_PCB_FROM_TCB(tcb);
    if (pcb->ipcInfo == NULL) {
        PRINT_ERR("pid %u Liteipc not create\n", pcb->processID);
        return -EINVAL;
    }

    UINT32 bufSz = sizeof(IpcListNode) + msg->dataSz + msg->spObjNum * sizeof(UINT32);
    IpcListNode *buf = (IpcListNode *)LiteIpcNodeAlloc(pcb, bufSz);
    if (buf == NULL) {
        PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
        return -ENOMEM;
    }
    ret = CopyDataFromUser(buf, bufSz, (const IpcMsg *)msg);
    if (ret != LOS_OK) {
        PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
        goto ERROR_COPY;
    }

    if (tcb->ipcTaskInfo == NULL) {
        tcb->ipcTaskInfo = LiteIpcTaskInit();
    }

    ret = HandleSpecialObjects(dstTid, buf, FALSE);
    if (ret != LOS_OK) {
        PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
        goto ERROR_COPY;
    }
    /* add data to list and wake up dest task */
    SCHEDULER_LOCK(intSave);
    LOS_ListTailInsert(&(tcb->ipcTaskInfo->msgListHead), &(buf->listNode));
    OsHookCall(LOS_HOOK_TYPE_IPC_WRITE, &buf->msg, dstTid, pcb->processID, tcb->waitFlag);
    if (tcb->waitFlag == OS_TASK_WAIT_LITEIPC) {
        OsTaskWakeClearPendMask(tcb);
        tcb->ops->wake(tcb);
        SCHEDULER_UNLOCK(intSave);
        LOS_MpSchedule(OS_MP_CPU_ALL);
        LOS_Schedule();
    } else {
        SCHEDULER_UNLOCK(intSave);
    }
    return LOS_OK;
ERROR_COPY:
    LiteIpcNodeFree(pcb, buf);
    return ret;
}

LITE_OS_SEC_TEXT STATIC UINT32 CheckReceivedMsg(IpcListNode *node, IpcContent *content, LosTaskCB *tcb)
{
    UINT32 ret = LOS_OK;
    if (node == NULL) {
        return -EINVAL;
    }
    switch (node->msg.type) {
        case MT_REQUEST:
            if ((content->flag & SEND) == SEND) {
                PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
                ret = -EINVAL;
            }
            break;
        case MT_FAILED_REPLY:
            ret = -ENOENT;
            /* fall-through */
        case MT_REPLY:
            if ((content->flag & SEND) != SEND) {
                PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
                ret = -EINVAL;
            }
#if (USE_TIMESTAMP == 1)
            if (node->msg.timestamp != content->outMsg->timestamp) {
                PRINT_ERR("Receive a unmatch reply, drop it\n");
                ret = -EINVAL;
            }
#else
            if ((node->msg.code != content->outMsg->code) ||
                (node->msg.target.token != content->outMsg->target.token)) {
                PRINT_ERR("Receive a unmatch reply, drop it\n");
                ret = -EINVAL;
            }
#endif
            break;
        case MT_DEATH_NOTIFY:
            break;
        default:
            PRINT_ERR("Unknown msg type:%d\n", node->msg.type);
            ret =  -EINVAL;
    }
    if (ret != LOS_OK) {
        OsHookCall(LOS_HOOK_TYPE_IPC_READ_DROP, &node->msg, tcb->waitFlag);
        (VOID)HandleSpecialObjects(LOS_CurTaskIDGet(), node, TRUE);
        (VOID)LiteIpcNodeFree(OsCurrProcessGet(), (VOID *)node);
    } else {
        OsHookCall(LOS_HOOK_TYPE_IPC_READ, &node->msg, tcb->waitFlag);
    }
    return ret;
}

LITE_OS_SEC_TEXT STATIC UINT32 LiteIpcRead(IpcContent *content)
{
    UINT32 intSave, ret;
    UINT32 selfTid = LOS_CurTaskIDGet();
    LOS_DL_LIST *listHead = NULL;
    LOS_DL_LIST *listNode = NULL;
    IpcListNode *node = NULL;
    UINT32 syncFlag = (content->flag & SEND) && (content->flag & RECV);
    UINT32 timeout = syncFlag ? LOS_MS2Tick(LITEIPC_TIMEOUT_MS) : LOS_WAIT_FOREVER;

    LosTaskCB *tcb = OS_TCB_FROM_TID(selfTid);
    if (tcb->ipcTaskInfo == NULL) {
        tcb->ipcTaskInfo = LiteIpcTaskInit();
    }

    listHead = &(tcb->ipcTaskInfo->msgListHead);
    do {
        SCHEDULER_LOCK(intSave);
        if (LOS_ListEmpty(listHead)) {
            OsTaskWaitSetPendMask(OS_TASK_WAIT_LITEIPC, OS_INVALID_VALUE, timeout);
            OsHookCall(LOS_HOOK_TYPE_IPC_TRY_READ, syncFlag ? MT_REPLY : MT_REQUEST, tcb->waitFlag);
            ret = tcb->ops->wait(tcb, &g_ipcPendlist, timeout);
            if (ret == LOS_ERRNO_TSK_TIMEOUT) {
                OsHookCall(LOS_HOOK_TYPE_IPC_READ_TIMEOUT, syncFlag ? MT_REPLY : MT_REQUEST, tcb->waitFlag);
                SCHEDULER_UNLOCK(intSave);
                return -ETIME;
            }

            if (OsTaskIsKilled(tcb)) {
                OsHookCall(LOS_HOOK_TYPE_IPC_KILL, syncFlag ? MT_REPLY : MT_REQUEST, tcb->waitFlag);
                SCHEDULER_UNLOCK(intSave);
                return -ERFKILL;
            }

            SCHEDULER_UNLOCK(intSave);
        } else {
            listNode = LOS_DL_LIST_FIRST(listHead);
            LOS_ListDelete(listNode);
            node = LOS_DL_LIST_ENTRY(listNode, IpcListNode, listNode);
            SCHEDULER_UNLOCK(intSave);
            ret = CheckReceivedMsg(node, content, tcb);
            if (ret == LOS_OK) {
                break;
            }
            if (ret == -ENOENT) { /* It means that we've received a failed reply */
                return ret;
            }
        }
    } while (1);
    node->msg.data = (VOID *)GetIpcUserAddr(OsCurrProcessGet(), (INTPTR)(node->msg.data));
    node->msg.offsets = (VOID *)GetIpcUserAddr(OsCurrProcessGet(), (INTPTR)(node->msg.offsets));
    content->inMsg = (VOID *)GetIpcUserAddr(OsCurrProcessGet(), (INTPTR)(&(node->msg)));
    EnableIpcNodeFreeByUser(OsCurrProcessGet(), (VOID *)node);
    return LOS_OK;
}

LITE_OS_SEC_TEXT STATIC UINT32 LiteIpcMsgHandle(IpcContent *con)
{
    UINT32 ret = LOS_OK;
    IpcContent localContent;
    IpcContent *content = &localContent;
    IpcMsg localMsg;
    IpcMsg *msg = &localMsg;
    IpcListNode *nodeNeedFree = NULL;

    if (copy_from_user((void *)content, (const void *)con, sizeof(IpcContent)) != LOS_OK) {
        PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
        return -EINVAL;
    }

    if ((content->flag & BUFF_FREE) == BUFF_FREE) {
        ret = CheckUsedBuffer(content->buffToFree, &nodeNeedFree);
        if (ret != LOS_OK) {
            PRINT_ERR("CheckUsedBuffer failed:%d\n", ret);
            return ret;
        }
    }

    if ((content->flag & SEND) == SEND) {
        if (content->outMsg == NULL) {
            PRINT_ERR("content->outmsg is null\n");
            ret = -EINVAL;
            goto BUFFER_FREE;
        }
        if (copy_from_user((void *)msg, (const void *)content->outMsg, sizeof(IpcMsg)) != LOS_OK) {
            PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
            ret = -EINVAL;
            goto BUFFER_FREE;
        }
        content->outMsg = msg;
        if ((content->outMsg->type < 0) || (content->outMsg->type >= MT_DEATH_NOTIFY)) {
            PRINT_ERR("LiteIpc unknown msg type:%d\n", content->outMsg->type);
            ret = -EINVAL;
            goto BUFFER_FREE;
        }
        ret = LiteIpcWrite(content);
        if (ret != LOS_OK) {
            PRINT_ERR("LiteIpcWrite failed\n");
            goto BUFFER_FREE;
        }
    }
BUFFER_FREE:
    if (nodeNeedFree != NULL) {
        UINT32 freeRet = LiteIpcNodeFree(OsCurrProcessGet(), nodeNeedFree);
        ret = (freeRet == LOS_OK) ? ret : freeRet;
    }
    if (ret != LOS_OK) {
        return ret;
    }

    if ((content->flag & RECV) == RECV) {
        ret = LiteIpcRead(content);
        if (ret != LOS_OK) {
            PRINT_ERR("LiteIpcRead failed ERROR: %d\n", (INT32)ret);
            return ret;
        }
        UINT32 offset = LOS_OFF_SET_OF(IpcContent, inMsg);
        ret = copy_to_user((char*)con + offset, (char*)content + offset, sizeof(IpcMsg *));
        if (ret != LOS_OK) {
            PRINT_ERR("%s, %d, %d\n", __FUNCTION__, __LINE__, ret);
            return -EINVAL;
        }
    }
    return ret;
}

LITE_OS_SEC_TEXT STATIC UINT32 HandleCmsCmd(CmsCmdContent *content)
{
    UINT32 ret = LOS_OK;
    CmsCmdContent localContent;
    if (content == NULL) {
        return -EINVAL;
    }
    if (IsCmsTask(LOS_CurTaskIDGet()) == FALSE) {
        return -EACCES;
    }
    if (copy_from_user((void *)(&localContent), (const void *)content, sizeof(CmsCmdContent)) != LOS_OK) {
        PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
        return -EINVAL;
    }
    switch (localContent.cmd) {
        case CMS_GEN_HANDLE:
            if ((localContent.taskID != 0) && (IsTaskAlive(localContent.taskID) == FALSE)) {
                return -EINVAL;
            }
            ret = GenerateServiceHandle(localContent.taskID, HANDLE_REGISTED, &(localContent.serviceHandle));
            if (ret == LOS_OK) {
                ret = copy_to_user((void *)content, (const void *)(&localContent), sizeof(CmsCmdContent));
            }
            (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);
            AddServiceAccess(g_cmsTask.taskID, localContent.serviceHandle);
            (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
            break;
        case CMS_REMOVE_HANDLE:
            if (localContent.serviceHandle >= MAX_SERVICE_NUM) {
                return -EINVAL;
            }
            RefreshServiceHandle(localContent.serviceHandle, -1);
            break;
        case CMS_ADD_ACCESS:
            if (IsTaskAlive(localContent.taskID) == FALSE) {
                return -EINVAL;
            }
            return AddServiceAccess(localContent.taskID, localContent.serviceHandle);
        default:
            PRINT_DEBUG("Unknown cmd cmd:%d\n", localContent.cmd);
            return -EINVAL;
    }
    return ret;
}

LITE_OS_SEC_TEXT STATIC UINT32 HandleGetVersion(IpcVersion *version)
{
    UINT32 ret = LOS_OK;
    IpcVersion localIpcVersion;
    if (version == NULL) {
        return -EINVAL;
    }

    localIpcVersion.driverVersion = DRIVER_VERSION;
    ret = copy_to_user((void *)version, (const void *)(&localIpcVersion), sizeof(IpcVersion));
    if (ret != LOS_OK) {
        PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
    }
    return ret; 
}

LITE_OS_SEC_TEXT int LiteIpcIoctl(struct file *filep, int cmd, unsigned long arg)
{
    UINT32 ret = LOS_OK;
    LosProcessCB *pcb = OsCurrProcessGet();
    ProcIpcInfo *ipcInfo = pcb->ipcInfo;

    if (ipcInfo == NULL) {
        PRINT_ERR("Liteipc pool not create!\n");
        return -EINVAL;
    }

    if (IsPoolMapped(ipcInfo) == FALSE) {
        PRINT_ERR("Liteipc Ipc pool not init, need to mmap first!\n");
        return -ENOMEM;
    }

    switch (cmd) {
        case IPC_SET_CMS:
            return (INT32)SetCms(arg);
        case IPC_CMS_CMD:
            return (INT32)HandleCmsCmd((CmsCmdContent *)(UINTPTR)arg);
        case IPC_GET_VERSION:
            return (INT32)HandleGetVersion((IpcVersion *)(UINTPTR)arg);
        case IPC_SET_IPC_THREAD:
            if (IsCmsSet() == FALSE) {
                PRINT_ERR("Liteipc ServiceManager not set!\n");
                return -EINVAL;
            }
            return (INT32)SetIpcTask();
        case IPC_SEND_RECV_MSG:
            if (arg == 0) {
                return -EINVAL;
            }
            if (IsCmsSet() == FALSE) {
                PRINT_ERR("Liteipc ServiceManager not set!\n");
                return -EINVAL;
            }
            ret = LiteIpcMsgHandle((IpcContent *)(UINTPTR)arg);
            if (ret != LOS_OK) {
                return (INT32)ret;
            }
            break;
        default:
            PRINT_ERR("Unknown liteipc ioctl cmd:%d\n", cmd);
            return -EINVAL;
    }
    return (INT32)ret;
}
