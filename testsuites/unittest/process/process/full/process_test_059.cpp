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
#include "it_test_process.h"
#include <spawn.h>

static const int HIGH_PRIORITY = 31;

static int TestSpawnAttrDef(posix_spawnattr_t *attr)
{
    sigset_t signalset;
    sigset_t signalset1;
    int ret;

    ret = sigemptyset(&signalset);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = sigaddset(&signalset, SIGPIPE);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = posix_spawnattr_setsigdefault(attr, &signalset);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    posix_spawnattr_getsigdefault(attr, &signalset1);
    ret = memcmp(&signalset, &signalset1, sizeof(sigset_t));
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = sigismember(&signalset, SIGPIPE);
    ICUNIT_GOTO_EQUAL(ret, 1, ret, EXIT);
    return 0;
EXIT:
    return 1;
}

static int TestSpawnAttrMask(posix_spawnattr_t *attr)
{
    sigset_t signalset;
    sigset_t signalset1;
    int ret;

    ret = sigemptyset(&signalset);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = sigaddset(&signalset, SIGPIPE);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = posix_spawnattr_setsigmask(attr, &signalset);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    posix_spawnattr_getsigmask(attr, &signalset1);
    ret = memcmp(&signalset, &signalset1, sizeof(sigset_t));
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = sigismember(&signalset, SIGPIPE);
    ICUNIT_GOTO_EQUAL(ret, 1, ret, EXIT);
    return 0;
EXIT:
    return 1;
}

static int TestSpawnAttrGroup(posix_spawnattr_t *attr)
{
    pid_t val = -1;
    pid_t pid = getpgrp();
    int ret;
    posix_spawnattr_getpgroup(attr, &val);
    ICUNIT_GOTO_EQUAL(val, 0, val, EXIT);

    val = getpgid(getpid());
    ret = posix_spawnattr_setpgroup(attr, val);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    val = -1;

    posix_spawnattr_getpgroup(attr, &val);
    ICUNIT_GOTO_EQUAL(val, pid, val, EXIT);
    return 0;
EXIT:
    return 1;
}

static int TestSpawnAttrPrio(posix_spawnattr_t *attr)
{
    struct sched_param val = { -1 };
    struct sched_param val1 = { -1 };
    int ret;
    posix_spawnattr_getschedparam(attr, &val);
    ICUNIT_GOTO_EQUAL(val.sched_priority, 0, ret, EXIT);

    val.sched_priority = HIGH_PRIORITY;
    ret = posix_spawnattr_setschedparam(attr, &val);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    posix_spawnattr_getschedparam(attr, &val1);
    ICUNIT_GOTO_EQUAL(val1.sched_priority, HIGH_PRIORITY, ret, EXIT);

    return 0;
EXIT:
    return 1;
}

static int TestSpawnAttrPol(posix_spawnattr_t *attr)
{
    int val = -1;
    int ret;

    posix_spawnattr_getschedpolicy(attr, &val);
    ICUNIT_GOTO_EQUAL(val, 0, val, EXIT);

    val = SCHED_RR;
    ret = posix_spawnattr_setschedpolicy(attr, val);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    val = -1;

    posix_spawnattr_getschedpolicy(attr, &val);
    ICUNIT_GOTO_EQUAL(val, SCHED_RR, val, EXIT);

    ret = TestSpawnAttrPrio(attr);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    return 0;
EXIT:
    return 1;
}

static int TestSpawnpAttr(short flag)
{
    pid_t pid;
    posix_spawnattr_t attr;
    int status = 1;
    char *argv1[] = {"tftp", NULL};
    short iflag = -1;
    int ret;

    ret = posix_spawnattr_init(&attr);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    posix_spawnattr_getflags(&attr, &iflag);
    ICUNIT_GOTO_EQUAL(iflag, 0, iflag, EXIT);

    ret = posix_spawnattr_setflags(&attr, flag);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    iflag = -1;

    posix_spawnattr_getflags(&attr, &iflag);
    ICUNIT_GOTO_EQUAL(iflag, flag, iflag, EXIT);

    if (POSIX_SPAWN_SETSIGDEF == flag) {
        ret = TestSpawnAttrDef(&attr);
    } else if (POSIX_SPAWN_SETSIGMASK == flag) {
        ret = TestSpawnAttrMask(&attr);
    } else if (POSIX_SPAWN_SETPGROUP == flag) {
        ret = TestSpawnAttrGroup(&attr);
    } else if (POSIX_SPAWN_SETSCHEDPARAM == flag) {
        ret = TestSpawnAttrPrio(&attr);
    } else if (POSIX_SPAWN_SETSCHEDULER == flag) {
        ret = TestSpawnAttrPol(&attr);
    }
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = posix_spawnp(&pid, "/bin/tftp", NULL, &attr, argv1, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = posix_spawnattr_destroy(&attr);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = waitpid(pid, &status, 0);
    ICUNIT_GOTO_EQUAL(ret, pid, ret, EXIT);

    return 0;
EXIT:
    return 1;
}

static int TestCase(void)
{
    int ret;

    ret = TestSpawnpAttr(POSIX_SPAWN_RESETIDS);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = TestSpawnpAttr(POSIX_SPAWN_SETSIGDEF);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = TestSpawnpAttr(POSIX_SPAWN_SETSIGMASK);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = TestSpawnpAttr(POSIX_SPAWN_SETSCHEDPARAM);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = TestSpawnpAttr(POSIX_SPAWN_SETSCHEDULER);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = TestSpawnpAttr(POSIX_SPAWN_SETPGROUP);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    return 0;
EXIT:
    return 1;
}

void ItTestProcess059(void)
{
    TEST_ADD_CASE("IT_POSIX_PROCESS_059", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
