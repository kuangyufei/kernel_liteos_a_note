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
    INT32 fd1, ret, len;
    INT32 flags;
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "liteos";
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname3[JFFS_STANDARD_NAME_LENGTH] = { JFFS_MAIN_DIR0 };
    struct stat buf1, buf2;
    struct utimbuf utime1;
    time_t ttime1;
    struct tm ttm1;

    ttm1.tm_year = 90; // random test year 90
    ttm1.tm_mon = 0; // random test mon 0
    ttm1.tm_mday = 1; // random test mday 1
    ttm1.tm_hour = 12; // random test hour 12
    ttm1.tm_min = 12; // random test min 12
    ttm1.tm_sec = 12; // random test sec 12
    ttm1.tm_isdst = 0;
    ttime1 = mktime(&ttm1);
    utime1.modtime = ttime1;

    ret = mkdir(pathname, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    strcat(pathname1, "/1463");
    fd1 = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    ICUNIT_GOTO_NOT_EQUAL(fd1, -1, fd1, EXIT2);

    ret = stat(pathname1, &buf1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);
    JffsStatPrintf(buf1);

    ret = utime(pathname1, &utime1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_UTIME_SUPPORT, ret, EXIT2);
    ICUNIT_GOTO_EQUAL(errno, ENOSYS, errno, EXIT);

    ret = stat(pathname1, &buf1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);
    JffsStatPrintf(buf1);

    ret = close(fd1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    ret = remove(pathname1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = chdir(JFFS_MAIN_DIR0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = rmdir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT2:
    close(fd1);
EXIT1:
    remove(pathname1);
EXIT:
    remove(pathname2);
    return JFFS_NO_ERROR;
}

/*
 *
testcase brief in English
 *
 */

VOID ItFsJffs413(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_413", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}
