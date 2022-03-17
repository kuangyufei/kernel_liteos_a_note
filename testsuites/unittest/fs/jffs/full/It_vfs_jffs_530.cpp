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
    INT32 i = 0;
    INT32 ret;
    INT32 fd;
    CHAR pathname1[JFFS_NAME_LIMITTED_SIZE] = { JFFS_MAIN_DIR0 };
    CHAR bufname[JFFS_NAME_LIMITTED_SIZE] = "";
    CHAR *realName = NULL;
    BOOL bool1 = 0;

    strcat(pathname1, "/");

    // PATH_MAX test. The dirname has occupied 9 bytes.
    while (i < 246) { // generate 246 length name
        i++;
        strcat(pathname1, "t");
    }
    ICUNIT_GOTO_EQUAL(strlen(pathname1), 255, strlen(pathname1), EXIT); // compare pathname length with 255
    fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    realName = realpath(pathname1, bufname);
    ICUNIT_GOTO_NOT_EQUAL(realName, NULL, realName, EXIT);
    ICUNIT_GOTO_STRING_EQUAL(realName, pathname1, realName, EXIT);

    ret = remove(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;

EXIT:
    close(fd);
    remove(pathname1);
    return JFFS_NO_ERROR;
}

/*
* -@test IT_FS_JFFS_616
* -@tspec The Function test for realpath
* -@ttitle Get the realpath about the directory which is multi-level directory;
* -@tprecon The filesystem module is open
* -@tbrief
1. create a  directory;
2. use the function realpath to get the full-path in another task;
3. N/A;
4. N/A.
* -@texpect
1. Return successed
2. Successful operation
3. N/A
4. N/A
* -@tprior 1
* -@tauto TRUE
* -@tremark
 */

VOID ItFsJffs530(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_530", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}
