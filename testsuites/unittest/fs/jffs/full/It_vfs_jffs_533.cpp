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
    INT32 fd, ret, len;
    struct flock fl = { 0 };
    CHAR filebuf[10] = "good";
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };

    fd = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT1);

    ret = fcntl(fd, F_GETLK, &fl);
    ICUNIT_GOTO_EQUAL(ret, -1, ret, EXIT1);
    len = write(fd, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, 4, len, EXIT1); // compare ret len with target len 4

    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 10; // star from 10
    fl.l_len = 10; // length 10
    ret = fcntl(fd, F_SETLK, &fl);
    ICUNIT_GOTO_EQUAL(ret, -1, ret, EXIT1);

    ret = fcntl(fd, F_GETLK, &fl);
    ICUNIT_GOTO_EQUAL(ret, -1, ret, EXIT1);

    len = write(fd, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, 4, len, EXIT1); // compare ret len with target len 4

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    ret = unlink(pathname);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT1:
    close(fd);
EXIT:
    unlink(pathname);
    return JFFS_NO_ERROR;
}

/*
 *
testcase brief in English
 *
 */

VOID ItFsJffs533(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_533", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}
