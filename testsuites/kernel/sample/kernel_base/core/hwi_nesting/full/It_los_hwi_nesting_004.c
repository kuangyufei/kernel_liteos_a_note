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


#ifdef LOSCFG_ARCH_INTERRUPT_PREEMPTION
static HwiIrqParam g_nestingPara;

/* middle in -> middle out -> low in -> low out -> hight in -> hight out */
static UINT32 g_expect[] = {
    MIDDLE_PRIO_INTR,
    0x80000000 | MIDDLE_PRIO_INTR,
    LOW_PRIO_INTR,
    0x80000000 | LOW_PRIO_INTR,
    HIGHT_PRIO_INTR,
    0x80000000 | HIGHT_PRIO_INTR
};

static VOID NestingPrioLow(INT32 irq, VOID *data)
{
    volatile UINT64 nestingDelay = 10000000; // 10000000, The loop frequency.

    RecordIntrTrace(LOW_PRIO_INTR, INTR_ENTRY);

    HalIrqPending(HIGHT_PRIO_INTR); /* pending hight prio interrupt */
    while (nestingDelay-- > 0);

    RecordIntrTrace(LOW_PRIO_INTR, INTR_EXIT);

    g_intrHandleEnd = 1;
}

static VOID NestingPrioMiddle(INT32 irq, VOID *data)
{
    volatile UINT64 nestingDelay = 10000000; // 10000000, The loop frequency.

    RecordIntrTrace(MIDDLE_PRIO_INTR, INTR_ENTRY);

    HalIrqPending(LOW_PRIO_INTR); /* pending low prio interrupt */
    while (nestingDelay-- > 0);

    RecordIntrTrace(MIDDLE_PRIO_INTR, INTR_EXIT);
}

static VOID NestingPrioHigh(INT32 irq, VOID *data)
{
    RecordIntrTrace(HIGHT_PRIO_INTR, INTR_ENTRY);
    RecordIntrTrace(HIGHT_PRIO_INTR, INTR_EXIT);
}

static UINT32 Testcase(VOID)
{
    UINT32 ret;

    LOS_HwiCreate(HIGHT_PRIO_INTR, SPI_MIDDLE_PRIO, 0, (HWI_PROC_FUNC)NestingPrioHigh, &g_nestingPara);
    LOS_HwiCreate(MIDDLE_PRIO_INTR, SPI_MIDDLE_PRIO, 0, (HWI_PROC_FUNC)NestingPrioMiddle, &g_nestingPara);
    LOS_HwiCreate(LOW_PRIO_INTR, SPI_MIDDLE_PRIO, 0, (HWI_PROC_FUNC)NestingPrioLow, &g_nestingPara);
    HalIrqUnmask(HIGHT_PRIO_INTR);
    HalIrqUnmask(MIDDLE_PRIO_INTR);
    HalIrqUnmask(LOW_PRIO_INTR);

    HalIrqPending(MIDDLE_PRIO_INTR); /* pending middle prio interrupt */

    while (g_intrHandleEnd == 0) {
        LOS_TaskDelay(10); // 10, set delay time.
    }

    ret = CheckIntrTrace(g_expect, sizeof(g_expect));
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

EXIT:
    LOS_HwiDelete(HIGHT_PRIO_INTR, &g_nestingPara);
    LOS_HwiDelete(MIDDLE_PRIO_INTR, &g_nestingPara);
    LOS_HwiDelete(LOW_PRIO_INTR, &g_nestingPara);
    HalIrqMask(HIGHT_PRIO_INTR);
    HalIrqMask(MIDDLE_PRIO_INTR);
    HalIrqMask(LOW_PRIO_INTR);
    g_intrHandleEnd = 0;
    ResetIntrTrace();

    return LOS_OK;
}


VOID ItLosHwiNest004(VOID)
{
    TEST_ADD_CASE("ItLosHwiNest004", Testcase, TEST_LOS, TEST_HWI, TEST_LEVEL1, TEST_FUNCTION);
}
#endif
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
