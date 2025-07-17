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
    @note_link 
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
// 每时钟滴答对应的毫秒数 = 每秒毫秒数 / 系统时钟频率
#define OS_MS_PER_TICK (OS_SYS_MS_PER_SECOND / LOSCFG_BASE_CORE_TICK_PER_SECOND)

#ifdef LOSCFG_KERNEL_PM
// PM信息打印宏，根据缓冲区是否为空选择不同输出方式
#define PM_INFO_SHOW(seqBuf, arg...) do {                   \
    if (seqBuf != NULL) {                                   \
        (void)LosBufPrintf((struct SeqBuf *)seqBuf, ##arg); \
    } else {                                                \
        PRINTK(arg);                                        \
    }                                                       \
} while (0)

#define OS_PM_LOCK_MAX         0xFFFFU  // PM锁最大计数(65535)
#define OS_PM_LOCK_NAME_MAX    28       // PM锁名称最大长度
#define OS_PM_SYS_EARLY        1        // 系统早期挂起阶段
#define OS_PM_SYS_DEVICE_EARLY 2        // 设备早期挂起阶段

// 挂起函数指针类型定义，参数为休眠模式
typedef UINT32 (*Suspend)(UINT32 mode);

// PM锁控制块结构体
typedef struct {
    CHAR         name[OS_PM_LOCK_NAME_MAX];  // 锁名称
    UINT32       count;                      // 锁计数
    UINT32       swtmrID;                    // 软件定时器ID
    LOS_DL_LIST  list;                       // 链表节点
} OsPmLockCB;

// PM控制块结构体
typedef struct {
    LOS_SysSleepEnum  pmMode;               // PM设置的休眠模式
    LOS_SysSleepEnum  sysMode;              // 系统实际进入的休眠模式
    UINT16            lock;                 // PM锁计数
    BOOL              isWake;               // 唤醒标志
    LosPmDevice       *device;              // 设备PM接口
    LosPmSysctrl      *sysctrl;             // 系统控制PM接口
    UINT64            enterSleepTime;       // 进入休眠时间
    LOS_DL_LIST       lockList;             // PM锁链表
} LosPmCB;

#define PM_EVENT_LOCK_MASK    0xF           // PM事件锁掩码(15)
#define PM_EVENT_LOCK_RELEASE 0x1           // PM事件锁释放标志(1)
STATIC EVENT_CB_S g_pmEvent;                // PM事件控制块
STATIC LosPmCB g_pmCB;                      // PM控制块实例
STATIC SPIN_LOCK_INIT(g_pmSpin);            // PM自旋锁初始化
STATIC LosPmSysctrl g_sysctrl;              // 系统控制PM接口实例

STATIC VOID OsPmSysctrlInit(VOID);          // 系统控制PM接口初始化函数声明

/**
 * @brief 启动PM滴答定时器
 * @param pm PM控制块指针
 * @return 无
 */
STATIC VOID OsPmTickTimerStart(LosPmCB *pm)
{
    (VOID)pm;  // 未使用参数，显式转换避免编译警告
    return;
}

/**
 * @brief 停止PM滴答定时器
 * @param pm PM控制块指针
 * @return 是否成功停止定时器
 */
STATIC BOOL OsPmTickTimerStop(LosPmCB *pm)
{
    (VOID)pm;  // 未使用参数，显式转换避免编译警告
    return FALSE;  // 默认返回FALSE，表示未停止
}

/**
 * @brief CPU从休眠模式恢复
 * @param pm PM控制块指针
 * @return 无
 */
STATIC VOID OsPmCpuResume(LosPmCB *pm)
{
    // 根据系统实际休眠模式调用对应恢复函数
    if ((pm->sysMode == LOS_SYS_NORMAL_SLEEP) && (pm->sysctrl->normalResume != NULL)) {
        pm->sysctrl->normalResume();  // 正常休眠恢复
    } else if ((pm->sysMode == LOS_SYS_LIGHT_SLEEP) && (pm->sysctrl->lightResume != NULL)) {
        pm->sysctrl->lightResume();   // 轻度休眠恢复
    } else if ((pm->sysMode == LOS_SYS_DEEP_SLEEP) && (pm->sysctrl->deepResume != NULL)) {
        pm->sysctrl->deepResume();    // 深度休眠恢复
    }
}

/**
 * @brief CPU进入休眠模式
 * @param pm PM控制块指针
 * @return 无
 */
VOID OsPmCpuSuspend(LosPmCB *pm)
{
    /* cpu enter low power mode */
    LOS_ASSERT(pm->sysctrl != NULL);  // 断言系统控制接口不为空

    // 根据系统实际休眠模式调用对应挂起函数
    if (pm->sysMode == LOS_SYS_NORMAL_SLEEP) {
        pm->sysctrl->normalSuspend();  // 正常休眠
    } else if (pm->sysMode == LOS_SYS_LIGHT_SLEEP) {
        pm->sysctrl->lightSuspend();   // 轻度休眠
    } else if (pm->sysMode == LOS_SYS_DEEP_SLEEP) {
        pm->sysctrl->deepSuspend();    // 深度休眠
    } else {
        pm->sysctrl->shutdownSuspend(); // 关机模式
    }
}

/**
 * @brief 休眠恢复准备工作
 * @param pm PM控制块指针
 * @param mode 休眠模式
 * @param prepare 准备阶段标志
 * @return 无
 */
STATIC VOID OsPmResumePrepare(LosPmCB *pm, UINT32 mode, UINT32 prepare)
{
    // 如果准备阶段为0且设备恢复接口存在，则调用设备恢复
    if ((prepare == 0) && (pm->device != NULL) && (pm->device->resume != NULL)) {
        pm->device->resume(mode);  // 调用设备恢复函数
    }

    // 如果准备阶段为0或设备早期阶段且系统后期恢复接口存在，则调用系统后期恢复
    if (((prepare == 0) || (prepare == OS_PM_SYS_DEVICE_EARLY)) && (pm->sysctrl->late != NULL)) {
        pm->sysctrl->late(mode);  // 调用系统后期恢复函数
    }
}

/**
 * @brief 休眠准备工作
 * @param sysSuspendEarly   系统早期挂起函数
 * @param deviceSuspend     设备挂起函数
 * @param mode              休眠模式
 * @param prepare           准备阶段标志指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 OsPmSuspendPrepare(Suspend sysSuspendEarly, Suspend deviceSuspend, UINT32 mode, UINT32 *prepare)
{
    UINT32 ret;

    // 如果系统早期挂起函数存在，则调用系统早期挂起
    if (sysSuspendEarly != NULL) {
        ret = sysSuspendEarly(mode);
        if (ret != LOS_OK) {
            *prepare = OS_PM_SYS_EARLY;  // 设置准备阶段为系统早期
            return ret;
        }
    }

    // 如果设备挂起函数存在，则调用设备挂起
    if (deviceSuspend != NULL) {
        ret = deviceSuspend(mode);
        if (ret != LOS_OK) {
            *prepare = OS_PM_SYS_DEVICE_EARLY;  // 设置准备阶段为设备早期
            return ret;
        }
    }

    return LOS_OK;
}

/**
 * @brief 休眠前检查
 * @param pm                PM控制块指针
 * @param sysSuspendEarly   系统早期挂起函数指针
 * @param deviceSuspend     设备挂起函数指针
 * @param mode              休眠模式指针
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATIC UINT32 OsPmSuspendCheck(LosPmCB *pm, Suspend *sysSuspendEarly, Suspend *deviceSuspend, LOS_SysSleepEnum *mode)
{
    LOS_SpinLock(&g_pmSpin);  // 获取PM自旋锁
    pm->sysMode = pm->pmMode; // 设置系统实际休眠模式为PM设置的模式
    if (pm->lock > 0) {       // 如果PM锁计数大于0
        pm->sysMode = LOS_SYS_NORMAL_SLEEP;  // 强制设置为正常休眠模式
        LOS_SpinUnlock(&g_pmSpin);           // 释放自旋锁
        return LOS_NOK;                      // 返回失败
    }

    pm->isWake = FALSE;                       // 清除唤醒标志
    *mode = pm->sysMode;                      // 输出休眠模式
    *sysSuspendEarly = pm->sysctrl->early;    // 输出系统早期挂起函数
    if (pm->device != NULL) {
        *deviceSuspend = pm->device->suspend; // 输出设备挂起函数
    } else {
        *deviceSuspend = NULL;                // 设备接口不存在则置空
    }
    LOS_SpinUnlock(&g_pmSpin);                // 释放自旋锁
    return LOS_OK;                            // 返回成功
}

/**
 * @brief 执行系统休眠
 * @param pm PM控制块指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 OsPmSuspendSleep(LosPmCB *pm)
{
    UINT32 ret, intSave;                      // ret:返回值，intSave:中断状态
    Suspend sysSuspendEarly, deviceSuspend;   // 挂起函数指针
    LOS_SysSleepEnum mode;                    // 休眠模式
    UINT32 prepare = 0;                       // 准备阶段标志
    BOOL tickTimerStop = FALSE;               // 滴答定时器停止标志

    // 休眠前检查
    ret = OsPmSuspendCheck(pm, &sysSuspendEarly, &deviceSuspend, &mode);
    if (ret != LOS_OK) {
        PRINT_ERR("Pm suspend mode is normal sleep! lock count %d\n", pm->lock);  // 打印锁计数不为0的错误
        return ret;
    }

    // 执行休眠准备工作
    ret = OsPmSuspendPrepare(sysSuspendEarly, deviceSuspend, (UINT32)mode, &prepare);
    if (ret != LOS_OK) {
        LOS_SpinLockSave(&g_pmSpin, &intSave);  // 获取自旋锁并保存中断状态
        LOS_TaskLock();                         // 任务调度锁
        goto EXIT;                              // 跳转到退出处理
    }

    LOS_SpinLockSave(&g_pmSpin, &intSave);      // 获取自旋锁并保存中断状态
    LOS_TaskLock();                             // 任务调度锁
    if (pm->isWake || (pm->lock > 0)) {         // 如果已唤醒或锁计数大于0
        goto EXIT;                              // 跳转到退出处理
    }

    tickTimerStop = OsPmTickTimerStop(pm);      // 停止滴答定时器
    if (!tickTimerStop) {
        OsSchedResponseTimeReset(0);            // 重置调度响应时间
        OsSchedExpireTimeUpdate();              // 更新调度过期时间
    }

    OsPmCpuSuspend(pm);                         // CPU进入休眠

    OsPmCpuResume(pm);                          // CPU从休眠恢复

    OsPmTickTimerStart(pm);                     // 启动滴答定时器

EXIT:
    pm->sysMode = LOS_SYS_NORMAL_SLEEP;         // 恢复系统模式为正常
    OsPmResumePrepare(pm, (UINT32)mode, prepare); // 执行恢复准备工作

    LOS_SpinUnlockRestore(&g_pmSpin, intSave);  // 释放自旋锁并恢复中断
    LOS_TaskUnlock();                           // 释放任务调度锁
    return ret;
}

/**
 * @brief 注册PM设备接口
 * @param pm      PM控制块指针
 * @param device  设备PM接口
 * @return 成功返回LOS_OK，失败返回LOS_EINVAL
 */
STATIC UINT32 OsPmDeviceRegister(LosPmCB *pm, LosPmDevice *device)
{
    // 检查设备挂起和恢复函数是否存在
    if ((device->suspend == NULL) || (device->resume == NULL)) {
        return LOS_EINVAL;  // 参数无效
    }

    LOS_SpinLock(&g_pmSpin);    // 获取自旋锁
    pm->device = device;        // 设置设备接口
    LOS_SpinUnlock(&g_pmSpin);  // 释放自旋锁

    return LOS_OK;
}

/**
 * @brief 注册PM系统控制接口
 * @param pm      PM控制块指针
 * @param sysctrl 系统控制PM接口
 * @return 成功返回LOS_OK
 */
STATIC UINT32 OsPmSysctrlRegister(LosPmCB *pm, LosPmSysctrl *sysctrl)
{
    LOS_SpinLock(&g_pmSpin);    // 获取自旋锁
    // 注册系统早期挂起函数
    if (sysctrl->early != NULL) {
        pm->sysctrl->early = sysctrl->early;
    }
    // 注册系统后期恢复函数
    if (sysctrl->late != NULL) {
        pm->sysctrl->late = sysctrl->late;
    }
    // 注册正常休眠挂起函数
    if (sysctrl->normalSuspend != NULL) {
        pm->sysctrl->normalSuspend = sysctrl->normalSuspend;
    }
    // 注册正常休眠恢复函数
    if (sysctrl->normalResume != NULL) {
        pm->sysctrl->normalResume = sysctrl->normalResume;
    }
    // 注册轻度休眠挂起函数
    if (sysctrl->lightSuspend != NULL) {
        pm->sysctrl->lightSuspend = sysctrl->lightSuspend;
    }
    // 注册轻度休眠恢复函数
    if (sysctrl->lightResume != NULL) {
        pm->sysctrl->lightResume = sysctrl->lightResume;
    }
    // 注册深度休眠挂起函数
    if (sysctrl->deepSuspend != NULL) {
        pm->sysctrl->deepSuspend = sysctrl->deepSuspend;
    }
    // 注册深度休眠恢复函数
    if (sysctrl->deepResume != NULL) {
        pm->sysctrl->deepResume = sysctrl->deepResume;
    }
    // 注册关机挂起函数
    if (sysctrl->shutdownSuspend != NULL) {
        pm->sysctrl->shutdownSuspend = sysctrl->shutdownSuspend;
    }
    // 注册关机恢复函数
    if (sysctrl->shutdownResume != NULL) {
        pm->sysctrl->shutdownResume = sysctrl->shutdownResume;
    }
    LOS_SpinUnlock(&g_pmSpin);  // 释放自旋锁

    return LOS_OK;
}

/**
 * @brief 注册PM节点
 * @param type 节点类型
 * @param node 节点指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 LOS_PmRegister(LOS_PmNodeType type, VOID *node)
{
    LosPmCB *pm = &g_pmCB;      // 获取PM控制块实例

    if (node == NULL) {
        return LOS_EINVAL;      // 参数无效
    }

    switch (type) {
        case LOS_PM_TYPE_DEVICE:       // 设备类型
            return OsPmDeviceRegister(pm, (LosPmDevice *)node);
        case LOS_PM_TYPE_TICK_TIMER:   // 滴答定时器类型(不支持)
            PRINT_ERR("Pm, %d is an unsupported type\n", type);
            return LOS_EINVAL;
        case LOS_PM_TYPE_SYSCTRL:      // 系统控制类型
            return OsPmSysctrlRegister(pm, (LosPmSysctrl *)node);
        default:                       // 未知类型
            break;
    }

    return LOS_EINVAL;
}

/**
 * @brief 注销PM设备接口
 * @param pm      PM控制块指针
 * @param device  设备PM接口
 * @return 成功返回LOS_OK，失败返回LOS_EINVAL
 */
STATIC UINT32 OsPmDeviceUnregister(LosPmCB *pm, const LosPmDevice *device)
{
    LOS_SpinLock(&g_pmSpin);    // 获取自旋锁
    if (pm->device == device) { // 如果是当前注册的设备
        pm->device = NULL;      // 清除设备接口
        pm->pmMode = LOS_SYS_NORMAL_SLEEP;  // 恢复为正常休眠模式
        LOS_SpinUnlock(&g_pmSpin);          // 释放自旋锁
        return LOS_OK;
    }

    LOS_SpinUnlock(&g_pmSpin);  // 释放自旋锁
    return LOS_EINVAL;          // 设备未注册
}

/**
 * @brief 注销PM系统控制接口
 * @param pm      PM控制块指针
 * @param sysctrl 系统控制PM接口
 * @return 成功返回LOS_OK
 */
STATIC UINT32 OsPmSysctrlUnregister(LosPmCB *pm, LosPmSysctrl *sysctrl)
{
    (VOID)sysctrl;              // 未使用参数
    LOS_SpinLock(&g_pmSpin);    // 获取自旋锁
    OsPmSysctrlInit();          // 重新初始化系统控制接口
    pm->pmMode = LOS_SYS_NORMAL_SLEEP;  // 恢复为正常休眠模式
    LOS_SpinUnlock(&g_pmSpin);  // 释放自旋锁

    return LOS_OK;
}

/**
 * @brief 注销PM节点
 * @param type 节点类型
 * @param node 节点指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 LOS_PmUnregister(LOS_PmNodeType type, VOID *node)
{
    LosPmCB *pm = &g_pmCB;      // 获取PM控制块实例

    if (node == NULL) {
        return LOS_EINVAL;      // 参数无效
    }

    switch (type) {
        case LOS_PM_TYPE_DEVICE:       // 设备类型
            return OsPmDeviceUnregister(pm, (LosPmDevice *)node);
        case LOS_PM_TYPE_TICK_TIMER:   // 滴答定时器类型(不支持)
            PRINT_ERR("Pm, %d is an unsupported type\n", type);
            return LOS_EINVAL;
        case LOS_PM_TYPE_SYSCTRL:      // 系统控制类型
            return OsPmSysctrlUnregister(pm, (LosPmSysctrl *)node);
        default:                       // 未知类型
            break;
    }

    return LOS_EINVAL;
}

/**
 * @brief 设置PM唤醒标志
 * @return 无
 */
VOID LOS_PmWakeSet(VOID)
{
    LosPmCB *pm = &g_pmCB;      // 获取PM控制块实例

    LOS_SpinLock(&g_pmSpin);    // 获取自旋锁
    pm->isWake = TRUE;          // 设置唤醒标志为TRUE
    LOS_SpinUnlock(&g_pmSpin);  // 释放自旋锁
    return;
}

/**
 * @brief 获取当前PM设置的休眠模式
 * @return 休眠模式
 */
LOS_SysSleepEnum LOS_PmModeGet(VOID)
{
    LOS_SysSleepEnum mode;      // 休眠模式
    LosPmCB *pm = &g_pmCB;      // 获取PM控制块实例

    LOS_SpinLock(&g_pmSpin);    // 获取自旋锁
    mode = pm->pmMode;          // 获取PM设置的模式
    LOS_SpinUnlock(&g_pmSpin);  // 释放自旋锁

    return mode;
}

/**
 * @brief 设置PM休眠模式
 * @param mode 要设置的休眠模式
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 LOS_PmModeSet(LOS_SysSleepEnum mode)
{
    LosPmCB *pm = &g_pmCB;      // 获取PM控制块实例
    INT32 sleepMode = (INT32)mode;  // 转换为有符号类型便于范围检查

    // 检查模式是否在有效范围内
    if ((sleepMode < 0) || (sleepMode > LOS_SYS_SHUTDOWN)) {
        return LOS_EINVAL;      // 参数无效
    }

    LOS_SpinLock(&g_pmSpin);    // 获取自旋锁
    // 检查轻度休眠模式是否支持
    if ((mode == LOS_SYS_LIGHT_SLEEP) && (pm->sysctrl->lightSuspend == NULL)) {
        LOS_SpinUnlock(&g_pmSpin);      // 释放自旋锁
        return LOS_EINVAL;              // 不支持轻度休眠
    }

    // 检查深度休眠模式是否支持
    if ((mode == LOS_SYS_DEEP_SLEEP) && (pm->sysctrl->deepSuspend == NULL)) {
        LOS_SpinUnlock(&g_pmSpin);      // 释放自旋锁
        return LOS_EINVAL;              // 不支持深度休眠
    }

    // 检查关机模式是否支持
    if ((mode == LOS_SYS_SHUTDOWN) && (pm->sysctrl->shutdownSuspend == NULL)) {
        LOS_SpinUnlock(&g_pmSpin);      // 释放自旋锁
        return LOS_EINVAL;              // 不支持关机模式
    }

    pm->pmMode = mode;          // 设置PM休眠模式
    LOS_SpinUnlock(&g_pmSpin);  // 释放自旋锁

    return LOS_OK;
}

/**
 * @brief 获取PM锁计数
 * @return PM锁计数值
 */
UINT32 LOS_PmLockCountGet(VOID)
{
    UINT16 count;               // 锁计数
    LosPmCB *pm = &g_pmCB;      // 获取PM控制块实例

    LOS_SpinLock(&g_pmSpin);    // 获取自旋锁
    count = pm->lock;           // 获取锁计数
    LOS_SpinUnlock(&g_pmSpin);  // 释放自旋锁

    return (UINT32)count;       // 返回锁计数
}

/**
 * @brief 显示PM锁信息
 * @param m 序列缓冲区指针
 * @return 无
 */
VOID LOS_PmLockInfoShow(struct SeqBuf *m)
{
    LosPmCB *pm = &g_pmCB;      // 获取PM控制块实例
    OsPmLockCB *lock = NULL;    // PM锁控制块指针
    LOS_DL_LIST *head = &pm->lockList;  // 锁链表头
    LOS_DL_LIST *list = head->pstNext;  // 链表当前节点

    LOS_SpinLock(&g_pmSpin);    // 获取自旋锁
    while (list != head) {      // 遍历锁链表
        lock = LOS_DL_LIST_ENTRY(list, OsPmLockCB, list);  // 获取锁节点
        PM_INFO_SHOW(m, "%-30s%5u\n\r", lock->name, lock->count);  // 打印锁名称和计数
        list = list->pstNext;   // 移动到下一个节点
    }
    LOS_SpinUnlock(&g_pmSpin);  // 释放自旋锁

    return;
}

/**
 * @brief 请求PM锁(内部实现)
 * @param name    锁名称
 * @param swtmrID 软件定时器ID
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsPmLockRequest(const CHAR *name, UINT32 swtmrID)
{
    INT32 len;                  // 名称长度
    errno_t err;                // 错误码
    UINT32 ret = LOS_EINVAL;    // 返回值
    LosPmCB *pm = &g_pmCB;      // 获取PM控制块实例
    OsPmLockCB *lock = NULL;    // PM锁控制块指针
    LOS_DL_LIST *head = &pm->lockList;  // 锁链表头
    LOS_DL_LIST *list = head->pstNext;  // 链表当前节点

    if (OS_INT_ACTIVE) {        // 如果在中断上下文中
        return LOS_EINTR;       // 不允许在中断中请求锁
    }

    len = strlen(name);         // 获取名称长度
    if (len <= 0) {             // 名称为空
        return LOS_EINVAL;      // 参数无效
    }

    LOS_SpinLock(&g_pmSpin);    // 获取自旋锁
    if (pm->lock >= OS_PM_LOCK_MAX) {  // 锁计数达到最大值
        LOS_SpinUnlock(&g_pmSpin);      // 释放自旋锁
        return LOS_EINVAL;              // 无法获取更多锁
    }

    // 查找同名锁
    while (list != head) {
        OsPmLockCB *listNode = LOS_DL_LIST_ENTRY(list, OsPmLockCB, list);
        if (strcmp(name, listNode->name) == 0) {  // 找到同名锁
            lock = listNode;
            break;
        }

        list = list->pstNext;   // 移动到下一个节点
    }

    if (lock == NULL) {         // 未找到同名锁，创建新锁
        lock = LOS_MemAlloc((VOID *)OS_SYS_MEM_ADDR, sizeof(OsPmLockCB));  // 分配内存
        if (lock == NULL) {     // 内存分配失败
            LOS_SpinUnlock(&g_pmSpin);  // 释放自旋锁
            return LOS_ENOMEM;          // 内存不足
        }
        err = memcpy_s(lock->name, OS_PM_LOCK_NAME_MAX, name, len + 1);  // 复制锁名称
        if (err != EOK) {       // 复制失败
            LOS_SpinUnlock(&g_pmSpin);  // 释放自旋锁
            (VOID)LOS_MemFree((VOID *)OS_SYS_MEM_ADDR, lock);  // 释放已分配内存
            return err;         // 返回错误码
        }
        lock->count = 1;        // 初始化锁计数为1
        lock->swtmrID = swtmrID;// 设置定时器ID
        LOS_ListTailInsert(head, &lock->list);  // 添加到链表尾部
    } else if (lock->count < OS_PM_LOCK_MAX) {  // 找到同名锁且计数未达上限
        lock->count++;          // 锁计数加1
    }

    // 如果是定时锁且计数大于1，不允许重入
    if ((lock->swtmrID != OS_INVALID) && (lock->count > 1)) {
        lock->count--;          // 恢复锁计数
        LOS_SpinUnlock(&g_pmSpin);  // 释放自旋锁
        return LOS_EINVAL;      // 返回错误
    }

    if (pm->lock < OS_PM_LOCK_MAX) {  // 全局锁计数未达上限
        pm->lock++;             // 全局锁计数加1
        ret = LOS_OK;           // 返回成功
    }

    LOS_SpinUnlock(&g_pmSpin);  // 释放自旋锁
    return ret;
}

/**
 * @brief 请求PM锁
 * @param name 锁名称
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 LOS_PmLockRequest(const CHAR *name)
{
    if (name == NULL) {
        return LOS_EINVAL;      // 参数无效
    }

    return OsPmLockRequest(name, OS_INVALID);  // 调用内部实现，定时器ID无效
}

/**
 * @brief 释放PM锁
 * @param name 锁名称
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 LOS_PmLockRelease(const CHAR *name)
{
    UINT32 ret = LOS_EINVAL;    // 返回值
    LosPmCB *pm = &g_pmCB;      // 获取PM控制块实例
    OsPmLockCB *lock = NULL;    // PM锁控制块指针
    LOS_DL_LIST *head = &pm->lockList;  // 锁链表头
    LOS_DL_LIST *list = head->pstNext;  // 链表当前节点
    OsPmLockCB *lockFree = NULL;        // 待释放的锁
    BOOL isRelease = FALSE;     // 是否完全释放标志
    UINT32 mode;                // 休眠模式

    if (name == NULL) {
        return LOS_EINVAL;      // 参数无效
    }

    if (OS_INT_ACTIVE) {        // 如果在中断上下文中
        return LOS_EINTR;       // 不允许在中断中释放锁
    }

    LOS_SpinLock(&g_pmSpin);    // 获取自旋锁
    if (pm->lock == 0) {        // 锁计数已为0
        LOS_SpinUnlock(&g_pmSpin);  // 释放自旋锁
        return LOS_EINVAL;      // 无锁可释放
    }

    mode = (UINT32)pm->pmMode;  // 获取当前休眠模式
    // 查找同名锁
    while (list != head) {
        OsPmLockCB *listNode = LOS_DL_LIST_ENTRY(list, OsPmLockCB, list);
        if (strcmp(name, listNode->name) == 0) {  // 找到同名锁
            lock = listNode;
            break;
        }

        list = list->pstNext;   // 移动到下一个节点
    }

    if (lock == NULL) {         // 未找到同名锁
        LOS_SpinUnlock(&g_pmSpin);  // 释放自旋锁
        return LOS_EINVAL;      // 锁不存在
    } else if (lock->count > 0) {  // 锁计数大于0
        lock->count--;          // 锁计数减1
        if (lock->count == 0) { // 锁计数减为0
            LOS_ListDelete(&lock->list);  // 从链表中删除
            lockFree = lock;    // 标记为待释放
        }
    }

    if (pm->lock > 0) {         // 全局锁计数大于0
        pm->lock--;             // 全局锁计数减1
        if (pm->lock == 0) {    // 全局锁计数减为0
            isRelease = TRUE;   // 设置完全释放标志
        }
        ret = LOS_OK;           // 返回成功
    }
    LOS_SpinUnlock(&g_pmSpin);  // 释放自旋锁

    if (lockFree != NULL) {     // 如果有锁需要释放
        (VOID)LOS_SwtmrDelete(lockFree->swtmrID);  // 删除定时器
        (VOID)LOS_MemFree((VOID *)OS_SYS_MEM_ADDR, lockFree);  // 释放锁内存
    }

    // 如果完全释放且当前模式不是正常休眠
    if (isRelease && (mode > LOS_SYS_NORMAL_SLEEP)) {
        (VOID)LOS_EventWrite(&g_pmEvent, PM_EVENT_LOCK_RELEASE);  // 写入锁释放事件
    }

    return ret;
}

/**
 * @brief PM定时锁超时处理函数
 * @param arg 锁名称
 * @return 无
 */
STATIC VOID OsPmSwtmrHandler(UINT32 arg)
{
    const CHAR *name = (const CHAR *)arg;  // 转换为锁名称
    UINT32 ret = LOS_PmLockRelease(name);  // 释放PM锁
    if (ret != LOS_OK) {
        PRINT_ERR("Pm delay lock %s release faled! : 0x%x\n", name, ret);  // 打印释放失败信息
    }
}

/**
 * @brief 请求PM定时锁
 * @param name        锁名称
 * @param millisecond 锁定时间(毫秒)
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 LOS_PmTimeLockRequest(const CHAR *name, UINT64 millisecond)
{
    UINT32 ticks;               // 转换后的时钟滴答数
    UINT16 swtmrID;             // 软件定时器ID
    UINT32 ret;

    if ((name == NULL) || !millisecond) {  // 参数检查
        return LOS_EINVAL;      // 参数无效
    }

    // 将毫秒转换为时钟滴答数(向上取整)
    ticks = (UINT32)((millisecond + OS_MS_PER_TICK - 1) / OS_MS_PER_TICK);
#if (LOSCFG_BASE_CORE_SWTMR_ALIGN == 1)
    // 创建对齐的软件定时器
    ret = LOS_SwtmrCreate(ticks, LOS_SWTMR_MODE_ONCE, OsPmSwtmrHandler, &swtmrID, (UINT32)(UINTPTR)name,
                          OS_SWTMR_ROUSES_ALLOW, OS_SWTMR_ALIGN_INSENSITIVE);
#else
    // 创建普通软件定时器
    ret = LOS_SwtmrCreate(ticks, LOS_SWTMR_MODE_ONCE, OsPmSwtmrHandler, &swtmrID, (UINT32)(UINTPTR)name);
#endif
    if (ret != LOS_OK) {
        return ret;             // 定时器创建失败
    }

    ret = OsPmLockRequest(name, swtmrID);  // 请求PM锁
    if (ret != LOS_OK) {
        (VOID)LOS_SwtmrDelete(swtmrID);    // 锁请求失败，删除定时器
        return ret;
    }

    ret = LOS_SwtmrStart(swtmrID);         // 启动定时器
    if (ret != LOS_OK) {
        (VOID)LOS_PmLockRelease(name);     // 定时器启动失败，释放锁
    }

    return ret;
}

/**
 * @brief 读取PM锁事件
 * @return 成功返回LOS_OK
 */
UINT32 LOS_PmReadLock(VOID)
{
    // 读取PM事件，等待锁释放事件
    UINT32 ret = LOS_EventRead(&g_pmEvent, PM_EVENT_LOCK_MASK, LOS_WAITMODE_OR | LOS_WAITMODE_CLR, LOS_WAIT_FOREVER);
    if (ret > PM_EVENT_LOCK_MASK) {        // 事件读取失败
        PRINT_ERR("%s event read failed! ERROR: 0x%x\n", __FUNCTION__, ret);
    }

    return LOS_OK;
}

/**
 * @brief 执行系统休眠
 * @param wakeCount 唤醒计数
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 LOS_PmSuspend(UINT32 wakeCount)
{
    (VOID)wakeCount;            // 未使用参数
    return OsPmSuspendSleep(&g_pmCB);  // 调用休眠实现函数
}

/**
 * @brief 检查是否处于PM模式
 * @return 是返回TRUE，否返回FALSE
 */
BOOL OsIsPmMode(VOID)
{
    LosPmCB *pm = &g_pmCB;      // 获取PM控制块实例

    LOS_SpinLock(&g_pmSpin);    // 获取自旋锁
    // 如果系统模式不是正常休眠且锁计数为0
    if ((pm->sysMode != LOS_SYS_NORMAL_SLEEP) && (pm->lock == 0)) {
        LOS_SpinUnlock(&g_pmSpin);  // 释放自旋锁
        return TRUE;               // 处于PM模式
    }
    LOS_SpinUnlock(&g_pmSpin);  // 释放自旋锁
    return FALSE;              // 不处于PM模式
}

/**
 * @brief PM默认挂起处理函数
 * @return 成功返回LOS_OK
 */
STATIC UINT32 OsPmSuspendDefaultHandler(VOID)
{
    PRINTK("Enter pm default handler!!!\n");  // 打印默认处理提示
    WFI;                        // 等待中断指令
    return LOS_OK;
}

/**
 * @brief 系统控制PM接口初始化
 * @return 无
 */
STATIC VOID OsPmSysctrlInit(VOID)
{
    /* Default handler functions, which are implemented by the product */
    g_sysctrl.early = NULL;                         // 系统早期挂起函数
    g_sysctrl.late = NULL;                          // 系统后期恢复函数
    g_sysctrl.normalSuspend = OsPmSuspendDefaultHandler;  // 正常休眠挂起函数
    g_sysctrl.normalResume = NULL;                  // 正常休眠恢复函数
    g_sysctrl.lightSuspend = OsPmSuspendDefaultHandler;   // 轻度休眠挂起函数
    g_sysctrl.lightResume = NULL;                   // 轻度休眠恢复函数
    g_sysctrl.deepSuspend = OsPmSuspendDefaultHandler;    // 深度休眠挂起函数
    g_sysctrl.deepResume = NULL;                    // 深度休眠恢复函数
    g_sysctrl.shutdownSuspend = NULL;               // 关机挂起函数
    g_sysctrl.shutdownResume = NULL;                // 关机恢复函数
}

/**
 * @brief PM模块初始化
 * @return 成功返回LOS_OK
 */
UINT32 OsPmInit(VOID)
{
    LosPmCB *pm = &g_pmCB;      // 获取PM控制块实例

    (VOID)memset_s(pm, sizeof(LosPmCB), 0, sizeof(LosPmCB));  // 初始化PM控制块

    pm->pmMode = LOS_SYS_NORMAL_SLEEP;  // 默认休眠模式为正常休眠
    LOS_ListInit(&pm->lockList);        // 初始化锁链表
    (VOID)LOS_EventInit(&g_pmEvent);    // 初始化PM事件

    OsPmSysctrlInit();                  // 初始化系统控制接口
    pm->sysctrl = &g_sysctrl;           // 设置系统控制接口
    return LOS_OK;
}

// PM模块初始化，初始化级别为扩展内核模块
LOS_MODULE_INIT(OsPmInit, LOS_INIT_LEVEL_KMOD_EXTENDED);
#endif
