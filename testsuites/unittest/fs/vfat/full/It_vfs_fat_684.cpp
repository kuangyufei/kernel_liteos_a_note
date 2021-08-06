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

static const off_t LEN = 10;

static UINT32 TestCase(VOID)
{
    INT32 fd = 0;
    INT32 ret;
    INT32 i = 0;
    INT32 size = 0x4000000;
    CHAR pathname1[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
    struct statfs buf1 = { 0 };
    struct statfs buf3 = { 0 };
    struct stat buf5 = { 0 };
    struct stat buf6 = { 0 };

    fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd, FAT_IS_ERROR, fd, EXIT1);

    ret = statfs(pathname1, &buf1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    FatStatfsPrintf(buf1);

    ret = stat(pathname1, &buf5);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    FatStatPrintf(buf5);

    ret = fallocate(fd, FAT_FALLOCATE_KEEP_SIZE, 0, LEN);
    if (g_fatFilesystem == FAT_FILE_SYSTEMTYPE_EXFAT) {
        ICUNIT_GOTO_EQUAL(ret, FAT_IS_ERROR, ret, EXIT1);
    } else
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = fallocate64(fd, FAT_FALLOCATE_KEEP_SIZE, 0, size);
    if (g_fatFilesystem == FAT_FILE_SYSTEMTYPE_EXFAT) {
        ICUNIT_GOTO_EQUAL(ret, FAT_IS_ERROR, ret, EXIT1);
    } else {
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    }

    ret = statfs(pathname1, &buf3);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    FatStatfsPrintf(buf3);

    ret = fallocate(fd, FAT_FALLOCATE_KEEP_SIZE, 0, size);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = fallocate64(fd, FAT_FALLOCATE_KEEP_SIZE, 0, size);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    for (i = 0; i < FAT_SHORT_ARRAY_LENGTH; i++) {
        fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
        ICUNIT_GOTO_NOT_EQUAL(fd, FAT_IS_ERROR, fd, EXIT1);

        ret = fallocate(fd, FAT_FALLOCATE_KEEP_SIZE, 0, size);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        ret = fallocate64(fd, FAT_FALLOCATE_KEEP_SIZE, 0, size);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        ret = ftruncate(fd, 0x400);
        if (g_fatFilesystem == FAT_FILE_SYSTEMTYPE_EXFAT) {
            ICUNIT_GOTO_EQUAL(ret, FAT_IS_ERROR, ret, EXIT1);
        } else
            ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        ret = ftruncate64(fd, size);
        if (g_fatFilesystem == FAT_FILE_SYSTEMTYPE_EXFAT) {
            ICUNIT_GOTO_EQUAL(ret, FAT_IS_ERROR, ret, EXIT1);
        } else
            ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        ret = ftruncate64(fd, 0x400);
        if (g_fatFilesystem == FAT_FILE_SYSTEMTYPE_EXFAT) {
            ICUNIT_GOTO_EQUAL(ret, FAT_IS_ERROR, ret, EXIT1);
        } else
            ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        ret = stat(pathname1, &buf6);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
        FatStatPrintf(buf6);
        if (g_fatFilesystem == FAT_FILE_SYSTEMTYPE_EXFAT)
            ICUNIT_GOTO_EQUAL(buf6.st_size, 0, buf6.st_size, EXIT1);
        else
            ICUNIT_GOTO_EQUAL(buf6.st_size, 0x400, buf6.st_size, EXIT1);

        ret = fallocate(fd, FAT_FALLOCATE_KEEP_SIZE, 0, size);
        if (g_fatFilesystem == FAT_FILE_SYSTEMTYPE_EXFAT) {
            ICUNIT_GOTO_EQUAL(ret, FAT_IS_ERROR, ret, EXIT1);
        } else {
            ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
        }

        ret = fallocate64(fd, FAT_FALLOCATE_KEEP_SIZE, LEN, size);
        if (g_fatFilesystem == FAT_FILE_SYSTEMTYPE_EXFAT) {
            ICUNIT_GOTO_EQUAL(ret, FAT_IS_ERROR, ret, EXIT1);
        } else
            ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        ret = close(fd);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    }

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
* -@test IT_FS_VFAT_684
* -@tspec The function test for fallocate
* -@ttitle Fallocate apply space for the same file fd multiple times
* -@tprecon The filesystem module is open
* -@tbrief
1. use the open to open one file;
2. use the function fallocate to apply the space for the same file fd multiple times;
3. compare the file size before and after fallocate;
4. close and remove the file.
* -@texpect
1. Return successed
2. Return successed
3. Return successed
4. Sucessful operation
* -@tprior 1
* -@tauto TRUE
* -@tremark
*/

VOID ItFsFat684(VOID)
{
    TEST_ADD_CASE("IT_FS_FAT_684", TestCase, TEST_VFS, TEST_VFAT, TEST_LEVEL0, TEST_FUNCTION);
}
