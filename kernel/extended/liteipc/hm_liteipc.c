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

#define USE_TASKID_AS_HANDLE 1 	///< 将任务ID当服务ID使用
#define USE_MMAP 1				///< 使用映射( 用户空间 <--> 物理地址 <--> 内核空间 ) ==> 用户空间 <-映射-> 内核空间 
#define IPC_IO_DATA_MAX 8192UL	///< 最大的消息内容 8K ,posix最大消息内容 64个字节
#define IPC_MSG_DATA_SZ_MAX (IPC_IO_DATA_MAX * sizeof(SpecialObj) / (sizeof(SpecialObj) + sizeof(size_t))) ///< 消息内容上限
#define IPC_MSG_OBJECT_NUM_MAX (IPC_MSG_DATA_SZ_MAX / sizeof(SpecialObj)) ///< 消息条数上限

#define LITE_IPC_POOL_NAME "liteipc"	///< ipc池名称
#define LITE_IPC_POOL_PAGE_MAX_NUM 64 	/* 256KB | IPC池最大物理页数 */
#define LITE_IPC_POOL_PAGE_DEFAULT_NUM 16 /* 64KB | IPC池默认物理页数  */
#define LITE_IPC_POOL_MAX_SIZE (LITE_IPC_POOL_PAGE_MAX_NUM << PAGE_SHIFT)	///< 最大IPC池 256K
#define LITE_IPC_POOL_DEFAULT_SIZE (LITE_IPC_POOL_PAGE_DEFAULT_NUM << PAGE_SHIFT)///< 默认IPC池 64K
#define LITE_IPC_POOL_UVADDR 0x10000000 ///< IPC默认在用户空间地址
#define INVAILD_ID (-1)

#define LITEIPC_TIMEOUT_MS 5000UL			///< 超时时间单位毫秒
#define LITEIPC_TIMEOUT_NS 5000000000ULL	///< 超时时间单位纳秒
/// 注意 ipc节点和消息是两个概念, 一个消息会包含多个节点
typedef struct {//已使用节点链表
    LOS_DL_LIST list;	///< 通过它挂到对应 ProcIpcInfo.ipUsedNodeList 上
    VOID *ptr;			///< 指向ipc节点内容
} IpcUsedNode;
//ServiceManager有两个主要功能：一是负责Service的注册和注销，
//二是负责管理Service的访问权限（只有有权限的任务（Task）可以向对应的Service发送IPC消息）。

STATIC LosMux g_serviceHandleMapMux; 
#if (USE_TASKID_AS_HANDLE == 1) // @note_why 前缀cms是何意思? 请官方解释下!!! 个人就是指 ServiceManager  :(
STATIC HandleInfo g_cmsTask;	///< 整个系统只能有一个ServiceManager
#else
STATIC HandleInfo g_serviceHandleMap[MAX_SERVICE_NUM]; ///< g_serviceHandleMap[0]为ServiceManager,这块鸿蒙的设计挺怪的,让人很懵逼
#endif
STATIC LOS_DL_LIST g_ipcPendlist;	///< 挂起/待办链表,上面挂等待读/写消息的任务LosTaskCB

/* ipc lock */
SPIN_LOCK_INIT(g_ipcSpin);//初始化IPC自旋锁
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
    .open = LiteIpcOpen,   /* open | 创建Ipc内存池*/
    .close = LiteIpcClose,  /* close */
    .ioctl = LiteIpcIoctl,  /* ioctl | 包含读写操作 */
    .mmap = LiteIpcMmap,   /* mmap | 实现线性区映射*/
};

/*!
 * @brief OsLiteIpcInit	初始化LiteIPC模块
 *
 * @return	
 *
 * @see
 */
LITE_OS_SEC_TEXT_INIT UINT32 OsLiteIpcInit(VOID)
{
    UINT32 ret;
#if (USE_TASKID_AS_HANDLE == 1) //两种管理方式,一种是 任务ID == service ID 
    g_cmsTask.status = HANDLE_NOT_USED;//默认未使用
#else
    memset_s(g_serviceHandleMap, sizeof(g_serviceHandleMap), 0, sizeof(g_serviceHandleMap));//默认未使用
#endif
    ret = LOS_MuxInit(&g_serviceHandleMapMux, NULL);
    if (ret != LOS_OK) {
        return ret;
    }
    ret = (UINT32)register_driver(LITEIPC_DRIVER, &g_liteIpcFops, LITEIPC_DRIVER_MODE, NULL);//注册字符驱动程序,从此支持文件访问LiteIpc
    if (ret != LOS_OK) {
        PRINT_ERR("register lite_ipc driver failed:%d\n", ret);
    }
    LOS_ListInit(&(g_ipcPendlist));

    return ret;
}

LOS_MODULE_INIT(OsLiteIpcInit, LOS_INIT_LEVEL_KMOD_EXTENDED);//内核IPC模块的初始化

/*!
 * @brief LiteIpcOpen
 * 以VFS方式为当前进程创建IPC消息池 
 * @param filep	
 * @return	
 *
 * @see
 */
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
/// 池是否已经映射
LITE_OS_SEC_TEXT STATIC BOOL IsPoolMapped(ProcIpcInfo *ipcInfo)
{
    return (ipcInfo->pool.uvaddr != NULL) && (ipcInfo->pool.kvaddr != NULL) &&
        (ipcInfo->pool.poolSize != 0);
}

/*!
 * @brief DoIpcMmap	
 * 做IPC层映射,将内核空间虚拟地址映射到用户空间,这样的好处是用户态下操作读写的背后是在读写内核态空间
 * 如此用户地址和内核地址指向同一个物理地址
 * @param pcb	
 * @param region	
 * @return	
 *
 * @see
 */
LITE_OS_SEC_TEXT STATIC INT32 DoIpcMmap(LosProcessCB *pcb, LosVmMapRegion *region)
{
    UINT32 i;
    INT32 ret = 0;
    PADDR_T pa;
    UINT32 uflags = VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_USER;
    LosVmPage *vmPage = NULL;
    VADDR_T uva = (VADDR_T)(UINTPTR)pcb->ipcInfo->pool.uvaddr;//用户空间地址
    VADDR_T kva = (VADDR_T)(UINTPTR)pcb->ipcInfo->pool.kvaddr;//内核空间地址

    (VOID)LOS_MuxAcquire(&pcb->vmSpace->regionMux);

    for (i = 0; i < (region->range.size >> PAGE_SHIFT); i++) {//获取线性区页数,一页一页映射
        pa = LOS_PaddrQuery((VOID *)(UINTPTR)(kva + (i << PAGE_SHIFT)));//通过内核空间查找物理地址
        if (pa == 0) {
            PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
            ret = -EINVAL;
            break;
        }
        vmPage = LOS_VmPageGet(pa);//获取物理页框
        if (vmPage == NULL) {//目的是检查物理页是否存在
            PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
            ret = -EINVAL;
            break;
        }
        STATUS_T err = LOS_ArchMmuMap(&pcb->vmSpace->archMmu, uva + (i << PAGE_SHIFT), pa, 1, uflags);//将物理页映射到用户空间
        if (err < 0) {
            ret = err;
            PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
            break;
        }
    }
    /* if any failure happened, rollback | 如果中间发生映射失败,则回滚*/
    if (i != (region->range.size >> PAGE_SHIFT)) {
        while (i--) {
            pa = LOS_PaddrQuery((VOID *)(UINTPTR)(kva + (i << PAGE_SHIFT)));//查询物理地址
            vmPage = LOS_VmPageGet(pa);//获取物理页框
            (VOID)LOS_ArchMmuUnmap(&pcb->vmSpace->archMmu, uva + (i << PAGE_SHIFT), 1);//取消与用户空间的映射
            LOS_PhysPageFree(vmPage);//释放物理页
        }
    }

    (VOID)LOS_MuxRelease(&pcb->vmSpace->regionMux);
    return ret;
}
///将参数线性区设为IPC专用区
LITE_OS_SEC_TEXT STATIC int LiteIpcMmap(struct file *filep, LosVmMapRegion *region)
{
    int ret = 0;
    LosVmMapRegion *regionTemp = NULL;
    LosProcessCB *pcb = OsCurrProcessGet();
    ProcIpcInfo *ipcInfo = pcb->ipcInfo;
	//被映射的线性区不能在常量和私有数据区
    if ((ipcInfo == NULL) || (region == NULL) || (region->range.size > LITE_IPC_POOL_MAX_SIZE) ||
        (!LOS_IsRegionPermUserReadOnly(region)) || (!LOS_IsRegionFlagPrivateOnly(region))) {
        ret = -EINVAL;
        goto ERROR_REGION_OUT;
    }
    if (IsPoolMapped(ipcInfo)) {//已经用户空间和内核空间之间存在映射关系了
        return -EEXIST;
    }
    if (ipcInfo->pool.uvaddr != NULL) {//ipc池已在进程空间有地址
        regionTemp = LOS_RegionFind(pcb->vmSpace, (VADDR_T)(UINTPTR)ipcInfo->pool.uvaddr);//在指定进程空间中找到所在线性区
        if (regionTemp != NULL) {
            (VOID)LOS_RegionFree(pcb->vmSpace, regionTemp);//先释放线性区
        }
		// 建议加上 ipcInfo->pool.uvaddr = NULL; 同下
    }
    ipcInfo->pool.uvaddr = (VOID *)(UINTPTR)region->range.base;//将指定的线性区和ipc池虚拟地址绑定
    if (ipcInfo->pool.kvaddr != NULL) {//如果存在内核空间地址
        LOS_VFree(ipcInfo->pool.kvaddr);//因为要重新映射,所以必须先释放掉物理内存
        ipcInfo->pool.kvaddr = NULL; //从效果上看, 这句话可以不加,但加上看着更舒服,    uvaddr 和 kvaddr 一对新人迎接美好未来
    }
    /* use vmalloc to alloc phy mem */
    ipcInfo->pool.kvaddr = LOS_VMalloc(region->range.size);//从内核动态空间中申请线性区,分配同等量的物理内存,做好 内核 <-->物理内存的映射
    if (ipcInfo->pool.kvaddr == NULL) {//申请物理内存失败, 肯定是玩不下去了.
        ret = -ENOMEM; //返回没有内存了
        goto ERROR_REGION_OUT;
    }
    /* do mmap */
    ret = DoIpcMmap(pcb, region);//对uvaddr和kvaddr做好映射关系,如此用户态下通过操作uvaddr达到操作kvaddr的目的
    if (ret) {
        goto ERROR_MAP_OUT;
    }
    /* ipc pool init */
    if (LOS_MemInit(ipcInfo->pool.kvaddr, region->range.size) != LOS_OK) {//初始化ipc池
        ret = -EINVAL;
        goto ERROR_MAP_OUT;
    }
    ipcInfo->pool.poolSize = region->range.size;//ipc池大小为线性区大小
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
///初始化进程的IPC内存池
LITE_OS_SEC_TEXT_INIT STATIC UINT32 LiteIpcPoolInit(ProcIpcInfo *ipcInfo)
{
    ipcInfo->pool.uvaddr = NULL;
    ipcInfo->pool.kvaddr = NULL;
    ipcInfo->pool.poolSize = 0;
    ipcInfo->ipcTaskID = INVAILD_ID;
    LOS_ListInit(&ipcInfo->ipcUsedNodelist);//上面将挂已使用的节点
    return LOS_OK;
}
///创建IPC消息内存池
LITE_OS_SEC_TEXT_INIT STATIC ProcIpcInfo *LiteIpcPoolCreate(VOID)
{
    ProcIpcInfo *ipcInfo = LOS_MemAlloc(m_aucSysMem1, sizeof(ProcIpcInfo));//从内核堆内存中申请IPC控制体
    if (ipcInfo == NULL) {
        return NULL;
    }

    (VOID)memset_s(ipcInfo, sizeof(ProcIpcInfo), 0, sizeof(ProcIpcInfo));

    (VOID)LiteIpcPoolInit(ipcInfo);
    return ipcInfo;
}

/*!
 * @brief LiteIpcPoolReInit	重新初始化进程的IPC消息内存池
 *
 * @param parent	
 * @return	
 *
 * @see
 */
LITE_OS_SEC_TEXT ProcIpcInfo *LiteIpcPoolReInit(const ProcIpcInfo *parent)
{
    ProcIpcInfo *ipcInfo = LiteIpcPoolCreate();
    if (ipcInfo == NULL) {
        return NULL;
    }

    ipcInfo->pool.uvaddr = parent->pool.uvaddr;//用户空间地址继续沿用
    ipcInfo->pool.kvaddr = NULL;
    ipcInfo->pool.poolSize = 0;
    ipcInfo->ipcTaskID = INVAILD_ID;
    return ipcInfo;
}
/// 释放进程的IPC消息内存池
STATIC VOID LiteIpcPoolDelete(ProcIpcInfo *ipcInfo, UINT32 processID)
{
    UINT32 intSave;
    IpcUsedNode *node = NULL;
    if (ipcInfo->pool.kvaddr != NULL) {//如果占用内核空间线性区
        LOS_VFree(ipcInfo->pool.kvaddr);//释放线性区
        ipcInfo->pool.kvaddr = NULL;
        IPC_LOCK(intSave);
        while (!LOS_ListEmpty(&ipcInfo->ipcUsedNodelist)) {//对进程的IPC已被读取的链表遍历
            node = LOS_DL_LIST_ENTRY(ipcInfo->ipcUsedNodelist.pstNext, IpcUsedNode, list);//一个个读取
            LOS_ListDelete(&node->list);//将自己从链表上摘出去
            free(node);//释放向内核堆内存申请的 sizeof(IpcUsedNode) 空间
        }
        IPC_UNLOCK(intSave);
    }
    /* remove process access to service | 删除进程对服务的访问,这里的服务指的就是任务*/
    for (UINT32 i = 0; i < MAX_SERVICE_NUM; i++) {//双方断交
        if (ipcInfo->access[i] == TRUE) {//允许访问
            ipcInfo->access[i] = FALSE; //设为不允许访问
            if (OS_TCB_FROM_TID(i)->ipcTaskInfo != NULL) {//任务有IPC时
                OS_TCB_FROM_TID(i)->ipcTaskInfo->accessMap[processID] = FALSE;//同样设置任务也不允许访问进程
            }
        }
    }
}
/// 销毁指定进程的IPC池
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
/// 申请并初始化一个任务IPC
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

/* Only when kernel no longer access ipc node content, can user free the ipc node 
| 使能一个空闲的IPC节点 */
LITE_OS_SEC_TEXT STATIC VOID EnableIpcNodeFreeByUser(UINT32 processID, VOID *buf)
{
    UINT32 intSave;
    ProcIpcInfo *ipcInfo = OS_PCB_FROM_PID(processID)->ipcInfo;
    IpcUsedNode *node = (IpcUsedNode *)malloc(sizeof(IpcUsedNode));//申请一个可使用的节点,这里是向内核堆空间申请内存
    if (node != NULL) {
        node->ptr = buf;//指向参数缓存
        IPC_LOCK(intSave);
        LOS_ListAdd(&ipcInfo->ipcUsedNodelist, &node->list);//将OBJ_PTR类型的消息 挂到进程已使用链表上
        IPC_UNLOCK(intSave);
    }
}
/// 注意这可是从内核空间的IPC池中申请内存
LITE_OS_SEC_TEXT STATIC VOID *LiteIpcNodeAlloc(UINT32 processID, UINT32 size)
{
    VOID *ptr = LOS_MemAlloc(OS_PCB_FROM_PID(processID)->ipcInfo->pool.kvaddr, size);//kvaddr实际指向了内核空间
    PRINT_INFO("LiteIpcNodeAlloc pid:%d, pool:%x buf:%x size:%d\n",
               processID, OS_PCB_FROM_PID(processID)->ipcInfo->pool.kvaddr, ptr, size);
    return ptr;
}
/// 从IPC内存池中释放一个IPC节点
LITE_OS_SEC_TEXT STATIC UINT32 LiteIpcNodeFree(UINT32 processID, VOID *buf)
{
    PRINT_INFO("LiteIpcNodeFree pid:%d, pool:%x buf:%x\n",
               processID, OS_PCB_FROM_PID(processID)->ipcInfo->pool.kvaddr, buf);
    return LOS_MemFree(OS_PCB_FROM_PID(processID)->ipcInfo->pool.kvaddr, buf);
}
///指定buf 是否是IPC节点,这个函数不应该命名为 Is...容易误导 ,应改为 Free... @note_thinking
LITE_OS_SEC_TEXT STATIC BOOL IsIpcNode(UINT32 processID, const VOID *buf)
{
    IpcUsedNode *node = NULL;
    UINT32 intSave;
    ProcIpcInfo *ipcInfo = OS_PCB_FROM_PID(processID)->ipcInfo;//获取进程的IPC信息
    IPC_LOCK(intSave);
    LOS_DL_LIST_FOR_EACH_ENTRY(node, &ipcInfo->ipcUsedNodelist, IpcUsedNode, list) {//遍历已使用IPC节点
        if (node->ptr == buf) {//如果一致
            LOS_ListDelete(&node->list);//从已使用链表上摘除
            IPC_UNLOCK(intSave);
            free(node);//释放内核堆内存
            return TRUE;
        }
    }
    IPC_UNLOCK(intSave);
    return FALSE;
}
/// 获得IPC用户空间地址
LITE_OS_SEC_TEXT STATIC INTPTR GetIpcUserAddr(UINT32 processID, INTPTR kernelAddr)
{
    IpcPool pool = OS_PCB_FROM_PID(processID)->ipcInfo->pool;
    INTPTR offset = (INTPTR)(pool.uvaddr) - (INTPTR)(pool.kvaddr);//先计算偏移量,注意这里应该是个负数,因为内核空间在高地址位
    return kernelAddr + offset; //再获取用户空间地址
}
/// 获得IPC内核空间地址
LITE_OS_SEC_TEXT STATIC INTPTR GetIpcKernelAddr(UINT32 processID, INTPTR userAddr)
{
    IpcPool pool = OS_PCB_FROM_PID(processID)->ipcInfo->pool;
    INTPTR offset = (INTPTR)(pool.uvaddr) - (INTPTR)(pool.kvaddr); //先计算偏移量,注意这里应该是个负数,因为用户空间在低地址位
    return userAddr - offset;
}
/// 检查指定地址在当前进程已使用的BUF
LITE_OS_SEC_TEXT STATIC UINT32 CheckUsedBuffer(const VOID *node, IpcListNode **outPtr)
{
    VOID *ptr = NULL;
    LosProcessCB *pcb = OsCurrProcessGet();
    IpcPool pool = pcb->ipcInfo->pool;//获取进程IPC内存池
    if ((node == NULL) || ((INTPTR)node < (INTPTR)(pool.uvaddr)) ||
        ((INTPTR)node > (INTPTR)(pool.uvaddr) + pool.poolSize)) {
        return -EINVAL;
    }
    ptr = (VOID *)GetIpcKernelAddr(pcb->processID, (INTPTR)(node));//通过用户空间地址获取内核空间地址
    if (IsIpcNode(pcb->processID, ptr) != TRUE) {//检查是否为IPC节点
        return -EFAULT;
    }
    *outPtr = (IpcListNode *)ptr;//参数带走节点,内核空间地址
    return LOS_OK;
}
/// 获取任务ID
LITE_OS_SEC_TEXT STATIC UINT32 GetTid(UINT32 serviceHandle, UINT32 *taskID)
{
    if (serviceHandle >= MAX_SERVICE_NUM) {//超过任务数
        return -EINVAL;
    }
    (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);
#if (USE_TASKID_AS_HANDLE == 1)
    *taskID = serviceHandle ? serviceHandle : g_cmsTask.taskID;
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
    return LOS_OK;
#else
    if (g_serviceHandleMap[serviceHandle].status == HANDLE_REGISTED) {//必须得是已注册
        *taskID = g_serviceHandleMap[serviceHandle].taskID;//获取已注册服务的任务ID
        (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
        return LOS_OK;
    }
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
    return -EINVAL;
#endif
}
/// 任务 注册服务,`LiteIPC`的核心思想就是在内核态为每个`Service`任务维护一个`IPC`消息队列, 一个任务可以创建多个服务
LITE_OS_SEC_TEXT STATIC UINT32 GenerateServiceHandle(UINT32 taskID, HandleStatus status, UINT32 *serviceHandle)
{
    (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);
#if (USE_TASKID_AS_HANDLE == 1)
    *serviceHandle = taskID ? taskID : LOS_CurTaskIDGet(); /* if taskID is 0, return curTaskID | 如果任务ID为0,返回当地任务ID ,那么请问0号任务是谁 ???*/
    if (*serviceHandle != g_cmsTask.taskID) {
        (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
        return LOS_OK;
    }
#else
    for (UINT32 i = 1; i < MAX_SERVICE_NUM; i++) {//每个任务都可以注册成一个服务,所有服务的上限等于任务的数量
        if (g_serviceHandleMap[i].status == HANDLE_NOT_USED) {//找一个可用的服务
            g_serviceHandleMap[i].taskID = taskID;//为哪个任务所占有
            g_serviceHandleMap[i].status = status;//修改状态  
            *serviceHandle = i;//带走占用第几个服务, 注意这里服务的ID和任务的ID并不一致 
            (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
            return LOS_OK;
        }
    }
#endif
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
    return -EINVAL;
}
/// 刷新指定服务, 多刷几次状态就变了, result != 0 时,服务停止
LITE_OS_SEC_TEXT STATIC VOID RefreshServiceHandle(UINT32 serviceHandle, UINT32 result)
{
#if (USE_TASKID_AS_HANDLE == 0)
    (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);
    if ((result == LOS_OK) && (g_serviceHandleMap[serviceHandle].status == HANDLE_REGISTING)) {//如果状态为注册中
        g_serviceHandleMap[serviceHandle].status = HANDLE_REGISTED; // 变成已注册
    } else {
        g_serviceHandleMap[serviceHandle].status = HANDLE_NOT_USED; // 变成未使用
    }
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
#endif
}

/*!
 * @brief AddServiceAccess	配置访问权限,具有两层含义
 * 1. taskID所在的进程可以访问 serviceHandle服务所属的任务
 * 2. serviceHandle服务所属的任务可以访问taskID所在的进程
 * @param serviceHandle	服务ID
 * @param taskID	
 * @return	
 *
 * @see
 */
LITE_OS_SEC_TEXT STATIC UINT32 AddServiceAccess(UINT32 taskID, UINT32 serviceHandle)
{
    UINT32 serviceTid = 0;
    UINT32 ret = GetTid(serviceHandle, &serviceTid);//通过服务获取任务 ,注意 任务:服务 = 1:N 
    if (ret != LOS_OK) {
        PRINT_ERR("Liteipc AddServiceAccess GetTid failed\n");
        return ret;
    }
    LosTaskCB *tcb = OS_TCB_FROM_TID(serviceTid);
    UINT32 processID = OS_TCB_FROM_TID(taskID)->processID;
    LosProcessCB *pcb = OS_PCB_FROM_PID(processID);
    if ((tcb->ipcTaskInfo == NULL) || (pcb->ipcInfo == NULL)) {
        PRINT_ERR("Liteipc AddServiceAccess ipc not create! pid %u tid %u\n", processID, tcb->taskID);
        return -EINVAL;
    }
    tcb->ipcTaskInfo->accessMap[processID] = TRUE;//允许任务访问所属进程,此处为任务所在的进程
    pcb->ipcInfo->access[serviceTid] = TRUE;//允许所属进程访问任务
    return LOS_OK;
}
/// 参数服务是否有访问当前进程的权限,实际中会有A进程的任务去给B进程发送IPC信息,所以需要鉴权 
LITE_OS_SEC_TEXT STATIC BOOL HasServiceAccess(UINT32 serviceHandle)
{
    UINT32 serviceTid = 0;
    UINT32 curProcessID = LOS_GetCurrProcessID();//获取当前进程ID
    UINT32 ret;
    if (serviceHandle >= MAX_SERVICE_NUM) {
        return FALSE;
    }
    if (serviceHandle == 0) {
        return TRUE;
    }
    ret = GetTid(serviceHandle, &serviceTid);//获取参数服务所属任务ID
    if (ret != LOS_OK) {
        PRINT_ERR("Liteipc HasServiceAccess GetTid failed\n");
        return FALSE;
    }
    if (OS_TCB_FROM_TID(serviceTid)->processID == curProcessID) {//如果任务所在进程就是当前进程,直接返回OK
        return TRUE;
    }

    if (OS_TCB_FROM_TID(serviceTid)->ipcTaskInfo == NULL) {//如果参数任务没有开通处理IPC信息功能
        return FALSE;
    }

    return OS_TCB_FROM_TID(serviceTid)->ipcTaskInfo->accessMap[curProcessID];//返回任务访问进程的权限
}
///将当前任务设置成进程ipc的任务ID
LITE_OS_SEC_TEXT STATIC UINT32 SetIpcTask(VOID)
{
    if (OsCurrProcessGet()->ipcInfo->ipcTaskID == INVAILD_ID) { //未设置时
        OsCurrProcessGet()->ipcInfo->ipcTaskID = LOS_CurTaskIDGet();//当前任务
        return OsCurrProcessGet()->ipcInfo->ipcTaskID;
    }
    PRINT_ERR("Liteipc curprocess %d IpcTask already set!\n", OsCurrProcessGet()->processID);
    return -EINVAL;
}
///是否设置ipc任务ID
LITE_OS_SEC_TEXT BOOL IsIpcTaskSet(VOID)
{
    if (OsCurrProcessGet()->ipcInfo->ipcTaskID == INVAILD_ID) {
        return FALSE;
    }
    return TRUE;
}
/// 获取IPC任务ID
LITE_OS_SEC_TEXT STATIC UINT32 GetIpcTaskID(UINT32 processID, UINT32 *ipcTaskID)
{
    if (OS_PCB_FROM_PID(processID)->ipcInfo->ipcTaskID == INVAILD_ID) {
        return LOS_NOK;
    }
    *ipcTaskID = OS_PCB_FROM_PID(processID)->ipcInfo->ipcTaskID;
    return LOS_OK;
}
/// serviceHandle 给 processID 发送死亡/结束消息, serviceHandle 为 taskID
LITE_OS_SEC_TEXT STATIC UINT32 SendDeathMsg(UINT32 processID, UINT32 serviceHandle)
{
    UINT32 ipcTaskID;
    UINT32 ret;
    IpcContent content;
    IpcMsg msg;
    LosProcessCB *pcb = OS_PCB_FROM_PID(processID);//获取指定进程控制块

    if (pcb->ipcInfo == NULL) {
        return -EINVAL;
    }

    pcb->ipcInfo->access[serviceHandle] = FALSE;

    ret = GetIpcTaskID(processID, &ipcTaskID);//获取操作该进程IPC的任务ID, processID 下的某个 taskID 
    if (ret != LOS_OK) {
        return -EINVAL;
    }
    content.flag = SEND;
    content.outMsg = &msg;
    memset_s(content.outMsg, sizeof(IpcMsg), 0, sizeof(IpcMsg));
    content.outMsg->type = MT_DEATH_NOTIFY;
    content.outMsg->target.handle = ipcTaskID;
    content.outMsg->target.token = serviceHandle;
    content.outMsg->code = 0;
    return LiteIpcWrite(&content);
}
/// 删除指定的Service
LITE_OS_SEC_TEXT VOID LiteIpcRemoveServiceHandle(UINT32 taskID)
{
    UINT32 j;
    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);
    IpcTaskInfo *ipcTaskInfo = taskCB->ipcTaskInfo;
    if (ipcTaskInfo == NULL) {
        return;
    }

#if (USE_TASKID_AS_HANDLE == 1) // 任务ID当做句柄使用

    UINT32 intSave;
    LOS_DL_LIST *listHead = NULL;
    LOS_DL_LIST *listNode = NULL;
    IpcListNode *node = NULL;
    UINT32 processID = taskCB->processID;

    listHead = &(ipcTaskInfo->msgListHead);// ipc 节点链表
    do {// 循环删除 任务IPC上挂的各个节点
        SCHEDULER_LOCK(intSave);
        if (LOS_ListEmpty(listHead)) {//空判
            SCHEDULER_UNLOCK(intSave);
            break;
        } else {
            listNode = LOS_DL_LIST_FIRST(listHead); //拿到首个节点
            LOS_ListDelete(listNode); //删除节点
            node = LOS_DL_LIST_ENTRY(listNode, IpcListNode, listNode);//获取节点所在结构体 IpcListNode
            SCHEDULER_UNLOCK(intSave);
            (VOID)HandleSpecialObjects(taskCB->taskID, node, TRUE);//处理节点
            (VOID)LiteIpcNodeFree(processID, (VOID *)node);//释放节点占用的进程空间
        }
    } while (1);

    ipcTaskInfo->accessMap[processID] = FALSE;
    for (j = 0; j < MAX_SERVICE_NUM; j++) {
        if (ipcTaskInfo->accessMap[j] == TRUE) {
            ipcTaskInfo->accessMap[j] = FALSE;
            (VOID)SendDeathMsg(j, taskCB->taskID); //给进程发送taskCB死亡的消息
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
        for (j = 0; j < MAX_SERVICE_NUM; j++) {
            if (ipcTaskInfo->accessMap[j] == TRUE) {
                (VOID)SendDeathMsg(j, i);
            }
        }
    }
#endif

    (VOID)LOS_MemFree(m_aucSysMem1, ipcTaskInfo);
    taskCB->ipcTaskInfo = NULL;
}
/// 设置CMS
LITE_OS_SEC_TEXT STATIC UINT32 SetCms(UINTPTR maxMsgSize)
{
    if (maxMsgSize < sizeof(IpcMsg)) {
        return -EINVAL;
    }
    (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);
#if (USE_TASKID_AS_HANDLE == 1)
    if (g_cmsTask.status == HANDLE_NOT_USED) {
        g_cmsTask.status = HANDLE_REGISTED; // 已注册   ServiceManager
        g_cmsTask.taskID = LOS_CurTaskIDGet();//将当前任务设置为 ServiceManager
        g_cmsTask.maxMsgSize = maxMsgSize;	//最大消息内容
        (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
        return LOS_OK;
    }
#else
    if (g_serviceHandleMap[0].status == HANDLE_NOT_USED) {
        g_serviceHandleMap[0].status = HANDLE_REGISTED;// 已注册   ServiceManager
        g_serviceHandleMap[0].taskID = LOS_CurTaskIDGet();//将当前任务设置为 ServiceManager
        (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
        return LOS_OK;
    }
#endif
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
    return -EEXIST;
}
/// 是否设置了 ServiceManager 整个系统只能有一个ServiceManager，而Service可以有多个。
/// ServiceManager有两个主要功能：一是负责Service的注册和注销，二是负责管理Service的访问权限（只有有权限的任务（Task）可以向对应的Service发送IPC消息）。
LITE_OS_SEC_TEXT STATIC BOOL IsCmsSet(VOID)
{
    BOOL ret;
    (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);
#if (USE_TASKID_AS_HANDLE == 1)
    ret = g_cmsTask.status == HANDLE_REGISTED; //已注册ServiceManager
#else
    ret = g_serviceHandleMap[0].status == HANDLE_REGISTED;//已注册 ServiceManager
#endif
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
    return ret;
}
/// 指定任务是否为 ServiceManager
LITE_OS_SEC_TEXT STATIC BOOL IsCmsTask(UINT32 taskID)
{
    BOOL ret;
    (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);
#if (USE_TASKID_AS_HANDLE == 1)
    ret = IsCmsSet() ? (OS_TCB_FROM_TID(taskID)->processID == OS_TCB_FROM_TID(g_cmsTask.taskID)->processID) : FALSE;//对比任务的进程和已注册ServiceManager是否一致
#else
    ret = IsCmsSet() ? (OS_TCB_FROM_TID(taskID)->processID ==
        OS_TCB_FROM_TID(g_serviceHandleMap[0].taskID)->processID) : FALSE; // g_serviceHandleMap[0] 为 ServiceManager, 把ServiceManager和service放一块很怪!!! @note_thinking 
#endif
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
    return ret;
}
/// 任务是否活跃
LITE_OS_SEC_TEXT STATIC BOOL IsTaskAlive(UINT32 taskID)
{
    LosTaskCB *tcb = NULL;
    if (OS_TID_CHECK_INVALID(taskID)) { //检查是否存在
        return FALSE;
    }
    tcb = OS_TCB_FROM_TID(taskID); //获取任务控制块
    if (!OsTaskIsUserMode(tcb)) {
        return FALSE;
    }
    if (OsTaskIsUnused(tcb)) {
        return FALSE;
    }
    if (OsTaskIsInactive(tcb)) {//任务是否活跃
        return FALSE;
    }
    return TRUE;
}
/// 按句柄方式处理, 参数 processID 往往不是当前进程
LITE_OS_SEC_TEXT STATIC UINT32 HandleFd(UINT32 processID, SpecialObj *obj, BOOL isRollback)
{
    int ret;
    if (isRollback == FALSE) { // 不回滚
        ret = CopyFdToProc(obj->content.fd, processID);//目的是将两个不同进程fd都指向同一个系统fd,共享FD的感觉
        if (ret < 0) {//返回 processID 的 新 fd
            return ret;
        }
        obj->content.fd = ret; // 记录 processID 的新FD, 可用于回滚
    } else {// 回滚时关闭进程FD
        ret = CloseProcFd(obj->content.fd, processID);
        if (ret < 0) {
            return ret;
        }
    }

    return LOS_OK;
}
/// 按指针方式处理
LITE_OS_SEC_TEXT STATIC UINT32 HandlePtr(UINT32 processID, SpecialObj *obj, BOOL isRollback)
{
    VOID *buf = NULL;
    UINT32 ret;
    if ((obj->content.ptr.buff == NULL) || (obj->content.ptr.buffSz == 0)) {
        return -EINVAL;
    }
    if (isRollback == FALSE) {
        if (LOS_IsUserAddress((vaddr_t)(UINTPTR)(obj->content.ptr.buff)) == FALSE) { // 判断是否为用户空间地址
            PRINT_ERR("Liteipc Bad ptr address\n"); //不在用户空间时
            return -EINVAL;
        }
        buf = LiteIpcNodeAlloc(processID, obj->content.ptr.buffSz);//在内核空间分配内存接受来自用户空间的数据
        if (buf == NULL) {
            PRINT_ERR("Liteipc DealPtr alloc mem failed\n");
            return -EINVAL;
        }
        ret = copy_from_user(buf, obj->content.ptr.buff, obj->content.ptr.buffSz);//从用户空间拷贝数据到内核空间
        if (ret != LOS_OK) {
            LiteIpcNodeFree(processID, buf);
            return ret;
        }//这里要说明下 obj->content.ptr.buff的变化,虽然都是用户空间的地址,但第二次已经意义变了,虽然数据一样,但指向的是申请经过拷贝后的内核空间
        obj->content.ptr.buff = (VOID *)GetIpcUserAddr(processID, (INTPTR)buf);//获取进程 processID的用户空间地址,如此用户空间操作buf其实操作的是内核空间
        EnableIpcNodeFreeByUser(processID, (VOID *)buf);//创建一个IPC节点,挂到可使用链表上,供读取
    } else {
        (VOID)LiteIpcNodeFree(processID, (VOID *)GetIpcKernelAddr(processID, (INTPTR)obj->content.ptr.buff));//在内核空间释放IPC节点
    }
    return LOS_OK;
}
/// 按服务的方式处理,此处推断 Svc 应该是 service 的简写 @note_thinking
LITE_OS_SEC_TEXT STATIC UINT32 HandleSvc(UINT32 dstTid, const SpecialObj *obj, BOOL isRollback)
{
    UINT32 taskID = 0;
    if (isRollback == FALSE) {
        if (IsTaskAlive(obj->content.svc.handle) == FALSE) {
            PRINT_ERR("Liteipc HandleSvc wrong svctid\n");
            return -EINVAL;
        }
        if (HasServiceAccess(obj->content.svc.handle) == FALSE) {
            PRINT_ERR("Liteipc %s, %d\n", __FUNCTION__, __LINE__);
            return -EACCES;
        }
        if (GetTid(obj->content.svc.handle, &taskID) == 0) {//获取参数消息服务ID所属任务
            if (taskID == OS_PCB_FROM_PID(OS_TCB_FROM_TID(taskID)->processID)->ipcInfo->ipcTaskID) {//如果任务ID一样,即任务ID为ServiceManager
                AddServiceAccess(dstTid, obj->content.svc.handle);
            }
        }
    }
    return LOS_OK;
}
/// 创建处理对象,好复杂, 消息套消息
LITE_OS_SEC_TEXT STATIC UINT32 HandleObj(UINT32 dstTid, SpecialObj *obj, BOOL isRollback)
{
    UINT32 ret;
    UINT32 processID = OS_TCB_FROM_TID(dstTid)->processID;//获取目标任务所在进程
    switch (obj->type) {
        case OBJ_FD://fd:文件描述符
            ret = HandleFd(processID, obj, isRollback);
            break;
        case OBJ_PTR://指针方式消息
            ret = HandlePtr(processID, obj, isRollback);
            break;
        case OBJ_SVC://服务类消息
            ret = HandleSvc(dstTid, (const SpecialObj *)obj, isRollback);
            break;
        default:
            ret = -EINVAL;
            break;
    }
    return ret;
}
//处理指定任务的某个IPC节点
LITE_OS_SEC_TEXT STATIC UINT32 HandleSpecialObjects(UINT32 dstTid, IpcListNode *node, BOOL isRollback)
{
    UINT32 ret = LOS_OK;
    IpcMsg *msg = &(node->msg);
    INT32 i;
    SpecialObj *obj = NULL;
    UINT32 *offset = (UINT32 *)(UINTPTR)(msg->offsets);//拿到消息偏移量
    if (isRollback) {//如果需要回滚
        i = msg->spObjNum;
        goto EXIT;
    }
    for (i = 0; i < msg->spObjNum; i++) {//遍历消息个数
        if (offset[i] > msg->dataSz - sizeof(SpecialObj)) {
            ret = -EINVAL;
            goto EXIT;
        }
        if ((i > 0) && (offset[i] < offset[i - 1] + sizeof(SpecialObj))) {
            ret = -EINVAL;
            goto EXIT;
        }
        obj = (SpecialObj *)((UINTPTR)msg->data + offset[i]);//一个个拿消息对象
        if (obj == NULL) {
            ret = -EINVAL;
            goto EXIT;
        }
        ret = HandleObj(dstTid, obj, FALSE);//一个个处理对象,三种消息对象
        if (ret != LOS_OK) {
            goto EXIT;
        }
    }
    return LOS_OK;
EXIT://异常回滚处理
    for (i--; i >= 0; i--) {
        obj = (SpecialObj *)((UINTPTR)msg->data + offset[i]);
        (VOID)HandleObj(dstTid, obj, TRUE);
    }
    return ret;
}
/// 检查消息内容大小
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
    totalSize = (UINT64)sizeof(IpcMsg) + msg->dataSz + msg->spObjNum * sizeof(UINT32);//缓冲总大小
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
            totalSize += obj->content.ptr.buffSz;//指针指向的buf存储在用户空间其他地方
        }
    }
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
    if (totalSize > g_cmsTask.maxMsgSize) {
        (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
        return -EINVAL;
    }
    (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
    return LOS_OK;
}
///< 从用户空间拷贝消息数据到内核空间
LITE_OS_SEC_TEXT STATIC UINT32 CopyDataFromUser(IpcListNode *node, UINT32 bufSz, const IpcMsg *msg)
{
    UINT32 ret;
    ret = (UINT32)memcpy_s((VOID *)(&node->msg), bufSz - sizeof(LOS_DL_LIST), (const VOID *)msg, sizeof(IpcMsg));
    if (ret != LOS_OK) {
        PRINT_DEBUG("%s, %d, %u\n", __FUNCTION__, __LINE__, ret);
        return ret;
    }

    if (msg->dataSz) {//消息的具体内容
        node->msg.data = (VOID *)((UINTPTR)node + sizeof(IpcListNode));
        ret = copy_from_user((VOID *)(node->msg.data), msg->data, msg->dataSz);//把真正的数据拷贝到其后
        if (ret != LOS_OK) {
            PRINT_DEBUG("%s, %d\n", __FUNCTION__, __LINE__);
            return ret;
        }
    } else {
        node->msg.data = NULL;
    }

    if (msg->spObjNum) {//消息个数
        node->msg.offsets = (VOID *)((UINTPTR)node + sizeof(IpcListNode) + msg->dataSz);//将指针定位到放置偏移量的位置
        ret = copy_from_user((VOID *)(node->msg.offsets), msg->offsets, msg->spObjNum * sizeof(UINT32));//拷贝偏移量
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
/// 是否有效回复
LITE_OS_SEC_TEXT STATIC BOOL IsValidReply(const IpcContent *content)
{
    UINT32 curProcessID = LOS_GetCurrProcessID();
    IpcListNode *node = (IpcListNode *)GetIpcKernelAddr(curProcessID, (INTPTR)(content->buffToFree));//通过用户空间地址获取内核地址
    IpcMsg *requestMsg = &node->msg;
    IpcMsg *replyMsg = content->outMsg;
    UINT32 reqDstTid = 0;
    /* Check whether the reply matches the request */
    if ((requestMsg->type != MT_REQUEST)  || //判断条件
        (requestMsg->flag == LITEIPC_FLAG_ONEWAY) ||
        (replyMsg->timestamp != requestMsg->timestamp) ||
        (replyMsg->target.handle != requestMsg->taskID) ||
        (GetTid(requestMsg->target.handle, &reqDstTid) != 0) ||
        (OS_TCB_FROM_TID(reqDstTid)->processID != curProcessID)) {
        return FALSE;
    }
    return TRUE;
}
/// 检查参数,并获取目标 任务ID
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
    switch (msg->type) {//消息类型
        case MT_REQUEST: //请求
            if (HasServiceAccess(msg->target.handle)) {//检查参数任务是否有权限访问当前进程
                ret = GetTid(msg->target.handle, dstTid);
                if (ret != LOS_OK) {
                    return -EINVAL;
                }
            } else {
                PRINT_ERR("Liteipc %s, %d\n", __FUNCTION__, __LINE__);
                return -EACCES;
            }
#if (USE_TIMESTAMP == 1)
            msg->timestamp = now;//记录时间戳
#endif
            break;
        case MT_REPLY://回复
        case MT_FAILED_REPLY: //回复失败消息
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
                 (*dstTid == INVAILD_ID) ? INVAILD_ID : OS_TCB_FROM_TID(*dstTid)->processID, 0);
                PRINT_ERR("Liteipc A timeout reply, request timestamp:%lld, now:%lld\n", msg->timestamp, now);
                return -ETIME;
            }
#endif
            *dstTid = msg->target.handle;
            break;
        case MT_DEATH_NOTIFY://死亡消息
            *dstTid = msg->target.handle;
#if (USE_TIMESTAMP == 1)
            msg->timestamp = now;
#endif
            break;
        default:
            PRINT_DEBUG("Unknow msg type:%d\n", msg->type);
            return -EINVAL;
    }

    if (IsTaskAlive(*dstTid) == FALSE) {
        return -EINVAL;
    }
    return LOS_OK;
}
/// 写IPC消息队列,从用户空间到内核空间
LITE_OS_SEC_TEXT STATIC UINT32 LiteIpcWrite(IpcContent *content)
{
    UINT32 ret, intSave;
    UINT32 dstTid;

    IpcMsg *msg = content->outMsg;

    ret = CheckPara(content, &dstTid);//获取目标任务ID
    if (ret != LOS_OK) {
        return ret;
    }

    LosTaskCB *tcb = OS_TCB_FROM_TID(dstTid);//目标任务实体
    LosProcessCB *pcb = OS_PCB_FROM_PID(tcb->processID);//目标进程实体
    if (pcb->ipcInfo == NULL) {
        PRINT_ERR("pid %u Liteipc not create\n", tcb->processID);
        return -EINVAL;
    }
	//这里为什么要申请msg->dataSz,因为IpcMsg中的真正数据体 data是个指针,它的大小是dataSz . 同时申请存储偏移量空间
    UINT32 bufSz = sizeof(IpcListNode) + msg->dataSz + msg->spObjNum * sizeof(UINT32);//这句话是理解上层消息在内核空间数据存放的关键!!! @note_good
    IpcListNode *buf = (IpcListNode *)LiteIpcNodeAlloc(tcb->processID, bufSz);//向内核空间申请bufSz大小内存
    if (buf == NULL) {
        PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
        return -ENOMEM;
    }//IpcListNode的第一个成员变量就是IpcMsg
    ret = CopyDataFromUser(buf, bufSz, (const IpcMsg *)msg);//将消息内容拷贝到内核空间,包括消息控制体+内容体+偏移量
    if (ret != LOS_OK) {
        PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
        goto ERROR_COPY;
    }

    if (tcb->ipcTaskInfo == NULL) {//如果任务还没有IPC信息
        tcb->ipcTaskInfo = LiteIpcTaskInit();//初始化这个任务的IPC信息模块,因为消息来了要处理了
    }

    ret = HandleSpecialObjects(dstTid, buf, FALSE);//处理消息
    if (ret != LOS_OK) {
        PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
        goto ERROR_COPY;
    }
    /* add data to list and wake up dest task *///向列表添加数据并唤醒目标任务
    SCHEDULER_LOCK(intSave);
    LOS_ListTailInsert(&(tcb->ipcTaskInfo->msgListHead), &(buf->listNode));//把消息控制体挂到目标任务的IPC链表头上
    OsHookCall(LOS_HOOK_TYPE_IPC_WRITE, &buf->msg, dstTid, tcb->processID, tcb->waitFlag);
    if (tcb->waitFlag == OS_TASK_WAIT_LITEIPC) {//如果这个任务在等这个消息,注意这个tcb可不是当前任务
        OsTaskWakeClearPendMask(tcb);//撕掉对应标签
        OsSchedTaskWake(tcb);//唤醒任务执行,因为任务在等待读取 IPC消息
        SCHEDULER_UNLOCK(intSave);
        LOS_MpSchedule(OS_MP_CPU_ALL);//设置调度方式,所有CPU核发生一次调度,这里非要所有CPU都调度吗? 
        //可不可以查询下该任务挂在哪个CPU上,只调度对应CPU呢?   注者在此抛出思考 @note_thinking
        LOS_Schedule();//发起调度
    } else {
        SCHEDULER_UNLOCK(intSave);
    }
    return LOS_OK;
ERROR_COPY:
    LiteIpcNodeFree(OS_TCB_FROM_TID(dstTid)->processID, buf);//拷贝发生错误就要释放内核堆内存,那可是好大一块堆内存啊
    return ret;
}
/// 检查收到的消息
LITE_OS_SEC_TEXT STATIC UINT32 CheckRecievedMsg(IpcListNode *node, IpcContent *content, LosTaskCB *tcb)
{
    UINT32 ret = LOS_OK;
    if (node == NULL) {
        return -EINVAL;
    }
    switch (node->msg.type) { //消息类型
        case MT_REQUEST: //请求消息
            if ((content->flag & SEND) == SEND) {
                PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
                ret = -EINVAL;
            }
            break;
        case MT_FAILED_REPLY://回复失败
            ret = -ENOENT;
            /* fall-through */
        case MT_REPLY://回复
            if ((content->flag & SEND) != SEND) {
                PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
                ret = -EINVAL;
            }
#if (USE_TIMESTAMP == 1)
            if (node->msg.timestamp != content->outMsg->timestamp) {//检查时间戳
                PRINT_ERR("Recieve a unmatch reply, drop it\n");
                ret = -EINVAL;
            }
#else
            if ((node->msg.code != content->outMsg->code) ||
                (node->msg.target.token != content->outMsg->target.token)) {
                PRINT_ERR("Recieve a unmatch reply, drop it\n");
                ret = -EINVAL;
            }
#endif
            break;
        case MT_DEATH_NOTIFY:
            break;
        default:
            PRINT_ERR("Unknow msg type:%d\n", node->msg.type);
            ret =  -EINVAL;
    }
    if (ret != LOS_OK) {
        OsHookCall(LOS_HOOK_TYPE_IPC_READ_DROP, &node->msg, tcb->waitFlag);
        (VOID)HandleSpecialObjects(LOS_CurTaskIDGet(), node, TRUE);
        (VOID)LiteIpcNodeFree(LOS_GetCurrProcessID(), (VOID *)node);
    } else {
        OsHookCall(LOS_HOOK_TYPE_IPC_READ, &node->msg, tcb->waitFlag);
    }
    return ret;
}
/// 读取IPC消息
LITE_OS_SEC_TEXT STATIC UINT32 LiteIpcRead(IpcContent *content)
{
    UINT32 intSave, ret;
    UINT32 selfTid = LOS_CurTaskIDGet();//当前任务ID
    LOS_DL_LIST *listHead = NULL;
    LOS_DL_LIST *listNode = NULL;
    IpcListNode *node = NULL;
    UINT32 syncFlag = (content->flag & SEND) && (content->flag & RECV);//同步标签
    UINT32 timeout = syncFlag ? LOS_MS2Tick(LITEIPC_TIMEOUT_MS) : LOS_WAIT_FOREVER;

    LosTaskCB *tcb = OS_TCB_FROM_TID(selfTid);//获取当前任务实体
    if (tcb->ipcTaskInfo == NULL) {//如果任务还没有赋予IPC功能
        tcb->ipcTaskInfo = LiteIpcTaskInit();//初始化任务的IPC
    }

    listHead = &(tcb->ipcTaskInfo->msgListHead);//获取IPC信息头节点
    do {//注意这里是个死循环
        SCHEDULER_LOCK(intSave);
        if (LOS_ListEmpty(listHead)) {//链表为空 ?
            OsTaskWaitSetPendMask(OS_TASK_WAIT_LITEIPC, OS_INVALID_VALUE, timeout);//设置当前任务要等待的信息
            OsHookCall(LOS_HOOK_TYPE_IPC_TRY_READ, syncFlag ? MT_REPLY : MT_REQUEST, tcb->waitFlag);//向hook模块输入等待日志信息
            ret = OsSchedTaskWait(&g_ipcPendlist, timeout, TRUE);//将任务挂到全局链表上,任务进入等IPC信息,等待时间(timeout),此处产生调度,将切换到别的任务执行
			//如果一个消息在超时前到达,则任务会被唤醒执行,返回就不是LOS_ERRNO_TSK_TIMEOUT
			if (ret == LOS_ERRNO_TSK_TIMEOUT) {//如果发生指定的时间还没有IPC到达时
                OsHookCall(LOS_HOOK_TYPE_IPC_READ_TIMEOUT, syncFlag ? MT_REPLY : MT_REQUEST, tcb->waitFlag);//打印任务等待IPC时发生 回复/请求超时
                SCHEDULER_UNLOCK(intSave);
                return -ETIME;
            }

            if (OsTaskIsKilled(tcb)) {//如果发生任务被干掉了的异常
                OsHookCall(LOS_HOOK_TYPE_IPC_KILL, syncFlag ? MT_REPLY : MT_REQUEST, tcb->waitFlag);//打印任务在等待IPC期间被干掉了的
                SCHEDULER_UNLOCK(intSave);
                return -ERFKILL;
            }

            SCHEDULER_UNLOCK(intSave);
        } else {//有IPC节点数据时
            listNode = LOS_DL_LIST_FIRST(listHead);//拿到首个节点
            LOS_ListDelete(listNode);//从链表上摘掉节点,读后即焚
            node = LOS_DL_LIST_ENTRY(listNode, IpcListNode, listNode);//获取节点实体
            SCHEDULER_UNLOCK(intSave);
            ret = CheckRecievedMsg(node, content, tcb);//检查收到的信息
            if (ret == LOS_OK) {//信息没问题
                break;
            }
            if (ret == -ENOENT) { /* It means that we've recieved a failed reply | 收到异常回复*/
                return ret;
            }
        }
    } while (1);
    node->msg.data = (VOID *)GetIpcUserAddr(LOS_GetCurrProcessID(), (INTPTR)(node->msg.data));//转成用户空间地址
    node->msg.offsets = (VOID *)GetIpcUserAddr(LOS_GetCurrProcessID(), (INTPTR)(node->msg.offsets));//转成用户空间的偏移量
    content->inMsg = (VOID *)GetIpcUserAddr(LOS_GetCurrProcessID(), (INTPTR)(&(node->msg)));//转成用户空间数据结构
    EnableIpcNodeFreeByUser(LOS_GetCurrProcessID(), (VOID *)node);//创建一个空闲节点,并挂到进程IPC已使用节点链表上
    return LOS_OK;
}
/// 处理 IPC 消息
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

    if ((content->flag & BUFF_FREE) == BUFF_FREE) { //释放buf
        ret = CheckUsedBuffer(content->buffToFree, &nodeNeedFree);
        if (ret != LOS_OK) {
            PRINT_ERR("CheckUsedBuffer failed:%d\n", ret);
            return ret;
        }
    }

    if ((content->flag & SEND) == SEND) { // 向外发送
        if (content->outMsg == NULL) {
            PRINT_ERR("content->outmsg is null\n");
            ret = -EINVAL;
            goto BUFFER_FREE;
        }// 1. 先将用户空间过来的消息拷贝到内核空间 (先拷贝壳)
        if (copy_from_user((void *)msg, (const void *)content->outMsg, sizeof(IpcMsg)) != LOS_OK) {
            PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
            ret = -EINVAL;
            goto BUFFER_FREE;
        }
        content->outMsg = msg;// outMsg 指向内核空间
        if ((content->outMsg->type < 0) || (content->outMsg->type >= MT_DEATH_NOTIFY)) {
            PRINT_ERR("LiteIpc unknown msg type:%d\n", content->outMsg->type);
            ret = -EINVAL;
            goto BUFFER_FREE;
        }
        ret = LiteIpcWrite(content);//写消息
        if (ret != LOS_OK) {
            PRINT_ERR("LiteIpcWrite failed\n");
            goto BUFFER_FREE;
        }
    }
BUFFER_FREE:
    if (nodeNeedFree != NULL) {
        UINT32 freeRet = LiteIpcNodeFree(LOS_GetCurrProcessID(), nodeNeedFree);
        ret = (freeRet == LOS_OK) ? ret : freeRet;
    }
    if (ret != LOS_OK) {
        return ret;
    }

    if ((content->flag & RECV) == RECV) {//接收到消息
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
/// 处理命令
LITE_OS_SEC_TEXT STATIC UINT32 HandleCmsCmd(CmsCmdContent *content)
{
    UINT32 ret = LOS_OK;
    CmsCmdContent localContent;//接受用户空间数据的栈空间
    if (content == NULL) {
        return -EINVAL;
    }
    if (IsCmsTask(LOS_CurTaskIDGet()) == FALSE) {//当前任务是否为ServiceManager
        return -EACCES;
    }
    if (copy_from_user((void *)(&localContent), (const void *)content, sizeof(CmsCmdContent)) != LOS_OK) {//将数据从用户空间拷贝到内核栈空间
        PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
        return -EINVAL;
    }
    switch (localContent.cmd) {
        case CMS_GEN_HANDLE: // 创建服务/添加权限
            if ((localContent.taskID != 0) && (IsTaskAlive(localContent.taskID) == FALSE)) {
                return -EINVAL;
            }
            ret = GenerateServiceHandle(localContent.taskID, HANDLE_REGISTED, &(localContent.serviceHandle));//注册服务,得到服务ID
            if (ret == LOS_OK) {
                ret = copy_to_user((void *)content, (const void *)(&localContent), sizeof(CmsCmdContent));//多了个服务ID,将数据拷贝回用户空间,
            }
            (VOID)LOS_MuxLock(&g_serviceHandleMapMux, LOS_WAIT_FOREVER);//此处的AddServiceAccess 标识着 ServiceManager可以访问所有服务
            AddServiceAccess(g_cmsTask.taskID, localContent.serviceHandle);//@note_thinking 此处好像漏了 #if (USE_TASKID_AS_HANDLE == 1)
            (VOID)LOS_MuxUnlock(&g_serviceHandleMapMux);
            break;
        case CMS_REMOVE_HANDLE: // 删除服务
            if (localContent.serviceHandle >= MAX_SERVICE_NUM) {
                return -EINVAL;
            }
            RefreshServiceHandle(localContent.serviceHandle, -1);// -1 代表服务停止/清空,可供其他任务使用
            break;
        case CMS_ADD_ACCESS: //添加权限
            if (IsTaskAlive(localContent.taskID) == FALSE) {
                return -EINVAL;
            }
            return AddServiceAccess(localContent.taskID, localContent.serviceHandle);//双向绑定
        default:
            PRINT_DEBUG("Unknow cmd cmd:%d\n", localContent.cmd);
            return -EINVAL;
    }
    return ret;
}
///真正的 IPC 控制操作 
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
	// 整个系统只能有一个ServiceManager，而Service可以有多个。ServiceManager有两个主要功能：一是负责Service的注册和注销，
	// 二是负责管理Service的访问权限（只有有权限的任务（Task）可以向对应的Service发送IPC消息）。
    switch (cmd) {
        case IPC_SET_CMS:
            return (INT32)SetCms(arg); //设置ServiceManager , 整个系统只能有一个ServiceManager
        case IPC_CMS_CMD: // 控制命令,创建/删除/添加权限 
            return (INT32)HandleCmsCmd((CmsCmdContent *)(UINTPTR)arg);
        case IPC_SET_IPC_THREAD://
            if (IsCmsSet() == FALSE) {//如果还没有指定 ServiceManager 
                PRINT_ERR("Liteipc ServiceManager not set!\n");
                return -EINVAL;
            }
            return (INT32)SetIpcTask();//将当前任务设置成当前进程的IPC任务ID
        case IPC_SEND_RECV_MSG://发送和接受消息,代表消息内容
            if (arg == 0) {
                return -EINVAL;
            }
            if (IsCmsSet() == FALSE) {
                PRINT_ERR("Liteipc ServiceManager not set!\n");
                return -EINVAL;
            }
            ret = LiteIpcMsgHandle((IpcContent *)(UINTPTR)arg);//处理IPC消息
            if (ret != LOS_OK) {
                return (INT32)ret;
            }
            break;
        default:
            PRINT_ERR("Unknow liteipc ioctl cmd:%d\n", cmd);
            return -EINVAL;
    }
    return (INT32)ret;
}
