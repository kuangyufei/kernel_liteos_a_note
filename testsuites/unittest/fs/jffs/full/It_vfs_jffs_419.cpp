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
    INT32 fd1, fd2, fd3;
    INT32 ret, len;
    INT32 flags;
    ssize_t lenV = 0;
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "liteos";
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname3[JFFS_STANDARD_NAME_LENGTH] = { JFFS_MAIN_DIR0 };
    CHAR bufW1[JFFS_SHORT_ARRAY_LENGTH + 1] = "0123456789";
    CHAR bufW2[JFFS_SHORT_ARRAY_LENGTH + 1] = "abcefghijk";
    CHAR bufW3[JFFS_STANDARD_NAME_LENGTH] = "lalalalala";

    ret = mkdir(pathname1, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = chdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    JffsStrcat2(pathname1, "/1475_1", JFFS_STANDARD_NAME_LENGTH);
    fd1 = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd1, -1, fd1, EXIT2);

    JffsStrcat2(pathname1, "/1475_2", JFFS_STANDARD_NAME_LENGTH);
    fd2 = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd2, -1, fd2, EXIT4);

    JffsStrcat2(pathname1, "/1475_3", JFFS_STANDARD_NAME_LENGTH);
    fd3 = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd2, -1, fd2, EXIT6);

    len = write(fd1, bufW1, JFFS_SHORT_ARRAY_LENGTH);
    ICUNIT_GOTO_EQUAL(len, 10, len, EXIT6); // compare ret len with target len 10

    len = write(fd2, bufW2, JFFS_SHORT_ARRAY_LENGTH);
    ICUNIT_GOTO_EQUAL(len, 10, len, EXIT6); // compare ret len with target len 10

    ret = close(fd1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT6);

    ret = close(fd2);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT6);

    JffsStrcat2(pathname1, "/1475_1", JFFS_STANDARD_NAME_LENGTH);
    fd1 = open(pathname1, O_NONBLOCK | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd1, -1, fd1, EXIT6);

    JffsStrcat2(pathname1, "/1475_2", JFFS_STANDARD_NAME_LENGTH);
    fd2 = open(pathname1, O_NONBLOCK | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd2, -1, fd2, EXIT6);

    memset(bufW1, 0, JFFS_SHORT_ARRAY_LENGTH + 1);
    memset(bufW2, 0, JFFS_SHORT_ARRAY_LENGTH + 1);

    len = read(fd1, bufW1, JFFS_SHORT_ARRAY_LENGTH);
    ICUNIT_GOTO_EQUAL(len, 10, len, EXIT6); // compare ret len with target len 10

    len = read(fd2, bufW2, JFFS_SHORT_ARRAY_LENGTH);
    ICUNIT_GOTO_EQUAL(len, 10, len, EXIT6); // compare ret len with target len 10

    g_jffsIov[0].iov_base = bufW1;
    g_jffsIov[0].iov_len = JFFS_SHORT_ARRAY_LENGTH - 1;
    g_jffsIov[1].iov_base = bufW2;
    g_jffsIov[1].iov_len = JFFS_SHORT_ARRAY_LENGTH - 1;

    lenV = writev(fd3, g_jffsIov, 2); // writes 2 buffers to the fd
    ICUNIT_GOTO_EQUAL(lenV, 2 * (JFFS_SHORT_ARRAY_LENGTH - 1), lenV, EXIT6);

    ret = close(fd3);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT6);

    JffsStrcat2(pathname1, "/1475_3", JFFS_STANDARD_NAME_LENGTH);
    ret = remove(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT5);

    ret = close(fd2);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT6);

    JffsStrcat2(pathname1, "/1475_2", JFFS_STANDARD_NAME_LENGTH);
    ret = remove(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

    ret = close(fd1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    JffsStrcat2(pathname1, "/1475_1", JFFS_STANDARD_NAME_LENGTH);
    ret = remove(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = chdir(JFFS_MAIN_DIR0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = rmdir(pathname2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;

EXIT6:
    close(fd3);
EXIT5:
    JffsStrcat2(pathname1, "/1475_3", JFFS_STANDARD_NAME_LENGTH);
    remove(pathname1);
EXIT4:
    close(fd2);
EXIT3:
    JffsStrcat2(pathname1, "/1475_2", JFFS_STANDARD_NAME_LENGTH);
    remove(pathname1);
EXIT2:
    close(fd1);
EXIT1:
    JffsStrcat2(pathname1, "/1475_1", JFFS_STANDARD_NAME_LENGTH);
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

VOID ItFsJffs419(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_419", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}
