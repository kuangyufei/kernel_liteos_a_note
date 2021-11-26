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
#ifndef _IT_PTHREAD_TEST_H
#define _IT_PTHREAD_TEST_H

#include "osTest.h"
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>

#define SLEEP_AND_YIELD(tick) usleep((tick)*10 * 1000)

#include "sys/syscall.h"

static inline int Syscall(int nbr, int parm1, int parm2, int parm3, int parm4)
{
    register int reg7 __asm__("r7") = (int)(nbr);
    register int reg3 __asm__("r3") = (int)(parm4);
    register int reg2 __asm__("r2") = (int)(parm3);
    register int reg1 __asm__("r1") = (int)(parm2);
    register int reg0 __asm__("r0") = (int)(parm1);

    __asm__ __volatile__("svc 0" : "=r"(reg0) : "r"(reg7), "r"(reg0), "r"(reg1), "r"(reg2), "r"(reg3) : "memory");

    return reg0;
}
extern INT32 g_iCunitErrCode;
extern INT32 g_iCunitErrLineNo;

extern void ItTestPthread001(void);
extern void ItTestPthread002(void);
extern void ItTestPthread003(void);
extern void ItTestPthread004(void);
extern void ItTestPthread005(void);
extern void ItTestPthread006(void);
extern void ItTestPthread007(void);
extern void ItTestPthread008(void);
extern void ItTestPthread009(void);
extern void ItTestPthread010(void);
extern void ItTestPthread012(void);
extern void ItTestPthread011(void);
extern void ItTestPthread013(void);
extern void ItTestPthread014(void);
extern void ItTestPthread015(void);
extern void ItTestPthread016(void);
extern void ItTestPthread017(void);
extern void ItTestPthread018(void);
extern void ItTestPthread019(void);
extern void ItTestPthreadAtfork001(void);
extern void ItTestPthreadAtfork002(void);
extern void ItTestPthreadOnce001(void);
extern void ItTestPthreadCond001(void);
extern void ItTestPthreadCond002(void);
extern void ItTestPthreadCond003(void);
extern void ItTestPthreadCond004(void);
#endif
