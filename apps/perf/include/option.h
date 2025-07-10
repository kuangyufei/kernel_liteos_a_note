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


#ifndef _OPTION_H
#define _OPTION_H

#include "perf.h"

#ifdef  __cplusplus
#if  __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */


/**
 * @brief 命令参数最大数量宏定义
 * @note 定义了perf命令支持的最大参数个数为10个
 */
#define CMD_MAX_PARAMS 10

/**
 * @brief 选项解析回调函数类型定义
 * @param[in] argv 命令行参数字符串
 * @return 成功返回0，失败返回非0值
 * @note 用于处理需要自定义解析逻辑的命令行选项
 */
typedef int (*CALL_BACK)(const char *argv);

/**
 * @brief 选项类型枚举
 * @details 定义perf支持的命令行选项数据类型
 */
enum OptionType {
    OPTION_TYPE_UINT,       // 无符号整数类型选项
    OPTION_TYPE_STRING,     // 字符串类型选项
    OPTION_TYPE_CALLBACK,   // 回调函数类型选项
};

/**
 * @brief 性能工具选项结构体
 * @details 用于描述perf命令支持的命令行选项及其属性
 */
typedef struct {
    int type;               // 选项类型，对应OptionType枚举
    const char *name;       // 选项名称（如"-e"、"-t"）
    const char **str;       // 字符串类型选项的值指针
    unsigned int *value;    // 无符号整数类型选项的值指针
    CALL_BACK cb;           // 回调函数类型选项的函数指针
} PerfOption;

/**
 * @brief 子命令结构体
 * @details 存储解析后的子命令路径及其参数列表
 */
typedef struct {
    const char *path;               // 子命令可执行文件路径
    char *params[CMD_MAX_PARAMS];   // 子命令参数数组，最大长度为CMD_MAX_PARAMS
} SubCmd;

/**
 * @brief 选项数组结束标记宏
 * @note 用于标记PerfOption数组的结束，需放在数组最后一项
 */
#define OPTION_END()          {.name = ""}

/**
 * @brief 无符号整数类型选项初始化宏
 * @param[in] n 选项名称（如"-p"）
 * @param[in] v 存储选项值的无符号整数指针
 */
#define OPTION_UINT(n, v)     {.type = OPTION_TYPE_UINT,     .name = (n), .value = (v)}

/**
 * @brief 字符串类型选项初始化宏
 * @param[in] n 选项名称（如"-o"）
 * @param[in] s 存储选项值的字符串指针
 */
#define OPTION_STRING(n, s)   {.type = OPTION_TYPE_STRING,   .name = (n), .str = (s)}

/**
 * @brief 回调函数类型选项初始化宏
 * @param[in] n 选项名称（如"-e"）
 * @param[in] c 处理该选项的回调函数指针
 */
#define OPTION_CALLBACK(n, c) {.type = OPTION_TYPE_CALLBACK, .name = (n), .cb = (c)}


int ParseOptions(int argc, char **argv, PerfOption *opt, SubCmd *cmd);
int ParseEvents(const char *argv, PerfEventConfig *eventsCfg, unsigned int *len);
int ParseIds(const char *argv, int *arr, unsigned int *len);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* _OPTION_H */
