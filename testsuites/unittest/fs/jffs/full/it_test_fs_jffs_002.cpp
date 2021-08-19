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
#include <stdio.h>
#include <stdlib.h>
#include <sys/statvfs.h>

#define FILEPATH  "/storage/testfstatvfs.txt"
#define FILEPATHLEN   (strlen(FILEPATH) + 1U)

static int TestCase0(void)
{
    INT32 fd = -1;
    INT32 ret = 0;
    struct statvfs fileInfo = {0};

    fd=open(FILEPATH, O_RDWR, JFFS_FILE_MODE);
    ICUNIT_GOTO_NOT_EQUAL(fd, 1, fd, EXIT);

    ret= fstatvfs(fd, &fileInfo);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    /*ICUNIT_GOTO_EQUAL(fileInfo.f_flag, 0, fileInfo.f_flag, EXIT);*/
    close(fd);
    return JFFS_NO_ERROR;

EXIT:
    close(fd);
    return JFFS_IS_ERROR;
}

static int TestCase1(void)
{
    INT32 fd = -1;
    INT32 ret= 0;
    INT32 errFd = -1;
    struct statvfs fileInfo = {0};

    fd = open(FILEPATH, O_RDWR, JFFS_FILE_MODE);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);

    /* EBADF */
    ret=fstatvfs(errFd, &fileInfo);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);
    ICUNIT_GOTO_EQUAL(errno, EBADF, errno, EXIT);

    /* EBADF(When the file does not exist, an error occurs when the file is
    converted from the fd file to the system FD file.) */
    close(fd);
    ret= remove(FILEPATH);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    errno =0;
    ret=fstatvfs(fd, &fileInfo);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);
    ICUNIT_GOTO_EQUAL(errno, EBADF, errno, EXIT);
    close(fd);

    return JFFS_NO_ERROR;
EXIT:
    close(fd);
    return JFFS_IS_ERROR;
}

static int TestCase(void)
{
    INT32 fd=-1;
    int ret =-1;

    fd=open(FILEPATH, O_CREAT | O_RDWR | O_EXCL, JFFS_FILE_MODE);
    ICUNIT_GOTO_NOT_EQUAL(fd, 1, fd, EXIT);

    close(fd);
    ret= TestCase0();
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret= TestCase1();
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    remove(FILEPATH);
    return JFFS_NO_ERROR;
EXIT:
    remove(FILEPATH);
    return JFFS_IS_ERROR;
}

void ItTestFsJffs002(void)
{
    TEST_ADD_CASE("It_Test_Fs_Jffs_002", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}

