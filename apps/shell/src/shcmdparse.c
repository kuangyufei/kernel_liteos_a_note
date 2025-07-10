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

#include "shcmd.h"
#include "sherr.h"


/*
 * Filter out double quote or single-quoted strings at both ends
 */
/**
 * @brief 创建去除引号的字符串副本
 * @param str 输入字符串（可能包含引号）
 * @return 成功返回新分配的去引号字符串指针，失败返回NULL
 */
char *OsCmdParseStrdup(const char *str)
{
    char *tempStr = NULL;  // 保存新字符串起始地址
    char *newStr = NULL;   // 用于构建新字符串的指针

    newStr = (char *)malloc(strlen(str) + 1);  // 分配内存（+1用于存储终止符'\0'）
    if (newStr == NULL) {                     // 内存分配失败检查
        return NULL;
    }

    tempStr = newStr;                         // 保存字符串起始位置
    for (; *str != '\0'; str++) {            // 遍历输入字符串
        if ((*str == '"') || (*str == '\'')) {  // 跳过双引号和单引号
            continue;
        }
        *newStr = *str;                       // 复制有效字符
        newStr++;
    }
    *newStr = '\0';                          // 添加字符串终止符
    return tempStr;                           // 返回新字符串
}

/**
 * @brief 解析命令参数并分配内存
 * @param value 输出参数，用于存储解析后的参数字符串
 * @param paraTokenStr 输入的参数令牌字符串
 * @return 成功返回SH_OK，失败返回SH_ERROR或SH_NOK
 */
unsigned int OsCmdParseParaGet(char **value, const char *paraTokenStr)
{
    if ((paraTokenStr == NULL) || (value == NULL)) {  // 输入参数有效性检查
        return (unsigned int)SH_ERROR;                // 参数无效返回错误码
    }
    *value = OsCmdParseStrdup(paraTokenStr);          // 调用去引号字符串复制函数
    if (*value == NULL) {                             // 内存分配失败检查
        return SH_NOK;                                // 分配失败返回错误码
    }
    return SH_OK;                                     // 成功返回
}

/**
 * @brief 解析单个命令令牌并添加到参数数组
 * @param cmdParsed 命令解析结果结构体指针
 * @param index 当前令牌索引
 * @param token 令牌字符串
 * @return 成功返回SH_OK，失败返回错误码
 */
unsigned int OsCmdParseOneToken(CmdParsed *cmdParsed, unsigned int index, const char *token)
{
    unsigned int ret = SH_OK;       // 函数返回值
    unsigned int tempLen;           // 参数计数临时变量

    if (cmdParsed == NULL) {        // 结构体指针有效性检查
        return (unsigned int)SH_ERROR;
    }

    if (index == 0) {               // 处理命令名（第一个令牌）
        if (cmdParsed->cmdType != CMD_TYPE_STD) {  // 非标准命令类型不处理
            return ret;
        }
    }

    // 令牌不为空且参数数量未达上限时处理
    if ((token != NULL) && (cmdParsed->paramCnt < CMD_MAX_PARAS)) {
        tempLen = cmdParsed->paramCnt;              // 获取当前参数数量
        // 解析参数并添加到参数数组
        ret = OsCmdParseParaGet(&(cmdParsed->paramArray[tempLen]), token);
        if (ret != SH_OK) {                         // 参数解析失败检查
            return ret;
        }
        cmdParsed->paramCnt++;                      // 参数计数加1
    }
    return ret;
}

/**
 * @brief 命令字符串分割为令牌（支持引号处理）
 * @param cmdStr 输入命令字符串
 * @param split 分隔符（通常为空格）
 * @param cmdParsed 命令解析结果结构体指针
 * @return 成功返回SH_OK，失败返回错误码
 */
unsigned int OsCmdTokenSplit(char *cmdStr, char split, CmdParsed *cmdParsed)
{
    enum {                          // 状态机状态定义
        STAT_INIT,                  // 初始状态
        STAT_TOKEN_IN,              // 令牌解析中
        STAT_TOKEN_OUT              // 令牌解析完成
    } state = STAT_INIT;            // 初始状态
    unsigned int count = 0;         // 令牌计数
    char *p = NULL;                 // 字符串遍历指针
    char *token = cmdStr;           // 当前令牌起始地址
    unsigned int ret = SH_OK;       // 函数返回值
    bool quotes = FALSE;            // 引号状态标志（FALSE:关闭,TRUE:打开）

    if (cmdStr == NULL) {           // 命令字符串有效性检查
        return (unsigned int)SH_ERROR;
    }

    // 遍历命令字符串进行令牌分割
    for (p = cmdStr; (*p != '\0') && (ret == SH_OK); p++) {
        if (*p == '"') {                       // 遇到双引号时切换引号状态
            SWITCH_QUOTES_STATUS(quotes);
        }
        switch (state) {
            case STAT_INIT:                    // 初始状态
            case STAT_TOKEN_IN:                // 令牌解析中状态
                // 遇到分隔符且不在引号内时
                if ((*p == split) && QUOTES_STATUS_CLOSE(quotes)) {
                    *p = '\0';                // 用\0结束当前令牌
                    // 解析当前令牌并增加计数
                    ret = OsCmdParseOneToken(cmdParsed, count++, token);
                    state = STAT_TOKEN_OUT;    // 切换到令牌解析完成状态
                }
                break;
            case STAT_TOKEN_OUT:               // 令牌解析完成状态
                if (*p != split) {              // 遇到非分隔符字符
                    token = p;                  // 设置新令牌起始地址
                    state = STAT_TOKEN_IN;      // 切换到令牌解析中状态
                }
                break;
            default:                           // 默认状态（不处理）
                break;
        }
    }

    // 处理最后一个令牌（如果存在）
    if (((ret == SH_OK) && (state == STAT_TOKEN_IN)) || (state == STAT_INIT)) {
        ret = OsCmdParseOneToken(cmdParsed, count, token);
    }

    return ret;
}

/**
 * @brief 命令解析入口函数
 * @param cmdStr 输入命令字符串
 * @param cmdParsed 命令解析结果结构体指针
 * @return 成功返回SH_OK，失败返回SH_ERROR
 */
unsigned int OsCmdParse(char *cmdStr, CmdParsed *cmdParsed)
{
    // 输入参数有效性检查
    if ((cmdStr == NULL) || (cmdParsed == NULL) || (strlen(cmdStr) == 0)) {
        return (unsigned int)SH_ERROR;
    }
    // 调用令牌分割函数（使用空格作为分隔符）
    return OsCmdTokenSplit(cmdStr, ' ', cmdParsed);
}

