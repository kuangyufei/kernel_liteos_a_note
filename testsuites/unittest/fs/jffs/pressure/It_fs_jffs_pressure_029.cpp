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

static UINT32 TestCase(VOID)
{
    INT32 fd = -1;
    INT32 ret, scandirCount1, scandirCount2;
    INT64 i, j;
    INT64 writeCount = 0;
    INT64 readCount = 0;
    INT64 perTime;
    INT64 totalSize = 0;
    INT64 totalTime = 0;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_MAIN_DIR0 };
    CHAR *bufW = NULL;
    CHAR *bufW1 = NULL;
    CHAR *bufW2 = NULL;
    CHAR filebuf[FILE_BUF_SIZE] =
        "abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    struct timeval testTime1;
    struct timeval testTime2;
    struct dirent **namelist = NULL;
    DOUBLE mountSpeed;
    struct statfs buf1 = { 0 };
    struct statfs buf2 = { 0 };
    struct statfs buf3 = { 0 };
    off_t off;

    INT32 bufWLen = BYTES_PER_MBYTE;
    INT32 bufW1Len = 512 * BYTES_PER_KBYTE; // 512 KB
    INT32 bufW2Len = 16 * BYTES_PER_KBYTE;  // 16 KB

    bufW = (CHAR *)malloc(bufWLen + 1);
    ICUNIT_ASSERT_NOT_EQUAL(bufW, NULL, 0);
    (void)memset_s(bufW, bufWLen + 1, 0, bufWLen + 1);

    bufW1 = (CHAR *)malloc(bufW1Len + 1);
    ICUNIT_GOTO_NOT_EQUAL(bufW1, NULL, 0, EXIT2);
    (void)memset_s(bufW1, bufW1Len + 1, 0, bufW1Len + 1);

    bufW2 = (CHAR *)malloc(bufW2Len + 1);
    ICUNIT_GOTO_NOT_EQUAL(bufW2, NULL, 0, EXIT3);
    (void)memset_s(bufW2, bufW2Len + 1, 0, bufW2Len + 1);

    for (j = 0; j < bufW2Len / strlen(filebuf); j++) {
        strcat_s(bufW2, bufW2Len + 1, filebuf);
    }

    for (j = 0; j < bufW1Len / bufW2Len; j++) {
        strcat_s(bufW1, bufW1Len + 1, bufW2);
    }

    for (i = 0; i < bufWLen / bufW1Len; i++) {
        strcat_s(bufW, bufWLen + 1, bufW1);
    }
    free(bufW1);
    free(bufW2);

    fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT3);

    while (writeCount < 100) { // loop times: 100
        ret = write(fd, bufW, strlen(bufW));
        dprintf("write_count=:%lld\n", writeCount);
        ICUNIT_GOTO_EQUAL(ret, BYTES_PER_MBYTE, ret, EXIT3);

        off = lseek(fd, 0, SEEK_CUR);
        ICUNIT_GOTO_EQUAL(off, (BYTES_PER_MBYTE * (writeCount + 1)), off, EXIT1);

        writeCount++;
    }

    free(bufW);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    ret = statfs(JFFS_MAIN_DIR0, &buf3);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    JffsStatfsPrintf(buf3);

    totalTime = 0;
    totalSize = 0;

    scandirCount1 = scandir(pathname2, &namelist, 0, alphasort);
    if (scandirCount1 >= 0) {
        JffsScandirFree(namelist, scandirCount1);
    }

    ret = chdir("/");
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    for (i = 0; i < JFFS_PRESSURE_CYCLES; i++) {
        gettimeofday(&testTime1, 0);

        ret = umount(pathname2);
        if (ret != 0) {
            dprintf("umount jffs is error\n");
            goto EXIT2;
        }

        ret = mount(JFFS_DEV_PATH0, JFFS_MAIN_DIR0, JFFS_FILESYS_TYPE, 0, NULL);
        if (ret != 0) {
            dprintf("mount jffs is error\n");
            goto EXIT2;
        }

        gettimeofday(&testTime2, 0);

        perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

        totalTime += perTime;

        dprintf("the size:%lld th,umount/mount cost %lld us \n", i, perTime);
    }

    totalSize = i;

    mountSpeed = totalSize * 1.0;
    mountSpeed = totalTime / mountSpeed;

    dprintf("total_size %lld,cost %lld us, speed: %0.3lf us/Size\n", totalSize, totalTime, mountSpeed);

    ret = chdir(JFFS_MAIN_DIR0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    scandirCount2 = scandir(pathname2, &namelist, 0, alphasort);
    if (scandirCount2 >= 0) {
        JffsScandirFree(namelist, scandirCount2);
    }

    ICUNIT_GOTO_EQUAL(scandirCount1, scandirCount2, scandirCount2, EXIT);

    fd = open(pathname1, O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT1);

    off = lseek(fd, 0, SEEK_END);
    ICUNIT_GOTO_EQUAL(off, 100 * 1024 * 1024, off, EXIT1); // 100 * 1024 * 1024: filesize

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    ret = statfs(JFFS_MAIN_DIR0, &buf2);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    JffsStatfsPrintf(buf2);

    ret = unlink(pathname1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = statfs(JFFS_MAIN_DIR0, &buf3);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    JffsStatfsPrintf(buf3);

    return JFFS_NO_ERROR;
EXIT4:
    free(bufW1);
EXIT3:
    free(bufW);
    goto EXIT1;
EXIT2:
    mount(JFFS_DEV_PATH0, JFFS_MAIN_DIR0, JFFS_FILESYS_TYPE, 0, NULL);
    goto EXIT;
EXIT1:
    close(fd);
EXIT:
    remove(pathname1);
    return JFFS_NO_ERROR;
}

VOID ItFsJffsPressure029(VOID)
{
    TEST_ADD_CASE("ItFsJffsPressure029", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
