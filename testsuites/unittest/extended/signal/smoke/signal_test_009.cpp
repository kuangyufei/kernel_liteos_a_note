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

static int g_sigCount = 0;
static void SigPrint(int sig)
{
    (void)sig;
    g_sigCount++;
}

static void SigPrint1(int sig)
{
    (void)sig;
    g_sigCount += 100; // 100, Used to calculate the progress of the program.
}

static int TestSigSet(void)
{
    sigset_t set = { 0 };
    sigset_t set1 = { 0 };
    sigset_t left = { 0 };
    sigset_t right = { 0 };

    void (*ret)(int);
    int retValue;

    left.__bits[0] = 0x1;
    right.__bits[0] = 0x2;

    retValue = sigandset(&set, &left, &right);
    ICUNIT_ASSERT_EQUAL(set.__bits[0], 0, set.__bits[0]);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);

    left.__bits[0] = 0x11;
    right.__bits[0] = 0x1;

    retValue = sigandset(&set, &left, &right);
    ICUNIT_ASSERT_EQUAL(set.__bits[0], 1, set.__bits[0]);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);

    left.__bits[0] = 0x11;
    right.__bits[0] = 0x11;

    retValue = sigandset(&set, &left, &right);
    ICUNIT_ASSERT_EQUAL(set.__bits[0], 0x11, set.__bits[0]);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);

    retValue = sigorset(&set, &left, &right);
    ICUNIT_ASSERT_EQUAL(set.__bits[0], 0x11, set.__bits[0]);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);

    retValue = sigdelset(&set, 1);
    ICUNIT_ASSERT_EQUAL(set.__bits[0], 0x10, set.__bits[0]);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);

    int sigs;
    sigset(1, SigPrint1);

    retValue = raise(1);
    ICUNIT_ASSERT_EQUAL(g_sigCount, 100, g_sigCount); // 100, assert that function Result is equal to this.
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);

    retValue = sigfillset(&set);
    ICUNIT_ASSERT_EQUAL(set.__bits[0], 0x7fffffff, set.__bits[0]);
    ICUNIT_ASSERT_EQUAL(set.__bits[1], 0xfffffffc, set.__bits[1]);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);

    retValue = sigaddset(&set1, 1);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);

    ret = signal(1, SigPrint);
    ICUNIT_ASSERT_NOT_EQUAL(ret, NULL, ret);
    retValue = sighold(1);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);

    retValue = raise(1);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);
    retValue = sigrelse(1);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);

    retValue = raise(1);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);
    ICUNIT_ASSERT_EQUAL(g_sigCount, 102, g_sigCount); // 102, assert that function Result is equal to this.

    retValue = sigisemptyset(&set1);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);

    retValue = siginterrupt(1, 0);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);
    return 0;
}


void ItPosixSignal009(void)
{
    TEST_ADD_CASE(__FUNCTION__, TestSigSet, TEST_POSIX, TEST_SIGNAL, TEST_LEVEL0, TEST_FUNCTION);
}
