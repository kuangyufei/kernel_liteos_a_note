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

#include "it_test_liteipc.h"
#include "sys/wait.h"

#include "unistd.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "sys/time.h"
#include "sys/ioctl.h"
#include "fcntl.h"

#include "liteipc.h"
#include "smgr_demo.h"

static int LiteIpcTest(void)
{
    unsigned int ret;
    IpcContent tmp;
    void *retptr = nullptr;
    /* testing open liteipc driver with different flag, expecting result is that flag matters nothing */
    int fd = open(LITEIPC_DRIVER, O_WRONLY | O_CLOEXEC);
    ICUNIT_ASSERT_NOT_EQUAL(fd, -1, fd);
    ret = close(fd);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    fd = open(LITEIPC_DRIVER, O_RDONLY | O_CREAT);
    ICUNIT_ASSERT_NOT_EQUAL(fd, -1, fd);
    ret = close(fd);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    fd = open(LITEIPC_DRIVER, O_RDWR | O_SYNC);
    ICUNIT_ASSERT_NOT_EQUAL(fd, -1, fd);

    /* testing mmap liteipc mem pool with different size and flag */
    retptr = mmap(nullptr, 1024 * 4096, PROT_READ, MAP_PRIVATE, fd, 0);
    ICUNIT_ASSERT_EQUAL((int)(intptr_t)retptr, -1, retptr);
    //retptr = mmap(nullptr, 0, PROT_READ, MAP_PRIVATE, fd, 0);
    //ICUNIT_ASSERT_EQUAL((int)(intptr_t)retptr, -1, retptr);
    retptr = mmap(nullptr, -1, PROT_READ, MAP_PRIVATE, fd, 0);
    ICUNIT_ASSERT_EQUAL((int)(intptr_t)retptr, -1, retptr);
    retptr = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    ICUNIT_ASSERT_EQUAL((int)(intptr_t)retptr, -1, retptr);
    retptr = mmap(nullptr, 4096, PROT_READ, MAP_SHARED, fd, 0);
    ICUNIT_ASSERT_EQUAL((int)(intptr_t)retptr, -1, retptr);

    retptr = mmap(nullptr, 1, PROT_READ, MAP_PRIVATE, fd, 0);
    ICUNIT_ASSERT_NOT_EQUAL((int)(intptr_t)retptr, -1, retptr);
    retptr = mmap(nullptr, 4095, PROT_READ, MAP_PRIVATE, fd, 0);
    ICUNIT_ASSERT_EQUAL((int)(intptr_t)retptr, -1, retptr);

    /* testing read/write api */
    char buf[10] = {0};
    ret = read(fd, buf, 10);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ret = write(fd, buf, 10);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    /* before set cms, testing ioctl cmd */
    ret = ioctl(fd, IPC_CMS_CMD, 0);
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, ret);
    ret = ioctl(fd, IPC_SET_IPC_THREAD, 0);
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, ret);
    ret = ioctl(fd, IPC_SEND_RECV_MSG, 0);
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, ret);
    ret = ioctl(fd, IPC_SEND_RECV_MSG, &tmp);
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, ret);

    sleep(2);
    /* after set cms, testing set cms cmd */
    ret = ioctl(fd, IPC_SET_CMS, 200);
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, ret);

    exit(0);
    return 0;
}

static int TestCase(void)
{
    unsigned int ret;
    int fd = -1;
    void *retptr = nullptr;
    int status = 0;
    pid_t pid = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT);
    if (pid == 0) {
        LiteIpcTest();
        exit(-1);
    }

    sleep(1);
    fd = open(LITEIPC_DRIVER, O_RDONLY);
    ICUNIT_ASSERT_NOT_EQUAL(fd, -1, fd);

    retptr = mmap(nullptr, 16 * 4096, PROT_READ, MAP_PRIVATE, fd, 0);
    ICUNIT_ASSERT_NOT_EQUAL((int)(intptr_t)retptr, -1, retptr);
    ret = ioctl(fd, IPC_SET_CMS, 0);
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, ret);
    ret = ioctl(fd, IPC_SET_CMS, 200);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = ioctl(fd, IPC_SET_CMS, 200);
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, ret);

    ret = waitpid(pid, &status, 0);
    ICUNIT_GOTO_EQUAL(ret, pid, ret, EXIT);
    status = WEXITSTATUS(status);
    ICUNIT_GOTO_EQUAL(status, 0, status, EXIT);

    return 0;
EXIT:
    return 1;
}

void ItPosixLiteIpc001(void)
{
    TEST_ADD_CASE("ItPosixLiteIpc001", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}

