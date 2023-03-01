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
#include <sys/mount.h>
#include "proc_fs.h"
#include "internal.h"
#include "los_process_pri.h"
#include "user_copy.h"
#include "los_memory.h"

#ifdef LOSCFG_KERNEL_CONTAINER
struct ProcSysUser {
    char         *name;
    mode_t       mode;
    int          type;
    const struct ProcFileOperations *fileOps;
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

static int GetContainerLimitValue(struct ProcFile *pf, const CHAR *buf, size_t count)
{
    int value;
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
    value = atoi(buf);
    (void)LOS_MemFree(m_aucSysMem1, kbuf);
    return value;
}

static ssize_t ProcSysUserWrite(struct ProcFile *pf, const char *buf, size_t size, loff_t *ppos)
{
    (void)ppos;
    unsigned ret;
    int value = GetContainerLimitValue(pf, buf, size);
    if (value < 0) {
        return -EINVAL;
    }

    ContainerType type = (ContainerType)(uintptr_t)pf->pPDE->data;
    ret = OsSetContainerLimit(type, value);
    if (ret != LOS_OK) {
        return -EINVAL;
    }
    return size;
}

static int ProcSysUserRead(struct SeqBuf *seqBuf, void *v)
{
    unsigned ret;
    if ((seqBuf == NULL) || (v == NULL)) {
        return EINVAL;
    }

    ContainerType type = (ContainerType)(uintptr_t)v;
    ret = OsGetContainerLimit(type);
    if (ret == OS_INVALID_VALUE) {
        return EINVAL;
    }
    (void)LosBufPrintf(seqBuf, "\nlimit: %u\n", ret);
    (void)LosBufPrintf(seqBuf, "count: %u\n", OsGetContainerCount(type));
    return 0;
}

static const struct ProcFileOperations SYS_USER_OPT = {
    .read = ProcSysUserRead,
    .write = ProcSysUserWrite,
};

static struct ProcSysUser g_sysUser[] = {
#ifdef LOSCFG_MNT_CONTAINER
    {
        .name = "max_mnt_container",
        .mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH,
        .type = MNT_CONTAINER,
        .fileOps = &SYS_USER_OPT

    },
#endif
#ifdef LOSCFG_PID_CONTAINER
    {
        .name = "max_pid_container",
        .mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH,
        .type = PID_CONTAINER,
        .fileOps = &SYS_USER_OPT
    },
#endif
#ifdef LOSCFG_USER_CONTAINER
    {
        .name = "max_user_container",
        .mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH,
        .type = USER_CONTAINER,
        .fileOps = &SYS_USER_OPT

    },
#endif
#ifdef LOSCFG_UTS_CONTAINER
    {
        .name = "max_uts_container",
        .mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH,
        .type = UTS_CONTAINER,
        .fileOps = &SYS_USER_OPT

    },
#endif
#ifdef LOSCFG_UTS_CONTAINER
    {
        .name = "max_time_container",
        .mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH,
        .type = UTS_CONTAINER,
        .fileOps = &SYS_USER_OPT

    },
#endif
#ifdef LOSCFG_IPC_CONTAINER
    {
        .name = "max_ipc_container",
        .mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH,
        .type = IPC_CONTAINER,
        .fileOps = &SYS_USER_OPT
    },
#endif
#ifdef LOSCFG_NET_CONTAINER
    {
        .name = "max_net_container",
        .mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH,
        .type = NET_CONTAINER,
        .fileOps = &SYS_USER_OPT
    },
#endif
};

static int ProcCreateSysUser(struct ProcDirEntry *parent)
{
    struct ProcDataParm parm;
    for (int index = 0; index < (sizeof(g_sysUser) / sizeof(struct ProcSysUser)); index++) {
        struct ProcSysUser *sysUser = &g_sysUser[index];
        parm.data = (void *)(uintptr_t)sysUser->type;
        parm.dataType = PROC_DATA_STATIC;
        struct ProcDirEntry *userFile = ProcCreateData(sysUser->name, sysUser->mode, parent, sysUser->fileOps, &parm);
        if (userFile == NULL) {
            PRINT_ERR("create /proc/%s/%s error!\n", parent->name, sysUser->name);
            return -1;
        }
    }
    return 0;
}

#define PROC_SYS_USER_MODE (S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)
void ProcSysUserInit(void)
{
    struct ProcDirEntry *parentPDE = CreateProcEntry("sys", PROC_SYS_USER_MODE, NULL);
    if (parentPDE == NULL) {
        return;
    }
    struct ProcDirEntry *pde = CreateProcEntry("user", PROC_SYS_USER_MODE, parentPDE);
    if (pde == NULL) {
        PRINT_ERR("create /proc/process error!\n");
        return;
    }

    int ret = ProcCreateSysUser(pde);
    if (ret < 0) {
        PRINT_ERR("Create proc sys user failed!\n");
    }
    return;
}
#endif
