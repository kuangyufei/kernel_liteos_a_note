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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void BufWriteTest(void *buf, int start, int end)
{
    for (int i = start; i <= end; i++) {
        ((char *)buf)[i] = 'a';
    }
}

static void BufReadTest(void *buf, int start, int end)
{
    char tmp;
    for (int i = start; i <= end; i++) {
        tmp = ((char *)buf)[i];
    }
}

static void LmsMallocTest(void)
{
#define TEST_SIZE 16
    printf("\n-------- LmsMallocTest Start --------\n");
    char *buf = (char *)malloc(TEST_SIZE);
    if (buf == NULL) {
        return;
    }
    printf("[LmsMallocTest] read overflow & underflow error should be triggered, read range[-1, TEST_SIZE]\n");
    BufReadTest(buf, -1, TEST_SIZE);
    printf("[LmsMallocTest] write overflow error should be triggered, write range[0, TEST_SIZE]\n");
    BufWriteTest(buf, 0, TEST_SIZE);

    free(buf);
    printf("\n-------- LmsMallocTest End --------\n");
}

static void LmsReallocTest(void)
{
#define TEST_SIZE     64
#define TEST_SIZE_MIN 32
    printf("\n-------- LmsReallocTest Start --------\n");
    char *buf = (char *)malloc(TEST_SIZE);
    printf("[LmsReallocTest] read overflow & underflow error should be triggered, read range[-1, TEST_SIZE]\n");
    BufReadTest(buf, -1, TEST_SIZE);
    char *buf1 = (char *)realloc(buf, TEST_SIZE_MIN);
    if (buf1 == NULL) {
        free(buf);
        return;
    }
    buf = NULL;
    printf("[LmsReallocTest] read overflow & underflow error should be triggered, read range[-1, TEST_SIZE_MIN]\n");
    BufReadTest(buf1, -1, TEST_SIZE_MIN);
    free(buf1);
    printf("\n-------- LmsReallocTest End --------\n");
}

static void LmsCallocTest(void)
{
#define TEST_SIZE 16
    printf("\n-------- LmsCallocTest Start --------\n");
    char *buf = (char *)calloc(4, 4); /* 4: test size */
    if (buf == NULL) {
        return;
    }
    printf("[LmsCallocTest] read overflow & underflow error should be triggered, read range[-1, TEST_SIZE]\n");
    BufReadTest(buf, -1, TEST_SIZE);
    free(buf);
    printf("\n-------- LmsCallocTest End --------\n");
}

static void LmsVallocTest(void)
{
#define TEST_SIZE 4096
    printf("\n-------- LmsVallocTest Start --------\n");
    char *buf = (char *)valloc(TEST_SIZE);
    if (buf == NULL) {
        return;
    }
    printf("[LmsVallocTest] read overflow & underflow error should be triggered, read range[-1, TEST_SIZE]\n");
    BufReadTest(buf, -1, TEST_SIZE);
    free(buf);
    printf("\n-------- LmsVallocTest End --------\n");
}

static void LmsAlignedAllocTest(void)
{
#define TEST_ALIGN_SIZE 64
#define TEST_SIZE       128
    printf("\n-------- LmsAlignedAllocTest Start --------\n");
    char *buf = (char *)aligned_alloc(TEST_ALIGN_SIZE, TEST_SIZE);
    if (buf == NULL) {
        return;
    }
    printf("[LmsAlignedAllocTest] read overflow & underflow error should be triggered, read range[-1,128]\n");
    BufReadTest(buf, -1, 128);
    free(buf);
    printf("\n-------- LmsAlignedAllocTest End --------\n");
}

static void LmsMemsetTest(void)
{
#define TEST_SIZE 32
    printf("\n-------- LmsMemsetTest Start --------\n");
    char *buf = (char *)malloc(TEST_SIZE);
    if (buf == NULL) {
        return;
    }
    printf("[LmsMemsetTest] memset overflow & underflow error should be triggered, memset size:%d\n", TEST_SIZE + 1);
    memset(buf, 0, TEST_SIZE + 1);
    free(buf);
    printf("\n-------- LmsMemsetTest End --------\n");
}

static void LmsMemcpyTest(void)
{
#define TEST_SIZE 20
    printf("\n-------- LmsMemcpyTest Start --------\n");
    char *buf = (char *)malloc(TEST_SIZE);
    if (buf == NULL) {
        return;
    }
    char localBuf[32] = {0}; /* 32: test size */
    printf("[LmsMemcpyTest] memcpy overflow error should be triggered, memcpy size:%d\n", TEST_SIZE + 1);
    memcpy(buf, localBuf, TEST_SIZE + 1);
    free(buf);
    printf("\n-------- LmsMemcpyTest End --------\n");
}

static void LmsMemmoveTest(void)
{
#define TEST_SIZE 20
    printf("\n-------- LmsMemmoveTest Start --------\n");
    char *buf = (char *)malloc(TEST_SIZE);
    if (buf == NULL) {
        return;
    }
    printf("[LmsMemmoveTest] memmove overflow error should be triggered\n");
    memmove(buf + 12, buf, 10); /* 12 and 10: test size */
    free(buf);
    printf("\n-------- LmsMemmoveTest End --------\n");
}

static void LmsStrcpyTest(void)
{
#define TEST_SIZE 16
    printf("\n-------- LmsStrcpyTest Start --------\n");
    char *buf = (char *)malloc(TEST_SIZE);
    if (buf == NULL) {
        return;
    }
    char *testStr = "bbbbbbbbbbbbbbbbb";
    printf("[LmsStrcpyTest] strcpy overflow error should be triggered, src string buf size:%d\n", strlen(testStr) + 1);
    strcpy(buf, testStr);
    free(buf);
    printf("\n-------- LmsStrcpyTest End --------\n");
}

static void LmsStrcatTest(void)
{
#define TEST_SIZE 16
    printf("\n-------- LmsStrcatTest Start --------\n");
    char *buf = (char *)malloc(TEST_SIZE);
    if (buf == NULL) {
        return;
    }
    buf[0] = 'a';
    buf[1] = 'b';
    buf[2] = 0;
    char *testStr = "cccccccccccccc";
    printf("[LmsStrcatTest] strcat overflow error should be triggered, src string:%s dest string:%s"
        "total buf size:%d\n",
        testStr, buf, strlen(testStr) + strlen(buf) + 1);
    strcat(buf, testStr);
    free(buf);
    printf("\n-------- LmsStrcatTest End --------\n");
}

static void LmsFreeTest(void)
{
#define TEST_SIZE 16
    printf("\n-------- LmsFreeTest Start --------\n");
    char *buf = (char *)malloc(TEST_SIZE);
    if (buf == NULL) {
        return;
    }
    printf("[LmsFreeTest] free size:%d\n", TEST_SIZE);
    free(buf);
    printf("[LmsFreeTest] Use after free error should be triggered, read range[1,1]\n");
    BufReadTest(buf, 1, 1);
    printf("[LmsFreeTest] double free error should be triggered\n");
    free(buf);
    printf("\n-------- LmsFreeTest End --------\n");
}

int main(int argc, char * const *argv)
{
    (void)argc;
    (void)argv;
    printf("\n############### Lms Test start ###############\n");
    char *tmp = (char *)malloc(5000); /* 5000: test mem size */
    if (tmp == NULL) {
        return;
    }
    LmsMallocTest();
    LmsReallocTest();
    LmsCallocTest();
    LmsVallocTest();
    LmsAlignedAllocTest();
    LmsMemsetTest();
    LmsMemcpyTest();
    LmsMemmoveTest();
    LmsStrcpyTest();
    LmsStrcatTest();
    LmsFreeTest();
    free(tmp);
    printf("\n############### Lms Test End ###############\n");
}