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

#include <sys/statfs.h>
#include <sys/mount.h>
#include "proc_fs.h"
#include "internal.h"
#ifdef LOSCFG_KERNEL_PM
#include "los_pm.h"
/**  
 * @brief 处理电源锁请求写入操作  
 * @details 将用户空间写入的电源锁请求转发给电源管理模块  
 * @param pf Proc文件结构体指针  
 * @param buf 写入缓冲区指针，包含锁请求信息  
 * @param count 写入字节数  
 * @param ppos 文件偏移量指针  
 * @return 成功返回0，失败返回负数错误码（取反的LOS_PmLockRequest返回值）  
 */  
static int PowerLockWrite(struct ProcFile *pf, const char *buf, size_t count, loff_t *ppos)  
{  
    (void)pf;   // 未使用的参数，显式标记避免编译警告  
    (void)count;  // 未使用的参数，显式标记避免编译警告  
    (void)ppos;  // 未使用的参数，显式标记避免编译警告  
    return -LOS_PmLockRequest(buf);  // 调用电源管理模块的锁请求接口，返回值取反以符合proc文件接口规范  
}  

/**  
 * @brief 处理电源锁信息读取操作  
 * @details 从电源管理模块获取锁状态信息并写入序列缓冲区  
 * @param m 序列缓冲区指针，用于存储输出内容  
 * @param v 未使用的私有数据指针  
 * @return 成功返回0，失败返回负数错误码  
 */  
static int PowerLockRead(struct SeqBuf *m, void *v)  
{  
    (void)v;  // 未使用的参数，显式标记避免编译警告  

    LOS_PmLockInfoShow(m);  // 将电源锁信息输出到序列缓冲区  
    return 0;  
}  

// 电源锁文件操作结构体，关联读写回调函数  
static const struct ProcFileOperations PowerLock = {  
    .write      = PowerLockWrite,  // 写操作回调函数  
    .read       = PowerLockRead,   // 读操作回调函数  
};  

/**  
 * @brief 处理电源锁释放写入操作  
 * @details 将用户空间写入的电源锁释放请求转发给电源管理模块  
 * @param pf Proc文件结构体指针  
 * @param buf 写入缓冲区指针，包含锁释放信息  
 * @param count 写入字节数  
 * @param ppos 文件偏移量指针  
 * @return 成功返回0，失败返回负数错误码（取反的LOS_PmLockRelease返回值）  
 */  
static int PowerUnlockWrite(struct ProcFile *pf, const char *buf, size_t count, loff_t *ppos)  
{  
    (void)pf;   // 未使用的参数，显式标记避免编译警告  
    (void)count;  // 未使用的参数，显式标记避免编译警告  
    (void)ppos;  // 未使用的参数，显式标记避免编译警告  
    return -LOS_PmLockRelease(buf);  // 调用电源管理模块的锁释放接口，返回值取反以符合proc文件接口规范  
}  

// 电源解锁文件操作结构体，关联读写回调函数  
static const struct ProcFileOperations PowerUnlock = {  
    .write      = PowerUnlockWrite,  // 写操作回调函数  
    .read       = PowerLockRead,     // 读操作复用电源锁的读回调函数  
};  

/**  
 * @brief 处理电源模式设置写入操作  
 * @details 将用户空间写入的电源模式字符串转换为枚举值并设置系统电源模式  
 * @param pf Proc文件结构体指针  
 * @param buf 写入缓冲区指针，包含电源模式字符串（normal/light/deep/shutdown）  
 * @param count 写入字节数  
 * @param ppos 文件偏移量指针  
 * @return 成功返回0，失败返回-EINVAL（不支持的模式）或取反的LOS_PmModeSet返回值  
 */  
static int PowerModeWrite(struct ProcFile *pf, const char *buf, size_t count, loff_t *ppos)  
{  
    (void)pf;   // 未使用的参数，显式标记避免编译警告  
    (void)count;  // 未使用的参数，显式标记避免编译警告  
    (void)ppos;  // 未使用的参数，显式标记避免编译警告  

    LOS_SysSleepEnum mode;  // 电源模式枚举变量  

    if (buf == NULL) {  // 输入缓冲区为空时直接返回  
        return 0;  
    }  

    // 根据输入字符串匹配电源模式  
    if (strcmp(buf, "normal") == 0) {  // 正常模式  
        mode = LOS_SYS_NORMAL_SLEEP;  
    } else if (strcmp(buf, "light") == 0) {  // 轻度睡眠模式  
        mode = LOS_SYS_LIGHT_SLEEP;  
    } else if (strcmp(buf, "deep") == 0) {  // 深度睡眠模式  
        mode = LOS_SYS_DEEP_SLEEP;  
    } else if (strcmp(buf, "shutdown") == 0) {  // 关机模式  
        mode = LOS_SYS_SHUTDOWN;  
    } else {  // 不支持的模式  
        PRINT_ERR("Unsupported hibernation mode: %s\n", buf);  // 打印不支持模式的错误信息  
        return -EINVAL;  // 返回无效参数错误码  
    }  

    return -LOS_PmModeSet(mode);  // 调用电源管理模块设置电源模式，返回值取反以符合proc文件接口规范  
}  

/**  
 * @brief 处理电源模式读取操作  
 * @details 输出支持的所有电源模式字符串到序列缓冲区  
 * @param m 序列缓冲区指针，用于存储输出内容  
 * @param v 未使用的私有数据指针  
 * @return 成功返回0  
 */  
static int PowerModeRead(struct SeqBuf *m, void *v)  
{  
    (void)v;  // 未使用的参数，显式标记避免编译警告  

    LosBufPrintf(m, "normal light deep shutdown\n");  // 输出支持的电源模式列表  
    return 0;  
}  

// 电源模式文件操作结构体，关联读写回调函数  
static const struct ProcFileOperations PowerMode = {  
    .write      = PowerModeWrite,  // 写操作回调函数  
    .read       = PowerModeRead,   // 读操作回调函数  
};  

/**  
 * @brief 处理电源锁计数读取操作  
 * @details 从电源管理模块获取当前锁计数并输出到序列缓冲区  
 * @param m 序列缓冲区指针，用于存储输出内容  
 * @param v 未使用的私有数据指针  
 * @return 成功返回0  
 */  
static int PowerCountRead(struct SeqBuf *m, void *v)  
{  
    (void)v;  // 未使用的参数，显式标记避免编译警告  
    UINT32 count = LOS_PmReadLock();  // 获取当前电源锁计数  

    LosBufPrintf(m, "%u\n", count);  // 输出锁计数值  
    return 0;  
}  

/**  
 * @brief 处理电源挂起写入操作  
 * @details 将用户空间写入的弱计数转换为整数并请求系统挂起  
 * @param pf Proc文件结构体指针  
 * @param buf 写入缓冲区指针，包含弱计数值字符串  
 * @param count 写入字节数  
 * @param ppos 文件偏移量指针  
 * @return 成功返回0，失败返回取反的LOS_PmSuspend返回值  
 */  
static int PowerCountWrite(struct ProcFile *pf, const char *buf, size_t count, loff_t *ppos)  
{  
    (void)pf;   // 未使用的参数，显式标记避免编译警告  
    (void)count;  // 未使用的参数，显式标记避免编译警告  
    (void)ppos;  // 未使用的参数，显式标记避免编译警告  

    int weakCount;  // 弱计数变量，用于电源挂起请求  

    if (buf == NULL) {  // 输入缓冲区为空时直接返回  
        return 0;  
    }  

    weakCount = atoi(buf);  // 将字符串转换为整数弱计数  
    return -LOS_PmSuspend(weakCount);  // 调用电源管理模块的挂起接口，返回值取反以符合proc文件接口规范  
}  

// 电源计数文件操作结构体，关联读写回调函数  
static const struct ProcFileOperations PowerCount = {  
    .write      = PowerCountWrite,  // 写操作回调函数  
    .read       = PowerCountRead,   // 读操作回调函数  
};  

// 电源相关文件的权限模式：所有者读写，组读写，其他读  
#define POWER_FILE_MODE  (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)  
// 电源相关文件的权限标识（UID/GID）  
#define OS_POWER_PRIVILEGE 7  

/**  
 * @brief 初始化/proc/power目录及相关文件节点  
 * @details 创建电源管理相关的proc文件系统节点，包括power_mode、power_lock、power_unlock和power_count  
 * @return 无返回值  
 */  
void ProcPmInit(void)  
{  
    // 创建/proc/power目录，权限为目录类型，所有者读写执行，组读写执行，其他读执行  
    struct ProcDirEntry *power = CreateProcEntry("power", S_IFDIR | S_IRWXU | S_IRWXG | S_IROTH, NULL);  
    if (power == NULL) {  // 目录创建失败处理  
        PRINT_ERR("create /proc/power error!\n");  // 打印错误信息  
        return;  
    }  
    power->uid = OS_POWER_PRIVILEGE;  // 设置目录所有者权限标识  
    power->gid = OS_POWER_PRIVILEGE;  // 设置目录组权限标识  

    // 创建/proc/power/power_mode文件，关联PowerMode操作结构体  
    struct ProcDirEntry *mode = CreateProcEntry("power/power_mode", POWER_FILE_MODE, NULL);  
    if (mode == NULL) {  // 文件创建失败处理  
        PRINT_ERR("create /proc/power/power_mode error!\n");  
        goto FREE_POWER;  // 跳转到错误处理标签，释放已分配资源  
    }  
    mode->procFileOps = &PowerMode;  // 关联文件操作结构体  
    mode->uid = OS_POWER_PRIVILEGE;  // 设置文件所有者权限标识  
    mode->gid = OS_POWER_PRIVILEGE;  // 设置文件组权限标识  

    // 创建/proc/power/power_lock文件，关联PowerLock操作结构体  
    struct ProcDirEntry *lock = CreateProcEntry("power/power_lock", POWER_FILE_MODE, NULL);  
    if (lock == NULL) {  // 文件创建失败处理  
        PRINT_ERR("create /proc/power/power_lock error!\n");  
        goto FREE_MODE;  // 跳转到错误处理标签，释放已分配资源  
    }  
    lock->procFileOps = &PowerLock;  // 关联文件操作结构体  
    lock->uid = OS_POWER_PRIVILEGE;  // 设置文件所有者权限标识  
    lock->gid = OS_POWER_PRIVILEGE;  // 设置文件组权限标识  

    // 创建/proc/power/power_unlock文件，关联PowerUnlock操作结构体  
    struct ProcDirEntry *unlock = CreateProcEntry("power/power_unlock", POWER_FILE_MODE, NULL);  
    if (unlock == NULL) {  // 文件创建失败处理  
        PRINT_ERR("create /proc/power/power_unlock error!\n");  
        goto FREE_LOCK;  // 跳转到错误处理标签，释放已分配资源  
    }  
    unlock->procFileOps = &PowerUnlock;  // 关联文件操作结构体  
    unlock->uid = OS_POWER_PRIVILEGE;  // 设置文件所有者权限标识  
    unlock->gid = OS_POWER_PRIVILEGE;  // 设置文件组权限标识  

    // 创建/proc/power/power_count文件，关联PowerCount操作结构体（仅读权限）  
    struct ProcDirEntry *count = CreateProcEntry("power/power_count", S_IRUSR | S_IRGRP | S_IROTH, NULL);  
    if (count == NULL) {  // 文件创建失败处理  
        PRINT_ERR("create /proc/power/power_count error!\n");  
        goto FREE_UNLOCK;  // 跳转到错误处理标签，释放已分配资源  
    }  
    count->procFileOps = &PowerCount;  // 关联文件操作结构体  
    count->uid = OS_POWER_PRIVILEGE;  // 设置文件所有者权限标识  
    count->gid = OS_POWER_PRIVILEGE;  // 设置文件组权限标识  

    return;  // 所有节点创建成功，返回  

// 错误处理标签：按创建逆序释放资源  
FREE_UNLOCK:  
    ProcFreeEntry(unlock);  // 释放power_unlock节点  
FREE_LOCK:  
    ProcFreeEntry(lock);    // 释放power_lock节点  
FREE_MODE:  
    ProcFreeEntry(mode);    // 释放power_mode节点  
FREE_POWER:  
    ProcFreeEntry(power);   // 释放power目录节点  
    return;  
}  
#endif  // 文件级条件编译结束  