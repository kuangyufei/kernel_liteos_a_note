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

#include "fs/fs.h"
#include "los_base.h"
#include "pthread.h"

#ifdef LOSCFG_FS_FAT_CACHE
#include "bcache.h"
#endif

#include "pthread.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

/***********************************************
https://blog.csdn.net/buzaikoulan/article/details/44405915
MBR 全称为Master Boot Record，即硬盘的主引导记录,硬盘分区有三种，主磁盘分区、扩展磁盘分区、逻辑分区
主分区：也叫引导分区，硬盘的启动分区,最多可能创建4个
扩展磁盘分区:除了主分区外，剩余的磁盘空间就是扩展分区了，扩展分区可以没有，最多1个。
逻辑分区：在扩展分区上面，可以创建多个逻辑分区。逻辑分区相当于一块存储截止，和操作系统还有别的逻辑分区、主分区没有什么关系，是“独立的”。
硬盘的容量＝主分区的容量＋扩展分区的容量 
扩展分区的容量＝各个逻辑分区的容量之和 
通俗的讲主分区是硬盘的主人，而扩展分区是这个硬盘上的仆人，主分区和扩展分区为主从关系。 

给新硬盘上建立分区时都要遵循以下的顺序：建立主分区→建立扩展分区→建立逻辑分区→激活主分区→格式化所有分区
--------------------------
与MBR对应的是GPT,是对超大容量磁盘的一种分区格式
GPT，即Globally Unique Identifier Partition Table Format，全局唯一标识符的分区表的格式。
这种分区模式相比MBR有着非常多的优势。
首先，它至少可以分出128个分区，完全不需要扩展分区和逻辑分区来帮忙就可以分出任何想要的分区来。
其次，GPT最大支持18EB的硬盘，几乎就相当于没有限制。

扇区:是硬件设备传送数据的基本单位。
块:	  是VFS和文件系统传送数据的基本单位。块大小必须是2的幂，而且不能超过一个页框（4K），它必须是扇区的整数倍，每块包含整数个扇区。
  块设备大小不唯一，同一个磁盘上的不同分区可能使用不同的块大小。每个块都需要自己的块缓冲区，即内核用来存放块内容的RAM内存区。
  当内核从磁盘读出一个块时，就用从硬件设备中所获得的值来填充相应的块缓冲区。写块也是同理。这个缓冲区就是 page cache(页高速缓存)

***********************************************/
#define SYS_MAX_DISK                5	//最大支持磁盘数量
#define MAX_DIVIDE_PART_PER_DISK    16	//每个磁盘最大支持逻辑分区数
#define MAX_PRIMARY_PART_PER_DISK   4	//每个磁盘最大支持主分区数
#define SYS_MAX_PART                (SYS_MAX_DISK * MAX_DIVIDE_PART_PER_DISK)	//系统最大支持分区数,80个分区
#define DISK_NAME                   255	//磁盘名称长度上限
#define DISK_MAX_SECTOR_SIZE        512	//扇区大小,最小不能小于 512

/***********************************************
MBR扇区由三部分构成：
第一部分是446字节的引导代码，也就是上面提到的MBR；
第二部分是DPT（Disk Partition Table，硬盘分区表），包含4个表项，每个表项16字节，共占用64字节；446 + 64 = 510;
	分区引导扇区也称DBR（DOS BOOT RECORD）在对硬盘分区之后，每一个分区均有一个DBR与之对应。DBR位于每个分区的第一个扇区，大小为512字节。
	DBR装入内存后，即开始执行该引导程序段，其主要功能是完成操作系统的自举并将控制权交给操作系统。
	每个分区都有引导扇区，但只有被设为活动分区才会被MBR装的DBR入内存运行,所谓的活动分区就是大家理解的主分区 C盘这类的概念,可以被引导启动.
第三部分从510开始,是2个字节的结束标志，0x55AA.
EBR（Extended Boot Record）是与MBR相对应的一个概念。前边已经讲过，MBR里有一个DPT(Disk Partition Table，磁盘分区表)的区域，
它一共是64字节，按每16个字节作为一个分区表项，它最多只能容纳4个分区。能够在MBR的DPT里进行说明的分区称为主分区。
如果我们想分区多于4个的时候，MBR的DPT里就会容纳不下来，于是微软就想出了另一个解决方案，
在MBR里，只放不多于三个主分区，剩下的分区，则由与MBR结构很相像的另一种分区结构（EBR，也就是扩展分区引导记录）进行说明。
一个EBR不够用时，可以增加另一个EBR，如此像一根链条一样地接下去，直到够用为止。

https://blog.csdn.net/Hilavergil/article/details/79270379
***********************************************/
//MBR格式解析
#define PAR_OFFSET           446     /* MBR: Partition table offset (2) *///记录分区表的位置
#define BS_SIG55AA           510     /* Signature word (2) */ //引导记录签名 
#define BS_FILSYSTEMTYPE32   82      /* File system type (1) */ //文件系统类型
#define BS_JMPBOOT           0       /* x86 jump instruction (3-byte) *///x86跳转指令,跳转到引导程序.
#define BS_FILSYSTYPE        0x36    /* File system type (2) */ //文件系统类型
#define BS_SIG55AA_VALUE     0xAA55	 //可理解为 MBR[BS_SIG55AA]上必须是 0xAA55

#define PAR_TYPE_OFFSET      4	//分区类型偏移,即 MBR[446 + 4]位置是分区的类型,一般为主分区,但可能是扩展分区.
#define PAR_START_OFFSET     8	//分区开始偏移,即 MBR[446 + 8]位置是分区的开始扇区号
#define PAR_COUNT_OFFSET     12	//分区数量,即 MBR[446 + 12]位置是分区的总扇区数量
#define PAR_TABLE_SIZE       16	//分区表大小,每个表项16字节
#define EXTENDED_PAR         0x0F 	//分区类型,类型:扩展分区
#define EXTENDED_8G          0x05	//也是分区类型,类型:扩展分区
#define EMMC                 0xEC	//eMMC=NAND闪存+闪存控制芯片+标准接口封装 https://www.huaweicloud.com/articles/bcdefd0d9da5de83d513123ef3aabcf0.html
#define OTHERS               0x01    /* sdcard or umass */

#define BS_FS_TYPE_MASK      0xFFFFFF	//掩码
#define BS_FS_TYPE_VALUE     0x544146	//文件系统类型
#define BS_FS_TYPE_FAT       0x0B		//FAT格式文件系统
#define BS_FS_TYPE_NTFS      0x07		//NTFS格式文件系统

#define FIRST_BYTE       1
#define SECOND_BYTE      2
#define THIRD_BYTE       3
#define FOURTH_BYTE      4

#define BIT_FOR_BYTE     8
//高低位互换函数(16位), 0x0102 - > 变成 0x0201
#define LD_WORD_DISK(ptr)    (UINT16)(((UINT16)*((UINT8 *)(ptr) + FIRST_BYTE) << (BIT_FOR_BYTE * FIRST_BYTE)) | \
                                      (UINT16)*(UINT8 *)(ptr))
//高低位互换函数(32位), 0x01020304 - > 变成 0x04030201                                      
#define LD_DWORD_DISK(ptr)   (UINT32)(((UINT32)*((UINT8 *)(ptr) + THIRD_BYTE) << (BIT_FOR_BYTE * THIRD_BYTE)) |   \
                                      ((UINT32)*((UINT8 *)(ptr) + SECOND_BYTE) << (BIT_FOR_BYTE * SECOND_BYTE)) | \
                                      ((UINT16)*((UINT8 *)(ptr) + FIRST_BYTE) << (BIT_FOR_BYTE * FIRST_BYTE)) |   \
                                      (*(UINT8 *)(ptr)))
//高低位互换函数(64位), 0x01..08 - > 变成 0x08...01
#define LD_QWORD_DISK(ptr)   ((UINT64)(((UINT64)LD_DWORD_DISK(&(ptr)[FOURTH_BYTE]) << (BIT_FOR_BYTE * FOURTH_BYTE)) | \
                              LD_DWORD_DISK(ptr)))
//每个分区的引导记录（VBR）
/* Check VBR string, including FAT, NTFS */ //确认文件系统
#define VERIFY_FS(ptr)       (((LD_DWORD_DISK(&(ptr)[BS_FILSYSTEMTYPE32]) & BS_FS_TYPE_MASK) == BS_FS_TYPE_VALUE) || \
                              !strncmp(&(ptr)[BS_FILSYSTYPE], "FAT", strlen("FAT")) || \
                              !strncmp(&(ptr)[BS_JMPBOOT], "\xEB\x52\x90" "NTFS    ",  \
                                       strlen("\xEB\x52\x90" "NTFS    ")))
//开头 EB5290 代表是 NTFS文件系统,ptr[36]开始为"FAT"代表 FAT文件系统
//https://www.jianshu.com/p/1be47267b98b
//GPT格式解析
#define PARTION_MODE_BTYE    (PAR_OFFSET + PAR_TYPE_OFFSET) /* 0xEE: GPT(GUID), else: MBR */ //两种分区模式
#define PARTION_MODE_GPT     0xEE /* 0xEE: GPT(GUID), else: MBR */ //GPT和MBR
#define SIGNATURE_OFFSET     0    /* The offset of GPT partition header signature */ //从这个位置读取分区表头签名
#define SIGNATURE_LEN        8    /* The length of GPT signature *///分区表签名的长度,占8个字节
#define HEADER_SIZE_OFFSET   12   /* The offset of GPT header size *///从这个位置读取分区表头大小
#define TABLE_SIZE_OFFSET    84   /* The offset of GPT table size *///从这个位置读取分区表大小
#define TABLE_NUM_OFFSET     80   /* The number of GPT table */	//从这个位置读取分区表数量
#define TABLE_START_SECTOR   2
#define TABLE_MAX_NUM        128
#define TABLE_SIZE           128
#define GPT_PAR_START_OFFSET      32	//开始扇区偏移位置
#define GPT_PAR_END_OFFSET        40
#define PAR_ENTRY_NUM_PER_SECTOR  4	//每个扇区分区项目数量
#define HEADER_SIZE_MASK          0xFFFFFFFF
#define HEADER_SIZE               0x5C
#define HARD_DISK_GUID_OFFSET     56
#define HARD_DISK_GUID_FOR_ESP    0x0020004900460045 
#define HARD_DISK_GUID_FOR_MSP    0x007200630069004D //@note_thinking 是否写错了,应该是 MSR
#define PAR_VALID_OFFSET0         0
#define PAR_VALID_OFFSET1         4
#define PAR_VALID_OFFSET2         8
#define PAR_VALID_OFFSET3         12
/*
一、esp即EFI系统分区

1、全称EFI system partition，简写为ESP。ESP虽然是一个FAT16或FAT32格式的物理分区，但是其分区标识是EF(十六进制) 
而非常规的0E或0C；因此，该分区在 Windows 操作系统下一般是不可见的。支持EFI模式的电脑需要从ESP启动系统，EFI固件
可从ESP加载EFI启动程序和应用程序。
2、ESP是一个独立于操作系统之外的分区，操作系统被引导之后，就不再依赖它。这使得ESP非常适合用来存储那些系统级的
维护性的工具和数据，比如：引导管理程序、驱动程序、系统维护工具、系统备份等，甚至可以在ESP里安装一个特殊的操作系统；
3、ESP也可以看做是一个安全的隐藏的分区，可以把引导管理程序、系统维护工具、系统恢复工具及镜像等放到ESP，可以自己
打造“一键恢复系统”。而且，不仅可以自己进行DIY，还要更方便、更通用；

二、msr分区是保留分区

1、windows不会向msr分区建立文件系统或者写数据，而是为了调整分区结构而保留的分区。在Win8以上系统更新时，会检测msr分区。
msr分区本质上就是写在分区表上面的【未分配空间】，目的是微软不想让别人乱动；
2、msr分区的用途是防止将一块GPT磁盘接到老系统中，被当作未格式化的空硬盘而继续操作（例如重新格式化）导致数据丢失。
GPT磁盘上有了这个分区，当把它接入XP等老系统中，会提示无法识别的磁盘，也无法进一步操作；
*/
#define VERIFY_GPT(ptr) ((!strncmp(&(ptr)[SIGNATURE_OFFSET], "EFI PART", SIGNATURE_LEN)) && \
                         ((LD_DWORD_DISK(&(ptr)[HEADER_SIZE_OFFSET]) & HEADER_SIZE_MASK) == HEADER_SIZE))

#define VERITY_PAR_VALID(ptr) ((LD_DWORD_DISK(&(ptr)[PAR_VALID_OFFSET0]) + \
                                LD_DWORD_DISK(&(ptr)[PAR_VALID_OFFSET1]) + \
                                LD_DWORD_DISK(&(ptr)[PAR_VALID_OFFSET2]) + \
                                LD_DWORD_DISK(&(ptr)[PAR_VALID_OFFSET3])) != 0)

/* ESP MSP */
#define VERITY_AVAILABLE_PAR(ptr) ((LD_QWORD_DISK(&(ptr)[HARD_DISK_GUID_OFFSET]) != HARD_DISK_GUID_FOR_ESP) && \
                                   (LD_QWORD_DISK(&(ptr)[HARD_DISK_GUID_OFFSET]) != HARD_DISK_GUID_FOR_MSP))

/* Command code for disk_ioctrl function */
/* Generic command (Used by FatFs) */
#define DISK_CTRL_SYNC          0   /* Complete pending write process */
#define DISK_GET_SECTOR_COUNT   1   /* Get media size */
#define DISK_GET_SECTOR_SIZE    2   /* Get sector size */
#define DISK_GET_BLOCK_SIZE     3   /* Get erase block size */
#define DISK_CTRL_TRIM          4   /* Inform device that the data on the block of sectors is no longer used */

/* Generic command (Not used by FatFs) */
#define DISK_CTRL_POWER         5   /* Get/Set power status */
#define DISK_CTRL_LOCK          6   /* Lock/Unlock media removal */
#define DISK_CTRL_EJECT         7   /* Eject media */
#define DISK_CTRL_FORMAT        8   /* Create physical format on the media */

/* MMC/SDC specific ioctl command */
#define DISK_MMC_GET_TYPE       10  /* Get card type */
#define DISK_MMC_GET_CSD        11  /* Get CSD */
#define DISK_MMC_GET_CID        12  /* Get CID */
#define DISK_MMC_GET_OCR        13  /* Get OCR */
#define DISK_MMC_GET_SDSTAT     14  /* Get SD status */

/* ATA/CF specific ioctl command */
#define DISK_ATA_GET_REV        20  /* Get F/W revision */
#define DISK_ATA_GET_MODEL      21  /* Get model name */
#define DISK_ATA_GET_SN         22  /* Get serial number */

typedef enum _disk_status_ {//磁盘的状态
    STAT_UNUSED,	//未使用
    STAT_INUSED,	//使用中
    STAT_UNREADY	//未准备,可理解为未格式化
} disk_status_e;

typedef struct _los_disk_ {	//磁盘描述符
    UINT32 disk_id : 8;     /* physics disk number */ 	//标识磁盘ID
    UINT32 disk_status : 2; /* status of disk */		//磁盘的状态 disk_status_e
    UINT32 part_count : 8;  /* current partition count */	//分了多少个区(los_part)
    UINT32 reserved : 14;	//保留，注意 disk_id|disk_status|part_count|reserved 共用一个UINT32				
    struct inode *dev;      /* device */	//磁盘中使用的索引节点, @note_why 这个节点和其首个分区的dev 有何关系 ?
#ifdef LOSCFG_FS_FAT_CACHE	//磁盘缓存，在所有分区中共享
    OsBcache *bcache;       /* cache of the disk, shared in all partitions */
#endif
    UINT32 sector_size;     /* disk sector size */	//扇区大小
    UINT64 sector_start;    /* disk start sector */	//开始扇区
    UINT64 sector_count;    /* disk sector number *///扇区数量	
    UINT8 type;				//flash的类型 例如:EMMC
    CHAR *disk_name;		//设备名称,通过名称创建一个磁盘,los_alloc_diskid_byname
    LOS_DL_LIST head;       /* link head of all the partitions */ //双向链表上挂所有分区(los_part->list)
    struct pthread_mutex disk_mutex;
} los_disk;


typedef struct _los_part_ {//分区描述符
    UINT32 disk_id : 8;      /* physics disk number */	//物理磁盘编号ID
    UINT32 part_id : 8;      /* partition number in the system */ //标识整个系统的分区数量
    UINT32 part_no_disk : 8; /* partition number in the disk */	  //标识所属磁盘的分区数量
    UINT32 part_no_mbr : 5;  /* partition number in the mbr */	  //硬盘主引导记录（即Master Boot Record，一般简称为MBR），主分区数量	
    UINT32 reserved : 3;	////保留，注意 disk_id|part_id|part_no_disk|part_no_mbr|reserved 共用一个UINT32
    UINT8 filesystem_type;   /* filesystem used in the partition */ //文件系统类型
    UINT8 type;				//flash的类型 例如:EMMC
    struct inode *dev;      /* dev devices used in the partition */ //分区中使用的索引节点
    CHAR *part_name;		//区名称
    UINT64 sector_start;     /* //开始扇区编号
                              * offset of a partition to the primary devices
                              * (multi-mbr partitions are seen as same parition)
                              */
    UINT64 sector_count;     /*	//扇区数量
                              * sector numbers of a partition. If there is no addpartition operation,
                              * then all the mbr devices equal to the primary device count.
                              */
    LOS_DL_LIST list;        /* linklist of partition */ //通过它挂到los_disk->head上
} los_part;

struct partition_info {//分区信息
    UINT8 type;	//分区类型,是主分区还是扩展分区
    UINT64 sector_start;//开始扇区位置
    UINT64 sector_count;//扇区总数量
};


struct disk_divide_info {//每个磁盘分区总信息
    UINT64 sector_count;	//磁盘扇区总大小
    UINT32 sector_size;		//扇区大小,一般是512字节
    UINT32 part_count;		//磁盘真实的分区数量 需 < MAX_DIVIDE_PART_PER_DISK + MAX_PRIMARY_PART_PER_DISK
    /*
     * The primary partition place should be reversed and set to 0 in case all the partitions are
     * logical partition (maximum 16 currently). So the maximum part number should be 4 + 16.
     */ //如果所有分区都是逻辑分区（目前最多16个），则主分区位置应颠倒并设置为0。所以最大分区号应该是4+16。
    struct partition_info part[MAX_DIVIDE_PART_PER_DISK + MAX_PRIMARY_PART_PER_DISK];//分区数组,记录每个分区的详细情况,默认20个
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
 * @param  bops      [IN] Type #const struct block_operations *   block driver control sturcture.
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
 * Get the correponding INUSED disk id by diskName.
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

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif
