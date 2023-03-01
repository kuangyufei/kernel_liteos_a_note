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
#include <mqueue.h>
#include <gtest/gtest.h>
#include "It_process_plimits.h"

static const int MQUEUE_STANDARD_NAME_LENGTH  = 50;
static const int MQUEUE_STANDARD_NAME_LENGTH_MAX = 102500;
static const int g_buff = 512;
static const int MQ_MAX_LIMIT_COUNT = 10;

static int FreeResource(mqd_t *mqueue, int index, char *mqname)
{
    int ret = -1;
    for (int k = 0; k < index; ++k) {
        ret = snprintf_s(mqname, MQUEUE_STANDARD_NAME_LENGTH, MQUEUE_STANDARD_NAME_LENGTH, "/mq_006_%d_%d",
                         k, LosCurTaskIDGet());
        if (ret < 0) {
            return -1; /* 1: errno */
        }
        ret = mq_close(mqueue[k]);
        if (ret < 0) {
            return -2; /* 2: errno */
        }
        ret = mq_unlink(mqname);
        if (ret < 0) {
            return -3; /* 3: errno */
        }
        (void)memset_s(mqname, MQUEUE_STANDARD_NAME_LENGTH, 0, MQUEUE_STANDARD_NAME_LENGTH);
    }
    return ret ;
}

void ItProcessPlimitsIpc013(void)
{
    INT32 ret;
    mode_t mode;
    CHAR mqname[MQUEUE_STANDARD_NAME_LENGTH] = "";
    mqd_t mqueue[g_buff];
    std::string subPlimitsPath = "/proc/plimits/test";
    std::string configFileMqCount = "/proc/plimits/test/ipc.mq_limit";
    std::string limitMqCount = "11";
    struct mq_attr attr = { 0 };
    attr.mq_msgsize = MQUEUE_STANDARD_NAME_LENGTH;
    attr.mq_maxmsg = 1;
    struct mq_attr attr_err = { 0 };
    attr_err.mq_msgsize = MQUEUE_STANDARD_NAME_LENGTH_MAX;
    attr_err.mq_maxmsg = 1;
    int index = 0;

    ret = mkdir(subPlimitsPath.c_str(), S_IFDIR | mode);
    ASSERT_EQ(ret, 0);

    ret = WriteFile(configFileMqCount.c_str(), limitMqCount.c_str());
    ASSERT_NE(ret, -1);

    for (index = 0; index < MQ_MAX_LIMIT_COUNT; ++index) {
        ret = snprintf_s(mqname, sizeof(mqname), MQUEUE_STANDARD_NAME_LENGTH, "/mq_006_%d_%d",
                         index, LosCurTaskIDGet());
        ASSERT_NE(ret, -1);
        mqueue[index] = mq_open(mqname, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attr);
        ASSERT_NE(mqueue[index], (mqd_t)-1);
        (void)memset_s(mqname, sizeof(mqname), 0, sizeof(mqname));
    }

    ret = snprintf_s(mqname, sizeof(mqname), MQUEUE_STANDARD_NAME_LENGTH, "/mq_006_%d_%d", index, LosCurTaskIDGet());
    ASSERT_NE(ret, -1);
    mqueue[index] = mq_open(mqname, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attr_err);
    ASSERT_EQ(mqueue[index], (mqd_t)-1);
    (void)memset_s(mqname, sizeof(mqname), 0, sizeof(mqname));
    ret = FreeResource(mqueue, index, mqname);
    ASSERT_EQ(ret, 0);

    ret = rmdir(subPlimitsPath.c_str());
    ASSERT_EQ(ret, 0);
    return;
}
