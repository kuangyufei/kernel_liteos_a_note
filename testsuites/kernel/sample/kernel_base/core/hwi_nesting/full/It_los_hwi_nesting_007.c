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

#define MAX_RECORD_SIZE 1000
static HwiIrqParam g_nestingPara;
static INT32 g_saveIndex = 0;
static UINT64 g_intPendTime;
static UINT64 g_recordTime[MAX_RECORD_SIZE] = {0};
static VOID NestingPrioHigh(INT32 irq, VOID *data)
{
    UINT64 curTime;
    curTime = LOS_CurrNanosec();
    g_recordTime[g_saveIndex] = curTime - g_intPendTime;
    if (g_saveIndex == MAX_RECORD_SIZE) {
        g_saveIndex = 0;
    }
    dprintf("curTime = %lld, pendTime = %lld \n", curTime, g_intPendTime);
    dprintf("%lld\n", curTime - g_intPendTime);
    dprintf("[swtmr] hwi response time : ##%lld \n", g_recordTime[g_saveIndex]);
    g_saveIndex++;
}

static VOID DumpResult()
{
    UINT32 i, count;
    UINT64 avg, sum;
    sum = 0;
    count = 0;
    for (i = 0; i < MAX_RECORD_SIZE; i++) {
        if (g_recordTime[i] != 0) {
            dprintf("%lld  ");
            sum += g_recordTime[i];
            count++;
        }
    }

    avg = sum / count;
    dprintf("***hwi perf test dump***: \n");
    dprintf("avg = %lld \n");
}

static VOID SwtmrF01(VOID)
{
    g_intPendTime = LOS_CurrNanosec();
    HalIrqPending(HIGHT_PRIO_INTR);
}
static UINT32 Testcase(VOID)
{
    UINT32 ret, swtmrId;

    LOS_HwiCreate(HIGHT_PRIO_INTR, SPI_HIGHT_PRIO, 0, (HWI_PROC_FUNC)NestingPrioHigh, &g_nestingPara);
    HalIrqUnmask(HIGHT_PRIO_INTR);

    // 10, Timing duration of the software timer to be created.
    ret = LOS_SwtmrCreate(10, LOS_SWTMR_MODE_PERIOD, SwtmrF01, &swtmrId,
        0xffff);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    LOS_SwtmrStart(swtmrId);

    /* EXIT time control */
    LOS_TaskDelay(100); // 100, set delay time.

    LOS_SwtmrStop(swtmrId);
    DumpResult();

EXIT:
    LOS_SwtmrDelete(swtmrId);
    LOS_HwiDelete(HIGHT_PRIO_INTR, &g_nestingPara);
    HalIrqMask(HIGHT_PRIO_INTR);

    return LOS_OK;
}


VOID ItLosHwiNest007(VOID)
{
    TEST_ADD_CASE("ItLosHwiNest007", Testcase, TEST_LOS, TEST_HWI, TEST_LEVEL1, TEST_FUNCTION);
}
#endif
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
