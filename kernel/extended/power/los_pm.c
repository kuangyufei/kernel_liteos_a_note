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

#include "los_pm.h"
#include "securec.h"
#include "los_sched_pri.h"
#include "los_init.h"
#include "los_memory.h"
#include "los_spinlock.h"
#include "los_swtmr.h"
#include "los_mp.h"

/**
 * @brief 删除链:删除由装入点管理的文件
 * @verbatim 
    @note_link http://weharmonyos.com/openharmony/zh-cn/readme/%E7%94%B5%E6%BA%90%E7%AE%A1%E7%90%86%E5%AD%90%E7%B3%BB%E7%BB%9F.html
    电源管理子系统提供如下功能：
        重启系统。
        管理休眠运行锁。
        系统电源状态查询。
        充电和电池状态查询和上报。
        亮灭屏管理和亮度调节。
    /base/powermgr
    ├── battery_manager            # 电池服务组件
    │   ├── hdi                    # HDI层
    │   ├── interfaces             # 接口层
    │   ├── sa_profile             # SA配置文件
    │   ├── services               # 服务层
    │   └── utils                  # 工具和通用层
    ├── display_manager            # 显示控制组件
    │   ├── interfaces             # 接口层
    │   └── sa_profile             # SA配置文件
    │   └── services               # 服务层
    │   └── utils                  # 工具和通用层
    ├── powermgr_lite              # 轻量级电源管理组件
    │   ├── interfaces             # 接口层
    │   └── services               # 服务层
    └── power_manager              # 电源管理服务组件
        ├── interfaces             # 接口层
        ├── sa_profile             # SA配置文件
        └── services               # 服务层
        └── utils                  # 工具和通用层
    开发者通过电源管理子系统提供的接口可以进行申请和释放休眠运行锁RunningLock、获取电池信息、亮度调节、重启设备、关机等操作。

    电源管理子系统相关仓库

    powermgr_battery_manager
    powermgr_power_manager
    powermgr_display_manager
 * @endverbatim
 */
#define OS_MS_PER_TICK (OS_SYS_MS_PER_SECOND / LOSCFG_BASE_CORE_TICK_PER_SECOND)

#ifdef LOSCFG_KERNEL_PM
#define PM_INFO_SHOW(seqBuf, arg...) do {                   \
    if (seqBuf != NULL) {                                   \
        (void)LosBufPrintf((struct SeqBuf *)seqBuf, ##arg); \
    } else {                                                \
        PRINTK(arg);                                        \
    }                                                       \
} while (0)

#define OS_PM_LOCK_MAX      0xFFFFU
#define OS_PM_LOCK_NAME_MAX 28
#define OS_PM_SYS_EARLY        1
#define OS_PM_SYS_DEVICE_EARLY 2

typedef UINT32 (*Suspend)(UINT32 mode);
///< 电源锁控制块
typedef struct {
    CHAR         name[OS_PM_LOCK_NAME_MAX]; ///< 电源锁名称
    UINT32       count;	///< 数量
    UINT32       swtmrID;
    LOS_DL_LIST  list;	///< 电源锁链表,上面挂的是 OsPmLockCB
} OsPmLockCB;
///< 电源管理控制块 
typedef struct {
    LOS_SysSleepEnum  pmMode;		///< 模式类型
    LOS_SysSleepEnum  sysMode;
    UINT16            lock;		///< 锁数量
    BOOL              isWake;
    LosPmDevice       *device;
    LosPmSysctrl      *sysctrl;
    UINT64            enterSleepTime;
    LOS_DL_LIST       lockList;	///< 电源锁链表头,上面挂的是 OsPmLockCB
} LosPmCB;

#define PM_EVENT_LOCK_MASK    0xF
#define PM_EVENT_LOCK_RELEASE 0x1
STATIC EVENT_CB_S g_pmEvent;
STATIC LosPmCB g_pmCB; ///< 电源控制块全局遍历
STATIC SPIN_LOCK_INIT(g_pmSpin); ///< 电源模块自旋锁
STATIC LosPmSysctrl g_sysctrl;

STATIC VOID OsPmSysctrlInit(VOID);

STATIC VOID OsPmTickTimerStart(LosPmCB *pm)
{
    (VOID)pm;
    return;
}

STATIC BOOL OsPmTickTimerStop(LosPmCB *pm)
{
    (VOID)pm;
    return FALSE;
}

STATIC VOID OsPmCpuResume(LosPmCB *pm)
{
    if ((pm->sysMode == LOS_SYS_NORMAL_SLEEP) && (pm->sysctrl->normalResume != NULL)) {
        pm->sysctrl->normalResume();
    } else if ((pm->sysMode == LOS_SYS_LIGHT_SLEEP) && (pm->sysctrl->lightResume != NULL)) {
        pm->sysctrl->lightResume();
    } else if ((pm->sysMode == LOS_SYS_DEEP_SLEEP) && (pm->sysctrl->deepResume != NULL)) {
        pm->sysctrl->deepResume();
    }
}

VOID OsPmCpuSuspend(LosPmCB *pm)
{
    /* cpu enter low power mode */
    LOS_ASSERT(pm->sysctrl != NULL);

    if (pm->sysMode == LOS_SYS_NORMAL_SLEEP) {
        pm->sysctrl->normalSuspend();
    } else if (pm->sysMode == LOS_SYS_LIGHT_SLEEP) {
        pm->sysctrl->lightSuspend();
    } else if (pm->sysMode == LOS_SYS_DEEP_SLEEP) {
        pm->sysctrl->deepSuspend();
    } else {
        pm->sysctrl->shutdownSuspend();
    }
}

STATIC VOID OsPmResumePrepare(LosPmCB *pm, UINT32 mode, UINT32 prepare)
{
    if ((prepare == 0) && (pm->device != NULL) && (pm->device->resume != NULL)) {
        pm->device->resume(mode);
    }

    if (((prepare == 0) || (prepare == OS_PM_SYS_DEVICE_EARLY)) && (pm->sysctrl->late != NULL)) {
        pm->sysctrl->late(mode);
    }
}

STATIC UINT32 OsPmSuspendPrepare(Suspend sysSuspendEarly, Suspend deviceSuspend, UINT32 mode, UINT32 *prepare)
{
    UINT32 ret;

    if (sysSuspendEarly != NULL) {
        ret = sysSuspendEarly(mode);
        if (ret != LOS_OK) {
            *prepare = OS_PM_SYS_EARLY;
            return ret;
        }
    }

    if (deviceSuspend != NULL) {
        ret = deviceSuspend(mode);
        if (ret != LOS_OK) {
            *prepare = OS_PM_SYS_DEVICE_EARLY;
            return ret;
        }
    }

    return LOS_OK;
}

STATIC UINT32 OsPmSuspendCheck(LosPmCB *pm, Suspend *sysSuspendEarly, Suspend *deviceSuspend, LOS_SysSleepEnum *mode)
{
    LOS_SpinLock(&g_pmSpin);
    pm->sysMode = pm->pmMode;
    if (pm->lock > 0) {
        pm->sysMode = LOS_SYS_NORMAL_SLEEP;
        LOS_SpinUnlock(&g_pmSpin);
        return LOS_NOK;
    }

    pm->isWake = FALSE;
    *mode = pm->sysMode;
    *sysSuspendEarly = pm->sysctrl->early;
    if (pm->device != NULL) {
        *deviceSuspend = pm->device->suspend;
    } else {
        *deviceSuspend = NULL;
    }
    LOS_SpinUnlock(&g_pmSpin);
    return LOS_OK;
}

STATIC UINT32 OsPmSuspendSleep(LosPmCB *pm)
{
    UINT32 ret, intSave;
    Suspend sysSuspendEarly, deviceSuspend;
    LOS_SysSleepEnum mode;
    UINT32 prepare = 0;
    BOOL tickTimerStop = FALSE;

    ret = OsPmSuspendCheck(pm, &sysSuspendEarly, &deviceSuspend, &mode);
    if (ret != LOS_OK) {
        PRINT_ERR("Pm suspend mode is normal sleep! lock count %d\n", pm->lock);
        return ret;
    }

    ret = OsPmSuspendPrepare(sysSuspendEarly, deviceSuspend, (UINT32)mode, &prepare);
    if (ret != LOS_OK) {
        LOS_SpinLockSave(&g_pmSpin, &intSave);
        LOS_TaskLock();
        goto EXIT;
    }

    LOS_SpinLockSave(&g_pmSpin, &intSave);
    LOS_TaskLock();
    if (pm->isWake || (pm->lock > 0)) {
        goto EXIT;
    }

    tickTimerStop = OsPmTickTimerStop(pm);
    if (!tickTimerStop) {
        OsSchedResponseTimeReset(0);
        OsSchedExpireTimeUpdate();
    }

    OsPmCpuSuspend(pm);

    OsPmCpuResume(pm);

    OsPmTickTimerStart(pm);

EXIT:
    pm->sysMode = LOS_SYS_NORMAL_SLEEP;
    OsPmResumePrepare(pm, (UINT32)mode, prepare);

    LOS_SpinUnlockRestore(&g_pmSpin, intSave);
    LOS_TaskUnlock();
    return ret;
}

STATIC UINT32 OsPmDeviceRegister(LosPmCB *pm, LosPmDevice *device)
{
    if ((device->suspend == NULL) || (device->resume == NULL)) {
        return LOS_EINVAL;
    }

    LOS_SpinLock(&g_pmSpin);
    pm->device = device;
    LOS_SpinUnlock(&g_pmSpin);

    return LOS_OK;
}

STATIC UINT32 OsPmSysctrlRegister(LosPmCB *pm, LosPmSysctrl *sysctrl)
{
    LOS_SpinLock(&g_pmSpin);
    if (sysctrl->early != NULL) {
        pm->sysctrl->early = sysctrl->early;
    }
    if (sysctrl->late != NULL) {
        pm->sysctrl->late = sysctrl->late;
    }
    if (sysctrl->normalSuspend != NULL) {
        pm->sysctrl->normalSuspend = sysctrl->normalSuspend;
    }
    if (sysctrl->normalResume != NULL) {
        pm->sysctrl->normalResume = sysctrl->normalResume;
    }
    if (sysctrl->lightSuspend != NULL) {
        pm->sysctrl->lightSuspend = sysctrl->lightSuspend;
    }
    if (sysctrl->lightResume != NULL) {
        pm->sysctrl->lightResume = sysctrl->lightResume;
    }
    if (sysctrl->deepSuspend != NULL) {
        pm->sysctrl->deepSuspend = sysctrl->deepSuspend;
    }
    if (sysctrl->deepResume != NULL) {
        pm->sysctrl->deepResume = sysctrl->deepResume;
    }
    if (sysctrl->shutdownSuspend != NULL) {
        pm->sysctrl->shutdownSuspend = sysctrl->shutdownSuspend;
    }
    if (sysctrl->shutdownResume != NULL) {
        pm->sysctrl->shutdownResume = sysctrl->shutdownResume;
    }
    LOS_SpinUnlock(&g_pmSpin);

    return LOS_OK;
}

UINT32 LOS_PmRegister(LOS_PmNodeType type, VOID *node)
{
    LosPmCB *pm = &g_pmCB;

    if (node == NULL) {
        return LOS_EINVAL;
    }

    switch (type) {
        case LOS_PM_TYPE_DEVICE:
            return OsPmDeviceRegister(pm, (LosPmDevice *)node);
        case LOS_PM_TYPE_TICK_TIMER:
            PRINT_ERR("Pm, %d is an unsupported type\n", type);
            return LOS_EINVAL;
        case LOS_PM_TYPE_SYSCTRL:
            return OsPmSysctrlRegister(pm, (LosPmSysctrl *)node);
        default:
            break;
    }

    return LOS_EINVAL;
}

STATIC UINT32 OsPmDeviceUnregister(LosPmCB *pm, LosPmDevice *device)
{
    LOS_SpinLock(&g_pmSpin);
    if (pm->device == device) {
        pm->device = NULL;
        pm->pmMode = LOS_SYS_NORMAL_SLEEP;
        LOS_SpinUnlock(&g_pmSpin);
        return LOS_OK;
    }

    LOS_SpinUnlock(&g_pmSpin);
    return LOS_EINVAL;
}

STATIC UINT32 OsPmSysctrlUnregister(LosPmCB *pm, LosPmSysctrl *sysctrl)
{
    (VOID)sysctrl;
    LOS_SpinLock(&g_pmSpin);
    OsPmSysctrlInit();
    pm->pmMode = LOS_SYS_NORMAL_SLEEP;
    LOS_SpinUnlock(&g_pmSpin);

    return LOS_OK;
}

UINT32 LOS_PmUnregister(LOS_PmNodeType type, VOID *node)
{
    LosPmCB *pm = &g_pmCB;

    if (node == NULL) {
        return LOS_EINVAL;
    }

    switch (type) {
        case LOS_PM_TYPE_DEVICE:
            return OsPmDeviceUnregister(pm, (LosPmDevice *)node);
        case LOS_PM_TYPE_TICK_TIMER:
            PRINT_ERR("Pm, %d is an unsupported type\n", type);
            return LOS_EINVAL;
        case LOS_PM_TYPE_SYSCTRL:
            return OsPmSysctrlUnregister(pm, (LosPmSysctrl *)node);
        default:
            break;
    }

    return LOS_EINVAL;
}

VOID LOS_PmWakeSet(VOID)
{
    LosPmCB *pm = &g_pmCB;

    LOS_SpinLock(&g_pmSpin);
    pm->isWake = TRUE;
    LOS_SpinUnlock(&g_pmSpin);
    return;
}
/// 获取电源模式
LOS_SysSleepEnum LOS_PmModeGet(VOID)
{
    LOS_SysSleepEnum mode;
    LosPmCB *pm = &g_pmCB;

    LOS_SpinLock(&g_pmSpin);
    mode = pm->pmMode;
    LOS_SpinUnlock(&g_pmSpin);

    return mode;
}
UINT32 LOS_PmModeSet(LOS_SysSleepEnum mode)
{
    LosPmCB *pm = &g_pmCB;
    INT32 sleepMode = (INT32)mode;

    if ((sleepMode < 0) || (sleepMode > LOS_SYS_SHUTDOWN)) {
        return LOS_EINVAL;
    }

    LOS_SpinLock(&g_pmSpin);
    if ((mode == LOS_SYS_LIGHT_SLEEP) && (pm->sysctrl->lightSuspend == NULL)) {
        LOS_SpinUnlock(&g_pmSpin);
        return LOS_EINVAL;
    }

    if ((mode == LOS_SYS_DEEP_SLEEP) && (pm->sysctrl->deepSuspend == NULL)) {
        LOS_SpinUnlock(&g_pmSpin);
        return LOS_EINVAL;
    }

    if ((mode == LOS_SYS_SHUTDOWN) && (pm->sysctrl->shutdownSuspend == NULL)) {
        LOS_SpinUnlock(&g_pmSpin);
        return LOS_EINVAL;
    }

    pm->pmMode = mode;
    LOS_SpinUnlock(&g_pmSpin);

    return LOS_OK;
}

///获取电源锁数量
UINT32 LOS_PmLockCountGet(VOID)
{
    UINT16 count;
    LosPmCB *pm = &g_pmCB;

    LOS_SpinLock(&g_pmSpin);
    count = pm->lock;
    LOS_SpinUnlock(&g_pmSpin);

    return (UINT32)count;
}
///显示所有电源锁信息
VOID LOS_PmLockInfoShow(struct SeqBuf *m)
{
    LosPmCB *pm = &g_pmCB;
    OsPmLockCB *lock = NULL;
    LOS_DL_LIST *head = &pm->lockList;
    LOS_DL_LIST *list = head->pstNext;

    LOS_SpinLock(&g_pmSpin);
    while (list != head) {//遍历链表
        lock = LOS_DL_LIST_ENTRY(list, OsPmLockCB, list);//获取 OsPmLockCB 实体
        PM_INFO_SHOW(m, "%-30s%5u\n\r", lock->name, lock->count); //打印名称和数量
        list = list->pstNext;
    }
    LOS_SpinUnlock(&g_pmSpin);

    return;
}
///请求获取指定的锁
UINT32 OsPmLockRequest(const CHAR *name, UINT32 swtmrID)
{
    INT32 len;
    errno_t err;
    UINT32 ret = LOS_EINVAL;
    LosPmCB *pm = &g_pmCB;
    OsPmLockCB *lock = NULL;
    LOS_DL_LIST *head = &pm->lockList;
    LOS_DL_LIST *list = head->pstNext;

    if (OS_INT_ACTIVE) {
        return LOS_EINTR;
    }

    len = strlen(name);
    if (len <= 0) {
        return LOS_EINVAL;
    }

    LOS_SpinLock(&g_pmSpin);
    if (pm->lock >= OS_PM_LOCK_MAX) {
        LOS_SpinUnlock(&g_pmSpin);
        return LOS_EINVAL;
    }
	//遍历找到参数对应的 OsPmLockCB 
    while (list != head) {
        OsPmLockCB *listNode = LOS_DL_LIST_ENTRY(list, OsPmLockCB, list);
        if (strcmp(name, listNode->name) == 0) {
            lock = listNode;
            break;
        }

        list = list->pstNext;
    }
	
    if (lock == NULL) {//没有记录则创建记录
        lock = LOS_MemAlloc((VOID *)OS_SYS_MEM_ADDR, sizeof(OsPmLockCB));
        if (lock == NULL) {
            LOS_SpinUnlock(&g_pmSpin);
            return LOS_ENOMEM;
        }

        err = memcpy_s(lock->name, OS_PM_LOCK_NAME_MAX, name, len + 1);
        if (err != EOK) {
            LOS_SpinUnlock(&g_pmSpin);
            (VOID)LOS_MemFree((VOID *)OS_SYS_MEM_ADDR, lock);
            return err;
        }
        lock->count = 1;//数量增加
        lock->swtmrID = swtmrID;
        LOS_ListTailInsert(head, &lock->list);//从尾部插入链表中
    } else if (lock->count < OS_PM_LOCK_MAX) {
        lock->count++;//存在记录时,数量增加
    }

    if ((lock->swtmrID != OS_INVALID) && (lock->count > 1)) {
        lock->count--;
        LOS_SpinUnlock(&g_pmSpin);
        return LOS_EINVAL;
    }

    if (pm->lock < OS_PM_LOCK_MAX) {
    	pm->lock++;//总数量增加
        ret = LOS_OK;
    }

    LOS_SpinUnlock(&g_pmSpin);
    return ret;
}

UINT32 LOS_PmLockRequest(const CHAR *name)
{
    if (name == NULL) {
        return LOS_EINVAL;
    }

    return OsPmLockRequest(name, OS_INVALID);
}
//释放方式跟系统描述符(sysfd)很像
UINT32 LOS_PmLockRelease(const CHAR *name)
{
    UINT32 ret = LOS_EINVAL;
    LosPmCB *pm = &g_pmCB;
    OsPmLockCB *lock = NULL;
    LOS_DL_LIST *head = &pm->lockList;
    LOS_DL_LIST *list = head->pstNext;
    OsPmLockCB *lockFree = NULL;
    BOOL isRelease = FALSE;
    UINT32 mode;

    if (name == NULL) {
        return LOS_EINVAL;
    }

    if (OS_INT_ACTIVE) {
        return LOS_EINTR;
    }

    LOS_SpinLock(&g_pmSpin);
    if (pm->lock == 0) {
        LOS_SpinUnlock(&g_pmSpin);
        return LOS_EINVAL;
    }

    mode = (UINT32)pm->pmMode;
    while (list != head) {//遍历找到参数对应的 OsPmLockCB
        OsPmLockCB *listNode = LOS_DL_LIST_ENTRY(list, OsPmLockCB, list);
        if (strcmp(name, listNode->name) == 0) {
            lock = listNode;
            break;//找到返回
        }

        list = list->pstNext;//继续遍历下一个结点
    }

    if (lock == NULL) {
        LOS_SpinUnlock(&g_pmSpin);
        return LOS_EINVAL;
    } else if (lock->count > 0) {//有记录且有数量
        lock->count--;	//数量减少
        if (lock->count == 0) {//没有了
            LOS_ListDelete(&lock->list);//讲自己从链表中摘除
            lockFree = lock;
        }
    }

    if (pm->lock > 0) {
        pm->lock--;
        if (pm->lock == 0) {
            isRelease = TRUE;
        }
        ret = LOS_OK;
    }
    LOS_SpinUnlock(&g_pmSpin);

    if (lockFree != NULL) {
        (VOID)LOS_SwtmrDelete(lockFree->swtmrID);
        (VOID)LOS_MemFree((VOID *)OS_SYS_MEM_ADDR, lockFree);
    }

    if (isRelease && (mode > LOS_SYS_NORMAL_SLEEP)) {
        (VOID)LOS_EventWrite(&g_pmEvent, PM_EVENT_LOCK_RELEASE);
    }

    return ret;
}

STATIC VOID OsPmSwtmrHandler(UINT32 arg)
{
    const CHAR *name = (const CHAR *)arg;
    UINT32 ret = LOS_PmLockRelease(name);
    if (ret != LOS_OK) {
        PRINT_ERR("Pm delay lock %s release faled! : 0x%x\n", name, ret);
    }
}

UINT32 LOS_PmTimeLockRequest(const CHAR *name, UINT64 millisecond)
{
    UINT32 ticks;
    UINT16 swtmrID;
    UINT32 ret;

    if ((name == NULL) || !millisecond) {
        return LOS_EINVAL;
    }

    ticks = (UINT32)((millisecond + OS_MS_PER_TICK - 1) / OS_MS_PER_TICK);
#if (LOSCFG_BASE_CORE_SWTMR_ALIGN == 1)
    ret = LOS_SwtmrCreate(ticks, LOS_SWTMR_MODE_ONCE, OsPmSwtmrHandler, &swtmrID, (UINT32)(UINTPTR)name,
                          OS_SWTMR_ROUSES_ALLOW, OS_SWTMR_ALIGN_INSENSITIVE);
#else
    ret = LOS_SwtmrCreate(ticks, LOS_SWTMR_MODE_ONCE, OsPmSwtmrHandler, &swtmrID, (UINT32)(UINTPTR)name);
#endif
    if (ret != LOS_OK) {
        return ret;
    }

    ret = OsPmLockRequest(name, swtmrID);
    if (ret != LOS_OK) {
        (VOID)LOS_SwtmrDelete(swtmrID);
        return ret;
    }

    ret = LOS_SwtmrStart(swtmrID);
    if (ret != LOS_OK) {
        (VOID)LOS_PmLockRelease(name);
    }

    return ret;
}

UINT32 LOS_PmReadLock(VOID)
{
    UINT32 ret = LOS_EventRead(&g_pmEvent, PM_EVENT_LOCK_MASK, LOS_WAITMODE_OR | LOS_WAITMODE_CLR, LOS_WAIT_FOREVER);
    if (ret > PM_EVENT_LOCK_MASK) {
        PRINT_ERR("%s event read failed! ERROR: 0x%x\n", __FUNCTION__, ret);
    }

    return LOS_OK;
}

UINT32 LOS_PmSuspend(UINT32 wakeCount)
{
    (VOID)wakeCount;
    return OsPmSuspendSleep(&g_pmCB);
}

BOOL OsIsPmMode(VOID)
{
    LosPmCB *pm = &g_pmCB;

    LOS_SpinLock(&g_pmSpin);
    if ((pm->sysMode != LOS_SYS_NORMAL_SLEEP) && (pm->lock == 0)) {
        LOS_SpinUnlock(&g_pmSpin);
        return TRUE;
    }
    LOS_SpinUnlock(&g_pmSpin);
    return FALSE;
}

STATIC UINT32 OsPmSuspendDefaultHandler(VOID)
{
    PRINTK("Enter pm default handler!!!\n");
    WFI;
    return LOS_OK;
}

STATIC VOID OsPmSysctrlInit(VOID)
{
    /* Default handler functions, which are implemented by the product */
    g_sysctrl.early = NULL;
    g_sysctrl.late = NULL;
    g_sysctrl.normalSuspend = OsPmSuspendDefaultHandler;
    g_sysctrl.normalResume = NULL;
    g_sysctrl.lightSuspend = OsPmSuspendDefaultHandler;
    g_sysctrl.lightResume = NULL;
    g_sysctrl.deepSuspend = OsPmSuspendDefaultHandler;
    g_sysctrl.deepResume = NULL;
    g_sysctrl.shutdownSuspend = NULL;
    g_sysctrl.shutdownResume = NULL;
}

UINT32 OsPmInit(VOID)
{
    LosPmCB *pm = &g_pmCB;

    (VOID)memset_s(pm, sizeof(LosPmCB), 0, sizeof(LosPmCB));//全局链表置0

    pm->pmMode = LOS_SYS_NORMAL_SLEEP;
    LOS_ListInit(&pm->lockList);//初始化链表
    (VOID)LOS_EventInit(&g_pmEvent);

    OsPmSysctrlInit();
    pm->sysctrl = &g_sysctrl;
    return LOS_OK;
}

LOS_MODULE_INIT(OsPmInit, LOS_INIT_LEVEL_KMOD_EXTENDED);//以扩展方式初始化电源管理模块
#endif
