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
    INT32 ret;
    INT32 i;
    INT32 index = 0;
    INT32 fd[JFFS_CREATFILE_NUM] = {};
    CHAR bufname[JFFS_SHORT_ARRAY_LENGTH] = "123456";
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR bufname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR bufname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = "";

    ret = mkdir(pathname, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    strcat(bufname1, "/test0");
    strcat(bufname2, "/test1");
    ret = mkdir(bufname1, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = mkdir(bufname2, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    for (i = 0; i < JFFS_CREATFILE_NUM; i++) {
        snprintf(pathname1, JFFS_STANDARD_NAME_LENGTH, "/storage/test/test1/file%d.txt", index);
        fd[index] = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);

        if (fd[index] == -1) {
            break;
        }

        ret = close(fd[index]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

        index++;
    }

    ret = rename(bufname1, bufname2);
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT);

    index--;
    for (i = index; i >= 0; i--) {
        snprintf(pathname1, JFFS_STANDARD_NAME_LENGTH, "/storage/test/test1/file%d.txt", i);
        ret = unlink(pathname1);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    }
    ret = rmdir(bufname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = rmdir(bufname2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    ret = rmdir(pathname);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;

EXIT2:
    for (i = index; i >= 0; i--) {
        close(fd[i]);
    }
EXIT1:
    for (i = index; i >= 0; i--) {
        snprintf(pathname1, JFFS_STANDARD_NAME_LENGTH, "/storage/test/test1/file%d.txt", i);
        unlink(pathname1);
    }
EXIT:
    rmdir(bufname1);
    rmdir(bufname2);
    rmdir(pathname);
    return JFFS_NO_ERROR;
}

/*
 *
* -@test IT_FS_JFFS_807
* -@tspec The function test for filesystem
* -@ttitle test1 and not null test2 called API rename,rename test1 to test2
* -@tprecon The filesystem module is open
* -@tbrief
1. mkdir test1 and test2;
2. open creat five files in test1;
3. rename test1 to test2;
4. unlink files in test1 and rmdir test1 and test2.
* -@texpect
1. Return successed
2. Return successed
3. Return successed
4. Sucessful operation
* -@tprior 1
* -@tauto TRUE
* -@tremark
 *
 */

VOID ItFsJffs807(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_807", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}
