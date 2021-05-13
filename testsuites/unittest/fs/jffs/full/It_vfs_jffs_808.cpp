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
    INT32 fd, ret;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    DIR *dir = NULL;

    ret = chdir("/");
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    fd = open(pathname1, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);

    ret = umount(JFFS_MOUNT_DIR0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT2);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = remove(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = umount(JFFS_MOUNT_DIR0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = mount(JFFS_DEV_PATH0, JFFS_MOUNT_DIR0, JFFS_FILESYS_TYPE, 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = mkdir(pathname1, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    dir = opendir(pathname1);
    ICUNIT_GOTO_NOT_EQUAL(dir, NULL, dir, EXIT1);

    ret = umount(JFFS_MOUNT_DIR0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT3);

    ret = closedir(dir);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = remove(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = umount(JFFS_MOUNT_DIR0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = mount(JFFS_DEV_PATH0, JFFS_MOUNT_DIR0, JFFS_FILESYS_TYPE, 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;

EXIT3:
    mount(JFFS_DEV_PATH0, JFFS_MOUNT_DIR0, JFFS_FILESYS_TYPE, 0, NULL);
    closedir(dir);
    goto EXIT1;
EXIT2:
    mount(JFFS_DEV_PATH0, JFFS_MOUNT_DIR0, JFFS_FILESYS_TYPE, 0, NULL);
    close(fd);
EXIT1:
    remove(pathname1);
EXIT:
    return JFFS_NO_ERROR;
}

/*
 * @ingroup jffs
 * API test
 * @brief 测试有文件或目录打开时卸载jffs文件系统
 * @par description: Test that umount the jffs filesystem when any file or directory is open
 * @par precon: jffs moudle open
 * 1、open a file and umount jffs.\n
 * 2、close the file and umount jffs.\n
 * 3、open a directory and umount jffs.\n
 * 4、close the directory and umount jffs.\n
 * 1、return failed. \n
 * 2、return success. \n
 * 3、return failed. \n
 * 4、return success. \n
 */

VOID ItFsJffs808(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_808", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}
