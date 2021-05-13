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

#include "It_posix_pthread.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

static VOID *PthreadF01(void *argument)
{
    _pthread_data *info = NULL;

    g_testCount++;
    dprintf("!!!!!!!!!\n");
    dprintf("%x\n", TASK_STACK_SIZE_TEST);
    info = pthread_get_self_data();

    // 3, here assert the result.
    ICUNIT_GOTO_EQUAL(info->attr.stacksize, (TASK_STACK_SIZE_TEST + 3) & ~3, info->attr.stacksize, EXIT); // failed
    ICUNIT_GOTO_EQUAL(info->attr.stacksize_set, 1, info->attr.stacksize_set, EXIT);
    // 3, here assert the result.
    ICUNIT_GOTO_EQUAL(info->task->stackSize, (TASK_STACK_SIZE_TEST + 3) & ~3, info->task->stackSize, EXIT);

    g_testCount++;

EXIT:
    return (void *)9; // 9, here set value about the return status.
}

static UINT32 Testcase(VOID)
{
    pthread_t newTh;
    int ret;
    pthread_attr_t attr;
    size_t stacksize;
    size_t stacksize2;

    g_testCount = 0;

    ret = pthread_attr_init(&attr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    stacksize = TASK_STACK_SIZE_TEST;
    ret = pthread_attr_setstacksize(&attr, stacksize);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_create(&newTh, &attr, PthreadF01, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

#ifdef TEST3559A_M7
    ret = LOS_TaskDelay(10); // 10, delay for Timing control.
#else
    ret = LOS_TaskDelay(10); // 10, delay for Timing control.
#endif
    ICUNIT_ASSERT_EQUAL(g_testCount, 2, g_testCount); // 2, here assert the result.

    ret = pthread_join(newTh, NULL);

    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_attr_getstacksize(&attr, &stacksize2);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(stacksize2, TASK_STACK_SIZE_TEST, stacksize2);

    attr.stacksize_set = 1;
    attr.stacksize = LOS_TASK_MIN_STACK_SIZE - 2 * sizeof(UINTPTR); // 2, Set reasonable stack size.
    ret = pthread_create(&newTh, &attr, PthreadF01, NULL);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    ret = pthread_attr_destroy(&attr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    return PTHREAD_NO_ERROR;
}

VOID ItPosixPthread045(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItPosixPthread045", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
