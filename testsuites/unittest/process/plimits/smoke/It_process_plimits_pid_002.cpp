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

static int g_pidGroup[10] = {0};
static int g_index = 0;
static int g_sleepTime = 5;

static int PidWrite(const char *filepath, char *buf)
{
    int fd = open(filepath, O_WRONLY);
    size_t ret = write(fd, buf, strlen(buf));
    close(fd);
    return ret;
}

static void PidFork(int index)
{
    pid_t fpid = fork();
    if (fpid == 0) {  // children proc
        sleep(g_sleepTime);
        exit(0);
    } else if (fpid > 0) {
        g_pidGroup[index] = fpid;
        return;
    }
}

void ItProcessPlimitsPid002(void)
{
    mode_t mode = 0;
    int status;
    const char *pidMaxPath = "/proc/plimits/test/pids.max";
    const char *procsPath = "/proc/plimits/test/plimits.procs";
    std::string path = "/proc/plimits/test";
    char writeBuf[3];
    int num = 4;
    int i = 0;
    pid_t fpid;
    int pidnum = 5;
    int ret = mkdir(path.c_str(), S_IFDIR | mode);
    ASSERT_EQ(ret, 0);

    (void)memset_s(writeBuf, sizeof(writeBuf), 0, sizeof(writeBuf));
    ret = sprintf_s(writeBuf, sizeof(writeBuf), "%d", pidnum);
    ASSERT_NE(ret, -1);
    ret = PidWrite(pidMaxPath, writeBuf);
    ASSERT_NE(ret, -1);

    (void)memset_s(writeBuf, sizeof(writeBuf), 0, sizeof(writeBuf));
    ret = sprintf_s(writeBuf, sizeof(writeBuf), "%d", getpid());
    ASSERT_NE(ret, -1);
    ret = PidWrite(procsPath, writeBuf);
    ASSERT_NE(ret, -1);

    for (i = g_index; i < num; ++i) {
        PidFork(i);
    }
    g_index = i;
    fpid = fork();
    if (fpid == 0) {
        exit(0);
    } else if (fpid > 0) {
        waitpid(fpid, &status, 0);
    }

    for (i = 0; i < g_index; ++i) {
        waitpid(g_pidGroup[i], &status, 0);
        ASSERT_EQ(WEXITSTATUS(status), 0);
    }
    ret = rmdir(path.c_str());
    ASSERT_EQ(ret, 0);
    return;
}
