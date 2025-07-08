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

#include "errno.h"
#include "stdlib.h"
#include "string.h"
#include "dirent.h"
#include "unistd.h"
#include "sys/select.h"
#include "sys/mount.h"
#include "sys/stat.h"
#include "sys/statfs.h"
#include "sys/prctl.h"
#include "fs/fd_table.h"
#include "fs/file.h"
#include "linux/spinlock.h"
#include "los_process_pri.h"
#include "los_task_pri.h"
#include "capability_api.h"
#include "vnode.h"
#define MAX_DIR_ENT 1024  // 最大目录项数量

/**
 * @brief 通过文件描述符获取文件状态
 * @param fd 文件描述符
 * @param buf 存储文件状态的结构体指针
 * @return 成功返回0，失败返回VFS_ERROR
 */
int fstat(int fd, struct stat *buf)
{
    struct file *filep = NULL;  // 文件结构体指针

    int ret = fs_getfilep(fd, &filep);  // 通过文件描述符获取文件结构体
    if (ret < 0) {  // 检查获取是否成功
        return VFS_ERROR;  // 获取失败，返回错误
    }

    return stat(filep->f_path, buf);  // 调用stat函数获取文件状态
}

/**
 * @brief 通过文件描述符获取64位文件状态
 * @param fd 文件描述符
 * @param buf 存储64位文件状态的结构体指针
 * @return 成功返回0，失败返回VFS_ERROR
 */
int fstat64(int fd, struct stat64 *buf)
{
    struct file *filep = NULL;  // 文件结构体指针

    int ret = fs_getfilep(fd, &filep);  // 通过文件描述符获取文件结构体
    if (ret < 0) {  // 检查获取是否成功
        return VFS_ERROR;  // 获取失败，返回错误
    }

    return stat64(filep->f_path, buf);  // 调用stat64函数获取64位文件状态
}

/**
 * @brief 获取符号链接文件状态（此处实现与stat相同）
 * @param path 文件路径
 * @param buffer 存储文件状态的结构体指针
 * @return 成功返回0，失败返回错误码
 */
int lstat(const char *path, struct stat *buffer)
{
    return stat(path, buffer);  // 调用stat函数获取文件状态
}

/**
 * @brief 检查vnode的访问权限
 * @param node vnode结构体指针
 * @param accMode 访问模式
 * @return 权限检查结果，0表示有权限，1表示无权限
 */
int VfsVnodePermissionCheck(const struct Vnode *node, int accMode)
{
    uint fuid = node->uid;  // 文件所有者ID
    uint fgid = node->gid;  // 文件所属组ID
    uint fileMode = node->mode;  // 文件权限模式
    return VfsPermissionCheck(fuid, fgid, fileMode, accMode);  // 调用权限检查函数
}

/**
 * @brief 检查文件访问权限
 * @param fuid 文件所有者ID
 * @param fgid 文件所属组ID
 * @param fileMode 文件权限模式
 * @param accMode 请求的访问模式
 * @return 权限检查结果，0表示有权限，1表示无权限
 */
int VfsPermissionCheck(uint fuid, uint fgid, uint fileMode, int accMode)
{
    uint uid = OsCurrUserGet()->effUserID;  // 获取当前用户有效ID
    mode_t tmpMode = fileMode;  // 临时存储文件权限模式

    if (uid == fuid) {  // 当前用户是文件所有者
        tmpMode >>= USER_MODE_SHIFT;  // 右移用户权限位
    } else if (LOS_CheckInGroups(fgid)) {  // 当前用户属于文件所属组
        tmpMode >>= GROUP_MODE_SHIFT;  // 右移组权限位
    }

    tmpMode &= (READ_OP | WRITE_OP | EXEC_OP);  // 提取读写执行权限位

    if (((uint)accMode & tmpMode) == accMode) {  // 检查权限是否满足
        return 0;  // 权限满足，返回0
    }

    tmpMode = 0;  // 重置临时权限变量
    if (S_ISDIR(fileMode)) {  // 如果是目录
        if (IsCapPermit(CAP_DAC_EXECUTE)  // 检查是否有执行权限
            || (!((uint)accMode & WRITE_OP) && IsCapPermit(CAP_DAC_READ_SEARCH))) {  // 检查是否有读搜索权限
            tmpMode |= EXEC_OP;  // 设置执行权限
        }
    } else {  // 如果是文件
        if (IsCapPermit(CAP_DAC_EXECUTE) && (fileMode & MODE_IXUGO)) {  // 检查是否有执行权限
            tmpMode |= EXEC_OP;  // 设置执行权限
        }
    }

    if (IsCapPermit(CAP_DAC_WRITE)) {  // 检查是否有写权限
        tmpMode |= WRITE_OP;  // 设置写权限
    }

    if (IsCapPermit(CAP_DAC_READ_SEARCH)) {  // 检查是否有读搜索权限
        tmpMode |= READ_OP;  // 设置读权限
    }

    if (((uint)accMode & tmpMode) == accMode) {  // 再次检查权限是否满足
        return 0;  // 权限满足，返回0
    }

    return 1;  // 权限不满足，返回1
}

#ifdef VFS_USING_WORKDIR
/**
 * @brief 设置当前工作目录
 * @param dir 目录路径
 * @param len 路径长度
 * @return 成功返回0，失败返回-1
 */
static int SetWorkDir(const char *dir, size_t len)
{
    errno_t ret;  // 错误码
    uint lock_flags;  // 锁标志
    LosProcessCB *curr = OsCurrProcessGet();  // 获取当前进程控制块

    spin_lock_irqsave(&curr->files->workdir_lock, lock_flags);  // 加锁保护工作目录
    ret = strncpy_s(curr->files->workdir, PATH_MAX, dir, len);  // 复制目录路径
    curr->files->workdir[PATH_MAX - 1] = '\0';  // 确保字符串结束符
    spin_unlock_irqrestore(&curr->files->workdir_lock, lock_flags);  // 解锁
    if (ret != EOK) {  // 检查复制是否成功
        return -1;  // 失败返回-1
    }

        return 0;  // 成功返回0
}
#endif

/**
 * @brief 切换当前工作目录
 * @param path 目标目录路径
 * @return 成功返回0，失败返回-1
 */
int chdir(const char *path)
{
    int ret;  // 错误码
    char *fullpath = NULL;  // 完整路径
    char *fullpath_bak = NULL;  // 完整路径备份
    struct stat statBuff;  // 文件状态结构体

    if (!path) {  // 检查路径是否为空
        set_errno(EFAULT);  // 设置错误码为EFAULT
        return -1;  // 返回-1
    }

    if (!strlen(path)) {  // 检查路径长度是否为0
        set_errno(ENOENT);  // 设置错误码为ENOENT
        return -1;  // 返回-1
    }

    if (strlen(path) > PATH_MAX) {  // 检查路径是否过长
        set_errno(ENAMETOOLONG);  // 设置错误码为ENAMETOOLONG
        return -1;  // 返回-1
    }

    ret = vfs_normalize_path((const char *)NULL, path, &fullpath);  // 规范化路径
    if (ret < 0) {  // 检查规范化是否成功
        set_errno(-ret);  // 设置错误码
        return -1;  /* 路径构建失败 */
    }
    fullpath_bak = fullpath;  // 备份完整路径
    ret = stat(fullpath, &statBuff);  // 获取文件状态
    if (ret < 0) {  // 检查获取是否成功
        free(fullpath_bak);  // 释放内存
        return -1;  // 返回-1
    }

    if (!S_ISDIR(statBuff.st_mode)) {  // 检查是否为目录
        set_errno(ENOTDIR);  // 设置错误码为ENOTDIR
        free(fullpath_bak);  // 释放内存
        return -1;  // 返回-1
    }

    if (VfsPermissionCheck(statBuff.st_uid, statBuff.st_gid, statBuff.st_mode, EXEC_OP)) {  // 检查执行权限
        set_errno(EACCES);  // 设置错误码为EACCES
        free(fullpath_bak);  // 释放内存
        return -1;  // 返回-1
    }

#ifdef VFS_USING_WORKDIR
    ret = SetWorkDir(fullpath, strlen(fullpath));  // 设置工作目录
    if (ret != 0) {  // 检查设置是否成功
        PRINT_ERR("chdir path error!\n");  // 打印错误信息
        ret = -1;  // 设置返回值为-1
    }
#endif

    /* 释放规范化的目录路径名 */

    free(fullpath_bak);  // 释放内存

    return ret;  // 返回结果
}

/**
 * this function is a POSIX compliant version, which will return current
 * working directory.
 *
 * @param buf the returned current directory.
 * @param size the buffer size.
 *
 * @return the returned current directory.
 */

/**
 * @brief 获取当前工作目录（POSIX兼容版本）
 * @param buf 存储当前目录的缓冲区
 * @param size 缓冲区大小
 * @return 成功返回当前目录路径，失败返回NULL
 */
char *getcwd(char *buf, size_t n)
{
#ifdef VFS_USING_WORKDIR
    int ret;  // 错误码
    unsigned int len;  // 路径长度
    UINTPTR lock_flags;  // 锁标志
    LosProcessCB *curr = OsCurrProcessGet();  // 获取当前进程控制块
#endif
    if (buf == NULL) {  // 检查缓冲区是否为空
        set_errno(EINVAL);  // 设置错误码为EINVAL
        return buf;  // 返回NULL
    }
#ifdef VFS_USING_WORKDIR
    spin_lock_irqsave(&curr->files->workdir_lock, lock_flags);  // 加锁保护工作目录
    len = strlen(curr->files->workdir);  // 获取工作目录长度
    if (n <= len) {  // 检查缓冲区是否足够大
        set_errno(ERANGE);  // 设置错误码为ERANGE
        spin_unlock_irqrestore(&curr->files->workdir_lock, lock_flags);  // 解锁
        return NULL;  // 返回NULL
    }
    ret = memcpy_s(buf, n, curr->files->workdir, len + 1);  // 复制工作目录到缓冲区
    if (ret != EOK) {  // 检查复制是否成功
        set_errno(ENAMETOOLONG);  // 设置错误码为ENAMETOOLONG
        spin_unlock_irqrestore(&curr->files->workdir_lock, lock_flags);  // 解锁
        return NULL;  // 返回NULL
    }
    spin_unlock_irqrestore(&curr->files->workdir_lock, lock_flags);  // 解锁
#else
    PRINT_ERR("NO_WORKING_DIR\n");  // 打印无工作目录错误
#endif

    return buf;  // 返回当前目录路径
}

/**
 * @brief 修改文件权限
 * @param path 文件路径
 * @param mode 新的权限模式
 * @return 成功返回0，失败返回VFS_ERROR
 */
int chmod(const char *path, mode_t mode)
{
    struct IATTR attr = {0};  // 文件属性结构体
    attr.attr_chg_mode = mode;  // 设置新的权限模式
    attr.attr_chg_valid = CHG_MODE; /* 更改模式标志 */
    int ret;  // 错误码

    ret = chattr(path, &attr);  // 调用chattr函数修改文件属性
    if (ret < 0) {  // 检查修改是否成功
        return VFS_ERROR;  // 返回错误
    }

    return OK;  // 成功返回0
}

/**
 * @brief 修改文件所有者和所属组
 * @param pathname 文件路径
 * @param owner 新的所有者ID
 * @param group 新的所属组ID
 * @return 成功返回0，失败返回VFS_ERROR
 */
int chown(const char *pathname, uid_t owner, gid_t group)
{
    struct IATTR attr = {0};  // 文件属性结构体
    attr.attr_chg_valid = 0;  // 初始化属性更改标志
    int ret;  // 错误码

    if (owner != (uid_t)-1) {  // 如果指定了新的所有者
        attr.attr_chg_uid = owner;  // 设置新的所有者ID
        attr.attr_chg_valid |= CHG_UID;  // 设置UID更改标志
    }
    if (group != (gid_t)-1) {  // 如果指定了新的所属组
        attr.attr_chg_gid = group;  // 设置新的所属组ID
        attr.attr_chg_valid |= CHG_GID;  // 设置GID更改标志
    }
    ret = chattr(pathname, &attr);  // 调用chattr函数修改文件属性
    if (ret < 0) {  // 检查修改是否成功
        return VFS_ERROR;  // 返回错误
    }

    return OK;  // 成功返回0
}

/**
 * @brief 检查文件访问权限
 * @param path 文件路径
 * @param amode 访问模式
 * @return 成功返回0，失败返回VFS_ERROR
 */
int access(const char *path, int amode)
{
    int ret;  // 错误码
    struct stat buf;  // 文件状态结构体
    struct statfs fsBuf;  // 文件系统状态结构体

    ret = statfs(path, &fsBuf);  // 获取文件系统状态
    if (ret != 0) {  // 检查获取是否成功
        if (get_errno() != ENOSYS) {  // 如果错误不是ENOSYS
            return VFS_ERROR;  // 返回错误
        }
        /* 设备没有statfs操作，需要devfs在后续处理 */
    }

    if ((fsBuf.f_flags & MS_RDONLY) && ((unsigned int)amode & W_OK)) {  // 如果文件系统只读且请求写权限
        set_errno(EROFS);  // 设置错误码为EROFS
        return VFS_ERROR;  // 返回错误
    }

    ret = stat(path, &buf);  // 获取文件状态
    if (ret != 0) {  // 检查获取是否成功
        return VFS_ERROR;  // 返回错误
    }

    if (VfsPermissionCheck(buf.st_uid, buf.st_gid, buf.st_mode, amode)) {  // 检查访问权限
        set_errno(EACCES);  // 设置错误码为EACCES
        return VFS_ERROR;  // 返回错误
    }

    return OK;  // 成功返回0
}

/**
 * @brief 获取目录文件列表
 * @param dir 目录路径
 * @param num 存储文件数量的指针
 * @param filter 过滤函数
 * @return 成功返回文件列表，失败返回NULL
 */
static struct dirent **scandir_get_file_list(const char *dir, int *num, int(*filter)(const struct dirent *))
{
    DIR *od = NULL;  // 目录流指针
    int listSize = MAX_DIR_ENT;  // 列表大小
    int n = 0;  // 文件计数
    struct dirent **list = NULL;  // 文件列表
    struct dirent **newList = NULL;  // 新文件列表（用于扩容）
    struct dirent *ent = NULL;  // 目录项
    struct dirent *p = NULL;  // 目录项副本
    int err;  // 错误码

    od = opendir(dir);  // 打开目录
    if (od == NULL) {  // 检查打开是否成功
        return NULL;  // 返回NULL
    }

    list = (struct dirent **)malloc(listSize * sizeof(struct dirent *));  // 分配文件列表内存
    if (list == NULL) {  // 检查分配是否成功
        (void)closedir(od);  // 关闭目录
        return NULL;  // 返回NULL
    }

    for (ent = readdir(od); ent != NULL; ent = readdir(od)) {  // 遍历目录项
        if (filter && !filter(ent)) {  // 如果设置了过滤函数且当前项不满足过滤条件
            continue;  // 跳过当前项
        }

        if (n == listSize) {  // 如果列表已满
            listSize += MAX_DIR_ENT;  // 扩容列表
            newList = (struct dirent **)malloc(listSize * sizeof(struct dirent *));  // 分配新列表内存
            if (newList == NULL) {  // 检查分配是否成功
                break;  // 分配失败，跳出循环
            }

            err = memcpy_s(newList, listSize * sizeof(struct dirent *), list, n * sizeof(struct dirent *));  // 复制旧列表到新列表
            if (err != EOK) {  // 检查复制是否成功
                free(newList);  // 释放新列表内存
                break;  // 复制失败，跳出循环
            }
            free(list);  // 释放旧列表内存
            list = newList;  // 更新列表指针
        }

        p = (struct dirent *)malloc(sizeof(struct dirent));  // 分配目录项内存
        if (p == NULL) {  // 检查分配是否成功
            break;  // 分配失败，跳出循环
        }

        (void)memcpy_s((void *)p, sizeof(struct dirent), (void *)ent, sizeof(struct dirent));  // 复制目录项
        list[n] = p;  // 添加到列表

        n++;  // 增加计数
    }

    if (closedir(od) < 0) {  // 关闭目录并检查是否成功
        while (n--) {  // 释放已分配的目录项
            free(list[n]);
        }
        free(list);  // 释放列表
        return NULL;  // 返回NULL
    }

    *num = n;  // 设置文件数量
    return list;  // 返回文件列表
}

/**
 * @brief 扫描目录并排序文件列表
 * @param dir 目录路径
 * @param namelist 存储文件列表的指针
 * @param filter 过滤函数
 * @param compar 比较函数（用于排序）
 * @return 成功返回文件数量，失败返回-1
 */
int scandir(const char *dir, struct dirent ***namelist, int(*filter)(const struct dirent *), int(*compar)(const struct dirent **, const struct dirent **))
{
    int n = 0;  // 文件数量
    struct dirent **list = NULL;  // 文件列表

    if ((dir == NULL) || (namelist == NULL)) {  // 检查参数是否为空
        return -1;  // 返回-1
    }

    list = scandir_get_file_list(dir, &n, filter);  // 获取文件列表
    if (list == NULL) {  // 检查获取是否成功
        return -1;  // 返回-1
    }

    /* 更改为返回数组大小 */
    *namelist = (struct dirent **)malloc(n * sizeof(struct dirent *));  // 分配结果列表内存
    if (*namelist == NULL && n > 0) {  // 如果分配失败且有文件
        *namelist = list;  // 直接使用原始列表
    } else if (*namelist != NULL) {  // 如果分配成功
        (void)memcpy_s(*namelist, n * sizeof(struct dirent *), list, n * sizeof(struct dirent *));  // 复制列表
        free(list);  // 释放原始列表
    } else {  // 如果没有文件
        free(list);  // 释放原始列表
    }

    /* 排序数组 */
    if (compar && *namelist) {  // 如果设置了比较函数且列表不为空
        qsort((void *)*namelist, (size_t)n, sizeof(struct dirent *), (int (*)(const void *, const void *))*compar);  // 排序列表
    }

    return n;  // 返回文件数量
}

/**
 * @brief 按字母顺序比较两个目录项
 * @param a 目录项a
 * @param b 目录项b
 * @return 比较结果
 */
int alphasort(const struct dirent **a, const struct dirent **b)
{
    return strcoll((*a)->d_name, (*b)->d_name);  // 调用strcoll比较文件名
}

/**
 * @brief 获取文件的完整路径
 * @param path 目录路径
 * @param pdirent 目录项
 * @return 成功返回完整路径，失败返回NULL
 */
char *rindex(const char *s, int c)
{
    if (s == NULL) {  // 检查字符串是否为空
        return NULL;  // 返回NULL
    }

    /* 无需跟踪 - strrchr可以处理 */
    return (char *)strrchr(s, c);  // 调用strrchr查找字符最后出现的位置
}

/**
 * @brief 获取文件的完整路径
 * @param path 目录路径
 * @param pdirent 目录项
 * @return 成功返回完整路径，失败返回NULL
 */
static char *ls_get_fullpath(const char *path, struct dirent *pdirent)
{
    char *fullpath = NULL;  // 完整路径
    int ret;  // 错误码

    if (path[1] != '\0') {  // 如果路径不是根目录
        /* 2，路径字符的位置：/ 和结束字符'\0' */
        fullpath = (char *)malloc(strlen(path) + strlen(pdirent->d_name) + 2);  // 分配内存
        if (fullpath == NULL) {  // 检查分配是否成功
            goto exit_with_nomem;  // 跳转到内存不足处理
        }

        /* 2，路径字符的位置：/ 和结束字符'\0' */
        ret = snprintf_s(fullpath, strlen(path) + strlen(pdirent->d_name) + 2, strlen(path) + strlen(pdirent->d_name) + 1, "%s/%s", path, pdirent->d_name);  // 构建完整路径
        if (ret < 0) {  // 检查构建是否成功
            free(fullpath);  // 释放内存
            set_errno(ENAMETOOLONG);  // 设置错误码
            return NULL;  // 返回NULL
        }
    } else {  // 如果路径是根目录
        /* 2，路径字符的位置：/ 和结束字符'\0' */
        fullpath = (char *)malloc(strlen(pdirent->d_name) + 2);  // 分配内存
        if (fullpath == NULL) {  // 检查分配是否成功
            goto exit_with_nomem;  // 跳转到内存不足处理
        }

        /* 2，路径字符的位置：/ 和结束字符'\0' */
        ret = snprintf_s(fullpath, strlen(pdirent->d_name) + 2, strlen(pdirent->d_name) + 1, "/%s", pdirent->d_name);  // 构建完整路径
        if (ret < 0) {  // 检查构建是否成功
            free(fullpath);  // 释放内存
            set_errno(ENAMETOOLONG);  // 设置错误码
            return NULL;  // 返回NULL
        }
    }
    return fullpath;  // 返回完整路径

exit_with_nomem:  // 内存不足处理标签
    set_errno(ENOSPC);  // 设置错误码为ENOSPC
    return (char *)NULL;  // 返回NULL
}

/**
 * @brief 打印64位文件信息
 * @param stat64Info 64位文件状态结构体
 * @param name 文件名
 * @param linkName 链接目标名
 */
static void PrintFileInfo64(const struct stat64 *stat64Info, const char *name, const char *linkName)
{
    mode_t mode;  // 文件权限模式
    char str[UGO_NUMS][UGO_NUMS + 1] = {0};  // 权限字符串数组
    char dirFlag;  // 文件类型标志
    int i;  // 循环变量

    for (i = 0; i < UGO_NUMS; i++) {  // 遍历用户、组、其他权限
        mode = stat64Info->st_mode >> (uint)(USER_MODE_SHIFT - i * UGO_NUMS);  // 获取对应权限位
        str[i][0] = (mode & READ_OP) ? 'r' : '-';  // 设置读权限标志
        str[i][1] = (mode & WRITE_OP) ? 'w' : '-';  // 设置写权限标志
        str[i][UGO_NUMS - 1] = (mode & EXEC_OP) ? 'x' : '-';  // 设置执行权限标志
    }

    if (S_ISDIR(stat64Info->st_mode)) {  // 如果是目录
        dirFlag = 'd';
    } else if (S_ISLNK(stat64Info->st_mode)) {  // 如果是符号链接
        dirFlag = 'l';
    } else {  // 其他文件类型
        dirFlag = '-';
    }

    if (S_ISLNK(stat64Info->st_mode)) {  // 如果是符号链接
        PRINTK("%c%s%s%s %-8lld u:%-5d g:%-5d %-10s -> %s\n", dirFlag, str[0], str[1], str[UGO_NUMS - 1], stat64Info->st_size, stat64Info->st_uid, stat64Info->st_gid, name, linkName);  // 打印带链接目标的信息
    } else {  // 其他文件类型
        PRINTK("%c%s%s%s %-8lld u:%-5d g:%-5d %-10s\n", dirFlag, str[0], str[1], str[UGO_NUMS - 1], stat64Info->st_size, stat64Info->st_uid, stat64Info->st_gid, name);  // 打印文件信息
    }
}

/**
 * @brief 打印文件信息
 * @param statInfo 文件状态结构体
 * @param name 文件名
 * @param linkName 链接目标名
 */
static void PrintFileInfo(const struct stat *statInfo, const char *name, const char *linkName)
{
    mode_t mode;  // 文件权限模式
    char str[UGO_NUMS][UGO_NUMS + 1] = {0};  // 权限字符串数组
    char dirFlag;  // 文件类型标志
    int i;  // 循环变量

    for (i = 0; i < UGO_NUMS; i++) {  // 遍历用户、组、其他权限
        mode = statInfo->st_mode >> (uint)(USER_MODE_SHIFT - i * UGO_NUMS);  // 获取对应权限位
        str[i][0] = (mode & READ_OP) ? 'r' : '-';  // 设置读权限标志
        str[i][1] = (mode & WRITE_OP) ? 'w' : '-';  // 设置写权限标志
        str[i][UGO_NUMS - 1] = (mode & EXEC_OP) ? 'x' : '-';  // 设置执行权限标志
    }

    if (S_ISDIR(statInfo->st_mode)) {  // 如果是目录
        dirFlag = 'd';
    } else if (S_ISLNK(statInfo->st_mode)) {  // 如果是符号链接
        dirFlag = 'l';
    } else {  // 其他文件类型
        dirFlag = '-';
    }

    if (S_ISLNK(statInfo->st_mode)) {  // 如果是符号链接
        PRINTK("%c%s%s%s %-8lld u:%-5d g:%-5d %-10s -> %s\n", dirFlag, str[0], str[1], str[UGO_NUMS - 1], statInfo->st_size, statInfo->st_uid, statInfo->st_gid, name, linkName);  // 打印带链接目标的信息
    } else {  // 其他文件类型
        PRINTK("%c%s%s%s %-8lld u:%-5d g:%-5d %-10s\n", dirFlag, str[0], str[1], str[UGO_NUMS - 1], statInfo->st_size, statInfo->st_uid, statInfo->st_gid, name);  // 打印文件信息
    }
}

/**
 * @brief 列出单个文件信息
 * @param path 文件路径
 * @return 成功返回0，失败返回-1
 */
int LsFile(const char *path)
{
    struct stat64 stat64Info;  // 64位文件状态结构体
    struct stat statInfo;  // 文件状态结构体
    char linkName[NAME_MAX] = { 0 };  // 链接目标名

    if (stat64(path, &stat64Info) == 0) {  // 获取64位文件状态
        if (S_ISLNK(stat64Info.st_mode)) {  // 如果是符号链接
            readlink(path, linkName, NAME_MAX);  // 读取链接目标
        }
        PrintFileInfo64(&stat64Info, path, (const char *)linkName);  // 打印64位文件信息
    } else if (stat(path, &statInfo) == 0) {  // 获取文件状态
        if (S_ISLNK(statInfo.st_mode)) {  // 如果是符号链接
            readlink(path, linkName, NAME_MAX);  // 读取链接目标
        }
        PrintFileInfo(&statInfo, path, (const char *)linkName);  // 打印文件信息
    } else {  // 获取状态失败
        return -1;  // 返回-1
    }

    return 0;  // 成功返回0
}

/**
 * @brief 列出目录下所有文件信息
 * @param path 目录路径
 * @return 成功返回0，失败返回-1
 */
int LsDir(const char *path)
{
    struct stat statInfo = { 0 };  // 文件状态结构体
    struct stat64 stat64Info = { 0 };  // 64位文件状态结构体
    char linkName[NAME_MAX] = { 0 };  // 链接目标名
    DIR *d = NULL;  // 目录流指针
    char *fullpath = NULL;  // 完整路径
    char *fullpath_bak = NULL;  // 完整路径备份
    struct dirent *pdirent = NULL;  // 目录项

    d = opendir(path);  // 打开目录
    if (d == NULL) {  // 检查打开是否成功
        return -1;  // 返回-1
    }

    PRINTK("Directory %s:\n", path);  // 打印目录名
    do {
        pdirent = readdir(d);  // 读取目录项
        if (pdirent == NULL) {  // 检查是否读取完毕
            break;  // 跳出循环
        }
        if (!strcmp(pdirent->d_name, ".") || !strcmp(pdirent->d_name, "..")) {  // 跳过当前目录和父目录
            continue;
        }
        (void)memset_s(&statInfo, sizeof(struct stat), 0, sizeof(struct stat));  // 清空文件状态结构体
        (void)memset_s(&stat64Info, sizeof(struct stat), 0, sizeof(struct stat));  // 清空64位文件状态结构体
        (void)memset_s(&linkName, sizeof(linkName), 0, sizeof(linkName));  // 清空链接目标名
        fullpath = ls_get_fullpath(path, pdirent);  // 获取完整路径
        if (fullpath == NULL) {  // 检查获取是否成功
            (void)closedir(d);  // 关闭目录
            return -1;  // 返回-1
        }

        fullpath_bak = fullpath;  // 备份完整路径
        if (stat64(fullpath, &stat64Info) == 0) {  // 获取64位文件状态
            if (S_ISLNK(stat64Info.st_mode)) {  // 如果是符号链接
                readlink(fullpath, linkName, NAME_MAX);  // 读取链接目标
            }
            PrintFileInfo64(&stat64Info, pdirent->d_name, linkName);  // 打印64位文件信息
        } else if (stat(fullpath, &statInfo) == 0) {  // 获取文件状态
            if (S_ISLNK(statInfo.st_mode)) {  // 如果是符号链接
                readlink(fullpath, linkName, NAME_MAX);  // 读取链接目标
            }
            PrintFileInfo(&statInfo, pdirent->d_name, linkName);  // 打印文件信息
        } else {  // 获取状态失败
            PRINTK("BAD file: %s\n", pdirent->d_name);  // 打印错误信息
        }
        free(fullpath_bak);  // 释放完整路径内存
    } while (1);
    (void)closedir(d);  // 关闭目录

    return 0;  // 成功返回0
}

/**
 * @brief 列出文件或目录信息
 * @param pathname 文件或目录路径，为NULL时列出当前工作目录
 */
void ls(const char *pathname)
{
    struct stat statInfo = { 0 };  // 文件状态结构体
    char *path = NULL;  // 路径
    int ret;  // 错误码

    if (pathname == NULL) {  // 如果路径为NULL
#ifdef VFS_USING_WORKDIR
        UINTPTR lock_flags;  // 锁标志
        LosProcessCB *curr = OsCurrProcessGet();  // 获取当前进程控制块

        /* 打开当前工作目录 */
        spin_lock_irqsave(&curr->files->workdir_lock, lock_flags);  // 加锁保护工作目录
        path = strdup(curr->files->workdir);  // 复制工作目录路径
        spin_unlock_irqrestore(&curr->files->workdir_lock, lock_flags);  // 解锁
#else
        path = strdup("/");  // 使用根目录
#endif
        if (path == NULL) {  // 检查复制是否成功
            return;  // 返回
        }
    } else {  // 如果路径不为NULL
        ret = vfs_normalize_path(NULL, pathname, &path);  // 规范化路径
        if (ret < 0) {  // 检查规范化是否成功
            set_errno(-ret);  // 设置错误码
            return;  // 返回
        }
    }

    ret = stat(path, &statInfo);  // 获取文件状态
    if (ret < 0) {  // 检查获取是否成功
        perror("ls error");  // 打印错误信息
        free(path);  // 释放路径内存
        return;  // 返回
    }

    if (statInfo.st_mode & S_IFDIR) { /* 列出所有目录和文件 */
        ret = LsDir((pathname == NULL) ? path : pathname);  // 调用LsDir列出目录
    } else { /* 显示文件信息 */
        ret = LsFile(path);  // 调用LsFile列出文件
    }
    if (ret < 0) {  // 检查列出是否成功
        perror("ls error");  // 打印错误信息
    }

    free(path);  // 释放路径内存
    return;  // 返回
}


/**
 * @brief 获取文件的真实路径
 * @param path 文件路径
 * @param resolved_path 存储真实路径的缓冲区
 * @return 成功返回真实路径，失败返回NULL
 */
char *realpath(const char *path, char *resolved_path)
{
    int ret, result;  // 错误码
    char *new_path = NULL;  // 规范化后的路径
    struct stat buf;  // 文件状态结构体

    ret = vfs_normalize_path(NULL, path, &new_path);  // 规范化路径
    if (ret < 0) {  // 检查规范化是否成功
        ret = -ret;  // 转换错误码
        set_errno(ret);  // 设置错误码
        return NULL;  // 返回NULL
    }

    result = stat(new_path, &buf);  // 获取文件状态

    if (resolved_path == NULL) {  // 如果缓冲区为NULL
        if (result != ENOERR) {  // 如果获取状态失败
            free(new_path);  // 释放内存
            return NULL;  // 返回NULL
        }
        return new_path;  // 返回规范化后的路径
    }

    ret = strcpy_s(resolved_path, PATH_MAX, new_path);  // 复制路径到缓冲区
    if (ret != EOK) {  // 检查复制是否成功
        ret = -ret;  // 转换错误码
        set_errno(ret);  // 设置错误码
        free(new_path);  // 释放内存
        return NULL;  // 返回NULL
    }

    free(new_path);  // 释放内存
    if (result != ENOERR) {  // 如果获取状态失败
        return NULL;  // 返回NULL
    }
    return resolved_path;  // 返回真实路径
}

/**
 * @brief 列出所有打开的文件描述符
 */
void lsfd(void)
{
    struct filelist *f_list = NULL;  // 文件列表结构体
    unsigned int i = 3; /* 文件起始描述符 */
    int ret;  // 错误码
    struct Vnode *node = NULL;  // vnode结构体

    f_list = &tg_filelist;  // 获取全局文件列表

    PRINTK("   fd    filename\n");  // 打印表头
    ret = sem_wait(&f_list->fl_sem);  // 获取信号量
    if (ret < 0) {  // 检查获取是否成功
        PRINTK("sem_wait error, ret=%d\n", ret);  // 打印错误信息
        return;  // 返回
    }

    while (i < CONFIG_NFILE_DESCRIPTORS) {  // 遍历文件描述符
        node = files_get_openfile(i);  // 获取打开的文件
        if (node) {  // 如果文件存在
            PRINTK("%5d   %s\n", i, f_list->fl_files[i].f_path);  // 打印文件描述符和路径
        }
        i++;  // 增加描述符
    }
    (void)sem_post(&f_list->fl_sem);  // 释放信号量
}

/**
 * @brief 获取当前进程的umask
 * @return 当前umask值
 */
mode_t GetUmask(void)
{
    return OsCurrProcessGet()->umask;  // 返回当前进程的umask
}

/**
 * @brief 设置当前进程的umask
 * @param mask 新的umask值
 * @return 旧的umask值
 */
mode_t SysUmask(mode_t mask)
{
    UINT32 intSave;  // 中断标志
    mode_t umask;  // 新的umask
    mode_t oldUmask;  // 旧的umask
    umask = mask & UMASK_FULL;  // 确保umask在有效范围内
    SCHEDULER_LOCK(intSave);  // 关闭调度
    oldUmask = OsCurrProcessGet()->umask;  // 保存旧的umask
    OsCurrProcessGet()->umask = umask;  // 设置新的umask
    SCHEDULER_UNLOCK(intSave);  // 开启调度
    return oldUmask;  // 返回旧的umask
}

#ifdef LOSCFG_CHROOT
/**
 * @brief 更改根目录
 * @param path 新的根目录路径
 * @return 成功返回LOS_OK，失败返回错误码
 */
int chroot(const char *path)
{
    int ret;  // 错误码
    struct Vnode *vnode = NULL;  // vnode结构体

    if (!path) {  // 检查路径是否为空
        set_errno(EFAULT);  // 设置错误码
        return VFS_ERROR;  // 返回错误
    }

    if (!strlen(path)) {  // 检查路径长度是否为0
        set_errno(ENOENT);  // 设置错误码
        return VFS_ERROR;  // 返回错误
    }

    if (strlen(path) > PATH_MAX) {  // 检查路径是否过长
        set_errno(ENAMETOOLONG);  // 设置错误码
        return VFS_ERROR;  // 返回错误
    }
    VnodeHold();  // 获取vnode锁
    ret = VnodeLookup(path, &vnode, 0);  // 查找vnode
    if (ret != LOS_OK) {  // 检查查找是否成功
        VnodeDrop();  // 释放vnode锁
        return ret;  // 返回错误
    }

    LosProcessCB *curr = OsCurrProcessGet();  // 获取当前进程控制块
    if ((curr->files == NULL) || (curr->files->rootVnode == NULL)) {  // 检查文件结构体是否有效
        VnodeDrop();  // 释放vnode锁
        return VFS_ERROR;  // 返回错误
    }
    if (curr->files->rootVnode->useCount > 0) {  // 如果旧根目录vnode引用计数大于0
        curr->files->rootVnode->useCount--;  // 减少引用计数
    }
    vnode->useCount++;  // 增加新根目录vnode引用计数
    curr->files->rootVnode = vnode;  // 设置新的根目录vnode

    VnodeDrop();  // 释放vnode锁
    return LOS_OK;  // 成功返回LOS_OK
}
#endif