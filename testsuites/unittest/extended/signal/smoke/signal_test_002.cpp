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
#include "it_test_signal.h"
#include "signal.h"
#include "sys/wait.h"

static int g_sigCount = 0;
static void SigPrint(int sig)
{
    (void)sig;
    g_sigCount++;
}

static int TestRaiseIgnore(void)
{
    int sig = SIGPWR;
    void *ret;
    int retValue;

    g_sigCount = 0;
    // trigger one
    ret = (void *)signal(sig, SigPrint);
    ICUNIT_ASSERT_NOT_EQUAL(ret, NULL, ret);

    retValue = raise(sig);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);

    usleep(1000); // 1000, Used to calculate the delay time.
    // trigger ignore
    ret = (void *)signal(sig, SIG_IGN);
    ICUNIT_ASSERT_NOT_EQUAL(ret, NULL, ret);

    retValue = raise(sig);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);
    // trigger one
    ret = (void *)signal(sig, SigPrint);
    ICUNIT_ASSERT_NOT_EQUAL(ret, NULL, ret);
    retValue = raise(sig);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);
    return g_sigCount;
}

static int TestCase(void)
{
    int count;

    count = TestRaiseIgnore();
    // the signal should be triggered twice
    ICUNIT_ASSERT_EQUAL(count, 2, count); // 2, assert that function Result is equal to this.
    return 0;
}

void ItPosixSignal002(void)
{
    TEST_ADD_CASE("IT_POSIX_SIGNAL_002", TestCase, TEST_POSIX, TEST_SIGNAL, TEST_LEVEL0, TEST_FUNCTION);
}
