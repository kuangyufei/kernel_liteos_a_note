/*
 * Copyright (c) 2023-2023 Huawei Device Co., Ltd. All rights reserved.
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
#ifndef _IT_PROCESS_FS_TEST_H
#define _IT_PROCESS_FS_TEST_H

#include <sys/time.h>
#include <sys/types.h>
#include "osTest.h"

extern VOID PrintTest(const CHAR *fmt, ...);

extern std::string GenProcPidPath(int pid);
extern std::string GenProcPidContainerPath(int pid, char *name);

extern void ItProcessFs001(void);
extern void ItProcessFs002(void);
extern void ItProcessFs003(void);
extern void ItProcessFs004(void);
extern void ItProcessFs005(void);
extern void ItProcessFs006(void);
extern void ItProcessFs007(void);
extern void ItProcessFs008(void);
extern void ItProcessFs009(void);
extern void ItProcessFs010(void);
extern void ItProcessFs011(void);
extern void ItProcessFs012(void);
extern void ItProcessFs013(void);
extern void ItProcessFs014(void);
extern void ItProcessFs015(void);
extern void ItProcessFs015(void);
extern void ItProcessFs016(void);
extern void ItProcessFs017(void);
extern void ItProcessFs018(void);
extern void ItProcessFs019(void);
extern void ItProcessFs020(void);
extern void ItProcessFs021(void);
extern void ItProcessFs022(void);
#endif
