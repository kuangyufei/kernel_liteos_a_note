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
    if (g_testCount != 2) { // 2, Execute if g_testCount is 2
        return;
    }
    g_testCount++;
    return;
}

static VOID HwiF01(VOID)
{
    UINT32 ret;
    UINT32 hwiNum;
#ifdef TEST3516A
    hwiNum = HWI_NUM_INT32;
#elif defined TEST3518E || defined TEST3516CV300
    hwiNum = HWI_NUM_INT31;
#elif defined TEST3559
    hwiNum = HWI_NUM_INT32;
#elif defined TEST3559A
    hwiNum = HWI_NUM_INT69;
#elif defined TEST3559A_M7
    hwiNum = HWI_NUM_INT11;
#elif defined TESTPBXA9 || defined TESTVIRTA53
    hwiNum = HWI_NUM_INT60;
#elif defined TEST3516DV300
    hwiNum = HWI_NUM_INT60;
#endif

    TEST_HwiClear(HWI_NUM_TEST3);
    g_testCount++;

    // 2, Timeout interval of a periodic software timer.
    ret = LOS_SwtmrCreate(2, LOS_SWTMR_MODE_ONCE, SwtmrF01, &g_swTmrID1, 0xffff);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    g_testCount++;

    return;
}

static UINT32 Testcase(VOID)
{
    UINT32 ret;
    g_testCount = 0;

    ret = LOS_HwiCreate(HWI_NUM_TEST3, 1, 0, HwiF01, 0);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    TestHwiTrigger(HWI_NUM_TEST3);

    TestExtraTaskDelay(2); // 2, set delay time

    ret = LOS_SwtmrStart(g_swTmrID1);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = LOS_TaskDelay(5); // 5, set delay time
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ICUNIT_GOTO_EQUAL(g_testCount, 3, g_testCount, EXIT); // 3, The expected value

EXIT:
    TEST_HwiDelete(HWI_NUM_TEST3);
#if SELF_DELETED
    ret = LOS_SwtmrDelete(g_swTmrID1);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
#endif
    return LOS_OK;
}

VOID ItLosSwtmr022(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItLosSwtmr022", Testcase, TEST_LOS, TEST_SWTMR, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */

