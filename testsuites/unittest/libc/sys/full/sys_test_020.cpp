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
#include "It_test_sys.h"

STATIC UINT32 TestCase(VOID)
{
    struct passwd *user1 = nullptr;
    struct passwd *user2 = nullptr;
    struct passwd *user3 = nullptr;
    struct passwd *user4 = nullptr;
    INT32 ret;
    CHAR *pathList[] = {"/etc/group", "/etc/passwd"};
    CHAR *streamList[] = {g_groupFileStream, g_passwdFileStream};
    INT32 streamLen[] = {strlen(g_groupFileStream), strlen(g_passwdFileStream)};

    ret = PrepareFileEnv(pathList, streamList, streamLen, 2); /* 2, group & passwd */
    if (ret != 0) {
        printf("error: need some env file, but prepare is not ok");
        (VOID)RecoveryFileEnv(pathList, 2); /* 2, group & passwd */
        return -1;
    }

    user1 = getpwuid(0);
    ICUNIT_GOTO_NOT_EQUAL(user1, nullptr, -1, ERROUT);
    ICUNIT_GOTO_STRING_EQUAL(user1->pw_name, "root", -1, ERROUT);
    ICUNIT_GOTO_STRING_EQUAL(user1->pw_passwd, "x", -1, ERROUT);
    ICUNIT_GOTO_EQUAL(user1->pw_uid, 0, -1, ERROUT);
    ICUNIT_GOTO_EQUAL(user1->pw_gid, 0, -1, ERROUT);
    ICUNIT_GOTO_STRING_EQUAL(user1->pw_gecos, "root", -1, ERROUT);
    ICUNIT_GOTO_STRING_EQUAL(user1->pw_dir, "/root", -1, ERROUT);
    ICUNIT_GOTO_STRING_EQUAL(user1->pw_shell, "/bin/bash", -1, ERROUT);

    user2 = getpwuid(1);
    ICUNIT_GOTO_NOT_EQUAL(user2, nullptr, -1, ERROUT);
    ICUNIT_GOTO_STRING_EQUAL(user2->pw_name, "daemon", -1, ERROUT);
    ICUNIT_GOTO_STRING_EQUAL(user2->pw_passwd, "x", -1, ERROUT);
    ICUNIT_GOTO_EQUAL(user2->pw_uid, 1, -1, ERROUT);
    ICUNIT_GOTO_EQUAL(user2->pw_gid, 1, -1, ERROUT);
    ICUNIT_GOTO_STRING_EQUAL(user2->pw_gecos, "daemon", -1, ERROUT);
    ICUNIT_GOTO_STRING_EQUAL(user2->pw_dir, "/usr/sbin", -1, ERROUT);
    ICUNIT_GOTO_STRING_EQUAL(user2->pw_shell, "/usr/sbin/nologin", -1, ERROUT);

    user3 = getpwuid(2); /* 2, from etc/group */
    ICUNIT_GOTO_NOT_EQUAL(user3, nullptr, -1, ERROUT);
    ICUNIT_GOTO_STRING_EQUAL(user3->pw_name, "bin", -1, ERROUT);
    ICUNIT_GOTO_STRING_EQUAL(user3->pw_passwd, "x", -1, ERROUT);
    ICUNIT_GOTO_EQUAL(user3->pw_uid, 2, -1, ERROUT); /* 2, from etc/group */
    ICUNIT_GOTO_EQUAL(user3->pw_gid, 2, -1, ERROUT); /* 2, from etc/group */
    ICUNIT_GOTO_STRING_EQUAL(user3->pw_gecos, "bin", -1, ERROUT);
    ICUNIT_GOTO_STRING_EQUAL(user3->pw_dir, "/bin", -1, ERROUT);
    ICUNIT_GOTO_STRING_EQUAL(user3->pw_shell, "/usr/sbin/nologin", -1, ERROUT);

    user4 = getpwuid(200);
    ICUNIT_GOTO_EQUAL(user4, nullptr, -1, ERROUT);

    user4 = getpwuid(-100);
    ICUNIT_GOTO_EQUAL(user4, nullptr, -1, ERROUT);

    user4 = getpwuid(100000);
    ICUNIT_GOTO_EQUAL(user4, nullptr, -1, ERROUT);

    (VOID)RecoveryFileEnv(pathList, 2); /* 2, group & passwd */
    return 0;
ERROUT:
    (VOID)RecoveryFileEnv(pathList, 2); /* 2, group & passwd */
    return -1;
}

VOID ItTestSys020(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
