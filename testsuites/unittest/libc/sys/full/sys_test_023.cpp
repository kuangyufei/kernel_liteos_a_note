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
#include "grp.h"

STATIC UINT32 TestCase(VOID)
{
    struct group *grp1 = nullptr;
    struct group *grp2 = nullptr;
    struct group *grp3 = nullptr;
    struct group *grp4 = nullptr;
    INT32 ret;
    CHAR *pathList[] = {"/etc/group"};
    CHAR *streamList[] = {g_groupFileStream};
    INT32 streamLen[] = {strlen(g_groupFileStream)};

    ret = PrepareFileEnv(pathList, streamList, streamLen, 1);
    if (ret != 0) {
        printf("error: need some env file, but prepare is not ok");
        (VOID)RecoveryFileEnv(pathList, 1);
        return -1;
    }

    grp1 = getgrent();
    ICUNIT_GOTO_NOT_EQUAL(grp1, nullptr, -1, ERROUT);
    ICUNIT_GOTO_STRING_EQUAL(grp1->gr_name, "root", -1, ERROUT);
    ICUNIT_GOTO_STRING_EQUAL(grp1->gr_passwd, "x", -1, ERROUT);
    ICUNIT_GOTO_EQUAL(grp1->gr_gid, 0, -1, ERROUT);

    grp2 = getgrent();
    ICUNIT_GOTO_NOT_EQUAL(grp2, nullptr, -1, ERROUT);
    ICUNIT_GOTO_STRING_EQUAL(grp2->gr_name, "daemon", -1, ERROUT);
    ICUNIT_GOTO_STRING_EQUAL(grp2->gr_passwd, "x", -1, ERROUT);
    ICUNIT_GOTO_EQUAL(grp2->gr_gid, 1, -1, ERROUT);

    grp3 = getgrent();
    ICUNIT_GOTO_NOT_EQUAL(grp3, nullptr, -1, ERROUT);
    ICUNIT_GOTO_STRING_EQUAL(grp3->gr_name, "bin", -1, ERROUT);
    ICUNIT_GOTO_STRING_EQUAL(grp3->gr_passwd, "x", -1, ERROUT);
    ICUNIT_GOTO_EQUAL(grp3->gr_gid, 2, -1, ERROUT); /* 2, from etc/group */

    setgrent();

    grp4 = getgrent();
    ICUNIT_GOTO_NOT_EQUAL(grp4, nullptr, -1, ERROUT);
    ICUNIT_GOTO_STRING_EQUAL(grp1->gr_name, grp4->gr_name, -1, ERROUT);

    setgrent();
    (VOID)RecoveryFileEnv(pathList, 1);
    return 0;
ERROUT:
    setgrent();
    (VOID)RecoveryFileEnv(pathList, 1);
    return -1;
}

VOID ItTestSys023(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
