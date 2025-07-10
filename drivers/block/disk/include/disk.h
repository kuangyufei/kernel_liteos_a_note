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

/**
 * @defgroup disk Disk
 * @ingroup filesystem
 */

#ifndef _DISK_H
#define _DISK_H

#include "fs/driver.h"
#include "los_base.h"
#include "pthread.h"

#ifdef LOSCFG_FS_FAT_CACHE
#include "bcache.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

/* 磁盘与分区数量限制宏定义 */
#define SYS_MAX_DISK                5                   /* 系统支持的最大磁盘数量 */
#define MAX_DIVIDE_PART_PER_DISK    16                  /* 每个磁盘支持的最大逻辑分区数 */
#define MAX_PRIMARY_PART_PER_DISK   4                   /* 每个磁盘支持的最大主分区数 (MBR规范) */
#define SYS_MAX_PART                (SYS_MAX_DISK * MAX_DIVIDE_PART_PER_DISK) /* 系统支持的总分区数 */
#define DISK_NAME                   255                 /* 磁盘名称最大长度 (字符) */
#define DISK_MAX_SECTOR_SIZE        512                 /* 磁盘扇区最大尺寸 (字节) */

/* MBR分区表相关偏移量定义 */
#define PAR_OFFSET           446     /* MBR分区表起始偏移量 (字节) */
#define BS_SIG55AA           510     /* MBR结束签名偏移量 (0xAA55) */
#define BS_FILSYSTEMTYPE32   82      /* 32位文件系统类型标识偏移量 */
#define BS_JMPBOOT           0       /* x86跳转指令偏移量 (3字节) */
#define BS_FILSYSTYPE        0x36    /* 文件系统类型字段偏移量 */
#define BS_SIG55AA_VALUE     0xAA55  /* MBR有效签名值 (小端格式) */

/* 分区表项字段偏移量 */
#define PAR_TYPE_OFFSET      4       /* 分区类型字段偏移量 (相对于分区表项起始) */
#define PAR_START_OFFSET     8       /* 分区起始扇区偏移量 */
#define PAR_COUNT_OFFSET     12      /* 分区扇区数量偏移量 */
#define PAR_TABLE_SIZE       16      /* 单个分区表项大小 (字节) */
#define EXTENDED_PAR         0x0F    /* 扩展分区类型标识 */
#define EXTENDED_8G          0x05    /* 大于8GB的扩展分区标识 */
#define EMMC                 0xEC    /* eMMC设备分区标识 */
#define OTHERS               0x01    /* 其他存储设备 (SD卡/U盘) 分区标识 */

/* 文件系统类型验证宏 */
#define BS_FS_TYPE_MASK      0xFFFFFF        /* 文件系统类型掩码 (3字节) */
#define BS_FS_TYPE_VALUE     0x544146        /* 文件系统类型特征值 (ASCII: FAT) */
#define BS_FS_TYPE_FAT       0x0B            /* FAT32文件系统类型值 */
#define BS_FS_TYPE_NTFS      0x07            /* NTFS文件系统类型值 */

/* 字节位置定义 */
#define FIRST_BYTE       1   /* 第一个字节索引 */
#define SECOND_BYTE      2   /* 第二个字节索引 */
#define THIRD_BYTE       3   /* 第三个字节索引 */
#define FOURTH_BYTE      4   /* 第四个字节索引 */

#define BIT_FOR_BYTE     8   /* 每字节位数 */

/* 磁盘数据读取宏 (小端格式转换) */
#define LD_WORD_DISK(ptr)    (UINT16)(((UINT16)*((UINT8 *)(ptr) + FIRST_BYTE) << (BIT_FOR_BYTE * FIRST_BYTE)) | \
                                      (UINT16)*(UINT8 *)(ptr)) /* 从磁盘读取16位无符号整数 */
#define LD_DWORD_DISK(ptr)   (UINT32)(((UINT32)*((UINT8 *)(ptr) + THIRD_BYTE) << (BIT_FOR_BYTE * THIRD_BYTE)) |   \
                                      ((UINT32)*((UINT8 *)(ptr) + SECOND_BYTE) << (BIT_FOR_BYTE * SECOND_BYTE)) | \
                                      ((UINT16)*((UINT8 *)(ptr) + FIRST_BYTE) << (BIT_FOR_BYTE * FIRST_BYTE)) |   \
                                      (*(UINT8 *)(ptr))) /* 从磁盘读取32位无符号整数 */

#define LD_QWORD_DISK(ptr)   ((UINT64)(((UINT64)LD_DWORD_DISK(&(ptr)[FOURTH_BYTE]) << (BIT_FOR_BYTE * FOURTH_BYTE)) | \
                              LD_DWORD_DISK(ptr))) /* 从磁盘读取64位无符号整数 */

/* 文件系统验证宏 */
#define VERIFY_FS(ptr)       (((LD_DWORD_DISK(&(ptr)[BS_FILSYSTEMTYPE32]) & BS_FS_TYPE_MASK) == BS_FS_TYPE_VALUE) || \
                              !strncmp(&(ptr)[BS_FILSYSTYPE], "FAT", strlen("FAT")) || \
                              !strncmp(&(ptr)[BS_JMPBOOT], "\xEB\x52\x90" "NTFS    ",  \
                                       strlen("\xEB\x52\x90" "NTFS    "))) /* 验证FAT/NTFS文件系统签名 */

#define PARTION_MODE_BTYE    (PAR_OFFSET + PAR_TYPE_OFFSET) /* 分区模式标识偏移量 */
#define PARTION_MODE_GPT     0xEE /* GPT分区表标识 (0xEE表示GPT分区模式) */
#define SIGNATURE_OFFSET     0    /* GPT头部签名偏移量 */
#define SIGNATURE_LEN        8    /* GPT签名长度 (字节) */
#define HEADER_SIZE_OFFSET   12   /* GPT头部大小偏移量 */
#define TABLE_SIZE_OFFSET    84   /* GPT分区表项大小偏移量 */
#define TABLE_NUM_OFFSET     80   /* GPT分区表项数量偏移量 */
#define TABLE_START_SECTOR   2    /* GPT分区表起始扇区 */
#define TABLE_MAX_NUM        128  /* GPT最大分区表项数量 */
#define TABLE_SIZE           128  /* GPT分区表项大小 (字节) */
#define GPT_PAR_START_OFFSET      32  /* GPT分区起始地址偏移量 */
#define GPT_PAR_END_OFFSET        40  /* GPT分区结束地址偏移量 */
#define PAR_ENTRY_NUM_PER_SECTOR  4   /* 每扇区GPT分区表项数量 */
#define HEADER_SIZE_MASK          0xFFFFFFFF /* GPT头部大小掩码 */
#define HEADER_SIZE               0x5C      /* GPT头部标准大小 (92字节) */
#define HARD_DISK_GUID_OFFSET     56       /* 硬盘GUID偏移量 */
#define HARD_DISK_GUID_FOR_ESP    0x0020004900460045 /* ESP分区GUID标识 */
#define HARD_DISK_GUID_FOR_MSP    0x007200630069004D /* MSP分区GUID标识 */
#define PAR_VALID_OFFSET0         0        /* 分区有效性校验偏移量0 */
#define PAR_VALID_OFFSET1         4        /* 分区有效性校验偏移量1 */
#define PAR_VALID_OFFSET2         8        /* 分区有效性校验偏移量2 */
#define PAR_VALID_OFFSET3         12       /* 分区有效性校验偏移量3 */

#define VERIFY_GPT(ptr) ((!strncmp(&(ptr)[SIGNATURE_OFFSET], "EFI PART", SIGNATURE_LEN)) && \
                         ((LD_DWORD_DISK(&(ptr)[HEADER_SIZE_OFFSET]) & HEADER_SIZE_MASK) == HEADER_SIZE)) /* 验证GPT分区表有效性 */

#define VERITY_PAR_VALID(ptr) ((LD_DWORD_DISK(&(ptr)[PAR_VALID_OFFSET0]) + \
                                LD_DWORD_DISK(&(ptr)[PAR_VALID_OFFSET1]) + \
                                LD_DWORD_DISK(&(ptr)[PAR_VALID_OFFSET2]) + \
                                LD_DWORD_DISK(&(ptr)[PAR_VALID_OFFSET3])) != 0) /* 验证分区表项有效性 */

/* ESP/MSP分区过滤 */
#define VERITY_AVAILABLE_PAR(ptr) ((LD_QWORD_DISK(&(ptr)[HARD_DISK_GUID_OFFSET]) != HARD_DISK_GUID_FOR_ESP) && \
                                   (LD_QWORD_DISK(&(ptr)[HARD_DISK_GUID_OFFSET]) != HARD_DISK_GUID_FOR_MSP)) /* 排除ESP和MSP分区 */

/* disk_ioctrl函数命令码定义 */
/* 通用命令 (FatFs使用) */
#define DISK_CTRL_SYNC          0   /* 完成挂起的写操作 */
#define DISK_GET_SECTOR_COUNT   1   /* 获取介质总扇区数 */
#define DISK_GET_SECTOR_SIZE    2   /* 获取扇区大小 (字节) */
#define DISK_GET_BLOCK_SIZE     3   /* 获取擦除块大小 (扇区数) */
#define DISK_CTRL_TRIM          4   /* 通知设备扇区数据不再使用 */

/* 通用命令 (非FatFs使用) */
#define DISK_CTRL_POWER         5   /* 获取/设置电源状态 */
#define DISK_CTRL_LOCK          6   /* 锁定/解锁介质移除 */
#define DISK_CTRL_EJECT         7   /* 弹出介质 */
#define DISK_CTRL_FORMAT        8   /* 格式化介质 */

/* MMC/SDC专用命令 */
#define DISK_MMC_GET_TYPE       10  /* 获取卡类型 */
#define DISK_MMC_GET_CSD        11  /* 获取CSD寄存器值 */
#define DISK_MMC_GET_CID        12  /* 获取CID寄存器值 */
#define DISK_MMC_GET_OCR        13  /* 获取OCR寄存器值 */
#define DISK_MMC_GET_SDSTAT     14  /* 获取SD状态 */

/* ATA/CF专用命令 */
#define DISK_ATA_GET_REV        20  /* 获取固件版本 */
#define DISK_ATA_GET_MODEL      21  /* 获取型号名称 */
#define DISK_ATA_GET_SN         22  /* 获取序列号 */

#ifndef LOSCFG_FS_FAT_CACHE
#define DISK_DIRECT_BUFFER_SIZE 4   /* 禁用缓存时的直接IO缓冲区大小 */
#endif

/**
 * @brief 磁盘状态枚举
 * @details 定义磁盘可能的三种状态：未使用、使用中、未就绪
 */
typedef enum _disk_status_ {
    STAT_UNUSED,   // 磁盘未使用
    STAT_INUSED,   // 磁盘使用中
    STAT_UNREADY   // 磁盘未就绪
} disk_status_e;

/**
 * @brief 磁盘结构体
 * @details 表示物理磁盘的基本信息和状态，包含分区链表头和缓存信息
 */
typedef struct _los_disk_ {
    UINT32 disk_id : 8;     /* 物理磁盘编号 */
    UINT32 disk_status : 2; /* 磁盘状态，取值为disk_status_e枚举 */
    UINT32 part_count : 8;  /* 当前分区数量 */
    UINT32 reserved : 14;   /* 保留位 */
    struct Vnode *dev;      /* 设备Vnode指针 */
#ifdef LOSCFG_FS_FAT_CACHE
    OsBcache *bcache;       /* 磁盘缓存，所有分区共享 */
#endif
    UINT32 sector_size;     /* 磁盘扇区大小（字节） */
    UINT64 sector_start;    /* 磁盘起始扇区 */
    UINT64 sector_count;    /* 磁盘扇区总数 */
    UINT8 type;             /* 磁盘类型 */
    CHAR *disk_name;        /* 磁盘名称 */
    LOS_DL_LIST head;       /* 所有分区的链表头 */
    struct pthread_mutex disk_mutex; /* 磁盘互斥锁 */
#ifndef LOSCFG_FS_FAT_CACHE
    UINT8 *buff;            /* 直接缓冲区（未启用FAT缓存时使用） */
#endif
} los_disk;

/**
 * @brief 分区结构体
 * @details 表示磁盘分区的详细信息，包含分区在磁盘和系统中的编号、文件系统类型等
 */
typedef struct _los_part_ {
    UINT32 disk_id : 8;      /* 所属物理磁盘编号 */
    UINT32 part_id : 8;      /* 系统中的分区编号 */
    UINT32 part_no_disk : 8; /* 磁盘中的分区编号 */
    UINT32 part_no_mbr : 5;  /* MBR中的分区编号 */
    UINT32 reserved : 3;     /* 保留位 */
    UINT8 filesystem_type;   /* 分区使用的文件系统类型 */
    UINT8 type;              /* 分区类型 */
    struct Vnode *dev;       /* 分区设备Vnode指针 */
    CHAR *part_name;         /* 分区名称 */
    UINT64 sector_start;     /* 分区相对于主设备的起始扇区偏移量
                              *（多MBR分区视为同一分区） */
    UINT64 sector_count;     /* 分区扇区总数
                              * 如果未执行addpartition操作，则等于主设备扇区数 */
    LOS_DL_LIST list;        /* 分区链表节点 */
} los_part;

/**
 * @brief 分区信息结构体
 * @details 存储单个分区的基本信息：类型、起始扇区和扇区数量
 */
struct partition_info {
    UINT8 type;              /* 分区类型 */
    UINT64 sector_start;     /* 分区起始扇区 */
    UINT64 sector_count;     /* 分区扇区数量 */
};

/**
 * @brief 磁盘分区划分信息结构体
 * @details 存储磁盘分区划分的完整信息，包括总扇区数、扇区大小和分区列表
 */
struct disk_divide_info {
    UINT64 sector_count;     /* 磁盘总扇区数 */
    UINT32 sector_size;      /* 扇区大小（字节） */
    UINT32 part_count;       /* 分区数量 */
    /*
     * 保留主分区位置并设置为0，以防所有分区都是逻辑分区（当前最多16个）
     * 因此最大分区数应为4（主分区）+ 16（逻辑分区）
     */
    struct partition_info part[MAX_DIVIDE_PART_PER_DISK + MAX_PRIMARY_PART_PER_DISK]; /* 分区信息数组 */
};

/**
 * @ingroup  disk
 * @brief Disk driver initialization.
 *
 * @par Description:
 * Initializate a disk dirver, and set the block cache.
 *
 * @attention
 * <ul>
 * <li>The parameter diskName must point a valid string, which end with the terminating null byte.</li>
 * <li>The total length of parameter diskName must be less than the value defined by PATH_MAX.</li>
 * <li>The parameter bops must pointed the right functions, otherwise the system
 * will crash when the disk is being operated.</li>
 * <li>The parameter info can be null or point to struct disk_divide_info. when info is null,
 * the disk will be divided base the information of MBR, otherwise,
 * the disk will be divided base the information of parameter info.</li>
 * </ul>
 *
 * @param  diskName  [IN] Type #const CHAR *                      disk driver name.
 * @param  bops      [IN] Type #const struct block_operations *   block driver control structure.
 * @param  priv      [IN] Type #VOID *                            private data of vnode.
 * @param  diskID    [IN] Type #INT32                             disk id number, less than SYS_MAX_DISK.
 * @param  info      [IN] Type #VOID *                            disk driver partition information.
 *
 * @retval #0      Initialization success.
 * @retval #-1     Initialization failed.
 *
 * @par Dependency:
 * <ul><li>disk.h</li></ul>
 * @see los_disk_deinit
 *
 */
INT32 los_disk_init(const CHAR *diskName, const struct block_operations *bops,
                    VOID *priv, INT32 diskID, VOID *info);

/**
 * @ingroup  disk
 * @brief Destroy a disk driver.
 *
 * @par Description:
 * Destroy a disk driver, free the dependent resource.
 *
 * @attention
 * <ul>
 * None
 * </ul>
 *
 * @param  diskID [IN] Type #INT32  disk driver id number, less than the value defined by SYS_MAX_DISK.
 *
 * @retval #0      Destroy success.
 * @retval #-1     Destroy failed.
 *
 * @par Dependency:
 * <ul><li>disk.h</li></ul>
 * @see los_disk_init
 *
 */
INT32 los_disk_deinit(INT32 diskID);

/**
 * @ingroup  disk
 * @brief Read data from disk driver.
 *
 * @par Description:
 * Read data from disk driver.
 *
 * @attention
 * <ul>
 * <li>The sector size of the disk to be read should be acquired by los_part_ioctl before calling this function.</li>
 * <li>The parameter buf must point to a valid memory and the buf size is count * sector_size.</li>
 * </ul>
 *
 * @param  drvID   [IN]  Type #INT32         disk driver id number, less than the value defined by SYS_MAX_DISK.
 * @param  buf     [OUT] Type #VOID *        memory which used to store read data.
 * @param  sector  [IN]  Type #UINT64        expected start sector number to read.
 * @param  count   [IN]  Type #UINT32        expected sector count to read.
 * @param  useRead [IN]  Type #BOOL          set FALSE to use the write block for optimization
 *
 * @retval #0      Read success.
 * @retval #-1     Read failed.
 *
 * @par Dependency:
 * <ul><li>disk.h</li></ul>
 * @see los_disk_write
 *
 */
INT32 los_disk_read(INT32 drvID, VOID *buf, UINT64 sector, UINT32 count, BOOL useRead);

/**
 * @ingroup  disk
 * @brief Write data to a disk driver.
 *
 * @par Description:
 * Write data to a disk driver.
 *
 * @attention
 * <ul>
 * <li>The sector size of the disk to be read should be acquired by los_part_ioctl before calling this function.</li>
 * <li>The parameter buf must point to a valid memory and the buf size is count * sector_size.</li>
 * </ul>
 *
 * @param  drvID   [IN]  Type #INT32           disk driver id number, less than the value defined by SYS_MAX_DISK.
 * @param  buf     [IN]  Type #const VOID *    memory which used to storage write data.
 * @param  sector  [IN]  Type #UINT64          expected start sector number to read.
 * @param  count   [IN]  Type #UINT32          experted sector count of write.
 *
 * @retval #0      Write success.
 * @retval #-1     Write failed.
 *
 * @par Dependency:
 * <ul><li>disk.h</li></ul>
 * @see los_disk_read
 *
 */
INT32 los_disk_write(INT32 drvID, const VOID *buf, UINT64 sector, UINT32 count);

/**
 * @ingroup  disk
 * @brief Get information of disk driver.
 *
 * @par Description:
 * Get information of disk driver.
 *
 * @attention
 * <ul>
 * None
 * </ul>
 *
 * @param  drvID [IN]  Type #INT32     disk driver id number, less than the value defined by SYS_MAX_DISK.
 * @param  cmd   [IN]  Type #INT32     command to issu, currently support GET_SECTOR_COUNT, GET_SECTOR_SIZE,
 *                                     GET_BLOCK_SIZE, CTRL_SYNC.
 * @param  buf   [OUT] Type #VOID *    memory to storage the information, the size must enough for data type(UINT64)
 *                                     when cmd type is DISK_GET_SECTOR_COUNT, others is size_t.
 *
 * @retval #0      Get information success.
 * @retval #-1     Get information failed.
 *
 * @par Dependency:
 * <ul><li>disk.h</li></ul>
 * @see None
 *
 */
INT32 los_disk_ioctl(INT32 drvID, INT32 cmd, VOID *buf);

/**
 * @ingroup  disk
 * @brief Sync blib cache.
 *
 * @par Description:
 * Sync blib cache, write the valid data to disk driver.
 *
 * @attention
 * <ul>
 * None
 * </ul>
 *
 * @param  drvID [IN] Type #INT32  disk driver id number, less than the value defined by SYS_MAX_DISK.
 *
 * @retval #0         Sync success.
 * @retval #INT32     Sync failed.
 *
 * @par Dependency:
 * <ul><li>disk.h</li></ul>
 * @see None
 *
 */
INT32 los_disk_sync(INT32 drvID);

/**
 * @ingroup  disk
 * @brief Set blib cache for the disk driver.
 *
 * @par Description:
 * Set blib cache for the disk driver, users can set the number of sectors of per block,
 * and the number of blocks.
 *
 * @attention
 * <ul>
 * None
 * </ul>
 *
 * @param  drvID             [IN] Type #INT32     disk driver id number, less than the value defined by SYS_MAX_DISK.
 * @param  sectorPerBlock    [IN] Type #UINT32    sector number of per block, only can be 32 * (1, 2, ..., 8).
 * @param  blockNum          [IN] Type #UINT32    block number of cache.
 *
 * @retval #0         Set success.
 * @retval #INT32     Set failed.
 *
 * @par Dependency:
 * <ul><li>disk.h</li></ul>
 * @see None
 *
 */
INT32 los_disk_set_bcache(INT32 drvID, UINT32 sectorPerBlock, UINT32 blockNum);

/**
 * @ingroup  disk
 * @brief Read data from chosen partition.
 *
 * @par Description:
 * Read data from chosen partition.
 *
 * @attention
 * <ul>
 * <li>The sector size of the disk to be read should be acquired by los_part_ioctl before calling this function.</li>
 * <li>The parameter buf must point to valid memory and the buf size is count * sector_size.</li>
 * </ul>
 *
 * @param  pt      [IN]  Type #INT32        partition number, less than the value defined by SYS_MAX_PART.
 * @param  buf     [OUT] Type #VOID *       memory which used to store the data to be read.
 * @param  sector  [IN]  Type #UINT64       start sector number of chosen partition.
 * @param  count   [IN]  Type #UINT32       the expected sector count for reading.
 * @param  useRead [IN]  Type #BOOL         FALSE when reading large contiguous data, TRUE for other situations
 *
 * @retval #0      Read success.
 * @retval #-1     Read failed.
 *
 * @par Dependency:
 * <ul><li>disk.h</li></ul>
 * @see los_part_read
 *
 */
INT32 los_part_read(INT32 pt, VOID *buf, UINT64 sector, UINT32 count, BOOL useRead);

/**
 * @ingroup  disk
 * @brief Write data to chosen partition.
 *
 * @par Description:
 * Write data to chosen partition.
 *
 * @attention
 * <ul>
 * <li>The sector size of the disk to be write should be acquired by los_part_ioctl before calling this function.</li>
 * <li>The parameter buf must point to valid memory and the buf size is count * sector_size.</li>
 * </ul>
 *
 * @param  pt      [IN] Type #INT32        partition number,less than the value defined by SYS_MAX_PART.
 * @param  buf     [IN] Type #VOID *       memory which used to storage the written data.
 * @param  sector  [IN] Type #UINT64       start sector number of chosen partition.
 * @param  count   [IN] Type #UINT32       the expected sector count for write.
 *
 * @retval #0      Write success.
 * @retval #-1     Write failed.
 *
 * @par Dependency:
 * <ul><li>disk.h</li></ul>
 * @see los_part_read
 *
 */
INT32 los_part_write(INT32 pt, const VOID *buf, UINT64 sector, UINT32 count);

/**
 * @ingroup  disk
 * @brief Clear the bcache data
 *
 * @par Description:
 * Flush the data and mark the block as unused.
 *
 * @attention
 * <ul>
 * None
 * </ul>
 *
 * @param  drvID      [IN] Type #INT32        disk id
 *
 * @retval #0      Write success.
 * @retval #-1     Write failed.
 *
 * @par Dependency:
 * <ul><li>disk.h</li></ul>
 * @see los_part_read
 *
 */
INT32 los_disk_cache_clear(INT32 drvID);

/**
 * @ingroup  disk
 * @brief Get information of chosen partition.
 *
 * @par Description:
 * By passed command to get information of chosen partition.
 *
 * @attention
 * <ul>
 * None
 * </ul>
 *
 * @param  pt   [IN]  Type #INT32      partition number,less than the value defined by SYS_MAX_PART.
 * @param  cmd  [IN]  Type #INT32      command to issu, currently support GET_SECTOR_COUNT, GET_SECTOR_SIZE,
 *                                     GET_BLOCK_SIZE, CTRL_SYNC.
 * @param  buf  [OUT] Type #VOID *     memory to store the information, the size must enough for data type (UINT64)
 *                                     when cmd type is DISK_GET_SECTOR_COUNT, others is size_t.
 *
 * @retval #0      Get information success.
 * @retval #-1     Get information failed.
 *
 * @par Dependency:
 * <ul><li>disk.h</li></ul>
 * @see None
 *
 */
INT32 los_part_ioctl(INT32 pt, INT32 cmd, VOID *buf);

/**
 * @ingroup  disk
 * @brief Decide the chosen partition is exist or not.
 *
 * @par Description:
 * Decide the chosen partition is exist or not.
 *
 * @attention
 * <ul>
 * <li>The parameter dev is a full path, which begin with '/' and end with '/0'.</li>
 * </ul>
 *
 * @param  dev  [IN]  Type #const CHAR *    partition driver name.
 * @param  mode [IN]  Type #mode_t          access modd.
 *
 * @retval #0      The chosen partition is exist.
 * @retval #-1     The chosen partition is not exist.
 *
 * @par Dependency:
 * <ul><li>disk.h</li></ul>
 * @see None
 *
 */
INT32 los_part_access(const CHAR *dev, mode_t mode);

/**
 * @ingroup  disk
 * @brief Find disk partition.
 *
 * @par Description:
 * By driver partition vnode to find disk partition.
 *
 * @attention
 * <ul>
 * None
 * </ul>
 *
 * @param  blkDriver  [IN]  Type #struct Vnode *    partition driver vnode.
 *
 * @retval #NULL           Can't find chosen disk partition.
 * @retval #los_part *     This is partition structure pointer of chosen disk partition.
 *
 * @par Dependency:
 * <ul><li>disk.h</li></ul>
 * @see None
 *
 */
los_part *los_part_find(struct Vnode *blkDriver);

/**
 * @ingroup  disk
 * @brief Find disk driver.
 *
 * @par Description:
 * By disk driver id number to find disk dirver.
 *
 * @attention
 * <ul>
 * None
 * </ul>
 *
 * @param  id  [IN]  Type #INT32  disk id number,less than the value defined by SYS_MAX_DISK.
 *
 * @retval #NULL           Can't find chosen disk driver.
 * @retval #los_disk *     This is disk structure pointer of chosen disk driver.
 *
 * @par Dependency:
 * <ul><li>disk.h</li></ul>
 * @see None
 *
 */
los_disk *get_disk(INT32 id);

/**
 * @ingroup  disk
 * @brief Find disk partition.
 *
 * @par Description:
 * By driver partition id number to find disk partition.
 *
 * @attention
 * <ul>
 * None
 * </ul>
 *
 * @param  id  [IN]  Type #INT32 partition id number,less than the value defined by SYS_MAX_PART.
 *
 * @retval #NULL           Can't find chosen disk partition.
 * @retval #los_part *     This is partition structure pointer of chosen disk partition.
 *
 * @par Dependency:
 * <ul><li>disk.h</li></ul>
 * @see None
 *
 */
los_part *get_part(INT32 id);

/**
 * @ingroup  disk
 * @brief Print partition information.
 *
 * @par Description:
 * Print partition information.
 *
 * @attention
 * <ul>
 * None
 * </ul>
 *
 * @param  part  [IN]  Type #los_part *  partition control structure pointer
 *
 * @par Dependency:
 * <ul><li>disk.h</li></ul>
 * @see None
 *
 */
VOID show_part(los_part *part);

/**
 * @ingroup  disk
 * @brief Add a new mmc partition.
 *
 * @par Description:
 * Add a new mmc partition, users can set the start sector and size of the new partition.
 *
 * @attention
 * <ul>
 * None
 * </ul>
 *
 * @param  info          [IN]  Type #struct disk_divide_info *  Disk driver information structure pointer.
 * @param  sectorStart   [IN]  Type #size_t                     Start sector number of the new partition.
 * @param  sectorCount   [IN]  Type #size_t                     Sector count of the new partition.
 *
 * @retval #0      Add partition success.
 * @retval #-1     Add partition failed.
 *
 * @par Dependency:
 * <ul><li>disk.h</li></ul>
 * @see None
 *
 */
INT32 add_mmc_partition(struct disk_divide_info *info, size_t sectorStart, size_t sectorCount);

/**
 * @ingroup  disk
 * @brief alloc a new UNUSED disk id.
 *
 * @par Description:
 * Get a free disk id for new device.
 *
 * @attention
 * <ul>
 * <li>The parameter diskName must point a valid string, which end with the null byte ('\0') </li>
 * <li>The total length of parameter diskName must be less than the value defined by DISK_NAME </li>
 * </ul>
 *
 * @param  diskName      [IN]  Type #const CHAR *   device name.
 *
 * @retval #INT32        available disk id
 * @retval #-1           alloc disk id failed

 * @par Dependency:
 * <ul><li>disk.h</li></ul>
 * @see los_get_diskid_byname
 *
 */
INT32 los_alloc_diskid_byname(const CHAR *diskName);

/**
 * @ingroup  disk
 * @brief get the INUSED disk id.
 *
 * @par Description:
 * Get the corresponding INUSED disk id by diskName.
 *
 * @attention
 * <ul>
 * <li>The parameter diskName must point a valid string, which end with the null byte ('\0') </li>
 * <li>The total length of parameter diskName must be less than the value defined by DISK_NAME </li>
 * </ul>
 *
 * @param  diskName      [IN]  Type #const CHAR *  device name.
 *
 * @retval #INT32       available disk id
 * @retval #-1          get disk id failed

 * @par Dependency:
 * <ul><li>disk.h</li></ul>
 * @see los_alloc_diskid_byname
 *
 */
INT32 los_get_diskid_byname(const CHAR *diskName);


los_disk *los_get_mmcdisk_bytype(UINT8 type);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif
