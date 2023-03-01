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
#include "It_container_test.h"
#include "sys/utsname.h"

const int EXIT_TRUE_CODE = 255;
const int MAX_PID_RANGE = 100000;
const int TEST_SET_OLD_ID = 1001;
const int TEST_SET_NEW_ID = 4;

std::string GetIdMapPath(int pid, const std::string& mapType);
int WriteIdMap(int pid);

static int TestMap(int oldUid, int oldGid)
{
    pid_t pid = getpid();
    int ret = WriteIdMap(pid);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_6;
    }

    int newUid = getuid();
    if (newUid != 1) {
        return EXIT_CODE_ERRNO_7;
    }

    int newGid = getgid();
    if (newGid != 1) {
        return EXIT_CODE_ERRNO_8;
    }

    ret = setuid(TEST_SET_NEW_ID);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_9;
    }

    ret = setgid(TEST_SET_NEW_ID);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_10;
    }

    newUid = getuid();
    if (newUid != TEST_SET_NEW_ID) {
        return EXIT_CODE_ERRNO_11;
    }

    newGid = getgid();
    if (newGid != TEST_SET_NEW_ID) {
        exit(1);
    }

    if (oldUid == newUid) {
        return EXIT_CODE_ERRNO_12;
    }

    if (oldGid == newGid) {
        return EXIT_CODE_ERRNO_13;
    }
    return 0;
}

static int childFunc(void *arg)
{
    (void)arg;
    int ret;
    int oldUid;
    int oldGid;

    ret = setuid(TEST_SET_OLD_ID);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_1;
    }

    oldUid = getuid();
    if (oldUid != TEST_SET_OLD_ID) {
        return EXIT_CODE_ERRNO_2;
    }

    ret = setgid(TEST_SET_OLD_ID);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_3;
    }

    oldGid = getgid();
    if (oldGid != TEST_SET_OLD_ID) {
        return EXIT_CODE_ERRNO_4;
    }

    ret = unshare(CLONE_NEWUSER);
    if (ret == -1) {
        return EXIT_CODE_ERRNO_5;
    }

    ret = TestMap(oldUid, oldGid);
    if (ret != 0) {
        return ret;
    }

    exit(EXIT_TRUE_CODE);
}

void ItUserContainer003(void)
{
    int ret;
    int status = 0;
    int arg = CHILD_FUNC_ARG;
    auto pid = CloneWrapper(childFunc, SIGCHLD, &arg);
    ASSERT_NE(pid, -1);

    ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);

    status = WEXITSTATUS(status);
    ASSERT_EQ(status, EXIT_TRUE_CODE);
}
