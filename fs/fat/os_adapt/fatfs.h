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
#include "fs/file.h"
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
// 文件系统核心常量定义
#define MAX_LFNAME_LENGTH       256  // 长文件名最大长度（字符）
#define LABEL_LEN              12  // 卷标名称长度（字符）
#define FAT_RESERVED_NUM       2  // FAT表保留数量
#define FAT32_MAXSIZE          0x100000000  // FAT32最大支持容量（4GB，0x100000000字节）
#define BAD_CLUSTER            0x7FFFFFFF  // 坏簇标记值
#define DISK_ERROR             0xFFFFFFFF  // 磁盘错误标记值
#define FAT12_END_OF_CLUSTER   0x00000FFF  // FAT12文件结束簇标记
#define FAT16_END_OF_CLUSTER   0x0000FFFF  // FAT16文件结束簇标记
#define FAT32_END_OF_CLUSTER   0x0FFFFFFF  // FAT32文件结束簇标记
#define FAT_ERROR              (-1)  // FAT操作错误返回值

/* MBR分区表相关定义 */
#define MBR_PRIMARY_PART_NUM 4  // MBR主分区表项数量，最多支持4个主分区
#define JUMP_CODE "\xEB\xFE\x90"  // 引导扇区跳转指令，跳转到引导代码执行

/* 分区类型标识（MBR分区表中的类型字段） */
#define FAT12                  0x01  // FAT12文件系统，用于第一个32MB物理分区
#define FAT16                  0x04  // FAT16文件系统，小于65536个扇区（32MB）
#define EXTENDED_PARTITION_CHS  0x05  // CHS寻址模式的扩展分区
#define FAT16B                 0x06  // FAT16B文件系统，65536个扇区及以上
#define FAT32_CHS              0x0B  // CHS寻址模式的FAT32文件系统
#define FAT32_LBA              0x0C  // LBA寻址模式的FAT32文件系统
#define EXTENDED_PARTITION_LBA 0x0F  // LBA寻址模式的扩展分区
#define GPT_PROTECTIVE_MBR     0xEE  // GPT磁盘的保护性MBR类型

/* 卷引导记录(VBR)类型 */
#define VBR_FAT                0  // 有效的FAT卷引导记录
#define VBR_BS_NOT_FAT         2  // 引导扇区但不是FAT文件系统
#define VBR_NOT_BS             3  // 不是引导扇区
#define VBR_DISK_ERR           4  // 磁盘错误

/* 文件系统限制与边界值 */
#define FAT_MAX_CLUSTER_SIZE     64  // FAT最大簇大小（扇区）
#define FAT32_MAX_CLUSTER_SIZE   128  // FAT32最大簇大小（扇区）
#define FAT32_ENTRY_SIZE         4  // FAT32表项大小（字节）
#define FAT16_ENTRY_SIZE         2  // FAT16表项大小（字节）
#define VOL_MIN_SIZE             128  // 卷最小大小（扇区）
#define SFD_START_SECTOR         63  //  superfloppy磁盘起始扇区
#define MAX_BLOCK_SIZE         32768  // 最大块大小（扇区）

/* 保留扇区数量定义 */
#define FAT32_RESERVED_SECTOR  32  // FAT32文件系统保留扇区数
#define FAT_RESERVED_SECTOR    1  // FAT12/FAT16文件系统保留扇区数

// 目录相关常量
#define DIR_NAME_LEN           11  // 短文件名长度（8.3格式，共11字符）
#define DIR_READ_COUNT         7  // 目录项读取计数

#define VOLUME_CHAR_LENGTH     4  // 卷字符长度

// 调试功能开关与宏定义
#define FAT_DEBUG  // FAT调试功能开关，取消注释启用调试
#ifdef FAT_DEBUG
#define FDEBUG(format, ...) do { \  // 调试打印宏
    PRINTK("[%s:%d]"format"\n", __func__, __LINE__, ##__VA_ARGS__); \
} while (0)
#else
#define FDEBUG(...)  // 调试功能关闭时为空宏
#endif

/* 格式化选项（format函数的第三个参数） */
#define FMT_FAT      0x01  // 格式化FAT12/FAT16标志
#define FMT_FAT32    0x02  // 格式化FAT32标志
#define FMT_ANY      0x07  // 自动选择FAT类型标志
#define FMT_ERASE    0x08  // 格式化时擦除介质标志

extern char FatLabel[LABEL_LEN];

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
int fatfs_mkfs(struct Vnode *device, int sectors, int option);
int fatfs_mkdir(struct Vnode *parent, const char *name, mode_t mode, struct Vnode **vpp);
int fatfs_rmdir(struct Vnode *parent, struct Vnode *vp, const char *name);
int fatfs_unlink(struct Vnode *parent, struct Vnode *vp, const char *name);
int fatfs_ioctl(struct file *filep, int req, unsigned long arg);
int fatfs_fscheck(struct Vnode* vnode, struct fs_dirent_s *dir);

FRESULT find_fat_partition(FATFS *fs, los_part *part, BYTE *format, QWORD *start_sector);
FRESULT init_fatobj(FATFS *fs, BYTE fmt, QWORD start_sector);
FRESULT _mkfs(los_part *partition, const MKFS_PARM *opt, BYTE *work, UINT len);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
