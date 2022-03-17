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
    INT32 fd[JFFS_SHORT_ARRAY_LENGTH];
    INT32 ret, len;
    INT32 i = 0;
    INT32 j = 0;
    static INT32 count = 0;
    signed long long offset;

    CHAR filebuf[JFFS_STANDARD_NAME_LENGTH] = "0000000000111111111122222222223333333    333";
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "liteos";
    CHAR bufname[JFFS_STANDARD_NAME_LENGTH] = "123";
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname3[JFFS_SHORT_ARRAY_LENGTH][JFFS_STANDARD_NAME_LENGTH] = {0};
    CHAR pathname4[JFFS_SHORT_ARRAY_LENGTH][JFFS_STANDARD_NAME_LENGTH] = {0};
    CHAR buffile[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    struct stat buf1 = { 0 };
    struct stat buf2 = { 0 };
    INT32 even = 2;

    struct stat statfile;

    ret = mkdir(pathname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = chdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = stat(buffile, &buf1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    JffsStatPrintf(buf1);

    for (i = 0; i < JFFS_SHORT_ARRAY_LENGTH; i++) {
        (void)memset_s(pathname2, JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
        (void)memset_s(bufname, JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
        (void)memset_s(pathname3[i], JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
        (void)memset_s(pathname4[i], JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);

        (void)strcpy_s(pathname2, JFFS_STANDARD_NAME_LENGTH, pathname1);
        snprintf_s(bufname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/1623_%d.txt", i);
        strcat_s(pathname2, JFFS_STANDARD_NAME_LENGTH, bufname);
        strcat_s(pathname3[i], JFFS_STANDARD_NAME_LENGTH, pathname2);

        (void)memset_s(pathname2, JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
        (void)memset_s(bufname, JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
        (void)strcpy_s(pathname2, JFFS_STANDARD_NAME_LENGTH, pathname1);
        snprintf_s(bufname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/test_%d.c", i);
        strcat_s(pathname2, JFFS_STANDARD_NAME_LENGTH, bufname);
        strcat_s(pathname4[i], JFFS_STANDARD_NAME_LENGTH, pathname2);

        fd[i] = open(pathname3[i], O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_NOT_EQUAL(fd[i], -1, fd[i], EXIT2);

        ret = close(fd[i]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);
    }

    for (j = 0; j < JFFS_PRESSURE_CYCLES; j++) {
        for (i = 0; i < JFFS_SHORT_ARRAY_LENGTH; i++) {
            if ((j % even) == 0) {
                ret = rename(pathname3[i], pathname4[i]);
                ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
            } else {
                ret = rename(pathname4[i], pathname3[i]);
                ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
            }
        }
    }

    if ((j % even) == 0) {
        for (i = 0; i < JFFS_SHORT_ARRAY_LENGTH; i++) {
            ret = unlink(pathname3[i]);
            ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
        }
    } else {
        for (i = 0; i < JFFS_SHORT_ARRAY_LENGTH; i++) {
            ret = unlink(pathname4[i]);
            ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
        }
    }

    ret = chdir(JFFS_MAIN_DIR0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = rmdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT2:
    for (i = 0; i < JFFS_SHORT_ARRAY_LENGTH; i++) {
        close(fd[i]);
    }
EXIT1:
    for (i = 0; i < JFFS_SHORT_ARRAY_LENGTH; i++) {
        remove(pathname3[i]);
        remove(pathname4[i]);
    }
EXIT:
    chdir(JFFS_MAIN_DIR0);
    rmdir(pathname1);
    return JFFS_NO_ERROR;
}

VOID ItFsJffsPressure025(VOID)
{
    TEST_ADD_CASE("ItFsJffsPressure025", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
