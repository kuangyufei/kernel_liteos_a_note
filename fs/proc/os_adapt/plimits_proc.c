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

#include <sys/statfs.h>
#include <stdio.h>
#include <unistd.h>
#include "stdlib.h"
#include "los_printf.h"
#include "los_base.h"
#include "los_seq_buf.h"
#include "internal.h"
#include "proc_fs.h"
#include "los_task_pri.h"
#include "los_process_pri.h"
#include "los_process.h"
#include "show.h"
#include "vnode.h"
#include "proc_file.h"
#include "user_copy.h"

#ifdef LOSCFG_KERNEL_PLIMITS
#include "los_plimits.h"

#define PLIMITS_ENTRY_NAME_MAX 64
#define PLIMITERSET_DELETE_ALLOC 4
#define UNITPTR_NULL ((uintptr_t)(0xFFFFFFFF))
#define PLIMIT_FILE_MODE_READ_WRITE        (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define PLIMIT_FILE_MODE_READ_ONLY         (S_IRUSR | S_IRGRP | S_IROTH)
#define PLIMIT_FILE_MODE_WRITE_ONLY         (S_IWUSR)
#define PLIMIT_FILE_MODE_MASK_WRITE        (~((mode_t)(S_IWUSR)))
#define PLIMIT_FILE_MODE_MASK_NONE         (~((mode_t)(0)))
#define LOS_MAX_CACHE (UINT64)(0xFFFFFFFFFFFFFFFF)
#define PLIMIT_CAT_BUF_SIZE  512
#define MAX_PROTECTED_PROCESS_ID 14
#define UNITPTR_NULL  ((uintptr_t)(0xFFFFFFFF))

static int ShowPids(struct SeqBuf *seqBuf, VOID *data);
static ssize_t PidMigrateFromProcLimiterSet(struct ProcFile *pf, const CHAR *buf, size_t count, loff_t *ppos);
static ssize_t PidLimitReadPidLimit(struct SeqBuf *seqBuf, VOID *data);
static ssize_t PidLimitReadPriorityLimit(struct SeqBuf *seqBuf, VOID *data);
static ssize_t PriorityLimitVariableWrite(struct ProcFile *pf, const CHAR *buf, size_t count, loff_t *ppos);
static ssize_t PidsMaxVariableWrite(struct ProcFile *pf, const CHAR *buf, size_t count, loff_t *ppos);
static ssize_t ProcLimitsShowLimiters(struct SeqBuf *seqBuf, VOID *data);
static int ProcfsPlimitsMkdir(struct ProcDirEntry *parent, const char *dirName, mode_t mode, struct ProcDirEntry **pde);
static int ProcfsPlimitsRmdir(struct ProcDirEntry *parent, struct ProcDirEntry *pde, const char *name);
#ifdef LOSCFG_KERNEL_MEM_PLIMIT
static ssize_t MemLimitReadLimit(struct SeqBuf *seqBuf, VOID *data);
static ssize_t MemLimitWriteLimit(struct ProcFile *pf, const CHAR *buf, size_t count, loff_t *ppos);
static ssize_t MemLimitStatShow(struct SeqBuf *seqBuf, VOID *data);
#endif
#ifdef LOSCFG_KERNEL_IPC_PLIMIT
static ssize_t IPCLimitReadMqLimit(struct SeqBuf *seqBuf, VOID *data);
static ssize_t IPCLimitWriteMqLimit(struct ProcFile *pf, const CHAR *buf, size_t count, loff_t *ppos);
static ssize_t IPCLimitReadShmLimit(struct SeqBuf *seqBuf, VOID *data);
static ssize_t IPCLimitWriteShmLimit(struct ProcFile *pf, const CHAR *buf, size_t count, loff_t *ppos);
static ssize_t IPCLimitShowStat(struct SeqBuf *seqBuf, VOID *data);
#endif
#ifdef LOSCFG_KERNEL_DEV_PLIMIT
static ssize_t DevLimitWriteAllow(struct ProcFile *pf, const CHAR *buf, size_t count, loff_t *ppos);
static ssize_t DevLimitWriteDeny(struct ProcFile *pf, const CHAR *buf, size_t count, loff_t *ppos);
static ssize_t DevLimitShow(struct SeqBuf *seqBuf, VOID *data);
#endif
#ifdef LOSCFG_KERNEL_SCHED_PLIMIT
static ssize_t SchedLimitReadPeriod(struct SeqBuf *seqBuf, VOID *data);
static ssize_t SchedLimitWritePeriod(struct ProcFile *pf, const CHAR *buf, size_t count, loff_t *ppos);
static ssize_t SchedLimitReadQuota(struct SeqBuf *seqBuf, VOID *data);
static ssize_t SchedLimitWriteQuota(struct ProcFile *pf, const CHAR *buf, size_t count, loff_t *ppos);
static ssize_t SchedLimitShow(struct SeqBuf *seqBuf, VOID *data);
#endif

struct PLimitsEntryOpt {
    int id;
    char name[PLIMITS_ENTRY_NAME_MAX];
    mode_t mode;
    uintptr_t offset;
    struct ProcFileOperations ops;
};

static struct ProcDirOperations g_procDirOperations = {
    .mkdir = ProcfsPlimitsMkdir,
    .rmdir = ProcfsPlimitsRmdir,
};

static struct PLimitsEntryOpt g_plimitsEntryOpts[] = {
    {
        .id = PROCESS_LIMITER_COUNT,
        .name = "plimits.limiters",
        .mode = PLIMIT_FILE_MODE_READ_ONLY,
        .offset = UNITPTR_NULL,
        .ops = {
            .read = ProcLimitsShowLimiters,
        }
    },
    {
        .id = PROCESS_LIMITER_COUNT,
        .name = "plimits.procs",
        .mode = PLIMIT_FILE_MODE_READ_WRITE,
        .offset = UNITPTR_NULL,
        .ops = {
            .read = ShowPids,
            .write = PidMigrateFromProcLimiterSet,
        },
    },
    {
        .id = PROCESS_LIMITER_ID_PIDS,
        .name = "pids.max",
        .mode = PLIMIT_FILE_MODE_READ_WRITE,
        .offset = 0,
        .ops = {
            .read = PidLimitReadPidLimit,
            .write = PidsMaxVariableWrite,
        },
    },
    {
        .id = PROCESS_LIMITER_ID_PIDS,
        .name = "pids.priority",
        .mode = PLIMIT_FILE_MODE_READ_WRITE,
        .offset = 0,
        .ops = {
            .read = PidLimitReadPriorityLimit,
            .write = PriorityLimitVariableWrite,
        },
    },
#ifdef LOSCFG_KERNEL_MEM_PLIMIT
    {
        .id = PROCESS_LIMITER_ID_MEM,
        .name = "memory.limit",
        .mode = PLIMIT_FILE_MODE_READ_WRITE,
        .offset = 0,
        .ops = {
            .read = MemLimitReadLimit,
            .write = MemLimitWriteLimit,
        },
    },
    {
        .id = PROCESS_LIMITER_ID_MEM,
        .name = "memory.stat",
        .mode = PLIMIT_FILE_MODE_READ_ONLY,
        .offset = UNITPTR_NULL,
        .ops = {
            .read = MemLimitStatShow,
        }
    },
#endif
#ifdef LOSCFG_KERNEL_IPC_PLIMIT
    {
        .id = PROCESS_LIMITER_ID_IPC,
        .name = "ipc.mq_limit",
        .mode = PLIMIT_FILE_MODE_READ_WRITE,
        .offset = 0,
        .ops = {
            .read = IPCLimitReadMqLimit,
            .write = IPCLimitWriteMqLimit,
        }
    },
    {
        .id = PROCESS_LIMITER_ID_IPC,
        .name = "ipc.shm_limit",
        .mode = PLIMIT_FILE_MODE_READ_WRITE,
        .offset = 0,
        .ops = {
            .read = IPCLimitReadShmLimit,
            .write = IPCLimitWriteShmLimit,
        }
    },
    {
        .id = PROCESS_LIMITER_ID_IPC,
        .name = "ipc.stat",
        .mode = PLIMIT_FILE_MODE_READ_ONLY,
        .offset = UNITPTR_NULL,
        .ops = {
            .read = IPCLimitShowStat,
        }
    },
#endif
#ifdef LOSCFG_KERNEL_DEV_PLIMIT
    {
        .id = PROCESS_LIMITER_ID_DEV,
        .name = "devices.allow",
        .mode = PLIMIT_FILE_MODE_WRITE_ONLY,
        .offset = UNITPTR_NULL,
        .ops = {
            .write = DevLimitWriteAllow,
        }
    },
    {
        .id = PROCESS_LIMITER_ID_DEV,
        .name = "devices.deny",
        .mode = PLIMIT_FILE_MODE_WRITE_ONLY,
        .offset = UNITPTR_NULL,
        .ops = {
            .write = DevLimitWriteDeny,
        }
    },
    {
        .id = PROCESS_LIMITER_ID_DEV,
        .name = "devices.list",
        .mode = PLIMIT_FILE_MODE_READ_ONLY,
        .offset = 0,
        .ops = {
            .read = DevLimitShow,
        }
    },
#endif
#ifdef LOSCFG_KERNEL_SCHED_PLIMIT
    {
        .id = PROCESS_LIMITER_ID_SCHED,
        .name = "sched.period",
        .mode = PLIMIT_FILE_MODE_READ_WRITE,
        .offset = 0,
        .ops = {
            .read = SchedLimitReadPeriod,
            .write = SchedLimitWritePeriod,
        },
    },
    {
        .id = PROCESS_LIMITER_ID_SCHED,
        .name = "sched.quota",
        .mode = PLIMIT_FILE_MODE_READ_WRITE,
        .offset = 0,
        .ops = {
            .read = SchedLimitReadQuota,
            .write = SchedLimitWriteQuota,
        },
    },
    {
        .id = PROCESS_LIMITER_ID_SCHED,
        .name = "sched.stat",
        .mode = PLIMIT_FILE_MODE_READ_ONLY,
        .offset = UNITPTR_NULL,
        .ops = {
            .read = SchedLimitShow,
        }
    },
#endif
};

static unsigned int MemUserCopy(const char *src, size_t len, char **kbuf)
{
    if (LOS_IsUserAddressRange((VADDR_T)(UINTPTR)src, len)) {
        char *kernelBuf = LOS_MemAlloc(m_aucSysMem1, len + 1);
        if (kernelBuf == NULL) {
            return ENOMEM;
        }

        if (LOS_ArchCopyFromUser(kernelBuf, src, len) != 0) {
            (VOID)LOS_MemFree(m_aucSysMem1, kernelBuf);
            return EFAULT;
        }
        kernelBuf[len] = '\0';
        *kbuf = kernelBuf;
        return 0;
    }
    return 0;
}

static inline struct ProcDirEntry *GetCurrDirectory(struct ProcDirEntry *dirEntry)
{
    return ((dirEntry == NULL) || S_ISDIR(dirEntry->mode)) ? dirEntry : dirEntry->parent;
}

static inline ProcLimiterSet *GetProcLimiterSetFromDirEntry(struct ProcDirEntry *dirEntry)
{
    struct ProcDirEntry *currDirectory = GetCurrDirectory(dirEntry);
    return (currDirectory == NULL) || (currDirectory->data == NULL) ? NULL : (ProcLimiterSet *)currDirectory->data;
}

static struct ProcDirEntry *ProcCreateLimiterFiles(struct ProcDirEntry *parentEntry,
                                                   struct PLimitsEntryOpt *entryOpt,
                                                   mode_t mode, void *data)
{
    struct ProcDataParm dataParm = {
        .data = data,
        .dataType = PROC_DATA_STATIC,
    };
    struct ProcDirEntry *plimitFile = ProcCreateData(entryOpt->name, entryOpt->mode & mode, parentEntry,
                                                     &entryOpt->ops, &dataParm);
    if (plimitFile == NULL) {
        return NULL;
    }
    return plimitFile;
}

static void ProcLimiterDirEntryInit(struct ProcDirEntry *dirEntry, unsigned mask, mode_t mode)
{
    struct ProcDirEntry *currDir = GetCurrDirectory(dirEntry);
    if (currDir == NULL) {
        return;
    }

    ProcLimiterSet *plimiterData = (ProcLimiterSet *)currDir->data;
    if (plimiterData == NULL) {
        return;
    }

    for (int index = 0; index < (sizeof(g_plimitsEntryOpts) / sizeof(struct PLimitsEntryOpt)); index++) {
        struct PLimitsEntryOpt *entryOpt = &g_plimitsEntryOpts[index];
        enum ProcLimiterID plimiterType = entryOpt->id;
        if (!(BIT(plimiterType) & mask)) {
            continue;
        }

        void *head = (entryOpt->offset == UNITPTR_NULL) ?
                     plimiterData : (void *)plimiterData->limitsList[plimiterType];
        struct ProcDirEntry *entry = ProcCreateLimiterFiles(currDir, entryOpt, mode, head);
        if (entry == NULL) {
            RemoveProcEntry(currDir->name, NULL);
            return;
        }
    }
    return;
}

static ssize_t PLimitsCopyLimits(struct ProcDirEntry *dirEntry)
{
    struct ProcDirEntry *parentPde = dirEntry->parent;
    ProcLimiterSet *parentPLimits = (ProcLimiterSet *)parentPde->data;
    if (parentPLimits == NULL) {
        return -EINVAL;
    }

    ProcLimiterSet *newPLimits = OsPLimitsCreate(parentPLimits);
    if (newPLimits == NULL) {
        return -ENOMEM;
    }
    dirEntry->data = (VOID *)newPLimits;
    dirEntry->dataType = PROC_DATA_STATIC;
    dirEntry->procDirOps = parentPde->procDirOps;
    ProcLimiterDirEntryInit(dirEntry, newPLimits->mask, PLIMIT_FILE_MODE_MASK_NONE);
    return 0;
}

static int ProcfsPlimitsMkdir(struct ProcDirEntry *parent, const char *dirName, mode_t mode, struct ProcDirEntry **pde)
{
    int ret;
    if (strcmp(parent->name, "plimits") != 0) {
        return -EPERM;
    }

    struct ProcDirEntry *plimitDir = ProcCreateData(dirName, S_IFDIR | mode, parent, NULL, NULL);
    if (plimitDir == NULL) {
        return -EINVAL;
    }

    ret = PLimitsCopyLimits(plimitDir);
    if (ret != LOS_OK) {
        ProcFreeEntry(plimitDir);
        return -ENOSYS;
    }
    *pde = plimitDir;
    return ret;
}

static int ProcfsPlimitsRmdir(struct ProcDirEntry *parent, struct ProcDirEntry *pde, const char *name)
{
    if (pde == NULL) {
        return -EINVAL;
    }

    ProcLimiterSet *plimits = GetProcLimiterSetFromDirEntry(pde);
    pde->data = NULL;

    unsigned ret = OsPLimitsFree(plimits);
    if (ret != 0) {
        pde->data = plimits;
        return -ret;
    }

    spin_lock(&procfsLock);
    ProcDetachNode(pde);
    spin_unlock(&procfsLock);

    RemoveProcEntryTravalsal(pde->subdir);

    ProcFreeEntry(pde);
    return 0;
}

static ssize_t ProcLimitsShowLimiters(struct SeqBuf *seqBuf, VOID *data)
{
    ProcLimiterSet *plimits = (ProcLimiterSet *)data;
    UINT32 mask;
    if (plimits == NULL) {
        return -LOS_NOK;
    }
    mask = plimits->mask;

    if (mask & BIT(PROCESS_LIMITER_ID_PIDS)) {
        LosBufPrintf(seqBuf, "%s ", "pids");
    }
#ifdef LOSCFG_KERNEL_MEM_PLIMIT
    if (mask & BIT(PROCESS_LIMITER_ID_MEM)) {
        LosBufPrintf(seqBuf, "%s ", "memory");
    }
#endif
#ifdef LOSCFG_KERNEL_IPC_PLIMIT
    if (mask & BIT(PROCESS_LIMITER_ID_IPC)) {
        LosBufPrintf(seqBuf, "%s ", "ipc");
    }
#endif
#ifdef LOSCFG_KERNEL_DEV_PLIMIT
    if (mask & BIT(PROCESS_LIMITER_ID_DEV)) {
        LosBufPrintf(seqBuf, "%s ", "devices");
    }
#endif
#ifdef LOSCFG_KERNEL_SCHED_PLIMIT
    if (mask & BIT(PROCESS_LIMITER_ID_SCHED)) {
        LosBufPrintf(seqBuf, "%s ", "sched");
    }
#endif
    return LOS_OK;
}

#define PLIMITS_PID_STR_LENGTH 4
static int ShowPids(struct SeqBuf *seqBuf, VOID *data)
{
    unsigned int size, pidMax;
    if (data == NULL) {
        return -EINVAL;
    }

    const ProcLimiterSet *plimits = (const ProcLimiterSet *)data;
    pidMax = LOS_GetSystemProcessMaximum();
    size = pidMax * sizeof(unsigned int);
    unsigned int *pids = (unsigned int *)LOS_MemAlloc(m_aucSysMem1, size);
    if (pids == NULL) {
        return -ENOMEM;
    }
    (void)memset_s(pids, size, 0, size);

    unsigned int ret = OsPLimitsPidsGet(plimits, pids, size);
    if (ret != LOS_OK) {
        (VOID)LOS_MemFree(m_aucSysMem1, pids);
        return -ret;
    }

    (void)LosBufPrintf(seqBuf, "\n");
    for (unsigned int index = 0; index < pidMax; index++) {
        if (pids[index] == 0) {
            continue;
        }
        (void)LosBufPrintf(seqBuf, "%u  ", index);
    }
    (void)LOS_MemFree(m_aucSysMem1, pids);
    return 0;
}

static long long int GetPidLimitValue(struct ProcFile *pf, const CHAR *buf, size_t count)
{
    long long int value;
    char *kbuf = NULL;

    if ((pf == NULL) || (pf->pPDE == NULL) || (buf == NULL) || (count <= 0)) {
        return -EINVAL;
    }

    unsigned ret = MemUserCopy(buf, count, &kbuf);
    if (ret != 0) {
        return -ret;
    } else if ((ret == 0) && (kbuf != NULL)) {
        buf = (const char *)kbuf;
    }

    if (strspn(buf, "0123456789") != count) {
        (void)LOS_MemFree(m_aucSysMem1, kbuf);
        return -EINVAL;
    }
    value = strtoll(buf, NULL, 10); /* 10: decimal */
    (void)LOS_MemFree(m_aucSysMem1, kbuf);
    return value;
}

static ssize_t PidMigrateFromProcLimiterSet(struct ProcFile *pf, const CHAR *buf, size_t count, loff_t *ppos)
{
    (VOID)ppos;
    unsigned ret;

    long long int pid = GetPidLimitValue(pf, buf, count);
    if (pid < 0) {
        return pid;
    }

    ProcLimiterSet *plimits = GetCurrDirectory(pf->pPDE)->data;
    ret = OsPLimitsAddPid(plimits, (unsigned int)pid);
    if (ret != LOS_OK) {
        return -ret;
    }
    return count;
}

static ssize_t PidLimitReadPidLimit(struct SeqBuf *seqBuf, VOID *data)
{
    PidLimit *pidLimit = (PidLimit *)data;
    if ((seqBuf == NULL) || (pidLimit == NULL)) {
        return -EINVAL;
    }

    (void)LosBufPrintf(seqBuf, "%u\n", pidLimit->pidLimit);
    return 0;
}

static ssize_t PidLimitReadPriorityLimit(struct SeqBuf *seqBuf, VOID *data)
{
    PidLimit *pidLimit = (PidLimit *)data;
    if ((seqBuf == NULL) || (pidLimit == NULL)) {
        return -EINVAL;
    }

    (void)LosBufPrintf(seqBuf, "%u\n", pidLimit->priorityLimit);
    return 0;
}

static ssize_t PriorityLimitVariableWrite(struct ProcFile *pf, const CHAR *buf, size_t count, loff_t *ppos)
{
    (void)ppos;
    long long int value = GetPidLimitValue(pf, buf, count);
    if (value < 0) {
        return value;
    }

    PidLimit *pidLimit = (PidLimit *)pf->pPDE->data;
    unsigned ret = PidLimitSetPriorityLimit(pidLimit, (unsigned)value);
    if (ret != LOS_OK) {
        return -ret;
    }
    return count;
}

static ssize_t PidsMaxVariableWrite(struct ProcFile *pf, const CHAR *buf, size_t count, loff_t *ppos)
{
    (void)ppos;
    long long int value = GetPidLimitValue(pf, buf, count);
    if (value < 0) {
        return value;
    }

    PidLimit *pidLimit = (PidLimit *)pf->pPDE->data;
    unsigned ret = PidLimitSetPidLimit(pidLimit, (unsigned)value);
    if (ret != LOS_OK) {
        return -ret;
    }
    return count;
}

#ifdef LOSCFG_KERNEL_MEM_PLIMIT
static ssize_t MemLimitReadLimit(struct SeqBuf *seqBuf, VOID *data)
{
    ProcMemLimiter *memLimit = (ProcMemLimiter *)data;
    if ((seqBuf == NULL) || (memLimit == NULL)) {
        return -EINVAL;
    }

    (void)LosBufPrintf(seqBuf, "%llu\n", memLimit->limit);
    return 0;
}

static ssize_t MemLimitWriteLimit(struct ProcFile *pf, const CHAR *buf, size_t count, loff_t *ppos)
{
    (void)ppos;
    long long int value = GetPidLimitValue(pf, buf, count);
    if ((value < 0) || (value > (long long int)OS_NULL_INT)) {
        return value;
    }

    ProcMemLimiter *memLimit = (ProcMemLimiter *)pf->pPDE->data;
    unsigned ret = OsMemLimitSetMemLimit(memLimit, (unsigned long long)value);
    if (ret != LOS_OK) {
        return -ret;
    }
    return count;
}

static ssize_t MemLimitStatShow(struct SeqBuf *seqBuf, VOID *data)
{
    ProcLimiterSet *plimits = (ProcLimiterSet *)data;
    if ((seqBuf == NULL) || (plimits == NULL)) {
        return -EINVAL;
    }

    UINT32 pidMax = LOS_GetSystemProcessMaximum();
    UINT32 size = sizeof(ProcMemLimiter) + pidMax * sizeof(unsigned long long);
    unsigned long long *usage = (unsigned long long *)LOS_MemAlloc(m_aucSysMem1, size);
    if (usage == NULL) {
        return -ENOMEM;
    }
    (void)memset_s(usage, size, 0, size);

    unsigned int ret = OsPLimitsMemUsageGet(plimits, usage, size);
    if (ret != LOS_OK) {
        (VOID)LOS_MemFree(m_aucSysMem1, usage);
        return -ret;
    }

    ProcMemLimiter *memLimit = (ProcMemLimiter *)usage;
    unsigned long long *memUsage = (unsigned long long *)((uintptr_t)usage + sizeof(ProcMemLimiter));
    (void)LosBufPrintf(seqBuf, "\nMem used: %llu\n", memLimit->usage);
    (void)LosBufPrintf(seqBuf, "Mem peak: %llu\n", memLimit->peak);
    (void)LosBufPrintf(seqBuf, "Mem failed count: %u\n", memLimit->failcnt);

    for (unsigned int index = 0; index < pidMax; index++) {
        if (memUsage[index] == 0) {
            continue;
        }
        (void)LosBufPrintf(seqBuf, "PID: %u  mem used: %llu \n", index, memUsage[index]);
    }
    (void)LOS_MemFree(m_aucSysMem1, usage);
    return 0;
}
#endif

#ifdef LOSCFG_KERNEL_IPC_PLIMIT
static ssize_t IPCLimitReadMqLimit(struct SeqBuf *seqBuf, VOID *data)
{
    ProcIPCLimit *ipcLimit = (ProcIPCLimit *)data;
    if ((seqBuf == NULL) || (ipcLimit == NULL)) {
        return -EINVAL;
    }

    (void)LosBufPrintf(seqBuf, "%u\n", ipcLimit->mqCountLimit);
    return 0;
}

static ssize_t IPCLimitWriteMqLimit(struct ProcFile *pf, const CHAR *buf, size_t count, loff_t *ppos)
{
    (void)ppos;
    long long int value = GetPidLimitValue(pf, buf, count);
    if (value < 0) {
        return value;
    }

    ProcIPCLimit *ipcLimit = (ProcIPCLimit *)pf->pPDE->data;
    unsigned ret = OsIPCLimitSetMqLimit(ipcLimit, (unsigned long long)value);
    if (ret != LOS_OK) {
        return -ret;
    }
    return count;
}

static ssize_t IPCLimitReadShmLimit(struct SeqBuf *seqBuf, VOID *data)
{
    ProcIPCLimit *ipcLimit = (ProcIPCLimit *)data;
    if ((seqBuf == NULL) || (ipcLimit == NULL)) {
        return -EINVAL;
    }

    (void)LosBufPrintf(seqBuf, "%u\n", ipcLimit->shmSizeLimit);
    return 0;
}

static ssize_t IPCLimitWriteShmLimit(struct ProcFile *pf, const CHAR *buf, size_t count, loff_t *ppos)
{
    (void)ppos;
    long long int value = GetPidLimitValue(pf, buf, count);
    if ((value < 0) || (value > (long long int)OS_NULL_INT)) {
        return value;
    }

    ProcIPCLimit *ipcLimit = (ProcIPCLimit *)pf->pPDE->data;
    unsigned ret = OsIPCLimitSetShmLimit(ipcLimit, (unsigned long long)value);
    if (ret != LOS_OK) {
        return -ret;
    }
    return count;
}

static ssize_t IPCLimitShowStat(struct SeqBuf *seqBuf, VOID *data)
{
    ProcLimiterSet *plimits = (ProcLimiterSet *)data;
    if ((seqBuf == NULL) || (plimits == NULL)) {
        return -EINVAL;
    }

    unsigned int size = sizeof(ProcIPCLimit);
    ProcIPCLimit *newIPCLimit = (ProcIPCLimit *)LOS_MemAlloc(m_aucSysMem1, size);
    if (newIPCLimit == NULL) {
        return -ENOMEM;
    }
    (void)memset_s(newIPCLimit, size, 0, size);

    unsigned int ret = OsPLimitsIPCStatGet(plimits, newIPCLimit, size);
    if (ret != LOS_OK) {
        (VOID)LOS_MemFree(m_aucSysMem1, newIPCLimit);
        return -ret;
    }

    (void)LosBufPrintf(seqBuf, "mq count: %u\n", newIPCLimit->mqCount);
    (void)LosBufPrintf(seqBuf, "mq failed count: %u\n", newIPCLimit->mqFailedCount);
    (void)LosBufPrintf(seqBuf, "shm size: %u\n", newIPCLimit->shmSize);
    (void)LosBufPrintf(seqBuf, "shm failed count: %u\n", newIPCLimit->shmFailedCount);
    (void)LOS_MemFree(m_aucSysMem1, newIPCLimit);
    return 0;
}
#endif

#ifdef LOSCFG_KERNEL_DEV_PLIMIT
static ssize_t DevLimitWriteAllow(struct ProcFile *pf, const CHAR *buf, size_t count, loff_t *ppos)
{
    (void)ppos;
    char *kbuf = NULL;

    if ((pf == NULL) || (pf->pPDE == NULL) || (buf == NULL) || (count <= 0)) {
        return -EINVAL;
    }

    unsigned ret = MemUserCopy(buf, count, &kbuf);
    if (ret != 0) {
        return -ret;
    } else if ((ret == 0) && (kbuf != NULL)) {
        buf = (const char *)kbuf;
    }

    ProcLimiterSet *plimit = (ProcLimiterSet *)pf->pPDE->data;
    ret = OsDevLimitWriteAllow(plimit, buf, count);
    (VOID)LOS_MemFree(m_aucSysMem1, kbuf);
    if (ret != LOS_OK) {
        return -ret;
    }
    return count;
}

static ssize_t DevLimitWriteDeny(struct ProcFile *pf, const CHAR *buf, size_t count, loff_t *ppos)
{
    (void)ppos;
    char *kbuf = NULL;

    if ((pf == NULL) || (pf->pPDE == NULL) || (buf == NULL) || (count <= 0)) {
        return -EINVAL;
    }

    unsigned ret = MemUserCopy(buf, count, &kbuf);
    if (ret != 0) {
        return -ret;
    } else if ((ret == 0) && (kbuf != NULL)) {
        buf = (const char *)kbuf;
    }

    ProcLimiterSet *plimit = (ProcLimiterSet *)pf->pPDE->data;
    ret = OsDevLimitWriteDeny(plimit, buf, count);
    (VOID)LOS_MemFree(m_aucSysMem1, kbuf);
    if (ret != LOS_OK) {
        return -ret;
    }
    return count;
}

static ssize_t DevLimitShow(struct SeqBuf *seqBuf, VOID *data)
{
    ProcDevLimit *devLimit = (ProcDevLimit *)data;
    if ((seqBuf == NULL) || (devLimit == NULL)) {
        return -EINVAL;
    }

    unsigned ret = OsDevLimitShow(devLimit, seqBuf);
    if (ret != LOS_OK) {
        return -ret;
    }
    return 0;
}
#endif

#ifdef LOSCFG_KERNEL_SCHED_PLIMIT
static ssize_t SchedLimitReadPeriod(struct SeqBuf *seqBuf, VOID *data)
{
    ProcSchedLimiter *schedLimit = (ProcSchedLimiter *)data;
    if ((seqBuf == NULL) || (schedLimit == NULL)) {
        return -EINVAL;
    }

    (void)LosBufPrintf(seqBuf, "%lld\n", schedLimit->period);
    return 0;
}

static ssize_t SchedLimitWritePeriod(struct ProcFile *pf, const CHAR *buf, size_t count, loff_t *ppos)
{
    (void)ppos;
    long long int value = GetPidLimitValue(pf, buf, count);
    if (value < 0) {
        return value;
    }

    ProcSchedLimiter *schedLimit = (ProcSchedLimiter *)pf->pPDE->data;
    unsigned ret = OsSchedLimitSetPeriod(schedLimit, (unsigned long long)value);
    if (ret != LOS_OK) {
        return -ret;
    }
    return count;
}

static ssize_t SchedLimitReadQuota(struct SeqBuf *seqBuf, VOID *data)
{
    ProcSchedLimiter *schedLimit = (ProcSchedLimiter *)data;
    if ((seqBuf == NULL) || (schedLimit == NULL)) {
        return -EINVAL;
    }

    (void)LosBufPrintf(seqBuf, "%lld\n", schedLimit->quota);
    return 0;
}

static ssize_t SchedLimitWriteQuota(struct ProcFile *pf, const CHAR *buf, size_t count, loff_t *ppos)
{
    (void)ppos;
    long long int value = GetPidLimitValue(pf, buf, count);
    if (value < 0) {
        return value;
    }

    ProcSchedLimiter *schedLimit = (ProcSchedLimiter *)pf->pPDE->data;
    unsigned ret = OsSchedLimitSetQuota(schedLimit, (unsigned long long)value);
    if (ret != LOS_OK) {
        return -ret;
    }
    return count;
}

#define TIME_CYCLE_TO_US(time) ((((UINT64)time) * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US)
#define SCHED_DEFAULT_VALUE (0x101010101010101)

static ssize_t SchedLimitShow(struct SeqBuf *seqBuf, VOID *data)
{
    ProcLimiterSet *plimits = (ProcLimiterSet *)data;
    if ((seqBuf == NULL) || (plimits == NULL)) {
        return -EINVAL;
    }

    UINT32 pidMax = LOS_GetSystemProcessMaximum();
    UINT32 size = pidMax * sizeof(unsigned long long);
    unsigned long long *usage = (unsigned long long *)LOS_MemAlloc(m_aucSysMem1, size);
    if (usage == NULL) {
        return -ENOMEM;
    }
    (void)memset_s(usage, size, 1, size);

    unsigned int ret = OsPLimitsSchedUsageGet(plimits, usage, size);
    if (ret != LOS_OK) {
        (VOID)LOS_MemFree(m_aucSysMem1, usage);
        return -ret;
    }

    for (unsigned int index = 0; index < pidMax; index++) {
        if (usage[index] == SCHED_DEFAULT_VALUE) {
            continue;
        }
        (void)LosBufPrintf(seqBuf, "PID: %u  runTime: %llu us\n", index, TIME_CYCLE_TO_US(usage[index]));
    }
    (void)LOS_MemFree(m_aucSysMem1, usage);
    return 0;
}
#endif

#define PROC_PLIMITS_MODE (S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)
void ProcLimitsInit(void)
{
    struct ProcDirEntry *parentPDE = CreateProcEntry("plimits", PROC_PLIMITS_MODE, NULL);
    if (parentPDE == NULL) {
        return;
    }
    ProcLimiterSet *plimits = OsRootPLimitsGet();
    parentPDE->procDirOps = &g_procDirOperations;
    parentPDE->data = (VOID *)plimits;
    parentPDE->dataType = PROC_DATA_STATIC;
    plimits->mask = BIT(PROCESS_LIMITER_ID_PIDS) | BIT(PROCESS_LIMITER_COUNT);
#ifdef LOSCFG_KERNEL_MEM_PLIMIT
    plimits->mask |= BIT(PROCESS_LIMITER_ID_MEM);
#endif
#ifdef LOSCFG_KERNEL_IPC_PLIMIT
    plimits->mask |= BIT(PROCESS_LIMITER_ID_IPC);
#endif
#ifdef LOSCFG_KERNEL_DEV_PLIMIT
    plimits->mask |= BIT(PROCESS_LIMITER_ID_DEV);
#endif
#ifdef LOSCFG_KERNEL_SCHED_PLIMIT
    plimits->mask |= BIT(PROCESS_LIMITER_ID_SCHED);
#endif
    ProcLimiterDirEntryInit(parentPDE, plimits->mask, PLIMIT_FILE_MODE_MASK_WRITE);
    return;
}
#endif
