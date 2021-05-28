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

#ifndef _FATFS_H
#define _FATFS_H

#include "ff.h"
#include "fs/fs.h"
#include "disk.h"
#include "unistd.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "time.h"
#include "sys/stat.h"
#include "sys/statfs.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define MAX_LFNAME_LENGTH       256
#define LABEL_LEN              12
#define FAT_RESERVED_NUM       2
#define FAT32_MAXSIZE          0x100000000
#define BAD_CLUSTER            0x7FFFFFFF
#define DISK_ERROR             0xFFFFFFFF
#define FAT12_END_OF_CLUSTER   0x00000FFF
#define FAT16_END_OF_CLUSTER   0x0000FFFF
#define FAT32_END_OF_CLUSTER   0x0FFFFFFF
#define FAT_ERROR              (-1)

/* MBR */
#define MBR_PRIMARY_PART_NUM 4
#define JUMP_CODE "\xEB\xFE\x90"

/* Partiton type */
#define FAT12                  0x01 /* FAT12 as primary partition in first physical 32MB */
#define FAT16                  0x04 /* FAT16 with less than 65536 sectors(32MB) */
#define EXTENDED_PARTITION_CHS  0x05
#define FAT16B                 0x06 /* FAT16B with 65536 or more sectors */
#define FAT32_CHS              0x0B
#define FAT32_LBA              0x0C
#define EXTENDED_PARTITION_LBA 0x0F
#define GPT_PROTECTIVE_MBR     0xEE

/* volume boot record type */
#define VBR_FAT                0
#define VBR_BS_NOT_FAT         2
#define VBR_NOT_BS             3
#define VBR_DISK_ERR           4

/* Limit and boundary */
#define FAT_MAX_CLUSTER_SIZE     64  /* (sectors) */
#define FAT32_MAX_CLUSTER_SIZE   128 /* (sectors) */
#define FAT32_ENTRY_SIZE         4 /* (bytes) */
#define FAT16_ENTRY_SIZE         2 /* (bytes) */
#define VOL_MIN_SIZE             128 /* (sectors) */
#define SFD_START_SECTOR         63
#define MAX_BLOCK_SIZE         32768 /* (sectors) */

/* Sector */
#define FAT32_RESERVED_SECTOR  32
#define FAT_RESERVED_SECTOR    1

#define DIR_NAME_LEN           11
#define DIR_READ_COUNT         7

#define VOLUME_CHAR_LENGTH     4

#define FAT_DEBUG
#ifdef FAT_DEBUG
#define FDEBUG(format, ...) do { \
    PRINTK("[%s:%d]"format"\n", __func__, __LINE__, ##__VA_ARGS__); \
} while (0)
#else
#define FDEBUG(...)
#endif

int fatfs_2_vfs(int result);
int fatfs_lookup(struct Vnode *parent, const char *name, int len, struct Vnode **vpp);
int fatfs_create(struct Vnode *parent, const char *name, int mode, struct Vnode **vpp);
int fatfs_read(struct file *filep, char *buff, size_t count);
off_t fatfs_lseek64(struct file *filep, off64_t offset, int whence);
off64_t fatfs_lseek(struct file *filep, off_t offset, int whence);
int fatfs_write(struct file *filep, const char *buff, size_t count);
int fatfs_fsync(struct file *filep);
int fatfs_fallocate64(struct file *filep, int mode, off64_t offset, off64_t len);
int fatfs_fallocate(struct file *filep, int mode, off_t offset, off_t len);
int fatfs_truncate64(struct Vnode *vnode, off64_t len);
int fatfs_truncate(struct Vnode *vnode, off_t len);
int fatfs_mount(struct Mount *mount, struct Vnode *device, const void *data);
int fatfs_umount(struct Mount *mount, struct Vnode **device);
int fatfs_statfs(struct Mount *mount, struct statfs *info);
int fatfs_stat(struct Vnode *vnode, struct stat *buff);
int fatfs_chattr(struct Vnode *vnode, struct IATTR *attr);
int fatfs_opendir(struct Vnode *vnode, struct fs_dirent_s *idir);
int fatfs_readdir(struct Vnode *vnode, struct fs_dirent_s *idir);
int fatfs_rewinddir(struct Vnode *vnode, struct fs_dirent_s *dir);
int fatfs_closedir(struct Vnode *vnode, struct fs_dirent_s *dir);
int fatfs_rename(struct Vnode *oldvnode, struct Vnode *newparent, const char *oldname, const char *newname);
int fatfs_mkfs (struct Vnode *device, int sectors, int option);
int fatfs_mkdir(struct Vnode *parent, const char *name, mode_t mode, struct Vnode **vpp);
int fatfs_rmdir(struct Vnode *parent, struct Vnode *vp, char *name);
int fatfs_unlink(struct Vnode *parent, struct Vnode *vp, char *name);
int fatfs_ioctl(struct file *filep, int req, unsigned long arg);
int fatfs_fscheck(struct Vnode* vnode, struct fs_dirent_s *dir);

FRESULT find_fat_partition(FATFS *fs, los_part *part, BYTE *format, QWORD *start_sector);
FRESULT init_fatobj(FATFS *fs, BYTE fmt, QWORD start_sector);
FRESULT _mkfs(los_part *partition, int sector, int opt, BYTE *work, UINT len);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
