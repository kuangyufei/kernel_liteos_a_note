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

#include "mtd_partition.h"
#include "stdlib.h"
#include "stdio.h"
#include "pthread.h"
#include "mtd_list.h"
#include "los_config.h"
#include "los_mux.h"

#include "mtd_common.h"

#ifdef LOSCFG_PLATFORM_QEMU_ARM_VIRT_CA7
#include "cfiflash.h"
#endif


#define DRIVER_NAME_ADD_SIZE    3 // 0p0 所以加三个字节
pthread_mutex_t g_mtdPartitionLock = PTHREAD_MUTEX_INITIALIZER;

//通常在NorFlash上会选取jffs及jffs2文件系统
/*
* GCC使用__attribute__关键字来描述函数，变量和数据类型的属性，用于编译器对源代码的优化
* 对外部目标文件的符号引用在目标文件被最终链接成可执行文件时，它们须要被正确决议，如果没有找到该符号的定义，
链接器就会报符号未定义错误，这种被称为强引用（Strong Reference）。

* 与之相对应还有一种弱引用（Weak Reference），在处理弱引用时，如果该符号有定义，则链接器将该符号的引用决议；
如果该符号未被定义，则链接器对于该引用不报错。

* 链接器处理强引用和弱引用的过程几乎一样，只是对于未定义的弱引用，链接器不认为它是一个错误。一般对于未定义的弱引用，
链接器默认其为0，或者是一个特殊的值，以便于程序代码能够识别。
* 在GCC中，我们可以通过使用"__attribute__((weakref("..."))"这个扩展关键字来声明对一个外部函数的引用为弱引用，
参考: https://www.cnblogs.com/pengdonglin137/p/3615345.html
*/
static VOID YaffsLockInit(VOID) __attribute__((weakref("yaffsfs_OSInitialisation")));
static VOID YaffsLockDeinit(VOID) __attribute__((weakref("yaffsfs_OsDestroy")));
static INT32 Jffs2LockInit(VOID) __attribute__((weakref("Jffs2MutexCreate")));//弱引用 Jffs2MutexCreate
static VOID Jffs2LockDeinit(VOID) __attribute__((weakref("Jffs2MutexDelete")));

partition_param *g_nandPartParam = NULL;	//nand flash 分区参数
partition_param *g_spinorPartParam = NULL;	//nor flash 分区参数
mtd_partition *g_spinorPartitionHead = NULL;	//spi nor flash分区头结点,上面将挂所有 spi nor分区结点
mtd_partition *g_nandPartitionHead = NULL;		//nand flash 分区头结点,上面将挂所有 nand分区结点

#define RWE_RW_RW 0755 //文件读/写/执权限,chmod 755

partition_param *GetNandPartParam(VOID)
{
    return g_nandPartParam;
}

partition_param *GetSpinorPartParam(VOID)
{
    return g_spinorPartParam;
}

mtd_partition *GetSpinorPartitionHead(VOID)
{
    return g_spinorPartitionHead;
}

//nand flash 参数初始化,本函数只会被调用一次
static VOID MtdNandParamAssign(partition_param *nandParam, const struct MtdDev *nandMtd)
{
    LOS_ListInit(&g_nandPartitionHead->node_info);//初始化全局链表
    /*
     * If the user do not want to use block mtd or char mtd ,
     * you can change the NANDBLK_NAME or NANDCHR_NAME to NULL.
     */
    nandParam->flash_mtd = (struct MtdDev *)nandMtd;
    nandParam->flash_ops = GetDevNandOps();	//获取块设备操作方法
    nandParam->char_ops = GetMtdCharFops(); //获取字符设备操作方法
    nandParam->blockname = NANDBLK_NAME;	// /dev/nandblk
    nandParam->charname = NANDCHR_NAME;		// /dev/nandchr
    nandParam->partition_head = g_nandPartitionHead;//头分区节点
    nandParam->block_size = nandMtd->eraseSize;//4K
}
//反初始化
static VOID MtdDeinitNandParam(VOID)
{
    if (YaffsLockDeinit != NULL) {
        YaffsLockDeinit();
    }
}
//nand flash 初始化
static partition_param *MtdInitNandParam(partition_param *nandParam)
{
    struct MtdDev *nandMtd = GetMtd("nand");
    if (nandMtd == NULL) {
        return NULL;
    }
    if (nandParam == NULL) {
        if (YaffsLockInit != NULL) {
            YaffsLockInit(); //加锁
        }
        nandParam = (partition_param *)zalloc(sizeof(partition_param));
        if (nandParam == NULL) {
            MtdDeinitNandParam();
            return NULL;
        }
        g_nandPartitionHead = (mtd_partition *)zalloc(sizeof(mtd_partition));
        if (g_nandPartitionHead == NULL) {
            MtdDeinitNandParam();
            free(nandParam);
            return NULL;
        }

        MtdNandParamAssign(nandParam, nandMtd);
    }

    return nandParam;
}
//nor flash 初始化,本函数只会被调用一次
static VOID MtdNorParamAssign(partition_param *spinorParam, const struct MtdDev *spinorMtd)
{
    LOS_ListInit(&g_spinorPartitionHead->node_info);//初始化全局链表,所有spi nor 分区节点都将挂上来
    /*
     * If the user do not want to use block mtd or char mtd ,
     * you can change the SPIBLK_NAME or SPICHR_NAME to NULL.
     *///如果用户不想使用 block mtd 或 char mtd ，您可以将 SPIBLK_NAME 或 SPICHR_NAME 更改为 NULL。
    spinorParam->flash_mtd = (struct MtdDev *)spinorMtd;
#ifndef LOSCFG_PLATFORM_QEMU_ARM_VIRT_CA7 //QEMU开关
    spinorParam->flash_ops = GetDevSpinorOps();//块操作
    spinorParam->char_ops = GetMtdCharFops();//字符操作
    spinorParam->blockname = SPIBLK_NAME;
    spinorParam->charname = SPICHR_NAME;
#else
    spinorParam->flash_ops = GetCfiBlkOps();
    spinorParam->char_ops = NULL;
    spinorParam->blockname = CFI_DRIVER;
    spinorParam->charname = NULL;
#endif
    spinorParam->partition_head = g_spinorPartitionHead;//头分区节点
    spinorParam->block_size = spinorMtd->eraseSize;//4K, 读/写/擦除 的最小单位
}

static VOID MtdDeinitSpinorParam(VOID)
{
    if (Jffs2LockDeinit != NULL) {
        Jffs2LockDeinit();
    }
}
//spi nor flash 参数初始化
static partition_param *MtdInitSpinorParam(partition_param *spinorParam)
{
#ifndef LOSCFG_PLATFORM_QEMU_ARM_VIRT_CA7 //
    struct MtdDev *spinorMtd = GetMtd("spinor");
#else
    struct MtdDev *spinorMtd = GetCfiMtdDev();
#endif
    if (spinorMtd == NULL) {
        return NULL;
    }
    if (spinorParam == NULL) {
        if (Jffs2LockInit != NULL) {
            if (Jffs2LockInit() != 0) { /* create jffs2 lock failed */
                return NULL;
            }
        }
        spinorParam = (partition_param *)zalloc(sizeof(partition_param));
        if (spinorParam == NULL) {
            PRINT_ERR("%s, partition_param malloc failed\n", __FUNCTION__);
            MtdDeinitSpinorParam();
            return NULL;
        }
        g_spinorPartitionHead = (mtd_partition *)zalloc(sizeof(mtd_partition));
        if (g_spinorPartitionHead == NULL) {
            PRINT_ERR("%s, mtd_partition malloc failed\n", __FUNCTION__);
            MtdDeinitSpinorParam();
            free(spinorParam);
            return NULL;
        }

        MtdNorParamAssign(spinorParam, spinorMtd);
    }

    return spinorParam;
}
//根据 flash-type 来初始化分区的参数, Fspar 是不是 flash partition 的意思? 这名字取得有点费解 @note_thinking 
/* According the flash-type to init the param of the partition. */
static INT32 MtdInitFsparParam(const CHAR *type, partition_param **fsparParam)
{
    if (strcmp(type, "nand") == 0) {// nand flash 的处理
        g_nandPartParam = MtdInitNandParam(g_nandPartParam);//初始化全局变量
        *fsparParam = g_nandPartParam;//参数接走全局变量
    } else if (strcmp(type, "spinor") == 0 || strcmp(type, "cfi-flash") == 0) { //nor flash的处理
        g_spinorPartParam = MtdInitSpinorParam(g_spinorPartParam);//初始化全局变量
        *fsparParam = g_spinorPartParam;//参数接走全局变量
    } else {
        return -EINVAL;
    }

    if ((*fsparParam == NULL) || ((VOID *)((*fsparParam)->flash_mtd) == NULL)) {
        return -ENODEV;
    }

    return ENOERR;
}
//根据flash-type 去初始化 分区的参数。
/* According the flash-type to deinit the param of the partition. */
static INT32 MtdDeinitFsparParam(const CHAR *type)
{
    if (strcmp(type, "nand") == 0) {
        MtdDeinitNandParam();//去初始化(析构) Nand flash 参数
        g_nandPartParam = NULL;
    } else if (strcmp(type, "spinor") == 0 || strcmp(type, "cfi-flash") == 0) {
        MtdDeinitSpinorParam();//去初始化(析构) Nor flash 参数, 注 : .h 文件有 spi 和 cfi 的区别介绍
        g_spinorPartParam = NULL;
    } else {
        return -EINVAL;
    }

    return ENOERR;
}
//增加MTD分区参数检查
static INT32 AddParamCheck(UINT32 startAddr,
                           const partition_param *param,
                           UINT32 partitionNum,
                           UINT32 length)
{
    UINT32 startBlk, endBlk;
    mtd_partition *node = NULL;
    if ((param->blockname == NULL) && (param->charname == NULL)) {//块/字符设备名称至少有一个
        return -EINVAL;
    }

    if ((length == 0) || (length < param->block_size) ||
        (((UINT64)(startAddr) + length) > param->flash_mtd->size)) {
        return -EINVAL;
    }

    ALIGN_ASSIGN(length, startAddr, startBlk, endBlk, param->block_size);

    if (startBlk > endBlk) {
        return -EINVAL;
    }
    LOS_DL_LIST_FOR_EACH_ENTRY(node, &param->partition_head->node_info, mtd_partition, node_info) {//遍历分区节点
        if ((node->start_block != 0) && (node->patitionnum == partitionNum)) {
            return -EINVAL;
        }
        if ((startBlk > node->end_block) || (endBlk < node->start_block)) {
            continue;
        }
        return -EINVAL;
    }

    return ENOERR;
}
//注册块设备,此函数之后设备将支持VFS访问
static INT32 BlockDriverRegisterOperate(mtd_partition *newNode,
                                        const partition_param *param,
                                        UINT32 partitionNum)
{
    INT32 ret;
    size_t driverNameSize;

    if (param->blockname != NULL) {
        driverNameSize = strlen(param->blockname) + DRIVER_NAME_ADD_SIZE; // 设备名称长度增加3个字节
        newNode->blockdriver_name = (CHAR *)malloc(driverNameSize);
        if (newNode->blockdriver_name == NULL) {
            return -ENOMEM;
        }

        ret = snprintf_s(newNode->blockdriver_name, driverNameSize,
            driverNameSize - 1, "%s%u", param->blockname, partitionNum);
        if (ret < 0) {
            free(newNode->blockdriver_name);
            newNode->blockdriver_name = NULL;
            return -ENAMETOOLONG;
        }
		//在伪文件系统中注册块驱动程序,对节点数据以块方式访问
        ret = register_blockdriver(newNode->blockdriver_name, param->flash_ops,
            RWE_RW_RW, newNode);
        if (ret) {
            free(newNode->blockdriver_name);
            newNode->blockdriver_name = NULL;
            PRINT_ERR("register blkdev partion error\n");
            return ret;
        }
    } else {
        newNode->blockdriver_name = NULL;
    }
    return ENOERR;
}
//注册字符设备,此函数之后设备将支持VFS访问
static INT32 CharDriverRegisterOperate(mtd_partition *newNode,
                                       const partition_param *param,
                                       UINT32 partitionNum)
{
    INT32 ret;
    size_t driverNameSize;

    if (param->charname != NULL) {
        driverNameSize = strlen(param->charname) + DRIVER_NAME_ADD_SIZE;
        newNode->chardriver_name = (CHAR *)malloc(driverNameSize);
        if (newNode->chardriver_name == NULL) {
            return -ENOMEM;
        }

        ret = snprintf_s(newNode->chardriver_name, driverNameSize,
            driverNameSize - 1, "%s%u", param->charname, partitionNum);
        if (ret < 0) {
            free(newNode->chardriver_name);
            newNode->chardriver_name = NULL;
            return -ENAMETOOLONG;
        }
		//在伪文件系统中注册字符驱动程序,以字符设备的方式访问数据
        ret = register_driver(newNode->chardriver_name, param->char_ops, RWE_RW_RW, newNode);
        if (ret) {
            PRINT_ERR("register chardev partion error\n");
            free(newNode->chardriver_name);
            newNode->chardriver_name = NULL;
            return ret;
        }
    } else {
        newNode->chardriver_name = NULL;
    }
    return ENOERR;
}
//注销块设备驱动
static INT32 BlockDriverUnregister(mtd_partition *node)
{
    INT32 ret;

    if (node->blockdriver_name != NULL) {
        ret = unregister_blockdriver(node->blockdriver_name);
        if (ret == -EBUSY) {
            PRINT_ERR("unregister blkdev partion error:%d\n", ret);
            return ret;
        }
        free(node->blockdriver_name);
        node->blockdriver_name = NULL;
    }
    return ENOERR;
}
//注销字符设备驱动
static INT32 CharDriverUnregister(mtd_partition *node)
{
    INT32 ret;

    if (node->chardriver_name != NULL) {
        ret = unregister_driver(node->chardriver_name);
        if (ret == -EBUSY) {
            PRINT_ERR("unregister chardev partion error:%d\n", ret);
            return ret;
        }
        free(node->chardriver_name);//名称占用的内核空间,所以必须释放.
        node->chardriver_name = NULL;
    }

    return ENOERR;
}

/*
 * Attention: both startAddr and length should be aligned with block size.
 * If not, the actual start address and length won't be what you expected.
 *///注意：startAddr 和length 都应该与block size 对齐。如果不是，实际的起始地址和长度将不是你所期望的
INT32 add_mtd_partition(const CHAR *type, UINT32 startAddr,
                        UINT32 length, UINT32 partitionNum)
{
    INT32 ret;
    mtd_partition *newNode = NULL;
    partition_param *param = NULL;

    if ((partitionNum >= CONFIG_MTD_PATTITION_NUM) || (type == NULL)) {
        return -EINVAL;
    }

    ret = pthread_mutex_lock(&g_mtdPartitionLock);
    if (ret != ENOERR) {
        PRINT_ERR("%s %d, mutex lock failed, error:%d\n", __FUNCTION__, __LINE__, ret);
    }

    ret = MtdInitFsparParam(type, &param);//初始化 flash 分区参数
    if (ret != ENOERR) {
        goto ERROR_OUT;
    }
	//参数检查
    ret = AddParamCheck(startAddr, param, partitionNum, length);
    if (ret != ENOERR) {
        goto ERROR_OUT;
    }

    newNode = (mtd_partition *)zalloc(sizeof(mtd_partition));//分配一个分区节点
    if (newNode == NULL) {
        (VOID)pthread_mutex_unlock(&g_mtdPartitionLock);
        return -ENOMEM;
    }
	//分区对齐
    PAR_ASSIGNMENT(newNode, length, startAddr, partitionNum, param->flash_mtd, param->block_size);
	//注册块设备驱动程序
    ret = BlockDriverRegisterOperate(newNode, param, partitionNum);
    if (ret) {
        goto ERROR_OUT1;
    }
	//注册字符设备驱动程序
    ret = CharDriverRegisterOperate(newNode, param, partitionNum);
    if (ret) {
        goto ERROR_OUT2;
    }

    LOS_ListTailInsert(&param->partition_head->node_info, &newNode->node_info);//挂到全局链表上
    (VOID)LOS_MuxInit(&newNode->lock, NULL);//初始化锁,每个分区节点都有自己的互斥锁

    ret = pthread_mutex_unlock(&g_mtdPartitionLock);
    if (ret != ENOERR) {
        PRINT_ERR("%s %d, mutex unlock failed, error:%d\n", __FUNCTION__, __LINE__, ret);
    }

    return ENOERR;
ERROR_OUT2:
    (VOID)BlockDriverUnregister(newNode);
ERROR_OUT1:
    free(newNode);
ERROR_OUT:
    (VOID)pthread_mutex_unlock(&g_mtdPartitionLock);
    return ret;
}
//检查分区参数
static INT32 DeleteParamCheck(UINT32 partitionNum,
                              const CHAR *type,
                              partition_param **param)
{
    if (strcmp(type, "nand") == 0) {
        *param = g_nandPartParam;
    } else if (strcmp(type, "spinor") == 0 || strcmp(type, "cfi-flash") == 0) {
        *param = g_spinorPartParam;
    } else {
        PRINT_ERR("type error \n");
        return -EINVAL;
    }

    if ((partitionNum >= CONFIG_MTD_PATTITION_NUM) ||
        ((*param) == NULL) || ((*param)->flash_mtd == NULL)) {
        return -EINVAL;
    }
    return ENOERR;
}
//删除分区驱动程序,注意每个分区的文件系统都可以不一样,驱动程序也都不同
static INT32 DeletePartitionUnregister(mtd_partition *node)
{
    INT32 ret;

    ret = BlockDriverUnregister(node);//注销块设备驱动程序
    if (ret == -EBUSY) {
        return ret;
    }

    ret = CharDriverUnregister(node);//注销字符设备驱动程序
    if (ret == -EBUSY) {
        return ret;
    }

    return ENOERR;
}
//获取分区链表节点
static INT32 OsNodeGet(mtd_partition **node, UINT32 partitionNum, const partition_param *param)
{	//遍历链表
    LOS_DL_LIST_FOR_EACH_ENTRY(*node, &param->partition_head->node_info, mtd_partition, node_info) {
        if ((*node)->patitionnum == partitionNum) {//找到分区ID
            break;
        }
    }
    if ((*node == NULL) || ((*node)->patitionnum != partitionNum) ||
        ((*node)->mountpoint_name != NULL)) {
        return -EINVAL;
    }

    return ENOERR;
}
//释放分区链表节点所占内存 sizeof(mtd_partition)
static INT32 OsResourceRelease(mtd_partition *node, const CHAR *type, partition_param *param)
{
    (VOID)LOS_MuxDestroy(&node->lock);
    LOS_ListDelete(&node->node_info);//将自己摘掉
    (VOID)memset_s(node, sizeof(mtd_partition), 0, sizeof(mtd_partition));
    free(node);
    (VOID)FreeMtd(param->flash_mtd);//释放MTD
    if (LOS_ListEmpty(&param->partition_head->node_info)) {//如果是最后一个
        free(param->partition_head);//释放头节点内存
        param->partition_head = NULL;
        free(param);

        if (MtdDeinitFsparParam(type) != ENOERR) {
            return -EINVAL;
        }
    }
    return ENOERR;
}
//删除MTD分区
INT32 delete_mtd_partition(UINT32 partitionNum, const CHAR *type)
{
    INT32 ret;
    mtd_partition *node = NULL;
    partition_param *param = NULL;

    if (type == NULL) {
        return -EINVAL;
    }

    ret = pthread_mutex_lock(&g_mtdPartitionLock);
    if (ret != ENOERR) {
        PRINT_ERR("%s %d, mutex lock failed, error:%d\n", __FUNCTION__, __LINE__, ret);
    }

    ret = DeleteParamCheck(partitionNum, type, &param);//删除操作时的参数检查
    if (ret) {
        PRINT_ERR("delete_mtd_partition param invalid\n");
        (VOID)pthread_mutex_unlock(&g_mtdPartitionLock);
        return ret;
    }

    ret = OsNodeGet(&node, partitionNum, param);
    if (ret) {
        (VOID)pthread_mutex_unlock(&g_mtdPartitionLock);
        return ret;
    }

    ret = DeletePartitionUnregister(node);
    if (ret) {
        PRINT_ERR("DeletePartitionUnregister error:%d\n", ret);
        (VOID)pthread_mutex_unlock(&g_mtdPartitionLock);
        return ret;
    }

    ret = OsResourceRelease(node, type, param);
    if (ret) {
        PRINT_ERR("DeletePartitionUnregister error:%d\n", ret);
        (VOID)pthread_mutex_unlock(&g_mtdPartitionLock);
        return ret;
    }

    ret = pthread_mutex_unlock(&g_mtdPartitionLock);
    if (ret != ENOERR) {
        PRINT_ERR("%s %d, mutex unlock failed, error:%d\n", __FUNCTION__, __LINE__, ret);
    }
    return ENOERR;
}

