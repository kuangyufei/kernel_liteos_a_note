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
// /proc 下的电源适配层
//写电源锁信息
static int PowerLockWrite(struct ProcFile *pf, const char *buf, size_t count, loff_t *ppos)
{
    (void)pf;
    (void)count;
    (void)ppos;
    return -LOS_PmLockRequest(buf);
}
///读取电源锁信息
static int PowerLockRead(struct SeqBuf *m, void *v)
{
    (void)v;

    LOS_PmLockInfoShow(m);
    return 0;
}
///proc 拿电源锁操作
static const struct ProcFileOperations PowerLock = {
    .write      = PowerLockWrite,
    .read       = PowerLockRead,
};
//proc 释放锁操作
static int PowerUnlockWrite(struct ProcFile *pf, const char *buf, size_t count, loff_t *ppos)
{
    (void)pf;
    (void)count;
    (void)ppos;
    return -LOS_PmLockRelease(buf);
}

static const struct ProcFileOperations PowerUnlock = {
    .write      = PowerUnlockWrite,
    .read       = PowerLockRead,
};

static int PowerModeWrite(struct ProcFile *pf, const char *buf, size_t count, loff_t *ppos)
{
    (void)pf;
    (void)count;
    (void)ppos;

    LOS_SysSleepEnum mode;

    if (buf == NULL) {
        return 0;
    }

    if (strcmp(buf, "normal") == 0) {
        mode = LOS_SYS_NORMAL_SLEEP;
    } else if (strcmp(buf, "light") == 0) {
        mode = LOS_SYS_LIGHT_SLEEP;
    } else if (strcmp(buf, "deep") == 0) {
        mode = LOS_SYS_DEEP_SLEEP;
    } else if (strcmp(buf, "shutdown") == 0) {
        mode = LOS_SYS_SHUTDOWN;
    } else {
        PRINT_ERR("Unsupported hibernation mode: %s\n", buf);
        return -EINVAL;
    }

    return -LOS_PmModeSet(mode);
}

static int PowerModeRead(struct SeqBuf *m, void *v)
{
    (void)v;

    LosBufPrintf(m, "normal light deep shutdown\n");
    return 0;
}

static const struct ProcFileOperations PowerMode = {
    .write      = PowerModeWrite,
    .read       = PowerModeRead,
};

static int PowerCountRead(struct SeqBuf *m, void *v)
{
    (void)v;
    UINT32 count = LOS_PmReadLock();

    LosBufPrintf(m, "%u\n", count);
    return 0;
}

static int PowerCountWrite(struct ProcFile *pf, const char *buf, size_t count, loff_t *ppos)
{
    (void)pf;
    (void)count;
    (void)ppos;

    int weakCount;

    if (buf == NULL) {
        return 0;
    }

    weakCount = atoi(buf);
    return -LOS_PmSuspend(weakCount);
}

static const struct ProcFileOperations PowerCount = {
    .write      = PowerCountWrite,
    .read       = PowerCountRead,
};

#define POWER_FILE_MODE  (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)
#define OS_POWER_PRIVILEGE 7	//电源特权
// /proc/power 的初始化 
void ProcPmInit(void)
{
    struct ProcDirEntry *power = CreateProcEntry("power", S_IFDIR | S_IRWXU | S_IRWXG | S_IROTH, NULL);
    if (power == NULL) {//创建 文件夹 /proc/power
        PRINT_ERR("create /proc/power error!\n");
        return;
    }
    power->uid = OS_POWER_PRIVILEGE;
    power->gid = OS_POWER_PRIVILEGE;

    struct ProcDirEntry *mode = CreateProcEntry("power/power_mode", POWER_FILE_MODE, NULL);
    if (mode == NULL) {//创建 文件夹 /proc/power/power_mode
        PRINT_ERR("create /proc/power/power_mode error!\n");
        goto FREE_POWER;
    }
    mode->procFileOps = &PowerMode;
    mode->uid = OS_POWER_PRIVILEGE;
    mode->gid = OS_POWER_PRIVILEGE;

    struct ProcDirEntry *lock = CreateProcEntry("power/power_lock", POWER_FILE_MODE, NULL);
    if (lock == NULL) {//创建 文件夹 /proc/power/power_lock
        PRINT_ERR("create /proc/power/power_lock error!\n");
        goto FREE_MODE;
    }
    lock->procFileOps = &PowerLock;
    lock->uid = OS_POWER_PRIVILEGE;
    lock->gid = OS_POWER_PRIVILEGE;

    struct ProcDirEntry *unlock = CreateProcEntry("power/power_unlock", POWER_FILE_MODE, NULL);
    if (unlock == NULL) {//创建 文件夹 /proc/power/power_unlock
        PRINT_ERR("create /proc/power/power_unlock error!\n");
        goto FREE_LOCK;
    }
    unlock->procFileOps = &PowerUnlock;
    unlock->uid = OS_POWER_PRIVILEGE;
    unlock->gid = OS_POWER_PRIVILEGE;

    struct ProcDirEntry *count = CreateProcEntry("power/power_count", S_IRUSR | S_IRGRP | S_IROTH, NULL);
    if (count == NULL) {//创建 文件夹 /proc/power/power_count
        PRINT_ERR("create /proc/power/power_count error!\n");
        goto FREE_UNLOCK;
    }
    count->procFileOps = &PowerCount;
    count->uid = OS_POWER_PRIVILEGE;
    count->gid = OS_POWER_PRIVILEGE;

    return;

FREE_UNLOCK:
    ProcFreeEntry(unlock);
FREE_LOCK:
    ProcFreeEntry(lock);
FREE_MODE:
    ProcFreeEntry(mode);
FREE_POWER:
    ProcFreeEntry(power);
    return;
}
#endif
