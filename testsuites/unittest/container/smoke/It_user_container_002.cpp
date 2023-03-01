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
const int TEST_SET_OLD_ID = 1001;
const int TEST_SET_NEW_ID = 4;
static const int SLEEP_TIME = 3;

std::string GetIdMapPath(int pid, const std::string& mapType)
{
    std::ostringstream buf;
    buf << "/proc/" << pid << "/" << mapType;
    return buf.str();
}

int WriteIdMap(int pid)
{
    std::string uidMapPath = GetIdMapPath(pid, "uid_map");
    std::string gidMapPath = GetIdMapPath(pid, "gid_map");

    const char* idMapStr = "0 1000 1\n1 1001 1\n2 1002 1\n3 1003 1\n4 1004 1\n";
    int strLen = strlen(idMapStr);
    int uidMap = open(uidMapPath.c_str(), O_WRONLY);
    if (uidMap == -1) {
        return EXIT_CODE_ERRNO_1;
    }

    int ret = write(uidMap, idMapStr, strLen);
    if (ret != strLen) {
        close(uidMap);
        return EXIT_CODE_ERRNO_2;
    }
    close(uidMap);

    int gidMap = open(gidMapPath.c_str(), O_WRONLY);
    if (gidMap == -1) {
        close(gidMap);
        return EXIT_CODE_ERRNO_3;
    }

    ret = write(gidMap, idMapStr, strLen);
    if (ret != strLen) {
        close(gidMap);
        return EXIT_CODE_ERRNO_4;
    }

    close(gidMap);
    return 0;
}

static int childFunc(void *arg)
{
    (void)arg;
    sleep(SLEEP_TIME);

    int newUid = getuid();
    if (newUid != 1) {
        return EXIT_CODE_ERRNO_1;
    }

    int newGid = getgid();
    if (newGid != 1) {
        return EXIT_CODE_ERRNO_2;
    }

    int ret = setuid(TEST_SET_NEW_ID);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_3;
    }

    ret = setgid(TEST_SET_NEW_ID);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_4;
    }

    newUid = getuid();
    if (newUid != TEST_SET_NEW_ID) {
        return EXIT_CODE_ERRNO_5;
    }

    newGid = getgid();
    if (newGid != TEST_SET_NEW_ID) {
        return EXIT_CODE_ERRNO_6;
    }

    exit(EXIT_TRUE_CODE);
}

void ItUserContainer002(void)
{
    int ret = setuid(TEST_SET_OLD_ID);
    ASSERT_EQ(ret, 0);

    int oldUid = getuid();
    ASSERT_EQ(oldUid, TEST_SET_OLD_ID);

    ret = setgid(TEST_SET_OLD_ID);
    ASSERT_EQ(ret, 0);

    int oldGid = getgid();
    ASSERT_EQ(oldGid, TEST_SET_OLD_ID);

    int arg = CHILD_FUNC_ARG;
    auto pid = CloneWrapper(childFunc, CLONE_NEWUSER, &arg);
    ASSERT_NE(pid, -1);

    ret = WriteIdMap(pid);
    ASSERT_EQ(ret, 0);

    int status;
    ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);

    status = WEXITSTATUS(status);
    ASSERT_EQ(status, EXIT_TRUE_CODE);

    int oldUid1 = getuid();
    ASSERT_EQ(oldUid1, TEST_SET_OLD_ID);

    int oldGid1 = getgid();
    ASSERT_EQ(oldGid1, TEST_SET_OLD_ID);

    ASSERT_EQ(oldUid, oldUid1);
    ASSERT_EQ(oldGid, oldGid1);
}
