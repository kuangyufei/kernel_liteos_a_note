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

#include "It_vfs_fat.h"
static const int FAT_FORMAT_SEC = 8;
static const int SIZE_4K = 4096;
static const int WRITE_TIME = 250;

static UINT32 TestCase(VOID)
{
    INT32 ret;
    INT32 fd = 0;
    INT32 len = 0;
    INT32 i = FAT_SHORT_ARRAY_LENGTH;
    CHAR pathname1[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
    CHAR buffile[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
    CHAR filebuf[FAT_STANDARD_NAME_LENGTH] = "12345678901234567890";
    struct statfs buf1 = { 0 };
    struct statfs buf2 = { 0 };
    struct statfs buf3 = { 0 };
    struct statfs buf4 = { 0 };

    ret = chdir("/");
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT0);

    ret = umount(FAT_MOUNT_DIR);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = format(FAT_DEV_PATH, FAT_FORMAT_SEC, FAT_FILE_SYSTEMTYPE_AUTO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = mount(FAT_DEV_PATH, FAT_MOUNT_DIR, FAT_FILESYS_TYPE, 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = mkdir(pathname1, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = strcat_s(pathname1, FAT_STANDARD_NAME_LENGTH, "/dir");
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = mkdir(pathname1, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = strcat_s(buffile, FAT_STANDARD_NAME_LENGTH, "/dir/files");
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    fd = open(buffile, O_NONBLOCK | O_CREAT | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd, FAT_IS_ERROR, fd, EXIT3);

    ret = statfs(pathname1, &buf1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);
    FatStatfsPrintf(buf1);

    ret = statfs(buffile, &buf2);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);
    FatStatfsPrintf(buf2);

    ICUNIT_GOTO_EQUAL(buf1.f_type, buf2.f_type, buf1.f_type, EXIT3);
    ICUNIT_GOTO_EQUAL(buf1.f_bavail, buf2.f_bavail, buf1.f_bavail, EXIT3);
    ICUNIT_GOTO_EQUAL(buf1.f_bsize, buf2.f_bsize, buf1.f_bsize, EXIT3);
    ICUNIT_GOTO_EQUAL(buf1.f_blocks, buf2.f_blocks, buf1.f_blocks, EXIT3);
    ICUNIT_GOTO_EQUAL(buf1.f_bfree, buf2.f_bfree, buf1.f_bfree, EXIT3);
    ICUNIT_GOTO_EQUAL(buf1.f_files, buf2.f_files, buf1.f_files, EXIT3);

    while (i--) {
        len = write(fd, filebuf, strlen(filebuf));
        ICUNIT_GOTO_EQUAL((UINT32)len, strlen(filebuf), len, EXIT3);
    }

    ret = statfs(pathname1, &buf3);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);
    FatStatfsPrintf(buf3);

    ret = statfs(buffile, &buf4);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);
    FatStatfsPrintf(buf4);

    ICUNIT_GOTO_EQUAL(buf3.f_type, 0x4d44, buf3.f_type, EXIT3);
    ICUNIT_GOTO_EQUAL(buf4.f_type, 0x4d44, buf4.f_type, EXIT3);
    ICUNIT_GOTO_EQUAL(buf3.f_namelen, 0xFF, buf3.f_namelen, EXIT3);
    ICUNIT_GOTO_EQUAL(buf4.f_namelen, 0xFF, buf4.f_namelen, EXIT3);
    ICUNIT_GOTO_EQUAL(buf3.f_bsize, SIZE_4K, buf3.f_bsize, EXIT3);
    ICUNIT_GOTO_EQUAL(buf4.f_bsize, SIZE_4K, buf4.f_bsize, EXIT3);
    ICUNIT_GOTO_EQUAL(buf3.f_bfree + 1, buf1.f_bfree, buf3.f_bfree + 1, EXIT3);
    ICUNIT_GOTO_EQUAL(buf4.f_bfree + 1, buf2.f_bfree, buf4.f_bfree + 1, EXIT3);

    i = WRITE_TIME;
    while (i--) {
        len = write(fd, filebuf, strlen(filebuf));
        ICUNIT_GOTO_EQUAL((UINT32)len, strlen(filebuf), len, EXIT3);
    }

    ret = statfs(pathname1, &buf3);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);
    FatStatfsPrintf(buf3);

    ret = statfs(buffile, &buf4);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);
    FatStatfsPrintf(buf4);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);

    ret = unlink(buffile);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = rmdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = rmdir(FAT_PATH_NAME);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    return FAT_NO_ERROR;
EXIT3:
    close(fd);
EXIT2:
    FatStrcat2(pathname1, "/dir/files", strlen(pathname1));
    remove(pathname1);
EXIT1:
    FatStrcat2(pathname1, "/dir", strlen(pathname1));
    remove(pathname1);
EXIT:
    remove(FAT_PATH_NAME);
EXIT0:
    return FAT_NO_ERROR;
}

/* *
* -@test IT_FS_FAT_026
* -@tspec The function test for filesystem
* -@ttitle Use the statfs to view the information for the filesystem
* -@tprecon The filesystem module is open
* -@tbrief
1. create one file;
2. use the statfs to view the information for the filesystem;
3. write the file and view the information once again;
4. delete all the file and directory.
* -@texpect
* -@tprior 1
* -@tauto TRUE
* -@tremark
* Successful operation
* -@tprior 1
* -@tauto TRUE
* -@tremark
*/

VOID ItFsFat026(VOID)
{
    TEST_ADD_CASE("IT_FS_FAT_026", TestCase, TEST_VFS, TEST_VFAT, TEST_LEVEL0, TEST_FUNCTION);
}
