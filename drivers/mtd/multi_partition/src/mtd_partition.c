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
#include "fs/driver.h"
#include "mtd/mtd_legacy_lite.h"
// 驱动名称添加的长度（用于分区编号）
#define DRIVER_NAME_ADD_SIZE    3
// MTD分区操作互斥锁，确保线程安全
pthread_mutex_t g_mtdPartitionLock = PTHREAD_MUTEX_INITIALIZER;

// YAFFS文件系统锁初始化（弱引用，实际实现由YAFFS提供）
static VOID YaffsLockInit(VOID) __attribute__((weakref("yaffsfs_OSInitialisation")));
// YAFFS文件系统锁释放（弱引用，实际实现由YAFFS提供）
static VOID YaffsLockDeinit(VOID) __attribute__((weakref("yaffsfs_OsDestroy")));
// JFFS2文件系统锁初始化（弱引用，实际实现由JFFS2提供）
static INT32 Jffs2LockInit(VOID) __attribute__((weakref("Jffs2MutexCreate")));
// JFFS2文件系统锁释放（弱引用，实际实现由JFFS2提供）
static VOID Jffs2LockDeinit(VOID) __attribute__((weakref("Jffs2MutexDelete")));

// NAND闪存分区参数全局指针
partition_param *g_nandPartParam = NULL;
// 自旋闪存分区参数全局指针
partition_param *g_spinorPartParam = NULL;
// 自旋闪存分区链表头指针
mtd_partition *g_spinorPartitionHead = NULL;
// NAND闪存分区链表头指针
mtd_partition *g_nandPartitionHead = NULL;

// 文件权限宏：所有者读写执行，组用户读写执行，其他用户读写执行
#define RWE_RW_RW 0755

/**
 * @brief 获取NAND分区参数
 * @return NAND分区参数指针
 */
partition_param *GetNandPartParam(VOID)
{
    return g_nandPartParam;
}

/**
 * @brief 获取自旋闪存分区参数
 * @return 自旋闪存分区参数指针
 */
partition_param *GetSpinorPartParam(VOID)
{
    return g_spinorPartParam;
}

/**
 * @brief 获取自旋闪存分区链表头
 * @return 自旋闪存分区链表头指针
 */
mtd_partition *GetSpinorPartitionHead(VOID)
{
    return g_spinorPartitionHead;
}


/**
 * @brief 初始化NAND分区参数
 * @param[in,out] nandParam NAND分区参数结构体指针
 * @param[in] nandMtd MTD设备结构体指针
 */
static VOID MtdNandParamAssign(partition_param *nandParam, const struct MtdDev *nandMtd)
{
    LOS_ListInit(&g_nandPartitionHead->node_info);  // 初始化分区链表
    /*
     * 如果用户不想使用块设备或字符设备，
     * 可以将NANDBLK_NAME或NANDCHR_NAME设置为NULL
     */
    nandParam->flash_mtd = (struct MtdDev *)nandMtd;       // MTD设备指针
    nandParam->flash_ops = GetDevNandOps();                // NAND闪存操作接口
    nandParam->char_ops = GetMtdCharFops();                // 字符设备操作接口
    nandParam->blockname = NANDBLK_NAME;                   // 块设备名称
    nandParam->charname = NANDCHR_NAME;                    // 字符设备名称
    nandParam->partition_head = g_nandPartitionHead;       // 分区链表头
    nandParam->block_size = nandMtd->eraseSize;            // 块大小（擦除单元大小）
}

/**
 * @brief 释放NAND分区参数资源
 */
static VOID MtdDeinitNandParam(VOID)
{
    if (YaffsLockDeinit != NULL) {  // 如果YAFFS锁释放函数存在
        YaffsLockDeinit();          // 释放YAFFS锁
    }
}

/**
 * @brief 初始化NAND分区参数结构体
 * @param[in] nandParam 已存在的NAND分区参数（可为NULL）
 * @return 初始化后的NAND分区参数指针；NULL 失败
 */
static partition_param *MtdInitNandParam(partition_param *nandParam)
{
    struct MtdDev *nandMtd = GetMtd("nand");  // 获取NAND类型MTD设备
    if (nandMtd == NULL) {                     // 检查设备是否存在
        return NULL;
    }
    if (nandParam == NULL) {                   // 如果参数结构体未分配
        if (YaffsLockInit != NULL) {           // 如果YAFFS锁初始化函数存在
            YaffsLockInit();                   // 初始化YAFFS锁
        }
        nandParam = (partition_param *)zalloc(sizeof(partition_param));  // 分配分区参数内存
        if (nandParam == NULL) {               // 检查内存分配
            MtdDeinitNandParam();              // 释放已初始化资源
            return NULL;
        }
        g_nandPartitionHead = (mtd_partition *)zalloc(sizeof(mtd_partition));  // 分配分区链表头
        if (g_nandPartitionHead == NULL) {     // 检查链表头分配
            MtdDeinitNandParam();              // 释放已初始化资源
            free(nandParam);                   // 释放分区参数
            return NULL;
        }

        MtdNandParamAssign(nandParam, nandMtd);  // 填充NAND分区参数
    }

    return nandParam;
}

/**
 * @brief 初始化自旋闪存分区参数
 * @param[in,out] spinorParam 自旋闪存分区参数结构体指针
 * @param[in] spinorMtd MTD设备结构体指针
 */
static VOID MtdNorParamAssign(partition_param *spinorParam, const struct MtdDev *spinorMtd)
{
    LOS_ListInit(&g_spinorPartitionHead->node_info);  // 初始化分区链表
    /*
     * 如果用户不想使用块设备或字符设备，
     * 可以将SPIBLK_NAME或SPICHR_NAME设置为NULL
     */
    spinorParam->flash_mtd = (struct MtdDev *)spinorMtd;     // MTD设备指针
    spinorParam->flash_ops = GetDevSpinorOps();              // 自旋闪存操作接口
    spinorParam->char_ops = GetMtdCharFops();                // 字符设备操作接口
    spinorParam->blockname = SPIBLK_NAME;                    // 块设备名称
    spinorParam->charname = SPICHR_NAME;                     // 字符设备名称
    spinorParam->partition_head = g_spinorPartitionHead;     // 分区链表头
    spinorParam->block_size = spinorMtd->eraseSize;          // 块大小（擦除单元大小）
}

/**
 * @brief 释放自旋闪存分区参数资源
 */
static VOID MtdDeinitSpinorParam(VOID)
{
    if (Jffs2LockDeinit != NULL) {  // 如果JFFS2锁释放函数存在
        Jffs2LockDeinit();          // 释放JFFS2锁
    }
}

/**
 * @brief 初始化自旋闪存分区参数结构体
 * @param[in] spinorParam 已存在的自旋闪存分区参数（可为NULL）
 * @return 初始化后的自旋闪存分区参数指针；NULL 失败
 */
static partition_param *MtdInitSpinorParam(partition_param *spinorParam)
{
    struct MtdDev *spinorMtd = GetMtd("spinor");  // 获取spinor类型MTD设备
    if (spinorMtd == NULL) {                       // 检查设备是否存在
        return NULL;
    }
    if (spinorParam == NULL) {                     // 如果参数结构体未分配
        if (Jffs2LockInit != NULL) {               // 如果JFFS2锁初始化函数存在
            if (Jffs2LockInit() != 0) { /* JFFS2锁创建失败 */
                return NULL;
            }
        }
        spinorParam = (partition_param *)zalloc(sizeof(partition_param));  // 分配分区参数内存
        if (spinorParam == NULL) {                 // 检查内存分配
            PRINT_ERR("%s, partition_param malloc failed\n", __FUNCTION__);
            MtdDeinitSpinorParam();                // 释放已初始化资源
            return NULL;
        }
        g_spinorPartitionHead = (mtd_partition *)zalloc(sizeof(mtd_partition));  // 分配分区链表头
        if (g_spinorPartitionHead == NULL) {       // 检查链表头分配
            PRINT_ERR("%s, mtd_partition malloc failed\n", __FUNCTION__);
            MtdDeinitSpinorParam();                // 释放已初始化资源
            free(spinorParam);                     // 释放分区参数
            return NULL;
        }

        MtdNorParamAssign(spinorParam, spinorMtd);  // 填充自旋闪存分区参数
    }

    return spinorParam;
}

/**
 * @brief 根据闪存类型初始化分区参数
 * @param[in] type 闪存类型（"nand"/"spinor"/"cfi-flash"）
 * @param[out] fsparParam 输出分区参数指针
 * @return ENOERR 成功；其他 失败
 */
static INT32 MtdInitFsparParam(const CHAR *type, partition_param **fsparParam)
{
    if (strcmp(type, "nand") == 0) {                      // NAND闪存类型
        g_nandPartParam = MtdInitNandParam(g_nandPartParam);
        *fsparParam = g_nandPartParam;
    } else if (strcmp(type, "spinor") == 0 || strcmp(type, "cfi-flash") == 0) {  // 自旋闪存类型
        g_spinorPartParam = MtdInitSpinorParam(g_spinorPartParam);
        *fsparParam = g_spinorPartParam;
    } else {
        return -EINVAL;  // 不支持的闪存类型
    }

    if ((*fsparParam == NULL) || ((VOID *)((*fsparParam)->flash_mtd) == NULL)) {
        return -ENODEV;  // 分区参数或MTD设备无效
    }

    return ENOERR;
}

/**
 * @brief 根据闪存类型释放分区参数
 * @param[in] type 闪存类型（"nand"/"spinor"/"cfi-flash"）
 * @return ENOERR 成功；-EINVAL 不支持的类型
 */
static INT32 MtdDeinitFsparParam(const CHAR *type)
{
    if (strcmp(type, "nand") == 0) {          // NAND闪存类型
        MtdDeinitNandParam();
        g_nandPartParam = NULL;
    } else if (strcmp(type, "spinor") == 0 || strcmp(type, "cfi-flash") == 0) {  // 自旋闪存类型
        MtdDeinitSpinorParam();
        g_spinorPartParam = NULL;
    } else {
        return -EINVAL;  // 不支持的闪存类型
    }

    return ENOERR;
}

/**
 * @brief 添加分区前的参数检查
 * @param[in] startAddr 分区起始地址
 * @param[in] param 分区参数
 * @param[in] partitionNum 分区编号
 * @param[in] length 分区长度
 * @return ENOERR 成功；-EINVAL 参数无效
 */
static INT32 AddParamCheck(UINT32 startAddr,
                           const partition_param *param,
                           UINT32 partitionNum,
                           UINT32 length)
{
    UINT32 startBlk, endBlk;  // 块起始地址和结束地址
    mtd_partition *node = NULL;  // 分区链表节点
    if ((param->blockname == NULL) && (param->charname == NULL)) {  // 未指定设备名称
        return -EINVAL;
    }

    if ((length == 0) || (length < param->block_size) ||  // 长度无效（0或小于块大小）
        (((UINT64)(startAddr) + length) > param->flash_mtd->size)) {  // 超出设备容量
        return -EINVAL;
    }

    ALIGN_ASSIGN(length, startAddr, startBlk, endBlk, param->block_size);  // 按块大小对齐地址和长度

    if (startBlk > endBlk) {  // 起始块大于结束块（无效范围）
        return -EINVAL;
    }
    // 检查分区是否重叠或编号重复
    LOS_DL_LIST_FOR_EACH_ENTRY(node, &param->partition_head->node_info, mtd_partition, node_info) {
        if ((node->start_block != 0) && (node->patitionnum == partitionNum)) {  // 分区编号已存在
            return -EINVAL;
        }
        if ((startBlk > node->end_block) || (endBlk < node->start_block)) {  // 分区不重叠
            continue;
        }
        return -EINVAL;  // 分区重叠
    }

    return ENOERR;
}

/**
 * @brief 注册块设备驱动
 * @param[in,out] newNode 新分区节点
 * @param[in] param 分区参数
 * @param[in] partitionNum 分区编号
 * @return ENOERR 成功；其他 失败
 */
static INT32 BlockDriverRegisterOperate(mtd_partition *newNode,
                                        const partition_param *param,
                                        UINT32 partitionNum)
{
    INT32 ret;
    size_t driverNameSize;  // 驱动名称长度

    if (param->blockname != NULL) {  // 如果块设备名称不为空
        driverNameSize = strlen(param->blockname) + DRIVER_NAME_ADD_SIZE;  // 计算驱动名称长度
        newNode->blockdriver_name = (CHAR *)malloc(driverNameSize);  // 分配驱动名称内存
        if (newNode->blockdriver_name == NULL) {
            return -ENOMEM;  // 内存分配失败
        }

        // 格式化驱动名称（设备名+分区编号）
        ret = snprintf_s(newNode->blockdriver_name, driverNameSize,
            driverNameSize - 1, "%s%u", param->blockname, partitionNum);
        if (ret < 0) {  // 格式化失败
            free(newNode->blockdriver_name);
            newNode->blockdriver_name = NULL;
            return -ENAMETOOLONG;  // 名称过长
        }

        // 注册块设备驱动
        ret = register_blockdriver(newNode->blockdriver_name, param->flash_ops,
            RWE_RW_RW, newNode);
        if (ret) {  // 注册失败
            free(newNode->blockdriver_name);
            newNode->blockdriver_name = NULL;
            PRINT_ERR("register blkdev partition error\n");
            return ret;
        }
    } else {
        newNode->blockdriver_name = NULL;  // 不注册块设备
    }
    return ENOERR;
}

/**
 * @brief 注册字符设备驱动
 * @param[in,out] newNode 新分区节点
 * @param[in] param 分区参数
 * @param[in] partitionNum 分区编号
 * @return ENOERR 成功；其他 失败
 */
static INT32 CharDriverRegisterOperate(mtd_partition *newNode,
                                       const partition_param *param,
                                       UINT32 partitionNum)
{
    INT32 ret;
    size_t driverNameSize;  // 驱动名称长度

    if (param->charname != NULL) {  // 如果字符设备名称不为空
        driverNameSize = strlen(param->charname) + DRIVER_NAME_ADD_SIZE;  // 计算驱动名称长度
        newNode->chardriver_name = (CHAR *)malloc(driverNameSize);  // 分配驱动名称内存
        if (newNode->chardriver_name == NULL) {
            return -ENOMEM;  // 内存分配失败
        }

        // 格式化驱动名称（设备名+分区编号）
        ret = snprintf_s(newNode->chardriver_name, driverNameSize,
            driverNameSize - 1, "%s%u", param->charname, partitionNum);
        if (ret < 0) {  // 格式化失败
            free(newNode->chardriver_name);
            newNode->chardriver_name = NULL;
            return -ENAMETOOLONG;  // 名称过长
        }

        // 注册字符设备驱动
        ret = register_driver(newNode->chardriver_name, param->char_ops, RWE_RW_RW, newNode);
        if (ret) {  // 注册失败
            PRINT_ERR("register chardev partition error\n");
            free(newNode->chardriver_name);
            newNode->chardriver_name = NULL;
            return ret;
        }
    } else {
        newNode->chardriver_name = NULL;  // 不注册字符设备
    }
    return ENOERR;
}

/**
 * @brief 注销块设备驱动
 * @param[in] node 分区节点
 * @return ENOERR 成功；-EBUSY 设备忙
 */
static INT32 BlockDriverUnregister(mtd_partition *node)
{
    INT32 ret;

    if (node->blockdriver_name != NULL) {  // 如果块设备名称存在
        ret = unregister_blockdriver(node->blockdriver_name);  // 注销块设备
        if (ret == -EBUSY) {  // 设备忙，无法注销
            PRINT_ERR("unregister blkdev partition error:%d\n", ret);
            return ret;
        }
        free(node->blockdriver_name);  // 释放驱动名称内存
        node->blockdriver_name = NULL;
    }
    return ENOERR;
}

/**
 * @brief 注销字符设备驱动
 * @param[in] node 分区节点
 * @return ENOERR 成功；-EBUSY 设备忙
 */
static INT32 CharDriverUnregister(mtd_partition *node)
{
    INT32 ret;

    if (node->chardriver_name != NULL) {  // 如果字符设备名称存在
        ret = unregister_driver(node->chardriver_name);  // 注销字符设备
        if (ret == -EBUSY) {  // 设备忙，无法注销
            PRINT_ERR("unregister chardev partition error:%d\n", ret);
            return ret;
        }
        free(node->chardriver_name);  // 释放驱动名称内存
        node->chardriver_name = NULL;
    }

    return ENOERR;
}

/**
 * @brief 添加MTD分区
 * @param[in] type 闪存类型（"nand"/"spinor"/"cfi-flash"）
 * @param[in] startAddr 分区起始地址（必须块对齐）
 * @param[in] length 分区长度（必须块对齐）
 * @param[in] partitionNum 分区编号
 * @return ENOERR 成功；其他 失败
 * @note 注意：startAddr和length必须按块大小对齐，否则实际地址和长度将与预期不符
 */
INT32 add_mtd_partition(const CHAR *type, UINT32 startAddr,
                        UINT32 length, UINT32 partitionNum)
{
    INT32 ret;
    mtd_partition *newNode = NULL;  // 新分区节点
    partition_param *param = NULL;  // 分区参数

    if ((partitionNum >= CONFIG_MTD_PATTITION_NUM) || (type == NULL)) {  // 参数检查
        return -EINVAL;
    }

    ret = pthread_mutex_lock(&g_mtdPartitionLock);  // 加锁保护分区操作
    if (ret != ENOERR) {
        PRINT_ERR("%s %d, mutex lock failed, error:%d\n", __FUNCTION__, __LINE__, ret);
    }

    ret = MtdInitFsparParam(type, &param);  // 初始化分区参数
    if (ret != ENOERR) {
        goto ERROR_OUT;  // 初始化失败，跳转至错误处理
    }

    ret = AddParamCheck(startAddr, param, partitionNum, length);  // 检查分区参数合法性
    if (ret != ENOERR) {
        goto ERROR_OUT;  // 参数非法，跳转至错误处理
    }

    newNode = (mtd_partition *)zalloc(sizeof(mtd_partition));  // 分配新分区节点
    if (newNode == NULL) {
        (VOID)pthread_mutex_unlock(&g_mtdPartitionLock);  // 解锁
        return -ENOMEM;  // 内存分配失败
    }

    // 填充分区节点信息（宏定义，包含地址、长度、编号等）
    PAR_ASSIGNMENT(newNode, length, startAddr, partitionNum, param->flash_mtd, param->block_size);

    ret = BlockDriverRegisterOperate(newNode, param, partitionNum);  // 注册块设备
    if (ret) {
        goto ERROR_OUT1;  // 块设备注册失败
    }

    ret = CharDriverRegisterOperate(newNode, param, partitionNum);  // 注册字符设备
    if (ret) {
        goto ERROR_OUT2;  // 字符设备注册失败
    }

    LOS_ListTailInsert(&param->partition_head->node_info, &newNode->node_info);  // 添加分区到链表
    (VOID)LOS_MuxInit(&newNode->lock, NULL);  // 初始化分区锁

    ret = pthread_mutex_unlock(&g_mtdPartitionLock);  // 解锁
    if (ret != ENOERR) {
        PRINT_ERR("%s %d, mutex unlock failed, error:%d\n", __FUNCTION__, __LINE__, ret);
    }

    return ENOERR;
ERROR_OUT2:
    (VOID)BlockDriverUnregister(newNode);  // 回滚块设备注册
ERROR_OUT1:
    free(newNode);  // 释放分区节点
ERROR_OUT:
    (VOID)pthread_mutex_unlock(&g_mtdPartitionLock);  // 解锁
    return ret;
}

/**
 * @brief 删除分区前的参数检查
 * @param[in] partitionNum 分区编号
 * @param[in] type 闪存类型
 * @param[out] param 输出分区参数指针
 * @return ENOERR 成功；-EINVAL 参数无效
 */
static INT32 DeleteParamCheck(UINT32 partitionNum,
                              const CHAR *type,
                              partition_param **param)
{
    if (strcmp(type, "nand") == 0) {  // NAND闪存类型
        *param = g_nandPartParam;
    } else if (strcmp(type, "spinor") == 0 || strcmp(type, "cfi-flash") == 0) {  // 自旋闪存类型
        *param = g_spinorPartParam;
    } else {
        PRINT_ERR("type error \n");
        return -EINVAL;  // 不支持的类型
    }

    if ((partitionNum >= CONFIG_MTD_PATTITION_NUM) ||  // 分区编号超出范围
        ((*param) == NULL) || ((*param)->flash_mtd == NULL)) {  // 分区参数或MTD设备无效
        return -EINVAL;
    }
    return ENOERR;
}

/**
 * @brief 注销分区关联的设备驱动
 * @param[in] node 分区节点
 * @return ENOERR 成功；-EBUSY 设备忙
 */
static INT32 DeletePartitionUnregister(mtd_partition *node)
{
    INT32 ret;

    ret = BlockDriverUnregister(node);  // 注销块设备
    if (ret == -EBUSY) {
        return ret;
    }

    ret = CharDriverUnregister(node);  // 注销字符设备
    if (ret == -EBUSY) {
        return ret;
    }

    return ENOERR;
}

/**
 * @brief 根据分区编号查找分区节点
 * @param[out] node 输出分区节点指针
 * @param[in] partitionNum 分区编号
 * @param[in] param 分区参数
 * @return ENOERR 成功；-EINVAL 未找到或已挂载
 */
static INT32 OsNodeGet(mtd_partition **node, UINT32 partitionNum, const partition_param *param)
{
    // 遍历分区链表查找指定编号的分区
    LOS_DL_LIST_FOR_EACH_ENTRY(*node, &param->partition_head->node_info, mtd_partition, node_info) {
        if ((*node)->patitionnum == partitionNum) {
            break;
        }
    }
    // 检查节点是否有效且未挂载
    if ((*node == NULL) || ((*node)->patitionnum != partitionNum) ||
        ((*node)->mountpoint_name != NULL)) {
        return -EINVAL;
    }

    return ENOERR;
}

/**
 * @brief 释放分区节点及相关资源
 * @param[in] node 分区节点
 * @param[in] type 闪存类型
 * @param[in] param 分区参数
 * @return ENOERR 成功；-EINVAL 失败
 */
static INT32 OsResourceRelease(mtd_partition *node, const CHAR *type, partition_param *param)
{
    (VOID)LOS_MuxDestroy(&node->lock);  // 销毁分区锁
    LOS_ListDelete(&node->node_info);  // 从链表中删除节点
    (VOID)memset_s(node, sizeof(mtd_partition), 0, sizeof(mtd_partition));  // 清空节点内存
    free(node);  // 释放节点
    (VOID)FreeMtd(param->flash_mtd);  // 释放MTD设备
    if (LOS_ListEmpty(&param->partition_head->node_info)) {  // 如果分区链表为空
        free(param->partition_head);  // 释放链表头
        param->partition_head = NULL;
        free(param);  // 释放分区参数

        if (MtdDeinitFsparParam(type) != ENOERR) {  // 释放分区参数资源
            return -EINVAL;
        }
    }
    return ENOERR;
}

/**
 * @brief 删除MTD分区
 * @param[in] partitionNum 分区编号
 * @param[in] type 闪存类型（"nand"/"spinor"/"cfi-flash"）
 * @return ENOERR 成功；其他 失败
 */
INT32 delete_mtd_partition(UINT32 partitionNum, const CHAR *type)
{
    INT32 ret;
    mtd_partition *node = NULL;  // 分区节点
    partition_param *param = NULL;  // 分区参数

    if (type == NULL) {  // 参数检查
        return -EINVAL;
    }

    ret = pthread_mutex_lock(&g_mtdPartitionLock);  // 加锁保护分区操作
    if (ret != ENOERR) {
        PRINT_ERR("%s %d, mutex lock failed, error:%d\n", __FUNCTION__, __LINE__, ret);
    }

    ret = DeleteParamCheck(partitionNum, type, &param);  // 检查删除参数
    if (ret) {
        PRINT_ERR("delete_mtd_partition param invalid\n");
        (VOID)pthread_mutex_unlock(&g_mtdPartitionLock);  // 解锁
        return ret;
    }

    ret = OsNodeGet(&node, partitionNum, param);  // 查找分区节点
    if (ret) {
        (VOID)pthread_mutex_unlock(&g_mtdPartitionLock);  // 解锁
        return ret;
    }

    ret = DeletePartitionUnregister(node);  // 注销分区设备驱动
    if (ret) {
        PRINT_ERR("DeletePartitionUnregister error:%d\n", ret);
        (VOID)pthread_mutex_unlock(&g_mtdPartitionLock);  // 解锁
        return ret;
    }

    ret = OsResourceRelease(node, type, param);  // 释放分区资源
    if (ret) {
        PRINT_ERR("DeletePartitionUnregister error:%d\n", ret);
        (VOID)pthread_mutex_unlock(&g_mtdPartitionLock);  // 解锁
        return ret;
    }

    ret = pthread_mutex_unlock(&g_mtdPartitionLock);  // 解锁
    if (ret != ENOERR) {
        PRINT_ERR("%s %d, mutex unlock failed, error:%d\n", __FUNCTION__, __LINE__, ret);
    }
    return ENOERR;
}
