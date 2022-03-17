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
#include <fcntl.h>
#include "sys/stat.h"
#include "string.h"

static UINT32 testcase8(VOID)
{
    struct stat buf;
    char pathname[200] = FILEPATH_775;
    int ret = 0;
    int fd = 0;

    /* omit to create test file dynamicly,use prepared test files in /storage instand. */
    errno = 0;
    fd = open(pathname, O_CREAT, 0777);
    TEST_PRINT("[INFO]%s:%d,%s,fd=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, fd, errno, strerror(errno));
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, OUT);

    errno = 0;
    ret = fstatat(fd, FILEPATH_RELATIVE, &buf, 0);
    TEST_PRINT("[INFO]%s:%d,%s,ret=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, ret, errno, strerror(errno));
    ICUNIT_GOTO_EQUAL(ret, -1, ret, OUT);
    ICUNIT_GOTO_EQUAL(errno, ENOTDIR, errno, OUT);

    return LOS_OK;
OUT:
    return LOS_NOK;
}

static UINT32 testcase7(VOID)
{
    struct stat buf;
    char pathname[] = FILEPATH_NOACCESS;
    char dirname[] = DIRPATH_775;
    DIR *dir = NULL;
    int ret = 0;
    int fd = 0;

    errno = 0;
    dir = opendir(dirname);
    fd = dirfd(dir);
    TEST_PRINT("[INFO]%s:%d,%s,fd=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, fd, errno, strerror(errno));
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, OUT);

    errno = 0;
    ret = fstatat(fd, pathname, &buf, 0);
    TEST_PRINT("[INFO]%s:%d,%s,ret=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, ret, errno, strerror(errno));
    ICUNIT_GOTO_EQUAL(ret, -1, ret, OUT);
    ICUNIT_GOTO_EQUAL(errno, EACCES, errno, OUT);

    return LOS_OK;
OUT:
    return LOS_NOK;
}

static UINT32 testcase6(VOID)
{
    struct stat buf;
    /* let the pathname more than 4096 characters,to generate the ENAMETOOLONG errno. */
    char pathname[] = PATHNAME_ENAMETOOLONG;
    int ret = 0;
    char dirname[] = DIRPATH_775;
    DIR *dir = NULL;
    int fd = 0;

    errno = 0;
    dir = opendir(dirname);
    fd = dirfd(dir);
    TEST_PRINT("[INFO]%s:%d,%s,fd=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, fd, errno, strerror(errno));
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, OUT);

    errno = 0;
    ret = fstatat(fd, pathname, &buf, 0);
    TEST_PRINT("[INFO]%s:%d,%s,ret=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, ret, errno, strerror(errno));
    ICUNIT_GOTO_EQUAL(ret, -1, ret, OUT);
    ICUNIT_GOTO_EQUAL(errno, ENAMETOOLONG, errno, OUT);

    return LOS_OK;
OUT:
    return LOS_NOK;
}

static UINT32 testcase5(VOID)
{
    struct stat buf;
    char pathname[] = FILEPATH_ENOENT;
    int ret = 0;

    errno = 0;
    ret = fstatat(FD_EBADF, pathname, &buf, 0);
    TEST_PRINT("[INFO]%s:%d,%s,ret=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, ret, errno, strerror(errno));
    ICUNIT_GOTO_EQUAL(ret, -1, ret, OUT);
    ICUNIT_GOTO_EQUAL(errno, ENOENT, errno, OUT);

    return LOS_OK;
OUT:
    return LOS_NOK;
}

static UINT32 testcase3(VOID)
{
    struct stat buf;
    char pathname[] = "";
    int ret = 0;

    errno = 0;
    ret = fstatat(AT_FDCWD, pathname, &buf, 0);
    TEST_PRINT("[INFO]%s:%d,%s,ret=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, ret, errno, strerror(errno));
    ICUNIT_GOTO_EQUAL(ret, -1, ret, OUT);
    ICUNIT_GOTO_EQUAL(errno, ENOENT, errno, OUT);

    return LOS_OK;
OUT:
    return LOS_NOK;
}

static UINT32 testcase2(VOID)
{
    struct stat buf;
    char pathname[] = FILEPATH_ENOENT;
    int ret = 0;

    errno = 0;
    ret = fstatat(AT_FDCWD, pathname, &buf, 0);
    TEST_PRINT("[INFO]%s:%d,%s,ret=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, ret, errno, strerror(errno));
    ICUNIT_GOTO_EQUAL(ret, -1, ret, OUT);
    ICUNIT_GOTO_EQUAL(errno, ENOENT, errno, OUT);

    return LOS_OK;
OUT:
    return LOS_NOK;
}

static UINT32 testcase1(VOID)
{
    struct stat buf;
    char pathname[] = FILEPATH_000;
    int ret = 0;

    errno = 0;
    ret = fstatat(1, pathname, &buf, 0);
    TEST_PRINT("[INFO]%s:%d,%s,ret=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, ret, errno, strerror(errno));
    ICUNIT_GOTO_EQUAL(ret, -1, ret, OUT);
    ICUNIT_GOTO_EQUAL(errno, EACCES, errno, OUT);

    return LOS_OK;
OUT:
    return LOS_NOK;
}

static UINT32 testcase(VOID)
{
    testcase8(); /* CASE:fd is no a dirfd */
    /* testcase7(); omitted as program can not create file with no access privilege. */
    testcase6();
    testcase5();
    testcase3();
    testcase2();
    /* testcase1(); CASE:no access */

    return LOS_OK;
}

VOID IO_TEST_FSTATAT_002(void)
{
    TEST_ADD_CASE(__FUNCTION__, testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
