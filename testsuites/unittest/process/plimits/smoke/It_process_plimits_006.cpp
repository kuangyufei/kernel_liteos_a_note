/*
 * Copyright (c) 2023-2023 Huawei Device Co., Ltd. All rights reserved.
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
#include <cstdio>
#include <unistd.h>
#include <cstdlib>
#include <fcntl.h>
#include "It_process_plimits.h"

void ItProcessPlimits006(void)
{
    int ret;
    const char *path3 = "/proc/plimits/plimits.procs";
    ret = remove(path3);
    ASSERT_EQ(ret, -1);

    const char *path4 = "/proc/plimits/pids.max";
    ret = remove(path4);
    ASSERT_EQ(ret, -1);

    const char *path5 = "/proc/plimits/pids.priority";
    ret = remove(path5);
    ASSERT_EQ(ret, -1);

    const char *path6 = "/proc/plimits/sched.period";
    ret = remove(path6);
    ASSERT_EQ(ret, -1);

    const char *path7 = "/proc/plimits/sched.quota";
    ret = remove(path7);
    ASSERT_EQ(ret, -1);

    const char *path8 = "/proc/plimits/sched.stat";
    ret = remove(path8);
    ASSERT_EQ(ret, -1);

    const char *path9 = "/proc/plimits/memory.limit";
    ret = remove(path9);
    ASSERT_EQ(ret, -1);

    const char *path10 = "/proc/plimits/memory.stat";
    ret = remove(path10);
    ASSERT_EQ(ret, -1);

    const char *path11 = "/proc/plimits/ipc.mq_limit";
    ret = remove(path11);
    ASSERT_EQ(ret, -1);

    const char *path12 = "/proc/plimits/ipc.shm_limit";
    ret = remove(path12);
    ASSERT_EQ(ret, -1);

    const char *path13 = "/proc/plimits/ipc.stat";
    ret = remove(path13);
    ASSERT_EQ(ret, -1);

    const char *path14 = "/proc/plimits/devices.list";
    ret = remove(path14);
    ASSERT_EQ(ret, -1);

    const char *path15 = "/proc/plimits/devices.deny";
    ret = remove(path15);
    ASSERT_EQ(ret, -1);

    const char *path16 = "/proc/plimits/devices.allow";
    ret = remove(path16);
    ASSERT_EQ(ret, -1);

    const char *path17 = "/proc/plimits/limiters";
    ret = remove(path17);
    ASSERT_EQ(ret, -1);
    return;
}
