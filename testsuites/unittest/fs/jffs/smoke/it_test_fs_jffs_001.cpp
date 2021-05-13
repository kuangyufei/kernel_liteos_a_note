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

#define FILEPATH  "/storage/test.txt"

static int TestCase(void)
{
    FILE *fp = NULL;
    int fd = -1;
    char buf[10U] = "123456789";
    char rdbuf[10U] = {0};
    int ret = -1;

    fd = open(FILEPATH, O_RDWR | O_EXCL | O_CREAT, JFFS_FILE_MODE);
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

    fd = open(FILEPATH, O_RDWR);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);

    /* a+ */
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

    ret = remove(FILEPATH);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;

EXIT:
    fclose(fp);
    remove(FILEPATH);

    return JFFS_IS_ERROR;
}

void ItTestFsJffs001(void)
{
    TEST_ADD_CASE("It_Test_Fs_Jffs_001", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}

