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
 * @defgroup filesystem FileSystem
 * @defgroup mtd_partition Multi Partition
 * @ingroup filesystem
 */
#ifndef _MTD_PARTITION_H
#define _MTD_PARTITION_H

#include "sys/types.h"
#include "los_mux.h"
#include "mtd_list.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @brief 
 * @verbatim
 * flash按照内部存储结构不同，分为两种：nor flash和nand flash
 * Nor FLASH使用方便，易于连接，可以在芯片上直接运行代码，稳定性出色，传输速率高，
	在小容量时有很高的性价比，这使其很适合应于嵌入式系统中作为 FLASH ROM。
	Nor Flash架构提供足够的地址线来映射整个存储器范围。
 * NandFLASH强调更高的性能，更低的成本，更小的体积，更长的使用寿命。
	这使Nand FLASH很擅于存储纯资料或数据等，在嵌入式系统中用来支持文件系统。
	缺点包括较慢的读取熟读和I/O映射类型或间接接口
	
 * 在通信方式上Nor Flash 分为两种类型：CFI Flash和 SPI Flash。即采用的通信协议不同.
    SPI Flash(serial peripheral interface)串行外围设备接口,是一种常见的时钟同步串行通信接口。
        有4线（时钟，两个数据线，片选线）或者3线（时钟，两个数据线）通信接口，由于它有两个数据线能实现全双工通信，
        读写速度上较快。拥有独立的数据总线和地址总线，能快速随机读取，允许系统直接从Flash中读取代码执行；
        可以单字节或单字编程，但不能单字节擦除，必须以Sector为单位或对整片执行擦除操作，在对存储器进行重新编程之前需要对Sector
        或整片进行预编程和擦除操作。
    CFI Flash
        英文全称是common flash interface,也就是公共闪存接口，是由存储芯片工业界定义的一种获取闪存芯片物理参数
        和结构参数的操作规程和标准。CFI有许多关于闪存芯片的规定，有利于嵌入式对FLASH的编程。现在的很多NOR FLASH 都支持CFI，
        但并不是所有的都支持。  
        CFI接口，相对于串口的SPI来说，也被称为parallel接口，并行接口；另外，CFI接口是JEDEC定义的，
        所以，有的又成CFI接口为JEDEC接口。所以，可以简单理解为：对于Nor Flash来说，
        CFI接口＝JEDEC接口＝Parallel接口 = 并行接口
        特点在于支持的容量更大，读写速度更快。
        缺点由于拥有独立的数据线和地址总线，会浪费电路电子设计上的更多资源。
        
    SPI Flash : 每次传输一个bit位的数据，传输速度慢，但是价格便宜，任意地址读数据，擦除按扇区进行
    CFI Flash : 每次传输一个字节 ，速度快，任意地址读数据，擦除按扇区进行
    Nand Flash：芯片操作是以“块”为基本单位.NAND闪存的块比较小，一般是8KB，然后每块又分成页，
        页大小一般是512字节.要修改NandFlash芯片中一个字节，必须重写整个数据块，读和写都是按照扇区进行的。

* 存储器技术设备（英语：Memory Technology Device，缩写为 MTD），是Linux系统中设备文件系统的一个类别，
    主要用于闪存的应用，是一种闪存转换层（Flash Translation Layer，FTL）。创造MTD子系统的主要目的是提供
    一个介于闪存硬件驱动程序与高端应用程序之间的抽象层。
    因为具备以下特性，所以 MTD 设备和硬盘相较之下，处理起来要复杂许多：
    1. 具有 eraseblocks 的特微，而不是像硬盘一样使用集群。
    2. eraseblocks (32KiB ~ 128KiB) 跟硬盘的 sector size（512 到 1024 bytes）比起来要大很多。
    3. 操作上主要分作三个动作： 从 eraseblock 读取、写入 eraseblock 、还有就是清除 eraseblock 。
    4. 坏掉的 eraseblocks 无法隐藏，需要软件加以处理。
    5. eraseblocks 的寿命大约会在 104 到 105 的清除动作之后结束。
    像U盘、多媒体记忆卡（MMC）、SD卡、CF卡等其他流行的可移动存储器要和MTD区分开来，虽然它们也叫"flash"，但它们不是使用MTD技术的存储器[1]。

* http://www.linux-mtd.infradead.org/faq/general.html#L_ext2_mtd
* http://www.linux-mtd.infradead.org/
* https://blog.csdn.net/zhejfl/article/details/78544796	
 * @endverbatim
 */

/* MTD设备节点路径定义 */
#define SPIBLK_NAME  "/dev/spinorblk"  /* SPI NOR闪存块设备节点路径 */
#define SPICHR_NAME  "/dev/spinorchr"  /* SPI NOR闪存字符设备节点路径 */

#define NANDBLK_NAME "/dev/nandblk"   /* NAND闪存块设备节点路径 */
#define NANDCHR_NAME "/dev/nandchr"   /* NAND闪存字符设备节点路径 */

/**
 * @brief MTD分区节点结构
 * @details 描述单个MTD分区的属性和操作信息，用于构建分区链表
 */
typedef struct mtd_node {
    UINT32 start_block;              /* 分区起始块号 */
    UINT32 end_block;                /* 分区结束块号 */
    UINT32 patitionnum;              /* 分区编号 (注意：原拼写"patitionnum"保留) */
    CHAR *blockdriver_name;          /* 块设备驱动名称 */
    CHAR *chardriver_name;           /* 字符设备驱动名称 */
    CHAR *mountpoint_name;           /* 挂载点路径 (NULL表示未挂载) */
    VOID *mtd_info;                  /* 分区使用的MTD设备驱动指针 */
    LOS_DL_LIST node_info;           /* 双向链表节点，用于挂载到分区链表 */
    LosMux lock;                     /* 分区操作互斥锁 */
    UINT32 user_num;                 /* 当前使用该分区的用户数量 */
} mtd_partition;

/**
 * @brief 分区参数配置结构
 * @details 包含MTD分区管理所需的设备信息、操作接口和配置参数
 */
typedef struct par_param {
    mtd_partition *partition_head;   /* 分区链表头指针 */
    struct MtdDev *flash_mtd;        /* MTD设备驱动指针 */
    const struct block_operations *flash_ops; /* 块设备操作接口 */
    const struct file_operations_vfs *char_ops; /* 字符设备操作接口 */
    CHAR *blockname;                 /* 块设备名称 */
    CHAR *charname;                  /* 字符设备名称 */
    UINT32 block_size;               /* 块大小 (字节) */
} partition_param;

/* 配置参数 */
#define CONFIG_MTD_PATTITION_NUM 20   /* MTD最大支持分区数量 (注意：原拼写"PATTITION"保留) */

/**
 * @brief 分区地址与块号对齐计算宏
 * @details 自动将长度和起始地址按块大小对齐，并计算起始块和结束块号
 * @param len [in/out] 输入原始长度，输出对齐后的长度
 * @param startAddr [in/out] 输入原始起始地址，输出对齐后的地址
 * @param startBlk [out] 计算得到的起始块号
 * @param endBlk [out] 计算得到的结束块号
 * @param blkSize [in] 块大小 (字节)
 */
#define ALIGN_ASSIGN(len, startAddr, startBlk, endBlk, blkSize) do {    \
    (len) = (((len) + ((blkSize) - 1)) & ~((blkSize) - 1));             \
    (startAddr) = ((startAddr) & ~((blkSize) - 1));                     \
    (startBlk) = (startAddr) / (blkSize);                               \
    (endBlk) = (len) / (blkSize) + ((startBlk) - 1);                    \
} while (0)

/**
 * @brief MTD分区节点初始化宏
 * @details 根据输入参数初始化mtd_partition结构体成员
 * @param node [out] mtd_partition结构体指针
 * @param len [in] 分区长度 (字节)
 * @param startAddr [in] 分区起始地址 (字节)
 * @param num [in] 分区编号
 * @param mtd [in] MTD设备驱动指针
 * @param blkSize [in] 块大小 (字节)
 */
#define PAR_ASSIGNMENT(node, len, startAddr, num, mtd, blkSize) do {    \
    (node)->start_block = (startAddr) / (blkSize);                      \
    (node)->end_block = (len) / (blkSize) + ((node)->start_block - 1);  \
    (node)->patitionnum = (num);                                        \
    (node)->mtd_info = (mtd);                                           \
    (node)->mountpoint_name = NULL;                                     \
} while (0)

/**
 * @brief 获取NAND分区参数
 * @return 指向partition_param结构体的指针，NULL表示获取失败
 */
partition_param *GetNandPartParam(VOID);

/**
 * @brief 获取SPI NOR分区参数
 * @return 指向partition_param结构体的指针，NULL表示获取失败
 */
partition_param *GetSpinorPartParam(VOID);

/**
 * @brief 获取SPI NOR分区链表头
 * @return 指向mtd_partition结构体的指针，NULL表示链表为空
 */
mtd_partition *GetSpinorPartitionHead(VOID);


/**
 * @ingroup mtd_partition
 * @brief Add a partition.
 *
 * @par Description:
 * <ul>
 * <li>This API is used to add a partition according to the passed-in parameters.</li>
 * </ul>
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @param type           [IN] Storage medium type, support "nand" and "spinor" currently.
 * @param startAddr      [IN] Starting address of a partition.
 * @param length         [IN] Partition size.
 * @param partitionNum   [IN] Partition number, less than the value defined by CONFIG_MTD_PATTITION_NUM.
 *
 * @retval #-ENODEV      The driver is not found.
 * @retval #-EINVAL      Invalid parameter.
 * @retval #-ENOMEM      Insufficient memory.
 * @retval #ENOERR       The partition is successfully created.
 *
 * @par Dependency:
 * <ul><li>mtd_partition.h: the header file that contains the API declaration.</li></ul>
 * @see delete_mtd_partition
 */
extern INT32 add_mtd_partition(const CHAR *type, UINT32 startAddr, UINT32 length, UINT32 partitionNum);

/**
 * @ingroup mtd_partition
 * @brief Delete a partition.
 *
 * @par Description:
 * <ul>
 * <li>This API is used to delete a partition according to its partition number and storage medium type.</li>
 * </ul>
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @param partitionNum   [IN] Partition number, less than the value defined by CONFIG_MTD_PATTITION_NUM.
 * @param type           [IN] Storage medium type, support "nand" and "spinor" currently.
 *
 * @retval #-EINVAL    Invalid parameter.
 * @retval #ENOERR     The partition is successfully deleted.
 *
 * @par Dependency:
 * <ul><li>mtd_partition.h: the header file that contains the API declaration.</li></ul>
 * @see add_mtd_partition
 */
extern INT32 delete_mtd_partition(UINT32 partitionNum, const CHAR *type);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _MTD_PARTITION_H */
