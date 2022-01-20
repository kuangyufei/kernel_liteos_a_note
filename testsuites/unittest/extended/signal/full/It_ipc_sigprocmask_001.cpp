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


static UINT32 Testcase(VOID)
{
    int spid = 0;
    int retval = 0;

    sigset_t set, oldset;
    spid = fork();
    ICUNIT_ASSERT_NOT_EQUAL(spid, -1, spid);
    if (spid == 0) {
        retval = sigemptyset(&set);
        if (retval == -1) {
            exit(-1);
        }
        retval = sigemptyset(&oldset);
        if (retval == -1) {
            exit(-1);
        }
        retval = sigaddset(&set, SIGTERM);
        if (retval == -1) {
            exit(-1);
        }
        retval = sigprocmask(SIG_BLOCK, &set, &oldset);
        if (retval == -1) {
            exit(-1);
        }
        sleep(2); // 2, sleep 2 second
        retval = RED_FLAG;
        printf("son retval check %d\n", retval);
        exit(retval);
    }
    sleep(1);
    kill(spid, SIGTERM);
    wait(&retval);
    printf("father retval check %d\n", WEXITSTATUS(retval));
    ICUNIT_ASSERT_EQUAL(WEXITSTATUS(retval), RED_FLAG, WEXITSTATUS(retval));
    return LOS_OK;
}

VOID ItIpcSigpromask001(void)
{
    TEST_ADD_CASE(__FUNCTION__, Testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
