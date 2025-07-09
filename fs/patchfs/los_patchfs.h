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
#ifndef _LOS_PATCHFS_H
#define _LOS_PATCHFS_H

#include "los_typedef.h"
#include "los_base.h"

/**
 * @brief 将宏参数转换为字符串字面量的辅助宏
 * @details 内部实现宏，用于处理可变参数并执行字符串化操作
 * @param x... 可变参数列表，需要被字符串化的内容
 */
#define __stringify_1(x...)     #x  // 将参数直接字符串化
/**
 * @brief 将宏参数转换为字符串字面量的封装宏
 * @details 提供更友好的接口，间接调用__stringify_1实现字符串化
 * @param x... 可变参数列表，需要被字符串化的内容
 */
#define __stringify(x...)       __stringify_1(x)  // 调用内部字符串化宏

/**
 * @brief 补丁分区编号定义
 * @details 指定补丁文件系统所使用的分区索引
 */
#define PATCH_PARTITIONNUM      1  // 补丁分区编号为1
#ifdef TEE_ENABLE  // 如果启用了TEE安全环境
/**
 * @brief TEE环境下补丁文件系统的Flash起始地址
 * @details 在TEE使能时使用的固定存储地址配置
 */
#define PATCHFS_FLASH_ADDR      0xC00000  // TEE模式下补丁区起始地址：0xC00000
/**
 * @brief TEE环境下补丁文件系统的大小
 * @details 在TEE使能时分配的存储空间大小(2MB)
 */
#define PATCHFS_FLASH_SIZE      0x200000  // TEE模式下补丁区大小：2MB(0x200000字节)
#else  // TEE未启用时的配置
/**
 * @brief 非TEE环境下补丁文件系统的Flash起始地址
 * @details 普通模式下使用的固定存储地址配置
 */
#define PATCHFS_FLASH_ADDR      0xD00000  // 普通模式下补丁区起始地址：0xD00000
/**
 * @brief 非TEE环境下补丁文件系统的大小
 * @details 普通模式下分配的存储空间大小(3MB)
 */
#define PATCHFS_FLASH_SIZE      0x300000  // 普通模式下补丁区大小：3MB(0x300000字节)
#endif

#if defined(LOSCFG_STORAGE_SPINOR)  // 如果配置了SPI NOR闪存
/**
 * @brief SPI NOR闪存环境下的补丁设备名称
 * @details 拼接SPI NOR设备路径与分区编号，形成完整设备名
 */
#define PATCH_FLASH_DEV_NAME          "/dev/spinorblk"__stringify(PATCH_PARTITIONNUM)  // SPI NOR设备名：/dev/spinorblk1
#else  // 默认使用MMC存储设备
/**
 * @brief MMC存储环境下的补丁设备名称
 * @details 拼接MMC设备路径与分区编号，形成完整设备名
 */
#define PATCH_FLASH_DEV_NAME          "/dev/mmcblk"__stringify(PATCH_PARTITIONNUM)  // MMC设备名：/dev/mmcblk1
#endif

/**
 * @brief 补丁文件系统的挂载点路径
 * @details 指定补丁FS在VFS中的挂载位置
 */
#define PATCHFS_MOUNT_POINT     "/patch"  // 补丁文件系统挂载点

/**
 * @brief 补丁分区的名称标识
 * @details 用于分区表中识别补丁分区的关键字
 */
#define PATCHPART_NAME          "patchfs"  // 补丁分区名称
/**
 * @brief 命令行中补丁FS配置的参数名
 * @details 启动参数中用于指定补丁FS配置的前缀
 */
#define PATCH_CMDLINE_ARGNAME   "patchfs="  // 命令行补丁FS参数名
/**
 * @brief 命令行中存储设备配置的参数名
 * @details 启动参数中用于指定补丁存储设备的前缀
 */
#define PATCH_STORAGE_ARGNAME   "patch="  // 命令行存储设备参数名
/**
 * @brief 命令行中文件系统类型配置的参数名
 * @details 启动参数中用于指定补丁FS类型的前缀
 */
#define PATCH_FSTYPE_ARGNAME    "patchfstype="  // 命令行文件系统类型参数名
/**
 * @brief 命令行中起始地址配置的参数名
 * @details 启动参数中用于指定补丁区起始地址的前缀
 */
#define PATCH_ADDR_ARGNAME      "patchaddr="  // 命令行起始地址参数名
/**
 * @brief 命令行中大小配置的参数名
 * @details 启动参数中用于指定补丁区大小的前缀
 */
#define PATCH_SIZE_ARGNAME      "patchsize="  // 命令行大小参数名

INT32 OsMountPatchFs(VOID);

#endif /* _LOS_PATCHFS_H */
