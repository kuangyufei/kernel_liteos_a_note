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
#define BBOX_DIR_MODE 0750  // 目录权限模式：所有者可读写执行，组用户可读执行，其他用户无权限
#define BBOX_FILE_MODE 0640  // 文件权限模式：所有者可读写，组用户可读，其他用户无权限
#endif

/* ------------ local prototypes ------------ */
/* ------------ local function declarations ------------ */
/* ------------ global function declarations ------------ */
/* ------------ local variables ------------ */
static bool g_isLogPartReady = FALSE;  // 日志分区就绪状态标识：FALSE-未就绪，TRUE-已就绪

/* ------------ function definitions ------------ */
/**
 * @brief 完整写入数据到文件
 * @param filePath 文件路径
 * @param buf 待写入数据缓冲区
 * @param bufSize 待写入数据大小
 * @param isAppend 是否追加模式：1-追加，0-覆盖
 * @return 0-成功，-1-失败
 */
int FullWriteFile(const char *filePath, const char *buf, size_t bufSize, int isAppend)
{
#ifdef LOSCFG_FS_VFS
    int fd;                          // 文件描述符
    int totalToWrite = (int)bufSize; // 剩余待写入字节数
    int totalWrite = 0;              // 已写入字节数

    if (filePath == NULL || buf == NULL || bufSize == 0) {  // 参数有效性检查
        BBOX_PRINT_ERR("filePath: %p, buf: %p, bufSize: %lu!\n", filePath, buf, bufSize);
        return -1;  // 参数无效返回错误
    }

    if (!IsLogPartReady()) {  // 检查日志分区是否就绪
        BBOX_PRINT_ERR("log path [%s] isn't ready to be written!\n", LOSCFG_BLACKBOX_LOG_ROOT_PATH);
        return -1;  // 分区未就绪返回错误
    }
    fd = open(filePath, O_CREAT | O_RDWR | (isAppend ? O_APPEND : O_TRUNC), BBOX_FILE_MODE);  // 打开/创建文件
    if (fd < 0) {  // 检查文件打开结果
        BBOX_PRINT_ERR("Create file [%s] failed, fd: %d!\n", filePath, fd);
        return -1;  // 打开失败返回错误
    }
    while (totalToWrite > 0) {  // 循环写入直到所有数据写完
        int writeThisTime = write(fd, buf, totalToWrite);  // 单次写入
        if (writeThisTime < 0) {  // 检查写入结果
            BBOX_PRINT_ERR("Failed to write file [%s]!\n", filePath);
            (void)close(fd);  // 关闭文件
            return -1;  // 写入失败返回错误
        }
        buf += writeThisTime;        // 移动缓冲区指针
        totalToWrite -= writeThisTime;  // 更新剩余待写入字节数
        totalWrite += writeThisTime;    // 更新已写入字节数
    }
    (void)fsync(fd);  // 刷新文件缓存到磁盘
    (void)close(fd);  // 关闭文件

    return (totalWrite == (int)bufSize) ? 0 : -1;  // 返回写入结果：完全写入返回0，否则返回-1
#else
    (VOID)filePath;   // 未启用VFS时，忽略参数
    (VOID)buf;        // 未启用VFS时，忽略参数
    (VOID)bufSize;    // 未启用VFS时，忽略参数
    (VOID)isAppend;   // 未启用VFS时，忽略参数
    return -1;        // 未启用VFS返回错误
#endif
}

/**
 * @brief 保存基本错误信息到文件
 * @param filePath 保存路径
 * @param info 错误信息结构体指针
 * @return 0-成功，-1-失败
 */
int SaveBasicErrorInfo(const char *filePath, const struct ErrorInfo *info)
{
    char *buf = NULL;  // 格式化错误信息缓冲区

    if (filePath == NULL || info == NULL) {  // 参数有效性检查
        BBOX_PRINT_ERR("filePath: %p, event: %p!\n", filePath, info);
        return -1;  // 参数无效返回错误
    }

    buf = LOS_MemAlloc(m_aucSysMem1, ERROR_INFO_MAX_LEN);  // 分配内存缓冲区
    if (buf == NULL) {  // 检查内存分配结果
        BBOX_PRINT_ERR("LOS_MemAlloc failed!\n");
        return -1;  // 分配失败返回错误
    }
    (void)memset_s(buf, ERROR_INFO_MAX_LEN, 0, ERROR_INFO_MAX_LEN);  // 初始化缓冲区
    if (snprintf_s(buf, ERROR_INFO_MAX_LEN, ERROR_INFO_MAX_LEN - 1,  // 格式化错误信息
        ERROR_INFO_HEADER_FORMAT, info->event, info->module, info->errorDesc) != -1) {
        *(buf + ERROR_INFO_MAX_LEN - 1) = '\0';  // 确保字符串结束符
        (void)FullWriteFile(filePath, buf, strlen(buf), 0);  // 写入文件（覆盖模式）
    } else {
        BBOX_PRINT_ERR("buf is not enough or snprintf_s failed!\n");  // 格式化失败
    }

    (void)LOS_MemFree(m_aucSysMem1, buf);  // 释放内存缓冲区

    return 0;  // 返回成功
}

#ifdef LOSCFG_FS_VFS
/**
 * @brief 挂载点遍历回调函数：检查日志分区是否已挂载
 * @param devPoint 设备节点
 * @param mountPoint 挂载点路径
 * @param statBuf 文件系统信息
 * @param arg 传入的挂载点参数
 * @return 0-继续遍历，非0-停止遍历
 */
static int IsLogPartMounted(const char *devPoint, const char *mountPoint, struct statfs *statBuf, void *arg)
{
    (void)devPoint;   // 未使用参数
    (void)statBuf;    // 未使用参数
    (void)arg;        // 未使用参数
    if (mountPoint != NULL && arg != NULL) {  // 参数有效性检查
        if (strcmp(mountPoint, (char *)arg) == 0) {  // 比较挂载点是否匹配目标
            g_isLogPartReady = TRUE;  // 设置日志分区就绪标识
        }
    }
    return 0;  // 继续遍历其他挂载点
}

/**
 * @brief 检查日志分区是否就绪
 * @return TRUE-就绪，FALSE-未就绪
 */
bool IsLogPartReady(void)
{
    if (!g_isLogPartReady) {  // 如果未就绪，则检查挂载状态
        (void)foreach_mountpoint((foreach_mountpoint_t)IsLogPartMounted, LOSCFG_BLACKBOX_LOG_PART_MOUNT_POINT);
    }
    return g_isLogPartReady;  // 返回就绪状态
}
#else
/**
 * @brief 检查日志分区是否就绪（未启用VFS版本）
 * @return 始终返回TRUE
 */
bool IsLogPartReady(void)
{
    return TRUE;  // 未启用VFS时默认就绪
}
#endif

#ifdef LOSCFG_FS_VFS
/**
 * @brief 创建新目录
 * @param dirPath 目录路径
 * @return 0-成功（或目录已存在），-1-失败
 */
int CreateNewDir(const char *dirPath)
{
    int ret;  // 系统调用返回值

    if (dirPath == NULL) {  // 参数有效性检查
        BBOX_PRINT_ERR("dirPath is NULL!\n");
        return -1;  // 参数无效返回错误
    }

    ret = access(dirPath, 0);  // 检查目录是否已存在
    if (ret == 0) {
        return 0;  // 目录已存在返回成功
    }
    ret = mkdir(dirPath, BBOX_DIR_MODE);  // 创建目录
    if (ret != 0) {  // 检查创建结果
        BBOX_PRINT_ERR("mkdir [%s] failed!\n", dirPath);
        return -1;  // 创建失败返回错误
    }

    return 0;  // 创建成功返回0
}

/**
 * @brief 递归创建日志目录
 * @param dirPath 目录路径（必须以/开头的绝对路径）
 * @return 0-成功，-1-失败
 */
int CreateLogDir(const char *dirPath)
{
    const char *temp = dirPath;  // 保存原始路径用于错误提示
    char curPath[PATH_MAX_LEN];  // 当前路径缓冲区
    int idx = 0;                 // 路径索引

    if (dirPath == NULL) {  // 参数有效性检查
        BBOX_PRINT_ERR("dirPath is NULL!\n");
        return -1;  // 参数无效返回错误
    }
    if (*dirPath != '/') {  // 检查是否为绝对路径
        BBOX_PRINT_ERR("Invalid dirPath: %s\n", dirPath);
        return -1;  // 非绝对路径返回错误
    }
    (void)memset_s(curPath, sizeof(curPath), 0, sizeof(curPath));  // 初始化路径缓冲区
    curPath[idx++] = *dirPath++;  // 复制根目录符号'/'
    while (*dirPath != '\0' && idx < sizeof(curPath)) {  // 遍历路径字符串
        if (*dirPath == '/') {  // 遇到路径分隔符
            if (CreateNewDir(curPath) != 0) {  // 创建当前子目录
                return -1;  // 创建失败返回错误
            }
        }
        curPath[idx] = *dirPath;  // 复制当前字符到缓冲区
        dirPath++;                // 移动源路径指针
        idx++;                    // 移动缓冲区索引
    }
    if (*dirPath != '\0') {  // 检查路径是否超出缓冲区长度
        BBOX_PRINT_ERR("dirPath [%s] is too long!\n", temp);
        return -1;  // 路径过长返回错误
    }

    return CreateNewDir(curPath);  // 创建最后一个目录并返回结果
}
#else
/**
 * @brief 创建日志目录（未启用VFS版本）
 * @param dirPath 目录路径（未使用）
 * @return 始终返回-1
 */
int CreateLogDir(const char *dirPath)
{
    (void)dirPath;  // 未使用参数
    return -1;      // 未启用VFS返回错误
}
#endif