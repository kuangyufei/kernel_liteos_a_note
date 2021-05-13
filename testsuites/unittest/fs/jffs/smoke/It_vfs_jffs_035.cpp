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

static UINT32 testcase(VOID)
{
    INT32 ret, fd, len;
    CHAR pathname1[50] = { JFFS_PATH_NAME0 };
    CHAR readbuf[50] = {0};

    fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT1);

    len = write(fd, "01234567890123456789012345", 16); // write length: 16
    ICUNIT_GOTO_EQUAL(len, 16, len, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = umount("/jffs2_test");
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    ret = access(pathname1, F_OK);
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT1);

    ret = mount(JFFS_DEV_PATH0, "/jffs2_test", JFFS_FILESYS_TYPE, 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT1);

    len = write(fd, "01234567890123456789012345", 16); // write length: 16
    ICUNIT_GOTO_EQUAL(len, 16, len, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT1);

    len = read(fd, readbuf, 20); // read length: 20
    ICUNIT_GOTO_EQUAL(len, 16, len, EXIT1); // file length: 16

    ret = access(pathname1, F_OK);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    ret = umount("/jffs2_test/test/test123");
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT2);

    ret = umount(pathname1);
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT2);

    ret = access(pathname1, F_OK);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    ret = mount(NULL, "/jffs2_test", "storage", 0, NULL);
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT2);

    ret = access("/jffs2_test", F_OK);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    ret = access(pathname1, F_OK);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = unlink(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;

EXIT2:
    mount(JFFS_DEV_PATH0, "/jffs2_test", JFFS_FILESYS_TYPE, 0, NULL);
EXIT1:
    close(fd);
EXIT:
    remove(JFFS_PATH_NAME0);
    return JFFS_NO_ERROR;
}

VOID ItFsJffs035(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_035", testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}
