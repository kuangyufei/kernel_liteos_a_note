/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
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

#define SPIBLK_NAME  "/dev/spinorblk"	//nor flash block 名称
#define SPICHR_NAME  "/dev/spinorchr"	//nor flash char 名称

#define NANDBLK_NAME "/dev/nandblk" //nand flash block 名称
#define NANDCHR_NAME "/dev/nandchr"	//nand flash char 名称
/***************************************************************
https://blog.csdn.net/lwj103862095/article/details/21545791

MTD，Memory Technology Device即内存技术设备，在Linux内核中，引入MTD层为
NOR FLASH和NAND FLASH设备提供统一接口。MTD将文件系统与底层FLASH存储器进行了隔离。

字符设备和块设备的区别在于前者只能被顺序读写，后者可以随机访问；同时，两者读写数据的基本单元不同。
字符设备，以字节为基本单位，在Linux中，字符设备实现的比较简单，不需要缓冲区即可直接读写，

内核例程和用户态API一一对应，用户层的Read函数直接对应了内核中的Read例程，这种映射关系由字符设备的
file_operations维护。

块设备，则以块为单位接受输入和返回输出。对这种设备的读写是按块进行的，其接口相对于字符设备复杂，
read、write API没有直接到块设备层，而是直接到文件系统层，然后再由文件系统层发起读写请求。 
同时，由于块设备的IO性能与CPU相比很差，因此，块设备的数据流往往会引入文件系统的Cache机制。

MTD设备既非块设备也不是字符设备，但可以同时提供字符设备和块设备接口来操作它。
MTD设备通常可分为四层 
这四层从上到下依次是：设备节点、MTD设备层、MTD原始设备层和硬件驱动层。
***************************************************************/
typedef struct mtd_node {//通过mknod在/dev子目录下建立MTD块设备节点（主设备号为31）和MTD字符设备节点（主设备号为90） 
    UINT32 start_block;
    UINT32 end_block;
    UINT32 patitionnum;
    CHAR *blockdriver_name;
    CHAR *chardriver_name;
    CHAR *mountpoint_name;
    VOID *mtd_info; /* Driver used by a partition */
    LOS_DL_LIST node_info;
    LosMux lock;
    UINT32 user_num;
} mtd_partition;

typedef struct par_param {
    mtd_partition *partition_head;
    struct MtdDev *flash_mtd;	
    const struct block_operations *flash_ops;//块设备的操作方法
    const struct file_operations_vfs *char_ops;//字符设备的操作方法
    CHAR *blockname;	//块设备名称
    CHAR *charname;		//字符设备名称
    UINT32 block_size;
} partition_param;

#define CONFIG_MTD_PATTITION_NUM 20

#define ALIGN_ASSIGN(len, startAddr, startBlk, endBlk, blkSize) do {    \
    (len) = (((len) + ((blkSize) - 1)) & ~((blkSize) - 1));             \
    (startAddr) = ((startAddr) & ~((blkSize) - 1));                     \
    (startBlk) = (startAddr) / (blkSize);                               \
    (endBlk) = (len) / (blkSize) + ((startBlk) - 1);                    \
} while (0)

#define PAR_ASSIGNMENT(node, len, startAddr, num, mtd, blkSize) do {    \
    (node)->start_block = (startAddr) / (blkSize);                      \
    (node)->end_block = (len) / (blkSize) + ((node)->start_block - 1);  \
    (node)->patitionnum = (num);                                        \
    (node)->mtd_info = (mtd);                                           \
    (node)->mountpoint_name = NULL;                                     \
} while (0)

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
