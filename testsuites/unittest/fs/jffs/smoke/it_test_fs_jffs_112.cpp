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
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = JFFS_MAIN_DIR0;
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = JFFS_MAIN_DIR0;
    CHAR *dname = NULL;
    INT32 ret;
    INT32 fd = 0;
    DIR *dir1 = NULL;
    DIR *dir2 = NULL;

    dir1 = opendir(pathname1);
    ICUNIT_GOTO_NOT_EQUAL(dir1, NULL, dir1, EXIT);

    ret = closedir(dir1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    strcat_s(pathname2, JFFS_STANDARD_NAME_LENGTH, "/test123");
    ret = mkdir(pathname2, 0777);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT1);

    fd = open(pathname2, O_DIRECTORY);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT1);

    dir2 = fdopendir(fd);
    ICUNIT_GOTO_NOT_EQUAL(dir2, NULL, dir2, EXIT2);

    ret = closedir(dir2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    ret = rmdir(pathname2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT2:
    if (fd) {
        close(fd);
    }
    if (dir2) {
        closedir(dir2);
    }
EXIT1:
    remove(pathname2);
    return JFFS_IS_ERROR;
EXIT:
    if (dir1) {
        closedir(dir1);
    }
    return JFFS_IS_ERROR;
}

void ItTestFsJffs112(void)
{
    TEST_ADD_CASE("It_Test_Fs_Jffs_112", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}