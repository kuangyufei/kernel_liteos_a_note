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
#include "los_partition_utils.h"
#if defined(LOSCFG_STORAGE_SPINOR)
#include "mtd_partition.h"
#endif

/**
 * @brief 解析分区位置信息
 * @details 从输入字符串中提取分区的位置信息（地址或大小），支持十进制数字加单位（M/K）或十六进制格式
 * @param p [IN] 输入的字符串指针，格式如"name=value"或"name=0xvalue"
 * @param partInfoName [IN] 分区信息名称（如"addr"、"size"）
 * @param partInfo [OUT] 解析出的分区位置信息（字节为单位）
 * @return INT32 - 操作结果
 *         LOS_OK：解析成功
 *         LOS_NOK：解析失败（格式错误或不支持的单位）
 */
STATIC INT32 MatchPartPos(CHAR *p, const CHAR *partInfoName, INT32 *partInfo)
{
    UINT32 offset;  // 用于存储数字部分的长度
    CHAR *value = NULL;  // 指向等号后的数值部分字符串

    // 检查输入字符串是否以目标分区信息名称开头
    if (strncmp(p, partInfoName, strlen(partInfoName)) == 0) {
        value = p + strlen(partInfoName);  // 定位到数值部分
        offset = strspn(value, DEC_NUMBER_STRING);  // 计算十进制数字的长度
        // 判断单位是否为MB
        if (strcmp(p + strlen(p) - 1, "M") == 0) {
            // 验证数字部分格式并转换为整数
            if ((offset < strlen(value) - 1) || (sscanf_s(value, "%d", partInfo) <= 0)) {
                goto ERROUT;  // 格式错误跳转到错误处理
            }
            *partInfo = *partInfo * BYTES_PER_MBYTE;  // 转换为字节单位
        } else if (strcmp(p + strlen(p) - 1, "K") == 0) {  // 判断单位是否为KB
            // 验证数字部分格式并转换为整数
            if ((offset < (strlen(value) - 1)) || (sscanf_s(value, "%d", partInfo) <= 0)) {
                goto ERROUT;  // 格式错误跳转到错误处理
            }
            *partInfo = *partInfo * BYTES_PER_KBYTE;  // 转换为字节单位
        } else if (sscanf_s(value, "0x%x", partInfo) > 0) {  // 尝试解析十六进制格式
            value += strlen("0x");  // 跳过"0x"前缀
            // 验证剩余部分是否全为十六进制字符
            if (strspn(value, HEX_NUMBER_STRING) < strlen(value)) {
                goto ERROUT;  // 格式错误跳转到错误处理
            }
        } else {  // 不支持的格式
            goto ERROUT;  // 跳转到错误处理
        }
    }

    return LOS_OK;  // 解析成功

ERROUT:  // 错误处理标签
    PRINT_ERR("Invalid format: %s\n", p + strlen(partInfoName));  // 打印错误信息
    return LOS_NOK;  // 返回失败
}

/**
 * @brief 匹配分区信息
 * @details 从输入字符串中提取分区的存储类型、文件系统类型、起始地址和大小等信息，并填充到分区信息结构体
 * @param p [IN] 输入的字符串指针，格式如"key=value"
 * @param partInfo [INOUT] 分区信息结构体指针，用于存储解析结果
 * @return INT32 - 操作结果
 *         LOS_OK：匹配成功
 *         LOS_NOK：匹配失败或内存分配失败
 */
STATIC INT32 MatchPartInfo(CHAR *p, struct PartitionInfo *partInfo)
{
    const CHAR *storageTypeArgName = partInfo->storageTypeArgName;  // 存储类型参数名
    const CHAR *fsTypeArgName      = partInfo->fsTypeArgName;       // 文件系统类型参数名
    const CHAR *addrArgName        = partInfo->addrArgName;         // 起始地址参数名
    const CHAR *partSizeArgName    = partInfo->partSizeArgName;     // 分区大小参数名

    // 匹配存储类型参数并分配内存存储值
    if ((partInfo->storageType == NULL) && (strncmp(p, storageTypeArgName, strlen(storageTypeArgName)) == 0)) {
        partInfo->storageType = strdup(p + strlen(storageTypeArgName));  // 复制字符串
        if (partInfo->storageType == NULL) {  // 内存分配失败检查
            return LOS_NOK;  // 返回失败
        }
        return LOS_OK;  // 匹配成功
    }

    // 匹配文件系统类型参数并分配内存存储值
    if ((partInfo->fsType == NULL) && (strncmp(p, fsTypeArgName, strlen(fsTypeArgName)) == 0)) {
        partInfo->fsType = strdup(p + strlen(fsTypeArgName));  // 复制字符串
        if (partInfo->fsType == NULL) {  // 内存分配失败检查
            return LOS_NOK;  // 返回失败
        }
        return LOS_OK;  // 匹配成功
    }

    // 解析起始地址（仅当尚未设置时）
    if (partInfo->startAddr < 0) {
        if (MatchPartPos(p, addrArgName, &partInfo->startAddr) != LOS_OK) {
            return LOS_NOK;  // 解析失败返回
        } else if (partInfo->startAddr >= 0) {
            return LOS_OK;  // 解析成功返回
        }
    }

    // 解析分区大小（仅当尚未设置时）
    if (partInfo->partSize < 0) {
        if (MatchPartPos(p, partSizeArgName, &partInfo->partSize) != LOS_OK) {
            return LOS_NOK;  // 解析失败返回
        }
    }

    return LOS_OK;  // 匹配成功或无需处理
}

/**
 * @brief 获取分区启动参数
 * @details 从启动命令行中读取指定名称的分区参数，并返回参数值字符串
 * @param argName [IN] 要获取的参数名称
 * @param args [OUT] 指向存储参数值字符串的指针
 * @return INT32 - 操作结果
 *         LOS_OK：获取成功
 *         LOS_NOK：内存分配失败或未找到参数
 */
STATIC INT32 GetPartitionBootArgs(const CHAR *argName, CHAR **args)
{
    INT32 i;  // 循环索引
    INT32 len = 0;  // 字符串长度
    CHAR *cmdLine = NULL;  // 存储命令行内容
    INT32 cmdLineLen;  // 命令行长度
    CHAR *tmp = NULL;  // 临时指针

    cmdLine = (CHAR *)malloc(COMMAND_LINE_SIZE);  // 分配命令行缓冲区
    if (cmdLine == NULL) {  // 内存分配失败检查
        PRINT_ERR("Malloc cmdLine space failed!\n");  // 打印错误信息
        return LOS_NOK;  // 返回失败
    }

#if defined(LOSCFG_STORAGE_SPINOR)  // 条件编译：如果配置了SPINOR存储
    struct MtdDev *mtd = GetMtd(FLASH_TYPE);  // 获取MTD设备
    if (mtd == NULL) {  // 设备获取失败检查
        PRINT_ERR("Get spinor mtd failed!\n");  // 打印错误信息
        goto ERROUT;  // 跳转到错误处理
    }
    // 从MTD设备读取命令行
    cmdLineLen = mtd->read(mtd, COMMAND_LINE_ADDR, COMMAND_LINE_SIZE, cmdLine);
    if ((cmdLineLen != COMMAND_LINE_SIZE)) {  // 读取长度检查
        PRINT_ERR("Read spinor command line failed!\n");  // 打印错误信息
        goto ERROUT;  // 跳转到错误处理
    }
#else  // 未配置SPINOR存储
    cmdLineLen = 0;  // 命令行长度设为0
#endif

    // 遍历命令行查找目标参数
    for (i = 0; i < cmdLineLen; i += len + 1) {
        len = strlen(cmdLine + i);  // 获取当前字符串长度
        tmp = strstr(cmdLine + i, argName);  // 查找参数名
        if (tmp != NULL) {  // 找到参数
            *args = strdup(tmp + strlen(argName));  // 复制参数值
            if (*args == NULL) {  // 内存分配失败检查
                goto ERROUT;  // 跳转到错误处理
            }
            free(cmdLine);  // 释放命令行缓冲区
            return LOS_OK;  // 返回成功
        }
    }

    PRINTK("no patch partition bootargs\n");  // 未找到参数

ERROUT:  // 错误处理标签
    free(cmdLine);  // 释放命令行缓冲区
    return LOS_NOK;  // 返回失败
}

/**
 * @brief 获取分区信息
 * @details 解析分区的启动参数，填充分区信息结构体，包括存储类型、文件系统类型、起始地址和大小
 * @param partInfo [INOUT] 分区信息结构体指针，用于存储解析结果
 * @return INT32 - 操作结果
 *         LOS_OK：获取成功
 *         LOS_NOK：获取失败或参数不完整
 */
INT32 GetPartitionInfo(struct PartitionInfo *partInfo)
{
    CHAR *args    = NULL;  // 存储参数列表
    CHAR *argsBak = NULL;  // 参数列表备份（用于释放内存）
    CHAR *p       = NULL;  // 当前参数指针

    // 获取分区启动参数
    if (GetPartitionBootArgs(partInfo->cmdlineArgName, &args) != LOS_OK) {
        return LOS_NOK;  // 获取失败返回
    }
    argsBak = args;  // 备份参数指针

    p = strsep(&args, " ");  // 分割参数（空格分隔）
    while (p != NULL) {  // 遍历所有参数
        if (MatchPartInfo(p, partInfo) != LOS_OK) {  // 匹配并解析参数
            goto ERROUT;  // 解析失败跳转到错误处理
        }
        p = strsep(&args, " ");  // 获取下一个参数
    }
    // 检查是否获取了所有必要信息
    if ((partInfo->fsType != NULL) && (partInfo->storageType != NULL)) {
        free(argsBak);  // 释放参数内存
        return LOS_OK;  // 返回成功
    }
    PRINT_ERR("Cannot find %s type\n", partInfo->partName);  // 打印信息缺失错误

ERROUT:  // 错误处理标签
    PRINT_ERR("Invalid %s information!\n", partInfo->partName);  // 打印无效信息错误
    // 释放已分配的内存
    if (partInfo->storageType != NULL) {
        free(partInfo->storageType);
        partInfo->storageType = NULL;
    }
    if (partInfo->fsType != NULL) {
        free(partInfo->fsType);
        partInfo->fsType = NULL;
    }
    free(argsBak);  // 释放参数内存

    return LOS_NOK;  // 返回失败
}

/**
 * @brief 获取分区设备名
 * @details 根据分区信息添加MTD分区，并返回设备名称
 * @param partInfo [IN] 分区信息结构体指针
 * @return const CHAR* - 设备名称字符串（成功）或NULL（失败）
 */
const CHAR *GetDevNameOfPartition(const struct PartitionInfo *partInfo)
{
    const CHAR *devName = NULL;  // 设备名称

    // 检查存储类型是否匹配
    if (strcmp(partInfo->storageType, STORAGE_TYPE) == 0) {
#if defined(LOSCFG_STORAGE_SPINOR)  // 条件编译：如果配置了SPINOR存储
        // 添加MTD分区
        INT32 ret = add_mtd_partition(FLASH_TYPE, partInfo->startAddr, partInfo->partSize, partInfo->partNum);
        if (ret != LOS_OK) {  // 添加分区失败检查
            PRINT_ERR("Failed to add %s partition! error = %d\n", partInfo->partName, ret);  // 打印错误信息
        } else {
            if (partInfo->devName != NULL) {  // 检查设备名是否存在
                devName = partInfo->devName;  // 设置设备名
            }
        }
#endif
    } else {  // 存储类型不匹配
        PRINT_ERR("Failed to find %s dev type: %s\n", partInfo->partName, partInfo->storageType);  // 打印错误信息
    }

    return devName;  // 返回设备名
}

/**
 * @brief 重置分区设备名
 * @details 删除指定的MTD分区，重置设备名称
 * @param partInfo [IN] 分区信息结构体指针
 * @return INT32 - 操作结果
 *         LOS_OK：删除成功
 *         LOS_NOK：删除失败或未配置SPINOR存储
 */
INT32 ResetDevNameofPartition(const struct PartitionInfo *partInfo)
{
    INT32 ret;
#if defined(LOSCFG_STORAGE_SPINOR)  // 条件编译：如果配置了SPINOR存储
    // 删除MTD分区
    ret = delete_mtd_partition(partInfo->partNum, FLASH_TYPE);
    if (ret != ENOERR) {  // 删除失败检查
        int err = get_errno();  // 获取错误码
        PRINT_ERR("Failed to delete %s, errno %d: %s\n", partInfo->devName, err, strerror(err));  // 打印错误信息
        ret = LOS_NOK;  // 设置返回值为失败
    }
#else  // 未配置SPINOR存储
    ret = LOS_NOK;  // 返回失败
#endif
    return ret;  // 返回操作结果
}