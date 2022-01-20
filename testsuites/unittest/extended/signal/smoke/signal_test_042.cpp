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

#include "pthread.h"

static int SigtimedwaitFailTest(const sigset_t *set, siginfo_t *info, const struct timespec *timeout,
    unsigned int expectErrno)
{
    int ret;
    errno = 0;
    if (expectErrno == 0) {
        ret = sigtimedwait(set, info, timeout);
        if (ret != 0) {
            printf("err: line %d, errno %d\n", __LINE__, errno);
            return errno;
        }
        return 0;
    }
    ret = sigtimedwait(set, info, timeout);
    if (ret == 0) {
        printf("err: line %d\n", __LINE__);
        return -1;
    }
    if (errno != expectErrno) {
        printf("err: line %d\n", __LINE__);
        return errno;
    }
    return 0;
}

static UINT32 TestCase()
{
    int ret;
    struct timespec time1 = { 0, 50 };
    sigset_t set;

    sigemptyset(&set);
    sigprocmask(SIG_SETMASK, &set, NULL);

    printf("check invlid sigset ...\n");
    int rt = sigaddset(&set, 0);
    ICUNIT_ASSERT_EQUAL(rt, -1, rt);
    ICUNIT_ASSERT_EQUAL(errno, EINVAL, errno);
    rt = sigaddset(&set, 100); // 100, set signal num
    ICUNIT_ASSERT_EQUAL(rt, -1, rt);
    ICUNIT_ASSERT_EQUAL(errno, EINVAL, errno);
    rt = sigaddset(&set, SIGALRM);
    ICUNIT_ASSERT_EQUAL(rt, 0, rt);

    siginfo_t si;
    time1.tv_sec = -1;
    printf("check invlid timespec: tv_sec=-1 ...\n");
    ret = SigtimedwaitFailTest(&set, &si, &time1, EINVAL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    time1.tv_sec = 1;
    time1.tv_nsec = -1;
    printf("check invlid timespec: tv_nsec=-1 ...\n");
    ret = SigtimedwaitFailTest(&set, &si, &time1, EINVAL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    time1.tv_sec = 1;
    time1.tv_nsec = 1000 * 1000 * 1000 + 1; // 1000, set the nsec of time.
    printf("check invlid timespec: tv_nsec overflow ...\n");
    ret = SigtimedwaitFailTest(&set, &si, &time1, EINVAL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return 0;
}

void ItPosixSignal042(void)
{
    TEST_ADD_CASE(__FUNCTION__, TestCase, TEST_POSIX, TEST_SIGNAL, TEST_LEVEL0, TEST_FUNCTION);
}
