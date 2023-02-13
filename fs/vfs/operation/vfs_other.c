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

#define MAX_DIR_ENT 1024
int fstat(int fd, struct stat *buf)
{
    struct file *filep = NULL;

    int ret = fs_getfilep(fd, &filep);
    if (ret < 0) {
        return VFS_ERROR;
    }

    return stat(filep->f_path, buf);//统计
}

int fstat64(int fd, struct stat64 *buf)
{
    struct file *filep = NULL;

    int ret = fs_getfilep(fd, &filep);
    if (ret < 0) {
        return VFS_ERROR;
    }

    return stat64(filep->f_path, buf);
}

int lstat(const char *path, struct stat *buffer)
{
    return stat(path, buffer);
}

int VfsVnodePermissionCheck(const struct Vnode *node, int accMode)
{
    uint fuid = node->uid;
    uint fgid = node->gid;
    uint fileMode = node->mode;
    return VfsPermissionCheck(fuid, fgid, fileMode, accMode);
}
int VfsPermissionCheck(uint fuid, uint fgid, uint fileMode, int accMode)
{
    uint uid = OsCurrUserGet()->effUserID;
    mode_t tmpMode = fileMode;

    if (uid == fuid) {
        tmpMode >>= USER_MODE_SHIFT;
    } else if (LOS_CheckInGroups(fgid)) {
        tmpMode >>= GROUP_MODE_SHIFT;
    }

    tmpMode &= (READ_OP | WRITE_OP | EXEC_OP);

    if (((uint)accMode & tmpMode) == accMode) {
        return 0;
    }

    tmpMode = 0;
    if (S_ISDIR(fileMode)) {
        if (IsCapPermit(CAP_DAC_EXECUTE)
            || (!((uint)accMode & WRITE_OP) && IsCapPermit(CAP_DAC_READ_SEARCH))) {
            tmpMode |= EXEC_OP;
        }
    } else {
        if (IsCapPermit(CAP_DAC_EXECUTE) && (fileMode & MODE_IXUGO)) {
            tmpMode |= EXEC_OP;
        }
    }

    if (IsCapPermit(CAP_DAC_WRITE)) {
        tmpMode |= WRITE_OP;
    }

    if (IsCapPermit(CAP_DAC_READ_SEARCH)) {
        tmpMode |= READ_OP;
    }

    if (((uint)accMode & tmpMode) == accMode) {
        return 0;
    }

    return 1;
}

#ifdef VFS_USING_WORKDIR
static int SetWorkDir(const char *dir, size_t len)
{
  errno_t ret;
  uint lock_flags;
  LosProcessCB *curr = OsCurrProcessGet();

  spin_lock_irqsave(&curr->files->workdir_lock, lock_flags);
  ret = strncpy_s(curr->files->workdir, PATH_MAX, dir, len);
  curr->files->workdir[PATH_MAX - 1] = '\0';
  spin_unlock_irqrestore(&curr->files->workdir_lock, lock_flags);
  if (ret != EOK) {
      return -1;
  }

  return 0;
}
#endif

int chdir(const char *path)
{
    int ret;
    char *fullpath = NULL;
    char *fullpath_bak = NULL;
    struct stat statBuff;


    if (!path) {
        set_errno(EFAULT);
        return -1;
    }

    if (!strlen(path)) {
        set_errno(ENOENT);
        return -1;
    }

    if (strlen(path) > PATH_MAX) {
        set_errno(ENAMETOOLONG);
        return -1;
    }

    ret = vfs_normalize_path((const char *)NULL, path, &fullpath);
    if (ret < 0) {
        set_errno(-ret);
        return -1; /* build path failed */
    }
    fullpath_bak = fullpath;
    ret = stat(fullpath, &statBuff);
    if (ret < 0) {
        free(fullpath_bak);
        return -1;
    }

    if (!S_ISDIR(statBuff.st_mode)) {
        set_errno(ENOTDIR);
        free(fullpath_bak);
        return -1;
    }

    if (VfsPermissionCheck(statBuff.st_uid, statBuff.st_gid, statBuff.st_mode, EXEC_OP)) {
        set_errno(EACCES);
        free(fullpath_bak);
        return -1;
    }

#ifdef VFS_USING_WORKDIR
    ret = SetWorkDir(fullpath, strlen(fullpath));
    if (ret != 0) {
        PRINT_ERR("chdir path error!\n");
        ret = -1;
    }
#endif

    /* release normalize directory path name */

    free(fullpath_bak);

    return ret;
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

char *getcwd(char *buf, size_t n)
{
#ifdef VFS_USING_WORKDIR
    int ret;
    unsigned int len;
    UINTPTR lock_flags;
    LosProcessCB *curr = OsCurrProcessGet();
#endif
    if (buf == NULL) {
        set_errno(EINVAL);
        return buf;
    }
#ifdef VFS_USING_WORKDIR
    spin_lock_irqsave(&curr->files->workdir_lock, lock_flags);
    len = strlen(curr->files->workdir);
    if (n <= len) {
        set_errno(ERANGE);
        spin_unlock_irqrestore(&curr->files->workdir_lock, lock_flags);
        return NULL;
    }
    ret = memcpy_s(buf, n, curr->files->workdir, len + 1);
    if (ret != EOK) {
        set_errno(ENAMETOOLONG);
        spin_unlock_irqrestore(&curr->files->workdir_lock, lock_flags);
        return NULL;
    }
    spin_unlock_irqrestore(&curr->files->workdir_lock, lock_flags);
#else
    PRINT_ERR("NO_WORKING_DIR\n");
#endif

    return buf;
}

int chmod(const char *path, mode_t mode)
{
    struct IATTR attr = {0};
    attr.attr_chg_mode = mode;
    attr.attr_chg_valid = CHG_MODE; /* change mode */
    int ret;

    ret = chattr(path, &attr);
    if (ret < 0) {
        return VFS_ERROR;
    }

    return OK;
}

int chown(const char *pathname, uid_t owner, gid_t group)
{
    struct IATTR attr = {0};
    attr.attr_chg_valid = 0;
    int ret;

    if (owner != (uid_t)-1) {
        attr.attr_chg_uid = owner;
        attr.attr_chg_valid |= CHG_UID;
    }
    if (group != (gid_t)-1) {
        attr.attr_chg_gid = group;
        attr.attr_chg_valid |= CHG_GID;
    }
    ret = chattr(pathname, &attr);
    if (ret < 0) {
        return VFS_ERROR;
    }

    return OK;
}

int access(const char *path, int amode)
{
    int ret;
    struct stat buf;
    struct statfs fsBuf;

    ret = statfs(path, &fsBuf);
    if (ret != 0) {
        if (get_errno() != ENOSYS) {
            return VFS_ERROR;
        }
        /* dev has no statfs ops, need devfs to handle this in feature */
    }

    if ((fsBuf.f_flags & MS_RDONLY) && ((unsigned int)amode & W_OK)) {
        set_errno(EROFS);
        return VFS_ERROR;
    }

    ret = stat(path, &buf);
    if (ret != 0) {
        return VFS_ERROR;
    }

    if (VfsPermissionCheck(buf.st_uid, buf.st_gid, buf.st_mode, amode)) {
        set_errno(EACCES);
        return VFS_ERROR;
    }

    return OK;
}

static struct dirent **scandir_get_file_list(const char *dir, int *num, int(*filter)(const struct dirent *))
{
    DIR *od = NULL;
    int listSize = MAX_DIR_ENT;
    int n = 0;
    struct dirent **list = NULL;
    struct dirent **newList = NULL;
    struct dirent *ent = NULL;
    struct dirent *p = NULL;
    int err;

    od = opendir(dir);
    if (od == NULL) {
        return NULL;
    }

    list = (struct dirent **)malloc(listSize * sizeof(struct dirent *));
    if (list == NULL) {
        (void)closedir(od);
        return NULL;
    }

    for (ent = readdir(od); ent != NULL; ent = readdir(od)) {
        if (filter && !filter(ent)) {
            continue;
        }

        if (n == listSize) {
            listSize += MAX_DIR_ENT;
            newList = (struct dirent **)malloc(listSize * sizeof(struct dirent *));
            if (newList == NULL) {
                break;
            }

            err = memcpy_s(newList, listSize * sizeof(struct dirent *), list, n * sizeof(struct dirent *));
            if (err != EOK) {
                free(newList);
                break;
            }
            free(list);
            list = newList;
        }

        p = (struct dirent *)malloc(sizeof(struct dirent));
        if (p == NULL) {
            break;
        }

        (void)memcpy_s((void *)p, sizeof(struct dirent), (void *)ent, sizeof(struct dirent));
        list[n] = p;

        n++;
    }

    if (closedir(od) < 0) {
        while (n--) {
            free(list[n]);
        }
        free(list);
        return NULL;
    }

    *num = n;
    return list;
}

int scandir(const char *dir, struct dirent ***namelist,
            int(*filter)(const struct dirent *),
            int(*compar)(const struct dirent **, const struct dirent **))
{
    int n = 0;
    struct dirent **list = NULL;

    if ((dir == NULL) || (namelist == NULL)) {
        return -1;
    }

    list = scandir_get_file_list(dir, &n, filter);
    if (list == NULL) {
        return -1;
    }

    /* Change to return to the array size */
    *namelist = (struct dirent **)malloc(n * sizeof(struct dirent *));
    if (*namelist == NULL && n > 0) {
        *namelist = list;
    } else if (*namelist != NULL) {
        (void)memcpy_s(*namelist, n * sizeof(struct dirent *), list, n * sizeof(struct dirent *));
        free(list);
    } else {
        free(list);
    }

    /* Sort array */

    if (compar && *namelist) {
        qsort((void *)*namelist, (size_t)n, sizeof(struct dirent *), (int (*)(const void *, const void *))*compar);
    }

    return n;
}

int alphasort(const struct dirent **a, const struct dirent **b)
{
    return strcoll((*a)->d_name, (*b)->d_name);
}

char *rindex(const char *s, int c)
{
    if (s == NULL) {
        return NULL;
    }

    /* Don't bother tracing - strrchr can do that */
    return (char *)strrchr(s, c);
}

static char *ls_get_fullpath(const char *path, struct dirent *pdirent)
{
    char *fullpath = NULL;
    int ret;

    if (path[1] != '\0') {
        /* 2, The position of the path character: / and the end character '/0' */
        fullpath = (char *)malloc(strlen(path) + strlen(pdirent->d_name) + 2);
        if (fullpath == NULL) {
            goto exit_with_nomem;
        }

        /* 2, The position of the path character: / and the end character '/0' */
        ret = snprintf_s(fullpath, strlen(path) + strlen(pdirent->d_name) + 2,
                         strlen(path) + strlen(pdirent->d_name) + 1, "%s/%s", path, pdirent->d_name);
        if (ret < 0) {
            free(fullpath);
            set_errno(ENAMETOOLONG);
            return NULL;
        }
    } else {
        /* 2, The position of the path character: / and the end character '/0' */
        fullpath = (char *)malloc(strlen(pdirent->d_name) + 2);
        if (fullpath == NULL) {
            goto exit_with_nomem;
        }

        /* 2, The position of the path character: / and the end character '/0' */
        ret = snprintf_s(fullpath, strlen(pdirent->d_name) + 2, strlen(pdirent->d_name) + 1,
                         "/%s", pdirent->d_name);
        if (ret < 0) {
            free(fullpath);
            set_errno(ENAMETOOLONG);
            return NULL;
        }
    }
    return fullpath;

exit_with_nomem:
    set_errno(ENOSPC);
    return (char *)NULL;
}

static void PrintFileInfo64(const struct stat64 *stat64Info, const char *name, const char *linkName)
{
    mode_t mode;
    char str[UGO_NUMS][UGO_NUMS + 1] = {0};
    char dirFlag;
    int i;

    for (i = 0; i < UGO_NUMS; i++) {
        mode = stat64Info->st_mode >> (uint)(USER_MODE_SHIFT - i * UGO_NUMS);
        str[i][0] = (mode & READ_OP) ? 'r' : '-';
        str[i][1] = (mode & WRITE_OP) ? 'w' : '-';
        str[i][UGO_NUMS - 1] = (mode & EXEC_OP) ? 'x' : '-';
    }

    if (S_ISDIR(stat64Info->st_mode)) {
        dirFlag = 'd';
    } else if (S_ISLNK(stat64Info->st_mode)) {
        dirFlag = 'l';
    } else {
        dirFlag = '-';
    }

    if (S_ISLNK(stat64Info->st_mode)) {
        PRINTK("%c%s%s%s %-8lld u:%-5d g:%-5d %-10s -> %s\n", dirFlag,
               str[0], str[1], str[UGO_NUMS - 1], stat64Info->st_size,
               stat64Info->st_uid, stat64Info->st_gid, name, linkName);
    } else {
        PRINTK("%c%s%s%s %-8lld u:%-5d g:%-5d %-10s\n", dirFlag,
               str[0], str[1], str[UGO_NUMS - 1], stat64Info->st_size,
               stat64Info->st_uid, stat64Info->st_gid, name);
    }
}

static void PrintFileInfo(const struct stat *statInfo, const char *name, const char *linkName)
{
    mode_t mode;
    char str[UGO_NUMS][UGO_NUMS + 1] = {0};
    char dirFlag;
    int i;

    for (i = 0; i < UGO_NUMS; i++) {
        mode = statInfo->st_mode >> (uint)(USER_MODE_SHIFT - i * UGO_NUMS);
        str[i][0] = (mode & READ_OP) ? 'r' : '-';
        str[i][1] = (mode & WRITE_OP) ? 'w' : '-';
        str[i][UGO_NUMS - 1] = (mode & EXEC_OP) ? 'x' : '-';
    }

    if (S_ISDIR(statInfo->st_mode)) {
        dirFlag = 'd';
    } else if (S_ISLNK(statInfo->st_mode)) {
        dirFlag = 'l';
    } else {
        dirFlag = '-';
    }

    if (S_ISLNK(statInfo->st_mode)) {
        PRINTK("%c%s%s%s %-8lld u:%-5d g:%-5d %-10s -> %s\n", dirFlag,
               str[0], str[1], str[UGO_NUMS - 1], statInfo->st_size,
               statInfo->st_uid, statInfo->st_gid, name, linkName);
    } else {
        PRINTK("%c%s%s%s %-8lld u:%-5d g:%-5d %-10s\n", dirFlag,
               str[0], str[1], str[UGO_NUMS - 1], statInfo->st_size,
               statInfo->st_uid, statInfo->st_gid, name);
    }
}

int LsFile(const char *path)
{
    struct stat64 stat64Info;
    struct stat statInfo;
    char linkName[NAME_MAX] = { 0 };

    if (stat64(path, &stat64Info) == 0) {
        if (S_ISLNK(stat64Info.st_mode)) {
            readlink(path, linkName, NAME_MAX);
        }
        PrintFileInfo64(&stat64Info, path, (const char *)linkName);
    } else if (stat(path, &statInfo) == 0) {
        if (S_ISLNK(statInfo.st_mode)) {
            readlink(path, linkName, NAME_MAX);
        }
        PrintFileInfo(&statInfo, path, (const char *)linkName);
    } else {
        return -1;
    }

    return 0;
}

int LsDir(const char *path)
{
    struct stat statInfo = { 0 };
    struct stat64 stat64Info = { 0 };
    char linkName[NAME_MAX] = { 0 };
    DIR *d = NULL;
    char *fullpath = NULL;
    char *fullpath_bak = NULL;
    struct dirent *pdirent = NULL;

    d = opendir(path);
    if (d == NULL) {
        return -1;
    }

    PRINTK("Directory %s:\n", path);
    do {
        pdirent = readdir(d);
        if (pdirent == NULL) {
            break;
        }
        if (!strcmp(pdirent->d_name, ".") || !strcmp(pdirent->d_name, "..")) {
            continue;
        }
        (void)memset_s(&statInfo, sizeof(struct stat), 0, sizeof(struct stat));
        (void)memset_s(&stat64Info, sizeof(struct stat), 0, sizeof(struct stat));
        (void)memset_s(&linkName, sizeof(linkName), 0, sizeof(linkName));
        fullpath = ls_get_fullpath(path, pdirent);
        if (fullpath == NULL) {
            (void)closedir(d);
            return -1;
        }

        fullpath_bak = fullpath;
        if (stat64(fullpath, &stat64Info) == 0) {
            if (S_ISLNK(stat64Info.st_mode)) {
                readlink(fullpath, linkName, NAME_MAX);
            }
            PrintFileInfo64(&stat64Info, pdirent->d_name, linkName);
        } else if (stat(fullpath, &statInfo) == 0) {
            if (S_ISLNK(statInfo.st_mode)) {
                readlink(fullpath, linkName, NAME_MAX);
            }
            PrintFileInfo(&statInfo, pdirent->d_name, linkName);
        } else {
            PRINTK("BAD file: %s\n", pdirent->d_name);
        }
        free(fullpath_bak);
    } while (1);
    (void)closedir(d);

    return 0;
}

void ls(const char *pathname)
{
    struct stat statInfo = { 0 };
    char *path = NULL;
    int ret;

    if (pathname == NULL) {
#ifdef VFS_USING_WORKDIR
        UINTPTR lock_flags;
        LosProcessCB *curr = OsCurrProcessGet();

        /* open current working directory */

        spin_lock_irqsave(&curr->files->workdir_lock, lock_flags);
        path = strdup(curr->files->workdir);
        spin_unlock_irqrestore(&curr->files->workdir_lock, lock_flags);
#else
        path = strdup("/");
#endif
        if (path == NULL) {
            return;
        }
    } else {
        ret = vfs_normalize_path(NULL, pathname, &path);
        if (ret < 0) {
            set_errno(-ret);
            return;
        }
    }

    ret = stat(path, &statInfo);
    if (ret < 0) {
        perror("ls error");
        free(path);
        return;
    }

    if (statInfo.st_mode & S_IFDIR) { /* list all directory and file */
        ret = LsDir((pathname == NULL) ? path : pathname);
    } else { /* show the file infomation */
        ret = LsFile(path);
    }
    if (ret < 0) {
        perror("ls error");
    }

    free(path);
    return;
}

//
char *realpath(const char *path, char *resolved_path)
{
    int ret, result;
    char *new_path = NULL;
    struct stat buf;

    ret = vfs_normalize_path(NULL, path, &new_path);
    if (ret < 0) {
        ret = -ret;
        set_errno(ret);
        return NULL;
    }

    result = stat(new_path, &buf);//获取统计信息,即属性信息

    if (resolved_path == NULL) {
        if (result != ENOERR) {
            free(new_path);
            return NULL;
        }
        return new_path;
    }

    ret = strcpy_s(resolved_path, PATH_MAX, new_path);
    if (ret != EOK) {
        ret = -ret;
        set_errno(ret);
        free(new_path);
        return NULL;
    }

    free(new_path);
    if (result != ENOERR) {
        return NULL;
    }
    return resolved_path;
}
///查看FD
void lsfd(void)
{
    struct filelist *f_list = NULL;
    unsigned int i = 3; /* file start fd */
    int ret;
    struct Vnode *node = NULL;

    f_list = &tg_filelist;

    PRINTK("   fd    filename\n");
    ret = sem_wait(&f_list->fl_sem);
    if (ret < 0) {
        PRINTK("sem_wait error, ret=%d\n", ret);
        return;
    }

    while (i < CONFIG_NFILE_DESCRIPTORS) {
        node = files_get_openfile(i);//bitmap对应位 为 1的
        if (node) {
            PRINTK("%5d   %s\n", i, f_list->fl_files[i].f_path);//打印fd 和 路径
        }
        i++;
    }
    (void)sem_post(&f_list->fl_sem);
}
///获取用户创建文件掩码
mode_t GetUmask(void)
{
    return OsCurrProcessGet()->umask;
}
///给当前进程设置系统创建文件掩码,并返回进程旧掩码
mode_t SysUmask(mode_t mask)
{
    UINT32 intSave;
    mode_t umask;
    mode_t oldUmask;
    umask = mask & UMASK_FULL;
    SCHEDULER_LOCK(intSave);
    oldUmask = OsCurrProcessGet()->umask;
    OsCurrProcessGet()->umask = umask;
    SCHEDULER_UNLOCK(intSave);
    return oldUmask;
}

#ifdef LOSCFG_CHROOT
int chroot(const char *path)
{
    int ret;
    struct Vnode *vnode = NULL;

    if (!path) {
        set_errno(EFAULT);
        return VFS_ERROR;
    }

    if (!strlen(path)) {
        set_errno(ENOENT);
        return VFS_ERROR;
    }

    if (strlen(path) > PATH_MAX) {
        set_errno(ENAMETOOLONG);
        return VFS_ERROR;
    }
    VnodeHold();
    ret = VnodeLookup(path, &vnode, 0);
    if (ret != LOS_OK) {
        VnodeDrop();
        return ret;
    }

    LosProcessCB *curr = OsCurrProcessGet();
    if ((curr->files == NULL) || (curr->files->rootVnode == NULL)) {
        VnodeDrop();
        return VFS_ERROR;
    }
    if (curr->files->rootVnode->useCount > 0) {
        curr->files->rootVnode->useCount--;
    }
    vnode->useCount++;
    curr->files->rootVnode = vnode;

    VnodeDrop();
    return LOS_OK;
}
#endif
