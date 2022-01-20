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
#include <grp.h>

#define GROUPFILE "/etc/group"

STATIC INT32 TestCase0(void)
{
    struct group getNam1 = { nullptr };
    struct group *getNam2 = nullptr;
    struct group getData1 = { nullptr };
    struct group *getData2 = nullptr;
    struct group *groupRet = nullptr;
    CHAR buf[1000]; /* 1000, buffer for test */
    size_t len = sizeof(buf);

    INT32 ret = getgrgid_r(0, &getNam1, buf, len, &getNam2);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ICUNIT_ASSERT_NOT_EQUAL(getNam2, nullptr, getNam2);
    ICUNIT_ASSERT_STRING_EQUAL(getNam2->gr_name, "root", getNam2->gr_name);
    ICUNIT_ASSERT_STRING_EQUAL(getNam2->gr_passwd, "x", getNam2->gr_passwd);
    ICUNIT_ASSERT_EQUAL(getNam2->gr_gid, 0, getNam2->gr_gid);

    groupRet = getgrgid(0);
    ICUNIT_ASSERT_NOT_EQUAL(groupRet, nullptr, groupRet);
    ICUNIT_ASSERT_STRING_EQUAL(groupRet->gr_name, "root", groupRet->gr_name);
    ICUNIT_ASSERT_STRING_EQUAL(groupRet->gr_passwd, "x", groupRet->gr_passwd);
    ICUNIT_ASSERT_EQUAL(groupRet->gr_gid, 0, groupRet->gr_gid);

    groupRet = getgrnam("root");
    ICUNIT_ASSERT_NOT_EQUAL(groupRet, nullptr, groupRet);
    ICUNIT_ASSERT_STRING_EQUAL(groupRet->gr_name, "root", groupRet->gr_name);
    ICUNIT_ASSERT_STRING_EQUAL(groupRet->gr_passwd, "x", groupRet->gr_passwd);
    ICUNIT_ASSERT_EQUAL(groupRet->gr_gid, 0, groupRet->gr_gid);

    ret = getgrnam_r("root", &getData1, buf, len, &getData2);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_NOT_EQUAL(getData2, nullptr, getData2);
    ICUNIT_ASSERT_STRING_EQUAL(getData2->gr_name, "root", getData2->gr_name);
    ICUNIT_ASSERT_STRING_EQUAL(getData2->gr_passwd, "x", getData2->gr_passwd);
    ICUNIT_ASSERT_EQUAL(getData2->gr_gid, 0, getData2->gr_gid);
    return 0;
}

STATIC INT32 TestCase1(void)
{
    INT32 len = 1000;
    CHAR buf[1000];
    struct group getNam1 = { nullptr };
    struct group *getNam2 = nullptr;
    struct group getData1 = { nullptr };
    struct group *getData2 = nullptr;
    struct group *groupRet = nullptr;

    INT32 ret = getgrgid_r(-1, &getNam1, buf, len, &getNam2);
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(errno, EAFNOSUPPORT, errno);
    errno = 0;

    groupRet = getgrgid(-1);
    ICUNIT_ASSERT_EQUAL(groupRet, 0, groupRet);
    ICUNIT_ASSERT_EQUAL(errno, EAFNOSUPPORT, errno);
    errno = 0;

    groupRet = getgrnam("null");
    ICUNIT_ASSERT_EQUAL(groupRet, nullptr, groupRet);
    ICUNIT_ASSERT_EQUAL(errno, EAFNOSUPPORT, errno);
    errno = 0;

    ret = getgrnam_r("null", &getData1, buf, len, &getData2);
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(errno, EAFNOSUPPORT, errno);
    errno = 0;

    return 0;
}

STATIC INT32 TestCase(void)
{
    CHAR *pathList[] = {"/etc/group"};
    CHAR *streamList[] = {g_groupFileStream};
    INT32 streamLen[] = {strlen(g_groupFileStream)};

    INT32 ret = PrepareFileEnv(pathList, streamList, streamLen, 1);
    if (ret != 0) {
        printf("error: need some env file, but prepare is not ok");
        (VOID)RecoveryFileEnv(pathList, 1);
        return -1;
    }

    ret = TestCase0();
    ICUNIT_GOTO_EQUAL(ret, 0, ret, ERROUT);

    ret = TestCase1();
    ICUNIT_GOTO_EQUAL(ret, 0, re, ERROUT);

    (VOID)RecoveryFileEnv(pathList, 1);
    return 0;
ERROUT:
    (VOID)RecoveryFileEnv(pathList, 1);
    return -1;
}

VOID ItTestSys025(VOID)
{
    TEST_ADD_CASE("IT_TEST_SYS_025", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}

