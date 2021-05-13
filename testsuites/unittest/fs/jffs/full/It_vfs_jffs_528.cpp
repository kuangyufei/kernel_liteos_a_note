/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
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

#include "It_vfs_jffs.h"

static VOID *PthreadF01(VOID *argument)
{
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR bufname[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR *realName = NULL;

    g_TestCnt++;

    realName = realpath(pathname1, bufname);
    ICUNIT_GOTO_NOT_EQUAL(realName, NULL, realName, EXIT);
    ICUNIT_GOTO_STRING_EQUAL(realName, pathname1, realName, EXIT);

    g_TestCnt++;

    return NULL;

EXIT:
    g_TestCnt++;
    return NULL;
}

static UINT32 Testcase(VOID)
{
    INT32 ret, fd;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    pthread_t newTh1;
    pthread_attr_t attr;

    g_TestCnt = 0;

    ret = mkdir(pathname1, S_IRWXU | S_IRWXG | S_IRWXO);

    PosixPthreadInit(&attr, 4); // init pthread  with attr 4

    ret = pthread_create(&newTh1, &attr, PthreadF01, NULL);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    while (g_TestCnt < 2) { // wait for test count 2 complete
        sleep(1);
    }

    ICUNIT_GOTO_EQUAL(g_TestCnt, 2, g_TestCnt, EXIT2); // compare g_TestCnt to 2

    ret = PosixPthreadDestroy(&attr, newTh1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    ret = remove(pathname1);

    return JFFS_NO_ERROR;
EXIT2:
    PosixPthreadDestroy(&attr, newTh1);
EXIT1:
    remove(pathname1);
EXIT:
    return JFFS_NO_ERROR;
}

/*
* -@test IT_FS_JFFS_528
* -@tspec The Function test for realpath
* -@ttitle Get the realpath about the directory in another task;
* -@tprecon The filesystem module is open
* -@tbrief
1. create a  directory;
2. use the function realpath to get the full-path in another task;
3. N/A;
4. N/A.
* -@texpect
1. Return successed
2. Sucessful operation
3. N/A
4. N/A
* -@tprior 1
* -@tauto TRUE
* -@tremark
 */

VOID ItFsJffs528(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_528", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}
