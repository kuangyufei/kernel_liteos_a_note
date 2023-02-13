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
    mqd_t mqueueChild;
    char msgrcd[MQUEUE_STANDARD_NAME_LENGTH] = {0};
    char mqname[MQUEUE_STANDARD_NAME_LENGTH] = "/testMQueue001";
    const char msgptr[] = "childMsgs";
    struct mq_attr attr = { 0 };
    attr.mq_msgsize = MQUEUE_TEST_SIZE;
    attr.mq_maxmsg = MQUEUE_TEST_MAX_MSG;
    mqueueChild = mq_open(mqname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR, &attr);
    if (mqueueChild == (mqd_t)-1) {
        goto EXIT1;
    }
    ret = mq_send(mqueueChild, msgptr, strlen(msgptr), 0);
    if (ret != 0) {
        goto EXIT1;
    }
    ret = mq_receive(mqueueChild, msgrcd, MQUEUE_STANDARD_NAME_LENGTH, NULL);
    if (ret == -1) {
        goto EXIT1;
    }
    if (strncmp(msgrcd, msgptr, strlen(msgptr)) != 0) {
        goto EXIT1;
    }

EXIT1:
    if (mqueueChild >= 0) {
        ret = mq_close(mqueueChild);
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

void ItIpcContainer001(void)
{
    uint32_t ret;
    int status;
    int exitCode;
    pid_t pid;
    mqd_t mqueueParent;
    int arg = CHILD_FUNC_ARG;
    char *stack = (char *)mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                               -1, 0);
    ASSERT_NE(stack, nullptr);
    char *stackTop = stack + STACK_SIZE;
    char msgrcd[MQUEUE_STANDARD_NAME_LENGTH] = {0};
    char mqname[MQUEUE_STANDARD_NAME_LENGTH] = "/testMQueue001";
    const char msgptr[] = "parentMsg";
    struct mq_attr attr = { 0 };
    attr.mq_msgsize = MQUEUE_TEST_SIZE;
    attr.mq_maxmsg = MQUEUE_TEST_MAX_MSG;

    mqueueParent = mq_open(mqname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR, NULL);
    MQueueFinalizer mQueueFinalizer(mqueueParent, mqname);

    ASSERT_NE(mqueueParent, (mqd_t)-1);

    (void)memset_s(&attr, sizeof(attr), 0, sizeof(attr));
    ret = mq_getattr(mqueueParent, &attr);
    ASSERT_EQ(ret, 0);

    attr.mq_flags |= O_NONBLOCK;
    ret = mq_setattr(mqueueParent, &attr, NULL);
    ASSERT_EQ(ret, 0);

    ret = mq_getattr(mqueueParent, &attr);
    ASSERT_EQ(ret, 0);

    ret = mq_send(mqueueParent, msgptr, strlen(msgptr), 0);
    ASSERT_EQ(ret, 0);

    pid = clone(childFunc, stackTop, CLONE_NEWIPC | SIGCHLD, &arg);
    ASSERT_NE(pid, -1);

    ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);

    ret = WIFEXITED(status);
    ASSERT_NE(ret, 0);

    exitCode = WEXITSTATUS(status);
    ASSERT_EQ(exitCode, EXIT_CODE_ERRNO_7);

    ret = mq_receive(mqueueParent, msgrcd, MQUEUE_STANDARD_NAME_LENGTH, NULL);
    ASSERT_NE(ret, -1);
    ASSERT_STREQ(msgrcd, msgptr);
}
