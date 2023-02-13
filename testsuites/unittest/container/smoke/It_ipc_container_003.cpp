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
#include <fstream>
#include <iostream>
#include "It_container_test.h"
using namespace std;

static int childFunc(void *arg)
{
    int ret;
    (void)arg;
    char msgrcd[MQUEUE_STANDARD_NAME_LENGTH] = {0};
    char mqname[] = "/testMQueue005";
    const char msgptr[] = "childMsg";
    struct mq_attr attr = { 0 };
    mqd_t mqueue;
    attr.mq_msgsize = MQUEUE_TEST_SIZE;
    attr.mq_maxmsg = MQUEUE_TEST_MAX_MSG;

    mqueue = mq_open(mqname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR, &attr);
    if (mqueue == (mqd_t)-1) {
        goto EXIT1;
    }
    ret = mq_send(mqueue, msgptr, strlen(msgptr), 0);
    if (ret != 0) {
        goto EXIT1;
    }
    ret = mq_receive(mqueue, msgrcd, MQUEUE_STANDARD_NAME_LENGTH, NULL);
    if (ret != strlen(msgptr)) {
        goto EXIT1;
    }
    if (strncmp(msgrcd, msgptr, strlen(msgptr)) != 0) {
        goto EXIT1;
    }

EXIT1:
    if (mqueue >= 0) {
        ret = mq_close(mqueue);
        if (ret != 0) {
            return EXIT_CODE_ERRNO_1;
        }
        ret = mq_unlink(mqname);
        if (ret != 0) {
            return EXIT_CODE_ERRNO_2;
        }
    }
    return EXIT_CODE_ERRNO_7;
}

static void IpcContainerUnshare(void)
{
    int status, exitCode, ret;
    int arg = CHILD_FUNC_ARG;
    char *stack = (char *)mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE, CLONE_STACK_MMAP_FLAG, -1, 0);
    ASSERT_NE(stack, nullptr);
    char *stackTop = stack + STACK_SIZE;
    struct mq_attr attr = { 0 };
    attr.mq_msgsize = MQUEUE_TEST_SIZE;
    attr.mq_maxmsg = MQUEUE_TEST_MAX_MSG;
    struct sigevent notification;
    notification.sigev_notify = 5; /* 5: test data */
    char msgrcd[MQUEUE_STANDARD_NAME_LENGTH] = {0};
    char mqname[] = "/testMQueue006";
    const char msgptr[] = "parentMsg";

    ret = unshare(CLONE_NEWIPC);
    ASSERT_EQ(ret, 0);

    mqd_t mqueue = mq_open(mqname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR, &attr);
    MQueueFinalizer mQueueFinalizer(mqueue, mqname);

    ASSERT_NE(mqueue, (mqd_t)-1);

    ret = mq_notify(mqueue, &notification);
    ASSERT_EQ(ret, -1);
    ASSERT_EQ(errno, EINVAL);

    notification.sigev_notify = SIGEV_THREAD;
    ret = mq_notify(mqueue, &notification);
    ASSERT_EQ(ret, -1);
    ASSERT_EQ(errno, ENOTSUP);

    notification.sigev_notify = SIGEV_NONE;

    ret = mq_notify(-1, &notification);
    ASSERT_EQ(ret, -1);
    ASSERT_EQ(errno, EBADF);

    ret = mq_notify(mqueue, &notification);
    ASSERT_EQ(ret, 0);

    (void)memset_s(&attr, sizeof(attr), 0, sizeof(attr));
    ret = mq_getattr(mqueue, &attr);
    ASSERT_EQ(ret, 0);

    attr.mq_flags |= O_NONBLOCK;
    ret = mq_setattr(mqueue, &attr, NULL);
    ASSERT_EQ(ret, 0);

    ret = mq_getattr(mqueue, &attr);
    ASSERT_EQ(ret, 0);

    ret = mq_send(mqueue, msgptr, strlen(msgptr), 0);
    ASSERT_EQ(ret, 0);

    pid_t pid = clone(childFunc, stackTop, CLONE_NEWIPC | SIGCHLD, &arg);
    ASSERT_NE(pid, -1);

    ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);

    ret = WIFEXITED(status);
    ASSERT_NE(pid, 0);

    exitCode = WEXITSTATUS(status);
    ASSERT_EQ(exitCode, EXIT_CODE_ERRNO_7);

    ret = mq_notify(mqueue, nullptr);
    ASSERT_EQ(ret, 0);

    ret = mq_receive(mqueue, msgrcd, MQUEUE_STANDARD_NAME_LENGTH, NULL);
    ASSERT_EQ(ret, strlen(msgptr));
    ASSERT_STREQ(msgrcd, msgptr);
}

void ItIpcContainer003(void)
{
    auto pid = fork();
    ASSERT_TRUE(pid != -1);
    if (pid == 0) {
        IpcContainerUnshare();
        exit(0);
    }
    auto ret = waitpid(pid, NULL, 0);
    ASSERT_EQ(ret, pid);
}
