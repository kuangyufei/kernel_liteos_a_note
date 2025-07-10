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

#include <stdio.h>
#include <string.h>
#include "option.h"
#include "perf_list.h"

/**
 * @brief 解析单个命令行选项
 * @param[in] argv - 命令行参数数组
 * @param[in,out] index - 当前解析的参数索引，函数内部会更新此值
 * @param[in] opts - 选项配置数组，描述了支持的选项及其类型
 * @return 0表示成功，-1表示失败
 * @details 根据选项配置解析对应的命令行参数值，并存储到选项配置结构体中
 */
static int ParseOption(char **argv, int *index, PerfOption *opts)
{
    int ret = 0;                  // 函数返回值，默认0表示成功
    const char *str = NULL;       // 用于临时存储字符串参数

    // 遍历选项配置数组，寻找匹配的选项名
    while ((opts->name != NULL) && (*opts->name != 0)) {
        if (strcmp(argv[*index], opts->name) == 0) {  // 找到匹配的选项
            switch (opts->type) {                     // 根据选项类型处理参数
                case OPTION_TYPE_UINT:                // 无符号整数类型选项
                    // 将下一个参数转换为无符号整数并存储
                    *opts->value = strtoul(argv[++(*index)], NULL, 0);
                    break;
                case OPTION_TYPE_STRING:              // 字符串类型选项
                    // 存储下一个参数的字符串指针
                    *opts->str = argv[++(*index)];
                    break;
                case OPTION_TYPE_CALLBACK:            // 回调函数类型选项
                    str = argv[++(*index)];           // 获取参数字符串
                    // 调用回调函数处理参数，若返回非0则表示解析失败
                    if ((*opts->cb)(str) != 0) {
                        printf("parse error\n");
                        ret = -1;
                    }
                    break;
                default:                              // 未知选项类型
                    printf("invalid option\n");
                    ret = -1;
                    break;
            }
            return ret;  // 返回解析结果
        }
        opts++;  // 检查下一个选项配置
    }

    return -1;  // 未找到匹配的选项
}

/**
 * @brief 解析所有命令行选项和子命令
 * @param[in] argc - 命令行参数数量
 * @param[in] argv - 命令行参数数组
 * @param[in] opts - 选项配置数组
 * @param[out] cmd - 解析出的子命令信息
 * @return 0表示成功，-1表示失败
 * @details 处理所有以'-'开头的选项，然后解析剩余的子命令及其参数
 */
int ParseOptions(int argc, char **argv, PerfOption *opts, SubCmd *cmd)
{
    int i;                  // 循环计数器
    int index = 0;          // 当前参数索引

    // 解析所有以'-'开头的选项参数
    while ((index < argc) && (argv[index] != NULL) && (*argv[index] == '-')) {
        // 解析单个选项，失败则返回错误
        if (ParseOption(argv, &index, opts) != 0) {
            return -1;
        }
        index++;  // 移动到下一个参数
    }

    // 解析子命令路径和参数
    if ((index < argc) && (argv[index] != NULL)) {
        cmd->path = argv[index];       // 子命令路径/名称
        cmd->params[0] = argv[index];  // 子命令参数数组的第一个元素是命令本身
        index++;                       // 移动到下一个参数
    } else {
        printf("no subcmd to execute\n");  // 未指定子命令
        return -1;
    }

    // 解析子命令的参数列表
    for (i = 1; (index < argc) && (i < CMD_MAX_PARAMS); index++, i++) {
        cmd->params[i] = argv[index];  // 存储子命令参数
    }
    // 调试输出：打印解析出的子命令和参数
    printf_debug("subcmd = %s\n", cmd->path);
    for (int j = 0; j < i; j++) {
        printf_debug("paras[%d]:%s\n", j, cmd->params[j]);
    }
    return 0;  // 解析成功
}

/**
 * @brief 解析逗号分隔的ID列表字符串
 * @param[in] argv - 包含ID列表的字符串（格式如"1,2,3"）
 * @param[out] arr - 存储解析出的ID的数组
 * @param[out] len - 解析出的ID数量
 * @return 0表示成功，-1表示失败
 * @details 将逗号分隔的ID字符串转换为整数数组，支持十进制和十六进制格式
 */
int ParseIds(const char *argv, int *arr, unsigned int *len)
{
    int res, ret;                  // res:单个ID值，ret:函数返回值
    unsigned int index  = 0;       // 数组索引
    char *sp = NULL;               // strtok_r的当前分割结果
    char *this = NULL;             // strtok_r的保存指针
    char *list = strdup(argv);     // 复制输入字符串用于分割（避免修改原字符串）

    if (list == NULL) {            // 内存分配失败检查
        printf("no memory for ParseIds\n");
        return -1;
    }

    // 使用逗号作为分隔符分割字符串
    sp = strtok_r(list, ",", &this);
    while (sp) {
        res = strtoul(sp, NULL, 0);  // 将字符串转换为整数（自动识别十进制/十六进制）
        if (res < 0) {               // 转换结果有效性检查
            ret = -1;
            goto EXIT;               // 跳转到统一的资源释放处
        }
        arr[index++] = res;          // 存储ID到数组
        sp = strtok_r(NULL, ",", &this);  // 获取下一个ID字符串
    }
    *len = index;  // 设置解析出的ID数量
    ret = 0;       // 解析成功
EXIT:
    free(list);    // 释放临时字符串内存
    return ret;    // 返回解析结果
}

/**
 * @brief 将事件名称字符串转换为对应的PerfEvent结构体
 * @param[in] str - 事件名称字符串
 * @return 成功返回PerfEvent结构体指针，失败返回NULL
 * @details 在全局事件列表g_events中查找名称匹配的事件
 * @note 这是一个内联函数，用于提高事件查找效率
 */
static inline const PerfEvent *StrToEvent(const char *str)
{
    const PerfEvent *evt = &g_events[0];  // 从全局事件列表的第一个元素开始查找

    // 遍历事件列表，直到找到结束标记（event == -1）
    for (; evt->event != -1; evt++) {
        if (strcmp(str, evt->name) == 0) {  // 事件名称匹配
            return evt;                     // 返回找到的事件
        }
    }
    return NULL;  // 未找到匹配的事件
}

/**
 * @brief 解析逗号分隔的事件名称列表
 * @param[in] argv - 包含事件名称列表的字符串（格式如"event1,event2"）
 * @param[out] eventsCfg - 事件配置结构体，用于存储解析出的事件信息
 * @param[out] len - 解析出的事件数量
 * @return 0表示成功，-1表示失败
 * @details 将事件名称转换为对应的事件ID，并确保所有事件类型相同
 */
int ParseEvents(const char *argv, PerfEventConfig *eventsCfg, unsigned int *len)
{
    int ret;                       // 函数返回值
    unsigned int index  = 0;       // 事件索引
    const PerfEvent *event = NULL; // 指向当前解析的事件
    char *sp = NULL;               // strtok_r的当前分割结果
    char *this = NULL;             // strtok_r的保存指针
    char *list = strdup(argv);     // 复制输入字符串用于分割

    if (list == NULL) {            // 内存分配失败检查
        printf("no memory for ParseEvents\n");
        return -1;
    }

    // 使用逗号作为分隔符分割事件名称字符串
    sp = strtok_r(list, ",", &this);
    while (sp) {
        event = StrToEvent(sp);    // 将事件名称转换为PerfEvent结构体
        if (event == NULL) {       // 事件名称无效
            ret = -1;
            goto EXIT;
        }

        // 检查事件类型一致性
        if (index == 0) {
            eventsCfg->type = event->type;  // 第一个事件的类型作为基准类型
        } else if (eventsCfg->type != event->type) {
            printf("events type must be same\n");  // 事件类型不一致
            ret = -1;
            goto EXIT;
        }
        eventsCfg->events[index].eventId = event->event;  // 存储事件ID
        sp = strtok_r(NULL, ",", &this);                // 获取下一个事件名称
        index++;
    }
    *len = index;  // 设置解析出的事件数量
    ret = 0;       // 解析成功
EXIT:
    free(list);    // 释放临时字符串内存
    return ret;    // 返回解析结果
}