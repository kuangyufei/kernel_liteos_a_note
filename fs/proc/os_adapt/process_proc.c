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

#include <sys/statfs.h>
#include <sys/mount.h>
#include "proc_fs.h"
#include "internal.h"
#include "los_process_pri.h"
#include "user_copy.h"
#include "los_memory.h"

#ifdef LOSCFG_PROC_PROCESS_DIR
#include "los_vm_dump.h"

typedef enum {
    PROC_PID,
    PROC_PID_MEM,
#ifdef LOSCFG_KERNEL_CPUP
    PROC_PID_CPUP,
#endif
#ifdef LOSCFG_USER_CONTAINER
    PROC_UID_MAP,
    PROC_GID_MAP,
#endif
    PROC_P_TYPE_MAX,
} ProcessDataType;

struct ProcProcess {
    char         *name;
    mode_t       mode;
    int          type;
    const struct ProcFileOperations *fileOps;
};

struct ProcessData {
    uintptr_t process;
    unsigned int type;
};

static LosProcessCB *ProcGetProcessCB(struct ProcessData *data)
{
    if (data->process != 0) {
        return (LosProcessCB *)data->process;
    }
    return OsCurrProcessGet();
}

#define PROC_PID_PRIVILEGE 7
#define PROC_PID_DIR_LEN 100
#ifdef LOSCFG_KERNEL_CONTAINER
static ssize_t ProcessContainerLink(unsigned int containerID, ContainerType type, char *buffer, size_t bufLen)
{
    ssize_t count = -1;
    if ((type == PID_CONTAINER) || (type == PID_CHILD_CONTAINER)) {
        count = snprintf_s(buffer, bufLen, bufLen - 1, "'pid:[%u]'", containerID);
    } else if (type == UTS_CONTAINER) {
        count = snprintf_s(buffer, bufLen, bufLen - 1, "'uts:[%u]'", containerID);
    } else if (type == MNT_CONTAINER) {
        count = snprintf_s(buffer, bufLen, bufLen - 1, "'mnt:[%u]'", containerID);
    } else if (type == IPC_CONTAINER) {
        count = snprintf_s(buffer, bufLen, bufLen - 1, "'ipc:[%u]'", containerID);
    } else if ((type == TIME_CONTAINER) || (type == TIME_CHILD_CONTAINER)) {
        count = snprintf_s(buffer, bufLen, bufLen - 1, "'time:[%u]'", containerID);
    } else if (type == USER_CONTAINER) {
        count = snprintf_s(buffer, bufLen, bufLen - 1, "'user:[%u]'", containerID);
    }  else if (type == NET_CONTAINER) {
        count = snprintf_s(buffer, bufLen, bufLen - 1, "'net:[%u]'", containerID);
    }

    if (count < 0) {
        return -EBADF;
    }
    return count;
}

static ssize_t ProcessContainerReadLink(struct ProcDirEntry *entry, char *buffer, size_t bufLen)
{
    char *freeBuf = NULL;
    char *buf = buffer;
    ssize_t count;
    unsigned int intSave;
    if (entry == NULL) {
        return -EINVAL;
    }
    struct ProcessData *data = (struct ProcessData *)entry->data;
    if (data == NULL) {
        return -EINVAL;
    }

    if (LOS_IsUserAddressRange((VADDR_T)(UINTPTR)buffer, bufLen)) {
        buf = LOS_MemAlloc(m_aucSysMem1, bufLen);
        if (buf == NULL) {
            return -ENOMEM;
        }
        (void)memset_s(buf, bufLen, 0, bufLen);
        freeBuf = buf;
    }

    LosProcessCB *processCB = ProcGetProcessCB(data);
    SCHEDULER_LOCK(intSave);
    UINT32 containerID = OsGetContainerID(processCB, (ContainerType)data->type);
    SCHEDULER_UNLOCK(intSave);
    if (containerID != OS_INVALID_VALUE) {
        count = ProcessContainerLink(containerID, (ContainerType)data->type, buf, bufLen);
    } else {
        count = strlen("(unknown)");
        if (memcpy_s(buf, bufLen, "(unknown)", count + 1) != EOK) {
            (void)LOS_MemFree(m_aucSysMem1, freeBuf);
            return -EBADF;
        }
    }
    if (count < 0) {
        (void)LOS_MemFree(m_aucSysMem1, freeBuf);
        return count;
    }

    if (LOS_IsUserAddressRange((VADDR_T)(UINTPTR)buffer, bufLen)) {
        if (LOS_ArchCopyToUser(buffer, buf, bufLen) != 0) {
            (void)LOS_MemFree(m_aucSysMem1, freeBuf);
            return -EFAULT;
        }
    }
    (void)LOS_MemFree(m_aucSysMem1, freeBuf);
    return count;
}

static const struct ProcFileOperations PID_CONTAINER_FOPS = {
    .readLink = ProcessContainerReadLink,
};

void *ProcfsContainerGet(int fd, unsigned int *containerType)
{
    if ((fd <= 0) || (containerType == NULL)) {
        return NULL;
    }

    VnodeHold();
    struct Vnode *vnode = VnodeFind(fd);
    if (vnode == NULL) {
        VnodeDrop();
        return NULL;
    }

    struct ProcDirEntry *entry = VnodeToEntry(vnode);
    if (entry == NULL) {
        VnodeDrop();
        return NULL;
    }

    struct ProcessData *data = (struct ProcessData *)entry->data;
    if (data == NULL) {
        VnodeDrop();
        return NULL;
    }

    void *processCB = (void *)ProcGetProcessCB(data);
    *containerType = data->type;
    VnodeDrop();
    return processCB;
}

#endif /* LOSCFG_KERNEL_CONTAINER */

static int ProcessMemInfoRead(struct SeqBuf *seqBuf, LosProcessCB *pcb)
{
    unsigned int intSave;
    unsigned int size = sizeof(LosVmSpace) + sizeof(LosVmMapRegion);
    LosVmSpace *vmSpace = (LosVmSpace *)LOS_MemAlloc(m_aucSysMem1, size);
    if (vmSpace == NULL) {
        return -ENOMEM;
    }
    (void)memset_s(vmSpace, size, 0, size);
    LosVmMapRegion *heap = (LosVmMapRegion *)((char *)vmSpace + sizeof(LosVmSpace));

    SCHEDULER_LOCK(intSave);
    if (OsProcessIsInactive(pcb)) {
        SCHEDULER_UNLOCK(intSave);
        (void)LOS_MemFree(m_aucSysMem1, vmSpace);
        return -EINVAL;
    }
    (void)memcpy_s(vmSpace, sizeof(LosVmSpace), pcb->vmSpace, sizeof(LosVmSpace));
    (void)memcpy_s(heap, sizeof(LosVmMapRegion), pcb->vmSpace->heap, sizeof(LosVmMapRegion));
    SCHEDULER_UNLOCK(intSave);

    (void)LosBufPrintf(seqBuf, "\nVMSpaceSize:      %u byte\n", vmSpace->size);
    (void)LosBufPrintf(seqBuf, "VMSpaceMapSize:   %u byte\n", vmSpace->mapSize);
    (void)LosBufPrintf(seqBuf, "VM TLB Asid:      %u\n", vmSpace->archMmu.asid);
    (void)LosBufPrintf(seqBuf, "VMHeapSize:       %u byte\n", heap->range.size);
    (void)LosBufPrintf(seqBuf, "VMHeapRegionName: %s\n", OsGetRegionNameOrFilePath(heap));
    (void)LosBufPrintf(seqBuf, "VMHeapRegionType: 0x%x\n", heap->regionType);
    (void)LOS_MemFree(m_aucSysMem1, vmSpace);
    return 0;
}

#ifdef LOSCFG_KERNEL_CPUP
#define TIME_CYCLE_TO_US(time) ((((UINT64)time) * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US)
static int ProcessCpupRead(struct SeqBuf *seqBuf, LosProcessCB *pcb)
{
    unsigned int intSave;
    OsCpupBase *processCpup = (OsCpupBase *)LOS_MemAlloc(m_aucSysMem1, sizeof(OsCpupBase));
    if (processCpup == NULL) {
        return -ENOMEM;
    }
    (void)memset_s(processCpup, sizeof(OsCpupBase), 0, sizeof(OsCpupBase));

    SCHEDULER_LOCK(intSave);
    if (OsProcessIsInactive(pcb)) {
        SCHEDULER_UNLOCK(intSave);
        (VOID)LOS_MemFree(m_aucSysMem1, processCpup);
        return -EINVAL;
    }
    (void)memcpy_s(processCpup, sizeof(OsCpupBase), pcb->processCpup, sizeof(OsCpupBase));
    SCHEDULER_UNLOCK(intSave);

    (void)LosBufPrintf(seqBuf, "\nTotalRunningTime: %lu us\n", TIME_CYCLE_TO_US(processCpup->allTime));
    (void)LosBufPrintf(seqBuf, "HistoricalRunningTime:(us) ");
    for (UINT32 i = 0; i < OS_CPUP_HISTORY_RECORD_NUM + 1; i++) {
        (void)LosBufPrintf(seqBuf, "%lu  ", TIME_CYCLE_TO_US(processCpup->historyTime[i]));
    }
    (void)LosBufPrintf(seqBuf, "\n");
    (void)LOS_MemFree(m_aucSysMem1, processCpup);
    return 0;
}
#endif

#ifdef LOSCFG_TIME_CONTAINER
static const CHAR *g_monotonic = "monotonic";
#define DECIMAL_BASE 10

static int ProcTimeContainerRead(struct SeqBuf *m, void *v)
{
    int ret;
    unsigned int intSave;
    struct timespec64 offsets = {0};

    if ((m == NULL) || (v == NULL)) {
        return -EINVAL;
    }

    struct ProcessData *data = (struct ProcessData *)v;
    SCHEDULER_LOCK(intSave);
    LosProcessCB *processCB = ProcGetProcessCB(data);
    ret = OsGetTimeContainerMonotonic(processCB, &offsets);
    SCHEDULER_UNLOCK(intSave);
    if (ret != LOS_OK) {
        return -ret;
    }

    LosBufPrintf(m, "monotonic %lld %ld\n", offsets.tv_sec, offsets.tv_nsec);
    return 0;
}

static int ProcSetTimensOffset(const char *buf, LosProcessCB *processCB)
{
    unsigned int intSave;
    struct timespec64 offsets;
    char *endptr = NULL;

    offsets.tv_sec = strtoll(buf, &endptr, DECIMAL_BASE);
    offsets.tv_nsec = strtoll(endptr, NULL, DECIMAL_BASE);
    if (offsets.tv_nsec >= OS_SYS_NS_PER_SECOND) {
        return -EACCES;
    }

    SCHEDULER_LOCK(intSave);
    unsigned int ret = OsSetTimeContainerMonotonic(processCB, &offsets);
    SCHEDULER_UNLOCK(intSave);
    if (ret != LOS_OK) {
        return -ret;
    }
    return 0;
}

static int ProcTimeContainerWrite(struct ProcFile *pf, const char *buf, size_t count, loff_t *ppos)
{
    (void)ppos;
    char *kbuf = NULL;
    int ret;

    if ((pf == NULL) || (count <= 0)) {
        return -EINVAL;
    }

    struct ProcDirEntry *entry = pf->pPDE;
    if (entry == NULL) {
        return -EINVAL;
    }

    struct ProcessData *data = (struct ProcessData *)entry->data;
    if (data == NULL) {
        return -EINVAL;
    }

    if (LOS_IsUserAddressRange((VADDR_T)(UINTPTR)buf, count)) {
        kbuf = LOS_MemAlloc(m_aucSysMem1, count + 1);
        if (kbuf == NULL) {
            return -ENOMEM;
        }

        if (LOS_ArchCopyFromUser(kbuf, buf, count) != 0) {
            (VOID)LOS_MemFree(m_aucSysMem1, kbuf);
            return -EFAULT;
        }
        kbuf[count] = '\0';
        buf = kbuf;
    }

    ret = strncmp(buf, g_monotonic, strlen(g_monotonic));
    if (ret != 0) {
        (VOID)LOS_MemFree(m_aucSysMem1, kbuf);
        return -EINVAL;
    }

    buf += strlen(g_monotonic);
    ret = ProcSetTimensOffset(buf, ProcGetProcessCB(data));
    if (ret < 0) {
        (VOID)LOS_MemFree(m_aucSysMem1, kbuf);
        return ret;
    }
    (VOID)LOS_MemFree(m_aucSysMem1, kbuf);
    return count;
}

static const struct ProcFileOperations TIME_CONTAINER_FOPS = {
    .read = ProcTimeContainerRead,
    .write = ProcTimeContainerWrite,
};
#endif

#ifdef LOSCFG_USER_CONTAINER

static void *MemdupUserNul(const void *src, size_t len)
{
    char *des = NULL;
    if (len <= 0) {
        return NULL;
    }
    des = LOS_MemAlloc(OS_SYS_MEM_ADDR, len + 1);
    if (des == NULL) {
        return NULL;
    }

    if (LOS_ArchCopyFromUser(des, src, len) != 0) {
        (VOID)LOS_MemFree(OS_SYS_MEM_ADDR, des);
        return NULL;
    }

    des[len] = '\0';
    return des;
}

static LosProcessCB *ProcUidGidMapWriteCheck(struct ProcFile *pf, const char *buf, size_t size,
                                             char **kbuf, ProcessDataType *type)
{
    if ((pf == NULL) || (size <= 0) || (size >= PAGE_SIZE)) {
        return NULL;
    }

    struct ProcDirEntry *entry = pf->pPDE;
    if (entry == NULL) {
        return NULL;
    }

    struct ProcessData *data = (struct ProcessData *)entry->data;
    if (data == NULL) {
        return NULL;
    }

    *kbuf = MemdupUserNul(buf, size);
    if (*kbuf == NULL) {
        return NULL;
    }
    *type = (ProcessDataType)data->type;
    return ProcGetProcessCB(data);
}

static ssize_t ProcIDMapWrite(struct ProcFile *file, const char *buf, size_t size, loff_t *ppos)
{
    (void)ppos;
    char *kbuf = NULL;
    int ret;
    unsigned int intSave;
    ProcessDataType type = PROC_P_TYPE_MAX;
    LosProcessCB *processCB = ProcUidGidMapWriteCheck(file, buf, size, &kbuf, &type);
    if (processCB == NULL) {
        return -EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    if ((processCB->credentials == NULL) || (processCB->credentials->userContainer == NULL)) {
        SCHEDULER_UNLOCK(intSave);
        (void)LOS_MemFree(m_aucSysMem1, kbuf);
        return -EINVAL;
    }
    UserContainer *userContainer = processCB->credentials->userContainer;
    if (userContainer->parent == NULL) {
        SCHEDULER_UNLOCK(intSave);
        (void)LOS_MemFree(m_aucSysMem1, kbuf);
        return -EPERM;
    }
    if (type == PROC_UID_MAP) {
        ret = OsUserContainerMapWrite(file, kbuf, size, CAP_SETUID,
                                      &userContainer->uidMap, &userContainer->parent->uidMap);
    } else {
        ret = OsUserContainerMapWrite(file, kbuf, size, CAP_SETGID,
                                      &userContainer->gidMap, &userContainer->parent->gidMap);
    }
    SCHEDULER_UNLOCK(intSave);
    (void)LOS_MemFree(m_aucSysMem1, kbuf);
    return ret;
}

static int ProcIDMapRead(struct SeqBuf *seqBuf, void *v)
{
    unsigned int intSave;
    if ((seqBuf == NULL) || (v == NULL)) {
        return -EINVAL;
    }
    struct ProcessData *data = (struct ProcessData *)v;
    LosProcessCB *processCB = ProcGetProcessCB(data);

    SCHEDULER_LOCK(intSave);
    if ((processCB->credentials == NULL) || (processCB->credentials->userContainer == NULL)) {
        SCHEDULER_UNLOCK(intSave);
        return -EINVAL;
    }
    UserContainer *userContainer = processCB->credentials->userContainer;
    if ((userContainer != NULL) && (userContainer->parent == NULL)) {
        UidGidExtent uidGidExtent = userContainer->uidMap.extent[0];
        SCHEDULER_UNLOCK(intSave);
        (void)LosBufPrintf(seqBuf, "%d %d %u\n", uidGidExtent.first, uidGidExtent.lowerFirst,
                           uidGidExtent.count);
        return 0;
    }
    SCHEDULER_LOCK(intSave);
    return 0;
}

static const struct ProcFileOperations UID_GID_MAP_FOPS = {
    .read = ProcIDMapRead,
    .write = ProcIDMapWrite,
};
#endif

static int ProcProcessRead(struct SeqBuf *m, void *v)
{
    if ((m == NULL) || (v == NULL)) {
        return -EINVAL;
    }
    struct ProcessData *data = (struct ProcessData *)v;
    switch (data->type) {
        case PROC_PID_MEM:
            return ProcessMemInfoRead(m, ProcGetProcessCB(data));
#ifdef LOSCFG_KERNEL_CPUP
        case PROC_PID_CPUP:
            return ProcessCpupRead(m, ProcGetProcessCB(data));
#endif
        default:
            break;
    }
    return -EINVAL;
}

static const struct ProcFileOperations PID_FOPS = {
    .read = ProcProcessRead,
};

static struct ProcProcess g_procProcess[] = {
    {
        .name = NULL,
        .mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IROTH,
        .type = PROC_PID,
        .fileOps = &PID_FOPS

    },
    {
        .name = "meminfo",
        .mode = 0,
        .type = PROC_PID_MEM,
        .fileOps = &PID_FOPS
    },
#ifdef LOSCFG_KERNEL_CPUP
    {
        .name = "cpup",
        .mode = 0,
        .type = PROC_PID_CPUP,
        .fileOps = &PID_FOPS

    },
#endif
#ifdef LOSCFG_KERNEL_CONTAINER
    {
        .name = "container",
        .mode = S_IFDIR | S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH,
        .type = CONTAINER,
        .fileOps = &PID_CONTAINER_FOPS

    },
#ifdef LOSCFG_PID_CONTAINER
    {
        .name = "container/pid",
        .mode = S_IFLNK,
        .type = PID_CONTAINER,
        .fileOps = &PID_CONTAINER_FOPS
    },
    {
        .name = "container/pid_for_children",
        .mode = S_IFLNK,
        .type = PID_CHILD_CONTAINER,
        .fileOps = &PID_CONTAINER_FOPS
    },
#endif
#ifdef LOSCFG_UTS_CONTAINER
    {
        .name = "container/uts",
        .mode = S_IFLNK,
        .type = UTS_CONTAINER,
        .fileOps = &PID_CONTAINER_FOPS
    },
#endif
#ifdef LOSCFG_MNT_CONTAINER
    {
        .name = "container/mnt",
        .mode = S_IFLNK,
        .type = MNT_CONTAINER,
        .fileOps = &PID_CONTAINER_FOPS
    },
#endif
#ifdef LOSCFG_IPC_CONTAINER
    {
        .name = "container/ipc",
        .mode = S_IFLNK,
        .type = IPC_CONTAINER,
        .fileOps = &PID_CONTAINER_FOPS
    },
#endif
#ifdef LOSCFG_TIME_CONTAINER
    {
        .name = "container/time",
        .mode = S_IFLNK,
        .type = TIME_CONTAINER,
        .fileOps = &PID_CONTAINER_FOPS
    },
    {
        .name = "container/time_for_children",
        .mode = S_IFLNK,
        .type = TIME_CHILD_CONTAINER,
        .fileOps = &PID_CONTAINER_FOPS
    },
    {
        .name = "time_offsets",
        .mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH,
        .type = TIME_CONTAINER,
        .fileOps = &TIME_CONTAINER_FOPS
    },
#endif
#ifdef LOSCFG_IPC_CONTAINER
    {
        .name = "container/user",
        .mode = S_IFLNK,
        .type = USER_CONTAINER,
        .fileOps = &PID_CONTAINER_FOPS
    },
    {
        .name = "uid_map",
        .mode = 0,
        .type = PROC_UID_MAP,
        .fileOps = &UID_GID_MAP_FOPS
    },
    {
        .name = "gid_map",
        .mode = 0,
        .type = PROC_GID_MAP,
        .fileOps = &UID_GID_MAP_FOPS
    },
#endif
#ifdef LOSCFG_IPC_CONTAINER
    {
        .name = "container/net",
        .mode = S_IFLNK,
        .type = NET_CONTAINER,
        .fileOps = &PID_CONTAINER_FOPS
    },
#endif
#endif
};

void ProcFreeProcessDir(struct ProcDirEntry *processDir)
{
    if (processDir == NULL) {
        return;
    }
    RemoveProcEntry(processDir->name, NULL);
}

static struct ProcDirEntry *ProcCreatePorcess(UINT32 pid, struct ProcProcess *porcess, uintptr_t processCB)
{
    int ret;
    struct ProcDataParm dataParm;
    char pidName[PROC_PID_DIR_LEN] = {0};
    struct ProcessData *data = (struct ProcessData *)malloc(sizeof(struct ProcessData));
    if (data == NULL) {
        return NULL;
    }
    if (pid != OS_INVALID_VALUE) {
        if (porcess->name != NULL) {
            ret = snprintf_s(pidName, PROC_PID_DIR_LEN, PROC_PID_DIR_LEN - 1, "%u/%s", pid, porcess->name);
        } else {
            ret = snprintf_s(pidName, PROC_PID_DIR_LEN, PROC_PID_DIR_LEN - 1, "%u", pid);
        }
    } else {
        if (porcess->name != NULL) {
            ret = snprintf_s(pidName, PROC_PID_DIR_LEN, PROC_PID_DIR_LEN - 1, "%s/%s", "self", porcess->name);
        } else {
            ret = snprintf_s(pidName, PROC_PID_DIR_LEN, PROC_PID_DIR_LEN - 1, "%s", "self");
        }
    }
    if (ret < 0) {
        free(data);
        return NULL;
    }

    data->process = processCB;
    data->type = porcess->type;
    dataParm.data = data;
    dataParm.dataType = PROC_DATA_FREE;
    struct ProcDirEntry *container = ProcCreateData(pidName, porcess->mode, NULL, porcess->fileOps, &dataParm);
    if (container == NULL) {
        free(data);
        PRINT_ERR("create /proc/%s error!\n", pidName);
        return NULL;
    }
    return container;
}

int ProcCreateProcessDir(UINT32 pid, uintptr_t process)
{
    unsigned int intSave;
    struct ProcDirEntry *pidDir = NULL;
    for (int index = 0; index < (sizeof(g_procProcess) / sizeof(struct ProcProcess)); index++) {
        struct ProcProcess *procProcess = &g_procProcess[index];
        struct ProcDirEntry *dir = ProcCreatePorcess(pid, procProcess, process);
        if (dir == NULL) {
            PRINT_ERR("create /proc/%s error!\n", procProcess->name);
            goto CREATE_ERROR;
        }
        if (index == 0) {
            pidDir = dir;
        }
    }

    if (process != 0) {
        SCHEDULER_LOCK(intSave);
        ((LosProcessCB *)process)->procDir = pidDir;
        SCHEDULER_UNLOCK(intSave);
    }

    return 0;

CREATE_ERROR:
    if (pidDir != NULL) {
        RemoveProcEntry(pidDir->name, NULL);
    }
    return -1;
}
#endif /* LOSCFG_PROC_PROCESS_DIR */

static int ProcessProcFill(struct SeqBuf *m, void *v)
{
    (void)v;
    (void)OsShellCmdTskInfoGet(OS_ALL_TASK_MASK, m, OS_PROCESS_INFO_ALL);
    return 0;
}

static const struct ProcFileOperations PROCESS_PROC_FOPS = {
    .read       = ProcessProcFill,
};

void ProcProcessInit(void)
{
    struct ProcDirEntry *pde = CreateProcEntry("process", 0, NULL);
    if (pde == NULL) {
        PRINT_ERR("create /proc/process error!\n");
        return;
    }
    pde->procFileOps = &PROCESS_PROC_FOPS;

#ifdef LOSCFG_PROC_PROCESS_DIR
    int ret = ProcCreateProcessDir(OS_INVALID_VALUE, 0);
    if (ret < 0) {
        PRINT_ERR("Create proc process self dir failed!\n");
    }

    ret = ProcCreateProcessDir(OS_USER_ROOT_PROCESS_ID, (uintptr_t)OsGetUserInitProcess());
    if (ret < 0) {
        PRINT_ERR("Create proc process %d dir failed!\n", OS_USER_ROOT_PROCESS_ID);
    }
#endif
    return;
}
