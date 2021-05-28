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

#include "fatfs.h"
#ifdef LOSCFG_FS_FAT
#include "ff.h"
#include "fs/vfs_util.h"
#include "disk_pri.h"
#include "diskio.h"
#include "fs/fs.h"
#include "fs/dirent_fs.h"
#include "fs_other.h"
#include "fs/mount.h"
#include "fs/vnode.h"
#include "fs/path_cache.h"
#ifdef LOSCFG_FS_FAT_VIRTUAL_PARTITION
#include "virpartff.h"
#include "errcode_fat.h"
#endif
#include "los_tables.h"
#include "user_copy.h"
#include "los_vm_filemap.h"
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>


struct VnodeOps fatfs_vops; /* forward define */
struct file_operations_vfs fatfs_fops;

#define BITMASK4 0x0F
#define BITMASK5 0x1F
#define BITMASK6 0x3F
#define BITMASK7 0x7F
#define FTIME_MIN_OFFSET 6 /* minute offset in word */
#define FTIME_HR_OFFSET 11 /* hour offset in word */
#define FTIME_MTH_OFFSET 5 /* month offset in word */
#define FTIME_YEAR_OFFSET 9 /* year offset in word */
#define FTIME_DATE_OFFSET 16 /* date offset in dword */
#define SEC_MULTIPLIER 2
#define YEAR_OFFSET 80 /* Year start from 1980 in FATFS, while start from 1900 in struct tm */

int fatfs_2_vfs(int result)
{
    int status = ENOERR;

#ifdef LOSCFG_FS_FAT_VIRTUAL_PARTITION
    if (result < 0 || result >= VIRERR_BASE) {
        return result;
    }
#else
    if (result < 0) {
        return result;
    }
#endif

    /* FatFs errno to Libc errno */
    switch (result) {
        case FR_OK:
            break;

        case FR_NO_FILE:
        case FR_NO_PATH:
            status = ENOENT;
            break;

        case FR_NO_FILESYSTEM:
            status = ENOTSUP;
            break;

        case FR_INVALID_NAME:
            status = EINVAL;
            break;

        case FR_EXIST:
        case FR_INVALID_OBJECT:
            status = EEXIST;
            break;

        case FR_DISK_ERR:
        case FR_NOT_READY:
        case FR_INT_ERR:
            status = EIO;
            break;

        case FR_WRITE_PROTECTED:
            status = EROFS;
            break;
        case FR_MKFS_ABORTED:
        case FR_INVALID_PARAMETER:
            status = EINVAL;
            break;

        case FR_NO_SPACE_LEFT:
            status = ENOSPC;
            break;
        case FR_NO_DIRENTRY:
            status = ENFILE;
            break;
        case FR_NO_EMPTY_DIR:
            status = ENOTEMPTY;
            break;
        case FR_IS_DIR:
            status = EISDIR;
            break;
        case FR_NO_DIR:
            status = ENOTDIR;
            break;
        case FR_NO_EPERM:
        case FR_DENIED:
            status = EPERM;
            break;
        case FR_LOCKED:
        case FR_TIMEOUT:
            status = EBUSY;
            break;
#ifdef LOSCFG_FS_FAT_VIRTUAL_PARTITION
        case FR_MODIFIED:
            status = -VIRERR_MODIFIED;
            break;
        case FR_CHAIN_ERR:
            status = -VIRERR_CHAIN_ERR;
            break;
        case FR_OCCUPIED:
            status = -VIRERR_OCCUPIED;
            break;
        case FR_NOTCLEAR:
            status = -VIRERR_NOTCLEAR;
            break;
        case FR_NOTFIT:
            status = -VIRERR_NOTFIT;
            break;
        case FR_INVAILD_FATFS:
            status = -VIRERR_INTER_ERR;
            break;
#endif
        default:
            status = -FAT_ERROR;
            break;
    }

    return status;
}

static bool fatfs_is_last_cluster(FATFS *fs, DWORD cclust)
{
    switch (fs->fs_type) {
        case FS_FAT12:
            return (cclust == FAT12_END_OF_CLUSTER);
        case FS_FAT16:
            return (cclust == FAT16_END_OF_CLUSTER);
        case FS_FAT32:
        default:
            return (cclust == FAT32_END_OF_CLUSTER);
    }
}

static int fatfs_sync(unsigned long mountflags, FATFS *fs)
{
#ifdef LOSCFG_FS_FAT_CACHE
    los_part *part = NULL;
    if (mountflags != MS_NOSYNC) {
        part = get_part((INT)fs->pdrv);
        if (part == NULL) {
            return -ENODEV;
        }

        (void)OsSdSync(part->disk_id);
    }
#endif
    return 0;
}
int fatfs_hash_cmp(struct Vnode *vp, void *arg)
{
    DIR_FILE *dfp_target = (DIR_FILE *)arg;
    DIR_FILE *dfp = (DIR_FILE *)vp->data;

    return dfp_target->f_dir.sect != dfp->f_dir.sect ||
              dfp_target->f_dir.dptr != dfp->f_dir.dptr ||
              dfp_target->fno.sclst != dfp->fno.sclst;
}

static DWORD fatfs_hash(QWORD sect, DWORD dptr, DWORD sclst)
{
    DWORD hash = FNV1_32_INIT;
    hash = fnv_32_buf(&sect, sizeof(QWORD), hash);
    hash = fnv_32_buf(&dptr, sizeof(DWORD), hash);
    hash = fnv_32_buf(&sclst, sizeof(DWORD), hash);

    return hash;
}

static mode_t fatfs_get_mode(BYTE attribute, mode_t fs_mode)
{
    mode_t mask = 0;
    if (attribute & AM_RDO) {
        mask = S_IWUSR | S_IWGRP | S_IWOTH;
    }
    fs_mode &= ~mask;
    if (attribute & AM_DIR) {
        fs_mode |= S_IFDIR;
    } else {
        fs_mode |= S_IFREG;
    }
    return fs_mode;
}

int fatfs_lookup(struct Vnode *parent, const char *path, int len, struct Vnode **vpp)
{
    struct Vnode *vp = NULL;
    FATFS *fs = (FATFS *)(parent->originMount->data);
    DIR_FILE *dfp;
    DIR *dp = NULL;
    FILINFO *finfo = NULL;
    DWORD hash;
    FRESULT result;
    int ret;

    dfp = (DIR_FILE *)zalloc(sizeof(DIR_FILE));
    if (dfp == NULL) {
        ret = ENOMEM;
        goto ERROR_EXIT;
    }

    ret = lock_fs(fs);
    if (ret == FALSE) {
        ret = EBUSY;
        goto ERROR_FREE;
    }
    finfo = &(dfp->fno);
    LOS_ListInit(&finfo->fp_list);
    dp = &(dfp->f_dir);
    dp->obj.fs = fs;
    dp->obj.sclust = ((DIR_FILE *)(parent->data))->fno.sclst;

    DEF_NAMBUF;
    INIT_NAMBUF(fs);
    result = create_name(dp, &path);
    if (result != FR_OK) {
        ret = fatfs_2_vfs(result);
        goto ERROR_UNLOCK;
    }

    result = dir_find(dp);
    if (result != FR_OK) {
        ret = fatfs_2_vfs(result);
        goto ERROR_UNLOCK;
    }

    if (dp->fn[NSFLAG] & NS_NONAME) {
        result = FR_INVALID_NAME;
        ret = fatfs_2_vfs(result);
        goto ERROR_UNLOCK;
    }

    get_fileinfo(dp, finfo);
    dp->obj.objsize = 0;

    hash = fatfs_hash(dp->sect, dp->dptr, finfo->sclst);
    ret = VfsHashGet(parent->originMount, hash, &vp, fatfs_hash_cmp, dfp);
    if (ret != 0) {
        ret = VnodeAlloc(&fatfs_vops, &vp);
        if (ret != 0) {
            ret = ENOMEM;
            result = FR_NOT_ENOUGH_CORE;
            goto ERROR_UNLOCK;
        }
        vp->parent = parent;
        vp->fop = &fatfs_fops;
        vp->data = dfp;
        vp->originMount = parent->originMount;
        vp->uid = fs->fs_uid;
        vp->gid = fs->fs_gid;
        vp->mode = fatfs_get_mode(finfo->fattrib, fs->fs_mode);
        if (finfo->fattrib & AM_DIR) {
            vp->type = VNODE_TYPE_DIR;
        } else {
            vp->type = VNODE_TYPE_REG;
        }

        ret = VfsHashInsert(vp, hash);
        if (ret != 0) {
            result = FR_INVALID_PARAMETER;
            goto ERROR_UNLOCK;
        }
    } else {
        vp->parent = parent;
        free(dfp); /* hash hit dfp is no needed */
    }

    unlock_fs(fs, FR_OK);
    FREE_NAMBUF();
    *vpp = vp;
    return 0;

ERROR_UNLOCK:
    unlock_fs(fs, result);
    FREE_NAMBUF();
ERROR_FREE:
    free(dfp);
ERROR_EXIT:
    return -ret;
}

int fatfs_create(struct Vnode *parent, const char *name, int mode, struct Vnode **vpp)
{
    struct Vnode *vp = NULL;
    FATFS *fs = (FATFS *)parent->originMount->data;
    DIR_FILE *dfp;
    DIR *dp = NULL;
    FILINFO *finfo = NULL;
    QWORD time;
    DWORD hash;
    FRESULT result;
    int ret;

    dfp = (DIR_FILE *)zalloc(sizeof(DIR_FILE));
    if (dfp == NULL) {
        ret = ENOMEM;
        goto ERROR_EXIT;
    }
    ret = lock_fs(fs);
    if (ret == FALSE) { /* lock failed */
        ret = EBUSY;
        goto ERROR_FREE;
    }

    finfo = &(dfp->fno);
    LOS_ListInit(&finfo->fp_list);
    dp = &(dfp->f_dir);
    dp->obj.fs = fs;
    dp->obj.sclust = ((DIR_FILE *)(parent->data))->fno.sclst;

    DEF_NAMBUF;
    INIT_NAMBUF(fs);
    result = create_name(dp, &name);
    if (result != FR_OK) {
        ret = fatfs_2_vfs(result);
        goto ERROR_UNLOCK;
    }
    result = dir_find(dp);
    if (result == FR_OK) {
        ret = EEXIST;
        goto ERROR_UNLOCK;
    }
    result = dir_register(dp);
    if (result != FR_OK) {
        ret = fatfs_2_vfs(result);
        goto ERROR_UNLOCK;
    }
    /* Set the directory entry attribute */
    if (time_status == SYSTEM_TIME_ENABLE) {
        time = GET_FATTIME();
    } else {
        time = 0;
    }
    st_dword(dp->dir + DIR_CrtTime, time);
    st_dword(dp->dir + DIR_ModTime, time);
    st_word(dp->dir + DIR_LstAccDate, time >> FTIME_DATE_OFFSET);
    dp->dir[DIR_Attr] = AM_ARC;
    if (((DWORD)mode & S_IWUSR) == 0) {
        dp->dir[DIR_Attr] |= AM_RDO;
    }
    st_clust(fs, dp->dir, 0);
    st_dword(dp->dir + DIR_FileSize, 0);

#ifdef LOSCFG_FS_FAT_VIRTUAL_PARTITION
    PARENTFS(fs)->wflag = 1;
#else
    fs->wflag = 1;
#endif
    result = sync_fs(fs);
    if (result != FR_OK) {
        ret = fatfs_2_vfs(result);
        goto ERROR_UNLOCK;
    }
    result = dir_read(dp, 0);
    if (result != FR_OK) {
        ret = fatfs_2_vfs(result);
        goto ERROR_UNLOCK;
    }
    dp->blk_ofs = dir_ofs(dp);
    get_fileinfo(dp, finfo);
    dp->obj.objsize = 0;

    ret = VnodeAlloc(&fatfs_vops, &vp);
    if (ret != 0) {
        ret = ENOMEM;
        goto ERROR_UNLOCK;
    }

    vp->parent = parent;
    vp->fop = &fatfs_fops;
    vp->data = dfp;
    vp->originMount = parent->originMount;
    vp->uid = fs->fs_uid;
    vp->gid = fs->fs_gid;
    vp->mode = fatfs_get_mode(finfo->fattrib, fs->fs_mode);
    vp->type = VNODE_TYPE_REG;

    hash = fatfs_hash(dp->sect, dp->dptr, finfo->sclst);
    ret = VfsHashInsert(vp, hash);
    if (ret != 0) {
        ret = EINVAL;
        goto ERROR_UNLOCK;
    }
    *vpp = vp;

    unlock_fs(fs, result);
    FREE_NAMBUF();
    return fatfs_sync(parent->originMount->mountFlags, fs);

ERROR_UNLOCK:
    unlock_fs(fs, result);
    FREE_NAMBUF();
ERROR_FREE:
    free(dfp);
ERROR_EXIT:
    return -ret;
}

int fatfs_open(struct file *filep)
{
    struct Vnode *vp = filep->f_vnode;
    FATFS *fs = (FATFS *)vp->originMount->data;
    DIR_FILE *dfp = (DIR_FILE *)vp->data;
    DIR *dp = &(dfp->f_dir);
    FILINFO *finfo = &(dfp->fno);
    FIL *fp;
    int ret;

    fp = (FIL *)zalloc(sizeof(FIL));
    if (fp == NULL) {
        ret = ENOMEM;
        goto ERROR_EXIT;
    }
    ret = lock_fs(fs);
    if (ret == FALSE) {
        ret = EBUSY;
        goto ERROR_EXIT;
    }

    fp->dir_sect = dp->sect;
    fp->dir_ptr = dp->dir;
    fp->obj.sclust = finfo->sclst;
    fp->obj.objsize = finfo->fsize;
#if FF_USE_FASTSEEK
    fp->cltbl = 0; /* Disable fast seek mode */
#endif
    fp->obj.fs = fs;
    fp->obj.id = fs->id;
    fp->flag = FA_READ | FA_WRITE;
    fp->err = 0;
    fp->sect = 0;
    fp->fptr = 0;
    fp->buf = (BYTE*) ff_memalloc(SS(fs));
    if (fp->buf == NULL) {
        ret = ENOMEM;
        goto ERROR_FREE;
    }
    LOS_ListAdd(&finfo->fp_list, &fp->fp_entry);
    unlock_fs(fs, FR_OK);

    filep->f_priv = fp;
    return fatfs_sync(vp->originMount->mountFlags, fs);

ERROR_FREE:
    unlock_fs(fs, FR_OK);
    free(fp);
ERROR_EXIT:
    return -ret;
}

int fatfs_close(struct file *filep)
{
    FIL *fp = (FIL *)filep->f_priv;
    FATFS *fs = fp->obj.fs;
    FRESULT result;
    int ret;

    ret = lock_fs(fs);
    if (ret == FALSE) {
        return -EBUSY;
    }
#if !FF_FS_READONLY
    result = f_sync(fp); /* Flush cached data */
    if (result != FR_OK) {
        goto EXIT;
    }
    ret = fatfs_sync(filep->f_vnode->originMount->mountFlags, fs);
    if (ret != 0) {
        unlock_fs(fs, FR_OK);
        return ret;
    }
#endif
    LOS_ListDelete(&fp->fp_entry);
    ff_memfree(fp->buf);
    free(fp);
    filep->f_priv = NULL;
EXIT:
    unlock_fs(fs, result);
    return -fatfs_2_vfs(result);
}

int fatfs_read(struct file *filep, char *buff, size_t count)
{
    FIL *fp = (FIL *)filep->f_priv;
    FATFS *fs = fp->obj.fs;
    struct Vnode *vp = filep->f_vnode;
    FILINFO *finfo = &((DIR_FILE *)(vp->data))->fno;
    size_t rcount;
    FRESULT result;
    int ret;

    ret = lock_fs(fs);
    if (ret == FALSE) {
        return -EBUSY;
    }
    fp->obj.objsize = finfo->fsize;
    fp->obj.sclust = finfo->sclst;
    result = f_read(fp, buff, count, &rcount);
    if (result != FR_OK) {
        goto EXIT;
    }
    filep->f_pos = fp->fptr;
EXIT:
    unlock_fs(fs, result);
    return rcount;
}

static FRESULT update_dir(DIR *dp, FILINFO *finfo)
{
    FATFS *fs = dp->obj.fs;
    DWORD tm;
    BYTE *dbuff = NULL;
    FRESULT result;

    result = move_window(fs, dp->sect);
    if (result != FR_OK) {
        return result;
    }
    dbuff = fs->win + dp->dptr % SS(fs);
    dbuff[DIR_Attr] = finfo->fattrib;
    st_clust(fs, dbuff, finfo->sclst); /* Update start cluster */
    st_dword(dbuff + DIR_FileSize, (DWORD)finfo->fsize); /* Update file size */
    if (time_status == SYSTEM_TIME_ENABLE) {
        tm = GET_FATTIME();
    } else {
        tm = 0;
    }
    st_dword(dbuff + DIR_ModTime, tm); /* Update mtime */
    st_word(dbuff + DIR_LstAccDate, tm >> FTIME_DATE_OFFSET); /* Update access date */
#ifndef LOSCFG_FS_FAT_VIRTUAL_PARTITION
    fs->wflag = 1;
#else
    PARENTFS(fs)->wflag = 1;
#endif
    return sync_fs(fs);
}

off64_t fatfs_lseek64(struct file *filep, off64_t offset, int whence)
{
    FIL *fp = (FIL *)filep->f_priv;
    FATFS *fs = fp->obj.fs;
    struct Vnode *vp = filep->f_vnode;
    DIR_FILE *dfp = (DIR_FILE *)vp->data;
    FILINFO *finfo = &(dfp->fno);
    FSIZE_t fpos;
    FRESULT result;
    int ret;

    switch (whence) {
        case SEEK_CUR:
            offset = filep->f_pos + offset;
            if (offset < 0) {
                return -EINVAL;
            }
            fpos = offset;
            break;
        case SEEK_SET:
            if (offset < 0) {
                return -EINVAL;
            }
            fpos = offset;
            break;
        case SEEK_END:
            offset = (off_t)((long long)finfo->fsize + offset);
            if (offset < 0) {
                return -EINVAL;
            }
            fpos = offset;
            break;
        default:
            return -EINVAL;
    }

    if (offset >= FAT32_MAXSIZE) {
        return -EINVAL;
    }

    ret = lock_fs(fs);
    if (ret == FALSE) {
        return -EBUSY;
    }
    fp->obj.sclust = finfo->sclst;
    fp->obj.objsize = finfo->fsize;

    result = f_lseek(fp, fpos);
    finfo->fsize = fp->obj.objsize;
    finfo->sclst = fp->obj.sclust;
    if (result != FR_OK) {
        goto ERROR_EXIT;
    }

    result = f_sync(fp);
    if (result != FR_OK) {
        goto ERROR_EXIT;
    }
    filep->f_pos = fpos;

    unlock_fs(fs, FR_OK);
    return fpos;
ERROR_EXIT:
    unlock_fs(fs, result);
    return -fatfs_2_vfs(result);
}

off_t fatfs_lseek(struct file *filep, off_t offset, int whence)
{
    return (off_t)fatfs_lseek64(filep, offset, whence);
}

static int update_filbuff(FILINFO *finfo, FIL *wfp, const char  *data)
{
    LOS_DL_LIST *list = &finfo->fp_list;
    FATFS *fs = wfp->obj.fs;
    FIL *entry = NULL;
    int ret = 0;

    LOS_DL_LIST_FOR_EACH_ENTRY(entry, list, FIL, fp_entry) {
        if (entry == wfp) {
            continue;
        }
        if (entry->sect != 0) {
            if (disk_read(fs->pdrv, entry->buf, entry->sect, 1) != RES_OK) {
                ret = -1;
            }
        }
    }

    return ret;
}

int fatfs_write(struct file *filep, const char *buff, size_t count)
{
    FIL *fp = (FIL *)filep->f_priv;
    FATFS *fs = fp->obj.fs;
    struct Vnode *vp = filep->f_vnode;
    FILINFO *finfo = &(((DIR_FILE *)vp->data)->fno);
    size_t wcount;
    FRESULT result;
    int ret;

    ret = lock_fs(fs);
    if (ret == FALSE) {
        return -EBUSY;
    }
    fp->obj.objsize = finfo->fsize;
    fp->obj.sclust = finfo->sclst;
    result = f_write(fp, buff, count, &wcount);
    if (result != FR_OK) {
        goto ERROR_EXIT;
    }

    finfo->fsize = fp->obj.objsize;
    finfo->sclst = fp->obj.sclust;
    result = f_sync(fp);
    if (result != FR_OK) {
        goto ERROR_EXIT;
    }
    update_filbuff(finfo, fp, buff);

    filep->f_pos = fp->fptr;

    unlock_fs(fs, FR_OK);
    return wcount;
ERROR_EXIT:
    unlock_fs(fs, result);
    return -fatfs_2_vfs(result);
}

int fatfs_fsync(struct file *filep)
{
    FIL *fp = filep->f_priv;
    FATFS *fs = fp->obj.fs;
    FRESULT result;
    int ret;

    ret = lock_fs(fs);
    if (ret == FALSE) {
        return -EBUSY;
    }

    result = f_sync(fp);
    unlock_fs(fs, result);
    return -fatfs_2_vfs(result);
}

int fatfs_fallocate64(struct file *filep, int mode, off64_t offset, off64_t len)
{
    FIL *fp = (FIL *)filep->f_priv;
    FATFS *fs = fp->obj.fs;
    struct Vnode *vp = filep->f_vnode;
    FILINFO *finfo = &((DIR_FILE *)(vp->data))->fno;
    FRESULT result;
    int ret;

    if (offset < 0 || len <= 0) {
        return -EINVAL;
    }

    if (len >= FAT32_MAXSIZE || offset >= FAT32_MAXSIZE ||
        len + offset >= FAT32_MAXSIZE) {
        return -EINVAL;
    }

    if (mode != FALLOC_FL_KEEP_SIZE) {
        return -EINVAL;
    }

    ret = lock_fs(fs);
    if (ret == FALSE) {
        return -EBUSY;
    }
    result = f_expand(fp, (FSIZE_t)offset, (FSIZE_t)len, 1);
    if (result == FR_OK && finfo->sclst == 0) {
        finfo->sclst = fp->obj.sclust;
    }
    result = f_sync(fp);
    unlock_fs(fs, FR_OK);

    return -fatfs_2_vfs(result);
}

static FRESULT realloc_cluster(FILINFO *finfo, FFOBJID *obj, FSIZE_t size)
{
    FATFS *fs = obj->fs;
    off64_t remain;
    DWORD cclust;
    DWORD pclust;
    QWORD csize;
    FRESULT result;

    if (size == 0) { /* Remove cluster chain */
        if (finfo->sclst != 0) {
            result = remove_chain(obj, finfo->sclst, 0);
            if (result != FR_OK) {
                return result;
            }
            finfo->sclst = 0;
        }
        return FR_OK;
    }

    remain = size;
    csize = SS(fs) * fs->csize;
    if (finfo->sclst == 0) { /* Allocate one cluster if file doesn't have any cluster */
        cclust = create_chain(obj, 0);
        if (cclust == 0) {
            return FR_NO_SPACE_LEFT;
        }
        if (cclust == 1 || cclust == DISK_ERROR) {
            return FR_DISK_ERR;
        }
        finfo->sclst = cclust;
    }
    cclust = finfo->sclst;
    while (remain > csize) { /* Follow or strentch the cluster chain */
        pclust = cclust;
        cclust = create_chain(obj, pclust);
        if (cclust == 0) {
            return FR_NO_SPACE_LEFT;
        }
        if (cclust == 1 || cclust == DISK_ERROR) {
            return FR_DISK_ERR;
        }
        remain -= csize;
    }
    pclust = cclust;
    cclust = get_fat(obj, pclust);
    if ((cclust == BAD_CLUSTER) || (cclust == DISK_ERROR)) {
        return FR_DISK_ERR;
    }
    if (!fatfs_is_last_cluster(obj->fs, cclust)) { /* Remove extra cluster if existing */
        result = remove_chain(obj, cclust, pclust);
        if (result != FR_OK) {
            return result;
        }
    }

    return FR_OK;
}

int fatfs_fallocate(struct file *filep, int mode, off_t offset, off_t len)
{
    return fatfs_fallocate64(filep, mode, offset, len);
}

int fatfs_truncate64(struct Vnode *vp, off64_t len)
{
    FATFS *fs = (FATFS *)vp->originMount->data;
    DIR_FILE *dfp = (DIR_FILE *)vp->data;
    DIR *dp = &(dfp->f_dir);
    FILINFO *finfo = &(dfp->fno);
    FFOBJID object;
    FRESULT result = FR_OK;
    int ret;

    if (len < 0 || len >= FAT32_MAXSIZE) {
        return -EINVAL;
    }

    ret = lock_fs(fs);
    if (ret == FALSE) {
        result = FR_TIMEOUT;
        goto ERROR_OUT;
    }
    if (len == finfo->fsize) {
        unlock_fs(fs, FR_OK);
        return 0;
    }

    object.fs = fs;
    result = realloc_cluster(finfo, &object, (FSIZE_t)len);
    if (result != FR_OK) {
        goto ERROR_UNLOCK;
    }
    finfo->fsize = (FSIZE_t)len;

    result = update_dir(dp, finfo);
    if (result != FR_OK) {
        goto ERROR_UNLOCK;
    }
    unlock_fs(fs, FR_OK);
    return fatfs_sync(vp->originMount->mountFlags, fs);
ERROR_UNLOCK:
    unlock_fs(fs, result);
ERROR_OUT:
    return -fatfs_2_vfs(result);
}

int fatfs_truncate(struct Vnode *vp, off_t len)
{
    return fatfs_truncate64(vp, len);
}

static int fat_bind_check(struct Vnode *blk_driver, los_part **partition)
{
    los_part *part = NULL;

    if (blk_driver == NULL || blk_driver->data == NULL) {
        return ENODEV;
    }

    struct drv_data *dd = blk_driver->data;
    if (dd->ops == NULL) {
        return ENODEV;
    }
    const struct block_operations *bops = dd->ops;
    if (bops->open == NULL) {
        return EINVAL;
    }
    if (bops->open(blk_driver) < 0) {
        return EBUSY;
    }

    part = los_part_find(blk_driver);
    if (part == NULL) {
        return ENODEV;
    }
    if (part->part_name != NULL) {
        bops->close(blk_driver);
        return EBUSY;
    }

#ifndef FF_MULTI_PARTITION
    if (part->part_no_mbr > 1) {
        bops->close(blk_driver);
        return EPERM;
    }
#endif

    *partition = part;
    return 0;
}

int fatfs_mount(struct Mount *mnt, struct Vnode *blk_device, const void *data)
{
    struct Vnode *vp = NULL;
    FATFS *fs = NULL;
    DIR_FILE *dfp = NULL;
    los_part *part = NULL;
    QWORD start_sector;
    BYTE fmt;
    DWORD hash;
    FRESULT result;
    int ret;

    ret = fat_bind_check(blk_device, &part);
    if (ret != 0) {
        goto ERROR_EXIT;
    }

    ret = SetDiskPartName(part, "vfat");
    if (ret != 0) {
        ret = EIO;
        goto ERROR_EXIT;
    }

    fs = (FATFS *)zalloc(sizeof(FATFS));
    if (fs == NULL) {
        ret = ENOMEM;
        goto ERROR_PARTNAME;
    }

#ifdef LOSCFG_FS_FAT_VIRTUAL_PARTITION
    fs->vir_flag = FS_PARENT;
    fs->parent_fs = fs;
    fs->vir_amount = DISK_ERROR;
    fs->vir_avail = FS_VIRDISABLE;
#endif

    ret = ff_cre_syncobj(0, &fs->sobj);
    if (ret == 0) { /* create sync object failed */
        ret = EINVAL;
        goto ERROR_WITH_FS;
    }

    ret = lock_fs(fs);
    if (ret == FALSE) {
        ret = EBUSY;
        goto ERROR_WITH_MUX;
    }

    fs->fs_type = 0;
    fs->pdrv = part->part_id;

#if FF_MAX_SS != FF_MIN_SS  /* Get sector size (multiple sector size cfg only) */
    if (disk_ioctl(fs->pdrv, GET_SECTOR_SIZE, &(fs->ssize)) != RES_OK) {
        ret = EIO;
        goto ERROR_WITH_LOCK;
    }
    if (fs->ssize > FF_MAX_SS || fs->ssize < FF_MIN_SS || (fs->ssize & (fs->ssize - 1))) {
        ret = EIO;
        goto ERROR_WITH_LOCK;
    }
#endif

    fs->win = (BYTE *)ff_memalloc(SS(fs));
    if (fs->win == NULL) {
        ret = ENOMEM;
        goto ERROR_WITH_LOCK;
    }

    result = find_fat_partition(fs, part, &fmt, &start_sector);
    if (result != FR_OK) {
        ret = fatfs_2_vfs(result);
        goto ERROR_WITH_FSWIN;
    }

    result = init_fatobj(fs, fmt, start_sector);
    if (result != FR_OK) {
        ret = fatfs_2_vfs(result);
        goto ERROR_WITH_FSWIN;
    }

    fs->fs_uid = mnt->vnodeBeCovered->uid;
    fs->fs_gid = mnt->vnodeBeCovered->gid;
    fs->fs_dmask = GetUmask();
    fs->fs_fmask = GetUmask();
    fs->fs_mode = mnt->vnodeBeCovered->mode & (S_IRWXU | S_IRWXG | S_IRWXO);

    dfp = (DIR_FILE *)zalloc(sizeof(DIR_FILE));
    if (dfp == NULL) {
        ret = ENOMEM;
        goto ERROR_WITH_FSWIN;
    }

    dfp->f_dir.obj.fs = fs;
    dfp->f_dir.obj.sclust = 0; /* set start clust 0, root */
    dfp->f_dir.obj.attr = AM_DIR;
    dfp->f_dir.obj.objsize = 0; /* dir size is 0 */
    dfp->fno.fsize = 0;
    dfp->fno.fdate = 0;
    dfp->fno.ftime = 0;
    dfp->fno.fattrib = AM_DIR;
    dfp->fno.sclst = 0;
    dfp->fno.fname[0] = '/'; /* Mark as root dir */
    dfp->fno.fname[1] = '\0';
    LOS_ListInit(&(dfp->fno.fp_list));

    ret = VnodeAlloc(&fatfs_vops, &vp);
    if (ret != 0) {
        ret = ENOMEM;
        goto ERROR_WITH_FSWIN;
    }

    mnt->data = fs;
    mnt->vnodeCovered = vp;

    vp->parent = mnt->vnodeBeCovered;
    vp->fop = &fatfs_fops;
    vp->data = dfp;
    vp->originMount = mnt;
    vp->uid = fs->fs_uid;
    vp->gid = fs->fs_gid;
    vp->mode = mnt->vnodeBeCovered->mode;
    vp->type = VNODE_TYPE_DIR;

    hash = fatfs_hash(0, 0, 0);
    ret = VfsHashInsert(vp, hash);
    if (ret != 0) {
        ret = -ret;
        goto ERROR_WITH_LOCK;
    }
    unlock_fs(fs, FR_OK);

    return 0;

ERROR_WITH_FSWIN:
    ff_memfree(fs->win);
ERROR_WITH_LOCK:
    unlock_fs(fs, FR_OK);
ERROR_WITH_MUX:
    ff_del_syncobj(&fs->sobj);
ERROR_WITH_FS:
    free(fs);
ERROR_PARTNAME:
    if (part->part_name) {
        free(part->part_name);
        part->part_name = NULL;
    }
ERROR_EXIT:
    return -ret;
}

int fatfs_umount(struct Mount *mnt, struct Vnode **blkdriver)
{
    struct Vnode *device;
    FATFS *fs = (FATFS *)mnt->data;
    los_part *part;
    int ret;

    ret = lock_fs(fs);
    if (ret == FALSE) {
        return -EBUSY;
    }

    part = get_part(fs->pdrv);
    if (part == NULL) {
        unlock_fs(fs, FR_OK);
        return -ENODEV;
    }
    device = part->dev;
    if (device == NULL) {
        unlock_fs(fs, FR_OK);
        return -ENODEV;
    }
#ifdef LOSCFG_FS_FAT_CACHE
    ret = OsSdSync(part->disk_id);
    if (ret != 0) {
        unlock_fs(fs, FR_DISK_ERR);
        return -EIO;
    }
#endif
    if (part->part_name != NULL) {
        free(part->part_name);
        part->part_name = NULL;
    }

    struct drv_data *dd = device->data;
    if (dd->ops == NULL) {
        unlock_fs(fs, FR_OK);
        return ENODEV;
    }

    const struct block_operations *bops = dd->ops;
    if (bops != NULL && bops->close != NULL) {
        bops->close(*blkdriver);
    }

    if (fs->win != NULL) {
        ff_memfree(fs->win);
    }

    unlock_fs(fs, FR_OK);

    ret = ff_del_syncobj(&fs->sobj);
    if (ret == FALSE) {
        return -EINVAL;
    }
    free(fs);

    *blkdriver = device;

    return 0;
}

int fatfs_statfs(struct Mount *mnt, struct statfs *info)
{
    FATFS *fs = (FATFS *)mnt->data;
    DWORD nclst = 0;
    FRESULT result = FR_OK;
    int ret;

    info->f_type = MSDOS_SUPER_MAGIC;
#if FF_MAX_SS != FF_MIN_SS
    info->f_bsize = fs->ssize * fs->csize;
#else
    info->f_bsize = FF_MIN_SS * fs->csize;
#endif
    info->f_blocks = fs->n_fatent;
    ret = lock_fs(fs);
    if (ret == FALSE) {
        return -EBUSY;
    }
    /* free cluster is unavailable, update it */
    if (fs->free_clst == DISK_ERROR) {
        result = fat_count_free_entries(&nclst, fs);
    }
    info->f_bfree = fs->free_clst;
    info->f_bavail = fs->free_clst;
    unlock_fs(fs, result);

#if FF_USE_LFN
    /* Maximum length of filenames */
    info->f_namelen = FF_MAX_LFN;
#else
    /* Maximum length of filenames: 8 is the basename length, 1 is the dot, 3 is the extension length */
    info->f_namelen = (8 + 1 + 3);
#endif
    info->f_fsid.__val[0] = MSDOS_SUPER_MAGIC;
    info->f_fsid.__val[1] = 1;
    info->f_frsize = SS(fs) * fs->csize;
    info->f_files = 0;
    info->f_ffree = 0;
    info->f_flags = mnt->mountFlags;

    return -fatfs_2_vfs(result);
}

static inline int GET_SECONDS(WORD ftime)
{
    return (ftime & BITMASK5) * SEC_MULTIPLIER;
}
static inline int GET_MINUTES(WORD ftime)
{
    return (ftime >> FTIME_MIN_OFFSET) & BITMASK6;
}
static inline int GET_HOURS(WORD ftime)
{
    return (ftime >> FTIME_HR_OFFSET) & BITMASK5;
}
static inline int GET_DAY(WORD fdate)
{
    return fdate & BITMASK5;
}
static inline int GET_MONTH(WORD fdate)
{
    return (fdate >> FTIME_MTH_OFFSET) & BITMASK4;
}
static inline int GET_YEAR(WORD fdate)
{
    return (fdate >> FTIME_YEAR_OFFSET) & BITMASK7;
}

static time_t fattime_transfer(WORD fdate, WORD ftime)
{
    struct tm time = { 0 };
    time.tm_sec = GET_SECONDS(ftime);
    time.tm_min = GET_MINUTES(ftime);
    time.tm_hour = GET_HOURS(ftime);
    time.tm_mday = GET_DAY(fdate);
    time.tm_mon = GET_MONTH(fdate);
    time.tm_year = GET_YEAR(fdate) + YEAR_OFFSET; /* Year start from 1980 in FATFS */
    time_t ret = mktime(&time);
    return ret;
}

DWORD fattime_format(time_t time)
{
    struct tm st;
    DWORD ftime;

    localtime_r(&time, &st);

    ftime = (DWORD)st.tm_mday;
    ftime |= ((DWORD)st.tm_mon) << FTIME_MTH_OFFSET;
    ftime |= ((DWORD)((st.tm_year > YEAR_OFFSET) ? (st.tm_year - YEAR_OFFSET) : 0)) << FTIME_YEAR_OFFSET;
    ftime <<= FTIME_DATE_OFFSET;

    ftime = (DWORD)st.tm_sec / SEC_MULTIPLIER;
    ftime |= ((DWORD)st.tm_min) << FTIME_MIN_OFFSET;
    ftime |= ((DWORD)st.tm_hour) << FTIME_HR_OFFSET;

    return ftime;
}

int fatfs_stat(struct Vnode *vp, struct stat* sp)
{
    FATFS *fs = (FATFS *)vp->originMount->data;
    DIR_FILE *dfp = (DIR_FILE *)vp->data;
    FILINFO *finfo = &(dfp->fno);
    time_t time;
    int ret;

    ret = lock_fs(fs);
    if (ret == FALSE) {
        return EBUSY;
    }

    sp->st_dev = fs->pdrv;
    sp->st_mode = vp->mode;
    sp->st_nlink = 1;
    sp->st_uid = fs->fs_uid;
    sp->st_gid = fs->fs_gid;
    sp->st_size = finfo->fsize;
    sp->st_blksize = fs->csize * SS(fs);
    sp->st_blocks = finfo->fsize ? ((finfo->fsize - 1) / SS(fs) / fs->csize + 1) : 0;
    time = fattime_transfer(finfo->fdate, finfo->ftime);
    sp->st_mtime = time;

    unlock_fs(fs, FR_OK);
    return 0;
}

void fatfs_chtime(DIR *dp, struct IATTR *attr)
{
    BYTE *dir = dp->dir;
    DWORD ftime;
    if (attr->attr_chg_valid & CHG_ATIME) {
        ftime = fattime_format(attr->attr_chg_atime);
        st_word(dir + DIR_LstAccDate, (ftime >> FTIME_DATE_OFFSET));
    }

    if (attr->attr_chg_valid & CHG_CTIME) {
        ftime = fattime_format(attr->attr_chg_ctime);
        st_dword(dir + DIR_CrtTime, ftime);
    }

    if (attr->attr_chg_valid & CHG_MTIME) {
        ftime = fattime_format(attr->attr_chg_mtime);
        st_dword(dir + DIR_ModTime, ftime);
    }
}

int fatfs_chattr(struct Vnode *vp, struct IATTR *attr)
{
    FATFS *fs = (FATFS *)vp->originMount->data;
    DIR_FILE *dfp = (DIR_FILE *)vp->data;
    DIR *dp = &(dfp->f_dir);
    FILINFO *finfo = &(dfp->fno);
    BYTE *dir = dp->dir;
    DWORD ftime;
    FRESULT result;
    int ret;

    if (finfo->fname[0] == '/') { /* Is root dir of fatfs ? */
        return 0;
    }

    ret = lock_fs(fs);
    if (ret == FALSE) {
        result = FR_TIMEOUT;
        goto ERROR_OUT;
    }

    result = move_window(fs, dp->sect);
    if (result != FR_OK) {
        goto ERROR_UNLOCK;
    }

    if (attr->attr_chg_valid & CHG_MODE) {
        /* FAT only support readonly flag */
        if ((attr->attr_chg_mode & S_IWUSR) == 0 && (finfo->fattrib & AM_RDO) == 0) {
            dir[DIR_Attr] |= AM_RDO;
            finfo->fattrib |= AM_RDO;
            fs->wflag = 1;
        } else if ((attr->attr_chg_mode & S_IWUSR) != 0 && (finfo->fattrib & AM_RDO) != 0) {
            dir[DIR_Attr] &= ~AM_RDO;
            finfo->fattrib &= ~AM_RDO;
            fs->wflag = 1;
        }
        vp->mode = fatfs_get_mode(finfo->fattrib, fs->fs_mode);
    }

    if (attr->attr_chg_valid & (CHG_ATIME | CHG_CTIME | CHG_MTIME)) {
        fatfs_chtime(dp, attr);
        ftime = ld_dword(dp->dir + DIR_ModTime);
        finfo->fdate = (WORD)(ftime >> FTIME_DATE_OFFSET);
        finfo->ftime = (WORD)ftime;
    }

    result = sync_window(fs);
    if (result != FR_OK) {
        goto ERROR_UNLOCK;
    }

    unlock_fs(fs, FR_OK);
    return fatfs_sync(vp->originMount->mountFlags, fs);
ERROR_UNLOCK:
    unlock_fs(fs, result);
ERROR_OUT:
    return -fatfs_2_vfs(result);
}

int fatfs_opendir(struct Vnode *vp, struct fs_dirent_s *idir)
{
    FATFS *fs = vp->originMount->data;
    DIR_FILE *dfp = (DIR_FILE *)vp->data;
    FILINFO *finfo = &(dfp->fno);
    DIR *dp;
    DWORD clst;
    FRESULT result;
    int ret;

    dp = (DIR*)zalloc(sizeof(DIR));
    if (dp == NULL) {
        return -ENOMEM;
    }

    ret = lock_fs(fs);
    if (ret == FALSE) {
        return -EBUSY;
    }
    clst = finfo->sclst;
    dp->obj.fs = fs;
    dp->obj.sclust = clst;

    result = dir_sdi(dp, 0);
    if (result != FR_OK) {
        free(dp);
        unlock_fs(fs, result);
        return -fatfs_2_vfs(result);
    }
    unlock_fs(fs, result);
    idir->u.fs_dir = dp;

    return 0;
}

int fatfs_readdir(struct Vnode *vp, struct fs_dirent_s *idir)
{
    FATFS *fs = vp->originMount->data;
    FILINFO fno;
    DIR* dp = (DIR*)idir->u.fs_dir;
    struct dirent *dirp = NULL;
    FRESULT result;
    int ret, i;

    ret = lock_fs(fs);
    if (ret == FALSE) { /* Lock fs failed */
        return -EBUSY;
    }
    DEF_NAMBUF;
    INIT_NAMBUF(fs);
    for (i = 0; i < idir->read_cnt; i++) {
        /* using dir_read_massive to promote performance */
        result = dir_read_massive(dp, 0);
        if (result == FR_NO_FILE) {
            break;
        } else if (result != FR_OK) {
            goto ERROR_UNLOCK;
        }
        get_fileinfo(dp, &fno);
        /* 0x00 for end of directory and 0xFF for directory is empty */
        if (fno.fname[0] == 0x00 || fno.fname[0] == (TCHAR)0xFF) {
            break;
        }

        dirp = &(idir->fd_dir[i]);
        if (fno.fattrib & AM_DIR) { /* is dir */
            dirp->d_type = DT_DIR;
        } else {
            dirp->d_type = DT_REG;
        }
        if (strncpy_s(dirp->d_name, sizeof(dirp->d_name), fno.fname, strlen(fno.fname)) != EOK) {
            result = FR_NOT_ENOUGH_CORE;
            goto ERROR_UNLOCK;
        }
        result = dir_next(dp, 0);
        if (result != FR_OK && result != FR_NO_FILE) {
            goto ERROR_UNLOCK;
        }
        idir->fd_position++;
        idir->fd_dir[i].d_off = idir->fd_position;
        idir->fd_dir[i].d_reclen = (uint16_t)sizeof(struct dirent);
    }
    unlock_fs(fs, FR_OK);
    FREE_NAMBUF();
    return i;
ERROR_UNLOCK:
    unlock_fs(fs, result);
    FREE_NAMBUF();
    return -fatfs_2_vfs(result);
}

int fatfs_rewinddir(struct Vnode *vp, struct fs_dirent_s *dir)
{
    DIR *dp = (DIR *)dir->u.fs_dir;
    FATFS *fs = dp->obj.fs;
    FRESULT result;
    int ret;

    ret = lock_fs(fs);
    if (ret == FALSE) {
        return -EBUSY;
    }

    result = dir_sdi(dp, 0);
    unlock_fs(fs, result);
    return -fatfs_2_vfs(result);
}

int fatfs_closedir(struct Vnode *vp, struct fs_dirent_s *dir)
{
    DIR *dp = (DIR *)dir->u.fs_dir;
    free(dp);
    dir->u.fs_dir = NULL;
    return 0;
}

static FRESULT rename_check(DIR *dp_new, FILINFO *finfo_new, DIR *dp_old, FILINFO *finfo_old)
{
    DIR dir_sub;
    FRESULT result;
    if (finfo_new->fattrib & AM_ARC) { /* new path is file */
        if (finfo_old->fattrib & AM_DIR) { /* but old path is dir */
            return FR_NO_DIR;
        }
    } else if (finfo_new->fattrib & AM_DIR) { /* new path is dir */
        if (finfo_old->fattrib & AM_ARC) { /* old path is file */
            return FR_IS_DIR;
        }
        dir_sub.obj.fs = dp_old->obj.fs;
        dir_sub.obj.sclust = finfo_new->sclst;
        result = dir_sdi(&dir_sub, 0);
        if (result != FR_OK) {
            return result;
        }
        result = dir_read(&dir_sub, 0);
        if (result == FR_OK) { /* new path isn't empty file */
            return FR_NO_EMPTY_DIR;
        }
    } else { /* System file or volume label */
        return FR_DENIED;
    }
    return FR_OK;
}

int fatfs_rename(struct Vnode *old_vnode, struct Vnode *new_parent, const char *oldname, const char *newname)
{
    FATFS *fs = (FATFS *)(old_vnode->originMount->data);
    DIR_FILE *dfp_old = (DIR_FILE *)old_vnode->data;
    DIR *dp_old = &(dfp_old->f_dir);
    FILINFO *finfo_old = &(dfp_old->fno);
    DIR_FILE *dfp_new = NULL;
    DIR* dp_new = NULL;
    FILINFO* finfo_new = NULL;
    DWORD clust;
    FRESULT result;
    int ret;

    ret = lock_fs(fs);
    if (ret == FALSE) { /* Lock fs failed */
        return -EBUSY;
    }

    dfp_new = (DIR_FILE *)zalloc(sizeof(DIR_FILE));
    if (dfp_new == NULL) {
        result = FR_NOT_ENOUGH_CORE;
        goto ERROR_UNLOCK;
    }
    dp_new = &(dfp_new->f_dir);
    finfo_new = &(dfp_new->fno);

    dp_new->obj.sclust = ((DIR_FILE *)(new_parent->data))->fno.sclst;
    dp_new->obj.fs = fs;

    /* Find new path */
    DEF_NAMBUF;
    INIT_NAMBUF(fs);
    result = create_name(dp_new, &newname);
    if (result != FR_OK) {
        goto ERROR_FREE;
    }
    result = dir_find(dp_new);
    if (result == FR_OK) { /* new path name exist */
        get_fileinfo(dp_new, finfo_new);
        result = rename_check(dp_new, finfo_new, dp_old, finfo_old);
        if (result != FR_OK) {
            goto ERROR_FREE;
        }
        result = dir_remove(dp_old);
        if (result != FR_OK) {
            goto ERROR_FREE;
        }
        clust = finfo_new->sclst;
        if (clust != 0) { /* remove the new path cluster chain if exists */
            result = remove_chain(&(dp_new->obj), clust, 0);
            if (result != FR_OK) {
                goto ERROR_FREE;
            }
        }
    } else { /* new path name not exist */
        result = dir_remove(dp_old);
        if (result != FR_OK) {
            goto ERROR_FREE;
        }
        result = dir_register(dp_new);
        if (result != FR_OK) {
            goto ERROR_FREE;
        }
    }

    /* update new dir entry with old info */
    result = update_dir(dp_new, finfo_old);
    if (result != FR_OK) {
        goto ERROR_FREE;
    }
    result = dir_read(dp_new, 0);
    if (result != FR_OK) {
        goto ERROR_FREE;
    }
    dp_new->blk_ofs = dir_ofs(dp_new);
    get_fileinfo(dp_new, finfo_new);

    dfp_new->fno.fp_list.pstNext = dfp_old->fno.fp_list.pstNext;
    dfp_new->fno.fp_list.pstPrev = dfp_old->fno.fp_list.pstPrev;
    ret = memcpy_s(dfp_old, sizeof(DIR_FILE), dfp_new, sizeof(DIR_FILE));
    if (ret != 0) {
        result = FR_NOT_ENOUGH_CORE;
        goto ERROR_FREE;
    }
    free(dfp_new);
    unlock_fs(fs, FR_OK);
    FREE_NAMBUF();
    return fatfs_sync(old_vnode->originMount->mountFlags, fs);

ERROR_FREE:
    free(dfp_new);
ERROR_UNLOCK:
    unlock_fs(fs, result);
    FREE_NAMBUF();
    return -fatfs_2_vfs(result);
}


static int fatfs_erase(los_part *part, int option)
{
    int opt = option;
    if ((UINT)opt & FMT_ERASE) {
        opt = (UINT)opt & (~FMT_ERASE);
        if (EraseDiskByID(part->disk_id, part->sector_start, part->sector_count) != LOS_OK) {
            PRINTK("Disk erase error.\n");
            return -1;
        }
    }

    if (opt != FM_FAT && opt != FM_FAT32) {
        opt = FM_ANY;
    }

    return opt;
}

static int fatfs_set_part_info(los_part *part)
{
    los_disk *disk = NULL;
    char *buf = NULL;
    int ret;

    /* If there is no MBR before, the partition info needs to be changed after mkfs */
    if (part->type != EMMC && part->part_no_mbr == 0) {
        disk = get_disk(part->disk_id);
        if (disk == NULL) {
            return -EIO;
        }
        buf = (char *)zalloc(disk->sector_size);
        if (buf == NULL) {
            return -ENOMEM;
        }
        (void)memset_s(buf, disk->sector_size, 0, disk->sector_size);
        ret = los_disk_read(part->disk_id, buf, 0, 1, TRUE); /* TRUE when not reading large data */
        if (ret < 0) {
            free(buf);
            return -EIO;
        }
        part->sector_start = LD_DWORD_DISK(&buf[PAR_OFFSET + PAR_START_OFFSET]);
        part->sector_count = LD_DWORD_DISK(&buf[PAR_OFFSET + PAR_COUNT_OFFSET]);
        part->part_no_mbr = 1;
        part->filesystem_type = buf[PAR_OFFSET + PAR_TYPE_OFFSET];

        free(buf);
    }
    return 0;
}

int fatfs_mkfs (struct Vnode *device, int sectors, int option)
{
    BYTE *work_buff = NULL;
    los_part *part = NULL;
    FRESULT result;
    int ret;

    part = los_part_find(device);
    if (part == NULL || device->data == NULL) {
        return -ENODEV;
    }

    if (sectors < 0 || sectors > FAT32_MAX_CLUSTER_SIZE || ((DWORD)sectors & ((DWORD)sectors - 1))) {
        return -EINVAL;
    }

    if (option != FMT_FAT && option != FMT_FAT32 && option != FMT_ANY && option != FMT_ERASE) {
        return -EINVAL;
    }

    if (part->part_name != NULL) { /* The part is mounted */
        return -EBUSY;
    }
    option = fatfs_erase(part, option);
    work_buff = (BYTE *)zalloc(FF_MAX_SS);
    if (work_buff == NULL) {
        return -ENOMEM;
    }

    result = _mkfs(part, sectors, option, work_buff, FF_MAX_SS);
    free(work_buff);
    if (result != FR_OK) {
        return -fatfs_2_vfs(result);
    }

#ifdef LOSCFG_FS_FAT_CACHE
    ret = OsSdSync(part->disk_id);
    if (ret != 0) {
        return -EIO;
    }
#endif

    ret = fatfs_set_part_info(part);
    if (ret != 0) {
        return -EIO;
    }

    return 0;
}

int fatfs_mkdir(struct Vnode *parent, const char *name, mode_t mode, struct Vnode **vpp)
{
    struct Vnode *vp = NULL;
    FATFS *fs =  (FATFS *)parent->originMount->data;
    DIR_FILE *dfp = (DIR_FILE *)parent->data;
    FILINFO *finfo = &(dfp->fno);
    DIR_FILE *dfp_new = NULL;
    QWORD sect;
    DWORD clust;
    BYTE *dir = NULL;
    DWORD hash;
    FRESULT result = FR_OK;
    int ret;
    UINT n;

    ret = lock_fs(fs);
    if (ret == FALSE) {
        result = FR_TIMEOUT;
        goto ERROR_OUT;
    }
    if (finfo->fattrib & AM_ARC) {
        result = FR_NO_DIR;
        goto ERROR_UNLOCK;
    }
    DEF_NAMBUF;
    INIT_NAMBUF(fs);

    dfp_new = (DIR_FILE *)zalloc(sizeof(DIR_FILE));
    if (dfp_new == NULL) {
        result = FR_NOT_ENOUGH_CORE;
        goto ERROR_UNLOCK;
    }
    LOS_ListInit(&(dfp_new->fno.fp_list));
    dfp_new->f_dir.obj.sclust = finfo->sclst;
    dfp_new->f_dir.obj.fs = fs;
    result = create_name(&(dfp_new->f_dir), &name);
    if (result != FR_OK) {
        goto ERROR_FREE;
    }
    result = dir_find(&(dfp_new->f_dir));
    if (result == FR_OK) {
        result = FR_EXIST;
        goto ERROR_FREE;
    }
    /* Allocate new chain for directory */
    clust = create_chain(&(dfp_new->f_dir.obj), 0);
    if (clust == 0) {
        result = FR_NO_SPACE_LEFT;
        goto ERROR_FREE;
    }
    if (clust == 1 || clust == DISK_ERROR) {
        result = FR_DISK_ERR;
        goto ERROR_FREE;
    }
    result = sync_window(fs); /* Flush FAT */
    if (result != FR_OK) {
        goto ERROR_REMOVE_CHAIN;
    }
    /* Initialize the new directory */
#ifndef LOSCFG_FS_FAT_VIRTUAL_PARTITION
    dir = fs->win;
#else
    dir = PARENTFS(fs)->win;
#endif

    sect = clst2sect(fs, clust);
    mem_set(dir, 0, SS(fs));
    for (n = fs->csize; n > 0; n--) {    /* Write zero to directory */
#ifndef LOSCFG_FS_FAT_VIRTUAL_PARTITION
        fs->winsect = sect++;
        fs->wflag = 1;
#else
        PARENTFS(fs)->winsect = sect++;
        PARENTFS(fs)->wflag = 1;
        result = sync_window(fs);
        if (result != FR_OK) break;
#endif
    }

    if (result != FR_OK) {
        goto ERROR_REMOVE_CHAIN;
    }
    result = dir_register(&(dfp_new->f_dir));
    if (result != FR_OK) {
        goto ERROR_REMOVE_CHAIN;
    }
    dir = dfp_new->f_dir.dir;
    st_dword(dir + DIR_ModTime, 0); /* Set the time */
    st_clust(fs, dir, clust); /* Set the start cluster */
    dir[DIR_Attr] = AM_DIR; /* Set the attrib */
    if ((mode & S_IWUSR) == 0) {
        dir[DIR_Attr] |= AM_RDO;
    }
#ifndef LOSCFG_FS_FAT_VIRTUAL_PARTITION
    fs->wflag = 1;
#else
    PARENTFS(fs)->wflag = 1;
#endif
    result = sync_fs(fs);
    if (result != FR_OK) {
        goto ERROR_REMOVE_CHAIN;
    }

    /* Set the FILINFO struct */
    result = dir_read(&(dfp_new->f_dir), 0);
    if (result != FR_OK) {
        goto ERROR_REMOVE_CHAIN;
    }
    dfp_new->f_dir.blk_ofs = dir_ofs(&(dfp_new->f_dir));
    get_fileinfo(&(dfp_new->f_dir), &(dfp_new->fno));

    ret = VnodeAlloc(&fatfs_vops, &vp);
    if (ret != 0) {
        result = FR_NOT_ENOUGH_CORE;
        goto ERROR_REMOVE_CHAIN;
    }
    vp->parent = parent;
    vp->fop = &fatfs_fops;
    vp->data = dfp_new;
    vp->originMount = parent->originMount;
    vp->uid = fs->fs_uid;
    vp->gid = fs->fs_gid;
    vp->mode = fatfs_get_mode(dfp_new->fno.fattrib, fs->fs_mode);
    vp->type = VNODE_TYPE_DIR;

    hash = fatfs_hash(dfp_new->f_dir.sect, dfp_new->f_dir.dptr, dfp_new->fno.sclst);
    ret = VfsHashInsert(vp, hash);
    if (ret != 0) {
        result = FR_NOT_ENOUGH_CORE;
        goto ERROR_REMOVE_CHAIN;
    }

    unlock_fs(fs, FR_OK);
    FREE_NAMBUF();
    *vpp = vp;
    return fatfs_sync(vp->originMount->mountFlags, fs);

ERROR_REMOVE_CHAIN:
    remove_chain(&(dfp_new->f_dir.obj), clust, 0);
ERROR_FREE:
    free(dfp_new);
ERROR_UNLOCK:
    unlock_fs(fs, result);
    FREE_NAMBUF();
ERROR_OUT:
    return -fatfs_2_vfs(result);
}

int fatfs_rmdir(struct Vnode *parent, struct Vnode *vp, char *name)
{
    FATFS *fs = (FATFS *)vp->originMount->data;
    DIR_FILE *dfp = (DIR_FILE *)vp->data;
    FILINFO *finfo = &(dfp->fno);
    DIR *dp = &(dfp->f_dir);
    DIR dir_sub;
    FRESULT result = FR_OK;
    int ret;

    if (finfo->fattrib & AM_ARC) {
        result = FR_NO_DIR;
        goto ERROR_OUT;
    }

    DEF_NAMBUF;
    INIT_NAMBUF(fs);

    ret = lock_fs(fs);
    if (ret == FALSE) {
        result = FR_TIMEOUT;
        goto ERROR_OUT;
    }
    dir_sub.obj.fs = fs;
    dir_sub.obj.sclust = finfo->sclst;
    result = dir_sdi(&dir_sub, 0);
    if (result != FR_OK) {
        goto ERROR_UNLOCK;
    }
    result = dir_read(&dir_sub, 0);
    if (result == FR_OK) {
        result = FR_NO_EMPTY_DIR;
        goto ERROR_UNLOCK;
    }
    result = dir_remove(dp); /* remove directory entry */
    if (result != FR_OK) {
        goto ERROR_UNLOCK;
    }
    /* Directory entry contains at least one cluster */
    result = remove_chain(&(dp->obj), finfo->sclst, 0);
    if (result != FR_OK) {
        goto ERROR_UNLOCK;
    }

    unlock_fs(fs, FR_OK);
    FREE_NAMBUF();
    return fatfs_sync(vp->originMount->mountFlags, fs);

ERROR_UNLOCK:
    unlock_fs(fs, result);
    FREE_NAMBUF();
ERROR_OUT:
    return -fatfs_2_vfs(result);
}

int fatfs_reclaim(struct Vnode *vp)
{
    free(vp->data);
    vp->data = NULL;
    return 0;
}

int fatfs_unlink(struct Vnode *parent, struct Vnode *vp, char *name)
{
    FATFS *fs = (FATFS *)vp->originMount->data;
    DIR_FILE *dfp = (DIR_FILE *)vp->data;
    FILINFO *finfo = &(dfp->fno);
    DIR *dp = &(dfp->f_dir);
    FRESULT result = FR_OK;
    int ret;

    if (finfo->fattrib & AM_DIR) {
        result = FR_IS_DIR;
        goto ERROR_OUT;
    }
    ret = lock_fs(fs);
    if (ret == FALSE) {
        result = FR_TIMEOUT;
        goto ERROR_OUT;
    }
    result = dir_remove(dp); /* remove directory entry */
    if (result != FR_OK) {
        goto ERROR_UNLOCK;
    }
    if (finfo->sclst != 0) { /* if cluster chain exists */
        result = remove_chain(&(dp->obj), finfo->sclst, 0);
        if (result != FR_OK) {
            goto ERROR_UNLOCK;
        }
    }
    result = sync_fs(fs);
    if (result != FR_OK) {
        goto ERROR_UNLOCK;
    }
    unlock_fs(fs, FR_OK);
    return fatfs_sync(vp->originMount->mountFlags, fs);

ERROR_UNLOCK:
    unlock_fs(fs, result);
ERROR_OUT:
    return -fatfs_2_vfs(result);
}

int fatfs_ioctl(struct file *filep, int req, unsigned long arg)
{
    return -ENOSYS;
}

#define CHECK_FILE_NUM 3
static inline DWORD combine_time(FILINFO *finfo)
{
    return (finfo->fdate << FTIME_DATE_OFFSET) | finfo->ftime;
}

static UINT get_oldest_time(DIR_FILE df[], DWORD *oldest_time, UINT len)
{
    int i;
    DWORD old_time = combine_time(&(df[0].fno));
    DWORD time;
    UINT index = 0;
    for (i = 1; i < len; i++) {
        time = combine_time(&(df[i].fno));
        if (time < old_time) {
            old_time = time;
            index = i;
        }
    }
    *oldest_time = old_time;
    return index;
}

int fatfs_fscheck(struct Vnode* vp, struct fs_dirent_s *dir)
{
    FATFS *fs = (FATFS *)vp->originMount->data;
    DIR_FILE df[CHECK_FILE_NUM] = {0};
    DIR *dp = NULL;
    FILINFO *finfo = &(((DIR_FILE *)(vp->data))->fno);
    FILINFO fno;
    DWORD old_time = -1;
    DWORD time;
    UINT count;
    UINT index = 0;
    los_part *part = NULL;
    FRESULT result;
    int ret;

    if (fs->fs_type != FS_FAT32) {
        return -EINVAL;
    }

    if ((finfo->fattrib & AM_DIR) == 0) {
        return -ENOTDIR;
    }

    ret = fatfs_opendir(vp, dir);
    if (ret < 0) {
        return ret;
    }

    ret = lock_fs(fs);
    if (ret == FALSE) {
        result = FR_TIMEOUT;
        goto ERROR_WITH_DIR;
    }

    dp = (DIR *)dir->u.fs_dir;
    dp->obj.id = fs->id;
    for (count = 0; count < CHECK_FILE_NUM; count++) {
        if ((result = f_readdir(dp, &fno)) != FR_OK) {
            goto ERROR_UNLOCK;
        } else {
            if (fno.fname[0] == 0 || fno.fname[0] == (TCHAR)0xFF) {
                break;
            }
            (void)memcpy_s(&df[count].f_dir, sizeof(DIR), dp, sizeof(DIR));
            (void)memcpy_s(&df[count].fno, sizeof(FILINFO), &fno, sizeof(FILINFO));
            time = combine_time(&(df[count].fno));
            if (time < old_time) {
                old_time = time;
                index = count;
            }
        }
    }
    while ((result = f_readdir(dp, &fno)) == FR_OK) {
        if (fno.fname[0] == 0 || fno.fname[0] == (TCHAR)0xFF) {
            break;
        }
        time = combine_time(&fno);
        if (time < old_time) {
            (void)memcpy_s(&df[index].f_dir, sizeof(DIR), dp, sizeof(DIR));
            (void)memcpy_s(&df[index].fno, sizeof(FILINFO), &fno, sizeof(FILINFO));
            index = get_oldest_time(df, &old_time, CHECK_FILE_NUM);
        }
    }
    if (result != FR_OK) {
        goto ERROR_UNLOCK;
    }

    for (index = 0; index < count; index++) {
        result = f_fcheckfat(&df[index]);
        if (result != FR_OK) {
            goto ERROR_UNLOCK;
        }
    }

    unlock_fs(fs, FR_OK);

    ret = fatfs_closedir(vp, dir);
    if (ret < 0) {
        return ret;
    }

#ifdef LOSCFG_FS_FAT_CACHE
    part = get_part((INT)fs->pdrv);
    if (part != NULL) {
        (void)OsSdSync(part->disk_id);
    }
#endif

    return 0;

ERROR_UNLOCK:
    unlock_fs(fs, result);
ERROR_WITH_DIR:
    fatfs_closedir(vp, dir);
    return -fatfs_2_vfs(result);
}

struct VnodeOps fatfs_vops = {
    /* file ops */
    .Getattr = fatfs_stat,
    .Chattr = fatfs_chattr,
    .Lookup = fatfs_lookup,
    .Rename = fatfs_rename,
    .Create = fatfs_create,
    .Unlink = fatfs_unlink,
    .Reclaim = fatfs_reclaim,
    .Truncate = fatfs_truncate,
    .Truncate64 = fatfs_truncate64,
    /* dir ops */
    .Opendir = fatfs_opendir,
    .Readdir = fatfs_readdir,
    .Rewinddir = fatfs_rewinddir,
    .Closedir = fatfs_closedir,
    .Mkdir = fatfs_mkdir,
    .Rmdir = fatfs_rmdir,
    .Fscheck = fatfs_fscheck,
};

struct MountOps fatfs_mops = {
    .Mount = fatfs_mount,
    .Unmount = fatfs_umount,
    .Statfs = fatfs_statfs,
};

struct file_operations_vfs fatfs_fops = {
    .open = fatfs_open,
    .read = fatfs_read,
    .write = fatfs_write,
    .seek = fatfs_lseek,
    .close = fatfs_close,
    .mmap = OsVfsFileMmap,
    .fallocate = fatfs_fallocate,
    .fallocate64 = fatfs_fallocate64,
    .fsync = fatfs_fsync,
    .ioctl = fatfs_ioctl,
};

FSMAP_ENTRY(fat_fsmap, "vfat", fatfs_mops, FALSE, TRUE);

#endif /* LOSCFG_FS_FAT */
