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

static UINT32 Testcase(VOID)
{
    INT32 ret, fd;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_MAIN_DIR0 };
    CHAR bufname[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR *realName = NULL;

    ret = mkdir(pathname1, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    realName = realpath(pathname1, bufname);
    ICUNIT_GOTO_NOT_EQUAL(realName, NULL, realName, EXIT);
    ICUNIT_GOTO_STRING_EQUAL(realName, pathname1, realName, EXIT);
    printf("%s-%d \n", __FUNCTION__, __LINE__);

    strcat(pathname2, "/////");
    strcat(pathname2, "test");

    realName = realpath(pathname2, bufname);
    ICUNIT_GOTO_NOT_EQUAL(realName, NULL, realName, EXIT);
    ICUNIT_GOTO_STRING_EQUAL(realName, pathname1, realName, EXIT);

    ret = rmdir(pathname2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;

EXIT:
    remove(pathname1);
    return JFFS_NO_ERROR;
}

/*
* -@test IT_FS_JFFS_526
* -@tspec The Function test for realpath
* -@ttitle Get the realpath about the directory which contains many connection symbols;
* -@tprecon The filesystem module is open
* -@tbrief
1. create a directory;
2. use the function realpath to get the full-path;
3. rmdir the directory;
4. N/A.
* -@texpect
1. Return successed
2. Return successed
3. Successful operation
4. N/A
* -@tprior 1
* -@tauto TRUE
* -@tremark
 */

VOID ItFsJffs526(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_526", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}
