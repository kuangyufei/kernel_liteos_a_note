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

#include "It_los_swtmr.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

static VOID SwtmrF01(UINT32 arg)
{
    UINT32 ret;

    TEST_HwiClear(HWI_NUM_TEST);
    ret = LOS_SwtmrStart(g_swTmrID1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    g_testCount1++;
    return;

EXIT:
    TEST_HwiDelete(HWI_NUM_TEST);
    ret = LOS_SwtmrDelete(g_swTmrID1);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_ERRNO_SWTMR_HWI_ACTIVE, ret);
}

static VOID SwtmrF02(UINT32 arg)
{
    g_testCount1++;
    return;
}


static UINT32 Testcase(VOID)
{
    UINT32 ret;
    HWI_PRIOR_T hwiPrio = 3;
    HWI_MODE_T hwiMode;
    HWI_ARG_T arg = 0;

    g_testCount1 = 0;
    ret = LOS_SwtmrCreate(1, LOS_SWTMR_MODE_ONCE, (SWTMR_PROC_FUNC)SwtmrF02, &g_swTmrID1, arg);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT3);
    hwiMode = 0;
    ret = TEST_HwiCreate(HWI_NUM_TEST, hwiPrio, hwiMode, (HWI_PROC_FUNC)SwtmrF01, (HwiIrqParam *)arg);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT3);
#ifdef LOSCFG_KERNEL_SMP
    HalIrqSetAffinity(HWI_NUM_TEST, CPUID_TO_AFFI_MASK(ArchCurrCpuid()));
#endif
    TestHwiTrigger(HWI_NUM_TEST);
    LOS_TaskDelay(5);                                        // 5, set delay time
    ICUNIT_GOTO_EQUAL(g_testCount1, 2, g_testCount1, EXIT5); // 2, The expected value
    TEST_HwiDelete(HWI_NUM_TEST);
EXIT5:
#if SELF_DELETED
    ret = LOS_SwtmrDelete(g_swTmrID1);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
#endif
    TEST_HwiDelete(HWI_NUM_TEST);
    return LOS_OK;

EXIT3:
    ret = LOS_SwtmrDelete(g_swTmrID1);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    TEST_HwiDelete(HWI_NUM_TEST);
    return LOS_OK;
}

VOID ItLosSwtmr039(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItLosSwtmr039", Testcase, TEST_LOS, TEST_SWTMR, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */

