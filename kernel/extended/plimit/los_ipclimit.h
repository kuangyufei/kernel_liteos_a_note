/*
 * Copyright (c) 2022-2022 Huawei Device Co., Ltd. All rights reserved.
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

#ifndef _LOS_IPCLIMIT_H
#define _LOS_IPCLIMIT_H

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

typedef struct ProcIPCLimit {
    UINT32 mqCount;
    UINT32 mqFailedCount;
    UINT32 mqCountLimit;
    UINT32 shmSize;
    UINT32 shmFailedCount;
    UINT32 shmSizeLimit;
} ProcIPCLimit;

enum IPCStatType {
    IPC_STAT_TYPE_MQ  = 0,
    IPC_STAT_TYPE_SHM = 3,
    IPC_STAT_TYPE_BUT   // buttock
};

enum IPCStatOffset {
    IPC_STAT_OFFSET_MQ  = 0,
    IPC_STAT_OFFSET_SHM = 3,
    IPC_STAT_OFFSET_BUT   // buttock
};

enum StatItem {
    STAT_ITEM_TOTAL,
    STAT_ITEM_FAILED,
    STAT_ITEM_LIMIT,
    STAT_ITEM_BUT   // buttock
};

VOID OsIPCLimitInit(UINTPTR limite);
VOID *OsIPCLimitAlloc(VOID);
VOID OsIPCLimitFree(UINTPTR limite);
VOID OsIPCLimitCopy(UINTPTR dest, UINTPTR src);
BOOL OsIPCLimiteMigrateCheck(UINTPTR curr, UINTPTR parent);
VOID OsIPCLimitMigrate(UINTPTR currLimit, UINTPTR parentLimit, UINTPTR process);
BOOL OsIPCLimitAddProcessCheck(UINTPTR limit, UINTPTR process);
VOID OsIPCLimitAddProcess(UINTPTR limit, UINTPTR process);
VOID OsIPCLimitDelProcess(UINTPTR limit, UINTPTR process);
UINT32 OsIPCLimitSetMqLimit(ProcIPCLimit *ipcLimit, UINT32 value);
UINT32 OsIPCLimitSetShmLimit(ProcIPCLimit *ipcLimit, UINT32 value);
UINT32 OsIPCLimitMqAlloc(VOID);
VOID OsIPCLimitMqFree(VOID);
UINT32 OsIPCLimitShmAlloc(UINT32 size);
VOID OsIPCLimitShmFree(UINT32 size);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_IPCIMIT_H */
