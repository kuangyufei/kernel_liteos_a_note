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
#include "los_memory.h"

/**
 * @brief 过滤字符串两端的双引号或单引号并复制字符串
 * @param str 输入字符串
 * @return CHAR* 处理后的新字符串指针，内存由LOS_MemAlloc分配，需调用者释放
 */
LITE_OS_SEC_TEXT_MINOR CHAR *OsCmdParseStrdup(const CHAR *str)
{
    CHAR *tempStr = NULL;  // 保存新字符串起始地址
    CHAR *newStr = NULL;   // 新字符串当前写入位置指针

    newStr = (CHAR *)LOS_MemAlloc(m_aucSysMem0, strlen(str) + 1);  // 分配内存(包含终止符)
    if (newStr == NULL) {  // 检查内存分配是否成功
        return NULL;  // 分配失败返回NULL
    }

    tempStr = newStr;  // 保存字符串起始地址
    for (; *str != '\0'; str++) {  // 遍历输入字符串
        if ((*str == '"') || (*str == '\'')) {  // 跳过双引号和单引号
            continue;  // 继续下一个字符
        }
        *newStr = *str;  // 复制有效字符
        newStr++;  // 移动写入指针
    }
    *newStr = '\0';  // 添加字符串终止符
    return tempStr;  // 返回处理后的字符串
}

/**
 * @brief 获取解析后的参数值
 * @param value 输出参数，指向存储参数值的指针
 * @param paraTokenStr 输入参数令牌字符串
 * @return UINT32 操作结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsCmdParseParaGet(CHAR **value, const CHAR *paraTokenStr)
{
    if ((paraTokenStr == NULL) || (value == NULL)) {  // 检查输入参数有效性
        return (UINT32)OS_ERROR;  // 参数无效返回错误
    }

    *value = OsCmdParseStrdup(paraTokenStr);  // 复制并处理参数字符串
    if (*value == NULL) {  // 检查字符串复制是否成功
        return LOS_NOK;  // 复制失败返回错误
    }
    return LOS_OK;  // 成功返回
}

/**
 * @brief 解析单个命令令牌并添加到解析结果中
 * @param cmdParsed 命令解析结果结构体指针
 * @param index 令牌索引
 * @param token 令牌字符串
 * @return UINT32 操作结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsCmdParseOneToken(CmdParsed *cmdParsed, UINT32 index, const CHAR *token)
{
    UINT32 ret = LOS_OK;  // 返回值
    UINT32 tempLen;       // 当前参数计数

    if (cmdParsed == NULL) {  // 检查解析结果结构体有效性
        return (UINT32)OS_ERROR;  // 结构体无效返回错误
    }

    if (index == 0) {  // 处理命令名(第一个令牌)
        if (cmdParsed->cmdType != CMD_TYPE_STD) {  // 非标准命令类型不处理
            return ret;  // 返回成功
        }
    }

    if ((token != NULL) && (cmdParsed->paramCnt < CMD_MAX_PARAS)) {  // 检查令牌有效性和参数计数
        tempLen = cmdParsed->paramCnt;  // 获取当前参数数量
        ret = OsCmdParseParaGet(&(cmdParsed->paramArray[tempLen]), token);  // 获取参数值
        if (ret != LOS_OK) {  // 检查参数获取是否成功
            return ret;  // 获取失败返回错误
        }
        cmdParsed->paramCnt++;  // 增加参数计数
    }
    return ret;  // 返回结果
}

/**
 * @brief 按指定分隔符拆分命令字符串为令牌
 * @param cmdStr 命令字符串
 * @param split 分隔符
 * @param cmdParsed 命令解析结果结构体指针
 * @return UINT32 操作结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsCmdTokenSplit(CHAR *cmdStr, CHAR split, CmdParsed *cmdParsed)
{
    enum {  // 状态枚举定义
        STAT_INIT,       // 初始状态
        STAT_TOKEN_IN,   // 令牌内部状态
        STAT_TOKEN_OUT   // 令牌外部状态
    } state = STAT_INIT;  // 初始状态
    UINT32 count = 0;     // 令牌计数
    CHAR *p = NULL;       // 字符串遍历指针
    CHAR *token = cmdStr; // 当前令牌起始地址
    UINT32 ret = LOS_OK;  // 返回值
    BOOL quotes = FALSE;  // 引号状态标志

    if (cmdStr == NULL) {  // 检查命令字符串有效性
        return (UINT32)OS_ERROR;  // 字符串无效返回错误
    }

    for (p = cmdStr; (*p != '\0') && (ret == LOS_OK); p++) {  // 遍历命令字符串
        if (*p == '"') {  // 遇到双引号
            SWITCH_QUOTES_STATUS(quotes);  // 切换引号状态
        }
        switch (state) {  // 根据当前状态处理
            case STAT_INIT:      // 初始状态
            case STAT_TOKEN_IN:  // 令牌内部状态
                if ((*p == split) && QUOTES_STATUS_CLOSE(quotes)) {  // 遇到分隔符且不在引号内
                    *p = '\0';  // 终止当前令牌
                    ret = OsCmdParseOneToken(cmdParsed, count++, token);  // 解析当前令牌
                    state = STAT_TOKEN_OUT;  // 切换到令牌外部状态
                }
                break;
            case STAT_TOKEN_OUT:  // 令牌外部状态
                if (*p != split) {  // 遇到非分隔符字符
                    token = p;  // 设置新令牌起始地址
                    state = STAT_TOKEN_IN;  // 切换到令牌内部状态
                }
                break;
            default:  // 默认状态
                break;
        }
    }

    if (((ret == LOS_OK) && (state == STAT_TOKEN_IN)) || (state == STAT_INIT)) {  // 处理最后一个令牌
        ret = OsCmdParseOneToken(cmdParsed, count, token);  // 解析最后一个令牌
    }

    return ret;  // 返回结果
}

/**
 * @brief 解析命令字符串为结构化数据
 * @param cmdStr 命令字符串
 * @param cmdParsed 命令解析结果结构体指针
 * @return UINT32 操作结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsCmdParse(CHAR *cmdStr, CmdParsed *cmdParsed)
{
    if ((cmdStr == NULL) || (cmdParsed == NULL) || (strlen(cmdStr) == 0)) {  // 检查输入参数有效性
        return (UINT32)OS_ERROR;  // 参数无效返回错误
    }
    return OsCmdTokenSplit(cmdStr, ' ', cmdParsed);  // 使用空格作为分隔符拆分命令
}
