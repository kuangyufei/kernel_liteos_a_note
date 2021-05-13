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
static UINT32 g_tick;
static UINT16 g_swTmrID71;
static VOID SwtmrF01(UINT32 arg)
{
    UINT32 ret;
    UINT32 tickTask1;

    if (arg != 0xffff) {
        return;
    }

    tickTask1 = (UINT32)LOS_TickCountGet();
    g_testCount++;
    return;
EXIT:
    LOS_SwtmrDelete(g_swTmrID71);
    return;
}
static UINT32 Testcase(VOID)
{
    UINT32 ret;
    g_testCount = 0;
    // 10, Timeout interval of a periodic software timer.
    ret = LOS_SwtmrCreate(10, LOS_SWTMR_MODE_ONCE, SwtmrF01, &g_swTmrID71, 0xffff);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = LOS_SwtmrTimeGet(g_swTmrID71, &g_tick); // Gets the number of ticks left in the timer before it starts
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_SWTMR_NOT_STARTED, ret, EXIT);

    ret = 0;
    ret = LOS_SwtmrStop(g_swTmrID71); // If the timer is not started, stop
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_SWTMR_NOT_STARTED, ret, EXIT);

    ret = LOS_SwtmrStart(g_swTmrID71); // Start timer
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = 0;
    ret = LOS_TaskDelay(5); // 5, set delay time
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = LOS_SwtmrTimeGet(g_swTmrID71, &g_tick); // Gets the number of ticks remaining for the timer being timed
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = LOS_SwtmrStop(g_swTmrID71); // Stops the started timer
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = LOS_SwtmrTimeGet(g_swTmrID71, &g_tick); // Gets the number of ticks left in the stopped timer
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_SWTMR_NOT_STARTED, ret, EXIT);

    ret = LOS_SwtmrStart(g_swTmrID71); // Start the timer again
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = LOS_TaskDelay(12); // 12, set delay time
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
    ICUNIT_GOTO_EQUAL(g_testCount, 1, g_testCount, EXIT);

    ret = LOS_SwtmrDelete(g_swTmrID71);
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_SWTMR_ID_INVALID, ret, EXIT);

    // After a single timer executes the callback function, the timer will be deleted, and the timer status will change
    // to OS_SWTMR_STATUS_UNUSED
    ret = LOS_SwtmrTimeGet(g_swTmrID71, &g_tick);
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_SWTMR_ID_INVALID, ret, EXIT);

#if SELF_DELETED
    ret = LOS_SwtmrDelete(g_swTmrID71);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    return LOS_OK;
#endif

    return LOS_OK;
EXIT:
    ret = LOS_SwtmrDelete(g_swTmrID71);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    return LOS_OK;
}

VOID ItLosSwtmr071(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItLosSwtmr071", Testcase, TEST_LOS, TEST_SWTMR, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */

