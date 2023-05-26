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
//拷贝用户空间路径到内核空间
#define HIGH_SHIFT_BIT 32
#define TIMESPEC_TIMES_NUM  2

static int CheckNewAttrTime(struct IATTR *attr, struct timespec times[TIMESPEC_TIMES_NUM])
{
    int ret = ENOERR;
    struct timespec stp = {0};

    if (times) {
        if (times[0].tv_nsec == UTIME_OMIT) {
            attr->attr_chg_valid &= ~CHG_ATIME;
        } else if (times[0].tv_nsec == UTIME_NOW) {
            ret = clock_gettime(CLOCK_REALTIME, &stp);
            if (ret < 0) {
                return -get_errno();
            }
            attr->attr_chg_atime = (unsigned int)stp.tv_sec;
            attr->attr_chg_valid |= CHG_ATIME;
        } else {
            attr->attr_chg_atime = (unsigned int)times[0].tv_sec;
            attr->attr_chg_valid |= CHG_ATIME;
        }

        if (times[1].tv_nsec == UTIME_OMIT) {
            attr->attr_chg_valid &= ~CHG_MTIME;
        } else if (times[1].tv_nsec == UTIME_NOW) {
            ret = clock_gettime(CLOCK_REALTIME, &stp);
            if (ret < 0) {
                return -get_errno();
            }
            attr->attr_chg_mtime = (unsigned int)stp.tv_sec;
            attr->attr_chg_valid |= CHG_MTIME;
        } else {
            attr->attr_chg_mtime = (unsigned int)times[1].tv_sec;
            attr->attr_chg_valid |= CHG_MTIME;
        }
    } else {
        ret = clock_gettime(CLOCK_REALTIME, &stp);
        if (ret < 0) {
            return -get_errno();
        }
        attr->attr_chg_atime = (unsigned int)stp.tv_sec;
        attr->attr_chg_mtime = (unsigned int)stp.tv_sec;
        attr->attr_chg_valid |= CHG_ATIME;
        attr->attr_chg_valid |= CHG_MTIME;
    }

    return ret;
}
///获取全路径
static int GetFullpathNull(int fd, const char *path, char **filePath)
{
    int ret;
    char *fullPath = NULL;
    struct file *file = NULL;

    if ((fd != AT_FDCWD) && (path == NULL)) {
        fd = GetAssociatedSystemFd(fd);
        ret = fs_getfilep(fd, &file);
        if (ret < 0) {
            return -get_errno();
        }
        fullPath = strdup(file->f_path);
        if (fullPath == NULL) {
            ret = -ENOMEM;
        }
    } else {
        ret = GetFullpath(fd, path, &fullPath);
        if (ret < 0) {
            return ret;
        }
    }

    *filePath = fullPath;
    return ret;
}
///用户空间 io向量地址范围检查
static int UserIovItemCheck(const struct iovec *iov, const int iovcnt)
{
    int i;
    for (i = 0; i < iovcnt; ++i) {
        if (iov[i].iov_len == 0) {
            continue;
        }

        if (!LOS_IsUserAddressRange((vaddr_t)(UINTPTR)iov[i].iov_base, iov[i].iov_len)) {
            return i;
        }
    }
    return iovcnt;
}

static int UserIovCopy(struct iovec **iovBuf, const struct iovec *iov, const int iovcnt, int *valid_iovcnt)
{
    int ret;
    int bufLen = iovcnt * sizeof(struct iovec);
    if (bufLen < 0) {
        return -EINVAL;
    }

    *iovBuf = (struct iovec*)LOS_MemAlloc(OS_SYS_MEM_ADDR, bufLen);
    if (*iovBuf == NULL) {
        return -ENOMEM;
    }

    if (LOS_ArchCopyFromUser(*iovBuf, iov, bufLen) != 0) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, *iovBuf);
        return -EFAULT;
    }

    ret = UserIovItemCheck(*iovBuf, iovcnt);
    if (ret == 0) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, *iovBuf);
        return -EFAULT;
    }

    *valid_iovcnt = ret;
    return 0;
}

static int PollfdToSystem(struct pollfd *fds, nfds_t nfds, int **pollFdsBak)
{
    if ((nfds != 0 && fds == NULL) || (pollFdsBak == NULL)) {
        set_errno(EINVAL);
        return -1;
    }
    if (nfds == 0) {
        return 0;
    }
    int *pollFds = (int *)malloc(sizeof(int) * nfds);
    if (pollFds == NULL) {
        set_errno(ENOMEM);
        return -1;
    }
    for (int i = 0; i < nfds; ++i) {
        struct pollfd *p_fds = &fds[i];
        pollFds[i] = p_fds->fd;
        if (p_fds->fd < 0) {
            set_errno(EBADF);
            free(pollFds);
            return -1;
        }
        p_fds->fd = GetAssociatedSystemFd(p_fds->fd);
    }
    *pollFdsBak = pollFds;
    return 0;
}

static void RestorePollfd(struct pollfd *fds, nfds_t nfds, const int *pollFds)
{
    if ((fds == NULL) || (pollFds == NULL)) {
        return;
    }
    for (int i = 0; i < nfds; ++i) {
        struct pollfd *p_fds = &fds[i];
        p_fds->fd = pollFds[i];
    }
}
///使用poll方式 实现IO多路复用的机制
static int UserPoll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    int *pollFds = NULL;
    int ret = PollfdToSystem(fds, nfds, &pollFds);
    if (ret < 0) {
        return -1;
    }

    ret = poll(fds, nfds, timeout);

    RestorePollfd(fds, nfds, pollFds);

    free(pollFds);
    return ret;
}

//关闭文件句柄
int SysClose(int fd)
{
    int ret;

    /* Process fd convert to system global fd */
    int sysfd = DisassociateProcessFd(fd);//先解除关联

    ret = close(sysfd);//关闭文件,个人认为应该先 close - > DisassociateProcessFd 
    if (ret < 0) {//关闭失败时
        AssociateSystemFd(fd, sysfd);//继续关联
        return -get_errno();
    }
    FreeProcessFd(fd);//释放进程fd
    return ret;
}
///系统调用|读文件:从文件中读取nbytes长度的内容到buf中(用户空间)
ssize_t SysRead(int fd, void *buf, size_t nbytes)
{
    int ret;

    if (nbytes == 0) {
        return 0;
    }
	//[buf,buf+nbytes]地址必须在用户空间
    if (!LOS_IsUserAddressRange((vaddr_t)(UINTPTR)buf, nbytes)) {
        return -EFAULT;
    }

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);//获得关联的系统fd,因为真正的read,write是需要sysFd的
    ret = read(fd, buf, nbytes);
    if (ret < 0) {
        return -get_errno();
    }
    return ret;
}
///系统调用|写文件:将buf中(用户空间)nbytes长度的内容写到文件中
ssize_t SysWrite(int fd, const void *buf, size_t nbytes)
{
    int ret;

    if (nbytes == 0) {
        return 0;
    }
	//[buf,buf+nbytes]地址必须在用户空间
    if (!LOS_IsUserAddressRange((vaddr_t)(UINTPTR)buf, nbytes)) {
        return -EFAULT;
    }

    /* Process fd convert to system global fd */
    int sysfd = GetAssociatedSystemFd(fd);
    ret = write(sysfd, buf, nbytes);
    if (ret < 0) {
        return -get_errno();
    }
    return ret;
}

// int vfs_normalize_path(const char *directory, const char *filename, char **pathname)
#ifdef LOSCFG_PID_CONTAINER
#ifdef LOSCFG_PROC_PROCESS_DIR
#define PROCESS_DIR_ROOT   "/proc"
static char *NextName(char *pos, uint8_t *len)
{
    char *name = NULL;
    while (*pos != 0 && *pos == '/') {
        pos++;
    }
    if (*pos == '\0') {
        return NULL;
    }
    name = (char *)pos;
    while (*pos != '\0' && *pos != '/') {
        pos++;
    }
    *len = pos - name;
    return name;
}

static unsigned int ProcRealProcessIDGet(unsigned int pid)
{
    unsigned int intSave;
    if (OS_PID_CHECK_INVALID(pid)) {
        return 0;
    }

    SCHEDULER_LOCK(intSave);
    LosProcessCB *pcb = OsGetPCBFromVpid(pid);
    if (OsProcessIsInactive(pcb)) {
        SCHEDULER_UNLOCK(intSave);
        return 0;
    }

    int rootPid = OsGetRootPid(pcb);
    SCHEDULER_UNLOCK(intSave);
    if ((rootPid == OS_INVALID_VALUE) || (rootPid == pid)) {
        return 0;
    }

    return rootPid;
}

static int ProcRealProcessDirGet(char *path)
{
    char pidBuf[PATH_MAX] = {0};
    char *fullPath = NULL;
    uint8_t strLen = 0;
    int pid, rootPid;
    int ret = vfs_normalize_path(NULL, path, &fullPath);
    if (ret < 0) {
        return ret;
    }

    int procLen = strlen(PROCESS_DIR_ROOT);
    if (strncmp(fullPath, PROCESS_DIR_ROOT, procLen) != 0) {
        free(fullPath);
        return 0;
    }

    char *pidStr = NextName(fullPath + procLen, &strLen);
    if (pidStr == NULL) {
        free(fullPath);
        return 0;
    }

    if ((*pidStr <= '0') || (*pidStr > '9')) {
        free(fullPath);
        return 0;
    }

    if (memcpy_s(pidBuf, PATH_MAX, pidStr, strLen) != EOK) {
        free(fullPath);
        return 0;
    }
    pidBuf[strLen] = '\0';

    pid = atoi(pidBuf);
    if (pid == 0) {
        free(fullPath);
        return 0;
    }

    rootPid = ProcRealProcessIDGet((unsigned)pid);
    if (rootPid == 0) {
        free(fullPath);
        return 0;
    }

    if (snprintf_s(path, PATH_MAX + 1, PATH_MAX, "/proc/%d%s", rootPid, (pidStr + strLen)) < 0) {
        free(fullPath);
        return -EFAULT;
    }

    free(fullPath);
    return 0;
}
#endif
#endif

static int GetPath(const char *path, char **pathRet)
{
    int ret = UserPathCopy(path, pathRet);
    if (ret != 0) {
        return ret;
    }
#ifdef LOSCFG_PID_CONTAINER
#ifdef LOSCFG_PROC_PROCESS_DIR
    ret = ProcRealProcessDirGet(*pathRet);
    if (ret != 0) {
        return ret;
    }
#endif
#endif
    return 0;
}
///系统调用|打开文件, 正常情况下返回进程的FD值
int SysOpen(const char *path, int oflags, ...)
{
    int ret;
    int procFd = -1;
    mode_t mode = DEFAULT_FILE_MODE; /* 0666: File read-write properties. */
    char *pathRet = NULL;

    if (path != NULL) {
        ret = GetPath(path, &pathRet);
        if (ret != 0) {
            goto ERROUT;
        }
    }

    procFd = AllocProcessFd();//分配进程描述符
    if (procFd < 0) {
        ret = -EMFILE;
        goto ERROUT;
    }

    if (oflags & O_CLOEXEC) {
        SetCloexecFlag(procFd);
    }

    if ((unsigned int)oflags & O_DIRECTORY) {//目录标签
        ret = do_opendir(pathRet, oflags);//打开目录
    } else {

#ifdef LOSCFG_FILE_MODE //文件权限开关
    va_list ap;

    va_start(ap, oflags);
    mode = va_arg(ap, int);//可变参数读取mode值
    va_end(ap);
#endif
//当fd参数的值是AT_FDCWD，并且path参数是一个相对路径名，fstatat会计算相对于当前目录的path参数。
//如果path是一个绝对路径，fd参数就会被忽略
        ret = do_open(AT_FDCWD, pathRet, oflags, mode);//打开文件,返回系统描述符
    }

    if (ret < 0) {
        ret = -get_errno();
        goto ERROUT;
    }

    AssociateSystemFd(procFd, ret);//进程<->系统描述符 关联/绑定
    if (pathRet != NULL) {
        LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return procFd;//返回进程描述符

ERROUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    if (procFd >= 0) {
        FreeProcessFd(procFd);
    }
    return ret;
}

/*!
 * @brief 创建文件,从实现看 SysCreat 和 SysOpen 并没有太大的区别,只有打开方式的区别
 * \n SysCreat函数完全可以被SysOpen函数替代
 * @param pathname 
 * @param mode 
 * @verbatim 常用标签如下:
    O_CREAT:若文件存在，此标志无用；若不存在，建新文件
    O_TRUNC:若文件存在，则长度被截为0，属性不变
    O_WRONLY:写文件 
    O_RDONLY:读文件
    O_BINARY:此标志可显示地给出以二进制方式打开文件 
    O_TEXT :此标志可用于显示地给出以文本方式打开文件
    O_RDWR :即读也写
    O_APPEND:即读也写，但每次写总是在文件尾添加
 * @endverbatim
 * @return int 
 */
int SysCreat(const char *pathname, mode_t mode)
{
    int ret = 0;
    char *pathRet = NULL;

    if (pathname != NULL) {
        ret = UserPathCopy(pathname, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    int procFd = AllocProcessFd();
    if (procFd  < 0) {
        ret = -EMFILE;
        goto OUT;
    }
	// 调用关系 open -> do_open chmod 666 
    ret = open((pathname ? pathRet : NULL), O_CREAT | O_TRUNC | O_WRONLY, mode);
    if (ret < 0) {
        FreeProcessFd(procFd);
        ret = -get_errno();
    } else {
        AssociateSystemFd(procFd, ret);
        ret = procFd;
    }

OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;
}

int SysLink(const char *oldpath, const char *newpath)
{
    int ret;
    char *oldpathRet = NULL;
    char *newpathRet = NULL;

    if (oldpath != NULL) {
        ret = UserPathCopy(oldpath, &oldpathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    if (newpath != NULL) {
        ret = UserPathCopy(newpath, &newpathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    ret = link(oldpathRet, newpathRet);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    if (oldpathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, oldpathRet);
    }
    if (newpathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, newpathRet);
    }
    return ret;
}

ssize_t SysReadlink(const char *pathname, char *buf, size_t bufsize)
{
    ssize_t ret;
    char *pathRet = NULL;

    if (bufsize == 0) {
        return -EINVAL;
    }

    if (pathname != NULL) {
        ret = UserPathCopy(pathname, &pathRet);
        if (ret != 0) {
            goto OUT;
        }

#ifdef LOSCFG_PID_CONTAINER
#ifdef LOSCFG_PROC_PROCESS_DIR
        ret = ProcRealProcessDirGet(pathRet);
        if (ret != 0) {
            goto OUT;
        }
#endif
#endif
    }

    if (!LOS_IsUserAddressRange((vaddr_t)(UINTPTR)buf, bufsize)) {
        ret = -EFAULT;
        goto OUT;
    }

    ret = readlink(pathRet, buf, bufsize);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;
}

int SysSymlink(const char *target, const char *linkpath)
{
    int ret;
    char *targetRet = NULL;
    char *pathRet = NULL;

    if (target != NULL) {
        ret = UserPathCopy(target, &targetRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    if (linkpath != NULL) {
        ret = UserPathCopy(linkpath, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    ret = symlink(targetRet, pathRet);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }

    if (targetRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, targetRet);
    }
    return ret;
}

/**
 * @brief 删除链:删除由装入点管理的文件
 * @verbatim 
    执行unlink()函数并不一定会真正的删除文件，它先会检查文件系统中此文件的连接数是否为1，
    如果不是1说明此文件还有其他链接对象，因此只对此文件的连接数进行减1操作。若连接数为1，
    并且在此时没有任何进程打开该文件，此内容才会真正地被删除掉。在有进程打开此文件的情况下，
    则暂时不会删除，直到所有打开该文件的进程都结束时文件就会被删除。
 * @endverbatim
 */
int SysUnlink(const char *pathname)
{
    int ret;
    char *pathRet = NULL;

    if (pathname != NULL) {
        ret = UserPathCopy(pathname, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }
	//删除由装入点管理的文件
    ret = do_unlink(AT_FDCWD, (pathname ? pathRet : NULL));
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;
}
///动态加载程序过程
#ifdef LOSCFG_KERNEL_DYNLOAD
/*
argv :参数数组 {"ls", "-al", "/etc/passwd", NULL};
envp :环境变量数组 {"PATH=/bin", NULL}  
*/
int SysExecve(const char *fileName, char *const *argv, char *const *envp)
{
    return LOS_DoExecveFile(fileName, argv, envp);
}
#endif

int SysFchdir(int fd)
{
    int ret;
    int sysFd;
    struct file *file = NULL;

    sysFd = GetAssociatedSystemFd(fd);
    if (sysFd < 0) {
        return -EBADF;
    }

    ret = fs_getfilep(sysFd, &file);
    if (ret < 0) {
        return -get_errno();
    }

    ret = chdir(file->f_path);
    if (ret < 0) {
        ret = -get_errno();
    }

    return ret;
}

int SysChdir(const char *path)
{
    int ret;
    char *pathRet = NULL;

    if (path != NULL) {
        ret = UserPathCopy(path, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    ret = chdir(path ? pathRet : NULL);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;
}
///移动文件指针
off_t SysLseek(int fd, off_t offset, int whence)
{
    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);

    return _lseek(fd, offset, whence);
}

//移动文件指针
off64_t SysLseek64(int fd, int offsetHigh, int offsetLow, off64_t *result, int whence)
{
    off64_t ret;
    off64_t res;
    int retVal;

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);

    ret = _lseek64(fd, offsetHigh, offsetLow, &res, whence);
    if (ret != 0) {
        return ret;
    }

    retVal = LOS_ArchCopyToUser(result, &res, sizeof(off64_t));
    if (retVal != 0) {
        return -EFAULT;
    }

    return 0;
}

#ifdef LOSCFG_FS_NFS
static int NfsMountRef(const char *serverIpAndPath, const char *mountPath,
                       unsigned int uid, unsigned int gid) __attribute__((weakref("nfs_mount")));

static int NfsMount(const char *serverIpAndPath, const char *mountPath,
                    unsigned int uid, unsigned int gid)
{
    int ret;

    if ((serverIpAndPath == NULL) || (mountPath == NULL)) {
        return -EINVAL;
    }
    ret = NfsMountRef(serverIpAndPath, mountPath, uid, gid);
    if (ret < 0) {
        ret = -get_errno();
    }
    return ret;
}
#endif

/*!
 * @brief SysMount	挂载文件系统
 * 挂载是指将一个存储设备挂接到一个已存在的路径上。我们要访问存储设备中的文件，必须将文件所在的分区挂载到一个已存在的路径上，
 * 然后通过这个路径来访问存储设备。如果只有一个存储设备，则可以直接挂载到根目录 / 上,变成根文件系统
 * @param data	特定文件系统的私有数据
 * @param filesystemtype 挂载的文件系统类型	
 * @param mountflags 读写标志位	
 * @param source 已经格式化的块设备名称	
 * @param target 挂载路径，即挂载点	
 * @return	
 *
 * @see
 */
int SysMount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags,
             const void *data)
{
    int ret;
    char *sourceRet = NULL;
    char *targetRet = NULL;
    char *dataRet = NULL;
    char fstypeRet[FILESYSTEM_TYPE_MAX + 1] = {0};

    if (!IsCapPermit(CAP_FS_MOUNT)) {
        return -EPERM;
    }

    if (target != NULL) {
        ret = UserPathCopy(target, &targetRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    if (filesystemtype != NULL) {
        ret = LOS_StrncpyFromUser(fstypeRet, filesystemtype, FILESYSTEM_TYPE_MAX + 1);
        if (ret < 0) {
            goto OUT;
        } else if (ret > FILESYSTEM_TYPE_MAX) {
            ret = -ENODEV;
            goto OUT;
        }

        if (strcmp(fstypeRet, "ramfs") && (source != NULL)) {
            ret = UserPathCopy(source, &sourceRet);
            if (ret != 0) {
                goto OUT;
            }
        }
#ifdef LOSCFG_FS_NFS
        if (strcmp(fstypeRet, "nfs") == 0) {
            ret = NfsMount(sourceRet, targetRet, 0, 0);
            goto OUT;
        }
#endif
    }

    if (data != NULL) {
        ret = UserPathCopy(data, &dataRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    ret = mount(sourceRet, targetRet, (filesystemtype ? fstypeRet : NULL), mountflags, dataRet);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    if (sourceRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, sourceRet);
    }
    if (targetRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, targetRet);
    }
    if (dataRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, dataRet);
    }
    return ret;
}
///卸载文件系统,当某个文件系统不需要再使用了，那么可以将它卸载掉。
int SysUmount(const char *target)
{
    int ret;
    char *pathRet = NULL;

    if (!IsCapPermit(CAP_FS_MOUNT)) {
        return -EPERM;
    }

    if (target != NULL) {
        ret = UserPathCopy(target, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    ret = umount(target ? pathRet : NULL);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;
}
///确定文件的可存取性
int SysAccess(const char *path, int amode)
{
    int ret;
    char *pathRet = NULL;

    if (path != NULL) {
        ret = UserPathCopy(path, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    ret = access(pathRet, amode);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }

    return ret;
}
///重命名文件
int SysRename(const char *oldpath, const char *newpath)
{
    int ret;
    char *pathOldRet = NULL;
    char *pathNewRet = NULL;

    if (oldpath != NULL) {
        ret = UserPathCopy(oldpath, &pathOldRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    if (newpath != NULL) {
        ret = UserPathCopy(newpath, &pathNewRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    ret = do_rename(AT_FDCWD, (oldpath ? pathOldRet : NULL), AT_FDCWD,
                    (newpath ? pathNewRet : NULL));
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    if (pathOldRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathOldRet);
    }
    if (pathNewRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathNewRet);
    }
    return ret;
}
///创建目录
int SysMkdir(const char *pathname, mode_t mode)
{
    int ret;
    char *pathRet = NULL;

    if (pathname != NULL) {
        ret = UserPathCopy(pathname, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    ret = do_mkdir(AT_FDCWD, (pathname ? pathRet : NULL), mode);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;
}
///删除目录
int SysRmdir(const char *pathname)
{
    int ret;
    char *pathRet = NULL;

    if (pathname != NULL) {
        ret = UserPathCopy(pathname, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    ret = do_rmdir(AT_FDCWD, (pathname ? pathRet : NULL));
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;
}

int SysDup(int fd)
{
    int sysfd = GetAssociatedSystemFd(fd);
    /* Check if the param is valid, note that: socket fd is not support dup2 */
    if ((sysfd < 0) || (sysfd >= CONFIG_NFILE_DESCRIPTORS)) {
        return -EBADF;
    }

    int dupfd = AllocProcessFd();
    if (dupfd < 0) {
        return -EMFILE;
    }

    files_refer(sysfd);
    AssociateSystemFd(dupfd, sysfd);
    return dupfd;
}
///将内存缓冲区数据写回硬盘
void SysSync(void)
{
    sync();
}
///卸载文件系统
int SysUmount2(const char *target, int flags)
{
    if (flags != 0) {
        return -EINVAL;
    }
    return SysUmount(target);
}
///I/O总控制函数
int SysIoctl(int fd, int req, void *arg)
{
    int ret;
    unsigned int size = _IOC_SIZE((unsigned int)req);
    unsigned int dir = _IOC_DIR((unsigned int)req);
    if ((size == 0) && (dir != _IOC_NONE)) {
        return -EINVAL;
    }

    if ((dir != _IOC_NONE) && (((void *)(uintptr_t)arg) == NULL)) {
        return -EINVAL;
    }

    if ((dir & _IOC_READ) || (dir & _IOC_WRITE)) {
        if (!LOS_IsUserAddressRange((uintptr_t)arg, size)) {
            return -EFAULT;
        }
    }

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);

    ret = ioctl(fd, req, arg);
    if (ret < 0) {
        return -get_errno();
    }
    return ret;
}

/**
 * @brief 
 * @verbatim 
用来修改已经打开文件的属性的函数包含5个功能：
1.复制一个已有文件描述符，功能和dup和dup2相同，对应的cmd：F_DUPFD、F_DUPFD_CLOEXEC。
	当使用这两个cmd时，需要传入第三个参数，fcntl返回复制后的文件描述符，此返回值是之前未被占用的描述符，
	并且必须一个大于等于第三个参数值。
	F_DUPFD命令要求返回的文件描述符会清除对应的FD_CLOEXEC
	F_DUPFD_CLOEXEC要求设置新描述符的FD_CLOEXEC标志。

2.获取、设置文件描述符标志，对应的cmd：F_GETFD、F_SETFD。
	用于设置FD_CLOEXEC标志，此标志的含义是：当进程执行exec系统调用后此文件描述符会被自动关闭。

3.获取、设置文件访问状态标志，对应的cmd：F_GETFL、F_SETFL。
	获取当前打开文件的访问标志，设置对应的访问标志，一般常用来设置做非阻塞读写操作。

4.获取、设置记录锁功能，对应的cmd：F_GETLK、F_SETLK、F_SETLKW。

5.获取、设置异步I/O所有权，对应的cmd：F_GETOWN、F_SETOWN。
  	获取和设置用来接收SIGIO/SIGURG信号的进程id或者进程组id。返回对应的进程id或者进程组id取负值。
 * @endverbatim  
 * @param fd 
 * @param cmd 
 * @param arg 
 * @return int 
 */
int SysFcntl(int fd, int cmd, void *arg)
{
    /* Process fd convert to system global fd */
    int sysfd = GetAssociatedSystemFd(fd);

    int ret = VfsFcntl(fd, cmd, arg);
    if (ret == CONTINE_NUTTX_FCNTL) {
        ret = fcntl(sysfd, cmd, arg);
    }

    if (ret < 0) {
        return -get_errno();
    }
    return ret;
}

#ifdef LOSCFG_KERNEL_PIPE
/**
 * @brief 
 * @verbatim 
    管道是一种最基本的IPC机制，作用于有血缘关系的进程之间，完成数据传递。
    调用pipe系统函数即可创建一个管道。有如下特质：

    1. 其本质是一个伪文件(实为内核缓冲区)
    2. 由两个文件描述符引用，一个表示读端，一个表示写端。
    3. 规定数据从管道的写端流入管道，从读端流出。

    管道的原理: 管道实为内核使用环形队列机制，借助内核缓冲区(4k)实现。
    管道的局限性：
        ① 数据自己读不能自己写。
        ② 数据一旦被读走，便不在管道中存在，不可反复读取。
        ③ 由于管道采用半双工通信方式。因此，数据只能在一个方向上流动。
        ④ 只能在有公共祖先的进程间使用管道。
    常见的通信方式有，单工通信、半双工通信、全双工通信。
 * @endverbatim 
 * @param pipefd 
 * @return int 
 */
int SysPipe(int pipefd[2]) /* 2 : pipe fds for read and write */
{
    int ret;
    int pipeFdIntr[2] = {0}; /* 2 : pipe fds for read and write */

    int procFd0 = AllocProcessFd();//读端管道,像 stdin对应标准输入
    if (procFd0 < 0) {
        return -EMFILE;
    }
    int procFd1 = AllocProcessFd();//写端管道,像 stdout对应标准输出
    if (procFd1 < 0) {
        FreeProcessFd(procFd0);
        return -EMFILE;
    }

    ret = pipe(pipeFdIntr);//创建管道
    if (ret < 0) {
        FreeProcessFd(procFd0);
        FreeProcessFd(procFd1);
        return -get_errno();
    }
    int sysPipeFd0 = pipeFdIntr[0];
    int sysPipeFd1 = pipeFdIntr[1];

    AssociateSystemFd(procFd0, sysPipeFd0);//进程FD和系统FD绑定
    AssociateSystemFd(procFd1, sysPipeFd1);

    pipeFdIntr[0] = procFd0;
    pipeFdIntr[1] = procFd1;

    ret = LOS_ArchCopyToUser(pipefd, pipeFdIntr, sizeof(pipeFdIntr));//参数都走两个进程FD
    if (ret != 0) {
        FreeProcessFd(procFd0);
        FreeProcessFd(procFd1);
        close(sysPipeFd0);
        close(sysPipeFd1);
        return -EFAULT;
    }
    return ret;
}
#endif

/// 复制文件描述符
int SysDup2(int fd1, int fd2)
{
    int ret;
    int sysfd1 = GetAssociatedSystemFd(fd1);
    int sysfd2 = GetAssociatedSystemFd(fd2);
	//检查参数是否有效，注意：socket fd不支持dup2
    /* Check if the param is valid, note that: socket fd is not support dup2 */
    if ((sysfd1 < 0) || (sysfd1 >= CONFIG_NFILE_DESCRIPTORS) || (CheckProcessFd(fd2) != OK)) {//socket的fd必大于CONFIG_NFILE_DESCRIPTORS
        return -EBADF;
    }

    /* Handle special circumstances */
    if (fd1 == fd2) {
        return fd2;
    }

    ret = AllocSpecifiedProcessFd(fd2);
    if (ret != OK) {
        return ret;
    }

    /* close the sysfd2 in need */
    if (sysfd2 >= 0) {
        ret = close(sysfd2);
        if (ret < 0) {
            AssociateSystemFd(fd2, sysfd2);
            return -get_errno();
        }
    }

    files_refer(sysfd1);
    AssociateSystemFd(fd2, sysfd1);

    /* if fd1 is not equal to fd2, the FD_CLOEXEC flag associated with fd2 shall be cleared */
    ClearCloexecFlag(fd2);
    return fd2;
}
///select()参数检查
static int SelectParamCheckCopy(fd_set *readfds, fd_set *writefds, fd_set *exceptfds, fd_set **fdsBuf)
{
    fd_set *readfdsRet = NULL;
    fd_set *writefdsRet = NULL;
    fd_set *exceptfdsRet = NULL;

    *fdsBuf = (fd_set *)LOS_MemAlloc(OS_SYS_MEM_ADDR, sizeof(fd_set) * 3); /* 3: three param need check and copy */
    if (*fdsBuf == NULL) {
        return -ENOMEM;
    }

    readfdsRet = *fdsBuf;        /* LOS_MemAlloc 3 sizeof(fd_set) space,first use for readfds */
    writefdsRet = *fdsBuf + 1;   /* 1: LOS_MemAlloc 3 sizeof(fd_set) space,second use for writefds */
    exceptfdsRet = *fdsBuf + 2;  /* 2: LOS_MemAlloc 3 sizeof(fd_set) space,thired use for exceptfds */

    if (readfds != NULL) {
        if (LOS_ArchCopyFromUser(readfdsRet, readfds, sizeof(fd_set)) != 0) {
            (void)LOS_MemFree(OS_SYS_MEM_ADDR, *fdsBuf);
            return -EFAULT;
        }
    }

    if (writefds != NULL) {
        if (LOS_ArchCopyFromUser(writefdsRet, writefds, sizeof(fd_set)) != 0) {
            (void)LOS_MemFree(OS_SYS_MEM_ADDR, *fdsBuf);
            return -EFAULT;
        }
    }

    if (exceptfds != NULL) {
        if (LOS_ArchCopyFromUser(exceptfdsRet, exceptfds, sizeof(fd_set)) != 0) {
            (void)LOS_MemFree(OS_SYS_MEM_ADDR, *fdsBuf);
            return -EFAULT;
        }
    }

    return 0;
}

/*!
 * @brief SysSelect	系统调用|文件系统|select .鸿蒙liteos目前也支持epoll方式
 *
 * @param exceptfds	文件集将监视文件集中的任何文件是否发生错误，可用于其他的用途，
 *					例如，监视带外数据OOB，带外数据使用MSG_OOB标志发送到套接字上。当select函数返回的时候，
 *					exceptfds将清除其中的其他文件描述符，只留下标记有OOB数据的文件描述符。
 * @param nfds	select监视的文件句柄数，一般设为要监视各文件中的最大文件描述符值加1。
 * @param readfds 文件描述符集合监视文件集中的任何文件是否有数据可读，当select函数返回的时候，
 *					readfds将清除其中不可读的文件描述符，只留下可读的文件描述符。	
 * @param timeout	参数是一个指向 struct timeval 类型的指针，它可以使 select()在等待 timeout 时间后
 *					若没有文件描述符准备好则返回。其timeval结构用于指定这段时间的秒数和微秒数。它可以使select处于三种状态：
 *		(1) 若将NULL以形参传入，即不传入时间结构，就是将select置于阻塞状态，一定等到监视文件描述符集合中某个文件描述符发生变化为止；
 *		(2) 若将时间值设为0秒0毫秒，就变成一个纯粹的非阻塞函数，不管文件描述符是否有变化，都立刻返回继续执行，文件无变化返回0，有变化返回一个正值； 
 *		(3) timeout的值大于0，这就是等待的超时时间，即select在timeout时间内阻塞，超时时间之内有事件到来就返回了，否则在超时后不管怎样一定返回，返回值同上述。
 * @param writefds	文件描述符集合监视文件集中的任何文件是否有数据可写，当select函数返回的时候，
 *					writefds将清除其中不可写的文件描述符，只留下可写的文件描述符。
 * @return	
 *
 * @see
 */
int SysSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
    int ret;
    fd_set *fdsRet = NULL;
    fd_set *readfdsRet = NULL;
    fd_set *writefdsRet = NULL;
    fd_set *exceptfdsRet = NULL;
    struct timeval timeoutRet = {0};

    ret = SelectParamCheckCopy(readfds, writefds, exceptfds, &fdsRet);//检查参数
    if (ret != 0) {
        return ret;
    }

    readfdsRet = fdsRet;        /* LOS_MemAlloc 3 sizeof(fd_set) space,first use for readfds */
    writefdsRet = fdsRet + 1;   /* 1: LOS_MemAlloc 3 sizeof(fd_set) space,second use for writefds */
    exceptfdsRet = fdsRet + 2;  /* 2: LOS_MemAlloc 3 sizeof(fd_set) space,thired use for exceptfds */

    if (timeout != NULL) {
        if (LOS_ArchCopyFromUser(&timeoutRet, timeout, sizeof(struct timeval)) != 0) {
            goto ERROUT;
        }
    }
	//poll()是在NuttX下执行此类监视操作的基本API
    ret = do_select(nfds, (readfds ? readfdsRet : NULL), (writefds ? writefdsRet : NULL),
                 (exceptfds ? exceptfdsRet : NULL), (timeout ? (&timeoutRet) : NULL), UserPoll);
    if (ret < 0) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, fdsRet);
        return -get_errno();
    }

    if (readfds != NULL) {
        if (LOS_ArchCopyToUser(readfds, readfdsRet, sizeof(fd_set)) != 0) {
            goto ERROUT;
        }
    }

    if (writefds != NULL) {
        if (LOS_ArchCopyToUser(writefds, writefdsRet, sizeof(fd_set)) != 0) {
            goto ERROUT;
        }
    }

    if (exceptfds != 0) {
        if (LOS_ArchCopyToUser(exceptfds, exceptfdsRet, sizeof(fd_set)) != 0) {
            goto ERROUT;
        }
    }

    (void)LOS_MemFree(OS_SYS_MEM_ADDR, fdsRet);
    return ret;

ERROUT:
    (void)LOS_MemFree(OS_SYS_MEM_ADDR, fdsRet);
    return -EFAULT;
}
///系统调用|文件系统|截断功能
int SysTruncate(const char *path, off_t length)
{
    int ret;
    int fd = -1;
    char *pathRet = NULL;

    if (path != NULL) {
        ret = UserPathCopy(path, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    fd = open((path ? pathRet : NULL), O_RDWR);
    if (fd < 0) {
        /* The errno value has already been set */
        ret = -get_errno();
        goto OUT;
    }

    ret = ftruncate(fd, length);
    close(fd);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;
}
///系统调用|文件系统|截断功能
int SysTruncate64(const char *path, off64_t length)
{
    int ret;
    int fd = -1;
    char *pathRet = NULL;

    if (path != NULL) {
        ret = UserPathCopy(path, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    fd = open((path ? pathRet : NULL), O_RDWR);
    if (fd < 0) {
        /* The errno value has already been set */
        ret = -get_errno();
        goto OUT;
    }

    ret = ftruncate64(fd, length);
    close(fd);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;
}
///系统调用|文件系统|截断功能
int SysFtruncate(int fd, off_t length)
{
    int ret;

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);

    ret = ftruncate(fd, length);
    if (ret < 0) {
        return -get_errno();
    }
    return ret;
}
///获取指定路径下文件的文件系统信息
int SysStatfs(const char *path, struct statfs *buf)
{
    int ret;
    char *pathRet = NULL;
    struct statfs bufRet = {0};

    if (path != NULL) {
        ret = UserPathCopy(path, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    ret = statfs((path ? pathRet : NULL), (buf ? (&bufRet) : NULL));
    if (ret < 0) {
        ret = -get_errno();
        goto OUT;
    }

    ret = LOS_ArchCopyToUser(buf, &bufRet, sizeof(struct statfs));
    if (ret != 0) {
        ret = -EFAULT;
    }

OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;
}
///获取文件系统信息
int SysStatfs64(const char *path, size_t sz, struct statfs *buf)
{
    int ret;
    char *pathRet = NULL;
    struct statfs bufRet = {0};

    if (path != NULL) {
        ret = UserPathCopy(path, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    if (sz != sizeof(*buf)) {
        ret = -EINVAL;
        goto OUT;
    }

    ret = statfs((path ? pathRet : NULL), (buf ? (&bufRet) : NULL));
    if (ret < 0) {
        ret = -get_errno();
        goto OUT;
    }

    ret = LOS_ArchCopyToUser(buf, &bufRet, sizeof(struct statfs));
    if (ret != 0) {
        ret = -EFAULT;
    }

OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;
}
///获取文件状态信息
int SysStat(const char *path, struct kstat *buf)
{
    int ret;
    char *pathRet = NULL;
    struct stat bufRet = {0};

    if (path != NULL) {
        ret = UserPathCopy(path, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    ret = stat((path ? pathRet : NULL), (buf ? (&bufRet) : NULL));
    if (ret < 0) {
        ret = -get_errno();
        goto OUT;
    }

    ret = LOS_ArchCopyToUser(buf, &bufRet, sizeof(struct kstat));
    if (ret != 0) {
        ret = -EFAULT;
    }

OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;
}
///参见SysStat
int SysLstat(const char *path, struct kstat *buffer)
{
    int ret;
    char *pathRet = NULL;
    struct stat bufRet = {0};

    if (path != NULL) {
        ret = UserPathCopy(path, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    ret = stat((path ? pathRet : NULL), (buffer ? (&bufRet) : NULL));
    if (ret < 0) {
        ret = -get_errno();
        goto OUT;
    }

    ret = LOS_ArchCopyToUser(buffer, &bufRet, sizeof(struct kstat));
    if (ret != 0) {
        ret = -EFAULT;
    }

OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;
}
///参见SysStat
int SysFstat(int fd, struct kstat *buf)
{
    int ret;
    struct stat bufRet = {0};
    struct file *filep = NULL;

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);

    ret = fs_getfilep(fd, &filep);
    if (ret < 0) {
        return -get_errno();
    }

    if (filep->f_oflags & O_DIRECTORY) {
        return -EBADF;
    }

    ret = stat(filep->f_path, (buf ? (&bufRet) : NULL));
    if (ret < 0) {
        return -get_errno();
    }

    ret = LOS_ArchCopyToUser(buf, &bufRet, sizeof(struct kstat));
    if (ret != 0) {
        return -EFAULT;
    }

    return ret;
}

int SysStatx(int fd, const char *restrict path, int flag, unsigned mask, struct statx *restrict stx)
{
    return -ENOSYS;
}
///把文件在内存中的部分写回磁盘
int SysFsync(int fd)
{
    int ret;
    struct file *filep = NULL;

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);

    /* Get the file structure corresponding to the file descriptor. */
    ret = fs_getfilep(fd, &filep);
    if (ret < 0) {
        /* The errno value has already been set */
        return -get_errno();
    }

    if (filep->f_oflags & O_DIRECTORY) {
        return -EBADF;
    }

    /* Perform the fsync operation */
    ret = file_fsync(filep);
    if (ret < 0) {
        return -get_errno();
    }
    return ret;
}
///通过FD读入数据到缓冲数组中,fd为进程描述符
ssize_t SysReadv(int fd, const struct iovec *iov, int iovcnt)
{
    int ret;
    int valid_iovcnt = -1;
    struct iovec *iovRet = NULL;

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);//进程FD转成系统FD
    if ((iov == NULL) || (iovcnt < 0) || (iovcnt > IOV_MAX)) {
        return -EINVAL;
    }

    if (iovcnt == 0) {
        return 0;
    }

    ret = UserIovCopy(&iovRet, iov, iovcnt, &valid_iovcnt);
    if (ret != 0) {
        return ret;
    }

    if (valid_iovcnt <= 0) {
        ret = -EFAULT;
        goto OUT;
    }

    ret = vfs_readv(fd, iovRet, valid_iovcnt, NULL);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    (void)LOS_MemFree(OS_SYS_MEM_ADDR, iovRet);
    return ret;
}
///将缓冲数组里的数据写入文件
ssize_t SysWritev(int fd, const struct iovec *iov, int iovcnt)
{
    int ret;
    int valid_iovcnt = -1;
    struct iovec *iovRet = NULL;

    /* Process fd convert to system global fd */
    int sysfd = GetAssociatedSystemFd(fd);
    if ((iovcnt < 0) || (iovcnt > IOV_MAX)) {
        return -EINVAL;
    }

    if (iovcnt == 0) {
        return 0;
    }

    if (iov == NULL) {
        return -EFAULT;
    }

    ret = UserIovCopy(&iovRet, iov, iovcnt, &valid_iovcnt);
    if (ret != 0) {
        return ret;
    }

    if (valid_iovcnt != iovcnt) {
        ret = -EFAULT;
        goto OUT_FREE;
    }

    ret = writev(sysfd, iovRet, valid_iovcnt);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT_FREE:
    (void)LOS_MemFree(OS_SYS_MEM_ADDR, iovRet);
    return ret;
}

/*!
 * @brief SysPoll I/O多路转换	
 *
 * @param fds	fds是一个struct pollfd类型的数组，用于存放需要检测其状态的socket描述符，并且调用poll函数之后fds数组不会被清空；
 *				一个pollfd结构体表示一个被监视的文件描述符，通过传递fds指示 poll() 监视多个文件描述符。
 * @param nfds	记录数组fds中描述符的总数量。
 * @param timeout	指定等待的毫秒数，无论 I/O 是否准备好，poll() 都会返回，和select函数是类似的。
 * @return	函数返回fds集合中就绪的读、写，或出错的描述符数量，返回0表示超时，返回-1表示出错；
 * 		poll改变了文件描述符集合的描述方式，使用了pollfd结构而不是select的fd_set结构，使得poll支持的文件描述符集合限制远大于select的1024。
 * 		这也是和select不同的地方。
 * @see
 */
int SysPoll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    int ret;
    struct pollfd *kfds = NULL;

    if ((nfds >= MAX_POLL_NFDS) || (nfds == 0) || (fds == NULL)) {
        return -EINVAL;
    }

    kfds = (struct pollfd *)malloc(sizeof(struct pollfd) * nfds);
    if (kfds != NULL) {
        if (LOS_ArchCopyFromUser(kfds, fds, sizeof(struct pollfd) * nfds) != 0) {
            ret = -EFAULT;
            goto OUT_KFD;
        }
    }

    int *pollFds = NULL;
    ret = PollfdToSystem(kfds, nfds, &pollFds);
    if (ret < 0) {
        ret = -get_errno();
        goto OUT_KFD;
    }

    ret = poll(kfds, nfds, timeout);
    if (ret < 0) {
        ret = -get_errno();
        goto OUT;
    }

    if (kfds != NULL) {
        RestorePollfd(kfds, nfds, pollFds);
        if (LOS_ArchCopyToUser(fds, kfds, sizeof(struct pollfd) * nfds) != 0) {
            ret = -EFAULT;
            goto OUT;
        }
    }

OUT:
    free(pollFds);
OUT_KFD:
    free(kfds);
    return ret;
}
///对进程进行特定操作
int SysPrctl(int option, ...)
{
    unsigned long name;
    va_list ap;
    errno_t err;

    va_start(ap, option);
    if (option != PR_SET_NAME) {
        PRINT_ERR("%s: %d, no support option : 0x%x\n", __FUNCTION__, __LINE__, option);
        err = EOPNOTSUPP;
        goto ERROR;
    }

    name = va_arg(ap, unsigned long);
    if (!LOS_IsUserAddress(name)) {
        err = EFAULT;
        goto ERROR;
    }

    err = OsSetTaskName(OsCurrTaskGet(), (const char *)(uintptr_t)name, TRUE);
    if (err != LOS_OK) {
        goto ERROR;
    }

    va_end(ap);
    return ENOERR;

ERROR:
    va_end(ap);
    return -err;
}
///对进程进行特定操作
ssize_t SysPread64(int fd, void *buf, size_t nbytes, off64_t offset)
{
    int ret, retVal;
    char *bufRet = NULL;

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);

    if (nbytes == 0) {
        ret = pread64(fd, buf, nbytes, offset);
        if (ret < 0) {
            return -get_errno();
        } else {
            return ret;
        }
    }

    bufRet = (char *)LOS_MemAlloc(OS_SYS_MEM_ADDR, nbytes);
    if (bufRet == NULL) {
        return -ENOMEM;
    }
    (void)memset_s(bufRet, nbytes, 0, nbytes);
    ret = pread64(fd, (buf ? bufRet : NULL), nbytes, offset);
    if (ret < 0) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, bufRet);
        return -get_errno();
    }

    retVal = LOS_ArchCopyToUser(buf, bufRet, ret);
    if (retVal != 0) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, bufRet);
        return -EFAULT;
    }

    (void)LOS_MemFree(OS_SYS_MEM_ADDR, bufRet);
    return ret;
}

ssize_t SysPwrite64(int fd, const void *buf, size_t nbytes, off64_t offset)
{
    int ret;
    char *bufRet = NULL;

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);

    if (nbytes == 0) {
        ret = pwrite64(fd, buf, nbytes, offset);
        if (ret < 0) {
            return -get_errno();
        }
        return ret;
    }

    bufRet = (char *)LOS_MemAlloc(OS_SYS_MEM_ADDR, nbytes);
    if (bufRet == NULL) {
        return -ENOMEM;
    }

    if (buf != NULL) {
        ret = LOS_ArchCopyFromUser(bufRet, buf, nbytes);
        if (ret != 0) {
            (void)LOS_MemFree(OS_SYS_MEM_ADDR, bufRet);
            return -EFAULT;
        }
    }

    ret = pwrite64(fd, (buf ? bufRet : NULL), nbytes, offset);
    if (ret < 0) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, bufRet);
        return -get_errno();
    }

    (void)LOS_MemFree(OS_SYS_MEM_ADDR, bufRet);
    return ret;
}

char *SysGetcwd(char *buf, size_t n)
{
    char *ret = NULL;
    char *bufRet = NULL;
    size_t bufLen = n;
    int retVal;

    if (bufLen > PATH_MAX) {
        bufLen = PATH_MAX;
    }

    bufRet = (char *)LOS_MemAlloc(OS_SYS_MEM_ADDR, bufLen);
    if (bufRet == NULL) {
        return (char *)(intptr_t)-ENOMEM;
    }
    (void)memset_s(bufRet, bufLen, 0, bufLen);

    ret = getcwd((buf ? bufRet : NULL), bufLen);
    if (ret == NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, bufRet);
        return (char *)(intptr_t)-get_errno();
    }

    retVal = LOS_ArchCopyToUser(buf, bufRet, bufLen);
    if (retVal != 0) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, bufRet);
        return (char *)(intptr_t)-EFAULT;
    }
    ret = buf;

    (void)LOS_MemFree(OS_SYS_MEM_ADDR, bufRet);
    return ret;
}

ssize_t SysSendFile(int outfd, int infd, off_t *offset, size_t count)
{
    int ret, retVal;
    off_t offsetRet;

    retVal = LOS_ArchCopyFromUser(&offsetRet, offset, sizeof(off_t));
    if (retVal != 0) {
        return -EFAULT;
    }

    /* Process fd convert to system global fd */
    outfd = GetAssociatedSystemFd(outfd);
    infd = GetAssociatedSystemFd(infd);

    ret = sendfile(outfd, infd, (offset ? (&offsetRet) : NULL), count);
    if (ret < 0) {
        return -get_errno();
    }

    retVal = LOS_ArchCopyToUser(offset, &offsetRet, sizeof(off_t));
    if (retVal != 0) {
        return -EFAULT;
    }

    return ret;
}

int SysFtruncate64(int fd, off64_t length)
{
    int ret;

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);

    ret = ftruncate64(fd, length);
    if (ret < 0) {
        return -get_errno();
    }
    return ret;
}

int SysOpenat(int dirfd, const char *path, int oflags, ...)
{
    int ret;
    int procFd;
    char *pathRet = NULL;
    mode_t mode;
#ifdef LOSCFG_FILE_MODE
    va_list ap;

    va_start(ap, oflags);
    mode = va_arg(ap, int);
    va_end(ap);
#else
    mode = 0666; /* 0666: File read-write properties. */
#endif

    if (path != NULL) {
        ret = UserPathCopy(path, &pathRet);
        if (ret != 0) {
            return ret;
        }
    }

    procFd = AllocProcessFd();
    if (procFd < 0) {
        ret = -EMFILE;
        goto ERROUT;
    }

    if (oflags & O_CLOEXEC) {
        SetCloexecFlag(procFd);
    }

    if (dirfd != AT_FDCWD) {
        /* Process fd convert to system global fd */
        dirfd = GetAssociatedSystemFd(dirfd);
    }

    ret = do_open(dirfd, (path ? pathRet : NULL), oflags, mode);
    if (ret < 0) {
        ret = -get_errno();
        goto ERROUT;
    }

    AssociateSystemFd(procFd, ret);
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return procFd;

ERROUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    if (procFd >= 0) {
        FreeProcessFd(procFd);
    }
    return ret;
}

int SysMkdirat(int dirfd, const char *pathname, mode_t mode)
{
    int ret;
    char *pathRet = NULL;

    if (pathname != NULL) {
        ret = UserPathCopy(pathname, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    if (dirfd != AT_FDCWD) {
        /* Process fd convert to system global fd */
        dirfd = GetAssociatedSystemFd(dirfd);
    }

    ret = do_mkdir(dirfd, (pathname ? pathRet : NULL), mode);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;
}

int SysLinkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags)
{
    int ret;
    char *oldpathRet = NULL;
    char *newpathRet = NULL;

    if (oldpath != NULL) {
        ret = UserPathCopy(oldpath, &oldpathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    if (newpath != NULL) {
        ret = UserPathCopy(newpath, &newpathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    if (olddirfd != AT_FDCWD) {
        /* Process fd convert to system global fd */
        olddirfd = GetAssociatedSystemFd(olddirfd);
    }

    if (newdirfd != AT_FDCWD) {
        /* Process fd convert to system global fd */
        newdirfd = GetAssociatedSystemFd(newdirfd);
    }

    ret = linkat(olddirfd, oldpathRet, newdirfd, newpathRet, flags);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    if (oldpathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, oldpathRet);
    }
    if (newpathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, newpathRet);
    }
    return ret;
}

int SysSymlinkat(const char *target, int dirfd, const char *linkpath)
{
    int ret;
    char *pathRet = NULL;
    char *targetRet = NULL;

    if (target != NULL) {
        ret = UserPathCopy(target, &targetRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    if (linkpath != NULL) {
        ret = UserPathCopy(linkpath, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    if (dirfd != AT_FDCWD) {
        /* Process fd convert to system global fd */
        dirfd = GetAssociatedSystemFd(dirfd);
    }

    ret = symlinkat(targetRet, dirfd, pathRet);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }

    if (targetRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, targetRet);
    }
    return ret;
}

ssize_t SysReadlinkat(int dirfd, const char *pathname, char *buf, size_t bufsize)
{
    ssize_t ret;
    char *pathRet = NULL;

    if (bufsize == 0) {
        return -EINVAL;
    }

    if (pathname != NULL) {
        ret = UserPathCopy(pathname, &pathRet);
        if (ret != 0) {
            goto OUT;
        }

#ifdef LOSCFG_PID_CONTAINER
#ifdef LOSCFG_PROC_PROCESS_DIR
        ret = ProcRealProcessDirGet(pathRet);
        if (ret != 0) {
            goto OUT;
        }
#endif
#endif
    }

    if (dirfd != AT_FDCWD) {
        /* Process fd convert to system global fd */
        dirfd = GetAssociatedSystemFd(dirfd);
    }

    if (!LOS_IsUserAddressRange((vaddr_t)(UINTPTR)buf, bufsize)) {
        ret = -EFAULT;
        goto OUT;
    }

    ret = readlinkat(dirfd, pathRet, buf, bufsize);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;
}

int SysUnlinkat(int dirfd, const char *pathname, int flag)
{
    int ret;
    char *pathRet = NULL;

    if (pathname != NULL) {
        ret = UserPathCopy(pathname, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    if (dirfd != AT_FDCWD) {
        /* Process fd convert to system global fd */
        dirfd = GetAssociatedSystemFd(dirfd);
    }

    ret = unlinkat(dirfd, (pathname ? pathRet : NULL), flag);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;
}

int SysRenameat(int oldfd, const char *oldpath, int newdfd, const char *newpath)
{
    int ret;
    char *pathOldRet = NULL;
    char *pathNewRet = NULL;

    if (oldpath != NULL) {
        ret = UserPathCopy(oldpath, &pathOldRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    if (newpath != NULL) {
        ret = UserPathCopy(newpath, &pathNewRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    if (oldfd != AT_FDCWD) {
        /* Process fd convert to system global fd */
        oldfd = GetAssociatedSystemFd(oldfd);
    }
    if (newdfd != AT_FDCWD) {
        /* Process fd convert to system global fd */
        newdfd = GetAssociatedSystemFd(newdfd);
    }

    ret = do_rename(oldfd, (oldpath ? pathOldRet : NULL), newdfd, (newpath ? pathNewRet : NULL));
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    if (pathOldRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathOldRet);
    }
    if (pathNewRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathNewRet);
    }
    return ret;
}

int SysFallocate(int fd, int mode, off_t offset, off_t len)
{
    int ret;

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);

    ret = fallocate(fd, mode, offset, len);
    if (ret < 0) {
        return -get_errno();
    }
    return ret;
}

int SysFallocate64(int fd, int mode, off64_t offset, off64_t len)
{
    int ret;

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);

    ret = fallocate64(fd, mode, offset, len);
    if (ret < 0) {
        return -get_errno();
    }
    return ret;
}

ssize_t SysPreadv(int fd, const struct iovec *iov, int iovcnt, long loffset, long hoffset)
{
    off_t offsetflag;
    offsetflag = (off_t)((unsigned long long)loffset | (((unsigned long long)hoffset) << HIGH_SHIFT_BIT));

    int ret;
    int valid_iovcnt = -1;
    struct iovec *iovRet = NULL;

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);
    if ((iov == NULL) || (iovcnt < 0) || (iovcnt > IOV_MAX)) {
        return -EINVAL;
    }

    if (iovcnt == 0) {
        return 0;
    }

    ret = UserIovCopy(&iovRet, iov, iovcnt, &valid_iovcnt);
    if (ret != 0) {
        return ret;
    }

    if (valid_iovcnt <= 0) {
        ret = -EFAULT;
        goto OUT_FREE;
    }

    ret = preadv(fd, iovRet, valid_iovcnt, offsetflag);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT_FREE:
    (void)(void)LOS_MemFree(OS_SYS_MEM_ADDR, iovRet);
    return ret;
}

ssize_t SysPwritev(int fd, const struct iovec *iov, int iovcnt, long loffset, long hoffset)
{
    off_t offsetflag;
    offsetflag = (off_t)((unsigned long long)loffset | (((unsigned long long)hoffset) << HIGH_SHIFT_BIT));
    int ret;
    int valid_iovcnt = -1;
    struct iovec *iovRet = NULL;

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);
    if ((iov == NULL) || (iovcnt < 0) || (iovcnt > IOV_MAX)) {
        return -EINVAL;
    }

    if (iovcnt == 0) {
        return 0;
    }

    ret = UserIovCopy(&iovRet, iov, iovcnt, &valid_iovcnt);
    if (ret != 0) {
        return ret;
    }

    if (valid_iovcnt != iovcnt) {
        ret = -EFAULT;
        goto OUT_FREE;
    }

    ret = pwritev(fd, iovRet, valid_iovcnt, offsetflag);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT_FREE:
    (void)LOS_MemFree(OS_SYS_MEM_ADDR, iovRet);
    return ret;
}

#ifdef LOSCFG_FS_FAT
int SysFormat(const char *dev, int sectors, int option)
{
    int ret;
    char *devRet = NULL;

    if (!IsCapPermit(CAP_FS_FORMAT)) {
        return -EPERM;
    }

    if (dev != NULL) {
        ret = UserPathCopy(dev, &devRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    ret = format((dev ? devRet : NULL), sectors, option);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    if (devRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, devRet);
    }
    return ret;
}
#endif

int SysFstat64(int fd, struct kstat *buf)
{
    int ret;
    struct stat64 bufRet = {0};

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);

    ret = fstat64(fd, (buf ? (&bufRet) : NULL));
    if (ret < 0) {
        return -get_errno();
    }

    ret = LOS_ArchCopyToUser(buf, &bufRet, sizeof(struct kstat));
    if (ret != 0) {
        return -EFAULT;
    }

    return ret;
}

int SysFcntl64(int fd, int cmd, void *arg)
{
    /* Process fd convert to system global fd */
    int sysfd = GetAssociatedSystemFd(fd);

    int ret = VfsFcntl(fd, cmd, arg);
    if (ret == CONTINE_NUTTX_FCNTL) {
        ret = fcntl64(sysfd, cmd, arg);
    }

    if (ret < 0) {
        return -get_errno();
    }
    return ret;
}

int SysGetdents64(int fd, struct dirent *de_user, unsigned int count)
{
    if (!LOS_IsUserAddressRange((VADDR_T)(UINTPTR)de_user, count)) {
        return -EFAULT;
    }

    struct dirent *de_knl = NULL;

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);

    int ret = do_readdir(fd, &de_knl, count);
    if (ret < 0) {
        return ret;
    }
    if (de_knl != NULL) {
        int cpy_ret = LOS_ArchCopyToUser(de_user, de_knl, ret);
        if (cpy_ret != 0)
        {
            return -EFAULT;
        }
    }
    return ret;
}

char *SysRealpath(const char *path, char *resolved_path)
{
    char *pathRet = NULL;
    char *resolved_pathRet = NULL;
    char *result = NULL;
    int ret;

    if (resolved_path == NULL) {
        return (char *)(intptr_t)-EINVAL;
    }

    if (path != NULL) {
        ret = UserPathCopy(path, &pathRet);
        if (ret != 0) {
            result = (char *)(intptr_t)ret;
            goto OUT;
        }
    }

    resolved_pathRet = realpath((path ? pathRet : NULL), NULL);
    if (resolved_pathRet == NULL) {
        result = (char *)(intptr_t)-get_errno();
        goto OUT;
    }

    ret = LOS_ArchCopyToUser(resolved_path, resolved_pathRet, strlen(resolved_pathRet) + 1);
    if (ret != 0) {
        result = (char *)(intptr_t)-EFAULT;
        goto OUT;
    }
    result = resolved_path;

OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    if (resolved_pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, resolved_pathRet);
    }
    return result;
}

int SysUtimensat(int fd, const char *path, struct timespec times[TIMESPEC_TIMES_NUM], int flag)
{
    int ret;
    int timeLen;
    struct IATTR attr = {0};
    char *filePath = NULL;

    timeLen = TIMESPEC_TIMES_NUM * sizeof(struct timespec);
    CHECK_ASPACE(times, timeLen);
    DUP_FROM_USER(times, timeLen);
    ret = CheckNewAttrTime(&attr, times);
    FREE_DUP(times);
    if (ret < 0) {
        goto OUT;
    }

    ret = GetFullpathNull(fd, path, &filePath);
    if (ret < 0) {
        goto OUT;
    }

    ret = chattr(filePath, &attr);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    PointerFree(filePath);
    return ret;
}

int SysChmod(const char *pathname, mode_t mode)
{
    int ret;
    char *pathRet = NULL;

    if (pathname != NULL) {
        ret = UserPathCopy(pathname, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    ret = chmod(pathRet, mode);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;
}

int SysFchmodat(int fd, const char *path, mode_t mode, int flag)
{
    int ret;
    char *pathRet = NULL;
    char *fullpath = NULL;
    struct IATTR attr = {
        .attr_chg_mode = mode,
        .attr_chg_valid = CHG_MODE,
    };

    if (path != NULL) {
        ret = UserPathCopy(path, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    if (fd != AT_FDCWD) {
        /* Process fd convert to system global fd */
        fd = GetAssociatedSystemFd(fd);
    }

    ret = vfs_normalize_pathat(fd, pathRet, &fullpath);
    if (ret < 0) {
        goto OUT;
    }

    ret = chattr(fullpath, &attr);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    PointerFree(pathRet);
    PointerFree(fullpath);

    return ret;
}

int SysFchmod(int fd, mode_t mode)
{
    int ret;
    int sysFd;
    struct IATTR attr = {
        .attr_chg_mode = mode,
        .attr_chg_valid = CHG_MODE, /* change mode */
    };
    struct file *file = NULL;

    sysFd = GetAssociatedSystemFd(fd);
    if (sysFd < 0) {
        return -EBADF;
    }

    ret = fs_getfilep(sysFd, &file);
    if (ret < 0) {
        return -get_errno();
    }

    ret = chattr(file->f_path, &attr);
    if (ret < 0) {
        return -get_errno();
    }

    return ret;
}

int SysFchownat(int fd, const char *path, uid_t owner, gid_t group, int flag)
{
    int ret;
    char *fullpath = NULL;
    struct IATTR attr = {
        .attr_chg_valid = 0,
    };

    ret = GetFullpath(fd, path, &fullpath);
    if (ret < 0) {
        goto OUT;
    }

    if (owner != (uid_t)-1) {
        attr.attr_chg_uid = owner;
        attr.attr_chg_valid |= CHG_UID;
    }
    if (group != (gid_t)-1) {
        attr.attr_chg_gid = group;
        attr.attr_chg_valid |= CHG_GID;
    }

    ret = chattr(fullpath, &attr);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    PointerFree(fullpath);

    return ret;
}

int SysFchown(int fd, uid_t owner, gid_t group)
{
    int ret;
    int sysFd;
    struct IATTR attr = {0};
    attr.attr_chg_valid = 0;
    struct file *file = NULL;

    sysFd = GetAssociatedSystemFd(fd);
    if (sysFd < 0) {
        return -EBADF;
    }

    ret = fs_getfilep(sysFd, &file);
    if (ret < 0) {
        return -get_errno();
    }

    if (owner != (uid_t)-1) {
        attr.attr_chg_uid = owner;
        attr.attr_chg_valid |= CHG_UID;
    }
    if (group != (gid_t)-1) {
        attr.attr_chg_gid = group;
        attr.attr_chg_valid |= CHG_GID;
    }
    ret = chattr(file->f_path, &attr);
    if (ret < 0) {
        ret = -get_errno();
    }

    return ret;
}

int SysChown(const char *pathname, uid_t owner, gid_t group)
{
    int ret;
    char *pathRet = NULL;

    if (pathname != NULL) {
        ret = UserPathCopy(pathname, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    ret = chown(pathRet, owner, group);
    if (ret < 0) {
        ret = -get_errno();
    }

OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;
}

int SysFstatat64(int dirfd, const char *restrict path, struct kstat *restrict buf, int flag)
{
    int ret;
    struct stat bufRet = {0};
    char *pathRet = NULL;
    char *fullpath = NULL;

    if (path != NULL) {
        ret = UserPathCopy(path, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    if (dirfd != AT_FDCWD) {
        /* Process fd convert to system global fd */
        dirfd = GetAssociatedSystemFd(dirfd);
    }

    ret = vfs_normalize_pathat(dirfd, pathRet, &fullpath);
    if (ret < 0) {
        goto OUT;
    }

    ret = stat(fullpath, &bufRet);
    if (ret < 0) {
        ret = -get_errno();
        goto OUT;
    }

    ret = LOS_ArchCopyToUser(buf, &bufRet, sizeof(struct kstat));
    if (ret != 0) {
        ret = -EFAULT;
        goto OUT;
    }

OUT:
    if (pathRet != NULL) {
        LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }

    if (fullpath != NULL) {
        free(fullpath);
    }
    return ret;
}

int SysFaccessat(int fd, const char *filename, int amode, int flag)
{
    int ret;
    struct stat buf;
    struct statfs fsBuf;
    char *fullDirectory = NULL;

    ret = GetFullpath(fd, filename, &fullDirectory);
    if (ret < 0) {
        goto OUT;
    }

    ret = statfs(fullDirectory, &fsBuf);
    if (ret != 0) {
        ret = -get_errno();
        goto OUT;
    }

    if ((fsBuf.f_flags & MS_RDONLY) && ((unsigned int)amode & W_OK)) {
        ret = -EROFS;
        goto OUT;
    }

    ret = stat(fullDirectory, &buf);
    if (ret != 0) {
        ret = -get_errno();
        goto OUT;
    }

    if (VfsPermissionCheck(buf.st_uid, buf.st_gid, buf.st_mode, amode)) {
        ret = -EACCES;
    }

OUT:
    PointerFree(fullDirectory);

    return ret;
}

int SysFstatfs(int fd, struct statfs *buf)
{
    int ret;
    struct file *filep = NULL;
    struct statfs bufRet = {0};

    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);

    ret = fs_getfilep(fd, &filep);
    if (ret < 0) {
        ret = -get_errno();
        return ret;
    }

    ret = statfs(filep->f_path, &bufRet);
    if (ret < 0) {
        ret = -get_errno();
        return ret;
    }

    ret = LOS_ArchCopyToUser(buf, &bufRet, sizeof(struct statfs));
    if (ret != 0) {
        ret = -EFAULT;
    }

    return ret;
}

int SysFstatfs64(int fd, size_t sz, struct statfs *buf)
{
    int ret;

    if (sz != sizeof(struct statfs)) {
        ret = -EINVAL;
        return ret;
    }

    ret = SysFstatfs(fd, buf);

    return ret;
}

int SysPpoll(struct pollfd *fds, nfds_t nfds, const struct timespec *tmo_p, const sigset_t *sigMask, int nsig)
{
    int timeout, retVal;
    sigset_t_l origMask = {0};
    sigset_t_l set = {0};

    CHECK_ASPACE(tmo_p, sizeof(struct timespec));
    CPY_FROM_USER(tmo_p);

    if (tmo_p != NULL) {
        timeout = tmo_p->tv_sec * OS_SYS_US_PER_MS + tmo_p->tv_nsec / OS_SYS_NS_PER_MS;
        if (timeout < 0) {
            return -EINVAL;
        }
    } else {
        timeout = -1;
    }

    if (sigMask != NULL) {
        retVal = LOS_ArchCopyFromUser(&set, sigMask, sizeof(sigset_t));
        if (retVal != 0) {
            return -EFAULT;
        }
        (VOID)OsSigprocMask(SIG_SETMASK, &set, &origMask);
    } else {
        (VOID)OsSigprocMask(SIG_SETMASK, NULL, &origMask);
    }

    retVal = SysPoll(fds, nfds, timeout);
    (VOID)OsSigprocMask(SIG_SETMASK, &origMask, NULL);

    return retVal;
}

int SysPselect6(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
    const struct timespec *timeout, const long data[2])
{
    int ret;
    int retVal;
    sigset_t_l origMask;
    sigset_t_l setl;

    CHECK_ASPACE(readfds, sizeof(fd_set));
    CHECK_ASPACE(writefds, sizeof(fd_set));
    CHECK_ASPACE(exceptfds, sizeof(fd_set));
    CHECK_ASPACE(timeout, sizeof(struct timeval));

    CPY_FROM_USER(readfds);
    CPY_FROM_USER(writefds);
    CPY_FROM_USER(exceptfds);
    DUP_FROM_USER(timeout, sizeof(struct timeval));

    if (timeout != NULL) {
        ((struct timeval *)timeout)->tv_usec = timeout->tv_nsec / 1000; /* 1000, convert ns to us */
    }

    if (data != NULL) {
        retVal = LOS_ArchCopyFromUser(&(setl.sig[0]), (int *)((UINTPTR)data[0]), sizeof(sigset_t));
        if (retVal != 0) {
            ret = -EFAULT;
            FREE_DUP(timeout);
            return ret;
        }
    }

    OsSigprocMask(SIG_SETMASK, &setl, &origMask);
    ret = do_select(nfds, readfds, writefds, exceptfds, (struct timeval *)timeout, UserPoll);
    if (ret < 0) {
        /* do not copy parameter back to user mode if do_select failed */
        ret = -get_errno();
        FREE_DUP(timeout);
        return ret;
    }
    OsSigprocMask(SIG_SETMASK, &origMask, NULL);

    CPY_TO_USER(readfds);
    CPY_TO_USER(writefds);
    CPY_TO_USER(exceptfds);
    FREE_DUP(timeout);

    return ret;
}

static int DoEpollCreate1(int flags)
{
    int ret;
    int procFd;

    ret = epoll_create1(flags);
    if (ret < 0) {
        ret = -get_errno();
        return ret;
    }

    procFd = AllocAndAssocProcessFd((INTPTR)(ret), MIN_START_FD);
    if (procFd == -1) {
        epoll_close(ret);
        return -EMFILE;
    }

    return procFd;
}

int SysEpollCreate(int size)
{
    (void)size;
    return DoEpollCreate1(0);
}

int SysEpollCreate1(int flags)
{
    return DoEpollCreate1(flags);
}

int SysEpollCtl(int epfd, int op, int fd, struct epoll_event *ev)
{
    int ret;

    CHECK_ASPACE(ev, sizeof(struct epoll_event));
    CPY_FROM_USER(ev);

    fd = GetAssociatedSystemFd(fd);
    epfd = GetAssociatedSystemFd(epfd);
    if ((fd < 0) || (epfd < 0)) {
        ret = -EBADF;
        goto OUT;
    }

    ret = epoll_ctl(epfd, op, fd, ev);
    if (ret < 0) {
        ret = -EBADF;
        goto OUT;
    }

    CPY_TO_USER(ev);
OUT:
    return (ret == -1) ? -get_errno() : ret;
}

int SysEpollWait(int epfd, struct epoll_event *evs, int maxevents, int timeout)
{
    int ret = 0;

    CHECK_ASPACE(evs, sizeof(struct epoll_event));
    CPY_FROM_USER(evs);

    epfd = GetAssociatedSystemFd(epfd);
    if  (epfd < 0) {
        ret = -EBADF;
        goto OUT;
    }

    ret = epoll_wait(epfd, evs, maxevents, timeout);
    if (ret < 0) {
        ret = -get_errno();
    }

    CPY_TO_USER(evs);
OUT:
    return (ret == -1) ? -get_errno() : ret;
}

int SysEpollPwait(int epfd, struct epoll_event *evs, int maxevents, int timeout, const sigset_t *mask)
{
    sigset_t_l origMask;
    sigset_t_l setl;
    int ret = 0;

    CHECK_ASPACE(mask, sizeof(sigset_t));

    if (mask != NULL) {
        ret = LOS_ArchCopyFromUser(&setl, mask, sizeof(sigset_t));
        if (ret != 0) {
            return -EFAULT;
        }
    }

    CHECK_ASPACE(evs, sizeof(struct epoll_event));
    CPY_FROM_USER(evs);

    epfd = GetAssociatedSystemFd(epfd);
    if (epfd < 0) {
        ret = -EBADF;
        goto OUT;
    }

    OsSigprocMask(SIG_SETMASK, &setl, &origMask);
    ret = epoll_wait(epfd, evs, maxevents, timeout);
    if (ret < 0) {
        ret = -get_errno();
    }

    OsSigprocMask(SIG_SETMASK, &origMask, NULL);

    CPY_TO_USER(evs);
OUT:
    return (ret == -1) ? -get_errno() : ret;
}

#ifdef LOSCFG_CHROOT
int SysChroot(const char *path)
{
    int ret;
    char *pathRet = NULL;

    if (path != NULL) {
        ret = UserPathCopy(path, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    ret = chroot(path ? pathRet : NULL);
    if (ret < 0) {
        ret = -get_errno();
    }
OUT:
    if (pathRet != NULL) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, pathRet);
    }
    return ret;
}
#endif
#endif
