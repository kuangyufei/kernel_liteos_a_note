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

#include "It_vfs_jffs.h"

static UINT32 TestCase(VOID)
{
    INT32 fd = -1;
    INT32 fd1 = -1;
    INT32 ret, len;
    INT32 i = 0;
    INT32 j = 0;
    signed long long offset;
    CHAR filebuf[JFFS_STANDARD_NAME_LENGTH] = "0000000000111111111122222222223333333333";
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "liteos";
    CHAR bufname[JFFS_SHORT_ARRAY_LENGTH] = "";
    CHAR pathname1[JFFS_NAME_LIMITTED_SIZE] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_NAME_LIMITTED_SIZE] = { JFFS_PATH_NAME0 };
    CHAR pathname3[JFFS_SHORT_ARRAY_LENGTH][JFFS_NAME_LIMITTED_SIZE] = {0};
    CHAR pathname4[JFFS_SHORT_ARRAY_LENGTH][JFFS_NAME_LIMITTED_SIZE] = {0};
    CHAR pathname5[JFFS_NAME_LIMITTED_SIZE] = { JFFS_PATH_NAME0 };
    CHAR pathname6[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR buffile[JFFS_NAME_LIMITTED_SIZE] = { JFFS_PATH_NAME0 };
    struct stat buf1 = { 0 };
    struct stat buf2 = { 0 };
    off_t off;

    ret = mkdir(pathname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = chdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = stat(buffile, &buf1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    JffsStatPrintf(buf1);

    for (i = 0; i < JFFS_PRESSURE_CYCLES; i++) {
        (void)memset_s(readbuf, JFFS_STANDARD_NAME_LENGTH, 0, strlen(readbuf));
        (void)memset_s(pathname2, JFFS_NAME_LIMITTED_SIZE, 0, JFFS_NAME_LIMITTED_SIZE);
        strcat_s(pathname2, JFFS_NAME_LIMITTED_SIZE, pathname6);

        for (j = 0; j < JFFS_SHORT_ARRAY_LENGTH; j++) {
            (void)memset_s(pathname3[j], JFFS_NAME_LIMITTED_SIZE, 0, JFFS_NAME_LIMITTED_SIZE);

            snprintf_s(bufname, JFFS_SHORT_ARRAY_LENGTH, JFFS_SHORT_ARRAY_LENGTH - 1, "/test%d", j);
            strcat_s(pathname2, JFFS_NAME_LIMITTED_SIZE, bufname);
            (void)strcpy_s(pathname3[j], JFFS_NAME_LIMITTED_SIZE, pathname2);

            ret = mkdir(pathname3[j], HIGHEST_AUTHORITY);
            ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

            (void)strcpy_s(pathname4[j], JFFS_NAME_LIMITTED_SIZE, pathname3[j]);
            strcat_s(pathname4[j], JFFS_NAME_LIMITTED_SIZE, ".txt");
        }

        for (j = 0; j < JFFS_SHORT_ARRAY_LENGTH; j++) {
            fd = open(pathname4[j], O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
            ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT2);

            off = lseek(fd, 0, SEEK_SET);
            ICUNIT_GOTO_EQUAL(off, 0, off, EXIT2);

            len = read(fd, readbuf, JFFS_STANDARD_NAME_LENGTH);
            ICUNIT_GOTO_EQUAL(len, 0, len, EXIT2);

            len = write(fd, filebuf, strlen(filebuf));
            ICUNIT_GOTO_EQUAL(len, strlen(filebuf), len, EXIT2);

            off = lseek(fd, 0, SEEK_SET);
            ICUNIT_GOTO_EQUAL(off, 0, off, EXIT2);

            len = read(fd, readbuf, JFFS_STANDARD_NAME_LENGTH);
            ICUNIT_GOTO_EQUAL(len, strlen(filebuf), len, EXIT2);
            ICUNIT_GOTO_STRING_EQUAL(readbuf, "0000000000111111111122222222223333333333", readbuf, EXIT2);

            ret = close(fd);
            ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

            ret = unlink(pathname4[j]);
            ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);
        }

        for (j = JFFS_SHORT_ARRAY_LENGTH - 1; j >= 0; j--) {
            ret = rmdir(pathname3[j]);
            ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
        }
    }

    ret = stat(buffile, &buf2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    JffsStatPrintf(buf2);

    ret = chdir(JFFS_MAIN_DIR0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = rmdir(pathname5);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT2:
    close(fd);
    for (i = 0; i < JFFS_SHORT_ARRAY_LENGTH; i++) {
        unlink(pathname4[i]);
    }
EXIT1:
    for (i = JFFS_SHORT_ARRAY_LENGTH - 1; i >= 0; i--) {
        rmdir(pathname3[i]);
    }
EXIT:
    rmdir(pathname5);
    return JFFS_NO_ERROR;
}

VOID ItFsJffsPressure007(VOID)
{
    TEST_ADD_CASE("ItFsJffsPressure007", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
