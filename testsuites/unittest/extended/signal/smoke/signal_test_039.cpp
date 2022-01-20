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
#include "time.h"
static const int INVALID_WHICH = 666;
#define IS32BIT(x) !((x) + 0x80000000ULL >> 32)

static UINT32 TestCase(VOID)
{
    int ret, status;
    struct itimerval newValue = { 0 };
    struct itimerval oldValue = { 0 };
    struct itimerval test;

    int fpid = fork();
    if (fpid == 0) {
        bool flag = IS32BIT(test.it_interval.tv_sec);
        ret = setitimer(INVALID_WHICH, &newValue, &oldValue);
        printf("ret=%x--flag=%d-----\n", ret, flag);
        if (ret != -1) {
            exit(ret);
        }
        if (errno != EINVAL) {
            exit(errno);
        }
        printf("---------------\n");

        exit(0);
    }

    /*  // EFAULT no support
    ret = setitimer(ITIMER_VIRTUAL, NULL, &oldValue);
    printf("ret = %d    errno = %d   EFAULT = %d\n", ret, errno, EFAULT);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(errno, EFAULT, errno); */

    ret = waitpid(fpid, &status, 0);
    ICUNIT_ASSERT_EQUAL(ret, fpid, ret);
    ICUNIT_ASSERT_EQUAL(WEXITSTATUS(status), 0, WEXITSTATUS(status));
    return LOS_OK;
}

void ItPosixSignal039(void)
{
    TEST_ADD_CASE(__FUNCTION__, TestCase, TEST_POSIX, TEST_SIGNAL, TEST_LEVEL0, TEST_FUNCTION);
}
