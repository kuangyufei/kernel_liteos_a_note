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
#ifndef _LOS_PARTITION_UTILS_H
#define _LOS_PARTITION_UTILS_H

#include "los_typedef.h"
#include "los_base.h"
/**
 * @brief 字节单位转换宏定义
 * @details 定义常用存储单位换算关系，用于分区大小和地址的单位转换
 */
#define BYTES_PER_MBYTE         0x100000  // 1MB对应的字节数（1024*1024）
#define BYTES_PER_KBYTE         0x400     // 1KB对应的字节数（1024）
#define DEC_NUMBER_STRING       "0123456789"  // 十进制数字字符集，用于数值合法性校验
#define HEX_NUMBER_STRING       "0123456789abcdefABCDEF"  // 十六进制数字字符集，用于数值合法性校验

/**
 * @brief 命令行参数配置宏
 * @details 定义启动命令行的存储地址和大小，用于从指定位置读取分区参数
 */
#define COMMAND_LINE_ADDR       LOSCFG_BOOTENV_ADDR * BYTES_PER_KBYTE  // 命令行参数在Flash中的起始地址（从配置项计算）
#define COMMAND_LINE_SIZE       1024  // 命令行参数的最大长度（字节）

/**
 * @brief 存储类型条件编译配置
 * @details 根据是否启用SPINOR存储，定义不同的存储类型和文件系统类型
 */
#if defined(LOSCFG_STORAGE_SPINOR)  // 若配置了SPINOR存储
#define FLASH_TYPE              "spinor"  // Flash类型为spinor
#define STORAGE_TYPE            "flash"   // 存储类型标识为flash
#define FS_TYPE                 "jffs2"   // 默认文件系统类型为jffs2
#else  // 未配置SPINOR存储（默认使用emmc）
#define STORAGE_TYPE            "emmc"    // 存储类型标识为emmc
#endif

/**
 * @brief 分区信息结构体
 * @details 定义分区的基本属性和参数，用于描述和管理一个存储分区
 */
struct PartitionInfo {
    const CHAR   *partName;           // 分区名称（如"boot"、"system"）
    const CHAR   *cmdlineArgName;     // 启动命令行中该分区的参数名（用于解析命令行）
    const CHAR   *storageTypeArgName; // 存储类型参数名（如"storage_type="）
    CHAR         *storageType;        // 存储类型值（如"flash"或"emmc"，动态解析填充）
    const CHAR   *fsTypeArgName;      // 文件系统类型参数名（如"fs_type="）
    CHAR         *fsType;             // 文件系统类型值（如"jffs2"，动态解析填充）
    const CHAR   *addrArgName;        // 起始地址参数名（如"addr="）
    INT32        startAddr;           // 分区起始地址（字节，动态解析填充，初始值-1表示未设置）
    const CHAR   *partSizeArgName;    // 分区大小参数名（如"size="）
    INT32        partSize;            // 分区大小（字节，动态解析填充，初始值-1表示未设置）
    CHAR         *devName;            // 分区设备名（如"mtdblock0"，动态分配）
    UINT32       partNum;             // 分区编号（用于标识不同分区）
};

INT32 GetPartitionInfo(struct PartitionInfo *partInfo);
const CHAR *GetDevNameOfPartition(const struct PartitionInfo *partInfo);
INT32 ResetDevNameofPartition(const struct PartitionInfo *partInfo);

#endif /* _LOS_PARTITION_UTILS_H */
