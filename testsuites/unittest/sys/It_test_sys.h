/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific prior written
 * permission.
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
#ifndef _IT_TEST_SYS_H
#define _IT_TEST_SYS_H

#include "libgen.h"
#include "setjmp.h"
#include "fenv.h"
#include "float.h"
#include "math.h"
#include "regex.h"
#include "locale.h"
#include "pwd.h"
#include "osTest.h"
#define FE_INVALID 1
#define FE_DIVBYZERO 4
#define FE_OVERFLOW 8
#define FE_UNDERFLOW 16
#define FE_INEXACT 32
#define FE_ALL_EXCEPT 0
#define FE_TONEAREST 0
#define FE_DOWNWARD 0x400
#define FE_UPWARD 0x800
#define FE_TOWARDZERO 0xc00

extern VOID ItTestSys001(VOID);
extern VOID IT_TEST_SYS_002(VOID);
extern VOID IT_TEST_SYS_003(VOID);
extern VOID ItTestSys004(VOID);
extern VOID ItTestSys005(VOID);
extern VOID ItTestSys006(VOID);
extern VOID ItTestSys007(VOID);
extern VOID ItTestSys008(VOID);
extern VOID ItTestSys009(VOID);
extern VOID ItTestSys010(VOID);
extern VOID IT_TEST_SYS_011(VOID);
extern VOID ItTestSys012(VOID);
extern VOID ItTestSys013(VOID);
extern VOID ItTestSys014(VOID);
extern VOID ItTestSys015(VOID);
extern VOID ItTestSys016(VOID);
extern VOID ItTestSys017(VOID);
extern VOID ItTestSys018(VOID);
extern VOID ItTestSys019(VOID);
extern VOID ItTestSys020(VOID);
extern VOID ItTestSys021(VOID);
extern VOID ItTestSys022(VOID);
extern VOID ItTestSys023(VOID);
extern VOID ItTestSys024(VOID);
extern VOID ItTestSys025(VOID);
extern VOID ItTestSys026(VOID);
extern VOID ItTestSys027(VOID);
extern VOID IT_TEST_SYS_028(VOID);
extern VOID ItTestSys029(VOID);
extern VOID IT_TEST_SYS_030(VOID);
extern VOID IT_TEST_SYS_031(VOID);
#endif
