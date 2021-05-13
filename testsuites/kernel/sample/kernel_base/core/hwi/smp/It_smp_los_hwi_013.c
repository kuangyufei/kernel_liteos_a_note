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

#include "It_smp_hwi.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */
#if (LOSCFG_KERNEL_SMP == YES)

static VOID SwtmrF01(void)
{
    LOS_AtomicInc(&g_testCount);
}
static VOID HwiF01(void)
{
    UINT32 ret, i;
    for (i = 0; i < TRandom() % 100; i++) { // Get a random number between 0 and 100
    }

    ret = LOS_SwtmrDelete(g_swTmrID);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);
    TestDumpCpuid();
    return;
}

static UINT32 Testcase(void)
{
    UINT32 ret, i, j;

    g_testCount = 0;
    g_testPeriod = 10; // 10, Timeout interval of a periodic software timer.

    ret = LOS_HwiCreate(HWI_NUM_TEST, 1, 0, (HWI_PROC_FUNC)HwiF01, 0);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    HalIrqSetAffinity(HWI_NUM_TEST, CPUID_TO_AFFI_MASK((ArchCurrCpuid() + 1) % LOSCFG_KERNEL_CORE_NUM)); // other cpu

    for (i = 0; i < LOOP; i++) {
        ret = LOS_SwtmrCreate(g_testPeriod, LOS_SWTMR_MODE_PERIOD, (SWTMR_PROC_FUNC)SwtmrF01, &g_swTmrID, 0);
        ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

        TestHwiTrigger(HWI_NUM_TEST); // delete swtmr in hwi on other cpu

        for (j = 0; j < TRandom() % 500; j++) { // Get a random number between 0 and 500
            LOS_SwtmrStart(g_swTmrID); // start swtmr in task on current cpu
        }

        LOS_TaskDelay(g_testPeriod + 1);

        ICUNIT_GOTO_EQUAL(g_testCount, 0, g_testCount, EXIT);
    }

EXIT:
    ret = LOS_HwiDelete(HWI_NUM_TEST, NULL);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = LOS_SwtmrDelete(g_swTmrID);
    ICUNIT_GOTO_NOT_EQUAL(ret, LOS_OK, ret, EXIT);
    return LOS_OK;
}

VOID ItSmpLosHwi013(VOID)
{
    TEST_ADD_CASE("ItSmpLosHwi013", Testcase, TEST_LOS, TEST_HWI, TEST_LEVEL1, TEST_FUNCTION);
}
#endif
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
