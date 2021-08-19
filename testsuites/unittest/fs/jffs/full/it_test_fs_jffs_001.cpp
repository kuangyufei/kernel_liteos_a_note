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

#define FILEPATH  "/storage/testfdopen.txt"
#define TESTFILE  "/storage/testfopen"
#define MAXFD  512
#define MINFD 8

static int TestCase0(void)
{
    FILE *fp = NULL;
    int fd = -1;
    char buf[10U] = "123456789";
    char rdbuf[10U] = {0};
    int ret = -1;

    fd = open(FILEPATH, O_RDWR);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);

    /* r+ */
    fp = fdopen(fd, "r+");
    ICUNIT_GOTO_NOT_EQUAL(fp, JFFS_TO_NULL, fp, EXIT);

    ret = fwrite(buf, 10U, 1, fp);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);

    ret = fseek(fp, 0, SEEK_SET);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);

    ret = fread(rdbuf, 10U, 1, fp);
    ICUNIT_GOTO_EQUAL(ret, 1, ret, EXIT);

    ret = fclose(fp);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);
    fp = NULL;

    /* a+ /appen + rw */
    fd = open(FILEPATH, O_RDWR);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);

    fp = fdopen(fd, "a+");
    ICUNIT_GOTO_NOT_EQUAL(fp, JFFS_TO_NULL, fp, EXIT);

    ret = fwrite(buf, 10U, 1, fp);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);

    ret = fseek(fp, 0, SEEK_SET);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);

    ret = fread(rdbuf, 10U, 1, fp);
    ICUNIT_GOTO_EQUAL(ret, 1, ret, EXIT);

    ret = fclose(fp);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);
    fp = NULL;

    /* r / only read */
    fd = open(FILEPATH, O_RDWR);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);

    fp = fdopen(fd, "r");
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_TO_NULL, fd, EXIT);

    ret = fwrite(buf, 10u, 1, fp);
    ICUNIT_GOTO_NOT_EQUAL(ret, 1, ret, EXIT);

    ret = fclose(fp);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);
    fp = NULL;

    /* w / only write */
    fd = open(FILEPATH, O_RDWR);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);

    fp = fdopen(fd, "w");
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_TO_NULL, fd, EXIT);

    ret = fread(rdbuf, 10u, 1, fp);
    ICUNIT_GOTO_NOT_EQUAL(ret, 1, ret, EXIT);

    ret = fclose(fp);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);
    fp = NULL;

    /* w+ */
    fd = open(FILEPATH, O_RDWR);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);

    fp = fdopen(fd, "w+");
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_TO_NULL, fd, EXIT);

    ret = fwrite(buf, 10u, 1, fp);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);

    ret = fseek(fp, 0, SEEK_SET);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);

    ret = fread(rdbuf, 10u, 1, fp);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);

    ret = fclose(fp);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);
    fp = NULL;

    /* a */
    fd = open(FILEPATH, O_WRONLY);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);

    fp = fdopen(fd, "a");
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_TO_NULL, fd, EXIT);

    ret = fwrite(buf, 10u, 1, fp);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);

    ret = fseek(fp, 0, SEEK_SET);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);

    ret = fread(rdbuf, 10u, 1, fp);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = fclose(fp);

    return JFFS_NO_ERROR;

EXIT:
    if(fp != NULL) {
        fclose(fp);
    } else {
        remove(FILEPATH);
    }

    return JFFS_IS_ERROR;
}

/* error */
static int TestCase1(void)
{
    FILE *fp = NULL;
    int fd = -1;
    int errFd = -1;
    int i = 0;

    fd = open(FILEPATH, O_RDWR, JFFS_FILE_MODE);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);

    /* EIVNAL */
    fp = fdopen(fd, "hello");
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_TO_NULL, fd, EXIT);
    ICUNIT_GOTO_EQUAL(errno, EINVAL, errno, EXIT);

    close(fd);
    remove(TESTFILE);

    return JFFS_NO_ERROR;

EXIT:
    close(fd);
    remove(TESTFILE);

    return JFFS_IS_ERROR;
}

static int TestCase(void)
{
    int ret = -1;
    int fd = -1;

    fd = open(FILEPATH, O_RDWR | O_EXCL | O_CREAT, JFFS_FILE_MODE);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);

    ret = TestCase0();
    ICUNIT_GOTO_EQUAL(ret, JFFS_TO_NULL, ret, EXIT);

    ret = TestCase1();
    ICUNIT_GOTO_EQUAL(ret, JFFS_TO_NULL, ret, EXIT);

    close(fd);
    remove(FILEPATH);

    return JFFS_NO_ERROR;

EXIT:
    close(fd);
    remove(FILEPATH);

    return JFFS_IS_ERROR;
}

void ItTestFsJffs001(void)
{
    TEST_ADD_CASE("It_Test_Fs_Jffs_001", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}

