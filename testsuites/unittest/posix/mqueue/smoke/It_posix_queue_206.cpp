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
#include "It_posix_queue.h"

static int Child(VOID)
{
    int ret;
    CHAR mqname[MQUEUE_STANDARD_NAME_LENGTH] = "";
    int pid;
    int status;
    mqd_t mqueue;
    struct sigevent notification;

    (void)snprintf_s(mqname, MQUEUE_STANDARD_NAME_LENGTH - 1, MQUEUE_STANDARD_NAME_LENGTH, "/mq206_%d", getpid());

    mqueue = mq_open(mqname, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, nullptr);
    ICUNIT_GOTO_NOT_EQUAL(mqueue, (mqd_t)-1, mqueue, EXIT);

    notification.sigev_notify = 5; // 5, User-defined signal.

    ret = mq_notify(mqueue, &notification);
    ICUNIT_GOTO_EQUAL(ret, -1, ret, EXIT);
    ICUNIT_GOTO_EQUAL(errno, EINVAL, errno, EXIT);

    notification.sigev_notify = SIGEV_THREAD;

    ret = mq_notify(mqueue, &notification);
    ICUNIT_GOTO_EQUAL(ret, -1, ret, EXIT);
    ICUNIT_GOTO_EQUAL(errno, ENOTSUP, errno, EXIT);

    notification.sigev_notify = SIGEV_NONE;

    ret = mq_notify(-1, &notification);
    ICUNIT_GOTO_EQUAL(ret, -1, ret, EXIT);
    ICUNIT_GOTO_EQUAL(errno, EBADF, errno, EXIT);

    ret = mq_notify(mqueue, &notification);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    pid = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT); // 100000, Valid range value of pid.

    if (pid == 0) {
        ret = mq_notify(mqueue, &notification);
        ICUNIT_ASSERT_EQUAL(ret, -1, ret);
        ICUNIT_ASSERT_EQUAL(errno, EBUSY, errno);

        exit(8); // 8, Set a exit status.
    } else {
        ret = waitpid(pid, &status, 0);
        ICUNIT_GOTO_EQUAL(ret, pid, ret, EXIT);

        status = WEXITSTATUS(status);
        ICUNIT_GOTO_EQUAL(status, 8, status, EXIT); // 8, Here, assert the ret.

        ret = mq_notify(mqueue, nullptr);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

        pid = fork();
        ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT); // 100000, Valid range value of pid.
        if (pid == 0) {
            notification.sigev_notify = SIGEV_SIGNAL;
            notification.sigev_signo = SIGUSR1;

            ret = mq_notify(mqueue, &notification);
            ICUNIT_ASSERT_EQUAL(ret, 0, ret);

            exit(11); // 11, Set a exit status.
        }

        ret = waitpid(pid, &status, 0);
        ICUNIT_GOTO_EQUAL(ret, pid, ret, EXIT);

        status = WEXITSTATUS(status);
        ICUNIT_GOTO_EQUAL(status, 11, status, EXIT); // 11, Here, assert the ret.

        mq_close(mqueue);
        mq_unlink(mqname);
    }
    exit(10); // 10, Set a exit status.

EXIT:
    mq_close(mqueue);
    mq_unlink(mqname);
    return MQUEUE_IS_ERROR;
}
static UINT32 Testcase(VOID)
{
    int ret;
    int status;

    pid_t pid = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT); // 100000, Valid range value of pid.

    if (pid == 0) {
        Child();
        exit(255); // 255, Set a exit status.
    }

    ret = waitpid(pid, &status, 0);
    ICUNIT_ASSERT_EQUAL(ret, pid, ret);

    status = WEXITSTATUS(status);
    ICUNIT_ASSERT_EQUAL(status, 10, status); // 10, Here, assert the ret.

    return 0;
EXIT:
    return 1;
}

VOID ItPosixQueue206(VOID)
{
    TEST_ADD_CASE("IT_POSIX_QUEUE_206", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}