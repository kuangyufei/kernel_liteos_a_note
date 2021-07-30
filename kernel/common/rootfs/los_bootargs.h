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

#define BYTES_PER_GBYTE         (1 << 30)
#define BYTES_PER_MBYTE         (1 << 20)
#define BYTES_PER_KBYTE         (1 << 10)
#define COMMAND_LINE_ADDR       (LOSCFG_BOOTENV_ADDR * BYTES_PER_KBYTE)
#define COMMAND_LINE_SIZE       1024
#define MAX_ARGS_NUM            100
#ifdef LOSCFG_STORAGE_EMMC
#define EMMC_SEC_SIZE           512
#endif

struct BootArgs {
    CHAR *argName;
    CHAR *argValue;
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