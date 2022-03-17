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
    INT32 fd;
    INT32 ret = 0;
    INT32 len = 0;
    INT32 size = 0xffff;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR *writebuf = NULL;
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "";
    struct statfs buf1 = { 0 };
    struct stat buf2 = { 0 };
    struct stat buf3 = { 0 };
    off_t off;

    writebuf = (CHAR *)malloc(size);
    ICUNIT_GOTO_NOT_EQUAL(writebuf, NULL, writebuf, EXIT);
    memset(writebuf, 0x61, size);

    fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT1);

    len = write(fd, writebuf, size);
    ICUNIT_GOTO_EQUAL(len, size, len, EXIT1);

    ret = statfs(pathname1, &buf1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    JffsStatfsPrintf(buf1);

    ret = stat(pathname1, &buf3);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    JffsStatPrintf(buf3);

    ret = fallocate(fd, 1, 0, size + JFFS_STANDARD_NAME_LENGTH);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT1);
    ICUNIT_GOTO_EQUAL(errno, EBADF, errno, EXIT1);

    ret = fstat(fd, &buf2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    JffsStatPrintf(buf2);

    ICUNIT_GOTO_EQUAL(buf3.st_size, size, buf3.st_size, EXIT1);

    len = write(fd, writebuf, size);
    ICUNIT_GOTO_EQUAL(len, size, len, EXIT1);

    off = lseek(fd, 0, SEEK_SET);
    ICUNIT_GOTO_EQUAL(off, 0, off, EXIT1);

    len = read(fd, readbuf, JFFS_STANDARD_NAME_LENGTH - 1);
    ICUNIT_GOTO_EQUAL(len, JFFS_STANDARD_NAME_LENGTH - 1, len, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = remove(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    free(writebuf);

    return JFFS_NO_ERROR;
EXIT1:
    close(fd);
EXIT:
    remove(pathname1);
    free(writebuf);
    return JFFS_NO_ERROR;
}

/*
* -@test IT_FS_JFFS_574
* -@tspec The function test for fallocate
* -@ttitle Fallocate allocates space while the offset + length is more than the size of the file
* -@tprecon The filesystem module is open
* -@tbrief
1. use the open to open one non-empty file;
2. use the function fallocate to allocates space while the offset + length is more than the size of the file ;
3. compare the file size before and after fallocate;
4. close and remove the file.
* -@texpect
1. Return successed
2. Return successed
3. Return successed
4. Successful operation
* -@tprior 1
* -@tauto TRUE
* -@tremark
 */

VOID ItFsJffs574(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_574", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}
