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
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <sys/shm.h>
#include <gtest/gtest.h>
#include "It_process_plimits.h"

static const int g_buffSize = 512;
static const int g_arryLen = 4;
static const int g_readLen = 254;

void ItProcessPlimitsIpc010(void)
{
    mode_t mode;
    mode_t shmAcess = 0666;
    char buf[g_buffSize] = { 0 };
    char *array[g_arryLen] = { nullptr };
    int mqSuccessCount;
    int mqFailedCount;
    int shmSuccessSize;
    int shmFailedCount;
    int shmid = -1;
    int shmid_1 = -1;
    int index = 0;
    int shmCkeckSize = PAGE_SIZE * 9;
    void *shared = nullptr;
    size_t shmSize = PAGE_SIZE * 9;
    std::string subPlimitsPath = "/proc/plimits/test";
    std::string configFileShmSize = "/proc/plimits/test/ipc.shm_limit";
    char shmemLimit[6] = "40960";

    int ret = mkdir(subPlimitsPath.c_str(), S_IFDIR | mode);
    ASSERT_EQ(ret, 0);

    ret = WriteFile(configFileShmSize.c_str(), shmemLimit);
    ASSERT_NE(ret, -1);

    shmid = shmget(IPC_PRIVATE, shmSize, shmAcess | IPC_CREAT);
    ASSERT_NE(shmid, -1);
    shmid_1 = shmget(IPC_PRIVATE, 2 * PAGE_SIZE, shmAcess | IPC_CREAT); /* 2: PAGE num */
    ASSERT_EQ(shmid_1, -1);

    ret = ReadFile("/proc/plimits/test/ipc.stat", buf);
    GetLine(buf, g_arryLen, g_readLen, array);
    mqSuccessCount = atoi(array[index++] + strlen("mq count: "));
    mqFailedCount = atoi(array[index++] + strlen("mq failed count: "));
    shmSuccessSize = atoi(array[index++] + strlen("shm size: "));
    shmFailedCount = atoi(array[index++] + strlen("shm failed count: "));

    ASSERT_EQ(shmSuccessSize, shmCkeckSize);
    ASSERT_EQ(shmFailedCount, 1);

    shared = shmat(shmid, nullptr, 0);
    ASSERT_NE(shared, (void *)-1);

    ret = shmdt(shared);
    ASSERT_NE(ret, -1);

    ret = shmctl(shmid, IPC_RMID, nullptr);
    ASSERT_NE(ret, -1);
    shmctl(shmid_1, IPC_RMID, nullptr);

    ret = rmdir(subPlimitsPath.c_str());
    ASSERT_EQ(ret, 0);
    return;
}
