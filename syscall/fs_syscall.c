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

#include "syscall_pub.h"
#ifdef LOSCFG_FS_VFS
#include "errno.h"
#include "unistd.h"
#include "fs/fd_table.h"
#include "fs/file.h"
#include "fs/fs.h"
#include "fs/fs_operation.h"
#include "sys/mount.h"
#include "los_task_pri.h"
#include "sys/utsname.h"
#include "sys/uio.h"
#include "poll.h"
#include "sys/prctl.h"
#include "epoll.h"
#ifdef LOSCFG_KERNEL_DYNLOAD
#include "los_exec_elf.h"
#endif
#include "los_syscall.h"
#include "dirent.h"
#include "user_copy.h"
#include "los_vm_map.h"
#include "los_memory.h"
#include "los_strncpy_from_user.h"
#include "capability_type.h"
#include "capability_api.h"
#include "sys/statfs.h"

// 定义高位偏移位，用于64位偏移计算
#define HIGH_SHIFT_BIT 32
// 定义时间规格数组元素数量
#define TIMESPEC_TIMES_NUM  2
// 定义默认的epoll大小
#define EPOLL_DEFAULT_SIZE  100

/**
 * @brief 检查并更新文件属性的访问时间和修改时间
 * @param[in,out] attr 文件属性结构体指针
 * @param[in] times 时间数组，包含访问时间和修改时间
 * @return 成功返回ENOERR，失败返回错误码
 */
static int CheckNewAttrTime(struct IATTR *attr, struct timespec times[TIMESPEC_TIMES_NUM])
{
    int ret = ENOERR;  // 初始化返回值为成功
    struct timespec stp = {0};  // 用于获取当前时间的结构体

    if (times) {  // 如果提供了时间参数
        // 处理访问时间(ATIME)
        if (times[0].tv_nsec == UTIME_OMIT) {  // 忽略访问时间更新
            attr->attr_chg_valid &= ~CHG_ATIME;  // 清除访问时间更新标志
        } else if (times[0].tv_nsec == UTIME_NOW) {  // 使用当前时间作为访问时间
            ret = clock_gettime(CLOCK_REALTIME, &stp);  // 获取当前实时时间
            if (ret < 0) {
                return -get_errno();  // 获取失败返回错误码
            }
            attr->attr_chg_atime = (unsigned int)stp.tv_sec;  // 设置访问时间为当前秒数
            attr->attr_chg_valid |= CHG_ATIME;  // 设置访问时间更新标志
        } else {  // 使用提供的时间作为访问时间
            attr->attr_chg_atime = (unsigned int)times[0].tv_sec;  // 设置访问时间
            attr->attr_chg_valid |= CHG_ATIME;  // 设置访问时间更新标志
        }

        // 处理修改时间(MTIME)
        if (times[1].tv_nsec == UTIME_OMIT) {  // 忽略修改时间更新
            attr->attr_chg_valid &= ~CHG_MTIME;  // 清除修改时间更新标志
        } else if (times[1].tv_nsec == UTIME_NOW) {  // 使用当前时间作为修改时间
            ret = clock_gettime(CLOCK_REALTIME, &stp);  // 获取当前实时时间
            if (ret < 0) {
                return -get_errno();  // 获取失败返回错误码
            }
            attr->attr_chg_mtime = (unsigned int)stp.tv_sec;  // 设置修改时间为当前秒数
            attr->attr_chg_valid |= CHG_MTIME;  // 设置修改时间更新标志
        } else {  // 使用提供的时间作为修改时间
            attr->attr_chg_mtime = (unsigned int)times[1].tv_sec;  // 设置修改时间
            attr->attr_chg_valid |= CHG_MTIME;  // 设置修改时间更新标志
        }
    } else {  // 未提供时间参数，使用当前时间同时更新访问时间和修改时间
        ret = clock_gettime(CLOCK_REALTIME, &stp);  // 获取当前实时时间
        if (ret < 0) {
            return -get_errno();  // 获取失败返回错误码
        }
        attr->attr_chg_atime = (unsigned int)stp.tv_sec;  // 设置访问时间
        attr->attr_chg_mtime = (unsigned int)stp.tv_sec;  // 设置修改时间
        attr->attr_chg_valid |= CHG_ATIME;  // 设置访问时间更新标志
        attr->attr_chg_valid |= CHG_MTIME;  // 设置修改时间更新标志
    }

    return ret;  // 返回操作结果
}

/**
 * @brief 获取文件的完整路径
 * @param[in] fd 文件描述符
 * @param[in] path 相对路径
 * @param[out] filePath 输出完整路径
 * @return 成功返回0，失败返回错误码
 */
static int GetFullpathNull(int fd, const char *path, char **filePath)
{
    int ret;  // 返回值
    char *fullPath = NULL;  // 完整路径缓冲区
    struct file *file = NULL;  // 文件结构体指针

    if ((fd != AT_FDCWD) && (path == NULL)) {  // 如果fd不是当前目录且路径为NULL
        fd = GetAssociatedSystemFd(fd);  // 获取系统关联的文件描述符
        ret = fs_getfilep(fd, &file);  // 通过fd获取文件结构体
        if (ret < 0) {
            return -get_errno();  // 获取失败返回错误码
        }
        fullPath = strdup(file->f_path);  // 复制文件路径
        if (fullPath == NULL) {
            ret = -ENOMEM;  // 内存分配失败
        }
    } else {  // 正常获取完整路径
        ret = GetFullpath(fd, path, &fullPath);  // 获取完整路径
        if (ret < 0) {
            return ret;  // 获取失败返回错误码
        }
    }

    *filePath = fullPath;  // 输出完整路径
    return ret;  // 返回操作结果
}

/**
 * @brief 检查用户空间的iovec数组是否有效
 * @param[in] iov iovec数组指针
 * @param[in] iovcnt 数组元素数量
 * @return 成功返回iovcnt，失败返回无效元素的索引
 */
static int UserIovItemCheck(const struct iovec *iov, const int iovcnt)
{
    int i;  // 循环变量
    for (i = 0; i < iovcnt; ++i) {  // 遍历所有iovec元素
        if (iov[i].iov_len == 0) {  // 跳过长度为0的元素
            continue;
        }

        // 检查缓冲区地址是否在用户空间
        if (!LOS_IsUserAddressRange((vaddr_t)(UINTPTR)iov[i].iov_base, iov[i].iov_len)) {
            return i;  // 地址无效，返回当前索引
        }
    }
    return iovcnt;  // 所有元素有效，返回元素总数
}

/**
 * @brief 从用户空间复制iovec数组并检查有效性
 * @param[out] iovBuf 输出内核空间的iovec数组
 * @param[in] iov 用户空间的iovec数组
 * @param[in] iovcnt 数组元素数量
 * @param[out] valid_iovcnt 有效元素数量
 * @return 成功返回0，失败返回错误码
 */
static int UserIovCopy(struct iovec **iovBuf, const struct iovec *iov, const int iovcnt, int *valid_iovcnt)
{
    int ret;  // 返回值
    int bufLen = iovcnt * sizeof(struct iovec);  // 计算缓冲区大小
    if (bufLen < 0) {  // 检查缓冲区大小是否有效
        return -EINVAL;  // 无效大小返回错误
    }

    // 分配内核空间缓冲区
    *iovBuf = (struct iovec*)LOS_MemAlloc(OS_SYS_MEM_ADDR, bufLen);
    if (*iovBuf == NULL) {  // 检查分配是否成功
        return -ENOMEM;  // 内存不足返回错误
    }

    // 从用户空间复制iovec数组
    if (LOS_ArchCopyFromUser(*iovBuf, iov, bufLen) != 0) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, *iovBuf);  // 复制失败释放内存
        return -EFAULT;  // 返回访问错误
    }

    // 检查iovec数组有效性
    ret = UserIovItemCheck(*iovBuf, iovcnt);
    if (ret == 0) {  // 第一个元素就无效
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, *iovBuf);  // 释放内存
        return -EFAULT;  // 返回访问错误
    }

    *valid_iovcnt = ret;  // 设置有效元素数量
    return 0;  // 返回成功
}

/**
 * @brief 将用户空间的pollfd转换为系统空间表示
 * @param[in,out] fds pollfd数组
 * @param[in] nfds 数组元素数量
 * @param[out] pollFdsBak 保存原始fd的数组
 * @return 成功返回0，失败返回-1
 */
static int PollfdToSystem(struct pollfd *fds, nfds_t nfds, int **pollFdsBak)
{
    if ((nfds != 0 && fds == NULL) || (pollFdsBak == NULL)) {  // 参数有效性检查
        set_errno(EINVAL);  // 设置无效参数错误
        return -1;
    }
    if (nfds == 0) {  // 没有需要处理的fd
        return 0;
    }
    // 分配保存原始fd的缓冲区
    int *pollFds = (int *)malloc(sizeof(int) * nfds);
    if (pollFds == NULL) {  // 内存分配失败
        set_errno(ENOMEM);  // 设置内存不足错误
        return -1;
    }
    for (int i = 0; i < nfds; ++i) {  // 遍历所有pollfd
        struct pollfd *p_fds = &fds[i];
        pollFds[i] = p_fds->fd;  // 保存原始fd
        if (p_fds->fd < 0) {  // 检查fd是否有效
            set_errno(EBADF);  // 设置无效fd错误
            free(pollFds);  // 释放内存
            return -1;
        }
        p_fds->fd = GetAssociatedSystemFd(p_fds->fd);  // 转换为系统fd
    }
    *pollFdsBak = pollFds;  // 输出保存的原始fd数组
    return 0;  // 返回成功
}

/**
 * @brief 恢复pollfd数组中的原始fd
 * @param[in,out] fds pollfd数组
 * @param[in] nfds 数组元素数量
 * @param[in] pollFds 保存原始fd的数组
 */
static void RestorePollfd(struct pollfd *fds, nfds_t nfds, const int *pollFds)
{
    if ((fds == NULL) || (pollFds == NULL)) {  // 参数有效性检查
        return;
    }
    for (int i = 0; i < nfds; ++i) {  // 遍历所有pollfd
        struct pollfd *p_fds = &fds[i];
        p_fds->fd = pollFds[i];  // 恢复原始fd
    }
}

/**
 * @brief 处理用户空间的poll系统调用
 * @param[in,out] fds pollfd数组
 * @param[in] nfds 数组元素数量
 * @param[in] timeout 超时时间(毫秒)
 * @return 成功返回就绪的文件描述符数量，失败返回-1
 */
static int UserPoll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    int *pollFds = NULL;  // 保存原始fd的数组
    int ret = PollfdToSystem(fds, nfds, &pollFds);  // 转换fd为系统表示
    if (ret < 0) {
        return -1;  // 转换失败返回错误
    }

    ret = poll(fds, nfds, timeout);  // 调用系统poll函数

    RestorePollfd(fds, nfds, pollFds);  // 恢复原始fd

    free(pollFds);  // 释放内存
    return ret;  // 返回poll结果
}

/**
 * @brief 系统调用：关闭文件描述符
 * @param[in] fd 文件描述符
 * @return 成功返回0，失败返回错误码
 */
int SysClose(int fd)
{
    int ret;  // 返回值

    /* 将进程fd转换为系统全局fd */
    int sysfd = DisassociateProcessFd(fd);

    ret = close(sysfd);  // 关闭系统fd
    if (ret < 0) {  // 关闭失败
        AssociateSystemFd(fd, sysfd);  // 重新关联fd
        return -get_errno();  // 返回错误码
    }
    FreeProcessFd(fd);  // 释放进程fd
    return ret;  // 返回成功
}

/**
 * @brief 系统调用：从文件读取数据
 * @param[in] fd 文件描述符
 * @param[out] buf 数据缓冲区
 * @param[in] nbytes 要读取的字节数
 * @return 成功返回读取的字节数，失败返回错误码
 */
ssize_t SysRead(int fd, void *buf, size_t nbytes)
{
    int ret;  // 返回值

    if (nbytes == 0) {  // 读取字节数为0
        return 0;  // 直接返回0
    }

    // 检查缓冲区是否在用户空间
    if (!LOS_IsUserAddressRange((vaddr_t)(UINTPTR)buf, nbytes)) {
        return -EFAULT;  // 缓冲区无效返回错误
    }

    /* 将进程fd转换为系统全局fd */
    fd = GetAssociatedSystemFd(fd);

    ret = read(fd, buf, nbytes);  // 调用系统read函数
    if (ret < 0) {
        return -get_errno();  // 读取失败返回错误码
    }
    return ret;  // 返回读取的字节数
}

/**
 * @brief 系统调用：向文件写入数据
 * @param[in] fd 文件描述符
 * @param[in] buf 数据缓冲区
 * @param[in] nbytes 要写入的字节数
 * @return 成功返回写入的字节数，失败返回错误码
 */
ssize_t SysWrite(int fd, const void *buf, size_t nbytes)
{
    int ret;  // 返回值

    if (nbytes == 0) {  // 写入字节数为0
        return 0;  // 直接返回0
    }

    // 检查缓冲区是否在用户空间
    if (!LOS_IsUserAddressRange((vaddr_t)(UINTPTR)buf, nbytes)) {
        return -EFAULT;  // 缓冲区无效返回错误
    }

    /* 将进程fd转换为系统全局fd */
    int sysfd = GetAssociatedSystemFd(fd);
    ret = write(sysfd, buf, nbytes);  // 调用系统write函数
    if (ret < 0) {
        return -get_errno();  // 写入失败返回错误码
    }
    return ret;  // 返回写入的字节数
}

// int vfs_normalize_path(const char *directory, const char *filename, char **pathname)
#ifdef LOSCFG_PID_CONTAINER
#ifdef LOSCFG_PROC_PROCESS_DIR
#define PROCESS_DIR_ROOT   "/proc"  // 进程目录根路径

/**
 * @brief 获取路径中的下一个文件名
 * @param[in,out] pos 当前路径位置指针
 * @param[out] len 文件名长度
 * @return 文件名起始指针，无文件名返回NULL
 */
static char *NextName(char *pos, uint8_t *len)
{
    char *name = NULL;  // 文件名指针
    while (*pos != 0 && *pos == '/') {  // 跳过路径分隔符
        pos++;
    }
    if (*pos == '\0') {  // 到达路径末尾
        return NULL;
    }
    name = (char *)pos;  // 记录文件名起始位置
    while (*pos != '\0' && *pos != '/') {  // 查找文件名结束位置
        pos++;
    }
    *len = pos - name;  // 计算文件名长度
    return name;  // 返回文件名起始指针
}

/**
 * @brief 获取进程的真实PID
 * @param[in] pid 虚拟PID
 * @return 真实PID，失败返回0
 */
static unsigned int ProcRealProcessIDGet(unsigned int pid)
{
    unsigned int intSave;  // 中断标志
    if (OS_PID_CHECK_INVALID(pid)) {  // 检查PID是否有效
        return 0;
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    LosProcessCB *pcb = OsGetPCBFromVpid(pid);  // 通过虚拟PID获取进程控制块
    if (OsProcessIsInactive(pcb)) {  // 检查进程是否活跃
        SCHEDULER_UNLOCK(intSave);  // 打开调度器
        return 0;
    }

    int rootPid = OsGetRootPid(pcb);  // 获取根PID
    SCHEDULER_UNLOCK(intSave);  // 打开调度器
    if ((rootPid == OS_INVALID_VALUE) || (rootPid == pid)) {  // 检查根PID是否有效
        return 0;
    }

    return rootPid;  // 返回真实PID
}

/**
 * @brief 获取进程的真实目录路径
 * @param[in,out] path 路径缓冲区
 * @return 成功返回0，失败返回错误码
 */
static int ProcRealProcessDirGet(char *path)
{
    char pidBuf[PATH_MAX] = {0};  // PID缓冲区
    char *fullPath = NULL;  // 完整路径
    uint8_t strLen = 0;  // 字符串长度
    int pid, rootPid;  // PID和根PID
    int ret = vfs_normalize_path(NULL, path, &fullPath);  // 标准化路径
    if (ret < 0) {
        return ret;  // 标准化失败返回错误
    }

    int procLen = strlen(PROCESS_DIR_ROOT);  // 获取进程目录根路径长度
    if (strncmp(fullPath, PROCESS_DIR_ROOT, procLen) != 0) {  // 检查是否为进程目录
        free(fullPath);  // 释放内存
        return 0;
    }

    char *pidStr = NextName(fullPath + procLen, &strLen);  // 获取PID字符串
    if (pidStr == NULL) {  // 没有PID部分
        free(fullPath);  // 释放内存
        return 0;
    }

    if ((*pidStr <= '0') || (*pidStr > '9')) {  // 检查是否为数字
        free(fullPath);  // 释放内存
        return 0;
    }

    // 复制PID字符串
    if (memcpy_s(pidBuf, PATH_MAX, pidStr, strLen) != EOK) {
        free(fullPath);  // 释放内存
        return 0;
    }
    pidBuf[strLen] = '\0';  // 添加字符串结束符

    pid = atoi(pidBuf);  // 转换为整数PID
    if (pid == 0) {  // PID无效
        free(fullPath);  // 释放内存
        return 0;
    }

    rootPid = ProcRealProcessIDGet((unsigned)pid);  // 获取真实PID
    if (rootPid == 0) {  // 获取失败
        free(fullPath);  // 释放内存
        return 0;
    }

    // 构建真实路径
    if (snprintf_s(path, PATH_MAX + 1, PATH_MAX, "/proc/%d%s", rootPid, (pidStr + strLen)) < 0) {
        free(fullPath);  // 释放内存
        return -EFAULT;  // 格式化失败返回错误
    }

    free(fullPath);  // 释放内存
    return 0;  // 返回成功
}
#endif
#endif

/**
 * @brief 获取标准化后的路径
 * @param[in] path 原始路径
 * @param[out] pathRet 输出标准化后的路径
 * @return 成功返回0，失败返回错误码
 */
static int GetPath(const char *path, char **pathRet)
{
    int ret = UserPathCopy(path, pathRet);  // 复制用户路径
    if (ret != 0) {
        return ret;  // 复制失败返回错误
    }
#ifdef LOSCFG_PID_CONTAINER
#ifdef LOSCFG_PROC_PROCESS_DIR
    ret = ProcRealProcessDirGet(*pathRet);  // 获取真实进程目录
    if (ret != 0) {
        return ret;  // 获取失败返回错误
    }
#endif
#endif
    return 0;  // 返回成功
}

/**
 * @brief 系统调用：打开文件
 * @param[in] path 文件路径
 * @param[in] oflags 打开标志
 * @param[in] ... 可变参数，文件权限mode
 * @return 成功返回文件描述符，失败返回错误码
 */
int SysOpen(const char *path, int oflags, ...)
{
    int ret;  // 返回值
    int procFd = -1;  // 进程文件描述符
    mode_t mode = DEFAULT_FILE_MODE; /* 0666: 文件默认读写权限 */
    char *pathRet = NULL;  // 标准化后的路径

    if (path != NULL) {  // 如果提供了路径
        ret = GetPath(path, &pathRet);  // 获取标准化路径
        if (ret != 0) {
            goto ERROUT;  // 获取失败跳转到错误处理
        }
    }

    procFd = AllocProcessFd();  // 分配进程文件描述符
    if (procFd < 0) {  // 分配失败
        ret = -EMFILE;  // 进程文件描述符用尽
        goto ERROUT;  // 跳转到错误处理
    }

    if (oflags & O_CLOEXEC) {  // 如果设置了执行时关闭标志
        SetCloexecFlag(procFd);  // 设置进程fd的CLOEXEC标志
    }

    if ((unsigned int)oflags & O_DIRECTORY) {  // 如果要打开目录
        ret = do_opendir(pathRet, oflags);  // 打开目录
    } else {  // 打开文件
#ifdef LOSCFG_FILE_MODE
        va_list ap;  // 可变参数列表
        va_start(ap, oflags);  // 初始化可变参数
        mode = va_arg(ap, int);  // 获取文件权限
        va_end(ap);  // 结束可变参数
#endif

        ret = do_open(AT_FDCWD, pathRet, oflags, mode);  // 打开文件
    }

    if (ret < 0) {  // 打开失败
        ret = -get_errno();  // 获取错误码
        goto ERROUT;  // 跳转到错误处理
    }

    AssociateSystemFd(procFd, ret);  // 关联进程fd和系统fd
    if (pathRet != NULL) {
        LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);  // 释放路径内存
    }
    return procFd;  // 返回进程文件描述符

ERROUT:  // 错误处理
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);  // 释放路径内存
    }
    if (procFd >= 0) {
        FreeProcessFd(procFd);  // 释放进程fd
    }
    return ret;  // 返回错误码
}

/**
 * @brief 系统调用：创建文件
 * @param[in] pathname 文件路径
 * @param[in] mode 文件权限
 * @return 成功返回文件描述符，失败返回错误码
 */
int SysCreat(const char *pathname, mode_t mode)
{
    int ret = 0;  // 返回值
    char *pathRet = NULL;  // 标准化后的路径

    if (pathname != NULL) {  // 如果提供了路径
        ret = UserPathCopy(pathname, &pathRet);  // 复制用户路径
        if (ret != 0) {
            goto OUT;  // 复制失败跳转到结束
        }
    }

    int procFd = AllocProcessFd();  // 分配进程文件描述符
    if (procFd  < 0) {  // 分配失败
        ret = -EMFILE;  // 进程文件描述符用尽
        goto OUT;  // 跳转到结束
    }

    // 以创建、截断、只写方式打开文件
    ret = open((pathname ? pathRet : NULL), O_CREAT | O_TRUNC | O_WRONLY, mode);
    if (ret < 0) {  // 打开失败
        FreeProcessFd(procFd);  // 释放进程fd
        ret = -get_errno();  // 获取错误码
    } else {
        AssociateSystemFd(procFd, ret);  // 关联进程fd和系统fd
        ret = procFd;  // 返回进程fd
    }

OUT:  // 结束处理
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);  // 释放路径内存
    }
    return ret;  // 返回结果
}

/**
 * @brief 系统调用：创建硬链接
 * @param[in] oldpath 原文件路径
 * @param[in] newpath 链接文件路径
 * @return 成功返回0，失败返回错误码
 */
int SysLink(const char *oldpath, const char *newpath)
{
    int ret;  // 返回值
    char *oldpathRet = NULL;  // 原路径标准化结果
    char *newpathRet = NULL;  // 新路径标准化结果

    if (oldpath != NULL) {  // 处理原路径
        ret = UserPathCopy(oldpath, &oldpathRet);  // 复制用户路径
        if (ret != 0) {
            goto OUT;  // 复制失败跳转到结束
        }
    }

    if (newpath != NULL) {  // 处理新路径
        ret = UserPathCopy(newpath, &newpathRet);  // 复制用户路径
        if (ret != 0) {
            goto OUT;  // 复制失败跳转到结束
        }
    }

    ret = link(oldpathRet, newpathRet);  // 创建硬链接
    if (ret < 0) {
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 结束处理
    if (oldpathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, oldpathRet);  // 释放原路径内存
    }
    if (newpathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, newpathRet);  // 释放新路径内存
    }
    return ret;  // 返回结果
}

/**
 * @brief 系统调用：读取符号链接
 * @param[in] pathname 符号链接路径
 * @param[out] buf 输出缓冲区
 * @param[in] bufsize 缓冲区大小
 * @return 成功返回读取的字节数，失败返回错误码
 */
ssize_t SysReadlink(const char *pathname, char *buf, size_t bufsize)
{
    ssize_t ret;  // 返回值
    char *pathRet = NULL;  // 标准化后的路径

    if (bufsize == 0) {  // 缓冲区大小为0
        return -EINVAL;  // 返回无效参数错误
    }

    if (pathname != NULL) {  // 处理路径
        ret = UserPathCopy(pathname, &pathRet);  // 复制用户路径
        if (ret != 0) {
            goto OUT;  // 复制失败跳转到结束
        }

#ifdef LOSCFG_PID_CONTAINER
#ifdef LOSCFG_PROC_PROCESS_DIR
        ret = ProcRealProcessDirGet(pathRet);  // 获取真实进程目录
        if (ret != 0) {
            goto OUT;  // 获取失败跳转到结束
        }
#endif
#endif
    }

    // 检查缓冲区是否在用户空间
    if (!LOS_IsUserAddressRange((vaddr_t)(UINTPTR)buf, bufsize)) {
        ret = -EFAULT;  // 缓冲区无效返回错误
        goto OUT;
    }

    ret = readlink(pathRet, buf, bufsize);  // 读取符号链接
    if (ret < 0) {
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 结束处理
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);  // 释放路径内存
    }
    return ret;  // 返回结果
}

/**
 * @brief 系统调用：创建符号链接
 * @param[in] target 目标文件路径
 * @param[in] linkpath 链接文件路径
 * @return 成功返回0，失败返回错误码
 */
int SysSymlink(const char *target, const char *linkpath)
{
    int ret;  // 返回值
    char *targetRet = NULL;  // 目标路径标准化结果
    char *pathRet = NULL;  // 链接路径标准化结果

    if (target != NULL) {  // 处理目标路径
        ret = UserPathCopy(target, &targetRet);  // 复制用户路径
        if (ret != 0) {
            goto OUT;  // 复制失败跳转到结束
        }
    }

    if (linkpath != NULL) {  // 处理链接路径
        ret = UserPathCopy(linkpath, &pathRet);  // 复制用户路径
        if (ret != 0) {
            goto OUT;  // 复制失败跳转到结束
        }
    }

    ret = symlink(targetRet, pathRet);  // 创建符号链接
    if (ret < 0) {
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 结束处理
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);  // 释放链接路径内存
    }

    if (targetRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, targetRet);  // 释放目标路径内存
    }
    return ret;  // 返回结果
}

/**
 * @brief 系统调用：删除文件或符号链接
 * @param[in] pathname 文件路径
 * @return 成功返回0，失败返回错误码
 */
int SysUnlink(const char *pathname)
{
    int ret;  // 返回值
    char *pathRet = NULL;  // 标准化后的路径

    if (pathname != NULL) {  // 处理路径
        ret = UserPathCopy(pathname, &pathRet);  // 复制用户路径
        if (ret != 0) {
            goto OUT;  // 复制失败跳转到结束
        }
    }

    ret = do_unlink(AT_FDCWD, (pathname ? pathRet : NULL));  // 执行删除操作
    if (ret < 0) {
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 结束处理
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);  // 释放路径内存
    }
    return ret;  // 返回结果
}

#ifdef LOSCFG_KERNEL_DYNLOAD
/**
 * @brief 系统调用：执行程序
 * @param[in] fileName 程序路径
 * @param[in] argv 参数列表
 * @param[in] envp 环境变量列表
 * @return 成功不返回，失败返回错误码
 */
int SysExecve(const char *fileName, char *const *argv, char *const *envp)
{
    return LOS_DoExecveFile(fileName, argv, envp);  // 执行程序
}
#endif

/**
 * @brief 系统调用：通过文件描述符改变当前工作目录
 * @param[in] fd 目录文件描述符
 * @return 成功返回0，失败返回错误码
 */
int SysFchdir(int fd)
{
    int ret;  // 返回值
    int sysFd;  // 系统文件描述符
    struct file *file = NULL;  // 文件结构体指针

    sysFd = GetAssociatedSystemFd(fd);  // 获取系统fd
    if (sysFd < 0) {
        return -EBADF;  // 无效fd返回错误
    }

    ret = fs_getfilep(sysFd, &file);  // 获取文件结构体
    if (ret < 0) {
        return -get_errno();  // 获取失败返回错误码
    }

    ret = chdir(file->f_path);  // 改变工作目录
    if (ret < 0) {
        ret = -get_errno();  // 获取错误码
    }

    return ret;  // 返回结果
}

/**
 * @brief 系统调用：改变当前工作目录
 * @param[in] path 目录路径
 * @return 成功返回0，失败返回错误码
 */
int SysChdir(const char *path)
{
    int ret;  // 返回值
    char *pathRet = NULL;  // 标准化后的路径

    if (path != NULL) {  // 处理路径
        ret = UserPathCopy(path, &pathRet);  // 复制用户路径
        if (ret != 0) {
            goto OUT;  // 复制失败跳转到结束
        }
    }

    ret = chdir(path ? pathRet : NULL);  // 改变工作目录
    if (ret < 0) {
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 结束处理
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);  // 释放路径内存
    }
    return ret;  // 返回结果
}

/**
 * @brief 系统调用：文件偏移量定位
 * @param[in] fd 文件描述符
 * @param[in] offset 偏移量
 * @param[in] whence 定位方式
 * @return 成功返回新的偏移量，失败返回-1
 */
off_t SysLseek(int fd, off_t offset, int whence)
{
    /* 将进程fd转换为系统全局fd */
    fd = GetAssociatedSystemFd(fd);

    return _lseek(fd, offset, whence);  // 调用底层lseek函数
}

/**
 * @brief 系统调用：64位文件偏移量定位
 * @param[in] fd 文件描述符
 * @param[in] offsetHigh 高位偏移量
 * @param[in] offsetLow 低位偏移量
 * @param[out] result 输出新的64位偏移量
 * @param[in] whence 定位方式
 * @return 成功返回0，失败返回错误码
 */
off64_t SysLseek64(int fd, int offsetHigh, int offsetLow, off64_t *result, int whence)
{
    off64_t ret;  // 返回值
    off64_t res;  // 64位偏移量结果
    int retVal;  // 复制结果

    /* 将进程fd转换为系统全局fd */
    fd = GetAssociatedSystemFd(fd);

    // 调用64位lseek函数
    ret = _lseek64(fd, offsetHigh, offsetLow, &res, whence);
    if (ret != 0) {
        return ret;  // 失败返回错误码
    }

    // 将结果复制到用户空间
    retVal = LOS_ArchCopyToUser(result, &res, sizeof(off64_t));
    if (retVal != 0) {
        return -EFAULT;  // 复制失败返回错误
    }

    return 0;  // 返回成功
}

#ifdef LOSCFG_FS_NFS
/**
 * @brief NFS挂载引用函数（弱引用）
 * @param[in] serverIpAndPath 服务器IP和路径
 * @param[in] mountPath 挂载点路径
 * @param[in] uid 用户ID
 * @param[in] gid 组ID
 * @return 成功返回0，失败返回错误码
 */
static int NfsMountRef(const char *serverIpAndPath, const char *mountPath,
                       unsigned int uid, unsigned int gid) __attribute__((weakref("nfs_mount")));

/**
 * @brief NFS挂载函数
 * @param[in] serverIpAndPath 服务器IP和路径
 * @param[in] mountPath 挂载点路径
 * @param[in] uid 用户ID
 * @param[in] gid 组ID
 * @return 成功返回0，失败返回错误码
 */
static int NfsMount(const char *serverIpAndPath, const char *mountPath,
                    unsigned int uid, unsigned int gid)
{
    int ret;  // 返回值

    if ((serverIpAndPath == NULL) || (mountPath == NULL)) {  // 参数检查
        return -EINVAL;  // 无效参数返回错误
    }
    ret = NfsMountRef(serverIpAndPath, mountPath, uid, gid);  // 调用实际挂载函数
    if (ret < 0) {
        ret = -get_errno();  // 获取错误码
    }
    return ret;  // 返回结果
}
#endif

/**
 * @brief 系统调用：挂载文件系统
 * @param[in] source 源设备或路径
 * @param[in] target 挂载点路径
 * @param[in] filesystemtype 文件系统类型
 * @param[in] mountflags 挂载标志
 * @param[in] data 挂载数据
 * @return 成功返回0，失败返回错误码
 */
int SysMount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags,
             const void *data)
{
    int ret;  // 返回值
    char *sourceRet = NULL;  // 源路径标准化结果
    char *targetRet = NULL;  // 目标路径标准化结果
    char *dataRet = NULL;  // 数据路径标准化结果
    char fstypeRet[FILESYSTEM_TYPE_MAX + 1] = {0};  // 文件系统类型缓冲区

    if (!IsCapPermit(CAP_FS_MOUNT)) {  // 检查挂载权限
        return -EPERM;  // 无权限返回错误
    }

    if (target != NULL) {  // 处理目标路径
        ret = UserPathCopy(target, &targetRet);  // 复制用户路径
        if (ret != 0) {
            goto OUT;  // 复制失败跳转到结束
        }
    }

    if (filesystemtype != NULL) {  // 处理文件系统类型
        // 从用户空间复制文件系统类型
        ret = LOS_StrncpyFromUser(fstypeRet, filesystemtype, FILESYSTEM_TYPE_MAX + 1);
        if (ret < 0) {
            goto OUT;  // 复制失败跳转到结束
        } else if (ret > FILESYSTEM_TYPE_MAX) {  // 类型名过长
            ret = -ENODEV;  // 不支持的文件系统
            goto OUT;
        }

        // 如果不是ramfs且提供了源路径
        if (strcmp(fstypeRet, "ramfs") && (source != NULL)) {
            ret = UserPathCopy(source, &sourceRet);  // 复制源路径
            if (ret != 0) {
                goto OUT;  // 复制失败跳转到结束
            }
        }
#ifdef LOSCFG_FS_NFS
        if (strcmp(fstypeRet, "nfs") == 0) {  // 如果是NFS文件系统
            ret = NfsMount(sourceRet, targetRet, 0, 0);  // 调用NFS挂载
            goto OUT;  // 跳转到结束
        }
#endif
    }

    if (data != NULL) {  // 处理挂载数据
        ret = UserPathCopy(data, &dataRet);  // 复制数据路径
        if (ret != 0) {
            goto OUT;  // 复制失败跳转到结束
        }
    }

    // 执行挂载操作
    ret = mount(sourceRet, targetRet, (filesystemtype ? fstypeRet : NULL), mountflags, dataRet);
    if (ret < 0) {
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 结束处理
    if (sourceRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, sourceRet);  // 释放源路径内存
    }
    if (targetRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, targetRet);  // 释放目标路径内存
    }
    if (dataRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, dataRet);  // 释放数据路径内存
    }
    return ret;  // 返回结果
}

/**
 * @brief 系统调用：卸载文件系统
 * @param[in] target 挂载点路径
 * @return 成功返回0，失败返回错误码
 */
int SysUmount(const char *target)
{
    int ret;  // 返回值
    char *pathRet = NULL;  // 标准化后的路径

    if (!IsCapPermit(CAP_FS_MOUNT)) {  // 检查卸载权限
        return -EPERM;  // 无权限返回错误
    }

    if (target != NULL) {  // 处理路径
        ret = UserPathCopy(target, &pathRet);  // 复制用户路径
        if (ret != 0) {
            goto OUT;  // 复制失败跳转到结束
        }
    }

    ret = umount(target ? pathRet : NULL);  // 执行卸载操作
    if (ret < 0) {
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 结束处理
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);  // 释放路径内存
    }
    return ret;  // 返回结果
}

/**
 * @brief 系统调用：检查文件访问权限
 * @param[in] path 文件路径
 * @param[in] amode 访问模式
 * @return 成功返回0，失败返回错误码
 */
int SysAccess(const char *path, int amode)
{
    int ret;  // 返回值
    char *pathRet = NULL;  // 标准化后的路径

    if (path != NULL) {  // 处理路径
        ret = UserPathCopy(path, &pathRet);  // 复制用户路径
        if (ret != 0) {
            goto OUT;  // 复制失败跳转到结束
        }
    }

    ret = access(pathRet, amode);  // 检查访问权限
    if (ret < 0) {
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 结束处理
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);  // 释放路径内存
    }

    return ret;  // 返回结果
}

/**
 * @brief 系统调用：重命名文件或目录
 * @param[in] oldpath 原路径
 * @param[in] newpath 新路径
 * @return 成功返回0，失败返回错误码
 */
int SysRename(const char *oldpath, const char *newpath)
{
    int ret;  // 返回值
    char *pathOldRet = NULL;  // 原路径标准化结果
    char *pathNewRet = NULL;  // 新路径标准化结果

    if (oldpath != NULL) {  // 处理原路径
        ret = UserPathCopy(oldpath, &pathOldRet);  // 复制用户路径
        if (ret != 0) {
            goto OUT;  // 复制失败跳转到结束
        }
    }

    if (newpath != NULL) {  // 处理新路径
        ret = UserPathCopy(newpath, &pathNewRet);  // 复制用户路径
        if (ret != 0) {
            goto OUT;  // 复制失败跳转到结束
        }
    }

    // 执行重命名操作
    ret = do_rename(AT_FDCWD, (oldpath ? pathOldRet : NULL), AT_FDCWD,
                    (newpath ? pathNewRet : NULL));
    if (ret < 0) {
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 结束处理
    if (pathOldRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathOldRet);  // 释放原路径内存
    }
    if (pathNewRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathNewRet);  // 释放新路径内存
    }
    return ret;  // 返回结果
}

/**
 * @brief 系统调用：创建目录
 * @param[in] pathname 目录路径
 * @param[in] mode 目录权限
 * @return 成功返回0，失败返回错误码
 */
int SysMkdir(const char *pathname, mode_t mode)
{
    int ret;  // 返回值
    char *pathRet = NULL;  // 标准化后的路径

    if (pathname != NULL) {  // 处理路径
        ret = UserPathCopy(pathname, &pathRet);  // 复制用户路径
        if (ret != 0) {
            goto OUT;  // 复制失败跳转到结束
        }
    }

    ret = do_mkdir(AT_FDCWD, (pathname ? pathRet : NULL), mode);  // 创建目录
    if (ret < 0) {
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 结束处理
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);  // 释放路径内存
    }
    return ret;  // 返回结果
}

/**
 * @brief 系统调用：删除目录
 * @param[in] pathname 目录路径
 * @return 成功返回0，失败返回错误码
 */
int SysRmdir(const char *pathname)
{
    int ret;  // 返回值
    char *pathRet = NULL;  // 标准化后的路径

    if (pathname != NULL) {  // 处理路径
        ret = UserPathCopy(pathname, &pathRet);  // 复制用户路径
        if (ret != 0) {
            goto OUT;  // 复制失败跳转到结束
        }
    }

    ret = do_rmdir(AT_FDCWD, (pathname ? pathRet : NULL));  // 删除目录
    if (ret < 0) {
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 结束处理
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);  // 释放路径内存
    }
    return ret;  // 返回结果
}

/**
 * @brief 系统调用：复制文件描述符
 * @param[in] fd 要复制的文件描述符
 * @return 成功返回新的文件描述符，失败返回错误码
 */
int SysDup(int fd)
{
    int sysfd = GetAssociatedSystemFd(fd);  // 获取系统fd
    /* 检查参数是否有效，注意：不支持socket fd的dup2操作 */
    if ((sysfd < 0) || (sysfd >= CONFIG_NFILE_DESCRIPTORS)) {
        return -EBADF;  // 无效fd返回错误
    }

    int dupfd = AllocProcessFd();  // 分配新的进程fd
    if (dupfd < 0) {
        return -EMFILE;  // 进程fd用尽返回错误
    }

    files_refer(sysfd);  // 增加文件引用计数
    AssociateSystemFd(dupfd, sysfd);  // 关联新的进程fd和系统fd
    return dupfd;  // 返回新的进程fd
}

/**
 * @brief 系统调用：同步文件系统缓存
 */
void SysSync(void)
{
    sync();  // 调用同步函数
}

/**
 * @brief 系统调用：带标志的卸载文件系统
 * @param[in] target 挂载点路径
 * @param[in] flags 卸载标志
 * @return 成功返回0，失败返回错误码
 */
int SysUmount2(const char *target, int flags)
{
    if (flags != 0) {  // 检查标志是否为0（当前不支持其他标志）
        return -EINVAL;  // 无效标志返回错误
    }
    return SysUmount(target);  // 调用普通umount
}

/**
 * @brief 系统调用：I/O控制
 * @param[in] fd 文件描述符
 * @param[in] req 控制命令
 * @param[in,out] arg 参数
 * @return 成功返回命令结果，失败返回错误码
 */
int SysIoctl(int fd, int req, void *arg)
{
    int ret;  // 返回值
    unsigned int size = _IOC_SIZE((unsigned int)req);  // 获取命令数据大小
    unsigned int dir = _IOC_DIR((unsigned int)req);  // 获取命令方向
    if ((size == 0) && (dir != _IOC_NONE)) {  // 检查大小和方向是否匹配
        return -EINVAL;  // 无效命令返回错误
    }

    if ((dir != _IOC_NONE) && (((void *)(uintptr_t)arg) == NULL)) {  // 参数检查
        return -EINVAL;  // 无效参数返回错误
    }

    // 如果命令需要读写数据
    if ((dir & _IOC_READ) || (dir & _IOC_WRITE)) {
        // 检查参数地址是否在用户空间
        if (!LOS_IsUserAddressRange((uintptr_t)arg, size)) {
            return -EFAULT;  // 地址无效返回错误
        }
    }

    /* 将进程fd转换为系统全局fd */
    fd = GetAssociatedSystemFd(fd);

    ret = ioctl(fd, req, arg);  // 执行ioctl操作
    if (ret < 0) {
        return -get_errno();  // 获取错误码
    }
    return ret;  // 返回结果
}

/**
 * @brief 系统调用：文件描述符控制
 * @param[in] fd 文件描述符
 * @param[in] cmd 控制命令
 * @param[in] arg 参数
 * @return 成功返回命令结果，失败返回错误码
 */
int SysFcntl(int fd, int cmd, void *arg)
{
    /* 将进程fd转换为系统全局fd */
    int sysfd = GetAssociatedSystemFd(fd);

    int ret = VfsFcntl(fd, cmd, arg);  // 执行VFS层fcntl
    if (ret == CONTINE_NUTTX_FCNTL) {  // 需要继续执行NuttX的fcntl
        ret = fcntl(sysfd, cmd, arg);  // 调用系统fcntl
    }

    if (ret < 0) {
        return -get_errno();  // 获取错误码
    }
    return ret;  // 返回结果
}

#ifdef LOSCFG_KERNEL_PIPE
/**
 * @brief 创建管道并返回文件描述符
 * @param pipefd[2] 用于存储管道读写端文件描述符的数组
 * @return 成功返回0，失败返回错误码
 */
int SysPipe(int pipefd[2]) /* 2 : pipe fds for read and write */
{
    int ret;                                 // 函数返回值
    int pipeFdIntr[2] = {0}; /* 2 : pipe fds for read and write */  // 临时存储管道文件描述符

    int procFd0 = AllocProcessFd();          // 分配进程文件描述符（读端）
    if (procFd0 < 0) {                       // 检查分配是否成功
        return -EMFILE;                      // 返回文件描述符过多错误
    }
    int procFd1 = AllocProcessFd();          // 分配进程文件描述符（写端）
    if (procFd1 < 0) {                       // 检查分配是否成功
        FreeProcessFd(procFd0);              // 释放已分配的读端描述符
        return -EMFILE;                      // 返回文件描述符过多错误
    }

    ret = pipe(pipeFdIntr);                  // 创建管道
    if (ret < 0) {                           // 检查管道创建是否成功
        FreeProcessFd(procFd0);              // 释放读端描述符
        FreeProcessFd(procFd1);              // 释放写端描述符
        return -get_errno();                 // 返回错误码
    }
    int sysPipeFd0 = pipeFdIntr[0];          // 系统级管道读端描述符
    int sysPipeFd1 = pipeFdIntr[1];          // 系统级管道写端描述符

    AssociateSystemFd(procFd0, sysPipeFd0);  // 关联进程读端描述符与系统读端描述符
    AssociateSystemFd(procFd1, sysPipeFd1);  // 关联进程写端描述符与系统写端描述符

    pipeFdIntr[0] = procFd0;                 // 将进程读端描述符存入结果数组
    pipeFdIntr[1] = procFd1;                 // 将进程写端描述符存入结果数组

    ret = LOS_ArchCopyToUser(pipefd, pipeFdIntr, sizeof(pipeFdIntr));  // 拷贝结果到用户空间
    if (ret != 0) {                          // 检查拷贝是否成功
        FreeProcessFd(procFd0);              // 释放读端描述符
        FreeProcessFd(procFd1);              // 释放写端描述符
        close(sysPipeFd0);                   // 关闭系统读端描述符
        close(sysPipeFd1);                   // 关闭系统写端描述符
        return -EFAULT;                      // 返回内存访问错误
    }
    return ret;                              // 返回成功
}
#endif

/**
 * @brief 复制文件描述符
 * @param fd1 源文件描述符
 * @param fd2 目标文件描述符
 * @return 成功返回fd2，失败返回错误码
 */
int SysDup2(int fd1, int fd2)
{
    int ret;                                 // 函数返回值
    int sysfd1 = GetAssociatedSystemFd(fd1);  // 获取源文件描述符对应的系统描述符
    int sysfd2 = GetAssociatedSystemFd(fd2);  // 获取目标文件描述符对应的系统描述符

    /* Check if the param is valid, note that: socket fd is not support dup2 */
    if ((sysfd1 < 0) || (sysfd1 >= CONFIG_NFILE_DESCRIPTORS) || (CheckProcessFd(fd2) != OK)) {
        return -EBADF;                       // 参数无效，返回错误的文件描述符
    }

    /* Handle special circumstances */
    if (fd1 == fd2) {                        // 源描述符与目标描述符相同
        return fd2;                          // 直接返回目标描述符
    }

    ret = AllocSpecifiedProcessFd(fd2);      // 分配指定的进程文件描述符
    if (ret != OK) {                         // 检查分配是否成功
        return ret;                          // 返回错误码
    }

    /* close the sysfd2 in need */
    if (sysfd2 >= 0) {                       // 如果目标系统描述符有效
        ret = close(sysfd2);                 // 关闭目标系统描述符
        if (ret < 0) {                       // 检查关闭是否成功
            AssociateSystemFd(fd2, sysfd2);  // 恢复关联关系
            return -get_errno();             // 返回错误码
        }
    }

    files_refer(sysfd1);                     // 增加源系统描述符引用计数
    AssociateSystemFd(fd2, sysfd1);          // 关联目标描述符与源系统描述符

    /* if fd1 is not equal to fd2, the FD_CLOEXEC flag associated with fd2 shall be cleared */
    ClearCloexecFlag(fd2);                   // 清除目标描述符的FD_CLOEXEC标志
    return fd2;                              // 返回目标描述符
}

/**
 * @brief 检查并复制select函数的参数
 * @param readfds 读文件描述符集
 * @param writefds 写文件描述符集
 * @param exceptfds 异常文件描述符集
 * @param fdsBuf 用于存储复制后文件描述符集的缓冲区
 * @return 成功返回0，失败返回错误码
 */
static int SelectParamCheckCopy(fd_set *readfds, fd_set *writefds, fd_set *exceptfds, fd_set **fdsBuf)
{
    fd_set *readfdsRet = NULL;               // 读文件描述符集副本
    fd_set *writefdsRet = NULL;              // 写文件描述符集副本
    fd_set *exceptfdsRet = NULL;             // 异常文件描述符集副本

    *fdsBuf = (fd_set *)LOS_MemAlloc(OS_SYS_MEM_ADDR, sizeof(fd_set) * 3); /* 3: three param need check and copy */
    if (*fdsBuf == NULL) {                   // 检查内存分配是否成功
        return -ENOMEM;                      // 返回内存不足错误
    }

    readfdsRet = *fdsBuf;        /* LOS_MemAlloc 3 sizeof(fd_set) space,first use for readfds */
    writefdsRet = *fdsBuf + 1;   /* 1: LOS_MemAlloc 3 sizeof(fd_set) space,second use for writefds */
    exceptfdsRet = *fdsBuf + 2;  /* 2: LOS_MemAlloc 3 sizeof(fd_set) space,thired use for exceptfds */

    if (readfds != NULL) {                   // 如果读文件描述符集不为空
        if (LOS_ArchCopyFromUser(readfdsRet, readfds, sizeof(fd_set)) != 0) {  // 从用户空间复制
            (void)LOS_MemFree(OS_SYS_MEM_ADDR, *fdsBuf);  // 释放内存
            return -EFAULT;                  // 返回内存访问错误
        }
    }

    if (writefds != NULL) {                  // 如果写文件描述符集不为空
        if (LOS_ArchCopyFromUser(writefdsRet, writefds, sizeof(fd_set)) != 0) {  // 从用户空间复制
            (void)LOS_MemFree(OS_SYS_MEM_ADDR, *fdsBuf);  // 释放内存
            return -EFAULT;                  // 返回内存访问错误
        }
    }

    if (exceptfds != NULL) {                 // 如果异常文件描述符集不为空
        if (LOS_ArchCopyFromUser(exceptfdsRet, exceptfds, sizeof(fd_set)) != 0) {  // 从用户空间复制
            (void)LOS_MemFree(OS_SYS_MEM_ADDR, *fdsBuf);  // 释放内存
            return -EFAULT;                  // 返回内存访问错误
        }
    }

    return 0;                                // 返回成功
}

/**
 * @brief 实现select系统调用
 * @param nfds 监控的最大文件描述符值+1
 * @param readfds 读文件描述符集
 * @param writefds 写文件描述符集
 * @param exceptfds 异常文件描述符集
 * @param timeout 超时时间
 * @return 成功返回就绪文件描述符数量，失败返回错误码
 */
int SysSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
    int ret;                                 // 函数返回值
    fd_set *fdsRet = NULL;                   // 文件描述符集缓冲区
    fd_set *readfdsRet = NULL;               // 读文件描述符集副本
    fd_set *writefdsRet = NULL;              // 写文件描述符集副本
    fd_set *exceptfdsRet = NULL;             // 异常文件描述符集副本
    struct timeval timeoutRet = {0};         // 超时时间副本

    ret = SelectParamCheckCopy(readfds, writefds, exceptfds, &fdsRet);  // 检查并复制参数
    if (ret != 0) {                          // 检查参数处理是否成功
        return ret;                          // 返回错误码
    }

    readfdsRet = fdsRet;        /* LOS_MemAlloc 3 sizeof(fd_set) space,first use for readfds */
    writefdsRet = fdsRet + 1;   /* 1: LOS_MemAlloc 3 sizeof(fd_set) space,second use for writefds */
    exceptfdsRet = fdsRet + 2;  /* 2: LOS_MemAlloc 3 sizeof(fd_set) space,thired use for exceptfds */

    if (timeout != NULL) {                   // 如果超时时间不为空
        if (LOS_ArchCopyFromUser(&timeoutRet, timeout, sizeof(struct timeval)) != 0) {  // 从用户空间复制
            goto ERROUT;                     // 跳转至错误处理
        }
    }

    ret = do_select(nfds, (readfds ? readfdsRet : NULL), (writefds ? writefdsRet : NULL),
                 (exceptfds ? exceptfdsRet : NULL), (timeout ? (&timeoutRet) : NULL), UserPoll);
    if (ret < 0) {                           // 检查select操作是否成功
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, fdsRet);  // 释放内存
        return -get_errno();                 // 返回错误码
    }

    if (readfds != NULL) {                   // 如果读文件描述符集不为空
        if (LOS_ArchCopyToUser(readfds, readfdsRet, sizeof(fd_set)) != 0) {  // 复制结果到用户空间
            goto ERROUT;                     // 跳转至错误处理
        }
    }

    if (writefds != NULL) {                  // 如果写文件描述符集不为空
        if (LOS_ArchCopyToUser(writefds, writefdsRet, sizeof(fd_set)) != 0) {  // 复制结果到用户空间
            goto ERROUT;                     // 跳转至错误处理
        }
    }

    if (exceptfds != 0) {                    // 如果异常文件描述符集不为空
        if (LOS_ArchCopyToUser(exceptfds, exceptfdsRet, sizeof(fd_set)) != 0) {  // 复制结果到用户空间
            goto ERROUT;                     // 跳转至错误处理
        }
    }

    (void)LOS_MemFree(OS_SYS_MEM_ADDR, fdsRet);  // 释放内存
    return ret;                              // 返回就绪文件描述符数量

ERROUT:
    (void)LOS_MemFree(OS_SYS_MEM_ADDR, fdsRet);  // 释放内存
    return -EFAULT;                          // 返回内存访问错误
}

/**
 * @brief 截断文件至指定长度
 * @param path 文件路径
 * @param length 目标长度
 * @return 成功返回0，失败返回错误码
 */
int SysTruncate(const char *path, off_t length)
{
    int ret;                                 // 函数返回值
    int fd = -1;                             // 文件描述符
    char *pathRet = NULL;                    // 解析后的文件路径

    if (path != NULL) {                      // 如果路径不为空
        ret = UserPathCopy(path, &pathRet);  // 复制并解析用户路径
        if (ret != 0) {                      // 检查路径处理是否成功
            goto OUT;                        // 跳转至退出
        }
    }

    fd = open((path ? pathRet : NULL), O_RDWR);  // 以读写方式打开文件
    if (fd < 0) {                           // 检查打开是否成功
        /* The errno value has already been set */
        ret = -get_errno();                 // 获取错误码
        goto OUT;                            // 跳转至退出
    }

    ret = ftruncate(fd, length);             // 截断文件
    close(fd);                               // 关闭文件
    if (ret < 0) {                           // 检查截断是否成功
        ret = -get_errno();                 // 获取错误码
    }

OUT:
    if (pathRet != NULL) {                   // 如果路径副本存在
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);  // 释放内存
    }
    return ret;                              // 返回结果
}

/**
 * @brief 截断文件至指定长度(64位)
 * @param path 文件路径
 * @param length 目标长度
 * @return 成功返回0，失败返回错误码
 */
int SysTruncate64(const char *path, off64_t length)
{
    int ret;                                 // 函数返回值
    int fd = -1;                             // 文件描述符
    char *pathRet = NULL;                    // 解析后的文件路径

    if (path != NULL) {                      // 如果路径不为空
        ret = UserPathCopy(path, &pathRet);  // 复制并解析用户路径
        if (ret != 0) {                      // 检查路径处理是否成功
            goto OUT;                        // 跳转至退出
        }
    }

    fd = open((path ? pathRet : NULL), O_RDWR);  // 以读写方式打开文件
    if (fd < 0) {                           // 检查打开是否成功
        /* The errno value has already been set */
        ret = -get_errno();                 // 获取错误码
        goto OUT;                            // 跳转至退出
    }

    ret = ftruncate64(fd, length);           // 截断文件(64位)
    close(fd);                               // 关闭文件
    if (ret < 0) {                           // 检查截断是否成功
        ret = -get_errno();                 // 获取错误码
    }

OUT:
    if (pathRet != NULL) {                   // 如果路径副本存在
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);  // 释放内存
    }
    return ret;                              // 返回结果
}

/**
 * @brief 通过文件描述符截断文件
 * @param fd 文件描述符
 * @param length 目标长度
 * @return 成功返回0，失败返回错误码
 */
int SysFtruncate(int fd, off_t length)
{
    int ret;                                 // 函数返回值

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);          // 将进程文件描述符转换为系统全局描述符

    ret = ftruncate(fd, length);             // 截断文件
    if (ret < 0) {                           // 检查截断是否成功
        return -get_errno();                 // 返回错误码
    }
    return ret;                              // 返回成功
}

/**
 * @brief 获取文件系统信息
 * @param path 文件路径
 * @param buf 存储文件系统信息的缓冲区
 * @return 成功返回0，失败返回错误码
 */
int SysStatfs(const char *path, struct statfs *buf)
{
    int ret;                                 // 函数返回值
    char *pathRet = NULL;                    // 解析后的文件路径
    struct statfs bufRet = {0};              // 文件系统信息缓冲区

    if (path != NULL) {                      // 如果路径不为空
        ret = UserPathCopy(path, &pathRet);  // 复制并解析用户路径
        if (ret != 0) {                      // 检查路径处理是否成功
            goto OUT;                        // 跳转至退出
        }
    }

    ret = statfs((path ? pathRet : NULL), (buf ? (&bufRet) : NULL));  // 获取文件系统信息
    if (ret < 0) {                           // 检查获取是否成功
        ret = -get_errno();                 // 获取错误码
        goto OUT;                            // 跳转至退出
    }

    ret = LOS_ArchCopyToUser(buf, &bufRet, sizeof(struct statfs));  // 复制结果到用户空间
    if (ret != 0) {                          // 检查复制是否成功
        ret = -EFAULT;                      // 返回内存访问错误
    }

OUT:
    if (pathRet != NULL) {                   // 如果路径副本存在
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);  // 释放内存
    }
    return ret;                              // 返回结果
}

/**
 * @brief 获取文件系统信息(64位)
 * @param path 文件路径
 * @param sz 结构体大小
 * @param buf 存储文件系统信息的缓冲区
 * @return 成功返回0，失败返回错误码
 */
int SysStatfs64(const char *path, size_t sz, struct statfs *buf)
{
    int ret;                                 // 函数返回值
    char *pathRet = NULL;                    // 解析后的文件路径
    struct statfs bufRet = {0};              // 文件系统信息缓冲区

    if (path != NULL) {                      // 如果路径不为空
        ret = UserPathCopy(path, &pathRet);  // 复制并解析用户路径
        if (ret != 0) {                      // 检查路径处理是否成功
            goto OUT;                        // 跳转至退出
        }
    }

    if (sz != sizeof(*buf)) {                // 检查结构体大小是否匹配
        ret = -EINVAL;                      // 返回无效参数错误
        goto OUT;                            // 跳转至退出
    }

    ret = statfs((path ? pathRet : NULL), (buf ? (&bufRet) : NULL));  // 获取文件系统信息
    if (ret < 0) {                           // 检查获取是否成功
        ret = -get_errno();                 // 获取错误码
        goto OUT;                            // 跳转至退出
    }

    ret = LOS_ArchCopyToUser(buf, &bufRet, sizeof(struct statfs));  // 复制结果到用户空间
    if (ret != 0) {                          // 检查复制是否成功
        ret = -EFAULT;                      // 返回内存访问错误
    }

OUT:
    if (pathRet != NULL) {                   // 如果路径副本存在
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);  // 释放内存
    }
    return ret;                              // 返回结果
}

/**
 * @brief 获取文件状态信息
 * @param path 文件路径
 * @param buf 存储文件状态信息的缓冲区
 * @return 成功返回0，失败返回错误码
 */
int SysStat(const char *path, struct kstat *buf)
{
    int ret;                                 // 函数返回值
    char *pathRet = NULL;                    // 解析后的文件路径
    struct stat bufRet = {0};                // 文件状态信息缓冲区

    if (path != NULL) {                      // 如果路径不为空
        ret = UserPathCopy(path, &pathRet);  // 复制并解析用户路径
        if (ret != 0) {                      // 检查路径处理是否成功
            goto OUT;                        // 跳转至退出
        }
    }

    ret = stat((path ? pathRet : NULL), (buf ? (&bufRet) : NULL));  // 获取文件状态信息
    if (ret < 0) {                           // 检查获取是否成功
        ret = -get_errno();                 // 获取错误码
        goto OUT;                            // 跳转至退出
    }

    ret = LOS_ArchCopyToUser(buf, &bufRet, sizeof(struct kstat));  // 复制结果到用户空间
    if (ret != 0) {                          // 检查复制是否成功
        ret = -EFAULT;                      // 返回内存访问错误
    }

OUT:
    if (pathRet != NULL) {                   // 如果路径副本存在
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);  // 释放内存
    }
    return ret;                              // 返回结果
}

/**
 * @brief 获取符号链接文件状态信息
 * @param path 文件路径
 * @param buffer 存储文件状态信息的缓冲区
 * @return 成功返回0，失败返回错误码
 */
int SysLstat(const char *path, struct kstat *buffer)
{
    int ret;                                 // 函数返回值
    char *pathRet = NULL;                    // 解析后的文件路径
    struct stat bufRet = {0};                // 文件状态信息缓冲区

    if (path != NULL) {                      // 如果路径不为空
        ret = UserPathCopy(path, &pathRet);  // 复制并解析用户路径
        if (ret != 0) {                      // 检查路径处理是否成功
            goto OUT;                        // 跳转至退出
        }
    }

    ret = stat((path ? pathRet : NULL), (buffer ? (&bufRet) : NULL));  // 获取文件状态信息
    if (ret < 0) {                           // 检查获取是否成功
        ret = -get_errno();                 // 获取错误码
        goto OUT;                            // 跳转至退出
    }

    ret = LOS_ArchCopyToUser(buffer, &bufRet, sizeof(struct kstat));  // 复制结果到用户空间
    if (ret != 0) {                          // 检查复制是否成功
        ret = -EFAULT;                      // 返回内存访问错误
    }

OUT:
    if (pathRet != NULL) {                   // 如果路径副本存在
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);  // 释放内存
    }
    return ret;                              // 返回结果
}

/**
 * @brief 通过文件描述符获取文件状态信息
 * @param fd 文件描述符
 * @param buf 存储文件状态信息的缓冲区
 * @return 成功返回0，失败返回错误码
 */
int SysFstat(int fd, struct kstat *buf)
{
    int ret;                                 // 函数返回值
    struct stat bufRet = {0};                // 文件状态信息缓冲区
    struct file *filep = NULL;               // 文件结构体指针

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);          // 将进程文件描述符转换为系统全局描述符

    ret = fs_getfilep(fd, &filep);           // 获取文件结构体
    if (ret < 0) {                           // 检查获取是否成功
        return -get_errno();                 // 返回错误码
    }

    if (filep->f_oflags & O_DIRECTORY) {     // 检查是否为目录
        return -EBADF;                       // 返回错误的文件描述符
    }

    ret = stat(filep->f_path, (buf ? (&bufRet) : NULL));  // 获取文件状态信息
    if (ret < 0) {                           // 检查获取是否成功
        return -get_errno();                 // 返回错误码
    }

    ret = LOS_ArchCopyToUser(buf, &bufRet, sizeof(struct kstat));  // 复制结果到用户空间
    if (ret != 0) {                          // 检查复制是否成功
        return -EFAULT;                      // 返回内存访问错误
    }

    return ret;                              // 返回成功
}

/**
 * @brief 获取扩展文件状态信息(未实现)
 * @param fd 文件描述符
 * @param path 文件路径
 * @param flag 标志位
 * @param mask 掩码
 * @param stx 存储扩展文件状态信息的缓冲区
 * @return 始终返回-ENOSYS
 */
int SysStatx(int fd, const char *restrict path, int flag, unsigned mask, struct statx *restrict stx)
{
    return -ENOSYS;                          // 返回系统调用未实现错误
}

/**
 * @brief 将文件缓冲区数据同步到磁盘
 * @param fd 文件描述符
 * @return 成功返回0，失败返回错误码
 */
int SysFsync(int fd)
{
    int ret;                                 // 函数返回值
    struct file *filep = NULL;               // 文件结构体指针

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);          // 将进程文件描述符转换为系统全局描述符

    /* Get the file structure corresponding to the file descriptor. */
    ret = fs_getfilep(fd, &filep);           // 获取文件结构体
    if (ret < 0) {                           // 检查获取是否成功
        /* The errno value has already been set */
        return -get_errno();                 // 返回错误码
    }

    if (filep->f_oflags & O_DIRECTORY) {     // 检查是否为目录
        return -EBADF;                       // 返回错误的文件描述符
    }

    /* Perform the fsync operation */
    ret = file_fsync(filep);                 // 执行文件同步操作
    if (ret < 0) {                           // 检查同步是否成功
        return -get_errno();                 // 返回错误码
    }
    return ret;                              // 返回成功
}

/**
 * @brief 从多个缓冲区读取数据
 * @param fd 文件描述符
 * @param iov 输入输出向量数组
 * @param iovcnt 向量数量
 * @return 成功返回读取字节数，失败返回错误码
 */
ssize_t SysReadv(int fd, const struct iovec *iov, int iovcnt)
{
    int ret;                                 // 函数返回值
    int valid_iovcnt = -1;                   // 有效向量数量
    struct iovec *iovRet = NULL;             // 向量数组副本

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);          // 将进程文件描述符转换为系统全局描述符
    if ((iov == NULL) || (iovcnt < 0) || (iovcnt > IOV_MAX)) {  // 检查参数有效性
        return -EINVAL;                      // 返回无效参数错误
    }

    if (iovcnt == 0) {                       // 如果向量数量为0
        return 0;                            // 返回0
    }

    ret = UserIovCopy(&iovRet, iov, iovcnt, &valid_iovcnt);  // 复制并验证向量数组
    if (ret != 0) {                          // 检查复制是否成功
        return ret;                          // 返回错误码
    }

    if (valid_iovcnt <= 0) {                 // 如果有效向量数量小于等于0
        ret = -EFAULT;                      // 返回内存访问错误
        goto OUT;                            // 跳转至退出
    }

    ret = vfs_readv(fd, iovRet, valid_iovcnt, NULL);  // 从多个缓冲区读取数据
    if (ret < 0) {                           // 检查读取是否成功
        ret = -get_errno();                 // 获取错误码
    }

OUT:
    (void)LOS_MemFree(OS_SYS_MEM_ADDR, iovRet);  // 释放内存
    return ret;                              // 返回读取字节数
}

/**
 * @brief 写入数据到多个缓冲区
 * @param fd 文件描述符
 * @param iov 输入输出向量数组
 * @param iovcnt 向量数量
 * @return 成功返回写入字节数，失败返回错误码
 */
ssize_t SysWritev(int fd, const struct iovec *iov, int iovcnt)
{
    int ret;                                 // 函数返回值
    int valid_iovcnt = -1;                   // 有效向量数量
    struct iovec *iovRet = NULL;             // 向量数组副本

    /* Process fd convert to system global fd */
    int sysfd = GetAssociatedSystemFd(fd);   // 将进程文件描述符转换为系统全局描述符
    if ((iovcnt < 0) || (iovcnt > IOV_MAX)) {  // 检查参数有效性
        return -EINVAL;                      // 返回无效参数错误
    }

    if (iovcnt == 0) {                       // 如果向量数量为0
        return 0;                            // 返回0
    }

    if (iov == NULL) {                       // 如果向量数组为空
        return -EFAULT;                      // 返回内存访问错误
    }

    ret = UserIovCopy(&iovRet, iov, iovcnt, &valid_iovcnt);  // 复制并验证向量数组
    if (ret != 0) {                          // 检查复制是否成功
        return ret;                          // 返回错误码
    }

    if (valid_iovcnt != iovcnt) {            // 如果有效向量数量不匹配
        ret = -EFAULT;                      // 返回内存访问错误
        goto OUT_FREE;                       // 跳转至释放内存
    }

    ret = writev(sysfd, iovRet, valid_iovcnt);  // 写入数据到多个缓冲区
    if (ret < 0) {                           // 检查写入是否成功
        ret = -get_errno();                 // 获取错误码
    }

OUT_FREE:
    (void)LOS_MemFree(OS_SYS_MEM_ADDR, iovRet);  // 释放内存
    return ret;                              // 返回写入字节数
}

/**
 * @brief 实现poll系统调用
 * @param fds 文件描述符事件数组
 * @param nfds 文件描述符数量
 * @param timeout 超时时间
 * @return 成功返回就绪文件描述符数量，失败返回错误码
 */
int SysPoll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    int ret;                                 // 函数返回值
    struct pollfd *kfds = NULL;              // 文件描述符事件数组副本

    if ((nfds >= MAX_POLL_NFDS) || (nfds == 0) || (fds == NULL)) {  // 检查参数有效性
        return -EINVAL;                      // 返回无效参数错误
    }

    kfds = (struct pollfd *)malloc(sizeof(struct pollfd) * nfds);  // 分配内存
    if (kfds != NULL) {                      // 如果分配成功
        if (LOS_ArchCopyFromUser(kfds, fds, sizeof(struct pollfd) * nfds) != 0) {  // 从用户空间复制
            ret = -EFAULT;                  // 返回内存访问错误
            goto OUT_KFD;                    // 跳转至释放内存
        }
    }

    int *pollFds = NULL;                     // 系统文件描述符数组
    ret = PollfdToSystem(kfds, nfds, &pollFds);  // 转换为系统文件描述符
    if (ret < 0) {                           // 检查转换是否成功
        ret = -get_errno();                 // 获取错误码
        goto OUT_KFD;                        // 跳转至释放内存
    }

    ret = poll(kfds, nfds, timeout);         // 执行poll操作
    if (ret < 0) {                           // 检查poll是否成功
        ret = -get_errno();                 // 获取错误码
        goto OUT;                            // 跳转至释放内存
    }

    if (kfds != NULL) {                      // 如果副本存在
        RestorePollfd(kfds, nfds, pollFds);  // 恢复文件描述符
        if (LOS_ArchCopyToUser(fds, kfds, sizeof(struct pollfd) * nfds) != 0) {  // 复制结果到用户空间
            ret = -EFAULT;                  // 返回内存访问错误
            goto OUT;                        // 跳转至释放内存
        }
    }

OUT:
    free(pollFds);                           // 释放系统文件描述符数组
OUT_KFD:
    free(kfds);                              // 释放事件数组副本
    return ret;                              // 返回就绪文件描述符数量
}

/**
 * @brief 进程控制
 * @param option 操作选项
 * @param ... 可变参数
 * @return 成功返回0，失败返回错误码
 */
int SysPrctl(int option, ...)
{
    unsigned long name;                      // 进程名
    va_list ap;                              // 可变参数列表
    errno_t err;                             // 错误码

    va_start(ap, option);                    // 初始化可变参数
    if (option != PR_SET_NAME) {             // 仅支持PR_SET_NAME选项
        PRINT_ERR("%s: %d, no support option : 0x%x\n", __FUNCTION__, __LINE__, option);
        err = EOPNOTSUPP;                   // 操作不支持
        goto ERROR;                          // 跳转至错误处理
    }

    name = va_arg(ap, unsigned long);        // 获取进程名参数
    if (!LOS_IsUserAddress(name)) {          // 检查地址是否为用户空间
        err = EFAULT;                        // 内存访问错误
        goto ERROR;                          // 跳转至错误处理
    }

    err = OsSetTaskName(OsCurrTaskGet(), (const char *)(uintptr_t)name, TRUE);  // 设置任务名
    if (err != LOS_OK) {                     // 检查设置是否成功
        goto ERROR;                          // 跳转至错误处理
    }

    va_end(ap);                              // 结束可变参数处理
    return ENOERR;                           // 返回成功

ERROR:
    va_end(ap);                              // 结束可变参数处理
    return -err;                             // 返回错误码
}

/**
 * @brief 从指定偏移量读取数据(64位)
 * @param fd 文件描述符
 * @param buf 数据缓冲区
 * @param nbytes 读取字节数
 * @param offset 偏移量
 * @return 成功返回读取字节数，失败返回错误码
 */
ssize_t SysPread64(int fd, void *buf, size_t nbytes, off64_t offset)
{
    int ret, retVal;                         // 函数返回值
    char *bufRet = NULL;                     // 内核缓冲区

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);          // 将进程文件描述符转换为系统全局描述符

    if (nbytes == 0) {                       // 如果读取字节数为0
        ret = pread64(fd, buf, nbytes, offset);  // 执行pread64
        if (ret < 0) {                       // 检查读取是否成功
            return -get_errno();             // 返回错误码
        } else {
            return ret;                      // 返回0
        }
    }

    bufRet = (char *)LOS_MemAlloc(OS_SYS_MEM_ADDR, nbytes);  // 分配内核缓冲区
    if (bufRet == NULL) {                    // 检查分配是否成功
        return -ENOMEM;                      // 返回内存不足错误
    }
    (void)memset_s(bufRet, nbytes, 0, nbytes);  // 初始化缓冲区
    ret = pread64(fd, (buf ? bufRet : NULL), nbytes, offset);  // 从指定偏移量读取数据
    if (ret < 0) {                           // 检查读取是否成功
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, bufRet);  // 释放缓冲区
        return -get_errno();                 // 返回错误码
    }

    retVal = LOS_ArchCopyToUser(buf, bufRet, ret);  // 复制数据到用户空间
    if (retVal != 0) {                       // 检查复制是否成功
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, bufRet);  // 释放缓冲区
        return -EFAULT;                      // 返回内存访问错误
    }

    (void)LOS_MemFree(OS_SYS_MEM_ADDR, bufRet);  // 释放缓冲区
    return ret;                              // 返回读取字节数
}

/**
 * @brief 写入数据到指定偏移量(64位)
 * @param fd 文件描述符
 * @param buf 数据缓冲区
 * @param nbytes 写入字节数
 * @param offset 偏移量
 * @return 成功返回写入字节数，失败返回错误码
 */
ssize_t SysPwrite64(int fd, const void *buf, size_t nbytes, off64_t offset)
{
    int ret;                                 // 函数返回值
    char *bufRet = NULL;                     // 内核缓冲区

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);          // 将进程文件描述符转换为系统全局描述符

    if (nbytes == 0) {                       // 如果写入字节数为0
        ret = pwrite64(fd, buf, nbytes, offset);  // 执行pwrite64
        if (ret < 0) {                       // 检查写入是否成功
            return -get_errno();             // 返回错误码
        }
        return ret;                          // 返回0
    }

    bufRet = (char *)LOS_MemAlloc(OS_SYS_MEM_ADDR, nbytes);  // 分配内核缓冲区
    if (bufRet == NULL) {                    // 检查分配是否成功
        return -ENOMEM;                      // 返回内存不足错误
    }

    if (buf != NULL) {                       // 如果用户缓冲区不为空
        ret = LOS_ArchCopyFromUser(bufRet, buf, nbytes);  // 复制数据到内核缓冲区
        if (ret != 0) {                      // 检查复制是否成功
            (void)LOS_MemFree(OS_SYS_MEM_ADDR, bufRet);  // 释放缓冲区
            return -EFAULT;                  // 返回内存访问错误
        }
    }

    ret = pwrite64(fd, (buf ? bufRet : NULL), nbytes, offset);  // 写入数据到指定偏移量
    if (ret < 0) {                           // 检查写入是否成功
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, bufRet);  // 释放缓冲区
        return -get_errno();                 // 返回错误码
    }

    (void)LOS_MemFree(OS_SYS_MEM_ADDR, bufRet);  // 释放缓冲区
    return ret;                              // 返回写入字节数
}
/**
 * @brief 获取当前工作目录
 * @details 将当前工作目录路径复制到用户提供的缓冲区中，若缓冲区大小超过PATH_MAX则截断
 * @param buf [out] 存储工作目录路径的缓冲区
 * @param n [in] 缓冲区大小
 * @return 成功时返回指向buf的指针，失败时返回错误码（负值）
 */
char *SysGetcwd(char *buf, size_t n)
{
    char *ret = NULL;  // 函数返回值
    char *bufRet = NULL;  // 内核空间临时缓冲区
    size_t bufLen = n;  // 实际使用的缓冲区长度
    int retVal;  // 系统调用返回值

    if (bufLen > PATH_MAX) {  // 限制缓冲区大小不超过系统最大路径长度
        bufLen = PATH_MAX;
    }

    bufRet = (char *)LOS_MemAlloc(OS_SYS_MEM_ADDR, bufLen);  // 分配内核空间缓冲区
    if (bufRet == NULL) {  // 内存分配失败检查
        return (char *)(intptr_t)-ENOMEM;  // 返回内存不足错误
    }
    (void)memset_s(bufRet, bufLen, 0, bufLen);  // 初始化缓冲区为0

    ret = getcwd((buf ? bufRet : NULL), bufLen);  // 获取当前工作目录
    if (ret == NULL) {  // 获取失败处理
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, bufRet);  // 释放内核缓冲区
        return (char *)(intptr_t)-get_errno();  // 返回错误码
    }

    retVal = LOS_ArchCopyToUser(buf, bufRet, bufLen);  // 将结果复制到用户空间
    if (retVal != 0) {  // 复制失败处理
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, bufRet);  // 释放内核缓冲区
        return (char *)(intptr_t)-EFAULT;  // 返回用户空间访问错误
    }
    ret = buf;  // 设置成功返回值

    (void)LOS_MemFree(OS_SYS_MEM_ADDR, bufRet);  // 释放内核缓冲区
    return ret;  // 返回用户空间缓冲区指针
}

/**
 * @brief 在文件描述符之间传输数据
 * @details 将数据从输入文件描述符传输到输出文件描述符，支持指定偏移量
 * @param outfd [in] 输出文件描述符
 * @param infd [in] 输入文件描述符
 * @param offset [in/out] 输入文件的偏移量指针，为NULL则使用当前偏移
 * @param count [in] 要传输的字节数
 * @return 成功时返回传输的字节数，失败时返回错误码（负值）
 */
ssize_t SysSendFile(int outfd, int infd, off_t *offset, size_t count)
{
    int ret, retVal;  // ret: sendfile返回值, retVal: 内存复制返回值
    off_t offsetRet;  // 内核空间偏移量变量

    retVal = LOS_ArchCopyFromUser(&offsetRet, offset, sizeof(off_t));  // 从用户空间复制偏移量
    if (retVal != 0) {  // 复制失败检查
        return -EFAULT;  // 返回用户空间访问错误
    }

    /* Process fd convert to system global fd */
    outfd = GetAssociatedSystemFd(outfd);  // 将进程文件描述符转换为系统全局文件描述符
    infd = GetAssociatedSystemFd(infd);  // 将进程文件描述符转换为系统全局文件描述符

    ret = sendfile(outfd, infd, (offset ? (&offsetRet) : NULL), count);  // 执行文件数据传输
    if (ret < 0) {  // 传输失败处理
        return -get_errno();  // 返回错误码
    }

    retVal = LOS_ArchCopyToUser(offset, &offsetRet, sizeof(off_t));  // 将更新后的偏移量复制回用户空间
    if (retVal != 0) {  // 复制失败处理
        return -EFAULT;  // 返回用户空间访问错误
    }

    return ret;  // 返回传输的字节数
}

/**
 * @brief 截断文件大小（64位版本）
 * @details 将指定文件截断或扩展到指定长度
 * @param fd [in] 文件描述符
 * @param length [in] 目标文件长度
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysFtruncate64(int fd, off64_t length)
{
    int ret;  // ftruncate64返回值

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);  // 将进程文件描述符转换为系统全局文件描述符

    ret = ftruncate64(fd, length);  // 执行文件截断操作
    if (ret < 0) {  // 操作失败处理
        return -get_errno();  // 返回错误码
    }
    return ret;  // 返回成功
}

/**
 * @brief 打开文件（带目录文件描述符版本）
 * @details 通过目录文件描述符和路径名打开文件，支持多种打开标志和文件权限
 * @param dirfd [in] 目录文件描述符，AT_FDCWD表示当前工作目录
 * @param path [in] 文件路径名
 * @param oflags [in] 打开标志（如O_RDONLY, O_WRONLY, O_CREAT等）
 * @param ... [in] 可变参数，当创建文件时指定文件权限
 * @return 成功时返回进程文件描述符，失败时返回错误码（负值）
 */
int SysOpenat(int dirfd, const char *path, int oflags, ...)
{
    int ret;  // 系统调用返回值
    int procFd;  // 进程文件描述符
    char *pathRet = NULL;  // 内核空间路径缓冲区
    mode_t mode;  // 文件权限
#ifdef LOSCFG_FILE_MODE
    va_list ap;  // 可变参数列表

    va_start(ap, oflags);  // 初始化可变参数
    mode = va_arg(ap, int);  // 获取文件权限参数
    va_end(ap);  // 结束可变参数处理
#else
    mode = 0666; /* 0666: File read-write properties. */
#endif

    if (path != NULL) {  // 路径不为空时复制路径
        ret = UserPathCopy(path, &pathRet);  // 将用户空间路径复制到内核空间
        if (ret != 0) {  // 复制失败处理
            return ret;  // 返回错误码
        }
    }

    procFd = AllocProcessFd();  // 分配进程文件描述符
    if (procFd < 0) {  // 分配失败处理
        ret = -EMFILE;  // 设置文件描述符用尽错误
        goto ERROUT;  // 跳转到错误处理
    }

    if (oflags & O_CLOEXEC) {  // 检查是否设置了执行时关闭标志
        SetCloexecFlag(procFd);  // 设置文件描述符的CLOEXEC标志
    }

    if (dirfd != AT_FDCWD) {  // 目录文件描述符不是当前工作目录
        /* Process fd convert to system global fd */
        dirfd = GetAssociatedSystemFd(dirfd);  // 转换为系统全局文件描述符
    }

    ret = do_open(dirfd, (path ? pathRet : NULL), oflags, mode);  // 执行打开文件操作
    if (ret < 0) {  // 打开失败处理
        ret = -get_errno();  // 获取错误码
        goto ERROUT;  // 跳转到错误处理
    }

    AssociateSystemFd(procFd, ret);  // 关联进程文件描述符和系统文件描述符
    if (pathRet != NULL) {  // 释放内核空间路径缓冲区
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return procFd;  // 返回进程文件描述符

ERROUT:  // 错误处理标签
    if (pathRet != NULL) {  // 释放内核空间路径缓冲区
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    if (procFd >= 0) {  // 释放已分配的进程文件描述符
        FreeProcessFd(procFd);
    }
    return ret;  // 返回错误码
}

/**
 * @brief 创建目录（带目录文件描述符版本）
 * @details 通过目录文件描述符和路径名创建目录
 * @param dirfd [in] 目录文件描述符，AT_FDCWD表示当前工作目录
 * @param pathname [in] 要创建的目录路径名
 * @param mode [in] 目录权限
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysMkdirat(int dirfd, const char *pathname, mode_t mode)
{
    int ret;  // 系统调用返回值
    char *pathRet = NULL;  // 内核空间路径缓冲区

    if (pathname != NULL) {  // 路径不为空时复制路径
        ret = UserPathCopy(pathname, &pathRet);  // 将用户空间路径复制到内核空间
        if (ret != 0) {  // 复制失败处理
            goto OUT;  // 跳转到退出处理
        }
    }

    if (dirfd != AT_FDCWD) {  // 目录文件描述符不是当前工作目录
        /* Process fd convert to system global fd */
        dirfd = GetAssociatedSystemFd(dirfd);  // 转换为系统全局文件描述符
    }

    ret = do_mkdir(dirfd, (pathname ? pathRet : NULL), mode);  // 执行创建目录操作
    if (ret < 0) {  // 创建失败处理
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 退出处理标签
    if (pathRet != NULL) {  // 释放内核空间路径缓冲区
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;  // 返回结果
}

/**
 * @brief 创建硬链接（带目录文件描述符版本）
 * @details 通过目录文件描述符和路径名创建硬链接
 * @param olddirfd [in] 旧路径目录文件描述符，AT_FDCWD表示当前工作目录
 * @param oldpath [in] 源文件路径名
 * @param newdirfd [in] 新路径目录文件描述符，AT_FDCWD表示当前工作目录
 * @param newpath [in] 链接文件路径名
 * @param flags [in] 链接标志（目前未使用）
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysLinkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags)
{
    int ret;  // 系统调用返回值
    char *oldpathRet = NULL;  // 内核空间旧路径缓冲区
    char *newpathRet = NULL;  // 内核空间新路径缓冲区

    if (oldpath != NULL) {  // 旧路径不为空时复制路径
        ret = UserPathCopy(oldpath, &oldpathRet);  // 将用户空间旧路径复制到内核空间
        if (ret != 0) {  // 复制失败处理
            goto OUT;  // 跳转到退出处理
        }
    }

    if (newpath != NULL) {  // 新路径不为空时复制路径
        ret = UserPathCopy(newpath, &newpathRet);  // 将用户空间新路径复制到内核空间
        if (ret != 0) {  // 复制失败处理
            goto OUT;  // 跳转到退出处理
        }
    }

    if (olddirfd != AT_FDCWD) {  // 旧路径目录文件描述符不是当前工作目录
        /* Process fd convert to system global fd */
        olddirfd = GetAssociatedSystemFd(olddirfd);  // 转换为系统全局文件描述符
    }

    if (newdirfd != AT_FDCWD) {  // 新路径目录文件描述符不是当前工作目录
        /* Process fd convert to system global fd */
        newdirfd = GetAssociatedSystemFd(newdirfd);  // 转换为系统全局文件描述符
    }

    ret = linkat(olddirfd, oldpathRet, newdirfd, newpathRet, flags);  // 执行创建硬链接操作
    if (ret < 0) {  // 创建失败处理
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 退出处理标签
    if (oldpathRet != NULL) {  // 释放内核空间旧路径缓冲区
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, oldpathRet);
    }
    if (newpathRet != NULL) {  // 释放内核空间新路径缓冲区
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, newpathRet);
    }
    return ret;  // 返回结果
}

/**
 * @brief 创建符号链接（带目录文件描述符版本）
 * @details 通过目录文件描述符和路径名创建符号链接
 * @param target [in] 目标文件路径名
 * @param dirfd [in] 目录文件描述符，AT_FDCWD表示当前工作目录
 * @param linkpath [in] 符号链接文件路径名
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysSymlinkat(const char *target, int dirfd, const char *linkpath)
{
    int ret;  // 系统调用返回值
    char *pathRet = NULL;  // 内核空间链接路径缓冲区
    char *targetRet = NULL;  // 内核空间目标路径缓冲区

    if (target != NULL) {  // 目标路径不为空时复制路径
        ret = UserPathCopy(target, &targetRet);  // 将用户空间目标路径复制到内核空间
        if (ret != 0) {  // 复制失败处理
            goto OUT;  // 跳转到退出处理
        }
    }

    if (linkpath != NULL) {  // 链接路径不为空时复制路径
        ret = UserPathCopy(linkpath, &pathRet);  // 将用户空间链接路径复制到内核空间
        if (ret != 0) {  // 复制失败处理
            goto OUT;  // 跳转到退出处理
        }
    }

    if (dirfd != AT_FDCWD) {  // 目录文件描述符不是当前工作目录
        /* Process fd convert to system global fd */
        dirfd = GetAssociatedSystemFd(dirfd);  // 转换为系统全局文件描述符
    }

    ret = symlinkat(targetRet, dirfd, pathRet);  // 执行创建符号链接操作
    if (ret < 0) {  // 创建失败处理
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 退出处理标签
    if (pathRet != NULL) {  // 释放内核空间链接路径缓冲区
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }

    if (targetRet != NULL) {  // 释放内核空间目标路径缓冲区
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, targetRet);
    }
    return ret;  // 返回结果
}

/**
 * @brief 读取符号链接内容（带目录文件描述符版本）
 * @details 通过目录文件描述符和路径名读取符号链接指向的目标路径
 * @param dirfd [in] 目录文件描述符，AT_FDCWD表示当前工作目录
 * @param pathname [in] 符号链接文件路径名
 * @param buf [out] 存储目标路径的缓冲区
 * @param bufsize [in] 缓冲区大小
 * @return 成功时返回读取的字节数，失败时返回错误码（负值）
 */
ssize_t SysReadlinkat(int dirfd, const char *pathname, char *buf, size_t bufsize)
{
    ssize_t ret;  // 系统调用返回值
    char *pathRet = NULL;  // 内核空间路径缓冲区

    if (bufsize == 0) {  // 缓冲区大小为0检查
        return -EINVAL;  // 返回无效参数错误
    }

    if (pathname != NULL) {  // 路径不为空时复制路径
        ret = UserPathCopy(pathname, &pathRet);  // 将用户空间路径复制到内核空间
        if (ret != 0) {  // 复制失败处理
            goto OUT;  // 跳转到退出处理
        }

#ifdef LOSCFG_PID_CONTAINER
#ifdef LOSCFG_PROC_PROCESS_DIR
        ret = ProcRealProcessDirGet(pathRet);  // 获取真实进程目录
        if (ret != 0) {  // 获取失败处理
            goto OUT;  // 跳转到退出处理
        }
#endif
#endif
    }

    if (dirfd != AT_FDCWD) {  // 目录文件描述符不是当前工作目录
        /* Process fd convert to system global fd */
        dirfd = GetAssociatedSystemFd(dirfd);  // 转换为系统全局文件描述符
    }

    if (!LOS_IsUserAddressRange((vaddr_t)(UINTPTR)buf, bufsize)) {  // 检查用户空间缓冲区地址有效性
        ret = -EFAULT;  // 返回用户空间访问错误
        goto OUT;  // 跳转到退出处理
    }

    ret = readlinkat(dirfd, pathRet, buf, bufsize);  // 执行读取符号链接操作
    if (ret < 0) {  // 读取失败处理
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 退出处理标签
    if (pathRet != NULL) {  // 释放内核空间路径缓冲区
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;  // 返回结果
}

/**
 * @brief 删除文件或目录（带目录文件描述符版本）
 * @details 通过目录文件描述符和路径名删除文件或目录
 * @param dirfd [in] 目录文件描述符，AT_FDCWD表示当前工作目录
 * @param pathname [in] 要删除的文件或目录路径名
 * @param flag [in] 删除标志（AT_REMOVEDIR表示删除目录）
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysUnlinkat(int dirfd, const char *pathname, int flag)
{
    int ret;  // 系统调用返回值
    char *pathRet = NULL;  // 内核空间路径缓冲区

    if (pathname != NULL) {  // 路径不为空时复制路径
        ret = UserPathCopy(pathname, &pathRet);  // 将用户空间路径复制到内核空间
        if (ret != 0) {  // 复制失败处理
            goto OUT;  // 跳转到退出处理
        }
    }

    if (dirfd != AT_FDCWD) {  // 目录文件描述符不是当前工作目录
        /* Process fd convert to system global fd */
        dirfd = GetAssociatedSystemFd(dirfd);  // 转换为系统全局文件描述符
    }

    ret = unlinkat(dirfd, (pathname ? pathRet : NULL), flag);  // 执行删除操作
    if (ret < 0) {  // 删除失败处理
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 退出处理标签
    if (pathRet != NULL) {  // 释放内核空间路径缓冲区
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;  // 返回结果
}

/**
 * @brief 重命名文件或目录（带目录文件描述符版本）
 * @details 通过目录文件描述符和路径名重命名文件或目录
 * @param oldfd [in] 旧路径目录文件描述符，AT_FDCWD表示当前工作目录
 * @param oldpath [in] 旧路径名
 * @param newdfd [in] 新路径目录文件描述符，AT_FDCWD表示当前工作目录
 * @param newpath [in] 新路径名
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysRenameat(int oldfd, const char *oldpath, int newdfd, const char *newpath)
{
    int ret;  // 系统调用返回值
    char *pathOldRet = NULL;  // 内核空间旧路径缓冲区
    char *pathNewRet = NULL;  // 内核空间新路径缓冲区

    if (oldpath != NULL) {  // 旧路径不为空时复制路径
        ret = UserPathCopy(oldpath, &pathOldRet);  // 将用户空间旧路径复制到内核空间
        if (ret != 0) {  // 复制失败处理
            goto OUT;  // 跳转到退出处理
        }
    }

    if (newpath != NULL) {  // 新路径不为空时复制路径
        ret = UserPathCopy(newpath, &pathNewRet);  // 将用户空间新路径复制到内核空间
        if (ret != 0) {  // 复制失败处理
            goto OUT;  // 跳转到退出处理
        }
    }

    if (oldfd != AT_FDCWD) {  // 旧路径目录文件描述符不是当前工作目录
        /* Process fd convert to system global fd */
        oldfd = GetAssociatedSystemFd(oldfd);  // 转换为系统全局文件描述符
    }
    if (newdfd != AT_FDCWD) {  // 新路径目录文件描述符不是当前工作目录
        /* Process fd convert to system global fd */
        newdfd = GetAssociatedSystemFd(newdfd);  // 转换为系统全局文件描述符
    }

    ret = do_rename(oldfd, (oldpath ? pathOldRet : NULL), newdfd, (newpath ? pathNewRet : NULL));  // 执行重命名操作
    if (ret < 0) {  // 重命名失败处理
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 退出处理标签
    if (pathOldRet != NULL) {  // 释放内核空间旧路径缓冲区
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathOldRet);
    }
    if (pathNewRet != NULL) {  // 释放内核空间新路径缓冲区
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathNewRet);
    }
    return ret;  // 返回结果
}

/**
 * @brief 预分配文件空间
 * @details 为指定文件预分配或释放磁盘空间
 * @param fd [in] 文件描述符
 * @param mode [in] 分配模式（0表示默认模式，FALLOC_FL_KEEP_SIZE表示保持文件大小）
 * @param offset [in] 起始偏移量
 * @param len [in] 要分配的长度
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysFallocate(int fd, int mode, off_t offset, off_t len)
{
    int ret;  // fallocate返回值

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);  // 将进程文件描述符转换为系统全局文件描述符

    ret = fallocate(fd, mode, offset, len);  // 执行预分配操作
    if (ret < 0) {  // 分配失败处理
        return -get_errno();  // 返回错误码
    }
    return ret;  // 返回成功
}

/**
 * @brief 预分配文件空间（64位版本）
 * @details 为指定文件预分配或释放磁盘空间（支持大文件）
 * @param fd [in] 文件描述符
 * @param mode [in] 分配模式（0表示默认模式，FALLOC_FL_KEEP_SIZE表示保持文件大小）
 * @param offset [in] 起始偏移量（64位）
 * @param len [in] 要分配的长度（64位）
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysFallocate64(int fd, int mode, off64_t offset, off64_t len)
{
    int ret;  // fallocate64返回值

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);  // 将进程文件描述符转换为系统全局文件描述符

    ret = fallocate64(fd, mode, offset, len);  // 执行预分配操作
    if (ret < 0) {  // 分配失败处理
        return -get_errno();  // 返回错误码
    }
    return ret;  // 返回成功
}

/**
 * @brief 从指定偏移量读取数据到分散的缓冲区
 * @details 从文件的指定偏移量读取数据到多个分散的缓冲区（向量I/O）
 * @param fd [in] 文件描述符
 * @param iov [in] iovec结构体数组，描述缓冲区
 * @param iovcnt [in] iovec结构体数量
 * @param loffset [in] 低32位偏移量
 * @param hoffset [in] 高32位偏移量
 * @return 成功时返回读取的字节数，失败时返回错误码（负值）
 */
ssize_t SysPreadv(int fd, const struct iovec *iov, int iovcnt, long loffset, long hoffset)
{
    off_t offsetflag;  // 组合后的64位偏移量
    offsetflag = (off_t)((unsigned long long)loffset | (((unsigned long long)hoffset) << HIGH_SHIFT_BIT));  // 计算64位偏移量

    int ret;  // preadv返回值
    int valid_iovcnt = -1;  // 有效的iovec数量
    struct iovec *iovRet = NULL;  // 内核空间iovec数组

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);  // 将进程文件描述符转换为系统全局文件描述符
    if ((iov == NULL) || (iovcnt < 0) || (iovcnt > IOV_MAX)) {  // 参数合法性检查
        return -EINVAL;  // 返回无效参数错误
    }

    if (iovcnt == 0) {  // 没有缓冲区时直接返回0
        return 0;
    }

    ret = UserIovCopy(&iovRet, iov, iovcnt, &valid_iovcnt);  // 复制用户空间iovec到内核空间
    if (ret != 0) {  // 复制失败处理
        return ret;  // 返回错误码
    }

    if (valid_iovcnt <= 0) {  // 有效缓冲区数量检查
        ret = -EFAULT;  // 返回用户空间访问错误
        goto OUT_FREE;  // 跳转到释放资源
    }

    ret = preadv(fd, iovRet, valid_iovcnt, offsetflag);  // 执行向量读取操作
    if (ret < 0) {  // 读取失败处理
        ret = -get_errno();  // 获取错误码
    }

OUT_FREE:  // 释放资源标签
    (void)(void)LOS_MemFree(OS_SYS_MEM_ADDR, iovRet);  // 释放内核空间iovec数组
    return ret;  // 返回结果
}

/**
 * @brief 从分散的缓冲区写入数据到指定偏移量
 * @details 将多个分散的缓冲区数据写入文件的指定偏移量（向量I/O）
 * @param fd [in] 文件描述符
 * @param iov [in] iovec结构体数组，描述缓冲区
 * @param iovcnt [in] iovec结构体数量
 * @param loffset [in] 低32位偏移量
 * @param hoffset [in] 高32位偏移量
 * @return 成功时返回写入的字节数，失败时返回错误码（负值）
 */
ssize_t SysPwritev(int fd, const struct iovec *iov, int iovcnt, long loffset, long hoffset)
{
    off_t offsetflag;  // 组合后的64位偏移量
    offsetflag = (off_t)((unsigned long long)loffset | (((unsigned long long)hoffset) << HIGH_SHIFT_BIT));  // 计算64位偏移量
    int ret;  // pwritev返回值
    int valid_iovcnt = -1;  // 有效的iovec数量
    struct iovec *iovRet = NULL;  // 内核空间iovec数组

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);  // 将进程文件描述符转换为系统全局文件描述符
    if ((iov == NULL) || (iovcnt < 0) || (iovcnt > IOV_MAX)) {  // 参数合法性检查
        return -EINVAL;  // 返回无效参数错误
    }

    if (iovcnt == 0) {  // 没有缓冲区时直接返回0
        return 0;
    }

    ret = UserIovCopy(&iovRet, iov, iovcnt, &valid_iovcnt);  // 复制用户空间iovec到内核空间
    if (ret != 0) {  // 复制失败处理
        return ret;  // 返回错误码
    }

    if (valid_iovcnt != iovcnt) {  // 检查所有iovec是否有效
        ret = -EFAULT;  // 返回用户空间访问错误
        goto OUT_FREE;  // 跳转到释放资源
    }

    ret = pwritev(fd, iovRet, valid_iovcnt, offsetflag);  // 执行向量写入操作
    if (ret < 0) {  // 写入失败处理
        ret = -get_errno();  // 获取错误码
    }

OUT_FREE:  // 释放资源标签
    (void)LOS_MemFree(OS_SYS_MEM_ADDR, iovRet);  // 释放内核空间iovec数组
    return ret;  // 返回结果
}

#ifdef LOSCFG_FS_FAT
/**
 * @brief 格式化文件系统
 * @details 对指定设备进行文件系统格式化，需要格式化权限
 * @param dev [in] 设备路径名
 * @param sectors [in] 扇区数
 * @param option [in] 格式化选项
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysFormat(const char *dev, int sectors, int option)
{
    int ret;  // 函数返回值
    char *devRet = NULL;  // 内核空间设备路径缓冲区

    if (!IsCapPermit(CAP_FS_FORMAT)) {  // 检查是否有格式化权限
        return -EPERM;  // 返回权限不足错误
    }

    if (dev != NULL) {  // 设备路径不为空时复制路径
        ret = UserPathCopy(dev, &devRet);  // 将用户空间路径复制到内核空间
        if (ret != 0) {  // 复制失败处理
            goto OUT;  // 跳转到退出处理
        }
    }

    ret = format((dev ? devRet : NULL), sectors, option);  // 执行格式化操作
    if (ret < 0) {  // 格式化失败处理
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 退出处理标签
    if (devRet != NULL) {  // 释放内核空间设备路径缓冲区
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, devRet);
    }
    return ret;  // 返回结果
}
#endif

/**
 * @brief 获取文件状态（64位版本）
 * @details 获取指定文件描述符对应的文件状态信息
 * @param fd [in] 文件描述符
 * @param buf [out] 存储文件状态的缓冲区
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysFstat64(int fd, struct kstat *buf)
{
    int ret;  // fstat64返回值
    struct stat64 bufRet = {0};  // 内核空间文件状态缓冲区

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);  // 将进程文件描述符转换为系统全局文件描述符

    ret = fstat64(fd, (buf ? (&bufRet) : NULL));  // 获取文件状态
    if (ret < 0) {  // 获取失败处理
        return -get_errno();  // 返回错误码
    }

    ret = LOS_ArchCopyToUser(buf, &bufRet, sizeof(struct kstat));  // 将结果复制到用户空间
    if (ret != 0) {  // 复制失败处理
        return -EFAULT;  // 返回用户空间访问错误
    }

    return ret;  // 返回成功
}

/**
 * @brief 文件描述符控制操作（64位版本）
 * @details 对文件描述符执行控制操作，如设置文件状态标志等
 * @param fd [in] 文件描述符
 * @param cmd [in] 控制命令
 * @param arg [in/out] 命令参数
 * @return 成功时返回命令结果，失败时返回错误码（负值）
 */
int SysFcntl64(int fd, int cmd, void *arg)
{
    /* Process fd convert to system global fd */
    int sysfd = GetAssociatedSystemFd(fd);  // 将进程文件描述符转换为系统全局文件描述符

    int ret = VfsFcntl(fd, cmd, arg);  // 执行VFS层fcntl操作
    if (ret == CONTINE_NUTTX_FCNTL) {  // 需要继续执行NuttX原生fcntl
        ret = fcntl64(sysfd, cmd, arg);  // 执行NuttX fcntl64
    }

    if (ret < 0) {  // 操作失败处理
        return -get_errno();  // 返回错误码
    }
    return ret;  // 返回命令结果
}

/**
 * @brief 获取目录项（64位版本）
 * @details 读取目录文件的目录项信息到用户空间缓冲区
 * @param fd [in] 目录文件描述符
 * @param de_user [out] 用户空间目录项缓冲区
 * @param count [in] 缓冲区大小
 * @return 成功时返回读取的字节数，失败时返回错误码（负值）
 */
int SysGetdents64(int fd, struct dirent *de_user, unsigned int count)
{
    if (!LOS_IsUserAddressRange((VADDR_T)(UINTPTR)de_user, count)) {  // 检查用户空间缓冲区地址有效性
        return -EFAULT;  // 返回用户空间访问错误
    }

    struct dirent *de_knl = NULL;  // 内核空间目录项缓冲区

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);  // 将进程文件描述符转换为系统全局文件描述符

    int ret = do_readdir(fd, &de_knl, count);  // 读取目录项
    if (ret < 0) {  // 读取失败处理
        return ret;  // 返回错误码
    }
    if (de_knl != NULL) {  // 目录项不为空时复制到用户空间
        int cpy_ret = LOS_ArchCopyToUser(de_user, de_knl, ret);  // 复制目录项数据
        if (cpy_ret != 0)  // 复制失败处理
        {
            return -EFAULT;  // 返回用户空间访问错误
        }
    }
    return ret;  // 返回读取的字节数
}

/**
 * @brief 获取绝对路径
 * @details 将相对路径解析为绝对路径并存储到用户提供的缓冲区
 * @param path [in] 相对路径
 * @param resolved_path [out] 存储绝对路径的缓冲区
 * @return 成功时返回指向resolved_path的指针，失败时返回错误码（负值）
 */
char *SysRealpath(const char *path, char *resolved_path)
{
    char *pathRet = NULL;  // 内核空间路径缓冲区
    char *resolved_pathRet = NULL;  // 内核空间解析后的绝对路径缓冲区
    char *result = NULL;  // 函数返回值
    int ret;  // 系统调用返回值

    if (resolved_path == NULL) {  // 检查输出缓冲区是否为空
        return (char *)(intptr_t)-EINVAL;  // 返回无效参数错误
    }

    if (path != NULL) {  // 路径不为空时复制路径
        ret = UserPathCopy(path, &pathRet);  // 将用户空间路径复制到内核空间
        if (ret != 0) {  // 复制失败处理
            result = (char *)(intptr_t)ret;  // 设置错误码
            goto OUT;  // 跳转到退出处理
        }
    }

    resolved_pathRet = realpath((path ? pathRet : NULL), NULL);  // 解析绝对路径
    if (resolved_pathRet == NULL) {  // 解析失败处理
        result = (char *)(intptr_t)-get_errno();  // 获取错误码
        goto OUT;  // 跳转到退出处理
    }

    ret = LOS_ArchCopyToUser(resolved_path, resolved_pathRet, strlen(resolved_pathRet) + 1);  // 复制结果到用户空间
    if (ret != 0) {  // 复制失败处理
        result = (char *)(intptr_t)-EFAULT;  // 返回用户空间访问错误
        goto OUT;  // 跳转到退出处理
    }
    result = resolved_path;  // 设置成功返回值

OUT:  // 退出处理标签
    if (pathRet != NULL) {  // 释放内核空间路径缓冲区
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    if (resolved_pathRet != NULL) {  // 释放内核空间解析路径缓冲区
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, resolved_pathRet);
    }
    return result;  // 返回结果
}

/**
 * @brief 设置文件访问和修改时间（带目录文件描述符版本）
 * @details 设置指定文件的访问时间和修改时间
 * @param fd [in] 目录文件描述符，AT_FDCWD表示当前工作目录
 * @param path [in] 文件路径名
 * @param times [in] 时间数组，包含访问时间和修改时间
 * @param flag [in] 标志位（AT_SYMLINK_NOFOLLOW表示不跟随符号链接）
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysUtimensat(int fd, const char *path, struct timespec times[TIMESPEC_TIMES_NUM], int flag)
{
    int ret;  // 系统调用返回值
    int timeLen;  // 时间数组长度
    struct IATTR attr = {0};  // 文件属性结构体
    char *filePath = NULL;  // 完整文件路径

    timeLen = TIMESPEC_TIMES_NUM * sizeof(struct timespec);  // 计算时间数组大小
    CHECK_ASPACE(times, timeLen);  // 检查用户空间时间数组地址有效性
    DUP_FROM_USER(times, timeLen);  // 复制用户空间时间数组到内核空间
    ret = CheckNewAttrTime(&attr, times);  // 检查并设置文件时间属性
    FREE_DUP(times);  // 释放内核空间时间数组
    if (ret < 0) {  // 时间属性检查失败
        goto OUT;  // 跳转到退出处理
    }

    ret = GetFullpathNull(fd, path, &filePath);  // 获取完整文件路径
    if (ret < 0) {  // 获取路径失败处理
        goto OUT;  // 跳转到退出处理
    }

    ret = chattr(filePath, &attr);  // 设置文件属性
    if (ret < 0) {  // 设置失败处理
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 退出处理标签
    PointerFree(filePath);  // 释放完整文件路径
    return ret;  // 返回结果
}

/**
 * @brief 修改文件权限
 * @details 修改指定路径文件的访问权限
 * @param pathname [in] 文件路径名
 * @param mode [in] 新的权限模式
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysChmod(const char *pathname, mode_t mode)
{
    int ret;  // 系统调用返回值
    char *pathRet = NULL;  // 内核空间路径缓冲区

    if (pathname != NULL) {  // 路径不为空时复制路径
        ret = UserPathCopy(pathname, &pathRet);  // 将用户空间路径复制到内核空间
        if (ret != 0) {  // 复制失败处理
            goto OUT;  // 跳转到退出处理
        }
    }

    ret = chmod(pathRet, mode);  // 修改文件权限
    if (ret < 0) {  // 修改失败处理
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 退出处理标签
    if (pathRet != NULL) {  // 释放内核空间路径缓冲区
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;  // 返回结果
}

/**
 * @brief 修改文件权限（带目录文件描述符版本）
 * @details 通过目录文件描述符和路径名修改文件权限
 * @param fd [in] 目录文件描述符，AT_FDCWD表示当前工作目录
 * @param path [in] 文件路径名
 * @param mode [in] 新的权限模式
 * @param flag [in] 标志位（未使用）
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysFchmodat(int fd, const char *path, mode_t mode, int flag)
{
    int ret;  // 系统调用返回值
    char *pathRet = NULL;  // 内核空间路径缓冲区
    char *fullpath = NULL;  // 完整文件路径
    struct IATTR attr = {  // 文件属性结构体
        .attr_chg_mode = mode,  // 新的权限模式
        .attr_chg_valid = CHG_MODE,  // 标识修改权限
    };

    if (path != NULL) {  // 路径不为空时复制路径
        ret = UserPathCopy(path, &pathRet);  // 将用户空间路径复制到内核空间
        if (ret != 0) {  // 复制失败处理
            goto OUT;  // 跳转到退出处理
        }
    }

    if (fd != AT_FDCWD) {  // 目录文件描述符不是当前工作目录
        /* Process fd convert to system global fd */
        fd = GetAssociatedSystemFd(fd);  // 转换为系统全局文件描述符
    }

    ret = vfs_normalize_pathat(fd, pathRet, &fullpath);  // 获取规范化的完整路径
    if (ret < 0) {  // 获取路径失败处理
        goto OUT;  // 跳转到退出处理
    }

    ret = chattr(fullpath, &attr);  // 设置文件属性（修改权限）
    if (ret < 0) {  // 设置失败处理
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 退出处理标签
    PointerFree(pathRet);  // 释放内核空间路径缓冲区
    PointerFree(fullpath);  // 释放完整路径

    return ret;  // 返回结果
}

/**
 * @brief 修改文件权限（通过文件描述符）
 * @details 通过文件描述符修改文件的访问权限
 * @param fd [in] 文件描述符
 * @param mode [in] 新的权限模式
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysFchmod(int fd, mode_t mode)
{
    int ret;  // 系统调用返回值
    int sysFd;  // 系统全局文件描述符
    struct IATTR attr = {  // 文件属性结构体
        .attr_chg_mode = mode,  // 新的权限模式
        .attr_chg_valid = CHG_MODE, /* change mode */  // 标识修改权限
    };
    struct file *file = NULL;  // 文件结构体指针

    sysFd = GetAssociatedSystemFd(fd);  // 将进程文件描述符转换为系统全局文件描述符
    if (sysFd < 0) {  // 无效文件描述符检查
        return -EBADF;  // 返回错误的文件描述符错误
    }

    ret = fs_getfilep(sysFd, &file);  // 获取文件结构体
    if (ret < 0) {  // 获取失败处理
        return -get_errno();  // 返回错误码
    }

    ret = chattr(file->f_path, &attr);  // 设置文件属性（修改权限）
    if (ret < 0) {  // 设置失败处理
        return -get_errno();  // 返回错误码
    }

    return ret;  // 返回成功
}

/**
 * @brief 修改文件所有者（带目录文件描述符版本）
 * @details 通过目录文件描述符和路径名修改文件的所有者和组
 * @param fd [in] 目录文件描述符，AT_FDCWD表示当前工作目录
 * @param path [in] 文件路径名
 * @param owner [in] 新的用户ID，-1表示不修改
 * @param group [in] 新的组ID，-1表示不修改
 * @param flag [in] 标志位（未使用）
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysFchownat(int fd, const char *path, uid_t owner, gid_t group, int flag)
{
    int ret;  // 系统调用返回值
    char *fullpath = NULL;  // 完整文件路径
    struct IATTR attr = {  // 文件属性结构体
        .attr_chg_valid = 0,  // 属性修改标识初始化为0
    };

    ret = GetFullpath(fd, path, &fullpath);  // 获取完整文件路径
    if (ret < 0) {  // 获取路径失败处理
        goto OUT;  // 跳转到退出处理
    }

    if (owner != (uid_t)-1) {  // 需要修改用户ID
        attr.attr_chg_uid = owner;  // 设置新的用户ID
        attr.attr_chg_valid |= CHG_UID;  // 设置用户ID修改标识
    }
    if (group != (gid_t)-1) {  // 需要修改组ID
        attr.attr_chg_gid = group;  // 设置新的组ID
        attr.attr_chg_valid |= CHG_GID;  // 设置组ID修改标识
    }

    ret = chattr(fullpath, &attr);  // 设置文件属性（修改所有者）
    if (ret < 0) {  // 设置失败处理
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 退出处理标签
    PointerFree(fullpath);  // 释放完整路径

    return ret;  // 返回结果
}

/**
 * @brief 修改文件所有者（通过文件描述符）
 * @details 通过文件描述符修改文件的所有者和组
 * @param fd [in] 文件描述符
 * @param owner [in] 新的用户ID，-1表示不修改
 * @param group [in] 新的组ID，-1表示不修改
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysFchown(int fd, uid_t owner, gid_t group)
{
    int ret;  // 系统调用返回值
    int sysFd;  // 系统全局文件描述符
    struct IATTR attr = {0};  // 文件属性结构体
    attr.attr_chg_valid = 0;  // 属性修改标识初始化为0
    struct file *file = NULL;  // 文件结构体指针

    sysFd = GetAssociatedSystemFd(fd);  // 将进程文件描述符转换为系统全局文件描述符
    if (sysFd < 0) {  // 无效文件描述符检查
        return -EBADF;  // 返回错误的文件描述符错误
    }

    ret = fs_getfilep(sysFd, &file);  // 获取文件结构体
    if (ret < 0) {  // 获取失败处理
        return -get_errno();  // 返回错误码
    }

    if (owner != (uid_t)-1) {  // 需要修改用户ID
        attr.attr_chg_uid = owner;  // 设置新的用户ID
        attr.attr_chg_valid |= CHG_UID;  // 设置用户ID修改标识
    }
    if (group != (gid_t)-1) {  // 需要修改组ID
        attr.attr_chg_gid = group;  // 设置新的组ID
        attr.attr_chg_valid |= CHG_GID;  // 设置组ID修改标识
    }
    ret = chattr(file->f_path, &attr);  // 设置文件属性（修改所有者）
    if (ret < 0) {  // 设置失败处理
        ret = -get_errno();  // 获取错误码
    }

    return ret;  // 返回结果
}

/**
 * @brief 修改文件所有者
 * @details 修改指定路径文件的所有者和组
 * @param pathname [in] 文件路径名
 * @param owner [in] 新的用户ID，-1表示不修改
 * @param group [in] 新的组ID，-1表示不修改
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysChown(const char *pathname, uid_t owner, gid_t group)
{
    int ret;  // 系统调用返回值
    char *pathRet = NULL;  // 内核空间路径缓冲区

    if (pathname != NULL) {  // 路径不为空时复制路径
        ret = UserPathCopy(pathname, &pathRet);  // 将用户空间路径复制到内核空间
        if (ret != 0) {  // 复制失败处理
            goto OUT;  // 跳转到退出处理
        }
    }

    ret = chown(pathRet, owner, group);  // 修改文件所有者
    if (ret < 0) {  // 修改失败处理
        ret = -get_errno();  // 获取错误码
    }

OUT:  // 退出处理标签
    if (pathRet != NULL) {  // 释放内核空间路径缓冲区
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;  // 返回结果
}

/**
 * @brief 获取文件状态（带目录文件描述符版本，64位）
 * @details 通过目录文件描述符和路径名获取文件状态信息
 * @param dirfd [in] 目录文件描述符，AT_FDCWD表示当前工作目录
 * @param path [in] 文件路径名
 * @param buf [out] 存储文件状态的缓冲区
 * @param flag [in] 标志位（AT_SYMLINK_NOFOLLOW表示不跟随符号链接）
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysFstatat64(int dirfd, const char *restrict path, struct kstat *restrict buf, int flag)
{
    int ret;  // 系统调用返回值
    struct stat bufRet = {0};  // 内核空间文件状态缓冲区
    char *pathRet = NULL;  // 内核空间路径缓冲区
    char *fullpath = NULL;  // 完整文件路径

    if (path != NULL) {  // 路径不为空时复制路径
        ret = UserPathCopy(path, &pathRet);  // 将用户空间路径复制到内核空间
        if (ret != 0) {  // 复制失败处理
            goto OUT;  // 跳转到退出处理
        }
    }

    if (dirfd != AT_FDCWD) {  // 目录文件描述符不是当前工作目录
        /* Process fd convert to system global fd */
        dirfd = GetAssociatedSystemFd(dirfd);  // 转换为系统全局文件描述符
    }

    ret = vfs_normalize_pathat(dirfd, pathRet, &fullpath);  // 获取规范化的完整路径
    if (ret < 0) {  // 获取路径失败处理
        goto OUT;  // 跳转到退出处理
    }

    ret = stat(fullpath, &bufRet);  // 获取文件状态
    if (ret < 0) {  // 获取失败处理
        ret = -get_errno();  // 获取错误码
        goto OUT;  // 跳转到退出处理
    }

    ret = LOS_ArchCopyToUser(buf, &bufRet, sizeof(struct kstat));  // 将结果复制到用户空间
    if (ret != 0) {  // 复制失败处理
        ret = -EFAULT;  // 返回用户空间访问错误
        goto OUT;  // 跳转到退出处理
    }

OUT:  // 退出处理标签
    if (pathRet != NULL) {  // 释放内核空间路径缓冲区
        LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }

    if (fullpath != NULL) {  // 释放完整路径
        free(fullpath);
    }
    return ret;  // 返回结果
}

/**
 * @brief 检查文件访问权限（带目录文件描述符版本）
 * @details 检查调用进程对指定文件的访问权限
 * @param fd [in] 目录文件描述符，AT_FDCWD表示当前工作目录
 * @param filename [in] 文件路径名
 * @param amode [in] 访问模式（R_OK, W_OK, X_OK, F_OK）
 * @param flag [in] 标志位（未使用）
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysFaccessat(int fd, const char *filename, int amode, int flag)
{
    int ret;  // 系统调用返回值
    struct stat buf;  // 文件状态结构体
    struct statfs fsBuf;  // 文件系统状态结构体
    char *fullDirectory = NULL;  // 完整文件路径

    ret = GetFullpath(fd, filename, &fullDirectory);  // 获取完整文件路径
    if (ret < 0) {  // 获取路径失败处理
        goto OUT;  // 跳转到退出处理
    }

    ret = statfs(fullDirectory, &fsBuf);  // 获取文件系统状态
    if (ret != 0) {  // 获取失败处理
        ret = -get_errno();  // 获取错误码
        goto OUT;  // 跳转到退出处理
    }

    if ((fsBuf.f_flags & MS_RDONLY) && ((unsigned int)amode & W_OK)) {  // 检查只读文件系统的写权限
        ret = -EROFS;  // 返回只读文件系统错误
        goto OUT;  // 跳转到退出处理
    }

    ret = stat(fullDirectory, &buf);  // 获取文件状态
    if (ret != 0) {  // 获取失败处理
        ret = -get_errno();  // 获取错误码
        goto OUT;  // 跳转到退出处理
    }

    if (VfsPermissionCheck(buf.st_uid, buf.st_gid, buf.st_mode, amode)) {  // 检查访问权限
        ret = -EACCES;  // 返回权限拒绝错误
    }

OUT:  // 退出处理标签
    PointerFree(fullDirectory);  // 释放完整路径

    return ret;  // 返回结果
}

/**
 * @brief 获取文件系统状态
 * @details 通过文件描述符获取文件所在文件系统的状态信息
 * @param fd [in] 文件描述符
 * @param buf [out] 存储文件系统状态的缓冲区
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysFstatfs(int fd, struct statfs *buf)
{
    int ret;  // 系统调用返回值
    struct file *filep = NULL;  // 文件结构体指针
    struct statfs bufRet = {0};  // 内核空间文件系统状态缓冲区

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);  // 将进程文件描述符转换为系统全局文件描述符

    ret = fs_getfilep(fd, &filep);  // 获取文件结构体
    if (ret < 0) {  // 获取失败处理
        ret = -get_errno();  // 获取错误码
        return ret;  // 返回错误码
    }

    ret = statfs(filep->f_path, &bufRet);  // 获取文件系统状态
    if (ret < 0) {  // 获取失败处理
        ret = -get_errno();  // 获取错误码
        return ret;  // 返回错误码
    }

    ret = LOS_ArchCopyToUser(buf, &bufRet, sizeof(struct statfs));  // 将结果复制到用户空间
    if (ret != 0) {  // 复制失败处理
        ret = -EFAULT;  // 返回用户空间访问错误
    }

    return ret;  // 返回结果
}

/**
 * @brief 获取文件系统状态（64位版本）
 * @details 检查缓冲区大小后获取文件系统状态信息
 * @param fd [in] 文件描述符
 * @param sz [in] 缓冲区大小
 * @param buf [out] 存储文件系统状态的缓冲区
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysFstatfs64(int fd, size_t sz, struct statfs *buf)
{
    int ret;  // 系统调用返回值

    if (sz != sizeof(struct statfs)) {  // 检查缓冲区大小是否匹配
        ret = -EINVAL;  // 返回无效参数错误
        return ret;  // 返回错误码
    }

    ret = SysFstatfs(fd, buf);  // 调用SysFstatfs获取文件系统状态

    return ret;  // 返回结果
}

/**
 * @brief 带信号屏蔽的poll
 * @details 执行poll操作前设置信号屏蔽，操作后恢复
 * @param fds [in/out] pollfd结构体数组
 * @param nfds [in] pollfd结构体数量
 * @param tmo_p [in] 超时时间
 * @param sigMask [in] 信号屏蔽集
 * @param nsig [in] 信号数量（未使用）
 * @return 成功时返回就绪文件描述符数量，失败时返回错误码（负值）
 */
int SysPpoll(struct pollfd *fds, nfds_t nfds, const struct timespec *tmo_p, const sigset_t *sigMask, int nsig)
{
    int timeout, retVal;  // timeout: 超时时间(毫秒), retVal: 系统调用返回值
    sigset_t_l origMask = {0};  // 原始信号屏蔽集
    sigset_t_l set = {0};  // 新的信号屏蔽集

    CHECK_ASPACE(tmo_p, sizeof(struct timespec));  // 检查超时时间地址有效性
    CPY_FROM_USER(tmo_p);  // 复制超时时间到内核空间

    if (tmo_p != NULL) {  // 计算超时时间（毫秒）
        timeout = tmo_p->tv_sec * OS_SYS_US_PER_MS + tmo_p->tv_nsec / OS_SYS_NS_PER_MS;  // 秒转换为毫秒+纳秒转换为毫秒
        if (timeout < 0) {  // 超时时间有效性检查
            return -EINVAL;  // 返回无效参数错误
        }
    } else {  // 无超时时间（阻塞等待）
        timeout = -1;
    }

    if (sigMask != NULL) {  // 需要设置信号屏蔽
        retVal = LOS_ArchCopyFromUser(&set, sigMask, sizeof(sigset_t));  // 复制信号屏蔽集
        if (retVal != 0) {  // 复制失败处理
            return -EFAULT;  // 返回用户空间访问错误
        }
        (VOID)OsSigprocMask(SIG_SETMASK, &set, &origMask);  // 设置信号屏蔽并保存原始屏蔽集
    } else {  // 不需要设置信号屏蔽，仅保存原始屏蔽集
        (VOID)OsSigprocMask(SIG_SETMASK, NULL, &origMask);
    }

    retVal = SysPoll(fds, nfds, timeout);  // 执行poll操作
    (VOID)OsSigprocMask(SIG_SETMASK, &origMask, NULL);  // 恢复原始信号屏蔽集

    return retVal;  // 返回poll结果
}

/**
 * @brief 带信号屏蔽的select
 * @details 执行select操作前设置信号屏蔽，操作后恢复
 * @param nfds [in] 最大文件描述符+1
 * @param readfds [in/out] 读文件描述符集
 * @param writefds [in/out] 写文件描述符集
 * @param exceptfds [in/out] 异常文件描述符集
 * @param timeout [in] 超时时间
 * @param data [in] 信号屏蔽集数据
 * @return 成功时返回就绪文件描述符数量，失败时返回错误码（负值）
 */
int SysPselect6(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
    const struct timespec *timeout, const long data[2])
{
    int ret;  // select返回值
    int retVal;  // 系统调用返回值
    sigset_t_l origMask;  // 原始信号屏蔽集
    sigset_t_l setl;  // 新的信号屏蔽集

    CHECK_ASPACE(readfds, sizeof(fd_set));  // 检查读文件描述符集地址有效性
    CHECK_ASPACE(writefds, sizeof(fd_set));  // 检查写文件描述符集地址有效性
    CHECK_ASPACE(exceptfds, sizeof(fd_set));  // 检查异常文件描述符集地址有效性
    CHECK_ASPACE(timeout, sizeof(struct timeval));  // 检查超时时间地址有效性

    CPY_FROM_USER(readfds);  // 复制读文件描述符集到内核空间
    CPY_FROM_USER(writefds);  // 复制写文件描述符集到内核空间
    CPY_FROM_USER(exceptfds);  // 复制异常文件描述符集到内核空间
    DUP_FROM_USER(timeout, sizeof(struct timeval));  // 复制超时时间到内核空间

    if (timeout != NULL) {  // 转换纳秒为微秒
        ((struct timeval *)timeout)->tv_usec = timeout->tv_nsec / 1000; /* 1000, convert ns to us */
    }

    if (data != NULL) {  // 复制信号屏蔽集
        retVal = LOS_ArchCopyFromUser(&(setl.sig[0]), (int *)((UINTPTR)data[0]), sizeof(sigset_t));
        if (retVal != 0) {  // 复制失败处理
            ret = -EFAULT;  // 返回用户空间访问错误
            FREE_DUP(timeout);  // 释放内核空间超时时间
            return ret;  // 返回错误码
        }
    }

    OsSigprocMask(SIG_SETMASK, &setl, &origMask);  // 设置信号屏蔽并保存原始屏蔽集
    ret = do_select(nfds, readfds, writefds, exceptfds, (struct timeval *)timeout, UserPoll);  // 执行select操作
    if (ret < 0) {  // select失败处理
        /* do not copy parameter back to user mode if do_select failed */
        ret = -get_errno();  // 获取错误码
        FREE_DUP(timeout);  // 释放内核空间超时时间
        return ret;  // 返回错误码
    }
    OsSigprocMask(SIG_SETMASK, &origMask, NULL);  // 恢复原始信号屏蔽集

    CPY_TO_USER(readfds);  // 将读文件描述符集复制回用户空间
    CPY_TO_USER(writefds);  // 将写文件描述符集复制回用户空间
    CPY_TO_USER(exceptfds);  // 将异常文件描述符集复制回用户空间
    FREE_DUP(timeout);  // 释放内核空间超时时间

    return ret;  // 返回select结果
}

/**
 * @brief 创建epoll实例（内部函数）
 * @details 创建epoll实例并关联进程文件描述符
 * @param flags [in] 创建标志（EPOLL_CLOEXEC等）
 * @return 成功时返回进程文件描述符，失败时返回错误码（负值）
 */
static int DoEpollCreate1(int flags)
{
    int ret;  // epoll_create1返回值
    int procFd;  // 进程文件描述符

    ret = epoll_create1(flags);  // 创建epoll实例
    if (ret < 0) {  // 创建失败处理
        ret = -get_errno();  // 获取错误码
        return ret;  // 返回错误码
    }

    procFd = AllocAndAssocProcessFd((INTPTR)(ret), MIN_START_FD);  // 分配并关联进程文件描述符
    if (procFd == -1) {  // 分配失败处理
        epoll_close(ret);  // 关闭epoll实例
        return -EMFILE;  // 返回文件描述符用尽错误
    }

    return procFd;  // 返回进程文件描述符
}

/**
 * @brief 创建epoll实例
 * @details 创建epoll实例（兼容旧接口）
 * @param size [in] 提示的文件描述符数量（已忽略）
 * @return 成功时返回进程文件描述符，失败时返回错误码（负值）
 */
int SysEpollCreate(int size)
{
    (void)size;  // 忽略size参数
    return DoEpollCreate1(0);  // 调用DoEpollCreate1创建epoll实例
}

/**
 * @brief 创建epoll实例（带标志）
 * @details 创建epoll实例并支持设置标志
 * @param flags [in] 创建标志（EPOLL_CLOEXEC等）
 * @return 成功时返回进程文件描述符，失败时返回错误码（负值）
 */
int SysEpollCreate1(int flags)
{
    return DoEpollCreate1(flags);  // 调用DoEpollCreate1创建epoll实例
}

/**
 * @brief 控制epoll实例
 * @details 向epoll实例添加、修改或删除感兴趣的文件描述符
 * @param epfd [in] epoll实例文件描述符
 * @param op [in] 操作类型（EPOLL_CTL_ADD, EPOLL_CTL_MOD, EPOLL_CTL_DEL）
 * @param fd [in] 要操作的文件描述符
 * @param ev [in/out] epoll事件结构体
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysEpollCtl(int epfd, int op, int fd, struct epoll_event *ev)
{
    int ret;  // epoll_ctl返回值

    CHECK_ASPACE(ev, sizeof(struct epoll_event));  // 检查事件结构体地址有效性
    CPY_FROM_USER(ev);  // 复制事件结构体到内核空间

    fd = GetAssociatedSystemFd(fd);  // 将进程文件描述符转换为系统全局文件描述符
    epfd = GetAssociatedSystemFd(epfd);  // 将epoll实例文件描述符转换为系统全局文件描述符
    if ((fd < 0) || (epfd < 0)) {  // 无效文件描述符检查
        ret = -EBADF;  // 返回错误的文件描述符错误
        goto OUT;  // 跳转到退出处理
    }

    ret = epoll_ctl(epfd, op, fd, ev);  // 执行epoll控制操作
    if (ret < 0) {  // 操作失败处理
        ret = -EBADF;  // 返回错误的文件描述符错误
        goto OUT;  // 跳转到退出处理
    }

    CPY_TO_USER(ev);  // 将事件结构体复制回用户空间
OUT:  // 退出处理标签
    return (ret == -1) ? -get_errno() : ret;  // 返回结果
}

/**
 * @brief 等待epoll事件
 * @details 等待epoll实例上注册的文件描述符发生事件
 * @param epfd [in] epoll实例文件描述符
 * @param evs [out] 存储事件的缓冲区
 * @param maxevents [in] 最大事件数量
 * @param timeout [in] 超时时间（毫秒，-1表示阻塞）
 * @return 成功时返回发生事件的数量，失败时返回错误码（负值）
 */
int SysEpollWait(int epfd, struct epoll_event *evs, int maxevents, int timeout)
{
    int ret = 0;  // epoll_wait返回值

    if ((maxevents <= 0) || (maxevents > EPOLL_DEFAULT_SIZE)) {  // 检查最大事件数量有效性
        ret = -EINVAL;  // 返回无效参数错误
        goto OUT;  // 跳转到退出处理
    }

    CHECK_ASPACE(evs, sizeof(struct epoll_event) * maxevents);  // 检查事件缓冲区地址有效性
    DUP_FROM_USER_NOCOPY(evs, sizeof(struct epoll_event) * maxevents);  // 复制事件缓冲区到内核空间

    epfd = GetAssociatedSystemFd(epfd);  // 将epoll实例文件描述符转换为系统全局文件描述符
    if  (epfd < 0) {  // 无效文件描述符检查
        ret = -EBADF;  // 返回错误的文件描述符错误
        goto OUT;  // 跳转到退出处理
    }

    ret = epoll_wait(epfd, evs, maxevents, timeout);  // 等待epoll事件
    if (ret < 0) {  // 等待失败处理
        ret = -get_errno();  // 获取错误码
    }

    DUP_TO_USER(evs, sizeof(struct epoll_event) * ret);  // 将事件结果复制回用户空间
    FREE_DUP(evs);  // 释放内核空间事件缓冲区
OUT:  // 退出处理标签
    return (ret == -1) ? -get_errno() : ret;  // 返回结果
}

/**
 * @brief 带信号屏蔽的epoll等待
 * @details 等待epoll事件前设置信号屏蔽，等待后恢复
 * @param epfd [in] epoll实例文件描述符
 * @param evs [out] 存储事件的缓冲区
 * @param maxevents [in] 最大事件数量
 * @param timeout [in] 超时时间（毫秒，-1表示阻塞）
 * @param mask [in] 信号屏蔽集
 * @return 成功时返回发生事件的数量，失败时返回错误码（负值）
 */
int SysEpollPwait(int epfd, struct epoll_event *evs, int maxevents, int timeout, const sigset_t *mask)
{
    sigset_t_l origMask;  // 原始信号屏蔽集
    sigset_t_l setl;  // 新的信号屏蔽集
    int ret = 0;  // epoll_wait返回值

    if ((maxevents <= 0) || (maxevents > EPOLL_DEFAULT_SIZE)) {  // 检查最大事件数量有效性
        ret = -EINVAL;  // 返回无效参数错误
        goto OUT;  // 跳转到退出处理
    }

    CHECK_ASPACE(mask, sizeof(sigset_t));  // 检查信号屏蔽集地址有效性

    if (mask != NULL) {  // 复制信号屏蔽集
        ret = LOS_ArchCopyFromUser(&setl, mask, sizeof(sigset_t));
        if (ret != 0) {  // 复制失败处理
            return -EFAULT;  // 返回用户空间访问错误
        }
    }

    CHECK_ASPACE(evs, sizeof(struct epoll_event) * maxevents);  // 检查事件缓冲区地址有效性
    DUP_FROM_USER_NOCOPY(evs, sizeof(struct epoll_event) * maxevents);  // 复制事件缓冲区到内核空间

    epfd = GetAssociatedSystemFd(epfd);  // 将epoll实例文件描述符转换为系统全局文件描述符
    if (epfd < 0) {  // 无效文件描述符检查
        ret = -EBADF;  // 返回错误的文件描述符错误
        goto OUT;  // 跳转到退出处理
    }

    OsSigprocMask(SIG_SETMASK, &setl, &origMask);  // 设置信号屏蔽并保存原始屏蔽集
    ret = epoll_wait(epfd, evs, maxevents, timeout);  // 等待epoll事件
    if (ret < 0) {  // 等待失败处理
        ret = -get_errno();  // 获取错误码
    }

    OsSigprocMask(SIG_SETMASK, &origMask, NULL);  // 恢复原始信号屏蔽集

    DUP_TO_USER(evs, sizeof(struct epoll_event) * ret);  // 将事件结果复制回用户空间
    FREE_DUP(evs);  // 释放内核空间事件缓冲区
OUT:  // 退出处理标签
    return (ret == -1) ? -get_errno() : ret;  // 返回结果
}

#ifdef LOSCFG_CHROOT
/**
 * @brief 更改根目录
 * @details 更改当前进程的根目录
 * @param path [in] 新的根目录路径
 * @return 成功时返回0，失败时返回错误码（负值）
 */
int SysChroot(const char *path)
{
    int ret;  // 系统调用返回值
    char *pathRet = NULL;  // 内核空间路径缓冲区

    if (path != NULL) {  // 路径不为空时复制路径
        ret = UserPathCopy(path, &pathRet);  // 将用户空间路径复制到内核空间
        if (ret != 0) {  // 复制失败处理
            goto OUT;  // 跳转到退出处理
        }
    }

    ret = chroot(path ? pathRet : NULL);  // 执行更改根目录操作
    if (ret < 0) {  // 操作失败处理
        ret = -get_errno();  // 获取错误码
    }
OUT:  // 退出处理标签
    if (pathRet != NULL) {  // 释放内核空间路径缓冲区
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;  // 返回结果
}
#endif
#endif
