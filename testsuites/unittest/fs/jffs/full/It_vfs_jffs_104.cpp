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
    INT32 fd[3] = { -1 };
    INT32 ret, len;
    CHAR filebuf[JFFS_STANDARD_NAME_LENGTH] = "1234567890";
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    off_t off;

    fd[0] = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY); // 0 means first fd
    ICUNIT_GOTO_NOT_EQUAL(fd[0], JFFS_IS_ERROR, fd[0], EXIT1); // 0 means first fd

    fd[1] = open(pathname1, O_NONBLOCK | O_RDONLY, HIGHEST_AUTHORITY); // 1 means second fd
    ICUNIT_GOTO_NOT_EQUAL(fd[1], JFFS_IS_ERROR, fd[1], EXIT2); // 1 means second fd

    ret = close(fd[1]); // 1 means second fd
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    ret = close(fd[0]); // 0 means first fd
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    fd[0] = open(pathname1, O_NONBLOCK | O_CREAT | O_WRONLY, HIGHEST_AUTHORITY); // 0 means first fd
    ICUNIT_GOTO_NOT_EQUAL(fd[0], JFFS_IS_ERROR, fd[0], EXIT1); // 0 means first fd

    fd[1] = open(pathname1, O_NONBLOCK | O_RDONLY, HIGHEST_AUTHORITY); // 1 means second fd
    ICUNIT_GOTO_NOT_EQUAL(fd[1], JFFS_IS_ERROR, fd[1], EXIT2); // 1 means second fd

    fd[2] = open(pathname1, O_NONBLOCK | O_WRONLY, HIGHEST_AUTHORITY); // 2 means third fd
    ICUNIT_GOTO_NOT_EQUAL(fd[2], JFFS_IS_ERROR, fd[2], EXIT3); // 2 means third fd

    ret = close(fd[2]); // 2 means third fd
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

    ret = close(fd[1]); // 1 means second fd
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    ret = close(fd[0]); // 0 means first fd
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    fd[0] = open(pathname1, O_NONBLOCK | O_CREAT | O_RDONLY, HIGHEST_AUTHORITY); // 0 means first fd
    ICUNIT_GOTO_NOT_EQUAL(fd[0], JFFS_IS_ERROR, fd[0], EXIT1); // 0 means first fd

    fd[1] = open(pathname1, O_NONBLOCK | O_RDONLY, HIGHEST_AUTHORITY); // 1 means second fd
    ICUNIT_GOTO_NOT_EQUAL(fd[1], JFFS_IS_ERROR, fd[1], EXIT2); // 1 means second fd

    ret = close(fd[1]); // 1 means second fd
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    ret = close(fd[0]); // 0 means first fd
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    fd[0] = open(pathname1, O_NONBLOCK | O_RDONLY, HIGHEST_AUTHORITY); // 0 means first fd
    ICUNIT_GOTO_NOT_EQUAL(fd[0], JFFS_IS_ERROR, fd[0], EXIT1); // 0 means first fd

    fd[1] = open(pathname1, O_NONBLOCK | O_RDONLY, HIGHEST_AUTHORITY); // 1 means second fd
    ICUNIT_GOTO_NOT_EQUAL(fd[1], JFFS_IS_ERROR, fd[1], EXIT2); // 1 means second fd

    fd[2] = open(pathname1, O_NONBLOCK | O_WRONLY, HIGHEST_AUTHORITY); // 2 means third fd
    ICUNIT_GOTO_NOT_EQUAL(fd[2], JFFS_IS_ERROR, fd[2], EXIT3); // 2 means third fd

    len = write(fd[0], filebuf, strlen(filebuf)); // 0 means first fd
    ICUNIT_GOTO_EQUAL(len, JFFS_IS_ERROR, len, EXIT3);
    ICUNIT_GOTO_EQUAL(errno, EACCES, errno, EXIT3);

    len = write(fd[2], filebuf, strlen(filebuf)); // 2 means third fd
    ICUNIT_GOTO_EQUAL(len, strlen(filebuf), len, EXIT3);

    off = lseek(fd[0], 0, SEEK_SET); // 0 means first fd
    ICUNIT_GOTO_EQUAL(off, JFFS_NO_ERROR, off, EXIT3);

    len = read(fd[1], readbuf, JFFS_STANDARD_NAME_LENGTH); // 1 means second fd
    ICUNIT_GOTO_EQUAL(len, strlen(filebuf), len, EXIT3);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, filebuf, readbuf, EXIT3);

    ret = close(fd[2]); // 2 means third fd
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

    ret = close(fd[1]); // 1 means second fd
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    ret = close(fd[0]); // 0 means first fd
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = unlink(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT3:
    close(fd[2]); // 2 means third fd
EXIT2:
    close(fd[1]); // 1 means second fd
EXIT1:
    close(fd[0]); // 0 means first fd
EXIT:
    unlink(pathname1);
    return JFFS_NO_ERROR;
}

/* *
* -@test IT_FS_JFFS_104
* -@tspec The Function test for open
* -@ttitle Open the file several times in read-only mode
* -@tprecon The filesystem module is open
* -@tbrief
1. create one file and opened it in read-write mode;
2. open the file several times in read-only mode
3. write and read these fd
4. close and remove all the file
* -@texpect
1. Return successed
2. Return successed
3. Return successed
4. Successful operation
* -@tprior 1
* -@tauto TRUE
* -@tremark
*/

VOID ItFsJffs104(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_104", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

