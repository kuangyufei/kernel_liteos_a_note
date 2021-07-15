/*
 * Copyright (c) 2021-2021 Huawei Device Co., Ltd. All rights reserved.
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

/* ------------ includes ------------ */
#include "los_blackbox_common.h"
#ifdef LOSCFG_LIB_LIBC
#include "stdlib.h"
#include "unistd.h"
#endif
#ifdef LOSCFG_FS_VFS
#include "fs/fs.h"
#endif
#include "securec.h"
#include "los_memory.h"

/* ------------ local macroes ------------ */
/* ------------ local prototypes ------------ */
/* ------------ local function declarations ------------ */
/* ------------ global function declarations ------------ */
/* ------------ local variables ------------ */
/* ------------ function definitions ------------ */
int FullWriteFile(const char *filePath, const char *buf, size_t bufSize, int isAppend)
{
#ifdef LOSCFG_FS_VFS
    int fd;
    int totalToWrite = (int)bufSize;
    int totalWrite = 0;

    if (filePath == NULL || buf == NULL || bufSize == 0) {
        BBOX_PRINT_ERR("filePath: %p, buf: %p, bufSize: %lu!\n", filePath, buf, bufSize);
        return -1;
    }

    if (!IsLogPartReady()) {
        BBOX_PRINT_ERR("log path [%s] isn't ready to be written!\n", LOSCFG_LOG_ROOT_PATH);
        return -1;
    }
    fd = open(filePath, O_CREAT | O_RDWR | (isAppend ? O_APPEND : O_TRUNC), 0644);
    if (fd < 0) {
        BBOX_PRINT_ERR("Create file [%s] failed, fd: %d!\n", filePath, fd);
        return -1;
    }
    while (totalToWrite > 0) {
        int writeThisTime = write(fd, buf, totalToWrite);
        if (writeThisTime < 0) {
            BBOX_PRINT_ERR("Failed to write file [%s]!\n", filePath);
            (void)close(fd);
            return -1;
        }
        buf += writeThisTime;
        totalToWrite -= writeThisTime;
        totalWrite += writeThisTime;
    }
    (void)close(fd);

    return (totalWrite == (int)bufSize) ? 0 : -1;
#else
    (VOID)filePath;
    (VOID)buf;
    (VOID)bufSize;
    (VOID)isAppend;
    return -1;
#endif
}

int SaveBasicErrorInfo(const char *filePath, struct ErrorInfo *info)
{
    char *buf = NULL;

    if (filePath == NULL || info == NULL) {
        BBOX_PRINT_ERR("filePath: %p, event: %p!\n", filePath, info);
        return -1;
    }

    buf = LOS_MemAlloc(m_aucSysMem1, ERROR_INFO_MAX_LEN);
    if (buf == NULL) {
        BBOX_PRINT_ERR("LOS_MemAlloc failed!\n");
        return -1;
    }
    (void)memset_s(buf, ERROR_INFO_MAX_LEN, 0, ERROR_INFO_MAX_LEN);
    (void)snprintf_s(buf, ERROR_INFO_MAX_LEN, ERROR_INFO_MAX_LEN - 1,
        ERROR_INFO_HEADER_FORMAT, info->event, info->module, info->errorDesc);
    *(buf + ERROR_INFO_MAX_LEN - 1) = '\0';
    (void)FullWriteFile(filePath, buf, strlen(buf), 0);
    (void)LOS_MemFree(m_aucSysMem1, buf);

    return 0;
}

#ifdef LOSCFG_FS_VFS
bool IsLogPartReady(void)
{
    return access(LOSCFG_LOG_ROOT_PATH, W_OK) == 0;
}
#else
bool IsLogPartReady(void)
{
    return TRUE;
}
#endif