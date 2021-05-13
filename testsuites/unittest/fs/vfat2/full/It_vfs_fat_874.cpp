/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei
 * Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source
 * code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.
 *
 * 2.
 * Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the
 * following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *
 * 3.
 * Neither the name of the copyright holder nor the names of its contributors may be used
 * to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED
 * BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
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
    INT32 i = 0;
    INT32 j = 0;
    INT32 len;
    CHAR pathname[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
    CHAR filebuf[260] = "abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123"
        "456789abcedfghij9876550210abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123456789abcedfgh"
        "ij9876550210abcdeabcde0123456789abcedfghij9876550210lalalalalalalala";
    CHAR *bufWrite = NULL;
    CHAR *bufWrite1 = NULL;
    CHAR *bufWrite2 = NULL;
    CHAR *readbuf = NULL;

    g_testCount = 0;

    ret = mkdir(pathname, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    bufWrite = (CHAR *)malloc(8 * BYTES_PER_MBYTES + 1); // 8 * BYTES_PER_MBYTES = 8MB
    ICUNIT_GOTO_NOT_EQUAL(bufWrite, NULL, 0, EXIT1);
    (void)memset_s(bufWrite, 8 * BYTES_PER_MBYTES, 0, 8 * BYTES_PER_MBYTES + 1); // 8 * BYTES_PER_MBYTES = 8MB

    bufWrite1 = (CHAR *)malloc(BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB
    ICUNIT_GOTO_NOT_EQUAL(bufWrite1, NULL, 0, EXIT2);
    (void)memset_s(bufWrite1, BYTES_PER_MBYTES + 1, 0, BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB

    bufWrite2 = (CHAR *)malloc(16 * BYTES_PER_KBYTES + 1); // 16 * BYTES_PER_KBYTES = 16KB
    ICUNIT_GOTO_NOT_EQUAL(bufWrite2, NULL, 0, EXIT3);
    (void)memset_s(bufWrite2, 16 * BYTES_PER_KBYTES + 1, 0, 16 * BYTES_PER_KBYTES + 1); // 16 kb

    for (j = 0; j < 16; j++) {                                         // 16 * 4 * 256 = 16K
        (void)strcat_s(bufWrite2, 16 * BYTES_PER_KBYTES + 1, filebuf); // 16 kb // 16 * BYTES_PER_KBYTES = 16KB
        (void)strcat_s(bufWrite2, 16 * BYTES_PER_KBYTES + 1, filebuf); // 16 kb // 16 * BYTES_PER_KBYTES = 16KB
        (void)strcat_s(bufWrite2, 16 * BYTES_PER_KBYTES + 1, filebuf); // 16 kb // 16 * BYTES_PER_KBYTES = 16KB
        (void)strcat_s(bufWrite2, 16 * BYTES_PER_KBYTES + 1, filebuf); // 16 kb // 16 * BYTES_PER_KBYTES = 16KB
    }

    for (j = 0; j < 16; j++) {                                      // 16 * 4 * 16K = 1M
        (void)strcat_s(bufWrite1, BYTES_PER_MBYTES + 1, bufWrite2); // BYTES_PER_MBYTES = 1MB
        (void)strcat_s(bufWrite1, BYTES_PER_MBYTES + 1, bufWrite2); // BYTES_PER_MBYTES = 1MB
        (void)strcat_s(bufWrite1, BYTES_PER_MBYTES + 1, bufWrite2); // BYTES_PER_MBYTES = 1MB
        (void)strcat_s(bufWrite1, BYTES_PER_MBYTES + 1, bufWrite2); // BYTES_PER_MBYTES = 1MB
    }

    for (i = 0; i < 4; i++) {                                          // 4 * 2 * 1M = 8M
        (void)strcat_s(bufWrite, 8 * BYTES_PER_MBYTES + 1, bufWrite1); // 8 * BYTES_PER_KBYTES = 8KB
        (void)strcat_s(bufWrite, 8 * BYTES_PER_MBYTES + 1, bufWrite1); // 8 * BYTES_PER_KBYTES = 8KB
    }

    free(bufWrite1);
    free(bufWrite2);

    g_testCount++;

    (void)memset_s(g_fatPathname1, FAT_STANDARD_NAME_LENGTH, 0, FAT_STANDARD_NAME_LENGTH);
    (void)strcat_s(g_fatPathname1, FAT_STANDARD_NAME_LENGTH, pathname);
    (void)strcat_s(g_fatPathname1, FAT_STANDARD_NAME_LENGTH, "/031.txt");

    g_fatFd = open64(g_fatPathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(g_fatFd, FAT_IS_ERROR, g_fatFd, EXIT2);

    while (1) {
        ret = write(g_fatFd, bufWrite, strlen(bufWrite));
        if (ret <= 0) {
            if (g_testCount < (4 * BYTES_PER_KBYTES / 8)) { // 4 * BYTES_PER_KBYTES  MB/GB, 8MB per write
                printf("The biggest file size is smaller than the 4GB");
                goto EXIT2;
            }
            printf("The cycle count = :%d,the file size = :%dMB,= :%0.3lfGB\n", g_testCount,
                g_testCount * 8,                             // 8MB per write
                (g_testCount * 8) * 1.0 / BYTES_PER_KBYTES); // BYTES_PER_KBYTES MB/GB, 8MB per write
            break;
        }
        if (g_testCount >= 256 + 1) { // write more than 256 times for 4GB
            printf("The cycle count = :%d,the file size = :%dMB,= :%0.3lfGB\n", g_testCount,
                g_testCount * 8,                             // 8MB per write
                (g_testCount * 8) * 1.0 / BYTES_PER_KBYTES); // BYTES_PER_KBYTES MB/GB, 8MB per write
            break;
        }
        g_testCount++;
    }

    readbuf = (CHAR *)malloc(0xffff + 1);
    ICUNIT_GOTO_NOT_EQUAL(readbuf, NULL, readbuf, EXIT2);
    (void)memset_s(readbuf, 0xffff + 1, 0, 0xffff + 1);

    len = pread64(g_fatFd, readbuf, 0xffff, 0);
    ICUNIT_GOTO_EQUAL(len, 0xffff, len, EXIT2);

    (void)memset_s(readbuf, 0xffff + 1, 0, 0xffff + 1);

    len = pread64(g_fatFd, readbuf, 0xffff, 1000); // offset is 1000
    ICUNIT_GOTO_EQUAL(len, 0xffff, len, EXIT2);

    (void)memset_s(readbuf, 0xffff + 1, 0, 0xffff + 1);

    len = pread64(g_fatFd, readbuf, 0xffff,
        (off64_t)(g_testCount * 8 * // 8mb per write
        BYTES_PER_MBYTES));         // 8M * 8 * BYTES_PER_MBYTES * times = file size, get to file end
    ICUNIT_GOTO_EQUAL(len, 0, len, EXIT2);

    free(readbuf);
    free(bufWrite);

    ret = close(g_fatFd);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = remove(g_fatPathname1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT0);

    ret = remove(pathname);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    return FAT_NO_ERROR;
EXIT3:
    free(bufWrite1);
EXIT2:
    free(readbuf);
    free(bufWrite);
EXIT1:
    close(g_fatFd);
EXIT0:
    remove(g_fatPathname1);
EXIT:
    remove(pathname);
    return FAT_NO_ERROR;
}

/* *
 * @ingroup los_fsoperationbigfile
 * @par type: void
 * API test
 * @brief  pread64 操作超过2G大小的文件
 * @par description： write the file size to 2GB and pread64 it
 * @par precon: task moudle open
 * @par step: see below
 * create 2GB file \n
 * pread64 this file
 * @par expect: see below
 * create file successful \n
 * operator it successful,return successed
 * @par prior: nothing
 */

VOID ItFsFat874(VOID)
{
    TEST_ADD_CASE("IT_FS_FAT_874", TestCase, TEST_VFS, TEST_VFAT, TEST_LEVEL3, TEST_FUNCTION);
}
