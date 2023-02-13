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
    char mqname[] = "/testMQueue003";
    const char msgptr[] = "childMsgs";
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

void ItIpcContainer002(void)
{
    uint32_t ret;
    int status;
    int exitCode;
    mqd_t mqueue;
    pid_t childPid;
    int arg = CHILD_FUNC_ARG;
    char *stack = (char *)mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                               -1, 0);
    ASSERT_NE(stack, nullptr);
    char *stackTop = stack + STACK_SIZE;
    struct mq_attr attr = { 0 };
    attr.mq_msgsize = MQUEUE_TEST_SIZE;
    attr.mq_maxmsg = MQUEUE_TEST_MAX_MSG;
    char msgrcd[MQUEUE_STANDARD_NAME_LENGTH] = {0};
    char mqname[] = "/testMQueue004";
    const char msgptr[] = "parentMsg";
    std::string containerType = "ipc";
    std::string filePath;
    int fd;
    int parentPid;
    std::string parentlink;
    std::string childlink;

    mqueue = mq_open(mqname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR, NULL);
    MQueueFinalizer mQueueFinalizer(mqueue, mqname);

    ASSERT_NE(mqueue, (mqd_t)-1);

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

    ret = mq_close(mqueue);
    ASSERT_EQ(ret, 0);

    ret = mq_unlink(mqname);
    ASSERT_EQ(ret, 0);

    childPid = clone(childFunc, stackTop, CLONE_NEWIPC | SIGCHLD, &arg);
    ASSERT_NE(childPid, -1);

    parentPid = getpid();
    parentlink = ReadlinkContainer(parentPid, containerType);
    childlink = ReadlinkContainer(childPid, containerType);
    filePath = GenContainerLinkPath(childPid, containerType);
    fd = open(filePath.c_str(), O_RDONLY);
    ASSERT_NE(fd, -1);

    ret = setns(fd, CLONE_NEWIPC);
    ASSERT_NE(ret, -1);

    ret = close(fd);
    ASSERT_NE(ret, -1);

    ret = waitpid(childPid, &status, 0);
    ASSERT_EQ(ret, childPid);

    ret = WIFEXITED(status);
    ASSERT_NE(ret, 0);

    exitCode = WEXITSTATUS(status);
    ASSERT_EQ(exitCode, EXIT_CODE_ERRNO_7);
}
