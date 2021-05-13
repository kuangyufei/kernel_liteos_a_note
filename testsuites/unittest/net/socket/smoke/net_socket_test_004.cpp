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

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <osTest.h>

static int SockOptTest(void)
{
    int ret, error, flag;
    struct timeval timeout;
    socklen_t len;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ICUNIT_ASSERT_NOT_EQUAL(fd, -1, fd);

    error = -1;
    len = sizeof(error);
    ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
    LogPrintln("getsockopt(%d, SOL_SOCKET, SO_ERROR, &error, &len)=%d, error=%d, len=%d, errno=%d", fd, ret, error, len, errno);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(error, 0, error);

    len = sizeof(timeout);
    ret = getsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, &len);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    timeout.tv_sec = 1000;
    len = sizeof(timeout);
    ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, len);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = memset_s(&timeout, len, 0, len);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = getsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, &len);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(timeout.tv_sec, 1000, ret);

    error = -1;
    len = sizeof(error);
    ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
    LogPrintln("getsockopt(%d, SOL_SOCKET, SO_ERROR, &error, &len)=%d, error=%d, len=%d, errno=%d", fd, ret, error, len, errno);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(error, 0, error);

    flag=1;
    ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    LogPrintln("setsockopt(TCP_NODELAY) ret=%d", ret);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = close(fd);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return ICUNIT_SUCCESS;
}

void NetSocketTest004(void)
{
    TEST_ADD_CASE(__FUNCTION__, SockOptTest, TEST_POSIX, TEST_TCP, TEST_LEVEL0, TEST_FUNCTION);
}
