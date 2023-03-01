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
#include <vector>
#include <cstring>
#include <sys/wait.h>
#include "It_process_plimits.h"

static int PidWrite(const char *filepath, char *buf)
{
    int fd = open(filepath, O_WRONLY);
    size_t ret = write(fd, buf, strlen(buf));
    close(fd);
    return ret;
}

void ItProcessPlimitsPid003(void)
{
    mode_t mode = 0;
    int status;
    struct sched_param param = { 0 };
    const char *pidMaxPath = "/proc/plimits/test/pids.priority";
    std::string path = "/proc/plimits/test";
    char writeBuf[3];
    int pidnum = 15; /* 15: priority limit */
    int ret = mkdir(path.c_str(), S_IFDIR | mode);
    ASSERT_EQ(ret, 0);

    (void)memset_s(writeBuf, sizeof(writeBuf), 0, sizeof(writeBuf));
    ret = sprintf_s(writeBuf, sizeof(writeBuf), "%d", pidnum);
    ASSERT_NE(ret, -1);

    ret = PidWrite(pidMaxPath, writeBuf);
    ASSERT_NE(ret, -1);

    int currProcessPri = getpriority(PRIO_PROCESS, getpid());

    param.sched_priority = pidnum - 1;
    ret = sched_setscheduler(getpid(), SCHED_RR, &param);
    ASSERT_NE(ret, 0);

    ret = setpriority(PRIO_PROCESS, getpid(), param.sched_priority);
    ASSERT_NE(ret, 0);

    ret = sched_setparam(getpid(), &param);
    ASSERT_NE(ret, 0);

    param.sched_priority = currProcessPri + 1;
    ret = sched_setscheduler(getpid(), SCHED_RR, &param);
    ASSERT_EQ(ret, 0);

    param.sched_priority++;
    ret = setpriority(PRIO_PROCESS, getpid(), param.sched_priority);
    ASSERT_EQ(ret, 0);

    param.sched_priority++;
    ret = sched_setparam(getpid(), &param);
    ASSERT_EQ(ret, 0);

    ret = rmdir(path.c_str());
    ASSERT_EQ(ret, 0);
    return;
}
