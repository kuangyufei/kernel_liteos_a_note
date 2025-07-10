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

#ifndef _SHCMDPARSE_H
#define _SHCMDPARSE_H

#include "string.h"
#include "show.h"

#ifdef  __cplusplus
#if  __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief   命令解析后的信息结构体
 * @details 存储命令解析后的关键信息，包括参数数量、命令类型、命令关键字和参数数组
 */
typedef struct {
    unsigned int paramCnt;          /* 参数数量，记录命令中包含的参数个数 */
    CmdType      cmdType;           /* 命令类型，用于判断命令关键字的类型 */
    char cmdKeyword[CMD_KEY_LEN];   /* 命令关键字字符串，存储解析出的命令名称 */
    char *paramArray[CMD_MAX_PARAS];/* 参数数组，存储指向各参数字符串的指针数组 */
} CmdParsed;

extern unsigned int OsCmdParse(char *cmdStr, CmdParsed *cmdParsed);
extern char *OsCmdParseStrdup(const char *str);
extern unsigned int OsCmdParseOneToken(CmdParsed *cmdParsed, unsigned int index, const char *token);
extern unsigned int OsCmdTokenSplit(char *cmdStr, char split, CmdParsed *cmdParsed);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* _SHCMDPARSE_H */
