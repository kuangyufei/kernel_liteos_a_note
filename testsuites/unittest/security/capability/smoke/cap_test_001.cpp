/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
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
#include "pthread.h"
#include "linux/capability.h"
#include <sys/capability.h>
#include "it_test_capability.h"
#include <signal.h>
#include <sys/types.h>
#include <time.h>

#define CAP_NUM 2
#define INVAILD_PID 65535
#define CHANGE_CHILD_UID 1000

static void Sigac(int param)
{
    return;
}

static void Child()
{
    int i = 10;
    signal(25, Sigac);

    while (i--) {
        sleep(1);
    }
    exit(0);
}

static int TestChild(VOID)
{
    struct __user_cap_header_struct capheader;
    struct __user_cap_data_struct capdata[CAP_NUM];
    struct __user_cap_data_struct capdatac[CAP_NUM];
    struct timespec tp;
    int ret;

    (void)memset_s(&capheader, sizeof(struct __user_cap_header_struct), 0, sizeof(struct __user_cap_header_struct));
    (void)memset_s(capdata, CAP_NUM * sizeof(struct __user_cap_data_struct), 0,
        CAP_NUM * sizeof(struct __user_cap_data_struct));
    capdata[0].permitted = 0xffffffff;
    capdata[1].permitted = 0xffffffff;
    capheader.version = _LINUX_CAPABILITY_VERSION_3;
    capdata[CAP_TO_INDEX(CAP_SYS_NICE)].effective |= CAP_TO_MASK(CAP_SETPCAP);
    capdata[CAP_TO_INDEX(CAP_SYS_NICE)].effective |= CAP_TO_MASK(CAP_SETUID);
    capdata[CAP_TO_INDEX(CAP_SYS_NICE)].effective |= CAP_TO_MASK(CAP_KILL);
    capdata[CAP_TO_INDEX(CAP_SYS_NICE)].effective |= CAP_TO_MASK(CAP_SYS_TIME);
    capdata[CAP_TO_INDEX(CAP_SYS_NICE)].effective |= CAP_TO_MASK(CAP_SYS_NICE);
    ret = capset(&capheader, &capdata[0]);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = capget(&capheader, &capdatac[0]);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    capheader.pid = INVAILD_PID;
    ret = capget(&capheader, &capdatac[0]);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    errno = 0;
    capheader.pid = 3;
    kill(capheader.pid, 0);
    if (errno != ESRCH) {
        ret = capget(&capheader, &capdatac[0]);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        printf("e %d,p %d\n", capdatac[0].effective, capdatac[0].permitted);
    }
    errno = 0;
    capheader.pid = 4;
    kill(capheader.pid, 0);
    if (errno != ESRCH) {
        ret = capget(&capheader, &capdatac[0]);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        printf("e %d,p %d\n", capdatac[0].effective, capdatac[0].permitted);
    }
    errno = 0;
    capheader.pid = 5;
    kill(capheader.pid, 0);
    if (errno != ESRCH) {
        ret = capget(&capheader, &capdatac[0]);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        printf("e %d,p %d\n", capdatac[0].effective, capdatac[0].permitted);
    }
    errno = 0;
    capheader.pid = 6;
    kill(capheader.pid, 0);
    if (errno != ESRCH) {
        ret = capget(&capheader, &capdatac[0]);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        printf("e %d,p %d\n", capdatac[0].effective, capdatac[0].permitted);
    }
    capheader.pid = 0;

    int pid = fork();
    if (pid == 0) {
        ret = setuid(CHANGE_CHILD_UID);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        Child();
    }
    sleep(1);
    ret = kill(pid, SIGXFSZ);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    capdata[CAP_TO_INDEX(CAP_SYS_NICE)].effective &= ~CAP_TO_MASK(CAP_KILL);
    ret = capset(&capheader, &capdata[0]);
    ret = kill(pid, SIGXFSZ);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    tp.tv_sec = 0;
    tp.tv_nsec = 0;
    ret = clock_settime(CLOCK_REALTIME, &tp);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    capdata[CAP_TO_INDEX(CAP_SYS_NICE)].effective &= ~CAP_TO_MASK(CAP_SYS_TIME);
    ret = capset(&capheader, &capdata[0]);
    ret = clock_settime(CLOCK_REALTIME, &tp);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    struct sched_param param = { 0 };
    ret = sched_getparam(pid, &param);
    param.sched_priority--;
    ret = sched_setparam(pid, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    capdata[CAP_TO_INDEX(CAP_SYS_NICE)].effective &= ~CAP_TO_MASK(CAP_SYS_NICE);
    ret = capset(&capheader, &capdata[0]);
    ret = sched_setparam(pid, &param);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    wait(nullptr);
    exit(92);

    return 0;
}

static int TestCase(VOID)
{
    int ret;
    int status = 0;
    pid_t pid = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT);
    if (pid == 0) {
        ret = TestChild();
        exit(__LINE__);
    }

    ret = waitpid(pid, &status, 0);
    ICUNIT_GOTO_EQUAL(ret, pid, ret, EXIT);
    status = WEXITSTATUS(status);
    ICUNIT_GOTO_EQUAL(status, 92, status, EXIT);

    return 0;

EXIT:
    return 1;
}

void ItTestCap001(void)
{
    TEST_ADD_CASE("IT_SEC_CAP_001", TestCase, TEST_POSIX, TEST_SEC, TEST_LEVEL0, TEST_FUNCTION);
}
