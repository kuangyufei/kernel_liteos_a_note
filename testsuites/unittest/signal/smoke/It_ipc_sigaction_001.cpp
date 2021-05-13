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


static int g_actionCnt1;
static void CatchAction1(int param)
{
    g_actionCnt1++;
    printf("---------%d---signum-%d-----\n", g_actionCnt1, param);
}

static UINT32 Testcase(VOID)
{
    int spid, ret;
    struct sigaction act, oldact;
    act.sa_handler = CatchAction1;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    spid = fork();
    ICUNIT_ASSERT_NOT_EQUAL(spid, -1, spid);
    if (spid == 0) {
        ret = sigaction(SIGCHLD, &act, &oldact);
        if (ret == -1) {
            exit(-1);
        }
        spid = fork();
        if (spid == 0) {
            spid = getppid();
            kill(spid, SIGCHLD);
            usleep(1000 * 10 * 10); // 1000, 10, 10, Used to calculate the delay time.
            exit(0);
        }
        if (spid == -1) {
            exit(-1);
        }
        ret = 0;
        while (ret == 0) {
            ret = waitpid(spid, NULL, WNOHANG);
        }

        ret = sigaction(SIGCHLD, &oldact, NULL);
        if (ret == -1) {
            exit(6); // 6, the value of son process unexpect exit, convenient to debug
        }
        sleep(1);
        printf("---son--cnt check----%d--------\n", g_actionCnt1);
        exit(g_actionCnt1);
    }
    wait(&ret);
    ICUNIT_ASSERT_EQUAL(WEXITSTATUS(ret), 2, WEXITSTATUS(ret)); // 2, assert that function Result is equal to this.
    return LOS_OK;
}

VOID ItIpcSigaction001(void)
{
    TEST_ADD_CASE(__FUNCTION__, Testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
