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

#include "capability_type.h"
#include "los_memory.h"
#include "los_process_pri.h"
#include "user_copy.h"
#include "los_printf.h"
/**
 * @brief  capability初始状态掩码（所有capability位均置1）
 */
#define CAPABILITY_INIT_STAT            0xffffffff
/**
 * @brief  获取指定capability的位掩码
 * @param  x capability索引
 * @return 对应capability的位掩码（第x位为1）
 */
#define CAPABILITY_GET_CAP_MASK(x)      (1 << ((x) & 31))
/**
 * @brief  最大capability索引（支持0-31共32种capability）
 */
#define CAPABILITY_MAX                  31
/**
 * @brief  检查capability权限是否越界
 * @param  a 待设置的capability掩码
 * @param  b 当前进程拥有的capability掩码
 * @return 非0表示越界（待设置的capability包含未拥有的权限），0表示合法
 */
#define VALID_CAPS(a, b)                (((a) & (~(b))) != 0)

/**
 * @brief  检查当前进程是否拥有指定capability权限
 * @param  capIndex capability索引（0-CAPABILITY_MAX）
 * @return TRUE 拥有权限; FALSE 无权限或索引无效
 */
BOOL IsCapPermit(UINT32 capIndex)
{
    UINT32 capability = OsCurrProcessGet()->capability;  // 获取当前进程的capability掩码
    if (capIndex > CAPABILITY_MAX || capIndex < 0) {  // 检查索引有效性
        PRINTK("%s,%d, get invalid capIndex %u\n", __FUNCTION__, __LINE__, capIndex);
        return FALSE;
    }

    return (capability & (CAPABILITY_GET_CAP_MASK(capIndex)));  // 检查对应位是否置1
}

/**
 * @brief  初始化进程的capability状态
 * @param  processCB 进程控制块指针
 */
VOID OsInitCapability(LosProcessCB *processCB)
{
    processCB->capability = CAPABILITY_INIT_STAT;  // 设置初始状态为全权限
}

/**
 * @brief  复制进程的capability状态
 * @param  from 源进程控制块
 * @param  to 目标进程控制块
 */
VOID OsCopyCapability(LosProcessCB *from, LosProcessCB *to)
{
    UINT32 intSave;

    SCHEDULER_LOCK(intSave);  // 关调度器保护临界区
    to->capability = from->capability;  // 复制capability掩码
    SCHEDULER_UNLOCK(intSave);  // 开调度器
}

/**
 * @brief  系统调用：设置当前进程的capability掩码
 * @param  caps 新的capability掩码
 * @return LOS_OK 设置成功; -EPERM 权限不足; 其他错误码
 */
UINT32 SysCapSet(UINT32 caps)
{
    UINT32 intSave;

    SCHEDULER_LOCK(intSave);  // 关调度器保护临界区
    if (!IsCapPermit(CAP_CAPSET)) {  // 检查是否有CAP_CAPSET权限
        SCHEDULER_UNLOCK(intSave);
        return -EPERM;
    }

    if (VALID_CAPS(caps, OsCurrProcessGet()->capability)) {  // 检查新权限是否越界
        SCHEDULER_UNLOCK(intSave);
        return -EPERM;
    }

    OsCurrProcessGet()->capability = caps;  // 更新capability掩码
    SCHEDULER_UNLOCK(intSave);  // 开调度器
    return LOS_OK;
}

/**
 * @brief  系统调用：获取指定进程的capability掩码
 * @param  pid 进程ID（0表示当前进程）
 * @param  caps 用户空间指针，用于存储获取到的capability掩码
 * @return LOS_OK 获取成功; -EINVAL 参数无效; -ESRCH 进程不存在; -EFAULT 内存复制失败
 */
UINT32 SysCapGet(pid_t pid, UINT32 *caps)
{
    UINT32 intSave;
    UINT32 kCaps;
    LosProcessCB *processCB = NULL;

    if ((OS_PID_CHECK_INVALID((UINT32)pid))) {  // 检查PID有效性
        return -EINVAL;
    }

    if (pid == 0) {  // 0表示获取当前进程的capability
        processCB = OsCurrProcessGet();
    } else {  // 根据PID查找进程控制块
        processCB = OS_PCB_FROM_PID(pid);
    }

    SCHEDULER_LOCK(intSave);  // 关调度器保护临界区
    if (OsProcessIsInactive(processCB)) {  // 检查进程是否处于活动状态
        SCHEDULER_UNLOCK(intSave);
        return -ESRCH;
    }

    kCaps = processCB->capability;  // 获取内核空间的capability值
    SCHEDULER_UNLOCK(intSave);  // 开调度器
    
	//@note_thinking 感觉这里可以不用 LOS_ArchCopyToUser 直接返回kCaps
    if (LOS_ArchCopyToUser(caps, &kCaps, sizeof(UINT32)) != LOS_OK) {  // 复制到用户空间
        return -EFAULT;
    }

    return LOS_OK;
}