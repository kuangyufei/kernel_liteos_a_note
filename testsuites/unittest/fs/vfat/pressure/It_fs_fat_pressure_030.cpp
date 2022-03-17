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


#include "It_vfs_fat.h"

static UINT32 TestCase(VOID)
{
    INT32 ret;
    INT32 len;
    INT32 i = 0;
    INT32 j = 0;
    INT32 fd = 0;
    CHAR pathname[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
    CHAR filebuf[258] = "abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR *bufWrite = NULL;
    CHAR *bufWrite1 = NULL;
    CHAR *bufWrite2 = NULL;
    struct stat buf1[FAT_PRESSURE_CYCLES][FAT_SHORT_ARRAY_LENGTH] = { 0 };

    g_fatFlag = 0;

    bufWrite = (CHAR *)malloc(5 * BYTES_PER_MBYTES + 1); // 5 * BYTES_PER_MBYTES = 5MB
    ICUNIT_ASSERT_NOT_EQUAL(bufWrite, NULL, 0);
    (void)memset_s(bufWrite, BYTES_PER_MBYTES + 1, 0, 5 * BYTES_PER_MBYTES + 1); // 5 * BYTES_PER_MBYTES = 5MB

    bufWrite1 = (CHAR *)malloc(256 * BYTES_PER_KBYTES + 1); // 256 * BYTES_PER_KBYTES = 256KB
    ICUNIT_GOTO_NOT_EQUAL(bufWrite1, NULL, 0, EXIT);
    (void)memset_s(bufWrite1, 256 * BYTES_PER_KBYTES + 1, 0, // 256 kb
        256 * BYTES_PER_KBYTES + 1);                         // 256 * BYTES_PER_KBYTES = 256KB

    bufWrite2 = (CHAR *)malloc(16 * BYTES_PER_KBYTES + 1); // 16 * BYTES_PER_KBYTES = 16KB
    ICUNIT_GOTO_NOT_EQUAL(bufWrite2, NULL, 0, EXIT1);
    (void)memset_s(bufWrite2, 16 * BYTES_PER_KBYTES + 1, 0, 16 * BYTES_PER_KBYTES + 1); // 16 kb

    for (j = 0; j < 16; j++) {                                     // loop 16 times
        (void)strcat_s(bufWrite2, 16 * BYTES_PER_KBYTES + 1, filebuf); // 16kb
        (void)strcat_s(bufWrite2, 16 * BYTES_PER_KBYTES + 1, filebuf); // 16kb
        (void)strcat_s(bufWrite2, 16 * BYTES_PER_KBYTES + 1, filebuf); // 16kb
        (void)strcat_s(bufWrite2, 16 * BYTES_PER_KBYTES + 1, filebuf); // 16kb
    }

    for (i = 0; i < 4; i++) {                                         // loop 4 times
        (void)strcat_s(bufWrite1, 256 * BYTES_PER_KBYTES + 1, bufWrite2); // 256kb
        (void)strcat_s(bufWrite1, 256 * BYTES_PER_KBYTES + 1, bufWrite2); // 256kb
        (void)strcat_s(bufWrite1, 256 * BYTES_PER_KBYTES + 1, bufWrite2); // 256kb
        (void)strcat_s(bufWrite1, 256 * BYTES_PER_KBYTES + 1, bufWrite2); // 256kb
    }
    for (i = 0; i < 5; i++) { // loop 5 times
        (void)strcat_s(bufWrite, 5 * BYTES_PER_MBYTES + 1, bufWrite1);
        (void)strcat_s(bufWrite, 5 * BYTES_PER_MBYTES + 1, bufWrite1);
        (void)strcat_s(bufWrite, 5 * BYTES_PER_MBYTES + 1, bufWrite1);
        (void)strcat_s(bufWrite, 5 * BYTES_PER_MBYTES + 1, bufWrite1);
    }
    free(bufWrite1);
    free(bufWrite2);

    for (i = 0; i < FAT_PRESSURE_CYCLES; i++) {
        printf("Loop %d a\n", i + 1);
        fd = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
        ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT);

        ret = close(fd);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
        printf("Loop %d b\n", i + 1);

        for (j = 0; j < 819; j++) { // loop 819 times
            fd = open(pathname, O_NONBLOCK | O_RDWR | O_APPEND, S_IRWXU | S_IRWXG | S_IRWXO);
            ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT);

            len = write(fd, bufWrite, strlen(bufWrite));
            ICUNIT_GOTO_EQUAL(len, 5 * BYTES_PER_MBYTES, len, EXIT); // 5 * BYTES_PER_MBYTES = 5MB

            ret = close(fd);
            ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
        }
        printf("Loop %d c\n", i + 1);

        for (j = 0; j < 4; j++) { // loop 4 times
            fd = open(pathname, O_NONBLOCK | O_RDWR | O_APPEND, S_IRWXU | S_IRWXG | S_IRWXO);
            ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT);

            len = write(fd, filebuf, strlen(filebuf));
            ICUNIT_GOTO_EQUAL(len, 256, len, EXIT); // should write 256 bytes

            ret = close(fd);
            ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
        }
        printf("Loop %d d\n", i + 1);

        for (j = 0; j < 10; j++) { // loop 10 times
            fd = open(pathname, O_NONBLOCK | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
            ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT);

            ret = close(fd);
            ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

            ret = stat(pathname, &buf1[i][j]);
            ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
        }

        ret = remove(pathname);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

        g_fatFlag++;

        printf("\tg_fatFlag=:%d\t", g_fatFlag);
    }

    free(bufWrite);

    return FAT_NO_ERROR;
EXIT1:
    free(bufWrite1);
EXIT:
    free(bufWrite);
    close(fd);
    remove(pathname);
    return FAT_NO_ERROR;
}

/* *
* -@test IT_FS_FAT_PRESSURE_030
* -@tspec The Pressure test
* -@ttitle write the file to 4GB
* -@tprecon The filesystem module is open
* -@tbrief
1. create two tasks;
2. the first task write the file to 4GB;
3. the second task write one byte to the file once more;
4. delete the files;
* -@texpect
1. Return successed
2. Return successed
3. Return failure
4. Successful operation
* -@tprior 1
* -@tauto TRUE
* -@tremark
*/

VOID ItFsFatPressure030(VOID)
{
    TEST_ADD_CASE("IT_FS_FAT_PRESSURE_030", TestCase, TEST_VFS, TEST_VFAT, TEST_LEVEL3, TEST_PRESSURE);
}
