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


#include "It_vfs_fat.h"

static UINT32 TestCase(VOID)
{
    INT32 fd = 0;
    INT32 ret;
    INT32 len;
    CHAR pathname1[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
    CHAR const writebuf[FAT_STANDARD_NAME_LENGTH] = "0123456789";
    CHAR readbuf[FAT_STANDARD_NAME_LENGTH] = "";
    struct statfs buf1 = { 0 };
    struct statfs buf2 = { 0 };
    off_t off;

    fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd, FAT_IS_ERROR, fd, EXIT1);

    ret = statfs(pathname1, &buf1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    FatStatfsPrintf(buf1);

    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = statfs(pathname1, &buf2);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    FatStatfsPrintf(buf2);

    ICUNIT_GOTO_EQUAL(buf1.f_type, buf2.f_type, buf1.f_type, EXIT1);
    ICUNIT_GOTO_EQUAL(buf1.f_bsize, buf2.f_bsize, buf1.f_bsize, EXIT1);
    ICUNIT_GOTO_EQUAL(buf1.f_blocks, buf2.f_blocks, buf1.f_blocks, EXIT1);
    ICUNIT_GOTO_EQUAL(buf1.f_files, buf2.f_files, buf1.f_files, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd, FAT_IS_ERROR, fd, EXIT1);

    if (g_fatFilesystem == FAT_FILE_SYSTEMTYPE_EXFAT) {
        ICUNIT_GOTO_EQUAL(ret, FAT_IS_ERROR, ret, EXIT1);

        ret = statfs(pathname1, &buf2);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
        FatStatfsPrintf(buf2);

        ICUNIT_GOTO_EQUAL(buf1.f_type, buf2.f_type, buf1.f_type, EXIT1);
        ICUNIT_GOTO_EQUAL(buf1.f_bavail, buf2.f_bavail, buf1.f_bavail, EXIT1);
        ICUNIT_GOTO_EQUAL(buf1.f_bsize, buf2.f_bsize, buf1.f_bsize, EXIT1);
        ICUNIT_GOTO_EQUAL(buf1.f_blocks, buf2.f_blocks, buf1.f_blocks, EXIT1);
        ICUNIT_GOTO_EQUAL(buf1.f_bfree, buf2.f_bfree, buf1.f_bfree, EXIT1);
        ICUNIT_GOTO_EQUAL(buf1.f_files, buf2.f_files, buf1.f_files, EXIT1);
    } else {
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        ret = statfs(pathname1, &buf2);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
        FatStatfsPrintf(buf2);

        ICUNIT_GOTO_EQUAL(buf1.f_type, buf2.f_type, buf1.f_type, EXIT1);
        ICUNIT_GOTO_EQUAL(buf1.f_bsize, buf2.f_bsize, buf1.f_bsize, EXIT1);
        ICUNIT_GOTO_EQUAL(buf1.f_blocks, buf2.f_blocks, buf1.f_blocks, EXIT1);
        ICUNIT_GOTO_EQUAL(buf1.f_files, buf2.f_files, buf1.f_files, EXIT1);
    }

    len = write(fd, writebuf, strlen(writebuf));
    ICUNIT_GOTO_EQUAL((UINT32)len, strlen(writebuf), len, EXIT1);

    off = lseek(fd, 0, SEEK_SET);
    ICUNIT_GOTO_EQUAL(off, 0, off, EXIT1);

    len = read(fd, readbuf, FAT_STANDARD_NAME_LENGTH - 1);
    ICUNIT_GOTO_EQUAL((UINT32)len, strlen(writebuf), len, EXIT1);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, writebuf, readbuf, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = remove(pathname1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd, FAT_IS_ERROR, fd, EXIT1);

    ret = statfs(pathname1, &buf1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    FatStatfsPrintf(buf1);

    ret = statfs(pathname1, &buf2);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    FatStatfsPrintf(buf2);

    ICUNIT_GOTO_EQUAL(buf1.f_type, buf2.f_type, buf1.f_type, EXIT1);
    ICUNIT_GOTO_EQUAL(buf1.f_bavail, buf2.f_bavail, buf1.f_bavail, EXIT1);
    ICUNIT_GOTO_EQUAL(buf1.f_bsize, buf2.f_bsize, buf1.f_bsize, EXIT1);
    ICUNIT_GOTO_EQUAL(buf1.f_blocks, buf2.f_blocks, buf1.f_blocks, EXIT1);
    ICUNIT_GOTO_EQUAL(buf1.f_bfree, buf2.f_bfree, buf1.f_bfree, EXIT1);
    ICUNIT_GOTO_EQUAL(buf1.f_files, buf2.f_files, buf1.f_files, EXIT1);

    if (g_fatFilesystem == FAT_FILE_SYSTEMTYPE_EXFAT) {
        ICUNIT_GOTO_EQUAL(ret, FAT_IS_ERROR, ret, EXIT1);

        ret = statfs(pathname1, &buf2);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
        FatStatfsPrintf(buf2);

        ICUNIT_GOTO_EQUAL(buf1.f_type, buf2.f_type, buf1.f_type, EXIT1);
        ICUNIT_GOTO_EQUAL(buf1.f_bavail, buf2.f_bavail, buf1.f_bavail, EXIT1);
        ICUNIT_GOTO_EQUAL(buf1.f_bsize, buf2.f_bsize, buf1.f_bsize, EXIT1);
        ICUNIT_GOTO_EQUAL(buf1.f_blocks, buf2.f_blocks, buf1.f_blocks, EXIT1);
        ICUNIT_GOTO_EQUAL(buf1.f_bfree, buf2.f_bfree, buf1.f_bfree, EXIT1);
        ICUNIT_GOTO_EQUAL(buf1.f_files, buf2.f_files, buf1.f_files, EXIT1);
    } else {
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        ret = statfs(pathname1, &buf2);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
        FatStatfsPrintf(buf2);

        ICUNIT_GOTO_EQUAL(buf1.f_type, buf2.f_type, buf1.f_type, EXIT1);
        ICUNIT_GOTO_EQUAL(buf1.f_bsize, buf2.f_bsize, buf1.f_bsize, EXIT1);
        ICUNIT_GOTO_EQUAL(buf1.f_blocks, buf2.f_blocks, buf1.f_blocks, EXIT1);
        ICUNIT_GOTO_EQUAL(buf1.f_files, buf2.f_files, buf1.f_files, EXIT1);
    }

    len = write(fd, writebuf, strlen(writebuf));
    ICUNIT_GOTO_EQUAL((UINT32)len, strlen(writebuf), len, EXIT1);

    off = lseek(fd, 0, SEEK_SET);
    ICUNIT_GOTO_EQUAL(off, 0, off, EXIT1);

    len = read(fd, readbuf, FAT_STANDARD_NAME_LENGTH - 1);
    ICUNIT_GOTO_EQUAL((UINT32)len, strlen(writebuf), len, EXIT1);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, writebuf, readbuf, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = remove(pathname1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    return FAT_NO_ERROR;
EXIT1:
    close(fd);
EXIT:
    remove(pathname1);
    return FAT_NO_ERROR;
}

/* *
* -@test IT_FS_FAT_677
* -@tspec The function test for fallocate fallocate64
* -@ttitle The function test for fallocate fallocate64 with the length is 2GB+1
* -@tprecon The filesystem module is open
* -@tbrief
1. use the open to create one file;
2. use the function fallocate fallocate64 to apply the space with the length is 2GB+1;
3. close the file;
4. remove the file.
* -@texpect
1. Return successed
2. Return successed
3. Return successed
4. Successful operation
* -@tprior 1
* -@tauto TRUE
* -@tremark
*/

VOID ItFsFat677(VOID)
{
    TEST_ADD_CASE("IT_FS_FAT_677", TestCase, TEST_VFS, TEST_VFAT, TEST_LEVEL2, TEST_FUNCTION);
}
