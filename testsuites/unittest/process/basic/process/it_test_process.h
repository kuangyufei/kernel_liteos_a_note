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
#ifndef IT_TEST_PROCESS_H
#define IT_TEST_PROCESS_H

#include "osTest.h"
#include "sys/resource.h"
#include "sys/wait.h"

#define WAIT_PARENT_FIRST_TO_RUN(tick) usleep((tick)*10 * 1000) // 10, 1000, wait time.
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
extern int GetCpuCount(void);

extern void Wait(const char *ptr, int scount);
extern void ItTestProcess001(void);
extern void ItTestProcess002(void);
extern void ItTestProcess004(void);
extern void ItTestProcess005(void);
extern void ItTestProcess006(void);
extern void ItTestProcess007(void);
extern void ItTestProcess008(void);
extern void ItTestProcess009(void);
extern void ItTestProcess010(void);
extern void ItTestProcess011(void);
extern void ItTestProcess012(void);
extern void ItTestProcess013(void);
extern void ItTestProcess014(void);
extern void ItTestProcess015(void);
extern void ItTestProcess016(void);
extern void ItTestProcess017(void);
extern void ItTestProcess018(void);
extern void ItTestProcess019(void);
extern void ItTestProcess020(void);
extern void ItTestProcess021(void);
extern void ItTestProcess022(void);
extern void ItTestProcess023(void);
extern void ItTestProcess024(void);
extern void ItTestProcess025(void);
extern void ItTestProcess026(void);
extern void ItTestProcess027(void);
extern void ItTestProcess029(void);
extern void ItTestProcess030(void);
extern void ItTestProcess031(void);
extern void ItTestProcess032(void);
extern void ItTestProcess033(void);
extern void ItTestProcess034(void);
extern void ItTestProcess035(void);
extern void ItTestProcess036(void);
extern void ItTestProcess037(void);
extern void ItTestProcess038(void);
extern void ItTestProcess039(void);
extern void ItTestProcess040(void);
extern void ItTestProcess041(void);
extern void ItTestProcess042(void);
extern void ItTestProcess043(void);
extern void ItTestProcess044(void);
extern void ItTestProcess045(void);
extern void ItTestProcess046(void);
extern void ItTestProcess047(void);
extern void ItTestProcess048(void);
extern void ItTestProcess049(void);
extern void ItTestProcess050(void);
extern void ItTestProcess051(void);
extern void ItTestProcess052(void);
extern void ItTestProcess053(void);
extern void ItTestProcess054(void);
extern void ItTestProcess055(void);
extern void ItTestProcess056(void);
extern void ItTestProcess057(void);
extern void ItTestProcess058(void);
extern void ItTestProcess059(void);
extern void ItTestProcess060(void);
extern void ItTestProcess061(void);
extern void ItTestProcess062(void);
extern void ItTestProcess063(void);
extern void ItTestProcess064(void);
extern void ItTestProcess065(void);
extern void ItTestProcess066(void);
extern void ItTestProcess067(void);
extern void ItTestProcess068(void);
extern void ItTestProcess069(void);
extern void ItTestProcessSmp001(void);
extern void ItTestProcessSmp002(void);
extern void ItTestProcessSmp003(void);
extern void ItTestProcessSmp004(void);
extern void ItTestProcessSmp005(void);
extern void ItTestProcessSmp006(void);
extern void ItTestProcessSmp007(void);
extern void ItTestProcessSmp008(void);
#endif
