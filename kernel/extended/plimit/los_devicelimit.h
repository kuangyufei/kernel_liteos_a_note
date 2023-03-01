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

#ifndef _LOS_DEVICELIMIT_H
#define _LOS_DEVICELIMIT_H

#include "los_typedef.h"
#include "los_list.h"
#include "vfs_config.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define DEVLIMIT_ACC_MKNOD 1
#define DEVLIMIT_ACC_READ  2
#define DEVLIMIT_ACC_WRITE 4
#define DEVLIMIT_ACC_MASK (DEVLIMIT_ACC_MKNOD | DEVLIMIT_ACC_READ | DEVLIMIT_ACC_WRITE)

#define DEVLIMIT_DEV_BLOCK 1
#define DEVLIMIT_DEV_CHAR  2
#define DEVLIMIT_DEV_ALL   4 /* all devices */

#define DEVLIMIT_ALLOW 1
#define DEVLIMIT_DENY 2

#define ACCLEN 4

struct SeqBuf;
typedef struct TagPLimiterSet ProcLimitSet;

enum DevLimitBehavior {
    DEVLIMIT_DEFAULT_NONE,
    DEVLIMIT_DEFAULT_ALLOW,
    DEVLIMIT_DEFAULT_DENY,
};

typedef struct DevAccessItem {
    INT16 type;
    INT16 access;
    LOS_DL_LIST list;
    CHAR name[PATH_MAX];
} DevAccessItem;

typedef struct ProcDevLimit {
    struct ProcDevLimit *parent;
    UINT8 allowFile;
    UINT8 denyFile;
    LOS_DL_LIST accessList; // device belong to devicelimite
    enum DevLimitBehavior behavior;
} ProcDevLimit;

VOID OsDevLimitInit(UINTPTR limit);
VOID *OsDevLimitAlloc(VOID);
VOID OsDevLimitFree(UINTPTR limit);
VOID OsDevLimitCopy(UINTPTR dest, UINTPTR src);
UINT32 OsDevLimitWriteAllow(ProcLimitSet *plimit, const CHAR *buf, UINT32 size);
UINT32 OsDevLimitWriteDeny(ProcLimitSet *plimit, const CHAR *buf, UINT32 size);
UINT32 OsDevLimitShow(ProcDevLimit *devLimit, struct SeqBuf *seqBuf);
UINT32 OsDevLimitCheckPermission(INT32 vnodeType, const CHAR *pathName, INT32 flags);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
#endif /* _LOS_DEVICELIMIT_H */
