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

static UINT32 TestCase(VOID)
{
    struct passwd *user1 = nullptr;
    struct passwd *user2 = nullptr;

    user1 = getpwent();
    ICUNIT_ASSERT_NOT_EQUAL(user1, NULL, -1);
    ICUNIT_ASSERT_STRING_EQUAL(user1->pw_name, "root", -1);
    ICUNIT_ASSERT_STRING_EQUAL(user1->pw_passwd, "x", -1);
    ICUNIT_ASSERT_EQUAL(user1->pw_uid, 0, -1);
    ICUNIT_ASSERT_EQUAL(user1->pw_gid, 0, -1);
    ICUNIT_ASSERT_STRING_EQUAL(user1->pw_gecos, "root", -1);
    ICUNIT_ASSERT_STRING_EQUAL(user1->pw_dir, "/root", -1);
    ICUNIT_ASSERT_STRING_EQUAL(user1->pw_shell, "/bin/bash", -1);

    user1 = getpwent();
    ICUNIT_ASSERT_NOT_EQUAL(user1, NULL, -1);
    ICUNIT_ASSERT_STRING_EQUAL(user1->pw_name, "daemon", -1);
    ICUNIT_ASSERT_STRING_EQUAL(user1->pw_passwd, "x", -1);
    ICUNIT_ASSERT_EQUAL(user1->pw_uid, 1, -1);
    ICUNIT_ASSERT_EQUAL(user1->pw_gid, 1, -1);
    ICUNIT_ASSERT_STRING_EQUAL(user1->pw_gecos, "daemon", -1);
    ICUNIT_ASSERT_STRING_EQUAL(user1->pw_dir, "/usr/sbin", -1);
    ICUNIT_ASSERT_STRING_EQUAL(user1->pw_shell, "/usr/sbin/nologin", -1);

    user1 = getpwent();
    ICUNIT_ASSERT_NOT_EQUAL(user1, NULL, -1);
    ICUNIT_ASSERT_STRING_EQUAL(user1->pw_name, "bin", -1);
    ICUNIT_ASSERT_STRING_EQUAL(user1->pw_passwd, "x", -1);
    ICUNIT_ASSERT_EQUAL(user1->pw_uid, 2, -1);
    ICUNIT_ASSERT_EQUAL(user1->pw_gid, 2, -1);
    ICUNIT_ASSERT_STRING_EQUAL(user1->pw_gecos, "bin", -1);
    ICUNIT_ASSERT_STRING_EQUAL(user1->pw_dir, "/bin", -1);
    ICUNIT_ASSERT_STRING_EQUAL(user1->pw_shell, "/usr/sbin/nologin", -1);

    setpwent();
    user2 = getpwent();
    ICUNIT_ASSERT_NOT_EQUAL(user2, NULL, -1);
    ICUNIT_ASSERT_STRING_EQUAL(user2->pw_name, user1->pw_name, -1);

    return 0;
}

VOID ItTestSys022(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
