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

#include "It_hwi_nesting.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

#define RECORD_SIZE 100

UINT32 g_intrTrace[RECORD_SIZE];
UINT32 g_traceIdx = 0;
UINT32 g_intrHandleEnd = 0;

VOID RecordIntrTrace(INT32 irq, INT32 direction)
{
    g_intrTrace[g_traceIdx++] = irq | (direction << 31); // 31, Bit shift mark.
}

VOID ResetIntrTrace(VOID)
{
    g_traceIdx = 0;
    memset(g_intrTrace, 0, RECORD_SIZE);
}

UINT32 CheckIntrTrace(UINT32 *expect, UINT32 num)
{
    UINT32 ret = LOS_OK;
    INT32 idx = 0;

    if ((expect == NULL) || (num > RECORD_SIZE)) {
        return LOS_NOK;
    }

    for (; idx < g_traceIdx; idx++) {
        if (expect[idx] != g_intrTrace[idx]) {
            ret = LOS_NOK;
            break;
        }
    }

    if (ret == LOS_NOK) {
        for (idx = 0; idx < g_traceIdx; idx++) {
            dprintf("%u ", g_intrTrace[idx]);
        }
    }
    dprintf("\n");

    return ret;
}

VOID ItSuiteHwiNesting(VOID)
{
#if defined(LOSCFG_TEST_FULL)
#ifdef LOSCFG_ARCH_INTERRUPT_PREEMPTION
    ItLosHwiNest001();
    ItLosHwiNest002();
    ItLosHwiNest003();
    ItLosHwiNest004();
    ItLosHwiNest005();
    ItLosHwiNest006();
    ItLosHwiNest007();
#endif
#endif
}


#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
