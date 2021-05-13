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

static UINT32 TestCase(VOID)
{
    int retval, status;
    sigset_t set, oldset;

    int fpid = fork();
    if (fpid == 0) {
        retval = sigemptyset(&set);
        ICUNIT_ASSERT_NOT_EQUAL(retval, -1, retval);
        retval = sigemptyset(&oldset);
        ICUNIT_ASSERT_NOT_EQUAL(retval, -1, retval);
        retval = sigaddset(&set, SIGTERM);
        ICUNIT_ASSERT_NOT_EQUAL(retval, -1, retval);
        printf("----------------------------------\n");
        retval = sigprocmask(666, &set, &oldset); // 666, invalid param
        ICUNIT_ASSERT_EQUAL(retval, -1, retval);
        ICUNIT_ASSERT_EQUAL(errno, EINVAL, errno);
        printf("----------------------------------\n");
        retval = sigprocmask(SIG_BLOCK, (sigset_t *)1, &oldset);
        ICUNIT_ASSERT_EQUAL(retval, -1, retval);
        ICUNIT_ASSERT_EQUAL(errno, EFAULT, errno);
        printf("----------------------------------\n");
        retval = sigprocmask(SIG_BLOCK, &set, &oldset);
        ICUNIT_ASSERT_EQUAL(retval, 0, retval);

        exit(0);
    }

    retval = waitpid(fpid, &status, 0);
    ICUNIT_ASSERT_EQUAL(retval, fpid, retval);
    ICUNIT_ASSERT_EQUAL(WEXITSTATUS(status), 0, WEXITSTATUS(status));

    return LOS_OK;
}

void ItPosixSignal037(void)
{
    TEST_ADD_CASE(__FUNCTION__, TestCase, TEST_POSIX, TEST_SIGNAL, TEST_LEVEL0, TEST_FUNCTION);
}