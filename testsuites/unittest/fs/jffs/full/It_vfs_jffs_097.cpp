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
    INT32 fd1 = -1;
    INT32 ret, len;
    INT32 flags;
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "liteos";
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname3[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    struct stat buf1, buf2;
    struct utimbuf utime1;
    time_t ttime1;
    struct tm ttm1;

    ttm1.tm_year = 90; // 90 means year
    ttm1.tm_mon = 0; // 0 means month
    ttm1.tm_mday = 1; // 1 means day
    ttm1.tm_hour = 12; // 12 means hour
    ttm1.tm_min = 12; // 12 means minute
    ttm1.tm_sec = 12; // 12 means second
    ttm1.tm_isdst = 0;
    ttime1 = mktime(&ttm1);
    utime1.modtime = ttime1;

    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    strcat_s(pathname1, sizeof(pathname1), "/097");
    fd1 = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, 0644); // 0644 means mode of file
    ICUNIT_GOTO_NOT_EQUAL(fd1, -1, fd1, EXIT2);

    ret = stat(pathname1, &buf1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);
    JffsStatPrintf(buf1);

    ret = utime(pathname1, NULL);
    ICUNIT_GOTO_EQUAL(ret, -1, ret, EXIT2);
    ICUNIT_GOTO_EQUAL(errno, ENOSYS, errno, EXIT2);

    ret = stat(pathname1, &buf1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);
    JffsStatPrintf(buf1);

    ret = close(fd1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    ret = utime(pathname1, NULL);
    ICUNIT_GOTO_EQUAL(ret, -1, ret, EXIT2);
    ICUNIT_GOTO_EQUAL(errno, ENOSYS, errno, EXIT2);

    ret = stat(pathname1, &buf1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);
    JffsStatPrintf(buf1);

    ret = remove(pathname1);
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

VOID ItFsJffs097(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_097", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

