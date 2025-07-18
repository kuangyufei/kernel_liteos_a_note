/*
 * Copyright (c) 2023-2023 Huawei Device Co., Ltd. All rights reserved.
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
#include "los_time_container_pri.h"
#include "los_process_pri.h"

#ifdef LOSCFG_TIME_CONTAINER
// 当前时间容器数量  用于跟踪系统中创建的时间容器总数
STATIC UINT32 g_currentTimeContainerNum;  // 当前时间容器数量

/**
 * @brief 创建新的时间容器
 * 
 * @param parent 父时间容器指针，若为根容器则为NULL
 * @return TimeContainer* 成功返回新创建的时间容器指针，失败返回NULL
 */
STATIC TimeContainer *CreateNewTimeContainer(TimeContainer *parent)
{
    UINT32 size = sizeof(TimeContainer);
    TimeContainer *timeContainer = (TimeContainer *)LOS_MemAlloc(m_aucSysMem1, size);  // 分配时间容器内存
    if (timeContainer == NULL) {  // 内存分配失败检查
        return NULL;
    }
    (VOID)memset_s(timeContainer, size, 0, size);  // 初始化时间容器内存为0

    timeContainer->containerID = OsAllocContainerID();  // 分配容器ID
    if (parent != NULL) {  // 非根容器初始化
        timeContainer->frozenOffsets = FALSE;  // 不冻结时间偏移量
        LOS_AtomicSet(&timeContainer->rc, 1);  // 设置引用计数为1
    } else {  // 根容器初始化
        timeContainer->frozenOffsets = TRUE;  // 冻结时间偏移量
        LOS_AtomicSet(&timeContainer->rc, 3); /* 3: 三个系统进程引用根容器 */
    }
    return timeContainer;
}

/**
 * @brief 创建时间容器并关联到子进程
 * 
 * @param child 子进程控制块指针
 * @param parent 父进程控制块指针
 * @return UINT32 成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 CreateTimeContainer(LosProcessCB *child, LosProcessCB *parent)
{
    UINT32 intSave;

    SCHEDULER_LOCK(intSave);  // 关闭调度器，防止并发访问
    TimeContainer *newTimeContainer = parent->container->timeForChildContainer;  // 获取父进程的子时间容器
    LOS_AtomicInc(&newTimeContainer->rc);  // 增加时间容器引用计数
    newTimeContainer->frozenOffsets = TRUE;  // 冻结时间偏移量
    child->container->timeContainer = newTimeContainer;  // 关联子进程时间容器
    child->container->timeForChildContainer = newTimeContainer;  // 设置子进程的子时间容器
    SCHEDULER_UNLOCK(intSave);  // 恢复调度器
    return LOS_OK;
}

/**
 * @brief 初始化根时间容器
 * 
 * @param timeContainer 根时间容器指针的指针
 * @return UINT32 成功返回LOS_OK，失败返回ENOMEM
 */
UINT32 OsInitRootTimeContainer(TimeContainer **timeContainer)
{
    UINT32 intSave;
    TimeContainer *newTimeContainer = CreateNewTimeContainer(NULL);  // 创建根时间容器
    if (newTimeContainer == NULL) {  // 创建失败检查
        return ENOMEM;
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器，防止并发访问
    *timeContainer = newTimeContainer;  // 设置根时间容器指针
    g_currentTimeContainerNum++;  // 增加时间容器计数
    SCHEDULER_UNLOCK(intSave);  // 恢复调度器
    return LOS_OK;
}

/**
 * @brief 复制时间容器
 * 
 * @param flags 克隆标志
 * @param child 子进程控制块指针
 * @param parent 父进程控制块指针
 * @return UINT32 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsCopyTimeContainer(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent)
{
    UINT32 intSave;
    TimeContainer *currTimeContainer = parent->container->timeContainer;  // 获取父进程时间容器

    if (currTimeContainer == parent->container->timeForChildContainer) {  // 检查是否共享时间容器
        SCHEDULER_LOCK(intSave);  // 关闭调度器，防止并发访问
        LOS_AtomicInc(&currTimeContainer->rc);  // 增加引用计数
        child->container->timeContainer = currTimeContainer;  // 子进程共享父进程时间容器
        child->container->timeForChildContainer = currTimeContainer;  // 设置子进程的子时间容器
        SCHEDULER_UNLOCK(intSave);  // 恢复调度器
        return LOS_OK;
    }

    if (OsContainerLimitCheck(TIME_CONTAINER, &g_currentTimeContainerNum) != LOS_OK) {  // 检查容器数量限制
        return EPERM;
    }

    return CreateTimeContainer(child, parent);  // 创建新的时间容器
}

/**
 * @brief 取消共享时间容器
 * 
 * @param flags 克隆标志
 * @param curr 当前进程控制块指针
 * @param newContainer 新容器指针
 * @return UINT32 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsUnshareTimeContainer(UINTPTR flags, LosProcessCB *curr, Container *newContainer)
{
    UINT32 intSave;
    if (!(flags & CLONE_NEWTIME)) {  // 检查是否需要创建新时间命名空间
        SCHEDULER_LOCK(intSave);  // 关闭调度器，防止并发访问
        newContainer->timeContainer = curr->container->timeContainer;  // 继承当前时间容器
        newContainer->timeForChildContainer = curr->container->timeForChildContainer;  // 继承子时间容器
        LOS_AtomicInc(&newContainer->timeContainer->rc);  // 增加时间容器引用计数
        if (newContainer->timeContainer != newContainer->timeForChildContainer) {  // 若时间容器与子时间容器不同
            LOS_AtomicInc(&newContainer->timeForChildContainer->rc);  // 增加子时间容器引用计数
        }
        SCHEDULER_UNLOCK(intSave);  // 恢复调度器
        return LOS_OK;
    }

    if (OsContainerLimitCheck(TIME_CONTAINER, &g_currentTimeContainerNum) != LOS_OK) {  // 检查容器数量限制
        return EPERM;
    }

    TimeContainer *timeForChild = CreateNewTimeContainer(curr->container->timeContainer);  // 创建新的子时间容器
    if (timeForChild == NULL) {  // 创建失败检查
        return ENOMEM;
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器，防止并发访问
    if (curr->container->timeContainer != curr->container->timeForChildContainer) {  // 检查时间容器是否可分离
        SCHEDULER_UNLOCK(intSave);  // 恢复调度器
        (VOID)LOS_MemFree(m_aucSysMem1, timeForChild);  // 释放已分配的子时间容器内存
        return EINVAL;
    }

    (VOID)memcpy_s(&timeForChild->monotonic, sizeof(struct timespec64),  // 复制单调时间偏移量
                   &curr->container->timeContainer->monotonic, sizeof(struct timespec64));
    newContainer->timeContainer = curr->container->timeContainer;  // 设置新容器的时间容器
    LOS_AtomicInc(&newContainer->timeContainer->rc);  // 增加时间容器引用计数
    newContainer->timeForChildContainer = timeForChild;  // 设置新容器的子时间容器
    g_currentTimeContainerNum++;  // 增加时间容器计数
    SCHEDULER_UNLOCK(intSave);  // 恢复调度器
    return LOS_OK;
}

/**
 * @brief 设置命名空间时间容器
 * 
 * @param flags 标志位
 * @param container 源容器指针
 * @param newContainer 新容器指针
 * @return UINT32 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsSetNsTimeContainer(UINT32 flags, Container *container, Container *newContainer)
{
    LosProcessCB *curr = OsCurrProcessGet();  // 获取当前进程控制块
    if (flags & CLONE_NEWTIME) {  // 检查是否需要设置新时间命名空间
        container->timeContainer->frozenOffsets = TRUE;  // 冻结时间偏移量
        newContainer->timeContainer = container->timeContainer;  // 设置新容器的时间容器
        newContainer->timeForChildContainer = container->timeContainer;  // 设置新容器的子时间容器
        LOS_AtomicInc(&container->timeContainer->rc);  // 增加时间容器引用计数
        return LOS_OK;
    }

    newContainer->timeContainer = curr->container->timeContainer;  // 继承当前进程的时间容器
    LOS_AtomicInc(&curr->container->timeContainer->rc);  // 增加时间容器引用计数
    newContainer->timeForChildContainer = curr->container->timeForChildContainer;  // 继承当前进程的子时间容器
    if (curr->container->timeContainer != curr->container->timeForChildContainer) {  // 若时间容器与子时间容器不同
        LOS_AtomicInc(&curr->container->timeForChildContainer->rc);  // 增加子时间容器引用计数
    }
    return LOS_OK;
}

/**
 * @brief 销毁时间容器
 * 
 * @param container 容器指针
 */
VOID OsTimeContainerDestroy(Container *container)
{
    UINT32 intSave;
    TimeContainer *timeContainer = NULL;
    TimeContainer *timeForChild = NULL;

    if (container == NULL) {  // 参数合法性检查
        return;
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器，防止并发访问
    if (container->timeContainer == NULL) {  // 时间容器不存在检查
        SCHEDULER_UNLOCK(intSave);  // 恢复调度器
        return;
    }

    if ((container->timeForChildContainer != NULL) && (container->timeContainer != container->timeForChildContainer)) {  // 子时间容器存在且与时间容器不同
        LOS_AtomicDec(&container->timeForChildContainer->rc);  // 减少子时间容器引用计数
        if (LOS_AtomicRead(&container->timeForChildContainer->rc) <= 0) {  // 引用计数为0，需要释放
            g_currentTimeContainerNum--;  // 减少时间容器计数
            timeForChild = container->timeForChildContainer;  // 保存子时间容器指针
            container->timeForChildContainer = NULL;  // 清空子时间容器指针
        }
    }

    LOS_AtomicDec(&container->timeContainer->rc);  // 减少时间容器引用计数
    if (LOS_AtomicRead(&container->timeContainer->rc) <= 0) {  // 引用计数为0，需要释放
        g_currentTimeContainerNum--;  // 减少时间容器计数
        timeContainer = container->timeContainer;  // 保存时间容器指针
        container->timeContainer = NULL;  // 清空时间容器指针
        container->timeForChildContainer = NULL;  // 清空子时间容器指针
    }
    SCHEDULER_UNLOCK(intSave);  // 恢复调度器
    (VOID)LOS_MemFree(m_aucSysMem1, timeForChild);  // 释放子时间容器内存
    (VOID)LOS_MemFree(m_aucSysMem1, timeContainer);  // 释放时间容器内存
    return;
}

/**
 * @brief 获取时间容器ID
 * 
 * @param timeContainer 时间容器指针
 * @return UINT32 成功返回容器ID，失败返回OS_INVALID_VALUE
 */
UINT32 OsGetTimeContainerID(TimeContainer *timeContainer)
{
    if (timeContainer == NULL) {  // 参数合法性检查
        return OS_INVALID_VALUE;
    }

    return timeContainer->containerID;  // 返回容器ID
}

/**
 * @brief 获取当前进程的时间容器
 * 
 * @return TimeContainer* 当前进程的时间容器指针
 */
TimeContainer *OsGetCurrTimeContainer(VOID)
{
    return OsCurrProcessGet()->container->timeContainer;  // 返回当前进程的时间容器
}

/**
 * @brief 获取时间容器的单调时间偏移量
 * 
 * @param processCB 进程控制块指针
 * @param offsets 单调时间偏移量指针
 * @return UINT32 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsGetTimeContainerMonotonic(LosProcessCB *processCB, struct timespec64 *offsets)
{
    if ((processCB == NULL) || (offsets == NULL)) {  // 参数合法性检查
        return EINVAL;
    }

    if (OsProcessIsInactive(processCB)) {  // 检查进程是否处于活动状态
        return ESRCH;
    }

    TimeContainer *timeContainer = processCB->container->timeForChildContainer;  // 获取进程的子时间容器
    (VOID)memcpy_s(offsets, sizeof(struct timespec64), &timeContainer->monotonic, sizeof(struct timespec64));  // 复制单调时间偏移量
    return LOS_OK;
}

/**
 * @brief 设置时间容器的单调时间偏移量
 * 
 * @param processCB 进程控制块指针
 * @param offsets 新的单调时间偏移量指针
 * @return UINT32 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsSetTimeContainerMonotonic(LosProcessCB *processCB, struct timespec64 *offsets)
{
    if ((processCB == NULL) || (offsets == NULL)) {  // 参数合法性检查
        return EINVAL;
    }

    if (OsProcessIsInactive(processCB)) {  // 检查进程是否处于活动状态
        return ESRCH;
    }

    TimeContainer *timeContainer = processCB->container->timeForChildContainer;  // 获取进程的子时间容器
    if (timeContainer->frozenOffsets) {  // 检查时间偏移量是否被冻结
        return EACCES;
    }

    timeContainer->monotonic.tv_sec = offsets->tv_sec;  // 设置秒数
    timeContainer->monotonic.tv_nsec = offsets->tv_nsec;  // 设置纳秒数
    return LOS_OK;
}

/**
 * @brief 获取时间容器数量
 * 
 * @return UINT32 当前时间容器总数
 */
UINT32 OsGetTimeContainerCount(VOID)
{
    return g_currentTimeContainerNum;  // 返回当前时间容器数量
}
#endif
