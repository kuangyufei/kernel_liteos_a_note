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

 * PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "It_vfs_fat.h"

static UINT32 TestCase(VOID)
{
    INT32 ret;
    INT32 fd1 = 0;
    INT32 i = 0;
    INT32 len;
    CHAR filebuf[FAT_STANDARD_NAME_LENGTH] = "abcdeabcde";
    CHAR readbuf[FAT_STANDARD_NAME_LENGTH] = "liteos";
    CHAR pathname1[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;

    ret = mkdir(pathname1, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    (void)strcat_s(pathname1, FAT_STANDARD_NAME_LENGTH, "/test");
    fd1 = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd1, FAT_IS_ERROR, fd1, EXIT1);

    len = write(fd1, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, FAT_SHORT_ARRAY_LENGTH, len, EXIT1);

    ret = close(fd1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    for (i = 0; i < FAT_MOUNT_CYCLES_TEST; i++) {
        ret = umount(FAT_MOUNT_DIR);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, i, EXIT2);

        ret = mount(FAT_DEV_PATH, FAT_MOUNT_DIR, FAT_FILESYS_TYPE, 0, NULL);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, i, EXIT2);
    }

    fd1 = open(pathname1, O_NONBLOCK | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd1, FAT_IS_ERROR, fd1, EXIT1);

    len = read(fd1, readbuf, FAT_STANDARD_NAME_LENGTH);
    ICUNIT_GOTO_EQUAL(len, FAT_SHORT_ARRAY_LENGTH, len, EXIT1);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, filebuf, readbuf, EXIT1);

    ret = close(fd1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = remove(pathname1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = remove(FAT_PATH_NAME);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    return FAT_NO_ERROR;
EXIT2:
    mount(FAT_DEV_PATH, FAT_MOUNT_DIR, FAT_FILESYS_TYPE, 0, NULL);
EXIT1:
    close(fd1);
    remove(pathname1);
EXIT:
    remove(FAT_PATH_NAME);
    return FAT_NO_ERROR;
}

/* *
* -@test IT_FS_FAT_074
* -@tspec The function test for filesystem
* -@ttitle Mount/umount the file system several times
* -@tprecon The filesystem module is open
* -@tbrief
1. create one file and write some data into it;
2. mount/umount the file system several times;
3. open the file and check the file`s content;
4. close and delete the file.
* -@texpect
1. Return successed
2. Return successed
3. Return successed
4. Successful operation
* -@tprior 1
* -@tauto TRUE
* -@tremark
*/

VOID ItFsFat074(VOID)
{
    TEST_ADD_CASE("IT_FS_FAT_074", TestCase, TEST_VFS, TEST_VFAT, TEST_LEVEL2, TEST_FUNCTION);
}
