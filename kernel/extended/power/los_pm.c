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

typedef UINT32 (*Suspend)(VOID);
///< 电源锁控制块
typedef struct {
    CHAR         name[OS_PM_LOCK_NAME_MAX]; ///< 电源锁名称
    UINT32       count;	///< 数量
    LOS_DL_LIST  list;	///< 电源锁链表,上面挂的是 OsPmLockCB
} OsPmLockCB;
///< 电源管理控制块 
typedef struct {
    LOS_SysSleepEnum  mode;		///< 模式类型
    UINT16            lock;		///< 锁数量
    LOS_DL_LIST       lockList;	///< 电源锁链表头,上面挂的是 OsPmLockCB
} LosPmCB;

STATIC LosPmCB g_pmCB; ///< 电源控制块全局遍历
STATIC SPIN_LOCK_INIT(g_pmSpin); ///< 电源模块自旋锁
/// 获取电源模式
LOS_SysSleepEnum LOS_PmModeGet(VOID)
{
    LOS_SysSleepEnum mode;
    LosPmCB *pm = &g_pmCB;

    LOS_SpinLock(&g_pmSpin);
    mode = pm->mode;	//枚举类型,4种状态
    LOS_SpinUnlock(&g_pmSpin);

    return mode;
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
    UINT32 intSave;
    LosPmCB *pm = &g_pmCB;
    OsPmLockCB *lock = NULL;
    LOS_DL_LIST *head = &pm->lockList;
    LOS_DL_LIST *list = head->pstNext;

    intSave = LOS_IntLock();
    while (list != head) {//遍历链表
        lock = LOS_DL_LIST_ENTRY(list, OsPmLockCB, list);//获取 OsPmLockCB 实体
        PM_INFO_SHOW(m, "%-30s%5u\n\r", lock->name, lock->count); //打印名称和数量
        list = list->pstNext;
    }
    LOS_IntRestore(intSave);

    return;
}
///请求获取指定的锁
UINT32 LOS_PmLockRequest(const CHAR *name)
{
    INT32 len;
    errno_t err;
    LosPmCB *pm = &g_pmCB;
    OsPmLockCB *listNode = NULL;
    OsPmLockCB *lock = NULL;
    LOS_DL_LIST *head = &pm->lockList;
    LOS_DL_LIST *list = head->pstNext;

    if (name == NULL) {
        return LOS_EINVAL;
    }

    if (OS_INT_ACTIVE) {
        return LOS_EINTR;
    }

    len = strlen(name);
    if (len == 0) {
        return LOS_EINVAL;
    }

    LOS_SpinLock(&g_pmSpin);
    if (pm->lock >= OS_PM_LOCK_MAX) {
        LOS_SpinUnlock(&g_pmSpin);
        return LOS_EINVAL;
    }
	//遍历找到参数对应的 OsPmLockCB 
    while (list != head) {
        listNode = LOS_DL_LIST_ENTRY(list, OsPmLockCB, list);
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
        LOS_ListTailInsert(head, &lock->list);//从尾部插入链表中
    } else if (lock->count < OS_PM_LOCK_MAX) {
        lock->count++;//存在记录时,数量增加
    }

    pm->lock++;//总数量增加
    LOS_SpinUnlock(&g_pmSpin);
    return LOS_OK;
}
///释放指定锁,注意这里的释放指的是释放数量,当数量为0时才删除锁
//释放方式跟系统描述符(sysfd)很像
UINT32 LOS_PmLockRelease(const CHAR *name)
{
    LosPmCB *pm = &g_pmCB;
    OsPmLockCB *lock = NULL;
    OsPmLockCB *listNode = NULL;
    LOS_DL_LIST *head = &pm->lockList;
    LOS_DL_LIST *list = head->pstNext;
    VOID *lockFree = NULL;

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

    while (list != head) {//遍历找到参数对应的 OsPmLockCB
        listNode = LOS_DL_LIST_ENTRY(list, OsPmLockCB, list);
        if (strcmp(name, listNode->name) == 0) {
            lock = listNode;
            break;//找到返回
        }

        list = list->pstNext;//继续遍历下一个结点
    }

    if (lock == NULL) {
        LOS_SpinUnlock(&g_pmSpin);
        return LOS_EACCES;
    } else if (lock->count > 0) {//有记录且有数量
        lock->count--;	//数量减少
        if (lock->count == 0) {//没有了
            LOS_ListDelete(&lock->list);//讲自己从链表中摘除
            lockFree = lock;
        }
    }
    pm->lock--;//总数量减少
    LOS_SpinUnlock(&g_pmSpin);

    (VOID)LOS_MemFree((VOID *)OS_SYS_MEM_ADDR, lockFree);
    return LOS_OK;
}
///电源管理模块初始化
UINT32 OsPmInit(VOID)
{
    LosPmCB *pm = &g_pmCB;

    (VOID)memset_s(pm, sizeof(LosPmCB), 0, sizeof(LosPmCB));//全局链表置0

    pm->mode = LOS_SYS_NORMAL_SLEEP;//初始模式
    LOS_ListInit(&pm->lockList);//初始化链表
    return LOS_OK;
}

LOS_MODULE_INIT(OsPmInit, LOS_INIT_LEVEL_KMOD_EXTENDED);//以扩展方式初始化电源管理模块
#endif
