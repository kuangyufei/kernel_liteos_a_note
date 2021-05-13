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
    INT32 fd = -1;
    INT32 fd1 = -1;
    INT32 ret, len;
    off_t off;
    CHAR filebuf[JFFS_STANDARD_NAME_LENGTH] = "1234567890";
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = " ";
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };

    ret = mkdir(pathname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = chdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    strcat_s(pathname1, sizeof(pathname1), "/120.txt");
    fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT1);

    len = write(fd, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, JFFS_SHORT_ARRAY_LENGTH, len, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    fd1 = open(pathname1, O_NONBLOCK | O_RDWR, 0x222);
    ICUNIT_GOTO_NOT_EQUAL(fd1, -1, fd1, EXIT2);

    len = write(fd1, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, JFFS_SHORT_ARRAY_LENGTH, len, EXIT2);

    off = lseek(fd1, 0, SEEK_SET);
    ICUNIT_GOTO_EQUAL(off, 0, off, EXIT2);

    len = read(fd1, readbuf, JFFS_STANDARD_NAME_LENGTH - 1);
    ICUNIT_GOTO_EQUAL(len, 10, len, EXIT2); // 10 means length of actually read data
    ICUNIT_GOTO_STRING_EQUAL(readbuf, filebuf, readbuf, EXIT2);

    ret = close(fd1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    ret = unlink(pathname1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    ret = chdir("..");
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = remove(pathname2);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT2:
    close(fd1);
EXIT1:
    close(fd);
    remove(pathname1);
EXIT:
    remove(pathname2);
    return JFFS_NO_ERROR;
}

/* *
* -@test IT_FS_JFFS_118
* -@tspec The Function test for open
* -@ttitle Create a file with permission 0x222 and open an existing file
* -@tprecon The filesystem module is open
* -@tbrief
1. create a file with permission 0x222 and open an existing file ;
2. read and write the file;
3. delete the file;
4. N/A
* -@texpect
1. Return successed
2. Return successed
3. Sucessful operation
4. N/A
* -@tprior 1
* -@tauto TRUE
* -@tremark
*/

VOID ItFsJffs118(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_118", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

