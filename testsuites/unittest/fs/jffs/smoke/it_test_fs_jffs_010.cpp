/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021, Huawei Device Co., Ltd. All rights reserved.
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

#include "It_fs_jffs.h"
#include <signal.h>

#define RECV_SIGNUM    5U

#define RTSIG0    (SIGRTMIN + 7)
#define RTSIG1    (SIGRTMIN + 6)
#define ERRSIG    (SIGRTMAX + 10)

static int m_sig[RECV_SIGNUM] = {0};
static char *m_recvData = NULL;
char m_msg[] = "hello sigqueue";
static int m_recvCount = 0;

void Handler(int sig, siginfo_t *info, void *ctx)
{
    m_sig[m_recvCount] = sig;
    m_recvData = (char *)info->si_value.sival_ptr;
    m_recvCount++;
}

void SonProcess0(pid_t pid)
{
    union sigval msg;

    msg.sival_ptr = m_msg;
    sigqueue(pid, RTSIG1, msg);
    sigqueue(pid, RTSIG0, msg);
    sigqueue(pid, RTSIG1, msg);

    sleep(3);
    exit(0);
}

static int TestCase0(void)
{
    pid_t pid0 = -1;
    pid_t pid1 = -1;
    int ret = -1;
    struct sigaction act = {0};
    act.sa_sigaction = Handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    int i = 0;
    int expectSig[RECV_SIGNUM] = {RTSIG1, RTSIG0, RTSIG1};

    ret = sigaction(RTSIG1, &act, NULL);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);

    ret = sigaction(RTSIG0, &act, NULL);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);

    pid1 = getpid();
    pid0 = fork();
    if (pid0 == 0) {
        SonProcess0(pid1);
    } else {
        sleep(2);
        for (i = 0; i<m_recvCount; i++) {
            ICUNIT_ASSERT_EQUAL(m_sig[i], expectSig[i], m_sig[m_recvCount]);
        }

        ret = strcmp(m_recvData, m_msg);
        ICUNIT_ASSERT_EQUAL(ret, JFFS_NO_ERROR, ret);
    }

    return JFFS_NO_ERROR;

EXIT:
    return JFFS_IS_ERROR;
}

static int TestCase1(void)
{
    int ret = -1;
    pid_t errPid = -1;
    int sig = 0;
    union sigval msg;

    /* ESRCH */
    sig = RTSIG0;
    ret = sigqueue(errPid, sig, msg);
    ICUNIT_GOTO_EQUAL(errno, ESRCH, ret, EXIT);

    /* EINVAL */
    sig = ERRSIG;
    ret = sigqueue(getpid(), sig, msg);
    ICUNIT_GOTO_EQUAL(errno, EINVAL, ret, EXIT);

    return JFFS_NO_ERROR;

EXIT:
    return JFFS_IS_ERROR;
}

static int TestCase(void)
{
    int ret = -1;

    ret = TestCase0();
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = TestCase1();
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;

EXIT:
    return JFFS_IS_ERROR;
}

void It_Test_Fs_Jffs_010(void)
{
    TEST_ADD_CASE("It_Test_Fs_Jffs_010", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}

