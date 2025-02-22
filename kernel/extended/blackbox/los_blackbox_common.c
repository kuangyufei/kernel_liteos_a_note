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
#include "fs/mount.h"
#endif
#include "securec.h"
#include "los_memory.h"

/* ------------ local macroes ------------ */
#ifdef LOSCFG_FS_VFS
#define BBOX_DIR_MODE 0750
#define BBOX_FILE_MODE 0640
#endif

/* ------------ local prototypes ------------ */
/* ------------ local function declarations ------------ */
/* ------------ global function declarations ------------ */
/* ------------ local variables ------------ */
static bool g_isLogPartReady = FALSE;

/* ------------ function definitions ------------ */
/**
 * 完整写入文件内容（支持追加/覆盖模式）
 * 
 * @param filePath  目标文件路径
 * @param buf       要写入的数据缓冲区
 * @param bufSize   数据缓冲区大小（字节）
 * @param isAppend  是否追加模式（1=追加，0=覆盖）
 * @return          操作结果（0成功，-1失败）
 */
int FullWriteFile(const char *filePath, const char *buf, size_t bufSize, int isAppend)
{
#ifdef LOSCFG_FS_VFS  // 文件系统特性是否启用
    int fd;
    int totalToWrite = (int)bufSize;  // 剩余待写入字节数
    int totalWrite = 0;               // 已写入字节数

    /* 参数有效性检查 */
    if (filePath == NULL || buf == NULL || bufSize == 0) {
        BBOX_PRINT_ERR("filePath: %p, buf: %p, bufSize: %lu!\n", filePath, buf, bufSize);
        return -1;
    }

    /* 检查日志分区挂载状态 */
    if (!IsLogPartReady()) {
        BBOX_PRINT_ERR("log path [%s] isn't ready to be written!\n", LOSCFG_BLACKBOX_LOG_ROOT_PATH);
        return -1;
    }

    /* 打开/创建文件（追加模式使用O_APPEND，覆盖模式使用O_TRUNC） */
    fd = open(filePath, O_CREAT | O_RDWR | (isAppend ? O_APPEND : O_TRUNC), BBOX_FILE_MODE);
    if (fd < 0) {
        BBOX_PRINT_ERR("Create file [%s] failed, fd: %d!\n", filePath, fd);
        return -1;
    }

    /* 循环写入直至全部数据写入完成 */
    while (totalToWrite > 0) {
        int writeThisTime = write(fd, buf, totalToWrite);
        if (writeThisTime < 0) {
            BBOX_PRINT_ERR("Failed to write file [%s]!\n", filePath);
            (void)close(fd);
            return -1;
        }
        /* 调整缓冲指针和剩余写入量 */
        buf += writeThisTime;         // 移动缓冲区指针
        totalToWrite -= writeThisTime; // 减少剩余写入量
        totalWrite += writeThisTime;  // 累计已写入量
    }

    /* 确保数据落盘并关闭文件 */
    (void)fsync(fd);  // 强制刷新到存储介质
    (void)close(fd);

    return (totalWrite == (int)bufSize) ? 0 : -1; // 验证写入完整性
#else  // 文件系统特性未启用
    (VOID)filePath;
    (VOID)buf;
    (VOID)bufSize;
    (VOID)isAppend;
    return -1;
#endif
}
int SaveBasicErrorInfo(const char *filePath, const struct ErrorInfo *info)
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
    if (snprintf_s(buf, ERROR_INFO_MAX_LEN, ERROR_INFO_MAX_LEN - 1,
        ERROR_INFO_HEADER_FORMAT, info->event, info->module, info->errorDesc) != -1) {
        *(buf + ERROR_INFO_MAX_LEN - 1) = '\0';
        (void)FullWriteFile(filePath, buf, strlen(buf), 0);
    } else {
        BBOX_PRINT_ERR("buf is not enough or snprintf_s failed!\n");
    }

    (void)LOS_MemFree(m_aucSysMem1, buf);

    return 0;
}

#ifdef LOSCFG_FS_VFS
static int IsLogPartMounted(const char *devPoint, const char *mountPoint, struct statfs *statBuf, void *arg)
{
    (void)devPoint;
    (void)statBuf;
    (void)arg;
    if (mountPoint != NULL && arg != NULL) {
        if (strcmp(mountPoint, (char *)arg) == 0) {
            g_isLogPartReady = TRUE;
        }
    }
    return 0;
}

bool IsLogPartReady(void)
{
    if (!g_isLogPartReady) {
        (void)foreach_mountpoint((foreach_mountpoint_t)IsLogPartMounted, LOSCFG_BLACKBOX_LOG_PART_MOUNT_POINT);
    }
    return g_isLogPartReady;
}
#else
bool IsLogPartReady(void)
{
    return TRUE;
}
#endif

#ifdef LOSCFG_FS_VFS
int CreateNewDir(const char *dirPath)
{
    int ret;

    if (dirPath == NULL) {
        BBOX_PRINT_ERR("dirPath is NULL!\n");
        return -1;
    }

    ret = access(dirPath, 0);
    if (ret == 0) {
        return 0;
    }
    ret = mkdir(dirPath, BBOX_DIR_MODE);
    if (ret != 0) {
        BBOX_PRINT_ERR("mkdir [%s] failed!\n", dirPath);
        return -1;
    }

    return 0;
}

int CreateLogDir(const char *dirPath)
{
    const char *temp = dirPath;
    char curPath[PATH_MAX_LEN];
    int idx = 0;

    if (dirPath == NULL) {
        BBOX_PRINT_ERR("dirPath is NULL!\n");
        return -1;
    }
    if (*dirPath != '/') {
        BBOX_PRINT_ERR("Invalid dirPath: %s\n", dirPath);
        return -1;
    }
    (void)memset_s(curPath, sizeof(curPath), 0, sizeof(curPath));
    curPath[idx++] = *dirPath++;
    while (*dirPath != '\0' && idx < sizeof(curPath)) {
        if (*dirPath == '/') {
            if (CreateNewDir(curPath) != 0) {
                return -1;
            }
        }
        curPath[idx] = *dirPath;
        dirPath++;
        idx++;
    }
    if (*dirPath != '\0') {
        BBOX_PRINT_ERR("dirPath [%s] is too long!\n", temp);
        return -1;
    }

    return CreateNewDir(curPath);
}
#else
int CreateLogDir(const char *dirPath)
{
    (void)dirPath;
    return -1;
}
#endif