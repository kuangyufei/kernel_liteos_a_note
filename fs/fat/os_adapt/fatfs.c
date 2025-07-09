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
#include "disk_pri.h"
#include "diskio.h"
#include "fs/file.h"
#include "fs/fs.h"
#include "fs/dirent_fs.h"
#include "fs/mount.h"
#include "vnode.h"
#include "path_cache.h"
#ifdef LOSCFG_FS_FAT_VIRTUAL_PARTITION
#include "virpartff.h"
#include "errcode_fat.h"
#endif
#include "los_tables.h"
#include "user_copy.h"
#include "los_vm_filemap.h"
#include "los_hash.h"
#include "los_vm_common.h"
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

/**
 * @brief 
 * @verbatim
FAT文件系统是File Allocation Table（文件配置表）的简称，主要包括DBR区、FAT区、DATA区三个区域。
其中，FAT区各个表项记录存储设备中对应簇的信息，包括簇是否被使用、文件下一个簇的编号、是否文件结尾等。
FAT文件系统有FAT12、FAT16、FAT32等多种格式，其中，12、16、32表示对应格式中FAT表项的字节数。
FAT文件系统支持多种介质，特别在可移动存储介质（U盘、SD卡、移动硬盘等）上广泛使用，
使嵌入式设备和Windows、Linux等桌面系统保持很好的兼容性，方便用户管理操作文件。

OpenHarmony内核支持FAT12、FAT16与FAT32三种格式的FAT文件系统，
具有代码量小、资源占用小、可裁切、支持多种物理介质等特性，并且与Windows、Linux等系统保持兼容，
支持多设备、多分区识别等功能。OpenHarmony内核支持硬盘多分区，可以在主分区以及逻辑分区上创建FAT文件系统。

驱动适配
	FAT文件系统的使用需要底层MMC相关驱动的支持。在一个带MMC存储设备的板子上运行FATFS，需要：
	1、适配板端EMMC驱动，实现disk_status、disk_initialize、disk_read、disk_write、disk_ioctl接口；
	2、新增fs_config.h文件，配置FS_MAX_SS（存储设备最大sector大小）、FF_VOLUME_STRS（分区名）等信息，
 * @endverbatim
 */
/**
 * @brief FAT文件系统的Vnode操作结构体
 * @details 声明FAT文件系统的虚拟节点操作集合，用于VFS层接口适配
 */
struct VnodeOps fatfs_vops; /* forward define */
/**
 * @brief FAT文件系统的文件操作结构体
 * @details 声明FAT文件系统的文件操作集合，实现VFS标准文件接口
 */
struct file_operations_vfs fatfs_fops;

/** @defgroup fatfs_bitmask_macros FATFS位掩码宏定义
 * @brief FAT文件系统中用于位操作的掩码定义
 * @{ */
#define BITMASK4 0x0F  // 4位掩码：00001111
#define BITMASK5 0x1F  // 5位掩码：00011111
#define BITMASK6 0x3F  // 6位掩码：00111111
#define BITMASK7 0x7F  // 7位掩码：01111111
/** @} */

/** @defgroup fatfs_time_offset_macros FATFS时间偏移宏定义
 * @brief FAT文件系统中时间字段的位偏移定义
 * @{ */
#define FTIME_MIN_OFFSET 6  /* 分钟在16位时间字段中的偏移位 */
#define FTIME_HR_OFFSET 11  /* 小时在16位时间字段中的偏移位 */
#define FTIME_MTH_OFFSET 5  /* 月份在16位日期字段中的偏移位 */
#define FTIME_YEAR_OFFSET 9 /* 年份在16位日期字段中的偏移位 */
#define FTIME_DATE_OFFSET 16 /* 日期在32位时间戳中的偏移位 */
#define SEC_MULTIPLIER 2    /* 秒数乘数（FAT时间精度为2秒） */
#define YEAR_OFFSET 80      /* 年份偏移量：FATFS从1980年开始，struct tm从1900年开始 */
/** @} */

/**
 * @brief FatFs错误码转换为VFS错误码
 * @details 将FatFs文件系统返回的错误码映射为标准VFS错误码，实现不同文件系统间错误码统一
 * @param result FatFs返回的错误码
 * @return 转换后的VFS错误码，ENOERR表示成功
 */
int fatfs_2_vfs(int result)
{
    int status = ENOERR;  // 默认返回成功状态

#ifdef LOSCFG_FS_FAT_VIRTUAL_PARTITION  // 如果启用虚拟分区功能
    /* 虚拟分区错误码直接返回，不做转换 */
    if (result < 0 || result >= VIRERR_BASE) {
        return result;
    }
#else  // 未启用虚拟分区功能
    /* 负数值错误码直接返回 */
    if (result < 0) {
        return result;
    }
#endif

    /* FatFs错误码到Libc错误码的映射 */
    switch (result) {
        case FR_OK:  // FatFs成功状态
            break;  // 保持ENOERR状态

        case FR_NO_FILE:    // 文件不存在
        case FR_NO_PATH:    // 路径不存在
            status = ENOENT;  // 映射为"无此文件或目录"错误
            break;

        case FR_NO_FILESYSTEM:  // 文件系统不存在
            status = ENOTSUP;   // 映射为"不支持的文件系统"错误
            break;

        case FR_INVALID_NAME:   // 无效的文件名
            status = EINVAL;    // 映射为"无效参数"错误
            break;

        case FR_EXIST:          // 文件已存在
        case FR_INVALID_OBJECT: // 无效的对象
            status = EEXIST;    // 映射为"文件已存在"错误
            break;

        case FR_DISK_ERR:       // 磁盘I/O错误
        case FR_NOT_READY:      // 设备未就绪
        case FR_INT_ERR:        // 内部逻辑错误
            status = EIO;       // 映射为"I/O错误"
            break;

        case FR_WRITE_PROTECTED:    // 介质写保护
            status = EROFS;         // 映射为"只读文件系统"错误
            break;
        case FR_MKFS_ABORTED:       // 格式化被中止
        case FR_INVALID_PARAMETER:  // 参数无效
            status = EINVAL;        // 映射为"无效参数"错误
            break;

        case FR_NO_SPACE_LEFT:  // 磁盘空间不足
            status = ENOSPC;     // 映射为"无可用空间"错误
            break;
        case FR_NO_DIRENTRY:    // 无可用目录项
            status = ENFILE;     // 映射为"打开文件过多"错误
            break;
        case FR_NO_EMPTY_DIR:   // 目录非空
            status = ENOTEMPTY;  // 映射为"目录非空"错误
            break;
        case FR_IS_DIR:         // 操作对象是目录
            status = EISDIR;     // 映射为"是一个目录"错误
            break;
        case FR_NO_DIR:         // 不是目录
            status = ENOTDIR;    // 映射为"不是目录"错误
            break;
        case FR_NO_EPERM:       // 权限不足
        case FR_DENIED:         // 访问被拒绝
            status = EPERM;      // 映射为"操作不允许"错误
            break;
        case FR_LOCKED:         // 文件被锁定
        case FR_TIMEOUT:        // 操作超时
            status = EBUSY;      // 映射为"设备或资源忙"错误
            break;
#ifdef LOSCFG_FS_FAT_VIRTUAL_PARTITION  // 虚拟分区相关错误码
        case FR_MODIFIED:           // 虚拟分区已修改
            status = -VIRERR_MODIFIED;       // 虚拟分区修改错误
            break;
        case FR_CHAIN_ERR:          // 簇链错误
            status = -VIRERR_CHAIN_ERR;       // 簇链错误
            break;
        case FR_OCCUPIED:           // 分区被占用
            status = -VIRERR_OCCUPIED;        // 分区占用错误
            break;
        case FR_NOTCLEAR:           // 未清空
            status = -VIRERR_NOTCLEAR;        // 未清空错误
            break;
        case FR_NOTFIT:             // 大小不匹配
            status = -VIRERR_NOTFIT;          // 大小不匹配错误
            break;
        case FR_INVAILD_FATFS:      // 无效的FATFS
            status = -VIRERR_INTER_ERR;       // 内部错误
            break;
#endif
        default:                // 未知错误
            status = -FAT_ERROR; // 映射为FAT通用错误
            break;
    }

    return status;  // 返回转换后的错误码
}

/**
 * @brief 判断是否为最后一个簇
 * @details 根据FAT文件系统类型（FAT12/FAT16/FAT32）判断当前簇是否为簇链的最后一个
 * @param fs FAT文件系统信息结构体指针
 * @param cclust 当前簇号
 * @return true - 是最后一个簇，false - 不是最后一个簇
 */
static bool fatfs_is_last_cluster(FATFS *fs, DWORD cclust)
{
    switch (fs->fs_type) {  // 根据文件系统类型判断
        case FS_FAT12:       // FAT12文件系统
            return (cclust == FAT12_END_OF_CLUSTER);
        case FS_FAT16:       // FAT16文件系统
            return (cclust == FAT16_END_OF_CLUSTER);
        case FS_FAT32:       // FAT32文件系统
        default:             // 默认情况（FAT32）
            return (cclust == FAT32_END_OF_CLUSTER);
    }
}
/**
 * @brief FAT文件系统同步函数
 * @details 根据挂载标志将缓存数据同步到物理设备
 * @param mountflags 挂载标志
 * @param fs FAT文件系统控制块指针
 * @return 0 - 成功，非0 - 错误码
 */
static int fatfs_sync(unsigned long mountflags, FATFS *fs)
{
#ifdef LOSCFG_FS_FAT_CACHE  // 如果启用FAT缓存功能
    los_part *part = NULL;  // 分区信息结构体指针
    /* 检查挂载标志：非同步禁用且非只读模式 */
    if (!(mountflags & (MS_NOSYNC | MS_RDONLY))) {
        part = get_part((INT)fs->pdrv);  // 获取分区信息
        if (part == NULL) {  // 分区不存在
            return -ENODEV;  // 返回设备不存在错误
        }

        (void)OsSdSync(part->disk_id);  // 执行SD卡同步操作
    }
#endif
    return 0;  // 同步成功
}

/**
 * @brief Vnode哈希比较函数
 * @details 比较两个Vnode的目录项信息是否相同，用于哈希表查找
 * @param vp Vnode结构体指针
 * @param arg 比较参数(DIR_FILE结构体指针)
 * @return 0 - 相同，非0 - 不同
 */
int fatfs_hash_cmp(struct Vnode *vp, void *arg)
{
    DIR_FILE *dfp_target = (DIR_FILE *)arg;  // 目标目录文件结构体
    DIR_FILE *dfp = (DIR_FILE *)vp->data;    // 当前Vnode关联的目录文件结构体

    /* 比较扇区号、目录项指针和起始簇号 */
    return dfp_target->f_dir.sect != dfp->f_dir.sect ||
              dfp_target->f_dir.dptr != dfp->f_dir.dptr ||
              dfp_target->fno.sclst != dfp->fno.sclst;
}

/**
 * @brief 计算目录项哈希值
 * @details 使用FNV32a算法计算目录项的唯一哈希值，用于Vnode缓存
 * @param sect 扇区号
 * @param dptr 目录项指针
 * @param sclst 起始簇号
 * @return 计算得到的哈希值
 */
static DWORD fatfs_hash(QWORD sect, DWORD dptr, DWORD sclst)
{
    DWORD hash = FNV1_32A_INIT;  // FNV哈希初始值
    hash = LOS_HashFNV32aBuf(&sect, sizeof(QWORD), hash);  // 累加扇区号哈希
    hash = LOS_HashFNV32aBuf(&dptr, sizeof(DWORD), hash);  // 累加分目录项指针哈希
    hash = LOS_HashFNV32aBuf(&sclst, sizeof(DWORD), hash); // 累加起始簇号哈希

    return hash;  // 返回最终哈希值
}

/**
 * @brief FAT属性转换为VFS文件模式
 * @details 将FAT文件系统的属性转换为标准Unix文件权限模式
 * @param attribute FAT文件属性
 * @param fs_mode 文件系统基础模式
 * @return 转换后的VFS文件模式
 */
static mode_t fatfs_get_mode(BYTE attribute, mode_t fs_mode)
{
    mode_t mask = 0;  // 权限掩码
    if (attribute & AM_RDO) {  // 如果文件是只读属性
        mask = S_IWUSR | S_IWGRP | S_IWOTH;  // 写权限掩码
    }
    fs_mode &= ~mask;  // 移除写权限
    if (attribute & AM_DIR) {  // 如果是目录
        fs_mode |= S_IFDIR;  // 设置目录模式位
    } else if (attribute & AM_LNK) {  // 如果是符号链接
        fs_mode |= S_IFLNK;  // 设置链接模式位
    } else {  // 普通文件
        fs_mode |= S_IFREG;  // 设置普通文件模式位
    }
    return fs_mode;  // 返回转换后的模式
}

/**
 * @brief FAT文件类型转换为Vnode类型
 * @details 将FAT文件系统的文件类型转换为VFS的Vnode节点类型
 * @param type FAT文件类型
 * @return 对应的Vnode类型
 */
static enum VnodeType fatfstype_2_vnodetype(BYTE type)
{
    switch (type) {
        case AM_ARC:  // 普通文件
            return VNODE_TYPE_REG;
        case AM_DIR:  // 目录
            return VNODE_TYPE_DIR;
        case AM_LNK:  // 符号链接
            return VNODE_TYPE_LNK;
        default:  // 未知类型
            return VNODE_TYPE_UNKNOWN;
    }
}

#define DIR_SIZE 32  // 目录项大小(32字节)
/**
 * @brief 初始化新簇
 * @details 为目录或符号链接分配并初始化新的簇链
 * @param pdfp 父目录文件结构体
 * @param dp_new 新目录结构体
 * @param fs FAT文件系统控制块
 * @param type 文件类型(AM_DIR/AM_LNK)
 * @param target 符号链接目标路径(仅对链接有效)
 * @param clust 输出参数，返回分配的簇号
 * @return FR_OK - 成功，其他值 - FatFs错误码
 */
static FRESULT init_cluster(DIR_FILE *pdfp, DIR *dp_new, FATFS *fs, int type, const char *target, DWORD *clust)
{
    FRESULT result;       // FatFs操作结果
    BYTE *dir = NULL;     // 目录数据缓冲区
    QWORD sect;           // 扇区号
    DWORD pclust;         // 父目录簇号
    UINT n;               // 循环计数器

    /* 分配新簇链 */
    *clust = create_chain(&(dp_new->obj), 0);
    if (*clust == 0) {  // 没有可用簇
        return FR_NO_SPACE_LEFT;  // 返回空间不足错误
    }
    if (*clust == 1 || *clust == DISK_ERROR) {  // 簇分配失败
        return FR_DISK_ERR;  // 返回磁盘错误
    }

    result = sync_window(fs); /* 刷新FAT表缓存 */
    if (result != FR_OK) {  // 刷新失败
        remove_chain(&(dp_new->obj), *clust, 0);  // 释放已分配的簇
        return result;  // 返回错误
    }

    /* 初始化新簇 */
#ifndef LOSCFG_FS_FAT_VIRTUAL_PARTITION  // 非虚拟分区模式
    dir = fs->win;  // 使用文件系统窗口缓存
#else  // 虚拟分区模式
    dir = PARENTFS(fs)->win;  // 使用父文件系统窗口缓存
#endif

    sect = clst2sect(fs, *clust);  // 将簇号转换为扇区号
    mem_set(dir, 0, SS(fs));  // 清空扇区缓存
    if (type == AM_LNK && target) {  // 如果是符号链接且目标路径有效
        /* 写入符号链接目标路径 */
        (void)strcpy_s((char *)dir, SS(fs), target);
    } else {  // 目录类型
        /* 写入目录簇内容 */
        mem_set(dir, 0, SS(fs));  // 清空扇区缓存
        /* 创建"."目录项(当前目录) */
        mem_set(dir + DIR_Name, ' ', 11);  // 清空文件名区域
        dir[DIR_Name] = '.';  // 设置文件名
        dir[DIR_Attr] = AM_DIR;  // 设置目录属性
        st_clust(fs, dir, *clust);  // 设置起始簇号
        mem_cpy(dir + DIR_SIZE, dir, DIR_SIZE); /* 创建".."目录项(父目录) */
        dir[DIR_SIZE + 1] = '.'; /* 添加第二个点 */
        pclust = pdfp->fno.sclst;  // 获取父目录簇号
        /* FAT32根目录特殊处理：父目录簇号设为0 */
        if (fs->fs_type == FS_FAT32 && pclust == fs->dirbase) {
            pclust = 0;
        }
        st_clust(fs, dir + DIR_SIZE, pclust);  // 设置父目录簇号
    }

#ifndef LOSCFG_FS_FAT_VIRTUAL_PARTITION  // 非虚拟分区模式
    fs->winsect = sect++;  // 设置窗口扇区号并递增
    fs->wflag = 1;  // 标记窗口内容已修改
#else  // 虚拟分区模式
    PARENTFS(fs)->winsect = sect++;  // 设置父文件系统窗口扇区号
    PARENTFS(fs)->wflag = 1;  // 标记父窗口内容已修改
#endif
    result = sync_window(fs);  // 将窗口内容写入磁盘
    if (result != FR_OK) {  // 写入失败
        remove_chain(&(dp_new->obj), *clust, 0);  // 释放簇
        return result;  // 返回错误
    }

    /* 目录簇剩余扇区清零 */
    if (type == AM_DIR) {  // 如果是目录类型
        mem_set(dir, 0, SS(fs));  // 清空缓存
        /* 遍历剩余扇区 */
        for (n = fs->csize - 1; n > 0; n--) {
#ifndef LOSCFG_FS_FAT_VIRTUAL_PARTITION  // 非虚拟分区模式
            fs->winsect = sect++;  // 设置扇区号
            fs->wflag = 1;  // 标记修改
#else  // 虚拟分区模式
            PARENTFS(fs)->winsect = sect++;  // 设置父文件系统扇区号
            PARENTFS(fs)->wflag = 1;  // 标记修改
#endif
            result = sync_window(fs);  // 写入磁盘
            if (result != FR_OK) {  // 写入失败
                remove_chain(&(dp_new->obj), *clust, 0);  // 释放簇
                return result;  // 返回错误
            }
        }
    }

    return FR_OK;  // 初始化成功
}

/**
 * @brief 创建FAT文件系统对象
 * @details 创建文件、目录或符号链接等文件系统对象，并关联Vnode
 * @param parent 父目录Vnode
 * @param name 对象名称
 * @param mode 文件权限模式
 * @param vpp 输出参数，返回新创建的Vnode
 * @param type 对象类型(AM_ARC/AM_DIR/AM_LNK)
 * @param target 符号链接目标路径(仅对链接有效)
 * @return 0 - 成功，非0 - 错误码
 */
static int fatfs_create_obj(struct Vnode *parent, const char *name, int mode, struct Vnode **vpp,
                            BYTE type,  const char *target)
{
    struct Vnode *vp = NULL;          // 新Vnode指针
    FATFS *fs = (FATFS *)parent->originMount->data;  // 文件系统控制块
    DIR_FILE *dfp = (DIR_FILE *)parent->data;        // 父目录文件结构体
    FILINFO *finfo = &(dfp->fno);     // 文件信息结构体
    DIR_FILE *dfp_new = NULL;         // 新目录文件结构体
    FILINFO *finfo_new = NULL;        // 新文件信息结构体
    DIR *dp_new = NULL;               // 新目录结构体
    QWORD time;                       // 创建时间
    DWORD hash;                       // 哈希值
    DWORD clust = 0;                  // 分配的簇号
    FRESULT result;                   // FatFs操作结果
    int ret;                          // 返回值

    /* 检查对象类型合法性 */
    if ((type != AM_ARC) && (type != AM_DIR) && (type != AM_LNK)) {
        result = FR_INVALID_NAME;  // 无效类型
        goto ERROR_EXIT;  // 跳转到错误处理
    }

    /* 分配新目录文件结构体 */
    dfp_new = (DIR_FILE *)zalloc(sizeof(DIR_FILE));
    if (dfp_new == NULL) {  // 分配失败
        result = FR_NOT_ENOUGH_CORE;  // 内存不足
        goto ERROR_EXIT;  // 跳转到错误处理
    }

    ret = lock_fs(fs);  // 锁定文件系统
    if (ret == FALSE) { /* 锁定失败 */
        result = FR_TIMEOUT;  // 超时错误
        goto ERROR_FREE;  // 跳转到释放内存
    }

    /* 检查父目录是否有效 */
    if (finfo->fattrib & AM_ARC || finfo->fattrib & AM_LNK) {
        result = FR_NO_DIR;  // 不是目录
        goto ERROR_UNLOCK;  // 跳转到解锁
    }

    finfo_new = &(dfp_new->fno);  // 新文件信息
    LOS_ListInit(&finfo_new->fp_list);  // 初始化文件指针列表
    dp_new = &(dfp_new->f_dir);  // 新目录结构体
    dp_new->obj.fs = fs;  // 设置文件系统
    dp_new->obj.sclust = finfo->sclst;  // 设置起始簇号

    DEF_NAMBUF;  // 定义文件名缓冲区
    INIT_NAMBUF(fs);  // 初始化文件名缓冲区

    result = create_name(dp_new, &name);  // 创建文件名
    if (result != FR_OK) {  // 创建失败
        goto ERROR_UNLOCK;  // 跳转到解锁
    }

    result = dir_find(dp_new);  // 查找目录项
    if (result == FR_OK) {  // 已存在
        result = FR_EXIST;  // 文件已存在
        goto ERROR_UNLOCK;  // 跳转到解锁
    }

    /* 如果是目录或符号链接，初始化簇 */
    if (type == AM_DIR || type == AM_LNK) {
        result = init_cluster(dfp, dp_new, fs, type, target, &clust);
        if (result != FR_OK) {  // 初始化失败
            goto ERROR_UNLOCK;  // 跳转到解锁
        }
    }

    result = dir_register(dp_new);  // 注册目录项
    if (result != FR_OK) {  // 注册失败
        goto ERROR_REMOVE_CHAIN;  // 跳转到移除簇链
    }

    /* 设置目录项属性 */
    if (time_status == SYSTEM_TIME_ENABLE) {  // 如果系统时间可用
        time = GET_FATTIME();  // 获取当前时间
    } else {
        time = 0;  // 时间不可用，设为0
    }
    st_dword(dp_new->dir + DIR_CrtTime, time);  // 设置创建时间
    st_dword(dp_new->dir + DIR_ModTime, time);  // 设置修改时间
    st_word(dp_new->dir + DIR_LstAccDate, time >> FTIME_DATE_OFFSET);  // 设置最后访问日期
    dp_new->dir[DIR_Attr] = type;  // 设置文件类型
    /* 如果没有用户写权限，设置只读属性 */
    if (((DWORD)mode & S_IWUSR) == 0) {
        dp_new->dir[DIR_Attr] |= AM_RDO;  // 添加只读属性
    }
    st_clust(fs, dp_new->dir, clust);  // 设置起始簇号
    if (type == AM_ARC) {  // 普通文件
        st_dword(dp_new->dir + DIR_FileSize, 0);  // 文件大小设为0
    } else if (type == AM_LNK) {  // 符号链接
        st_dword(dp_new->dir + DIR_FileSize, strlen(target));  // 设置链接目标长度
    }

#ifdef LOSCFG_FS_FAT_VIRTUAL_PARTITION  // 虚拟分区模式
    PARENTFS(fs)->wflag = 1;  // 标记父文件系统缓存修改
#else  // 非虚拟分区模式
    fs->wflag = 1;  // 标记文件系统缓存修改
#endif
    result = sync_fs(fs);  // 同步文件系统
    if (result != FR_OK) {  // 同步失败
        goto ERROR_REMOVE_CHAIN;  // 跳转到移除簇链
    }
    result = dir_read(dp_new, 0);  // 读取目录项
    if (result != FR_OK) {  // 读取失败
        goto ERROR_REMOVE_CHAIN;  // 跳转到移除簇链
    }
    dp_new->blk_ofs = dir_ofs(dp_new);  // 设置块偏移
    get_fileinfo(dp_new, finfo_new);  // 获取文件信息
    if (type == AM_ARC) {  // 普通文件
        dp_new->obj.objsize = 0;  // 对象大小设为0
    } else if (type == AM_LNK) {  // 符号链接
        dp_new->obj.objsize = strlen(target);  // 设置链接目标长度
    } else {  // 目录
        finfo_new->fsize = fs->csize * SS(fs);  // 目录大小 = 簇大小 * 扇区大小
    }

    /* 分配新Vnode */
    ret = VnodeAlloc(&fatfs_vops, &vp);
    if (ret != 0) {  // 分配失败
        result = FR_NOT_ENOUGH_CORE;  // 内存不足
        goto ERROR_REMOVE_CHAIN;  // 跳转到移除簇链
    }

    /* 初始化Vnode属性 */
    vp->parent = parent;  // 设置父Vnode
    vp->fop = &fatfs_fops;  // 设置文件操作函数
    vp->data = dfp_new;  // 关联目录文件结构体
    vp->originMount = parent->originMount;  // 设置挂载点
    vp->uid = fs->fs_uid;  // 设置用户ID
    vp->gid = fs->fs_gid;  // 设置组ID
    vp->mode = fatfs_get_mode(finfo_new->fattrib, fs->fs_mode);  // 设置文件模式
    vp->type = fatfstype_2_vnodetype(type);  // 设置Vnode类型

    /* 计算哈希并插入哈希表 */
    hash = fatfs_hash(dp_new->sect, dp_new->dptr, finfo_new->sclst);
    ret = VfsHashInsert(vp, hash);
    if (ret != 0) {  // 插入失败
        result = FR_NOT_ENOUGH_CORE;  // 内存不足
        goto ERROR_REMOVE_CHAIN;  // 跳转到移除簇链
    }
    *vpp = vp;  // 返回新Vnode

    unlock_fs(fs, FR_OK);  // 解锁文件系统
    FREE_NAMBUF();  // 释放文件名缓冲区
    return fatfs_sync(parent->originMount->mountFlags, fs);  // 同步并返回

ERROR_REMOVE_CHAIN:
    remove_chain(&(dp_new->obj), clust, 0);  // 移除已分配的簇链
ERROR_UNLOCK:
    unlock_fs(fs, result);  // 解锁文件系统
    FREE_NAMBUF();  // 释放文件名缓冲区
ERROR_FREE:
    free(dfp_new);  // 释放目录文件结构体
ERROR_EXIT:
    return -fatfs_2_vfs(result);  // 转换错误码并返回
}

/**
 * @brief 查找文件系统对象
 * @details 根据路径查找文件或目录，并创建对应的Vnode
 * @param parent 父目录Vnode
 * @param path 要查找的路径
 * @param len 路径长度
 * @param vpp 输出参数，返回找到的Vnode
 * @return 0 - 成功，非0 - 错误码
 */
int fatfs_lookup(struct Vnode *parent, const char *path, int len, struct Vnode **vpp)
{
    struct Vnode *vp = NULL;          // 查找到的Vnode
    FATFS *fs = (FATFS *)(parent->originMount->data);  // 文件系统控制块
    DIR_FILE *dfp;                    // 目录文件结构体
    DIR *dp = NULL;                   // 目录结构体
    FILINFO *finfo = NULL;            // 文件信息结构体
    DWORD hash;                       // 哈希值
    FRESULT result;                   // FatFs操作结果
    int ret;                          // 返回值

    /* 分配目录文件结构体 */
    dfp = (DIR_FILE *)zalloc(sizeof(DIR_FILE));
    if (dfp == NULL) {  // 分配失败
        ret = ENOMEM;  // 内存不足
        goto ERROR_EXIT;  // 跳转到错误处理
    }

    ret = lock_fs(fs);  // 锁定文件系统
    if (ret == FALSE) {  // 锁定失败
        ret = EBUSY;  // 资源忙
        goto ERROR_FREE;  // 跳转到释放内存
    }
    finfo = &(dfp->fno);  // 文件信息
    LOS_ListInit(&finfo->fp_list);  // 初始化文件指针列表
    dp = &(dfp->f_dir);  // 目录结构体
    dp->obj.fs = fs;  // 设置文件系统
    dp->obj.sclust = ((DIR_FILE *)(parent->data))->fno.sclst;  // 设置起始簇号

    DEF_NAMBUF;  // 定义文件名缓冲区
    INIT_NAMBUF(fs);  // 初始化文件名缓冲区
    result = create_name(dp, &path);  // 创建文件名
    if (result != FR_OK) {  // 创建失败
        ret = fatfs_2_vfs(result);  // 转换错误码
        goto ERROR_UNLOCK;  // 跳转到解锁
    }

    result = dir_find(dp);  // 查找目录项
    if (result != FR_OK) {  // 查找失败
        ret = fatfs_2_vfs(result);  // 转换错误码
        goto ERROR_UNLOCK;  // 跳转到解锁
    }

    if (dp->fn[NSFLAG] & NS_NONAME) {  // 无效文件名
        result = FR_INVALID_NAME;  // 无效名称
        ret = fatfs_2_vfs(result);  // 转换错误码
        goto ERROR_UNLOCK;  // 跳转到解锁
    }

    get_fileinfo(dp, finfo);  // 获取文件信息
    dp->obj.objsize = 0;  // 初始化对象大小

    /* 计算哈希并查找Vnode缓存 */
    hash = fatfs_hash(dp->sect, dp->dptr, finfo->sclst);
    ret = VfsHashGet(parent->originMount, hash, &vp, fatfs_hash_cmp, dfp);
    if (ret != 0) {  // 缓存未命中
        /* 分配新Vnode */
        ret = VnodeAlloc(&fatfs_vops, &vp);
        if (ret != 0) {  // 分配失败
            ret = ENOMEM;  // 内存不足
            result = FR_NOT_ENOUGH_CORE;
            goto ERROR_UNLOCK;  // 跳转到解锁
        }
        /* 初始化Vnode属性 */
        vp->parent = parent;  // 设置父Vnode
        vp->fop = &fatfs_fops;  // 设置文件操作函数
        vp->data = dfp;  // 关联目录文件结构体
        vp->originMount = parent->originMount;  // 设置挂载点
        vp->uid = fs->fs_uid;  // 设置用户ID
        vp->gid = fs->fs_gid;  // 设置组ID
        vp->mode = fatfs_get_mode(finfo->fattrib, fs->fs_mode);  // 设置文件模式
        if (finfo->fattrib & AM_DIR) {  // 如果是目录
            vp->type = VNODE_TYPE_DIR;  // 设置目录类型
            finfo->fsize = fs->csize * SS(fs);  // 目录大小 = 簇大小 * 扇区大小
        } else {  // 普通文件
            vp->type = VNODE_TYPE_REG;  // 设置普通文件类型
        }

        /* 插入Vnode到哈希表 */
        ret = VfsHashInsert(vp, hash);
        if (ret != 0) {  // 插入失败
            result = FR_INVALID_PARAMETER;  // 参数无效
            goto ERROR_UNLOCK;  // 跳转到解锁
        }
    } else {  // 缓存命中
        vp->parent = parent;  // 设置父Vnode
        free(dfp); /* 缓存命中，释放临时目录文件结构体 */
    }

    unlock_fs(fs, FR_OK);  // 解锁文件系统
    FREE_NAMBUF();  // 释放文件名缓冲区
    *vpp = vp;  // 返回查找到的Vnode
    return 0;  // 成功

ERROR_UNLOCK:
    unlock_fs(fs, result);  // 解锁文件系统
    FREE_NAMBUF();  // 释放文件名缓冲区
ERROR_FREE:
    free(dfp);  // 释放目录文件结构体
ERROR_EXIT:
    return -ret;  // 返回错误码
}

/**
 * @brief 创建普通文件
 * @details 调用通用对象创建函数创建普通文件
 * @param parent 父目录Vnode
 * @param name 文件名
 * @param mode 文件权限模式
 * @param vpp 输出参数，返回新创建的Vnode
 * @return 0 - 成功，非0 - 错误码
 */
int fatfs_create(struct Vnode *parent, const char *name, int mode, struct Vnode **vpp)
{
    /* 调用通用创建函数，类型为普通文件(AM_ARC) */
    return fatfs_create_obj(parent, name, mode, vpp, AM_ARC, NULL);
}

/**
 * @brief 打开文件
 * @details 初始化文件结构体，准备文件读写操作
 * @param filep 文件结构体指针
 * @return 0 - 成功，非0 - 错误码
 */
int fatfs_open(struct file *filep)
{
    struct Vnode *vp = filep->f_vnode;  // 文件对应的Vnode
    FATFS *fs = (FATFS *)vp->originMount->data;  // 文件系统控制块
    DIR_FILE *dfp = (DIR_FILE *)vp->data;  // 目录文件结构体
    DIR *dp = &(dfp->f_dir);  // 目录结构体
    FILINFO *finfo = &(dfp->fno);  // 文件信息
    FIL *fp;  // 文件结构体
    int ret;  // 返回值

    /* 分配文件结构体(包含扇区大小的缓冲区) */
    fp = (FIL *)zalloc(sizeof(FIL) + SS(fs));
    if (fp == NULL) {  // 分配失败
        ret = ENOMEM;  // 内存不足
        goto ERROR_EXIT;  // 跳转到错误处理
    }
    ret = lock_fs(fs);  // 锁定文件系统
    if (ret == FALSE) {  // 锁定失败
        ret = EBUSY;  // 资源忙
        goto ERROR_FREE;  // 跳转到释放内存
    }

    /* 初始化文件结构体 */
    fp->dir_sect = dp->sect;  // 目录项扇区号
    fp->dir_ptr = dp->dir;    // 目录项指针
    fp->obj.sclust = finfo->sclst;  // 起始簇号
    fp->obj.objsize = finfo->fsize;  // 文件大小
#if FF_USE_FASTSEEK
    fp->cltbl = 0; /* 禁用快速查找模式 */
#endif
    fp->obj.fs = fs;  // 文件系统控制块
    fp->obj.id = fs->id;  // 文件系统ID
    fp->flag = FA_READ | FA_WRITE;  // 读写模式
    fp->err = 0;  // 错误码
    fp->sect = 0;  // 当前扇区
    fp->fptr = 0;  // 文件指针
    fp->buf = (BYTE *)fp + sizeof(FIL);  // 数据缓冲区
    LOS_ListAdd(&finfo->fp_list, &fp->fp_entry);  // 添加到文件指针列表
    unlock_fs(fs, FR_OK);  // 解锁文件系统

    filep->f_priv = fp;  // 关联文件结构体
    return 0;  // 成功

ERROR_FREE:
    free(fp);  // 释放文件结构体
ERROR_EXIT:
    return -ret;  // 返回错误码
}

/**
 * @brief 关闭文件
 * @details 刷新缓存，释放文件结构体资源
 * @param filep 文件结构体指针
 * @return 0 - 成功，非0 - 错误码
 */
int fatfs_close(struct file *filep)
{
    FIL *fp = (FIL *)filep->f_priv;  // 文件结构体
    FATFS *fs = fp->obj.fs;  // 文件系统控制块
    FRESULT result;  // FatFs操作结果
    int ret;  // 返回值

    ret = lock_fs(fs);  // 锁定文件系统
    if (ret == FALSE) {  // 锁定失败
        return -EBUSY;  // 返回资源忙错误
    }
#if !FF_FS_READONLY  // 非只读文件系统
    result = f_sync(fp); /* 刷新文件缓存 */
    if (result != FR_OK) {  // 刷新失败
        goto EXIT;  // 跳转到解锁
    }
    /* 同步文件系统 */
    ret = fatfs_sync(filep->f_vnode->originMount->mountFlags, fs);
    if (ret != 0) {  // 同步失败
        unlock_fs(fs, FR_OK);  // 解锁
        return ret;  // 返回错误
    }
#endif
    LOS_ListDelete(&fp->fp_entry);  // 从文件指针列表移除
    free(fp);  // 释放文件结构体
    filep->f_priv = NULL;  // 清空私有指针
EXIT:
    unlock_fs(fs, result);  // 解锁文件系统
    return -fatfs_2_vfs(result);  // 转换错误码并返回
}

/**
 * @brief 读取文件数据
 * @details 从文件读取指定长度的数据到缓冲区
 * @param filep 文件结构体指针
 * @param buff 数据缓冲区
 * @param count 要读取的字节数
 * @return 实际读取的字节数，负数表示错误
 */
int fatfs_read(struct file *filep, char *buff, size_t count)
{
    FIL *fp = (FIL *)filep->f_priv;  // 文件结构体
    FATFS *fs = fp->obj.fs;  // 文件系统控制块
    struct Vnode *vp = filep->f_vnode;  // 文件Vnode
    FILINFO *finfo = &((DIR_FILE *)(vp->data))->fno;  // 文件信息
    size_t rcount;  // 实际读取字节数
    FRESULT result;  // FatFs操作结果
    int ret;  // 返回值

    ret = lock_fs(fs);  // 锁定文件系统
    if (ret == FALSE) {  // 锁定失败
        return -EBUSY;  // 返回资源忙错误
    }
    fp->obj.objsize = finfo->fsize;  // 更新对象大小
    fp->obj.sclust = finfo->sclst;  // 更新起始簇号
    result = f_read(fp, buff, count, &rcount);  // 读取数据
    if (result != FR_OK) {  // 读取失败
        goto EXIT;  // 跳转到解锁
    }
    filep->f_pos = fp->fptr;  // 更新文件位置
EXIT:
    unlock_fs(fs, result);  // 解锁文件系统
    return rcount;  // 返回实际读取字节数
}
/**
 * @brief 更新目录项信息
 * @details 将文件信息更新到目录项中，包括文件属性、起始簇、大小和修改时间等
 * @param dp 目录对象指针
 * @param finfo 文件信息结构体指针
 * @return FRESULT - 成功返回FR_OK，失败返回对应错误码
 */
static FRESULT update_dir(DIR *dp, FILINFO *finfo)
{
    FATFS *fs = dp->obj.fs;        // 获取文件系统对象
    DWORD tm;                      // 时间变量
    BYTE *dbuff = NULL;            // 目录项缓冲区指针
    FRESULT result;                // 函数返回结果

    result = move_window(fs, dp->sect);  // 将目录扇区加载到窗口
    if (result != FR_OK) {               // 检查扇区加载是否成功
        return result;                   // 失败则返回错误码
    }
    dbuff = fs->win + dp->dptr % SS(fs); // 计算目录项在窗口中的位置
    dbuff[DIR_Attr] = finfo->fattrib;    // 更新文件属性
    st_clust(fs, dbuff, finfo->sclst);   // 更新起始簇号
    st_dword(dbuff + DIR_FileSize, (DWORD)finfo->fsize); // 更新文件大小
    if (time_status == SYSTEM_TIME_ENABLE) {             // 检查系统时间是否启用
        tm = GET_FATTIME();                              // 获取当前FAT时间
    } else {
        tm = 0;                                          // 未启用则时间设为0
    }
    st_dword(dbuff + DIR_ModTime, tm);   // 更新修改时间
    st_word(dbuff + DIR_LstAccDate, tm >> FTIME_DATE_OFFSET); // 更新访问日期
#ifndef LOSCFG_FS_FAT_VIRTUAL_PARTITION
    fs->wflag = 1;                       // 非虚拟分区：设置文件系统写标志
#else
    PARENTFS(fs)->wflag = 1;             // 虚拟分区：设置父文件系统写标志
#endif
    return sync_fs(fs);                  // 同步文件系统并返回结果
}

/**
 * @brief 64位文件指针定位
 * @details 根据偏移量和起始位置调整文件指针，支持SEEK_SET/SEEK_CUR/SEEK_END三种模式
 * @param filep 文件结构体指针
 * @param offset 偏移量
 * @param whence 起始位置
 * @return off64_t - 成功返回新的文件指针位置，失败返回-错误码
 */
off64_t fatfs_lseek64(struct file *filep, off64_t offset, int whence)
{
    FIL *fp = (FIL *)filep->f_priv;      // 获取FAT文件对象
    FATFS *fs = fp->obj.fs;              // 获取文件系统对象
    struct Vnode *vp = filep->f_vnode;   // 获取Vnode节点
    DIR_FILE *dfp = (DIR_FILE *)vp->data;// 获取目录文件数据
    FILINFO *finfo = &(dfp->fno);        // 获取文件信息
    struct Mount *mount = vp->originMount; // 获取挂载点信息
    FSIZE_t fpos;                        // 文件位置
    FRESULT result;                      // 函数返回结果
    int ret;                             // 临时返回值

    switch (whence) {                    // 根据起始位置类型处理偏移量
        case SEEK_CUR:                   // 从当前位置开始
            offset = filep->f_pos + offset; // 计算新偏移量
            if (offset < 0) {            // 检查偏移量是否合法
                return -EINVAL;          // 非法则返回参数错误
            }
            fpos = offset;               // 设置文件位置
            break;
        case SEEK_SET:                   // 从文件开头开始
            if (offset < 0) {            // 检查偏移量是否合法
                return -EINVAL;          // 非法则返回参数错误
            }
            fpos = offset;               // 设置文件位置
            break;
        case SEEK_END:                   // 从文件末尾开始
            offset = (off_t)((long long)finfo->fsize + offset); // 计算新偏移量
            if (offset < 0) {            // 检查偏移量是否合法
                return -EINVAL;          // 非法则返回参数错误
            }
            fpos = offset;               // 设置文件位置
            break;
        default:                         // 不支持的模式
            return -EINVAL;              // 返回参数错误
    }

    if (offset >= FAT32_MAXSIZE) {       // 检查偏移量是否超过FAT32最大文件大小
        return -EINVAL;                  // 超过则返回参数错误
    }

    ret = lock_fs(fs);                   // 锁定文件系统
    if (ret == FALSE) {                  // 检查锁定是否成功
        return -EBUSY;                   // 失败则返回设备忙
    }

    if (fpos > finfo->fsize) {           // 如果新位置超过文件大小
        if ((filep->f_oflags & O_ACCMODE) == O_RDONLY) { // 检查文件是否为只读模式
            result = FR_DENIED;          // 设置拒绝访问错误
            goto ERROR_EXIT;             // 跳转到错误处理
        }
        if (mount->mountFlags & MS_RDONLY) { // 检查文件系统是否为只读
            result = FR_WRITE_PROTECTED; // 设置写保护错误
            goto ERROR_EXIT;             // 跳转到错误处理
        }
    }
    fp->obj.sclust = finfo->sclst;       // 更新文件对象起始簇
    fp->obj.objsize = finfo->fsize;      // 更新文件对象大小

    result = f_lseek(fp, fpos);          // 调用FatFs库函数定位文件指针
    finfo->fsize = fp->obj.objsize;      // 更新文件大小信息
    finfo->sclst = fp->obj.sclust;       // 更新文件起始簇信息
    if (result != FR_OK) {               // 检查定位是否成功
        goto ERROR_EXIT;                 // 失败则跳转到错误处理
    }

    result = f_sync(fp);                 // 同步文件数据到磁盘
    if (result != FR_OK) {               // 检查同步是否成功
        goto ERROR_EXIT;                 // 失败则跳转到错误处理
    }
    filep->f_pos = fpos;                 // 更新VFS文件位置

    unlock_fs(fs, FR_OK);                // 解锁文件系统
    return fpos;                         // 返回新的文件位置
ERROR_EXIT:
    unlock_fs(fs, result);               // 解锁文件系统并传递错误码
    return -fatfs_2_vfs(result);         // 转换错误码并返回
}

/**
 * @brief 32位文件指针定位
 * @details 兼容32位系统的文件指针定位函数，内部调用64位版本实现
 * @param filep 文件结构体指针
 * @param offset 偏移量
 * @param whence 起始位置
 * @return off_t - 成功返回新的文件指针位置，失败返回-错误码
 */
off_t fatfs_lseek(struct file *filep, off_t offset, int whence)
{
    return (off_t)fatfs_lseek64(filep, offset, whence); // 调用64位版本并转换返回值
}

/**
 * @brief 更新文件缓冲区
 * @details 当文件内容被修改时，更新其他打开的文件句柄对应的缓冲区
 * @param finfo 文件信息结构体指针
 * @param wfp 当前写文件指针
 * @param data 写入的数据
 * @return int - 成功返回0，失败返回-1
 */
static int update_filbuff(FILINFO *finfo, FIL *wfp, const char  *data)
{
    LOS_DL_LIST *list = &finfo->fp_list; // 获取文件指针链表
    FATFS *fs = wfp->obj.fs;             // 获取文件系统对象
    FIL *entry = NULL;                   // 链表遍历入口
    int ret = 0;                         // 函数返回值

    // 遍历所有打开的文件指针
    LOS_DL_LIST_FOR_EACH_ENTRY(entry, list, FIL, fp_entry) {
        if (entry == wfp) {              // 跳过当前写文件指针
            continue;
        }
        if (entry->sect != 0) {          // 如果缓冲区有效
            // 重新读取扇区数据到缓冲区
            if (disk_read(fs->pdrv, entry->buf, entry->sect, 1) != RES_OK) {
                ret = -1;                // 读取失败则设置错误码
            }
        }
    }

    return ret;                          // 返回结果
}

/**
 * @brief 写入文件数据
 * @details 将数据写入文件，并更新文件大小和相关元数据
 * @param filep 文件结构体指针
 * @param buff 数据缓冲区指针
 * @param count 要写入的字节数
 * @return int - 成功返回实际写入的字节数，失败返回-错误码
 */
int fatfs_write(struct file *filep, const char *buff, size_t count)
{
    FIL *fp = (FIL *)filep->f_priv;      // 获取FAT文件对象
    FATFS *fs = fp->obj.fs;              // 获取文件系统对象
    struct Vnode *vp = filep->f_vnode;   // 获取Vnode节点
    FILINFO *finfo = &(((DIR_FILE *)vp->data)->fno); // 获取文件信息
    size_t wcount;                       // 实际写入字节数
    FRESULT result;                      // 函数返回结果
    int ret;                             // 临时返回值

    ret = lock_fs(fs);                   // 锁定文件系统
    if (ret == FALSE) {                  // 检查锁定是否成功
        return -EBUSY;                   // 失败则返回设备忙
    }
    fp->obj.objsize = finfo->fsize;      // 更新文件对象大小
    fp->obj.sclust = finfo->sclst;       // 更新文件对象起始簇
    result = f_write(fp, buff, count, &wcount); // 调用FatFs库函数写入数据
    if (result != FR_OK) {               // 检查写入是否成功
        goto ERROR_EXIT;                 // 失败则跳转到错误处理
    }

    finfo->fsize = fp->obj.objsize;      // 更新文件大小信息
    finfo->sclst = fp->obj.sclust;       // 更新文件起始簇信息
    result = f_sync(fp);                 // 同步文件数据到磁盘
    if (result != FR_OK) {               // 检查同步是否成功
        goto ERROR_EXIT;                 // 失败则跳转到错误处理
    }
    update_filbuff(finfo, fp, buff);     // 更新其他文件句柄的缓冲区

    filep->f_pos = fp->fptr;             // 更新VFS文件位置

    unlock_fs(fs, FR_OK);                // 解锁文件系统
    return wcount;                       // 返回实际写入字节数
ERROR_EXIT:
    unlock_fs(fs, result);               // 解锁文件系统并传递错误码
    return -fatfs_2_vfs(result);         // 转换错误码并返回
}

/**
 * @brief 同步文件数据
 * @details 将文件缓存中的数据同步到磁盘，确保数据持久化
 * @param filep 文件结构体指针
 * @return int - 成功返回0，失败返回-错误码
 */
int fatfs_fsync(struct file *filep)
{
    FIL *fp = filep->f_priv;             // 获取FAT文件对象
    FATFS *fs = fp->obj.fs;              // 获取文件系统对象
    FRESULT result;                      // 函数返回结果
    int ret;                             // 临时返回值

    ret = lock_fs(fs);                   // 锁定文件系统
    if (ret == FALSE) {                  // 检查锁定是否成功
        return -EBUSY;                   // 失败则返回设备忙
    }

    result = f_sync(fp);                 // 调用FatFs库函数同步文件
    unlock_fs(fs, result);               // 解锁文件系统
    return -fatfs_2_vfs(result);         // 转换错误码并返回
}

/**
 * @brief 64位文件空间预分配
 * @details 为文件预分配指定大小的磁盘空间，仅支持FALLOC_FL_KEEP_SIZE模式
 * @param filep 文件结构体指针
 * @param mode 分配模式（仅支持FALLOC_FL_KEEP_SIZE）
 * @param offset 起始偏移量
 * @param len 要分配的长度
 * @return int - 成功返回0，失败返回-错误码
 */
int fatfs_fallocate64(struct file *filep, int mode, off64_t offset, off64_t len)
{
    FIL *fp = (FIL *)filep->f_priv;      // 获取FAT文件对象
    FATFS *fs = fp->obj.fs;              // 获取文件系统对象
    struct Vnode *vp = filep->f_vnode;   // 获取Vnode节点
    FILINFO *finfo = &((DIR_FILE *)(vp->data))->fno; // 获取文件信息
    FRESULT result;                      // 函数返回结果
    int ret;                             // 临时返回值

    if (offset < 0 || len <= 0) {        // 检查偏移量和长度是否合法
        return -EINVAL;                  // 非法则返回参数错误
    }

    // 检查是否超过FAT32最大文件大小限制
    if (len >= FAT32_MAXSIZE || offset >= FAT32_MAXSIZE ||
        len + offset >= FAT32_MAXSIZE) {
        return -EINVAL;                  // 超过则返回参数错误
    }

    if (mode != FALLOC_FL_KEEP_SIZE) {   // 检查分配模式是否支持
        return -EINVAL;                  // 不支持则返回参数错误
    }

    ret = lock_fs(fs);                   // 锁定文件系统
    if (ret == FALSE) {                  // 检查锁定是否成功
        return -EBUSY;                   // 失败则返回设备忙
    }
    // 调用FatFs库函数扩展文件空间
    result = f_expand(fp, (FSIZE_t)offset, (FSIZE_t)len, 1);
    if (result == FR_OK) {               // 检查扩展是否成功
        if (finfo->sclst == 0) {         // 如果文件尚未分配簇
            finfo->sclst = fp->obj.sclust; // 更新文件起始簇
        }
        result = f_sync(fp);             // 同步文件数据到磁盘
    }
    unlock_fs(fs, result);               // 解锁文件系统

    return -fatfs_2_vfs(result);         // 转换错误码并返回
}

/**
 * @brief 重新分配文件簇链
 * @details 根据指定大小调整文件的簇链，支持扩展和收缩操作
 * @param finfo 文件信息结构体指针
 * @param obj 文件系统对象ID
 * @param size 目标大小
 * @return FRESULT - 成功返回FR_OK，失败返回对应错误码
 */
static FRESULT realloc_cluster(FILINFO *finfo, FFOBJID *obj, FSIZE_t size)
{
    FATFS *fs = obj->fs;                 // 获取文件系统对象
    off64_t remain;                      // 剩余需要分配的大小
    DWORD cclust;                        // 当前簇号
    DWORD pclust;                        // 前一个簇号
    QWORD csize;                         // 每簇字节数
    FRESULT result;                      // 函数返回结果

    if (size == 0) {                     // 如果目标大小为0（删除文件）
        if (finfo->sclst != 0) {         // 如果文件已有簇链
            // 删除簇链
            result = remove_chain(obj, finfo->sclst, 0);
            if (result != FR_OK) {       // 检查删除是否成功
                return result;           // 失败则返回错误码
            }
            finfo->sclst = 0;            // 重置起始簇
        }
        return FR_OK;                    // 返回成功
    }

    remain = size;                       // 初始化剩余大小
    csize = SS(fs) * fs->csize;          // 计算每簇字节数 = 扇区大小 * 每簇扇区数
    if (finfo->sclst == 0) {             // 如果文件尚未分配簇
        cclust = create_chain(obj, 0);   // 创建新簇链
        if (cclust == 0) {               // 检查是否成功分配
            return FR_NO_SPACE_LEFT;     // 失败则返回空间不足
        }
        if (cclust == 1 || cclust == DISK_ERROR) { // 检查是否有磁盘错误
            return FR_DISK_ERR;          // 返回磁盘错误
        }
        finfo->sclst = cclust;           // 设置起始簇
    }
    cclust = finfo->sclst;               // 从起始簇开始
    while (remain > csize) {             // 当剩余大小大于一簇
        pclust = cclust;                 // 保存当前簇
        cclust = create_chain(obj, pclust); // 扩展簇链
        if (cclust == 0) {               // 检查是否成功分配
            return FR_NO_SPACE_LEFT;     // 失败则返回空间不足
        }
        if (cclust == 1 || cclust == DISK_ERROR) { // 检查是否有磁盘错误
            return FR_DISK_ERR;          // 返回磁盘错误
        }
        remain -= csize;                 // 减少剩余大小
    }
    pclust = cclust;                     // 保存当前簇
    cclust = get_fat(obj, pclust);       // 获取下一簇
    if ((cclust == BAD_CLUSTER) || (cclust == DISK_ERROR)) { // 检查簇是否损坏
        return FR_DISK_ERR;              // 返回磁盘错误
    }
    // 如果不是最后一簇，则删除多余簇链
    if (!fatfs_is_last_cluster(obj->fs, cclust)) {
        result = remove_chain(obj, cclust, pclust); // 删除多余簇
        if (result != FR_OK) {           // 检查删除是否成功
            return result;               // 失败则返回错误码
        }
    }

    return FR_OK;                        // 返回成功
}

/**
 * @brief 32位文件空间预分配
 * @details 兼容32位系统的文件空间预分配函数，内部调用64位版本实现
 * @param filep 文件结构体指针
 * @param mode 分配模式
 * @param offset 起始偏移量
 * @param len 要分配的长度
 * @return int - 成功返回0，失败返回-错误码
 */
int fatfs_fallocate(struct file *filep, int mode, off_t offset, off_t len)
{
    return fatfs_fallocate64(filep, mode, offset, len); // 调用64位版本
}

/**
 * @brief 64位文件截断
 * @details 将文件大小截断为指定长度，可能释放多余的磁盘空间
 * @param vp Vnode节点指针
 * @param len 目标长度
 * @return int - 成功返回0，失败返回-错误码
 */
int fatfs_truncate64(struct Vnode *vp, off64_t len)
{
    FATFS *fs = (FATFS *)vp->originMount->data; // 获取文件系统对象
    DIR_FILE *dfp = (DIR_FILE *)vp->data;       // 获取目录文件数据
    DIR *dp = &(dfp->f_dir);                    // 获取目录对象
    FILINFO *finfo = &(dfp->fno);               // 获取文件信息
    FFOBJID object;                             // 文件系统对象ID
    FRESULT result = FR_OK;                     // 函数返回结果
    int ret;                                    // 临时返回值

    if (len < 0 || len >= FAT32_MAXSIZE) {      // 检查目标长度是否合法
        return -EINVAL;                         // 非法则返回参数错误
    }

    ret = lock_fs(fs);                          // 锁定文件系统
    if (ret == FALSE) {                         // 检查锁定是否成功
        result = FR_TIMEOUT;                    // 设置超时错误
        goto ERROR_OUT;                         // 跳转到错误处理
    }
    if (len == finfo->fsize) {                  // 如果目标长度等于当前大小
        unlock_fs(fs, FR_OK);                   // 解锁文件系统
        return 0;                               // 直接返回成功
    }

    object.fs = fs;                             // 设置文件系统对象
    // 重新分配簇链以匹配目标大小
    result = realloc_cluster(finfo, &object, (FSIZE_t)len);
    if (result != FR_OK) {                      // 检查重新分配是否成功
        goto ERROR_UNLOCK;                      // 失败则跳转到错误处理
    }
    finfo->fsize = (FSIZE_t)len;                // 更新文件大小

    result = update_dir(dp, finfo);             // 更新目录项信息
    if (result != FR_OK) {                      // 检查目录项更新是否成功
        goto ERROR_UNLOCK;                      // 失败则跳转到错误处理
    }
    unlock_fs(fs, FR_OK);                       // 解锁文件系统
    return fatfs_sync(vp->originMount->mountFlags, fs); // 同步文件系统
ERROR_UNLOCK:
    unlock_fs(fs, result);                      // 解锁文件系统并传递错误码
ERROR_OUT:
    return -fatfs_2_vfs(result);                // 转换错误码并返回
}

/**
 * @brief 32位文件截断
 * @details 兼容32位系统的文件截断函数，内部调用64位版本实现
 * @param vp Vnode节点指针
 * @param len 目标长度
 * @return int - 成功返回0，失败返回-错误码
 */
int fatfs_truncate(struct Vnode *vp, off_t len)
{
    return fatfs_truncate64(vp, len); // 调用64位版本
}

/**
 * @brief 绑定块设备检查
 * @details 检查块设备是否可以绑定到FAT文件系统，并获取分区信息
 * @param blk_driver 块设备Vnode指针
 * @param partition 输出参数，分区信息指针
 * @return int - 成功返回0，失败返回错误码
 */
static int fat_bind_check(struct Vnode *blk_driver, los_part **partition)
{
    los_part *part = NULL;                     // 分区信息指针

    if (blk_driver == NULL || blk_driver->data == NULL) { // 检查块设备是否有效
        return ENODEV;                         // 返回设备不存在错误
    }

    struct drv_data *dd = blk_driver->data;    // 获取驱动数据
    if (dd->ops == NULL) {                     // 检查驱动操作是否存在
        return ENODEV;                         // 返回设备不存在错误
    }
    const struct block_operations *bops = dd->ops; // 获取块设备操作
    if (bops->open == NULL) {                  // 检查open操作是否存在
        return EINVAL;                         // 返回参数错误
    }
    if (bops->open(blk_driver) < 0) {          // 尝试打开块设备
        return EBUSY;                          // 失败则返回设备忙
    }

    part = los_part_find(blk_driver);          // 查找分区
    if (part == NULL) {                        // 检查分区是否存在
        return ENODEV;                         // 返回设备不存在错误
    }
    if (part->part_name != NULL) {             // 检查分区是否已命名
        bops->close(blk_driver);               // 关闭块设备
        return EBUSY;                          // 返回设备忙
    }

#ifndef FF_MULTI_PARTITION
    if (part->part_no_mbr > 1) {               // 检查是否支持多分区
        bops->close(blk_driver);               // 关闭块设备
        return EPERM;                          // 返回权限错误
    }
#endif

    *partition = part;                         // 设置输出分区指针
    return 0;                                  // 返回成功
}

/**
 * @brief FAT文件系统挂载
 * @details 挂载FAT文件系统到指定挂载点，包括设备绑定、文件系统初始化和Vnode创建等
 * @param mnt 挂载点结构体指针
 * @param blk_device 块设备Vnode指针
 * @param data 挂载参数（未使用）
 * @return int - 成功返回0，失败返回-错误码
 */
int fatfs_mount(struct Mount *mnt, struct Vnode *blk_device, const void *data)
{
    struct Vnode *vp = NULL;                   // Vnode节点指针
    FATFS *fs = NULL;                          // FAT文件系统对象
    DIR_FILE *dfp = NULL;                      // 目录文件数据
    los_part *part = NULL;                     // 分区信息指针
    QWORD start_sector;                        // 分区起始扇区
    BYTE fmt;                                  // 文件系统格式
    DWORD hash;                                // Vnode哈希值
    FRESULT result;                            // 函数返回结果
    int ret;                                   // 临时返回值

    ret = fat_bind_check(blk_device, &part);   // 检查块设备绑定
    if (ret != 0) {                            // 检查绑定是否成功
        goto ERROR_EXIT;                       // 失败则跳转到错误处理
    }

    ret = SetDiskPartName(part, "vfat");       // 设置分区名称
    if (ret != 0) {                            // 检查设置是否成功
        ret = EIO;                             // 设置I/O错误
        goto ERROR_EXIT;                       // 失败则跳转到错误处理
    }

    fs = (FATFS *)zalloc(sizeof(FATFS));       // 分配文件系统对象内存
    if (fs == NULL) {                          // 检查内存分配是否成功
        ret = ENOMEM;                          // 设置内存不足错误
        goto ERROR_PARTNAME;                   // 失败则跳转到错误处理
    }

#ifdef LOSCFG_FS_FAT_VIRTUAL_PARTITION
    fs->vir_flag = FS_PARENT;                  // 设置虚拟分区标志
    fs->parent_fs = fs;                        // 设置父文件系统
    fs->vir_amount = DISK_ERROR;               // 初始化虚拟分区数量
    fs->vir_avail = FS_VIRDISABLE;             // 禁用虚拟分区
#endif

    ret = ff_cre_syncobj(0, &fs->sobj);        // 创建同步对象
    if (ret == 0) {                            // 检查创建是否成功
        ret = EINVAL;                          // 设置参数错误
        goto ERROR_WITH_FS;                    // 失败则跳转到错误处理
    }

    ret = lock_fs(fs);                         // 锁定文件系统
    if (ret == FALSE) {                        // 检查锁定是否成功
        ret = EBUSY;                           // 设置设备忙错误
        goto ERROR_WITH_MUX;                   // 失败则跳转到错误处理
    }

    fs->fs_type = 0;                           // 初始化文件系统类型
    fs->pdrv = part->part_id;                  // 设置物理驱动器号

#if FF_MAX_SS != FF_MIN_SS                     // 如果支持多种扇区大小
    // 获取扇区大小
    if (disk_ioctl(fs->pdrv, GET_SECTOR_SIZE, &(fs->ssize)) != RES_OK) {
        ret = EIO;                             // 设置I/O错误
        goto ERROR_WITH_LOCK;                  // 失败则跳转到错误处理
    }
    // 检查扇区大小是否合法
    if (fs->ssize > FF_MAX_SS || fs->ssize < FF_MIN_SS || (fs->ssize & (fs->ssize - 1))) {
        ret = EIO;                             // 设置I/O错误
        goto ERROR_WITH_LOCK;                  // 失败则跳转到错误处理
    }
#endif

    fs->win = (BYTE *)ff_memalloc(SS(fs));     // 分配扇区窗口内存
    if (fs->win == NULL) {                     // 检查内存分配是否成功
        ret = ENOMEM;                          // 设置内存不足错误
        goto ERROR_WITH_LOCK;                  // 失败则跳转到错误处理
    }

    // 查找FAT分区
    result = find_fat_partition(fs, part, &fmt, &start_sector);
    if (result != FR_OK) {                     // 检查查找是否成功
        ret = fatfs_2_vfs(result);             // 转换错误码
        goto ERROR_WITH_FSWIN;                 // 失败则跳转到错误处理
    }

    // 初始化FAT对象
    result = init_fatobj(fs, fmt, start_sector);
    if (result != FR_OK) {                     // 检查初始化是否成功
        ret = fatfs_2_vfs(result);             // 转换错误码
        goto ERROR_WITH_FSWIN;                 // 失败则跳转到错误处理
    }

    // 设置文件系统权限信息
    fs->fs_uid = mnt->vnodeBeCovered->uid;     // 设置用户ID
    fs->fs_gid = mnt->vnodeBeCovered->gid;     // 设置组ID
    fs->fs_dmask = GetUmask();                 // 设置目录权限掩码
    fs->fs_fmask = GetUmask();                 // 设置文件权限掩码
    // 设置文件系统模式
    fs->fs_mode = mnt->vnodeBeCovered->mode & (S_IRWXU | S_IRWXG | S_IRWXO);

    dfp = (DIR_FILE *)zalloc(sizeof(DIR_FILE)); // 分配目录文件数据内存
    if (dfp == NULL) {                         // 检查内存分配是否成功
        ret = ENOMEM;                          // 设置内存不足错误
        goto ERROR_WITH_FSWIN;                 // 失败则跳转到错误处理
    }

    // 初始化目录文件数据
    dfp->f_dir.obj.fs = fs;                    // 设置文件系统对象
    dfp->f_dir.obj.sclust = 0;                 // 设置起始簇为0（根目录）
    dfp->f_dir.obj.attr = AM_DIR;              // 设置属性为目录
    dfp->f_dir.obj.objsize = 0;                // 目录大小为0
    dfp->fno.fsize = 0;                        // 文件大小为0
    dfp->fno.fdate = 0;                        // 文件日期为0
    dfp->fno.ftime = 0;                        // 文件时间为0
    dfp->fno.fattrib = AM_DIR;                 // 文件属性为目录
    dfp->fno.sclst = 0;                        // 起始簇为0
    dfp->fno.fsize = fs->csize * SS(fs);       // 设置目录大小为簇大小
    dfp->fno.fname[0] = '/';                   // 标记为根目录
    dfp->fno.fname[1] = '\0';                  // 字符串结束符
    LOS_ListInit(&(dfp->fno.fp_list));         // 初始化文件指针链表

    ret = VnodeAlloc(&fatfs_vops, &vp);        // 分配Vnode节点
    if (ret != 0) {                            // 检查分配是否成功
        ret = ENOMEM;                          // 设置内存不足错误
        goto ERROR_WITH_FSWIN;                 // 失败则跳转到错误处理
    }

    mnt->data = fs;                            // 设置挂载点数据
    mnt->vnodeCovered = vp;                    // 设置覆盖Vnode

    // 初始化Vnode节点
    vp->parent = mnt->vnodeBeCovered;          // 设置父Vnode
    vp->fop = &fatfs_fops;                     // 设置文件操作
    vp->data = dfp;                            // 设置数据指针
    vp->originMount = mnt;                     // 设置挂载点
    vp->uid = fs->fs_uid;                      // 设置用户ID
    vp->gid = fs->fs_gid;                      // 设置组ID
    vp->mode = mnt->vnodeBeCovered->mode;      // 设置模式
    vp->type = VNODE_TYPE_DIR;                 // 设置类型为目录

    hash = fatfs_hash(0, 0, 0);                // 计算哈希值
    ret = VfsHashInsert(vp, hash);             // 插入Vnode哈希表
    if (ret != 0) {                            // 检查插入是否成功
        ret = -ret;                            // 转换错误码
        goto ERROR_WITH_LOCK;                  // 失败则跳转到错误处理
    }
    unlock_fs(fs, FR_OK);                      // 解锁文件系统

    return 0;                                  // 返回成功

ERROR_WITH_FSWIN:
    ff_memfree(fs->win);                       // 释放扇区窗口内存
ERROR_WITH_LOCK:
    unlock_fs(fs, FR_OK);                      // 解锁文件系统
ERROR_WITH_MUX:
    ff_del_syncobj(&fs->sobj);                 // 删除同步对象
ERROR_WITH_FS:
    free(fs);                                  // 释放文件系统对象
ERROR_PARTNAME:
    if (part->part_name) {                     // 如果分区名称已设置
        free(part->part_name);                 // 释放分区名称内存
        part->part_name = NULL;                // 重置分区名称
    }
ERROR_EXIT:
    return -ret;                               // 返回错误码
}
/**
 * @brief FAT文件系统卸载
 * @details 执行FAT文件系统的卸载操作，包括释放资源、关闭设备和清理分区信息
 * @param mnt 挂载点结构体指针
 * @param blkdriver 输出参数，块设备Vnode指针
 * @return int - 成功返回0，失败返回-错误码
 */
int fatfs_umount(struct Mount *mnt, struct Vnode **blkdriver)
{
    struct Vnode *device;               // 块设备Vnode指针
    FATFS *fs = (FATFS *)mnt->data;     // FAT文件系统对象
    los_part *part;                     // 分区信息指针
    int ret;                            // 临时返回值

    ret = lock_fs(fs);                  // 锁定文件系统
    if (ret == FALSE) {                 // 检查锁定是否成功
        return -EBUSY;                  // 失败则返回设备忙
    }

    part = get_part(fs->pdrv);          // 获取分区信息
    if (part == NULL) {                 // 检查分区是否存在
        unlock_fs(fs, FR_OK);           // 解锁文件系统
        return -ENODEV;                 // 返回设备不存在错误
    }
    device = part->dev;                 // 获取块设备
    if (device == NULL) {               // 检查设备是否有效
        unlock_fs(fs, FR_OK);           // 解锁文件系统
        return -ENODEV;                 // 返回设备不存在错误
    }
#ifdef LOSCFG_FS_FAT_CACHE
    ret = OsSdSync(part->disk_id);      // 同步SD卡缓存
    if (ret != 0) {                     // 检查同步是否成功
        unlock_fs(fs, FR_DISK_ERR);     // 解锁文件系统并传递错误码
        return -EIO;                    // 返回I/O错误
    }
#endif
    if (part->part_name != NULL) {      // 如果分区名称已设置
        free(part->part_name);          // 释放分区名称内存
        part->part_name = NULL;         // 重置分区名称
    }

    struct drv_data *dd = device->data; // 获取驱动数据
    if (dd->ops == NULL) {              // 检查驱动操作是否存在
        unlock_fs(fs, FR_OK);           // 解锁文件系统
        return ENODEV;                  // 返回设备不存在错误
    }

    const struct block_operations *bops = dd->ops; // 获取块设备操作
    if (bops != NULL && bops->close != NULL) {     // 如果close操作存在
        bops->close(*blkdriver);        // 关闭块设备
    }

    if (fs->win != NULL) {              // 如果扇区窗口已分配
        ff_memfree(fs->win);            // 释放扇区窗口内存
    }

    unlock_fs(fs, FR_OK);               // 解锁文件系统

    ret = ff_del_syncobj(&fs->sobj);    // 删除同步对象
    if (ret == FALSE) {                 // 检查删除是否成功
        return -EINVAL;                 // 返回参数错误
    }
    free(fs);                           // 释放文件系统对象

    *blkdriver = device;                // 设置输出块设备指针

    return 0;                           // 返回成功
}

/**
 * @brief 文件系统同步适配
 * @details 适配层同步函数，用于同步文件系统数据到磁盘
 * @param mnt 挂载点结构体指针
 * @return int - 成功返回0，失败返回错误码
 */
int fatfs_sync_adapt(struct Mount *mnt)
{
    (void)mnt;                          // 未使用的参数
    int ret = 0;                        // 函数返回值
#ifdef LOSCFG_FS_FAT_CACHE
    struct Vnode *dev = NULL;           // 设备Vnode指针
    los_part *part = NULL;              // 分区信息指针

    if (mnt == NULL) {                  // 检查挂载点是否有效
        return -EINVAL;                 // 返回参数错误
    }

    dev = mnt->vnodeDev;                // 获取设备Vnode
    part = los_part_find(dev);          // 查找分区
    if (part == NULL) {                 // 检查分区是否存在
        return -EINVAL;                 // 返回参数错误
    }

    ret = OsSdSync(part->disk_id);      // 同步SD卡缓存
#endif
    return ret;                         // 返回结果
}

/**
 * @brief 获取文件系统统计信息
 * @details 获取FAT文件系统的统计信息，包括块大小、总块数、空闲块数等
 * @param mnt 挂载点结构体指针
 * @param info 输出参数，文件系统统计信息结构体
 * @return int - 成功返回0，失败返回-错误码
 */
int fatfs_statfs(struct Mount *mnt, struct statfs *info)
{
    FATFS *fs = (FATFS *)mnt->data;     // FAT文件系统对象
    DWORD nclst = 0;                    // 簇计数变量
    FRESULT result = FR_OK;             // 函数返回结果
    int ret;                            // 临时返回值

    info->f_type = MSDOS_SUPER_MAGIC;   // 文件系统类型：MS-DOS魔法数
#if FF_MAX_SS != FF_MIN_SS
    info->f_bsize = fs->ssize * fs->csize; // 块大小 = 扇区大小 * 每簇扇区数
#else
    info->f_bsize = FF_MIN_SS * fs->csize; // 块大小 = 默认扇区大小 * 每簇扇区数
#endif
    info->f_blocks = fs->n_fatent;      // 总块数 = FAT表项数
    ret = lock_fs(fs);                  // 锁定文件系统
    if (ret == FALSE) {                 // 检查锁定是否成功
        return -EBUSY;                  // 返回设备忙
    }
    /* 空闲簇无效时更新 */
    if (fs->free_clst == DISK_ERROR) {
        result = fat_count_free_entries(&nclst, fs); // 计算空闲簇
    }
    info->f_bfree = fs->free_clst;      // 空闲块数 = 空闲簇数
    info->f_bavail = fs->free_clst;     // 可用块数 = 空闲簇数
    unlock_fs(fs, result);              // 解锁文件系统

#if FF_USE_LFN
    /* 文件名最大长度 */
    info->f_namelen = FF_MAX_LFN;       // 长文件名支持：最大长度为FF_MAX_LFN
#else
    /* 文件名最大长度：8为基本名长度，1为点，3为扩展名长度 */
    info->f_namelen = (8 + 1 + 3);      // 短文件名：8.3格式
#endif
    info->f_fsid.__val[0] = MSDOS_SUPER_MAGIC; // 文件系统ID：MS-DOS魔法数
    info->f_fsid.__val[1] = 1;          // 文件系统ID第二部分
    info->f_frsize = SS(fs) * fs->csize; // 分块大小 = 扇区大小 * 每簇扇区数
    info->f_files = 0;                  // 文件总数（未实现）
    info->f_ffree = 0;                  // 空闲文件数（未实现）
    info->f_flags = mnt->mountFlags;    // 挂载标志

    return -fatfs_2_vfs(result);        // 转换错误码并返回
}

/**
 * @brief 从FAT时间中提取秒
 * @param ftime FAT格式时间（低5位为秒）
 * @return int - 秒数（0-29，实际值需乘以2）
 */
static inline int GET_SECONDS(WORD ftime)
{
    return (ftime & BITMASK5) * SEC_MULTIPLIER; // 秒数 = 低5位值 * 2（FAT时间秒精度为2秒）
}

/**
 * @brief 从FAT时间中提取分钟
 * @param ftime FAT格式时间（5-10位为分钟）
 * @return int - 分钟数（0-59）
 */
static inline int GET_MINUTES(WORD ftime)
{
    return (ftime >> FTIME_MIN_OFFSET) & BITMASK6; // 分钟数 = (时间 >> 5) & 0x3F
}

/**
 * @brief 从FAT时间中提取小时
 * @param ftime FAT格式时间（11-15位为小时）
 * @return int - 小时数（0-23）
 */
static inline int GET_HOURS(WORD ftime)
{
    return (ftime >> FTIME_HR_OFFSET) & BITMASK5; // 小时数 = (时间 >> 11) & 0x1F
}

/**
 * @brief 从FAT日期中提取日
 * @param fdate FAT格式日期（低5位为日）
 * @return int - 日（1-31）
 */
static inline int GET_DAY(WORD fdate)
{
    return fdate & BITMASK5;            // 日 = 日期 & 0x1F
}

/**
 * @brief 从FAT日期中提取月
 * @param fdate FAT格式日期（5-8位为月）
 * @return int - 月（1-12）
 */
static inline int GET_MONTH(WORD fdate)
{
    return (fdate >> FTIME_MTH_OFFSET) & BITMASK4; // 月 = (日期 >> 5) & 0x0F
}

/**
 * @brief 从FAT日期中提取年
 * @param fdate FAT格式日期（9-15位为年）
 * @return int - 年（相对于1980的偏移值）
 */
static inline int GET_YEAR(WORD fdate)
{
    return (fdate >> FTIME_YEAR_OFFSET) & BITMASK7; // 年 = (日期 >> 9) & 0x7F
}

/**
 * @brief FAT时间转换为Unix时间
 * @details 将FAT格式的日期和时间转换为Unix时间戳（秒数）
 * @param fdate FAT格式日期
 * @param ftime FAT格式时间
 * @return time_t - 转换后的Unix时间戳
 */
static time_t fattime_transfer(WORD fdate, WORD ftime)
{
    struct tm time = { 0 };             // 时间结构体
    time.tm_sec = GET_SECONDS(ftime);   // 秒
    time.tm_min = GET_MINUTES(ftime);   // 分钟
    time.tm_hour = GET_HOURS(ftime);    // 小时
    time.tm_mday = GET_DAY(fdate);      // 日
    time.tm_mon = GET_MONTH(fdate);     // 月（注意：tm_mon从0开始）
    // FAT年份从1980开始，tm_year从1900开始
    time.tm_year = GET_YEAR(fdate) + YEAR_OFFSET; 
    time_t ret = mktime(&time);         // 转换为Unix时间戳
    return ret;                         // 返回结果
}

/**
 * @brief Unix时间转换为FAT时间
 * @details 将Unix时间戳转换为FAT格式的日期和时间
 * @param time Unix时间戳
 * @return DWORD - FAT格式时间（高16位日期，低16位时间）
 */
DWORD fattime_format(time_t time)
{
    struct tm st;                       // 时间结构体
    DWORD ftime;                        // FAT格式时间

    localtime_r(&time, &st);            // 将时间戳转换为本地时间

    // 构建日期部分（高16位）
    ftime = (DWORD)st.tm_mday;          // 日（1-31）
    ftime |= ((DWORD)st.tm_mon) << FTIME_MTH_OFFSET; // 月（0-11）
    // 年份：FAT年份 = 实际年份 - 1980
    ftime |= ((DWORD)((st.tm_year > YEAR_OFFSET) ? (st.tm_year - YEAR_OFFSET) : 0)) << FTIME_YEAR_OFFSET;
    ftime <<= FTIME_DATE_OFFSET;        // 移至高位（日期部分）

    // 构建时间部分（低16位）
    ftime |= (DWORD)st.tm_sec / SEC_MULTIPLIER; // 秒（0-29，步长为2）
    ftime |= ((DWORD)st.tm_min) << FTIME_MIN_OFFSET; // 分钟（0-59）
    ftime |= ((DWORD)st.tm_hour) << FTIME_HR_OFFSET;  // 小时（0-23）

    return ftime;                       // 返回FAT格式时间
}

/**
 * @brief 获取文件状态
 * @details 获取文件的详细状态信息，包括大小、权限、修改时间等
 * @param vp Vnode节点指针
 * @param sp 输出参数，状态信息结构体
 * @return int - 成功返回0，失败返回错误码
 */
int fatfs_stat(struct Vnode *vp, struct stat* sp)
{
    FATFS *fs = (FATFS *)vp->originMount->data; // FAT文件系统对象
    DIR_FILE *dfp = (DIR_FILE *)vp->data;       // 目录文件数据
    FILINFO *finfo = &(dfp->fno);               // 文件信息
    time_t time;                                // 时间戳
    int ret;                                    // 临时返回值

    ret = lock_fs(fs);                          // 锁定文件系统
    if (ret == FALSE) {                         // 检查锁定是否成功
        return EBUSY;                           // 返回设备忙
    }

    sp->st_dev = fs->pdrv;                      // 设备ID
    sp->st_mode = vp->mode;                     // 文件模式（权限和类型）
    sp->st_nlink = 1;                           // 硬链接数（FAT不支持，固定为1）
    sp->st_uid = fs->fs_uid;                    // 用户ID
    sp->st_gid = fs->fs_gid;                    // 组ID
    sp->st_size = finfo->fsize;                 // 文件大小（字节）
    sp->st_blksize = fs->csize * SS(fs);        // 块大小（字节）
    if (finfo->fattrib & AM_ARC) {              // 如果是普通文件
        // 计算块数：(大小-1)/块大小 + 1（向上取整）
        sp->st_blocks = finfo->fsize ? ((finfo->fsize - 1) / SS(fs) / fs->csize + 1) : 0;
    } else {                                   // 如果是目录
        sp->st_blocks = fs->csize;              // 块数 = 簇大小
    }
    time = fattime_transfer(finfo->fdate, finfo->ftime); // 转换修改时间
    sp->st_mtime = time;                        // 修改时间戳

    /* 适配kstat成员"long tv_sec" */
    sp->__st_mtim32.tv_sec = (long)time;        // 32位时间戳

    unlock_fs(fs, FR_OK);                       // 解锁文件系统
    return 0;                                   // 返回成功
}

/**
 * @brief 修改文件时间
 * @details 根据属性修改请求更新文件的访问时间、创建时间或修改时间
 * @param dp 目录对象指针
 * @param attr 属性修改结构体
 */
void fatfs_chtime(DIR *dp, struct IATTR *attr)
{
    BYTE *dir = dp->dir;                        // 目录项缓冲区
    DWORD ftime;                                // FAT格式时间
    if (attr->attr_chg_valid & CHG_ATIME) {     // 如果需要修改访问时间
        ftime = fattime_format(attr->attr_chg_atime); // 转换为FAT时间
        // 设置最后访问日期（仅日期部分）
        st_word(dir + DIR_LstAccDate, (ftime >> FTIME_DATE_OFFSET));
    }

    if (attr->attr_chg_valid & CHG_CTIME) {     // 如果需要修改创建时间
        ftime = fattime_format(attr->attr_chg_ctime); // 转换为FAT时间
        st_dword(dir + DIR_CrtTime, ftime);     // 设置创建时间
    }

    if (attr->attr_chg_valid & CHG_MTIME) {     // 如果需要修改修改时间
        ftime = fattime_format(attr->attr_chg_mtime); // 转换为FAT时间
        st_dword(dir + DIR_ModTime, ftime);     // 设置修改时间
    }
}

/**
 * @brief 修改文件属性
 * @details 修改文件的权限和时间属性，FAT文件系统仅支持只读属性
 * @param vp Vnode节点指针
 * @param attr 属性修改结构体
 * @return int - 成功返回0，失败返回-错误码
 */
int fatfs_chattr(struct Vnode *vp, struct IATTR *attr)
{
    FATFS *fs = (FATFS *)vp->originMount->data; // FAT文件系统对象
    DIR_FILE *dfp = (DIR_FILE *)vp->data;       // 目录文件数据
    DIR *dp = &(dfp->f_dir);                    // 目录对象
    FILINFO *finfo = &(dfp->fno);               // 文件信息
    BYTE *dir = dp->dir;                        // 目录项缓冲区
    DWORD ftime;                                // FAT格式时间
    FRESULT result;                             // 函数返回结果
    int ret;                                    // 临时返回值

    if (finfo->fname[0] == '/') { /* 是否为FAT根目录 */
        return 0;                               // 根目录不允许修改属性
    }

    ret = lock_fs(fs);                          // 锁定文件系统
    if (ret == FALSE) {                         // 检查锁定是否成功
        result = FR_TIMEOUT;                    // 设置超时错误
        goto ERROR_OUT;                         // 跳转到错误处理
    }

    result = move_window(fs, dp->sect);         // 将目录扇区加载到窗口
    if (result != FR_OK) {                      // 检查扇区加载是否成功
        goto ERROR_UNLOCK;                      // 跳转到错误处理
    }

    if (attr->attr_chg_valid & CHG_MODE) {      // 如果需要修改模式（权限）
        /* FAT仅支持只读标志 */
        // 如果设置只读且当前不是只读
        if ((attr->attr_chg_mode & S_IWUSR) == 0 && (finfo->fattrib & AM_RDO) == 0) {
            dir[DIR_Attr] |= AM_RDO;           // 设置目录项只读属性
            finfo->fattrib |= AM_RDO;          // 更新文件信息只读属性
            fs->wflag = 1;                      // 设置文件系统写标志
        } else if ((attr->attr_chg_mode & S_IWUSR) != 0 && (finfo->fattrib & AM_RDO) != 0) {
            // 如果取消只读且当前是只读
            dir[DIR_Attr] &= ~AM_RDO;          // 清除目录项只读属性
            finfo->fattrib &= ~AM_RDO;         // 更新文件信息只读属性
            fs->wflag = 1;                      // 设置文件系统写标志
        }
        // 更新Vnode模式
        vp->mode = fatfs_get_mode(finfo->fattrib, fs->fs_mode);
    }

    // 如果需要修改访问时间、创建时间或修改时间
    if (attr->attr_chg_valid & (CHG_ATIME | CHG_CTIME | CHG_MTIME)) {
        fatfs_chtime(dp, attr);                 // 修改文件时间
        ftime = ld_dword(dp->dir + DIR_ModTime); // 读取修改时间
        finfo->fdate = (WORD)(ftime >> FTIME_DATE_OFFSET); // 更新日期
        finfo->ftime = (WORD)ftime;             // 更新时间
    }

    result = sync_window(fs);                   // 同步扇区窗口到磁盘
    if (result != FR_OK) {                      // 检查同步是否成功
        goto ERROR_UNLOCK;                      // 跳转到错误处理
    }

    unlock_fs(fs, FR_OK);                       // 解锁文件系统
    return fatfs_sync(vp->originMount->mountFlags, fs); // 同步文件系统
ERROR_UNLOCK:
    unlock_fs(fs, result);                      // 解锁文件系统并传递错误码
ERROR_OUT:
    return -fatfs_2_vfs(result);                // 转换错误码并返回
}

/**
 * @brief   打开目录
 * @param   vp      - 目录对应的Vnode节点
 * @param   idir    - 目录项结构体指针，用于存储目录信息
 * @return  0表示成功，负数表示失败
 */
int fatfs_opendir(struct Vnode *vp, struct fs_dirent_s *idir)
{
    FATFS *fs = vp->originMount->data;  // 获取文件系统对象
    DIR_FILE *dfp = (DIR_FILE *)vp->data;  // 获取目录文件信息
    FILINFO *finfo = &(dfp->fno);  // 获取文件信息结构体
    DIR *dp;  // 目录结构体指针
    DWORD clst;  // 簇号
    FRESULT result;  // FatFs操作结果
    int ret;  // 返回值

    dp = (DIR*)zalloc(sizeof(DIR));  // 分配目录结构体内存
    if (dp == NULL) {
        return -ENOMEM;  // 内存分配失败，返回ENOMEM错误
    }

    ret = lock_fs(fs);  // 锁定文件系统
    if (ret == FALSE) {
        return -EBUSY;  // 锁定失败，返回EBUSY错误
    }
    clst = finfo->sclst;  // 获取起始簇号
    dp->obj.fs = fs;  // 设置文件系统对象
    dp->obj.sclust = clst;  // 设置起始簇号

    result = dir_sdi(dp, 0);  // 设置目录索引
    if (result != FR_OK) {
        free(dp);  // 释放目录结构体内存
        unlock_fs(fs, result);  // 解锁文件系统
        return -fatfs_2_vfs(result);  // 转换错误码并返回
    }
    unlock_fs(fs, result);  // 解锁文件系统
    idir->u.fs_dir = dp;  // 保存目录结构体指针

    return 0;  // 成功返回0
}

/**
 * @brief   读取目录
 * @param   vp      - 目录对应的Vnode节点
 * @param   idir    - 目录项结构体指针，用于存储读取结果
 * @return  读取到的目录项数量，负数表示失败
 */
int fatfs_readdir(struct Vnode *vp, struct fs_dirent_s *idir)
{
    FATFS *fs = vp->originMount->data;  // 获取文件系统对象
    FILINFO fno;  // 文件信息结构体
    DIR* dp = (DIR*)idir->u.fs_dir;  // 获取目录结构体指针
    struct dirent *dirp = NULL;  // 目录项指针
    FRESULT result;  // FatFs操作结果
    int ret, i;  // 返回值和循环变量

    ret = lock_fs(fs);  // 锁定文件系统
    if (ret == FALSE) {  // 锁定失败
        return -EBUSY;  // 返回EBUSY错误
    }
    DEF_NAMBUF;  // 定义名称缓冲区
    INIT_NAMBUF(fs);  // 初始化名称缓冲区
    for (i = 0; i < idir->read_cnt; i++) {  // 循环读取目录项
        result = dir_read_massive(dp, 0);  // 批量读取目录项以提高性能
        if (result == FR_NO_FILE) {  // 没有更多文件
            break;  // 跳出循环
        } else if (result != FR_OK) {  // 读取失败
            goto ERROR_UNLOCK;  // 跳转到错误处理
        }
        get_fileinfo(dp, &fno);  // 获取文件信息
        // 0x00表示目录结束，0xFF表示目录为空
        if (fno.fname[0] == 0x00 || fno.fname[0] == (TCHAR)0xFF) {
            break;  // 跳出循环
        }

        dirp = &(idir->fd_dir[i]);  // 获取目录项结构体
        if (fno.fattrib & AM_DIR) {  // 判断是否为目录
            dirp->d_type = DT_DIR;  // 设置目录类型
        } else {
            dirp->d_type = DT_REG;  // 设置文件类型
        }
        // 复制文件名到目录项
        if (strncpy_s(dirp->d_name, sizeof(dirp->d_name), fno.fname, strlen(fno.fname)) != EOK) {
            result = FR_NOT_ENOUGH_CORE;  // 设置内存不足错误
            goto ERROR_UNLOCK;  // 跳转到错误处理
        }
        result = dir_next(dp, 0);  // 移动到下一个目录项
        if (result != FR_OK && result != FR_NO_FILE) {  // 移动失败
            goto ERROR_UNLOCK;  // 跳转到错误处理
        }
        idir->fd_position++;  // 增加文件位置
        idir->fd_dir[i].d_off = idir->fd_position;  // 设置目录项偏移
        idir->fd_dir[i].d_reclen = (uint16_t)sizeof(struct dirent);  // 设置目录项长度
    }
    unlock_fs(fs, FR_OK);  // 解锁文件系统
    FREE_NAMBUF();  // 释放名称缓冲区
    return i;  // 返回读取到的目录项数量
ERROR_UNLOCK:
    unlock_fs(fs, result);  // 解锁文件系统
    FREE_NAMBUF();  // 释放名称缓冲区
    return -fatfs_2_vfs(result);  // 转换错误码并返回
}

/**
 * @brief   重置目录读取位置
 * @param   vp      - 目录对应的Vnode节点
 * @param   dir     - 目录项结构体指针
 * @return  0表示成功，负数表示失败
 */
int fatfs_rewinddir(struct Vnode *vp, struct fs_dirent_s *dir)
{
    DIR *dp = (DIR *)dir->u.fs_dir;  // 获取目录结构体指针
    FATFS *fs = dp->obj.fs;  // 获取文件系统对象
    FRESULT result;  // FatFs操作结果
    int ret;  // 返回值

    ret = lock_fs(fs);  // 锁定文件系统
    if (ret == FALSE) {
        return -EBUSY;  // 锁定失败，返回EBUSY错误
    }

    result = dir_sdi(dp, 0);  // 重置目录索引
    unlock_fs(fs, result);  // 解锁文件系统
    return -fatfs_2_vfs(result);  // 转换错误码并返回
}

/**
 * @brief   关闭目录
 * @param   vp      - 目录对应的Vnode节点
 * @param   dir     - 目录项结构体指针
 * @return  0表示成功
 */
int fatfs_closedir(struct Vnode *vp, struct fs_dirent_s *dir)
{
    DIR *dp = (DIR *)dir->u.fs_dir;  // 获取目录结构体指针
    free(dp);  // 释放目录结构体内存
    dir->u.fs_dir = NULL;  // 清空目录结构体指针
    return 0;  // 成功返回0
}

/**
 * @brief   重命名前检查新旧路径的有效性
 * @param   dp_new      - 新路径的目录结构体
 * @param   finfo_new   - 新路径的文件信息
 * @param   dp_old      - 旧路径的目录结构体
 * @param   finfo_old   - 旧路径的文件信息
 * @return  FR_OK表示成功，其他表示失败
 */
static FRESULT rename_check(DIR *dp_new, FILINFO *finfo_new, DIR *dp_old, FILINFO *finfo_old)
{
    DIR dir_sub;  // 子目录结构体
    FRESULT result;  // FatFs操作结果
    if (finfo_new->fattrib & AM_ARC) {  // 新路径是文件
        if (finfo_old->fattrib & AM_DIR) {  // 但旧路径是目录
            return FR_NO_DIR;  // 返回目录不存在错误
        }
    } else if (finfo_new->fattrib & AM_DIR) {  // 新路径是目录
        if (finfo_old->fattrib & AM_ARC) {  // 旧路径是文件
            return FR_IS_DIR;  // 返回是目录错误
        }
        dir_sub.obj.fs = dp_old->obj.fs;  // 设置文件系统对象
        dir_sub.obj.sclust = finfo_new->sclst;  // 设置起始簇号
        result = dir_sdi(&dir_sub, 0);  // 设置目录索引
        if (result != FR_OK) {
            return result;  // 返回错误
        }
        result = dir_read(&dir_sub, 0);  // 读取目录
        if (result == FR_OK) {  // 新路径不是空目录
            return FR_NO_EMPTY_DIR;  // 返回目录非空错误
        }
    } else {  // 系统文件或卷标
        return FR_DENIED;  // 返回拒绝访问错误
    }
    return FR_OK;  // 检查通过
}

/**
 * @brief   重命名文件或目录
 * @param   old_vnode   - 旧路径的Vnode节点
 * @param   new_parent  - 新路径的父目录Vnode节点
 * @param   oldname     - 旧名称
 * @param   newname     - 新名称
 * @return  0表示成功，负数表示失败
 */
int fatfs_rename(struct Vnode *old_vnode, struct Vnode *new_parent, const char *oldname, const char *newname)
{
    FATFS *fs = (FATFS *)(old_vnode->originMount->data);  // 获取文件系统对象
    DIR_FILE *dfp_old = (DIR_FILE *)old_vnode->data;  // 获取旧路径目录文件信息
    DIR *dp_old = &(dfp_old->f_dir);  // 获取旧路径目录结构体
    FILINFO *finfo_old = &(dfp_old->fno);  // 获取旧路径文件信息
    DIR_FILE *dfp_new = NULL;  // 新路径目录文件信息指针
    DIR* dp_new = NULL;  // 新路径目录结构体指针
    FILINFO* finfo_new = NULL;  // 新路径文件信息指针
    DWORD clust;  // 簇号
    FRESULT result;  // FatFs操作结果
    int ret;  // 返回值

    ret = lock_fs(fs);  // 锁定文件系统
    if (ret == FALSE) {  // 锁定失败
        return -EBUSY;  // 返回EBUSY错误
    }

    dfp_new = (DIR_FILE *)zalloc(sizeof(DIR_FILE));  // 分配新路径目录文件信息内存
    if (dfp_new == NULL) {
        result = FR_NOT_ENOUGH_CORE;  // 设置内存不足错误
        goto ERROR_UNLOCK;  // 跳转到错误处理
    }
    dp_new = &(dfp_new->f_dir);  // 获取新路径目录结构体
    finfo_new = &(dfp_new->fno);  // 获取新路径文件信息

    // 设置新路径目录结构体的起始簇号和文件系统对象
    dp_new->obj.sclust = ((DIR_FILE *)(new_parent->data))->fno.sclst;
    dp_new->obj.fs = fs;

    // 查找新路径
    DEF_NAMBUF;  // 定义名称缓冲区
    INIT_NAMBUF(fs);  // 初始化名称缓冲区
    result = create_name(dp_new, &newname);  // 创建名称
    if (result != FR_OK) {
        goto ERROR_FREE;  // 跳转到释放内存
    }
    result = dir_find(dp_new);  // 查找目录
    if (result == FR_OK) {  // 新路径已存在
        get_fileinfo(dp_new, finfo_new);  // 获取新路径文件信息
        result = rename_check(dp_new, finfo_new, dp_old, finfo_old);  // 重命名检查
        if (result != FR_OK) {
            goto ERROR_FREE;  // 跳转到释放内存
        }
        result = dir_remove(dp_old);  // 删除旧路径目录项
        if (result != FR_OK) {
            goto ERROR_FREE;  // 跳转到释放内存
        }
        clust = finfo_new->sclst;  // 获取新路径簇号
        if (clust != 0) {  // 如果新路径存在簇链
            result = remove_chain(&(dp_new->obj), clust, 0);  // 删除新路径簇链
            if (result != FR_OK) {
                goto ERROR_FREE;  // 跳转到释放内存
            }
        }
    } else {  // 新路径不存在
        result = dir_remove(dp_old);  // 删除旧路径目录项
        if (result != FR_OK) {
            goto ERROR_FREE;  // 跳转到释放内存
        }
        result = dir_register(dp_new);  // 注册新路径目录项
        if (result != FR_OK) {
            goto ERROR_FREE;  // 跳转到释放内存
        }
    }

    // 用旧信息更新新目录项
    result = update_dir(dp_new, finfo_old);
    if (result != FR_OK) {
        goto ERROR_FREE;  // 跳转到释放内存
    }
    result = dir_read(dp_new, 0);  // 读取新目录项
    if (result != FR_OK) {
        goto ERROR_FREE;  // 跳转到释放内存
    }
    dp_new->blk_ofs = dir_ofs(dp_new);  // 获取目录偏移
    get_fileinfo(dp_new, finfo_new);  // 获取新文件信息

    // 更新文件指针链表
    dfp_new->fno.fp_list.pstNext = dfp_old->fno.fp_list.pstNext;
    dfp_new->fno.fp_list.pstPrev = dfp_old->fno.fp_list.pstPrev;
    // 复制新目录文件信息到旧目录文件信息
    ret = memcpy_s(dfp_old, sizeof(DIR_FILE), dfp_new, sizeof(DIR_FILE));
    if (ret != 0) {
        result = FR_NOT_ENOUGH_CORE;  // 设置内存不足错误
        goto ERROR_FREE;  // 跳转到释放内存
    }
    free(dfp_new);  // 释放新目录文件信息内存
    unlock_fs(fs, FR_OK);  // 解锁文件系统
    FREE_NAMBUF();  // 释放名称缓冲区
    return fatfs_sync(old_vnode->originMount->mountFlags, fs);  // 同步文件系统并返回

ERROR_FREE:
    free(dfp_new);  // 释放新目录文件信息内存
ERROR_UNLOCK:
    unlock_fs(fs, result);  // 解锁文件系统
    FREE_NAMBUF();  // 释放名称缓冲区
    return -fatfs_2_vfs(result);  // 转换错误码并返回
}


/**
 * @brief   擦除分区
 * @param   part    - 分区结构体
 * @param   option  - 擦除选项
 * @return  擦除后的选项
 */
static int fatfs_erase(los_part *part, int option)
{
    int opt = option;  // 选项副本
    if ((UINT)opt & FMT_ERASE) {  // 如果需要擦除
        opt = (UINT)opt & (~FMT_ERASE);  // 清除擦除标志
        // 按ID擦除磁盘
        if (EraseDiskByID(part->disk_id, part->sector_start, part->sector_count) != LOS_OK) {
            PRINTK("Disk erase error.\n");  // 打印擦除错误
            return -1;  // 返回错误
        }
    }

    if (opt != FM_FAT && opt != FM_FAT32) {  // 如果不是FAT或FAT32格式
        opt = FM_ANY;  // 设置为自动检测格式
    }

    return opt;  // 返回选项
}

/**
 * @brief   设置分区信息
 * @param   part    - 分区结构体
 * @return  0表示成功，负数表示失败
 */
static int fatfs_set_part_info(los_part *part)
{
    los_disk *disk = NULL;  // 磁盘结构体指针
    char *buf = NULL;  // 缓冲区指针
    int ret;  // 返回值

    // 如果之前没有MBR，mkfs后需要更改分区信息
    if (part->type != EMMC && part->part_no_mbr == 0) {
        disk = get_disk(part->disk_id);  // 获取磁盘
        if (disk == NULL) {
            return -EIO;  // 返回I/O错误
        }
        buf = (char *)zalloc(disk->sector_size);  // 分配扇区大小的缓冲区
        if (buf == NULL) {
            return -ENOMEM;  // 返回内存不足错误
        }
        (void)memset_s(buf, disk->sector_size, 0, disk->sector_size);  // 清空缓冲区
        // 读取磁盘扇区（TRUE表示不读取大量数据）
        ret = los_disk_read(part->disk_id, buf, 0, 1, TRUE);
        if (ret < 0) {
            free(buf);  // 释放缓冲区
            return -EIO;  // 返回I/O错误
        }
        // 从MBR中读取分区起始扇区
        part->sector_start = LD_DWORD_DISK(&buf[PAR_OFFSET + PAR_START_OFFSET]);
        // 从MBR中读取分区扇区数量
        part->sector_count = LD_DWORD_DISK(&buf[PAR_OFFSET + PAR_COUNT_OFFSET]);
        part->part_no_mbr = 1;  // 标记已读取MBR
        // 从MBR中读取文件系统类型
        part->filesystem_type = buf[PAR_OFFSET + PAR_TYPE_OFFSET];

        free(buf);  // 释放缓冲区
    }
    return 0;  // 成功返回0
}

/**
 * @brief   设置卷标
 * @param   part    - 分区结构体
 * @return  FR_OK表示成功，其他表示失败
 */
static FRESULT fatfs_setlabel(los_part *part)
{
    QWORD start_sector = 0;  // 起始扇区
    BYTE fmt = 0;  // 文件系统格式
    FATFS fs;  // 文件系统对象
    FRESULT result;  // FatFs操作结果

#ifdef LOSCFG_FS_FAT_VIRTUAL_PARTITION
    fs.vir_flag = FS_PARENT;  // 设置虚拟分区标志
    fs.parent_fs = &fs;  // 设置父文件系统
    fs.vir_amount = DISK_ERROR;  // 设置虚拟分区数量
    fs.vir_avail = FS_VIRDISABLE;  // 禁用虚拟分区
#endif
    // 获取扇区大小
    if (disk_ioctl(fs.pdrv, GET_SECTOR_SIZE, &(fs.ssize)) != RES_OK) {
        return -EIO;  // 返回I/O错误
    }
    fs.win = (BYTE *)ff_memalloc(fs.ssize);  // 分配扇区缓冲区
    if (fs.win == NULL) {
        return -ENOMEM;  // 返回内存不足错误
    }

    // 查找FAT分区
    result = find_fat_partition(&fs, part, &fmt, &start_sector);
    if (result != FR_OK) {
        free(fs.win);  // 释放缓冲区
        return -fatfs_2_vfs(result);  // 转换错误码并返回
    }

    // 初始化FAT对象
    result = init_fatobj(&fs, fmt, start_sector);
    if (result != FR_OK) {
        free(fs.win);  // 释放缓冲区
        return -fatfs_2_vfs(result);  // 转换错误码并返回
    }

    result = set_volumn_label(&fs, FatLabel);  // 设置卷标
    free(fs.win);  // 释放缓冲区

    return result;  // 返回结果
}

/**
 * @brief   创建FAT文件系统
 * @param   device  - 设备Vnode节点
 * @param   sectors - 扇区数
 * @param   option  - 选项
 * @return  0表示成功，负数表示失败
 */
int fatfs_mkfs(struct Vnode *device, int sectors, int option)
{
    BYTE *work_buff = NULL;  // 工作缓冲区
    los_part *part = NULL;  // 分区结构体指针
    FRESULT result;  // FatFs操作结果
    MKFS_PARM opt = {0};  // mkfs参数
    int ret;  // 返回值

    part = los_part_find(device);  // 查找分区
    if (part == NULL || device->data == NULL) {
        return -ENODEV;  // 返回设备不存在错误
    }

    // 检查扇区数是否有效（非负、不超过最大簇大小、是2的幂）
    if (sectors < 0 || sectors > FAT32_MAX_CLUSTER_SIZE || ((DWORD)sectors & ((DWORD)sectors - 1))) {
        return -EINVAL;  // 返回无效参数错误
    }

    // 检查选项是否有效
    if (option != FMT_FAT && option != FMT_FAT32 && option != FMT_ANY && option != FMT_ERASE) {
        return -EINVAL;  // 返回无效参数错误
    }

    if (part->part_name != NULL) {  // 分区已挂载
        return -EBUSY;  // 返回忙错误
    }
    option = fatfs_erase(part, option);  // 擦除分区
    work_buff = (BYTE *)zalloc(FF_MAX_SS);  // 分配工作缓冲区
    if (work_buff == NULL) {
        return -ENOMEM;  // 返回内存不足错误
    }

    opt.n_sect = sectors;  // 设置扇区数
    opt.fmt = (BYTE)option;  // 设置格式
    // 创建文件系统
    result = _mkfs(part, &opt, work_buff, FF_MAX_SS);
    free(work_buff);  // 释放工作缓冲区
    if (result != FR_OK) {
        return -fatfs_2_vfs(result);  // 转换错误码并返回
    }

    result = fatfs_setlabel(part);  // 设置卷标
    if (result == FR_OK) {
#ifdef LOSCFG_FS_FAT_CACHE
        ret = OsSdSync(part->disk_id);  // 同步SD卡
        if (ret != 0) {
            return -EIO;  // 返回I/O错误
        }
#endif
    }

    ret = fatfs_set_part_info(part);  // 设置分区信息
    if (ret != 0) {
        return -EIO;  // 返回I/O错误
    }

    return -fatfs_2_vfs(result);  // 转换错误码并返回
}

/**
 * @brief   创建目录
 * @param   parent  - 父目录Vnode节点
 * @param   name    - 目录名称
 * @param   mode    - 权限模式
 * @param   vpp     - 输出新目录的Vnode节点指针
 * @return  0表示成功，负数表示失败
 */
int fatfs_mkdir(struct Vnode *parent, const char *name, mode_t mode, struct Vnode **vpp)
{
    // 调用创建对象函数，指定为目录类型
    return fatfs_create_obj(parent, name, mode, vpp, AM_DIR, NULL);
}

/**
 * @brief   删除目录
 * @param   parent  - 父目录Vnode节点
 * @param   vp      - 要删除的目录Vnode节点
 * @param   name    - 目录名称
 * @return  0表示成功，负数表示失败
 */
int fatfs_rmdir(struct Vnode *parent, struct Vnode *vp, const char *name)
{
    FATFS *fs = (FATFS *)vp->originMount->data;  // 获取文件系统对象
    DIR_FILE *dfp = (DIR_FILE *)vp->data;  // 获取目录文件信息
    FILINFO *finfo = &(dfp->fno);  // 获取文件信息
    DIR *dp = &(dfp->f_dir);  // 获取目录结构体
    DIR dir_sub;  // 子目录结构体
    FRESULT result = FR_OK;  // FatFs操作结果
    int ret;  // 返回值

    if (finfo->fattrib & AM_ARC) {  // 如果是文件
        result = FR_NO_DIR;  // 设置不是目录错误
        goto ERROR_OUT;  // 跳转到错误处理
    }

    DEF_NAMBUF;  // 定义名称缓冲区
    INIT_NAMBUF(fs);  // 初始化名称缓冲区

    ret = lock_fs(fs);  // 锁定文件系统
    if (ret == FALSE) {
        result = FR_TIMEOUT;  // 设置超时错误
        goto ERROR_OUT;  // 跳转到错误处理
    }
    dir_sub.obj.fs = fs;  // 设置文件系统对象
    dir_sub.obj.sclust = finfo->sclst;  // 设置起始簇号
    result = dir_sdi(&dir_sub, 0);  // 设置目录索引
    if (result != FR_OK) {
        goto ERROR_UNLOCK;  // 跳转到解锁
    }
    result = dir_read(&dir_sub, 0);  // 读取目录
    if (result == FR_OK) {  // 目录非空
        result = FR_NO_EMPTY_DIR;  // 设置目录非空错误
        goto ERROR_UNLOCK;  // 跳转到解锁
    }
    result = dir_remove(dp);  // 删除目录项
    if (result != FR_OK) {
        goto ERROR_UNLOCK;  // 跳转到解锁
    }
    // 目录项至少包含一个簇
    result = remove_chain(&(dp->obj), finfo->sclst, 0);  // 删除簇链
    if (result != FR_OK) {
        goto ERROR_UNLOCK;  // 跳转到解锁
    }

    unlock_fs(fs, FR_OK);  // 解锁文件系统
    FREE_NAMBUF();  // 释放名称缓冲区
    return fatfs_sync(vp->originMount->mountFlags, fs);  // 同步文件系统并返回

ERROR_UNLOCK:
    unlock_fs(fs, result);  // 解锁文件系统
    FREE_NAMBUF();  // 释放名称缓冲区
ERROR_OUT:
    return -fatfs_2_vfs(result);  // 转换错误码并返回
}
/**
 * @brief 释放Vnode关联的数据内存
 * @param vp Vnode指针
 * @return 0 - 成功，非0 - 失败
 */
int fatfs_reclaim(struct Vnode *vp)
{
    free(vp->data);  // 释放Vnode数据内存
    vp->data = NULL;  // 将数据指针置空，避免野指针
    return 0;  // 返回成功
}

/**
 * @brief 删除文件或目录
 * @param parent 父目录Vnode
 * @param vp 要删除的文件/目录Vnode
 * @param name 要删除的文件名
 * @return 0 - 成功，负数 - 错误码
 */
int fatfs_unlink(struct Vnode *parent, struct Vnode *vp, const char *name)
{
    FATFS *fs = (FATFS *)vp->originMount->data;  // 获取文件系统对象
    DIR_FILE *dfp = (DIR_FILE *)vp->data;  // 获取目录文件结构
    FILINFO *finfo = &(dfp->fno);  // 文件信息结构体
    DIR *dp = &(dfp->f_dir);  // 目录结构体
    FRESULT result = FR_OK;  // FatFs操作结果
    int ret;  // 临时返回值

    if (finfo->fattrib & AM_DIR) {  // 检查是否为目录
        result = FR_IS_DIR;  // 设置目录删除错误
        goto ERROR_OUT;  // 跳转到错误处理
    }
    ret = lock_fs(fs);  // 锁定文件系统
    if (ret == FALSE) {  // 锁定失败
        result = FR_TIMEOUT;  // 设置超时错误
        goto ERROR_OUT;  // 跳转到错误处理
    }
    result = dir_remove(dp);  // 删除目录项
    if (result != FR_OK) {  // 目录项删除失败
        goto ERROR_UNLOCK;  // 跳转到解锁处理
    }
    if (finfo->sclst != 0) {  // 如果存在簇链
        result = remove_chain(&(dp->obj), finfo->sclst, 0);  // 删除簇链
        if (result != FR_OK) {  // 簇链删除失败
            goto ERROR_UNLOCK;  // 跳转到解锁处理
        }
    }
    result = sync_fs(fs);  // 同步文件系统
    if (result != FR_OK) {  // 同步失败
        goto ERROR_UNLOCK;  // 跳转到解锁处理
    }
    unlock_fs(fs, FR_OK);  // 解锁文件系统
    return fatfs_sync(vp->originMount->mountFlags, fs);  // 同步并返回结果

ERROR_UNLOCK:
    unlock_fs(fs, result);  // 解锁文件系统并传递错误
ERROR_OUT:
    return -fatfs_2_vfs(result);  // 转换错误码并返回
}

/**
 * @brief 文件I/O控制（未实现）
 * @param filep 文件指针
 * @param req 控制请求
 * @param arg 参数
 * @return -ENOSYS 表示未实现
 */
int fatfs_ioctl(struct file *filep, int req, unsigned long arg)
{
    return -ENOSYS;  // 返回未实现错误
}

#define CHECK_FILE_NUM 3  // 文件检查数量：3个

/**
 * @brief 合并FAT文件时间和日期
 * @param finfo 文件信息结构体
 * @return 合并后的时间值
 */
static inline DWORD combine_time(FILINFO *finfo)
{
    return (finfo->fdate << FTIME_DATE_OFFSET) | finfo->ftime;  // 日期左移偏移后与时间合并
}

/**
 * @brief 获取最旧文件的索引
 * @param df DIR_FILE数组
 * @param oldest_time 输出参数，最旧时间
 * @param len 数组长度
 * @return 最旧文件的索引
 */
static UINT get_oldest_time(DIR_FILE df[], DWORD *oldest_time, UINT len)
{
    int i;  // 循环变量
    DWORD old_time = combine_time(&(df[0].fno));  // 初始化为第一个文件时间
    DWORD time;  // 当前文件时间
    UINT index = 0;  // 最旧文件索引
    for (i = 1; i < len; i++) {  // 遍历剩余文件
        time = combine_time(&(df[i].fno));  // 获取当前文件时间
        if (time < old_time) {  // 如果当前文件更旧
            old_time = time;  // 更新最旧时间
            index = i;  // 更新索引
        }
    }
    *oldest_time = old_time;  // 输出最旧时间
    return index;  // 返回索引
}

/**
 * @brief 文件系统检查实现
 * @param dp 目录结构体指针
 * @return FR_OK 成功，其他值 失败
 */
static FRESULT fscheck(DIR *dp)
{
    DIR_FILE df[CHECK_FILE_NUM] = {0};  // 存储检查文件信息
    FILINFO fno;  // 文件信息
    UINT index = 0;  // 当前索引
    UINT count;  // 文件计数
    DWORD time;  // 文件时间
    DWORD old_time = -1;  // 最旧时间初始值
    FRESULT result;  // 操作结果
    for (count = 0; count < CHECK_FILE_NUM; count++) {  // 读取前3个文件
        if ((result = f_readdir(dp, &fno)) != FR_OK) {  // 读取目录项
            return result;  // 返回错误
        } else {
            if (fno.fname[0] == 0 || fno.fname[0] == (TCHAR)0xFF) {  // 目录结束
                break;  // 退出循环
            }
            (void)memcpy_s(&df[count].f_dir, sizeof(DIR), dp, sizeof(DIR));  // 复制目录信息
            (void)memcpy_s(&df[count].fno, sizeof(FILINFO), &fno, sizeof(FILINFO));  // 复制文件信息
            time = combine_time(&(df[count].fno));  // 合并时间
            if (time < old_time) {  // 更新最旧时间
                old_time = time;
                index = count;
            }
        }
    }
    while ((result = f_readdir(dp, &fno)) == FR_OK) {  // 继续读取剩余文件
        if (fno.fname[0] == 0 || fno.fname[0] == (TCHAR)0xFF) {  // 目录结束
            break;  // 退出循环
        }
        time = combine_time(&fno);  // 合并时间
        if (time < old_time) {  // 如果当前文件更旧
            (void)memcpy_s(&df[index].f_dir, sizeof(DIR), dp, sizeof(DIR));  // 更新目录信息
            (void)memcpy_s(&df[index].fno, sizeof(FILINFO), &fno, sizeof(FILINFO));  // 更新文件信息
            index = get_oldest_time(df, &old_time, CHECK_FILE_NUM);  // 获取新的最旧索引
        }
    }
    index = 0;  // 重置索引
    while (result == FR_OK && index < count) {  // 检查选中的文件
        result = f_fcheckfat(&df[index]);  // 执行FAT检查
        ++index;  // 索引递增
    }

    return result;  // 返回检查结果
}

/**
 * @brief 文件系统检查入口
 * @param vp Vnode指针
 * @param dir 目录结构体
 * @return 0 - 成功，负数 - 错误码
 */
int fatfs_fscheck(struct Vnode* vp, struct fs_dirent_s *dir)
{
    FATFS *fs = (FATFS *)vp->originMount->data;  // 获取文件系统对象
    DIR *dp = NULL;  // 目录指针
    FILINFO *finfo = &(((DIR_FILE *)(vp->data))->fno);  // 文件信息
#ifdef LOSCFG_FS_FAT_CACHE
    los_part *part = NULL;  // 分区结构体（缓存模式）
#endif
    FRESULT result;  // 操作结果
    int ret;  // 临时返回值

    if (fs->fs_type != FS_FAT32) {  // 检查是否为FAT32文件系统
        return -EINVAL;  // 返回无效参数错误
    }

    if ((finfo->fattrib & AM_DIR) == 0) {  // 检查是否为目录
        return -ENOTDIR;  // 返回非目录错误
    }

    ret = fatfs_opendir(vp, dir);  // 打开目录
    if (ret < 0) {  // 打开失败
        return ret;  // 返回错误
    }

    ret = lock_fs(fs);  // 锁定文件系统
    if (ret == FALSE) {  // 锁定失败
        result = FR_TIMEOUT;  // 设置超时错误
        goto ERROR_WITH_DIR;  // 跳转到带目录的错误处理
    }

    dp = (DIR *)dir->u.fs_dir;  // 获取目录指针
    dp->obj.id = fs->id;  // 设置对象ID
    result = fscheck(dp);  // 执行文件系统检查
    if (result != FR_OK) {  // 检查失败
        goto ERROR_UNLOCK;  // 跳转到解锁处理
    }

    unlock_fs(fs, FR_OK);  // 解锁文件系统

    ret = fatfs_closedir(vp, dir);  // 关闭目录
    if (ret < 0) {  // 关闭失败
        return ret;  // 返回错误
    }

#ifdef LOSCFG_FS_FAT_CACHE
    part = get_part((INT)fs->pdrv);  // 获取分区信息
    if (part != NULL) {
        (void)OsSdSync(part->disk_id);  // 同步SD卡
    }
#endif

    return 0;  // 返回成功

ERROR_UNLOCK:
    unlock_fs(fs, result);  // 解锁文件系统并传递错误
ERROR_WITH_DIR:
    fatfs_closedir(vp, dir);  // 关闭目录
    return -fatfs_2_vfs(result);  // 转换错误码并返回
}

/**
 * @brief 创建符号链接
 * @param parentVnode 父目录Vnode
 * @param newVnode 输出参数，新Vnode指针
 * @param path 链接路径
 * @param target 目标路径
 * @return 0 - 成功，负数 - 错误码
 */
int fatfs_symlink(struct Vnode *parentVnode, struct Vnode **newVnode, const char *path, const char *target)
{
    return fatfs_create_obj(parentVnode, path, 0, newVnode, AM_LNK, target);  // 创建链接对象
}

/**
 * @brief 读取符号链接内容
 * @param vnode 符号链接Vnode
 * @param buffer 输出缓冲区
 * @param bufLen 缓冲区长度
 * @return 读取的字节数，负数 - 错误码
 */
ssize_t fatfs_readlink(struct Vnode *vnode, char *buffer, size_t bufLen)
{
    int ret;  // 临时返回值
    FRESULT res;  // 操作结果
    DWORD clust;  // 簇号
    QWORD sect;  // 扇区号
    DIR_FILE *dfp = (DIR_FILE *)(vnode->data);  // 目录文件结构
    DIR *dp = &(dfp->f_dir);  // 目录结构体
    FATFS *fs = dp->obj.fs;  // 文件系统对象
    FILINFO *finfo = &(dfp->fno);  // 文件信息
    size_t targetLen = finfo->fsize;  // 目标长度
    size_t cnt;  // 复制字节数

    ret = lock_fs(fs);  // 锁定文件系统
    if (ret == FALSE) {  // 锁定失败
        return -EBUSY;  // 返回忙错误
    }

    clust = finfo->sclst;  // 获取起始簇
    sect = clst2sect(fs, clust);    // 转换簇为扇区
    if (sect == 0) {  // 扇区无效
        res = FR_DISK_ERR;  // 设置磁盘错误
        goto ERROUT;  // 跳转到错误处理
    }

    if (move_window(fs, sect) != FR_OK) {  // 移动窗口到扇区
        res = FR_DISK_ERR;  // 设置磁盘错误
        goto ERROUT;  // 跳转到错误处理
    }

    cnt = (bufLen - 1) < targetLen ? (bufLen - 1) : targetLen;  // 计算复制长度
    ret = LOS_CopyFromKernel(buffer, bufLen, fs->win, cnt);  // 从内核空间复制
    if (ret != EOK) {  // 复制失败
        res = FR_INVALID_PARAMETER;  // 设置无效参数错误
        goto ERROUT;  // 跳转到错误处理
    }
    buffer[cnt] = '\0';  // 添加字符串结束符

    unlock_fs(fs, FR_OK);  // 解锁文件系统
    return cnt;  // 返回复制字节数

ERROUT:
    unlock_fs(fs, res);  // 解锁文件系统并传递错误
    return -fatfs_2_vfs(res);  // 转换错误码并返回
}

/**
 * @brief 读取文件页
 * @param vnode 文件Vnode
 * @param buff 数据缓冲区
 * @param pos 读取位置
 * @return 读取的字节数，负数 - 错误码
 */
ssize_t fatfs_readpage(struct Vnode *vnode, char *buff, off_t pos)
{
    FATFS *fs = (FATFS *)(vnode->originMount->data);  // 文件系统对象
    DIR_FILE *dfp = (DIR_FILE *)(vnode->data);  // 目录文件结构
    FILINFO *finfo = &(dfp->fno);  // 文件信息
    FAT_ENTRY *ep = &(dfp->fat_entry);  // FAT条目
    DWORD clust;  // 当前簇
    DWORD sclust;  // 起始簇
    QWORD sect;  // 当前扇区
    QWORD step;  // 每次读取扇区数
    QWORD n;  // 已读取扇区数
    size_t position; /* 字节偏移 */
    BYTE *buf = (BYTE *)buff;  // 数据缓冲区
    size_t buflen = PAGE_SIZE;  // 页大小
    FRESULT result;  // 操作结果
    int ret;  // 临时返回值

    ret = lock_fs(fs);  // 锁定文件系统
    if (ret == FALSE) {  // 锁定失败
        result = FR_TIMEOUT;  // 设置超时错误
        goto ERROR_OUT;  // 跳转到错误处理
    }

    if (finfo->fsize <= pos) {  // 读取位置超出文件大小
        result = FR_OK;  // 设置成功
        goto ERROR_UNLOCK;  // 跳转到解锁处理
    }

    if (ep->clst == 0) {  // 簇未初始化
        ep->clst = finfo->sclst;  // 设置起始簇
    }

    if (pos >= ep->pos) {  // 读取位置在当前位置之后
        clust = ep->clst;  // 使用当前簇
        position = ep->pos;  // 使用当前位置
    } else {
        clust = finfo->sclst;  // 从起始簇开始
        position = 0;  // 位置归零
    }

    /* 获取当前簇 */
    n = pos / SS(fs) / fs->csize - position / SS(fs) / fs->csize;  // 计算簇偏移
    while (n--) {  // 移动到目标簇
        clust = get_fat(&(dfp->f_dir.obj), clust);  // 获取下一簇
        if ((clust == BAD_CLUSTER) || (clust == DISK_ERROR)) {  // 簇错误
            result = FR_DISK_ERR;  // 设置磁盘错误
            goto ERROR_UNLOCK;  // 跳转到解锁处理
        }
    }

    /* 获取当前扇区 */
    sect = clst2sect(fs, clust);  // 簇转换为扇区
    sect += (pos / SS(fs)) & (fs->csize - 1);  // 计算扇区内偏移

    /* 计算每次读取扇区数 */
    if (fs->csize < buflen / SS(fs)) {  // 簇大小小于页大小/扇区大小
        step = fs->csize;  // 每次读取一个簇
    } else {
        step = buflen / SS(fs);  // 每次读取页大小对应的扇区数
    }

    n = 0;  // 已读取扇区计数
    sclust = clust;  // 保存起始簇
    while (n < buflen / SS(fs)) {  // 读取直到填满缓冲区
        if (disk_read(fs->pdrv, buf, sect, step) != RES_OK) {  // 读取磁盘
            result = FR_DISK_ERR;  // 设置磁盘错误
            goto ERROR_UNLOCK;  // 跳转到解锁处理
        }
        n += step;  // 更新已读取扇区数
        if (n >= buflen / SS(fs)) {  // 缓冲区已满
            break;  // 退出循环
        }

        /* 簇大小对齐，当簇大小小于页大小时需要跳转到下一簇 */
        clust = get_fat(&(dfp->f_dir.obj), clust);  // 获取下一簇
        if ((clust == BAD_CLUSTER) || (clust == DISK_ERROR)) {  // 簇错误
            result = FR_DISK_ERR;  // 设置磁盘错误
            goto ERROR_UNLOCK;  // 跳转到解锁处理
        } else if (fatfs_is_last_cluster(fs, clust)) {  // 最后一个簇
            break;  // 读取结束
        }
        sect = clst2sect(fs, clust);  // 下一簇转换为扇区
        buf += step * SS(fs);  // 移动缓冲区指针
    }

    ep->clst = sclust;  // 更新当前簇
    ep->pos = pos;  // 更新当前位置

    unlock_fs(fs, FR_OK);  // 解锁文件系统

    return (ssize_t)min(finfo->fsize - pos, n * SS(fs));  // 返回实际读取字节数

ERROR_UNLOCK:
    unlock_fs(fs, result);  // 解锁文件系统并传递错误
ERROR_OUT:
    return -fatfs_2_vfs(result);  // 转换错误码并返回
}

/**
 * @brief 写入文件页
 * @param vnode 文件Vnode
 * @param buff 数据缓冲区
 * @param pos 写入位置
 * @param buflen 写入长度
 * @return 写入的字节数，负数 - 错误码
 */
ssize_t fatfs_writepage(struct Vnode *vnode, char *buff, off_t pos, size_t buflen)
{
    FATFS *fs = (FATFS *)(vnode->originMount->data);  // 文件系统对象
    DIR_FILE *dfp = (DIR_FILE *)(vnode->data);  // 目录文件结构
    FILINFO *finfo = &(dfp->fno);  // 文件信息
    FAT_ENTRY *ep = &(dfp->fat_entry);  // FAT条目
    DWORD clust;  // 当前簇
    DWORD sclst;  // 起始簇
    QWORD sect;  // 当前扇区
    QWORD step;  // 每次写入扇区数
    QWORD n;  // 已写入扇区数
    size_t position; /* 字节偏移 */
    BYTE *buf = (BYTE *)buff;  // 数据缓冲区
    FRESULT result;  // 操作结果
    FIL fil;  // 文件结构体
    int ret;  // 临时返回值

    ret = lock_fs(fs);  // 锁定文件系统
    if (ret == FALSE) {  // 锁定失败
        result = FR_TIMEOUT;  // 设置超时错误
        goto ERROR_OUT;  // 跳转到错误处理
    }

    if (finfo->fsize <= pos) {  // 写入位置超出文件大小
        result = FR_OK;  // 设置成功
        goto ERROR_UNLOCK;  // 跳转到解锁处理
    }

    if (ep->clst == 0) {  // 簇未初始化
        ep->clst = finfo->sclst;  // 设置起始簇
    }

    if (pos >= ep->pos) {  // 写入位置在当前位置之后
        clust = ep->clst;  // 使用当前簇
        position = ep->pos;  // 使用当前位置
    } else {
        clust = finfo->sclst;  // 从起始簇开始
        position = 0;  // 位置归零
    }

    /* 获取当前簇 */
    n = pos / SS(fs) / fs->csize - position / SS(fs) / fs->csize;  // 计算簇偏移
    while (n--) {  // 移动到目标簇
        clust = get_fat(&(dfp->f_dir.obj), clust);  // 获取下一簇
        if ((clust == BAD_CLUSTER) || (clust == DISK_ERROR)) {  // 簇错误
            result = FR_DISK_ERR;  // 设置磁盘错误
            goto ERROR_UNLOCK;  // 跳转到解锁处理
        }
    }

    /* 获取当前扇区 */
    sect = clst2sect(fs, clust);  // 簇转换为扇区
    sect += (pos / SS(fs)) & (fs->csize - 1);  // 计算扇区内偏移

    /* 计算每次写入扇区数 */
    if (fs->csize < buflen / SS(fs)) {  // 簇大小小于写入长度/扇区大小
        step = fs->csize;  // 每次写入一个簇
    } else {
        step = buflen / SS(fs);  // 每次写入指定长度对应的扇区数
    }

    n = 0;  // 已写入扇区计数
    sclst = clust;  // 保存起始簇
    while (n < buflen / SS(fs)) {  // 写入直到完成
        if (disk_write(fs->pdrv, buf, sect, step) != RES_OK) {  // 写入磁盘
            result = FR_DISK_ERR;  // 设置磁盘错误
            goto ERROR_UNLOCK;  // 跳转到解锁处理
        }
        n += step;  // 更新已写入扇区数
        if (n >= buflen / SS(fs)) {  // 写入完成
            break;  // 退出循环
        }

        /* 簇大小对齐，当簇大小小于页大小时需要跳转到下一簇 */
        clust = get_fat(&(dfp->f_dir.obj), clust);  // 获取下一簇
        if ((clust == BAD_CLUSTER) || (clust == DISK_ERROR)) {  // 簇错误
            result = FR_DISK_ERR;  // 设置磁盘错误
            goto ERROR_UNLOCK;  // 跳转到解锁处理
        } else if (fatfs_is_last_cluster(fs, clust)) {  // 最后一个簇
            break;  // 写入结束
        }
        sect = clst2sect(fs, clust);  // 下一簇转换为扇区
        buf += step * SS(fs);  // 移动缓冲区指针
    }

    ep->clst = sclst;  // 更新当前簇
    ep->pos = pos;  // 更新当前位置

    fil.obj.fs = fs;  // 设置文件系统
    if (update_filbuff(finfo, &fil, NULL) < 0) {  // 更新文件缓冲区
        result = FR_DISK_ERR;  // 设置磁盘错误
        goto ERROR_UNLOCK;  // 跳转到解锁处理
    }

    unlock_fs(fs, FR_OK);  // 解锁文件系统

    return (ssize_t)min(finfo->fsize - pos, n * SS(fs));  // 返回实际写入字节数
ERROR_UNLOCK:
    unlock_fs(fs, result);  // 解锁文件系统并传递错误
ERROR_OUT:
    return -fatfs_2_vfs(result);  // 转换错误码并返回
}

/**
 * @brief Vnode操作函数集合
 */
struct VnodeOps fatfs_vops = {
    /* 文件操作 */
    .Getattr = fatfs_stat,  // 获取文件属性
    .Chattr = fatfs_chattr,  // 修改文件属性
    .Lookup = fatfs_lookup,  // 查找文件
    .Rename = fatfs_rename,  // 重命名文件
    .Create = fatfs_create,  // 创建文件
    .ReadPage = fatfs_readpage,  // 读取页
    .WritePage = fatfs_writepage,  // 写入页
    .Unlink = fatfs_unlink,  // 删除文件
    .Reclaim = fatfs_reclaim,  // 释放资源
    .Truncate = fatfs_truncate,  // 截断文件（32位）
    .Truncate64 = fatfs_truncate64,  // 截断文件（64位）
    /* 目录操作 */
    .Opendir = fatfs_opendir,  // 打开目录
    .Readdir = fatfs_readdir,  // 读取目录
    .Rewinddir = fatfs_rewinddir,  // 重置目录指针
    .Closedir = fatfs_closedir,  // 关闭目录
    .Mkdir = fatfs_mkdir,  // 创建目录
    .Rmdir = fatfs_rmdir,  // 删除目录
    .Fscheck = fatfs_fscheck,  // 文件系统检查
    .Symlink = fatfs_symlink,  // 创建符号链接
    .Readlink = fatfs_readlink,  // 读取符号链接
};

/**
 * @brief 挂载操作函数集合
 */
struct MountOps fatfs_mops = {
    .Mount = fatfs_mount,  // 挂载文件系统
    .Unmount = fatfs_umount,  // 卸载文件系统
    .Statfs = fatfs_statfs,  // 获取文件系统状态
    .Sync = fatfs_sync_adapt,  // 同步文件系统
};

/**
 * @brief 文件操作函数集合
 */
struct file_operations_vfs fatfs_fops = {
    .open = fatfs_open,  // 打开文件
    .read = fatfs_read,  // 读取文件
    .write = fatfs_write,  // 写入文件
    .seek = fatfs_lseek,  // 文件定位
    .close = fatfs_close,  // 关闭文件
    .mmap = OsVfsFileMmap,  // 内存映射
    .fallocate = fatfs_fallocate,  // 预分配空间（32位）
    .fallocate64 = fatfs_fallocate64,  // 预分配空间（64位）
    .fsync = fatfs_fsync,  // 同步文件
    .ioctl = fatfs_ioctl,  // I/O控制
};

/* 文件系统映射入口 */
FSMAP_ENTRY(fat_fsmap, "vfat", fatfs_mops, FALSE, TRUE);  // 注册vfat文件系统
#endif /* LOSCFG_FS_FAT */
