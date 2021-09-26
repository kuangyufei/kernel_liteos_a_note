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


/*
 * Filter out double quote or single-quoted strings at both ends
 *///过滤掉两端的双引号或单引号字符串
LITE_OS_SEC_TEXT_MINOR CHAR *OsCmdParseStrdup(const CHAR *str)
{
    CHAR *tempStr = NULL;
    CHAR *newStr = NULL;

    newStr = (CHAR *)LOS_MemAlloc(m_aucSysMem0, strlen(str) + 1);
    if (newStr == NULL) {
        return NULL;
    }

    tempStr = newStr;
    for (; *str != '\0'; str++) {
        if ((*str == '\"') || (*str == '\'')) {//过滤掉 \" 和 \'
            continue;
        }
        *newStr = *str;
        newStr++;
    }
    *newStr = '\0';
    return tempStr;
}
//获取一个参数实体
LITE_OS_SEC_TEXT_MINOR UINT32 OsCmdParseParaGet(CHAR **value, const CHAR *paraTokenStr)
{
    if ((paraTokenStr == NULL) || (value == NULL)) {
        return (UINT32)OS_ERROR;
    }

    *value = OsCmdParseStrdup(paraTokenStr);
    if (*value == NULL) {
        return LOS_NOK;
    }
    return LOS_OK;
}
//解析出一个参数
LITE_OS_SEC_TEXT_MINOR UINT32 OsCmdParseOneToken(CmdParsed *cmdParsed, UINT32 index, const CHAR *token)
{
    UINT32 ret = LOS_OK;
    UINT32 tempLen;

    if (cmdParsed == NULL) {
        return (UINT32)OS_ERROR;
    }

    if (index == 0) {
        if (cmdParsed->cmdType != CMD_TYPE_STD) {
            return ret;
        }
    }
	//参数限制,最多不能超过32个
    if ((token != NULL) && (cmdParsed->paramCnt < CMD_MAX_PARAS)) {
        tempLen = cmdParsed->paramCnt;
        ret = OsCmdParseParaGet(&(cmdParsed->paramArray[tempLen]), token);//获取一个参数实体
        if (ret != LOS_OK) {
            return ret;
        }
        cmdParsed->paramCnt++;//参数增加一个
    }
    return ret;
}
//将shell命令按 ' ' 分开处理
LITE_OS_SEC_TEXT_MINOR UINT32 OsCmdTokenSplit(CHAR *cmdStr, CHAR split, CmdParsed *cmdParsed)
{
    enum {
        STAT_INIT,
        STAT_TOKEN_IN,
        STAT_TOKEN_OUT
    } state = STAT_INIT;
    UINT32 count = 0;
    CHAR *p = NULL;
    CHAR *token = cmdStr;
    UINT32 ret = LOS_OK;
    BOOL quotes = FALSE;

    if (cmdStr == NULL) {
        return (UINT32)OS_ERROR;
    }

    for (p = cmdStr; (*p != '\0') && (ret == LOS_OK); p++) {
        if (*p == '\"') {
            SWITCH_QUOTES_STATUS(quotes);
        }
        switch (state) {
            case STAT_INIT:
            case STAT_TOKEN_IN://开始参数获取
                if ((*p == split) && QUOTES_STATUS_CLOSE(quotes)) {
                    *p = '\0';
                    ret = OsCmdParseOneToken(cmdParsed, count++, token);//解析一个令牌参数
                    state = STAT_TOKEN_OUT;//参数获取完成
                }
                break;
            case STAT_TOKEN_OUT:
                if (*p != split) {
                    token = p;
                    state = STAT_TOKEN_IN;
                }
                break;
            default:
                break;
        }
    }

    if (((ret == LOS_OK) && (state == STAT_TOKEN_IN)) || (state == STAT_INIT)) {
        ret = OsCmdParseOneToken(cmdParsed, count, token);
    }

    return ret;
}
//解析cmd命令,将关键字,参数分离出来
LITE_OS_SEC_TEXT_MINOR UINT32 OsCmdParse(CHAR *cmdStr, CmdParsed *cmdParsed)
{
    if ((cmdStr == NULL) || (cmdParsed == NULL) || (strlen(cmdStr) == 0)) {
        return (UINT32)OS_ERROR;
    }
    return OsCmdTokenSplit(cmdStr, ' ', cmdParsed);
}

