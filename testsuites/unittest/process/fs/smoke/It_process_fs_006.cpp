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
#include "It_process_fs_test.h"

static int const maxContainerNum = 5;
static int const configLen       = 16;
static int nInitArry[maxContainerNum] = {61, 54, 49, 44, 41};

static std::string arryEntries[maxContainerNum] = {
    "max_mnt_container",
    "max_pid_container",
    "max_user_container",
    "max_net_container",
    "max_uts_container"
};

static void WriteContainer(const char *filepath, int value)
{
    PrintTest("writeproc %d >> %s\n", value, filepath);
    int fd = open(filepath, O_WRONLY);
    ASSERT_NE(fd, -1);
    char buf[configLen];
    size_t twd = sprintf_s(buf, configLen, "%d", value);
    ASSERT_GT(twd, 0);
    twd = write(fd, buf, (strlen(buf)+1));
    ASSERT_EQ(twd, -1);
    (void)close(fd);
}

static void ReadContainer(std::string strFile, int value)
{
    char szStatBuf[configLen];
    FILE *fp = fopen(strFile.c_str(), "rb");
    ASSERT_NE(fp, nullptr);

    int ret;
    (void)memset_s(szStatBuf, configLen, 0, configLen);
    ret = fread(szStatBuf, 1, configLen, fp);
    ASSERT_NE(ret, 0);
    PrintTest("cat %s\n", strFile.c_str());

    PrintTest("%s\n", szStatBuf);
    ret = atoi(szStatBuf);
    ASSERT_EQ(ret, value);

    (void)fclose(fp);
}

static void ErrWriteContainer0(const char *filepath)
{
    int fd = open(filepath, O_WRONLY);
    ASSERT_NE(fd, -1);
    char buf[configLen];
    int invalidNum = 0;
    size_t twd1 = sprintf_s(buf, configLen, "%d", invalidNum);
    ASSERT_GT(twd1, 0);
    PrintTest("writeproc %d >> %s\n", invalidNum, filepath);
    twd1 = write(fd, buf, (strlen(buf)+1));
    (void)close(fd);
    ASSERT_EQ(twd1, -1);
}

static void ErrWriteContainer65(const char *filepath)
{
    int fd = open(filepath, O_WRONLY);
    ASSERT_NE(fd, -1);
    char buf[configLen];
    int invalidNum = 65;
    size_t twd2 = sprintf_s(buf, configLen, "%d", invalidNum);
    ASSERT_GT(twd2, 0);
    PrintTest("writeproc %d >> %s\n", invalidNum, filepath);
    twd2 = write(fd, buf, (strlen(buf)+1));
    (void)close(fd);
    ASSERT_EQ(twd2, -1);
}

void ItProcessFs006(void)
{
    const int CONFIG_FILE_LEN = 1024;
    char szFile[CONFIG_FILE_LEN] = {0};
    for (int i = 0; i < maxContainerNum; i++) {
        size_t count = sprintf_s(szFile, CONFIG_FILE_LEN, "/proc/sys/user/%s", arryEntries[i].c_str());
        ASSERT_GT(count, 0);
        WriteContainer(szFile, nInitArry[i]);
        ReadContainer(szFile, nInitArry[i]);
    }

    for (int i = 0; i < maxContainerNum; i++) {
        size_t count = sprintf_s(szFile, CONFIG_FILE_LEN, "/proc/sys/user/%s", arryEntries[i].c_str());
        ASSERT_GT(count, 0);
        ErrWriteContainer0(szFile);

        ErrWriteContainer65(szFile);
    }
}
