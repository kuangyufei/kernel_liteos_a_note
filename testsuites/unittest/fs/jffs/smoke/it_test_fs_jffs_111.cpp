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
#include "It_fs_jffs.h"

static int TestCase(void)
{
    INT32 ret = JFFS_IS_ERROR;
    INT32 fd = 0;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = JFFS_PATH_NAME01;
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = JFFS_PATH_NAME01;
    time_t timep;
    time_t timep1;
    struct tm p;
    struct stat fstate;
    struct timespec times[2];
    struct tm t;

    p.tm_sec = 30;
    p.tm_min = 27;
    p.tm_hour = 16;
    p.tm_mday = 18;
    p.tm_mon = 0;
    p.tm_year = 2020 - 1900;
    p.tm_wday = 1;
    p.tm_yday = 17;
    p.tm_isdst = 0;

    timep = mktime(&p);

    p.tm_min = 28;
    p.tm_hour = 17;
    timep1 = mktime(&p);

    times[0].tv_sec = timep;
    times[0].tv_nsec = 0;
    times[1].tv_sec = timep1;
    times[1].tv_nsec = 0;

    ret = mkdir(pathname1, 0777);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    strcat_s(pathname2, JFFS_STANDARD_NAME_LENGTH, "/test.txt");
    fd = open(pathname2, O_CREAT | O_RDWR | O_TRUNC);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT1);

    futimens(fd, times);
    ret = stat(pathname2, &fstate);
    ICUNIT_GOTO_EQUAL(fstate.st_atim.tv_sec, timep, fstate.st_atim.tv_sec, EXIT1);
    ICUNIT_GOTO_EQUAL(fstate.st_mtim.tv_sec, timep1, fstate.st_mtim.tv_sec, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT1);

    ret = unlink(pathname2);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT1);

    ret = rmdir(JFFS_PATH_NAME01);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;

EXIT1:
    if (fd > 0) {
        close(fd);
    }
    unlink(pathname2);
EXIT:
    rmdir(JFFS_PATH_NAME01);
    return JFFS_IS_ERROR;
}

void ItTestFsJffs111(void)
{
    TEST_ADD_CASE("It_Test_Fs_Jffs_111", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}
