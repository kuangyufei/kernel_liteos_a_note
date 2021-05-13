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

static VOID NestingPrioHigh(INT32 irq, VOID *data)
{
    g_intrHandleEnd = 1;
    UINT32 temp[0x100][2] = {0};
    memset(temp, 1, sizeof(UINT32) * 0x100 * 2); // 2, buffer size.
}

static VOID TaskF01(VOID)
{
    HalIrqUnmask(HIGHT_PRIO_INTR);
    HalIrqPending(HIGHT_PRIO_INTR);
}
static UINT32 Testcase(VOID)
{
    UINT32 ret, taskID;

    TSK_INIT_PARAM_S task1 = { 0 };

    // 4, Task priority calculation.
    TEST_TASK_PARAM_INIT(task1, "ItLosHwiNest010", (TSK_ENTRY_FUNC)TaskF01, TASK_PRIO_TEST - 4);
    task1.uwStackSize = 0x2000;
    LOS_HwiCreate(HIGHT_PRIO_INTR, SPI_HIGHT_PRIO, 0, (HWI_PROC_FUNC)NestingPrioHigh, &g_nestingPara);
    HalIrqUnmask(HIGHT_PRIO_INTR);

    ret = LOS_TaskCreate(&taskID, &task1);

    while (g_intrHandleEnd == 0) {
        LOS_TaskDelay(10); // 10, set delay time.
    }

    ICUNIT_GOTO_EQUAL(g_intrHandleEnd, 1, g_intrHandleEnd, EXIT);

EXIT:
    LOS_HwiDelete(HIGHT_PRIO_INTR, &g_nestingPara);
    HalIrqMask(HIGHT_PRIO_INTR);
    g_intrHandleEnd = 0;

    return LOS_OK;
}


VOID ItLosHwiNest006(VOID)
{
    TEST_ADD_CASE("ItLosHwiNest006", Testcase, TEST_LOS, TEST_HWI, TEST_LEVEL1, TEST_FUNCTION);
}
#endif
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
