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

#include "los_bootargs.h"
#include "los_base.h"
#include "string.h"

#if defined(LOSCFG_STORAGE_SPINOR) || defined(LOSCFG_STORAGE_SPINAND)
#include "mtd_list.h"
#endif

#ifdef LOSCFG_STORAGE_EMMC
#include "disk.h"
#endif
// 全局变量声明
STATIC CHAR *g_cmdLine = NULL;  // 存储从设备读取的命令行字符串
STATIC UINT64 g_alignSize = 0;  // 存储设备对齐大小（扇区大小或擦除块大小）
STATIC struct BootArgs g_bootArgs[MAX_ARGS_NUM] = {0};  // 存储解析后的启动参数键值对数组

/**
 * @brief 从存储设备读取启动命令行
 * @details 根据配置的存储设备类型（EMMC/SPI NOR/SPI NAND）读取命令行数据
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
INT32 LOS_GetCmdLine(VOID)
{
    int ret;  // 函数返回值，用于错误处理

    g_cmdLine = (CHAR *)malloc(COMMAND_LINE_SIZE);  // 分配命令行缓冲区内存
    if (g_cmdLine == NULL) {  // 检查内存分配是否成功
        PRINT_ERR("Malloc g_cmdLine space error!\n");  // 打印内存分配错误信息
        return LOS_NOK;  // 返回失败
    }

#ifdef LOSCFG_STORAGE_EMMC  // 条件编译：如果启用EMMC存储
    los_disk *emmcDisk = los_get_mmcdisk_bytype(EMMC);  // 获取EMMC磁盘设备
    if (emmcDisk == NULL) {  // 检查EMMC磁盘是否获取成功
        PRINT_ERR("Get EMMC disk failed!\n");  // 打印错误信息
        goto ERROUT;  // 跳转到错误处理流程
    }
    g_alignSize = EMMC_SEC_SIZE;  // 设置对齐大小为EMMC扇区大小
    // 从EMMC读取命令行：参数依次为磁盘ID、缓冲区、起始扇区、扇区数、是否同步
    ret = los_disk_read(emmcDisk->disk_id, g_cmdLine, COMMAND_LINE_ADDR / EMMC_SEC_SIZE,
                        COMMAND_LINE_SIZE / EMMC_SEC_SIZE, TRUE);
    if (ret == 0) {  // 检查读取是否成功（0表示成功）
        return LOS_OK;  // 返回成功
    }
#endif

#ifdef LOSCFG_STORAGE_SPINOR  // 条件编译：如果启用SPI NOR存储
    struct MtdDev *mtd = GetMtd("spinor");  // 获取SPI NOR MTD设备
    if (mtd == NULL) {  // 检查MTD设备是否获取成功
        PRINT_ERR("Get spinor mtd failed!\n");  // 打印错误信息
        goto ERROUT;  // 跳转到错误处理流程
    }
    g_alignSize = mtd->eraseSize;  // 设置对齐大小为SPI NOR擦除块大小
    // 从SPI NOR读取命令行：参数依次为MTD设备、起始地址、长度、缓冲区
    ret = mtd->read(mtd, COMMAND_LINE_ADDR, COMMAND_LINE_SIZE, g_cmdLine);
    if (ret == COMMAND_LINE_SIZE) {  // 检查读取字节数是否等于预期长度
        return LOS_OK;  // 返回成功
    }
#endif

#ifdef LOSCFG_STORAGE_SPINAND  // 条件编译：如果启用SPI NAND存储
    struct MtdDev *mtd = GetMtd("nand");  // 获取SPI NAND MTD设备
    if (mtd == NULL) {  // 检查MTD设备是否获取成功
        PRINT_ERR("Get nand mtd failed!\n");  // 打印错误信息
        goto ERROUT;  // 跳转到错误处理流程
    }
    g_alignSize = mtd->eraseSize;  // 设置对齐大小为SPI NAND擦除块大小
    // 从SPI NAND读取命令行：参数依次为MTD设备、起始地址、长度、缓冲区
    ret = mtd->read(mtd, COMMAND_LINE_ADDR, COMMAND_LINE_SIZE, g_cmdLine);
    if (ret == COMMAND_LINE_SIZE) {  // 检查读取字节数是否等于预期长度
        return LOS_OK;  // 返回成功
    }
#endif

    PRINT_ERR("Read cmdline error!\n");  // 所有存储设备读取失败
ERROUT:  // 错误处理标签
    free(g_cmdLine);  // 释放已分配的命令行缓冲区
    g_cmdLine = NULL;  // 重置全局指针
    return LOS_NOK;  // 返回失败
}

/**
 * @brief 释放命令行缓冲区内存
 * @details 安全释放g_cmdLine指向的内存并重置指针
 */
VOID LOS_FreeCmdLine(VOID)
{
    if (g_cmdLine != NULL) {  // 检查指针是否有效
        free(g_cmdLine);  // 释放内存
        g_cmdLine = NULL;  // 重置指针避免野指针
    }
}

/**
 * @brief 获取启动参数字符串
 * @param[out] args 输出参数，指向启动参数字符串的指针
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATIC INT32 GetBootargs(CHAR **args)
{
#ifdef LOSCFG_BOOTENV_RAM  // 条件编译：如果使用RAM中的启动环境
    *args = OsGetArgsAddr();  // 直接从RAM获取参数地址
    return LOS_OK;  // 返回成功
#else  // 从命令行解析启动参数
    INT32 i;  // 循环索引
    INT32 len = 0;  // 当前字符串长度
    CHAR *tmp = NULL;  // 临时指针，用于查找bootargs
    const CHAR *bootargsName = "bootargs=";  // 启动参数关键字

    if (g_cmdLine == NULL) {  // 检查命令行是否已读取
        PRINT_ERR("Should call LOS_GetCmdLine() first!\n");  // 提示先调用读取命令行函数
        return LOS_NOK;  // 返回失败
    }

    // 遍历命令行缓冲区，查找bootargs关键字
    for (i = 0; i < COMMAND_LINE_SIZE; i += len + 1) {
        len = strlen(g_cmdLine + i);  // 获取当前字符串长度
        tmp = strstr(g_cmdLine + i, bootargsName);  // 查找bootargs关键字
        if (tmp != NULL) {  // 如果找到
            *args = tmp + strlen(bootargsName);  // 指向参数值部分（跳过"bootargs="）
            return LOS_OK;  // 返回成功
        }
    }
    PRINT_ERR("Cannot find bootargs!\n");  // 未找到bootargs
    return LOS_NOK;  // 返回失败
#endif
}

/**
 * @brief 解析启动参数为键值对
 * @details 将启动参数字符串分割为"name=value"格式的键值对，存储到g_bootArgs数组
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
INT32 LOS_ParseBootargs(VOID)
{
    INT32 idx = 0;  // 参数索引，用于g_bootArgs数组
    INT32 ret;  // 函数返回值
    CHAR *args = NULL;  // 指向原始启动参数字符串
    CHAR *argName = NULL;  // 参数名称
    CHAR *argValue = NULL;  // 参数值

    ret = GetBootargs(&args);  // 获取启动参数字符串
    if (ret != LOS_OK) {  // 检查获取是否成功
        return LOS_NOK;  // 返回失败
    }

    // 使用空格分割参数，循环处理每个参数
    while ((argValue = strsep(&args, " ")) != NULL) {
        argName = strsep(&argValue, "=");  // 使用等号分割参数名和值
        if (argValue == NULL) {  // 如果没有等号（非"name=value"格式）
            /* If the argument is not compliance with the format 'foo=bar' */
            g_bootArgs[idx].argName = argName;  // 参数名设为整个字符串
            g_bootArgs[idx].argValue = argName;  // 参数值也设为整个字符串
        } else {  // 正常"name=value"格式
            g_bootArgs[idx].argName = argName;  // 存储参数名
            g_bootArgs[idx].argValue = argValue;  // 存储参数值
        }
        if (++idx >= MAX_ARGS_NUM) {  // 检查是否超过最大参数数量
            /* Discard the rest arguments */
            break;  // 丢弃剩余参数
        }
    }
    return LOS_OK;  // 返回成功
}

/**
 * @brief 根据参数名获取参数值
 * @param[in] argName 要查找的参数名称
 * @param[out] argValue 输出参数，指向找到的参数值
 * @return 成功返回LOS_OK，未找到返回LOS_NOK
 */
INT32 LOS_GetArgValue(CHAR *argName, CHAR **argValue)
{
    INT32 idx = 0;  // 循环索引

    while (idx < MAX_ARGS_NUM) {  // 遍历参数数组
        if (g_bootArgs[idx].argName == NULL) {  // 遇到未使用的参数项
            break;  // 退出循环
        }
        // 比较参数名（区分大小写）
        if (strcmp(argName, g_bootArgs[idx].argName) == 0) {
            *argValue = g_bootArgs[idx].argValue;  // 找到匹配的参数值
            return LOS_OK;  // 返回成功
        }
        idx++;  // 下一个参数
    }

    return LOS_NOK;  // 未找到参数
}

/**
 * @brief 获取存储设备对齐大小
 * @return 对齐大小（字节），可能是扇区大小或擦除块大小
 */
UINT64 LOS_GetAlignsize(VOID)
{
    return g_alignSize;  // 返回全局对齐大小变量
}

/**
 * @brief 将大小字符串转换为数值
 * @details 支持格式：十进制（如"1024"）、十六进制（如"0x400"）、带单位（k/m/g，如"2k"）
 * @param[in] value 要转换的大小字符串
 * @return 转换后的数值（字节），转换失败返回0并打印错误
 */
UINT64 LOS_SizeStrToNum(CHAR *value)
{
    UINT64 num = 0;  // 存储转换后的数值

    /* If the string is a hexadecimal value */
    // 尝试解析十六进制格式（0x开头）
    if (sscanf_s(value, "0x%llx", &num) > 0) {
        value += strlen("0x");  // 跳过"0x"前缀
        // 检查剩余字符是否都是十六进制数字
        if (strspn(value, "0123456789abcdefABCDEF") < strlen(value)) {
            goto ERROUT;  // 包含非十六进制字符，跳转到错误处理
        }
        return num;  // 返回解析结果
    }

    /* If the string is a decimal value in unit *Bytes */
    // 尝试解析十进制数字部分
    INT32 ret = sscanf_s(value, "%d", &num);
    // 获取数字部分的长度（跳过所有数字字符）
    INT32 decOffset = strspn(value, "0123456789");
    CHAR *endPos = value + decOffset;  // 指向数字部分后的字符
    // 检查解析结果或数字部分是否完整（如"123abc"视为无效）
    if ((ret <= 0) || (decOffset < (strlen(value) - 1))) {
        goto ERROUT;  // 解析失败，跳转到错误处理
    }

    // 根据单位后缀转换数值
    if (strlen(endPos) == 0) {  // 无单位（字节）
        return num;  // 直接返回数值
    } else if (strcasecmp(endPos, "k") == 0) {  // 单位：KB（1024字节）
        num = num * BYTES_PER_KBYTE;
    } else if (strcasecmp(endPos, "m") == 0) {  // 单位：MB（1048576字节）
        num = num * BYTES_PER_MBYTE;
    } else if (strcasecmp(endPos, "g") == 0) {  // 单位：GB（1073741824字节）
        num = num * BYTES_PER_GBYTE;
    } else {  // 未知单位
        goto ERROUT;  // 跳转到错误处理
    }

    return num;  // 返回转换后的数值

ERROUT:  // 错误处理标签
    PRINT_ERR("Invalid value string \"%s\"!\n", value);  // 打印无效格式错误
    return num;  // 返回0（此时num未被成功赋值）
}