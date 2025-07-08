/*
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
 
#ifndef _LOS_BOOTARGS_H
#define _LOS_BOOTARGS_H

#include "los_typedef.h"
/**
 * @def BYTES_PER_GBYTE
 * @brief 1GB对应的字节数
 * @note 1GB = 2^30字节 = 1073741824字节
 */
#define BYTES_PER_GBYTE         (1 << 30)  // 1GB字节数：2^30
/**
 * @def BYTES_PER_MBYTE
 * @brief 1MB对应的字节数
 * @note 1MB = 2^20字节 = 1048576字节
 */
#define BYTES_PER_MBYTE         (1 << 20)  // 1MB字节数：2^20
/**
 * @def BYTES_PER_KBYTE
 * @brief 1KB对应的字节数
 * @note 1KB = 2^10字节 = 1024字节
 */
#define BYTES_PER_KBYTE         (1 << 10)  // 1KB字节数：2^10
/**
 * @def COMMAND_LINE_ADDR
 * @brief 启动命令行在内存中的起始地址
 * @note 计算公式：LOSCFG_BOOTENV_ADDR(KB) * BYTES_PER_KBYTE(字节/KB)
 */
#define COMMAND_LINE_ADDR       (LOSCFG_BOOTENV_ADDR * BYTES_PER_KBYTE)  // 命令行地址：启动环境地址(KB)转换为字节
/**
 * @def COMMAND_LINE_SIZE
 * @brief 启动命令行的最大长度
 * @note 单位：字节，固定为1024字节
 */
#define COMMAND_LINE_SIZE       1024  // 命令行缓冲区大小：1024字节
/**
 * @def MAX_ARGS_NUM
 * @brief 启动参数的最大数量
 * @note 最多支持100个启动参数
 */
#define MAX_ARGS_NUM            100  // 最大启动参数数量：100个
/**
 * @def EMMC_SEC_SIZE
 * @brief EMMC扇区大小
 * @note 仅在启用LOSCFG_STORAGE_EMMC时定义，固定为512字节
 */
#ifdef LOSCFG_STORAGE_EMMC
#define EMMC_SEC_SIZE           512  // EMMC扇区大小：512字节
#endif

/**
 * @struct BootArgs
 * @brief 启动参数键值对结构体
 * @details 用于存储解析后的启动参数名称和对应值
 */
struct BootArgs {
    CHAR *argName;   // 参数名称字符串指针
    CHAR *argValue;  // 参数值字符串指针
};

INT32 LOS_GetCmdLine(VOID);
VOID LOS_FreeCmdLine(VOID);
INT32 LOS_ParseBootargs(VOID);
INT32 LOS_GetArgValue(CHAR *argName, CHAR **argValue);
UINT64 LOS_GetAlignsize(VOID);
UINT64 LOS_SizeStrToNum(CHAR *value);

#ifdef LOSCFG_BOOTENV_RAM
CHAR *OsGetArgsAddr(VOID);
#endif
#endif /* _LOS_BOOTARGS_H */