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
#include "It_test_IO.h"

struct PollParam {
    int nfds;
    int timeout;
    int ret;
    int err;
};

static UINT32 Testcase(VOID)
{
    const int TEST_STR_LEN = 12;
    int pipeFd[2], ret, err; // 2, pipe return 2 file descirpter
    struct pollfd pollFd;
    struct PollParam pollParam[4] = { /* nfds timeout ret     err */
        {    0,   100,  -1, EINVAL},
        { 4096,    10,  -1, EINVAL},
        { 4095,    10,  -1, EFAULT},
        {    1,    -1,   1,      0}
    };

    ret = pipe(pipeFd);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = write(pipeFd[1], "hello world", TEST_STR_LEN);
    printf("write first status: %d\n", ret);
    ICUNIT_GOTO_EQUAL(ret, TEST_STR_LEN, ret, EXIT);

    pollFd.fd = pipeFd[0];
    pollFd.events = POLLIN;

    for (int i = 0; i < sizeof(pollParam) / sizeof(pollParam[0]); i++) {
        ret = poll(&pollFd, pollParam[i].nfds, pollParam[i].timeout);
        ICUNIT_GOTO_EQUAL(ret, pollParam[i].ret, ret, EXIT);
        if (ret == -1) {
            printf("i = %d\n", i);
            if (i == 2) { // 2, pollParam[2].
                ICUNIT_GOTO_TWO_EQUAL(errno, EFAULT, EBADF, errno, EXIT);
            } else {
                ICUNIT_GOTO_EQUAL(errno, pollParam[i].err, errno, EXIT);
            }
        }
    }

    close(pipeFd[0]);
    close(pipeFd[1]);
    return LOS_OK;
EXIT:
    close(pipeFd[0]);
    close(pipeFd[1]);
    return LOS_NOK;
}

VOID ItStdlibPoll003(void)
{
    TEST_ADD_CASE(__FUNCTION__, Testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
