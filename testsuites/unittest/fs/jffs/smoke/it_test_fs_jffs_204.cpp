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

#define DIRPATH "/storage/test"
#define FILEPATH "/storage/test/1.txt"

static int TestCase(void)
{
    INT32 dirFd = -1;
    INT32 fd = -1;
    INT32 flags = -1;
    INT32 ret = -1;
    DIR *dir = nullptr;
    ret = mkdir(DIRPATH, 0777);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    dir = opendir(DIRPATH);
    ICUNIT_GOTO_NOT_EQUAL(dir, NULL, dir, EXIT1);
    fd = open("FILEPATH", O_CREAT | O_RDWR, 0777);
    dirFd = dirfd(dir);
    ICUNIT_GOTO_NOT_EQUAL(dirFd, JFFS_IS_ERROR, dirFd, EXIT1);

    ret = unlinkat(dirFd, DIRPATH, 0);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT1);

    ret = unlinkat(dirFd, FILEPATH, AT_REMOVEDIR);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT1);

EXIT1:
    close(fd);
    unlink(DIRPATH);
    closedir(dir);
EXIT:
    rmdir(JFFS_PATH_NAME01);
    return JFFS_NO_ERROR;
}

void ItTestFsJffs204(void)
{
    TEST_ADD_CASE("It_Test_Fs_Jffs_204", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}
