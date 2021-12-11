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
    printf("\n-------- LmsMallocTest Start --------\n");
    char *buf = (char *)malloc(16);
    printf("[LmsMallocTest] malloc addr:%p    size:%d\n", buf, 16);

    printf("[LmsMallocTest] read overflow & underflow error should be triggered, read range[-1,16]\n");
    BufReadTest(buf, -1, 16);
    printf("[LmsMallocTest] write overflow error should be triggered, write range[0,16]\n");
    BufWriteTest(buf, 0, 16);

    free(buf);
    printf("\n-------- LmsMallocTest End --------\n");
}

static void LmsReallocTest(void)
{
    printf("\n-------- LmsReallocTest Start --------\n");
    char *buf = (char *)malloc(64);
    printf("[LmsReallocTest] malloc addr:%p size:%d\n", buf, 64);
    printf("[LmsReallocTest] read overflow & underflow error should be triggered, read range[-1,64]\n");
    BufReadTest(buf, -1, 64);
    buf = (char *)realloc(buf, 32);
    printf("[LmsReallocTest] realloc addr:%p size:%d\n", buf, 32);
    printf("[LmsReallocTest] read overflow & underflow error should be triggered, read range[-1,32]\n");
    BufReadTest(buf, -1, 32);
    free(buf);
    printf("\n-------- LmsReallocTest End --------\n");
}

static void LmsCallocTest(void)
{
    printf("\n-------- LmsCallocTest Start --------\n");
    char *buf = (char *)calloc(4, 4);
    printf("[LmsCallocTest] calloc addr:%p size:%d\n", buf, 16);
    printf("[LmsCallocTest] read overflow & underflow error should be triggered, read range[-1,16]\n");
    BufReadTest(buf, -1, 16);
    free(buf);
    printf("\n-------- LmsCallocTest End --------\n");
}

static void LmsVallocTest(void)
{
    printf("\n-------- LmsVallocTest Start --------\n");
    char *buf = (char *)valloc(4096);
    printf("[LmsVallocTest] valloc addr:%p size:%d\n", buf, 4096);
    printf("[LmsVallocTest] read overflow & underflow error should be triggered, read range[-1,4096]\n");
    BufReadTest(buf, -1, 4096);
    free(buf);
    printf("\n-------- LmsVallocTest End --------\n");
}

static void LmsAlignedAllocTest(void)
{
    printf("\n-------- LmsAlignedAllocTest Start --------\n");
    char *buf = (char *)aligned_alloc(64, 128);
    printf("[LmsAlignedAllocTest] aligned_alloc boundsize:%d addr:%p size:%d\n", 64, buf, 128);
    printf("[LmsAlignedAllocTest] read overflow & underflow error should be triggered, read range[-1,128]\n");
    BufReadTest(buf, -1, 128);
    free(buf);
    printf("\n-------- LmsAlignedAllocTest End --------\n");
}

static void LmsMemsetTest(void)
{
    printf("\n-------- LmsMemsetTest Start --------\n");
    char *buf = (char *)malloc(32);
    printf("[LmsMemsetTest] malloc addr:%p size:%d\n", buf, 32);
    printf("[LmsMemsetTest] memset overflow & underflow error should be triggered, memset size:%d\n", 33);
    memset(buf, 0, 33);
    free(buf);
    printf("\n-------- LmsMemsetTest End --------\n");
}

static void LmsMemcpyTest(void)
{
    printf("\n-------- LmsMemcpyTest Start --------\n");
    char *buf = (char *)malloc(20);
    printf("[LmsMemcpyTest] malloc addr:%p size:%d\n", buf, 20);
    char localBuf[32] = {0};
    printf("[LmsMemcpyTest] memcpy overflow error should be triggered, memcpy size:%d\n", 21);
    memcpy(buf, localBuf, 21);
    free(buf);
    printf("\n-------- LmsMemcpyTest End --------\n");
}

static void LmsMemmoveTest(void)
{
    printf("\n-------- LmsMemmoveTest Start --------\n");
    char *buf = (char *)malloc(20);
    printf("[LmsMemmoveTest] malloc addr:%p size:%d\n", buf, 20);
    printf("[LmsMemmoveTest] memmove overflow error should be triggered, dest addr:%p src addr:%p size:%d\n", buf + 12,
        buf, 10);
    memmove(buf + 12, buf, 10);
    free(buf);
    printf("\n-------- LmsMemmoveTest End --------\n");
}

static void LmsStrcpyTest(void)
{
    printf("\n-------- LmsStrcpyTest Start --------\n");
    char *buf = (char *)malloc(16);
    printf("[LmsStrcpyTest] malloc addr:%p size:%d\n", buf, 16);
    char *testStr = "bbbbbbbbbbbbbbbbb";
    printf("[LmsStrcpyTest] strcpy overflow error should be triggered, src string buf size:%d\n", strlen(testStr) + 1);
    strcpy(buf, testStr);
    free(buf);
    printf("\n-------- LmsStrcpyTest End --------\n");
}

static void LmsStrcatTest(void)
{
    printf("\n-------- LmsStrcatTest Start --------\n");
    char *buf = (char *)malloc(16);
    printf("[LmsStrcatTest] malloc addr:%p size:%d\n", buf, 16);
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
    printf("\n-------- LmsFreeTest Start --------\n");
    char *buf = (char *)malloc(16);
    printf("[LmsFreeTest] malloc addr:%p size:%d\n", buf, 16);
    printf("[LmsFreeTest] free addr:%p\n", buf, 16);
    free(buf);
    printf("[LmsFreeTest] Use after free error should be triggered, read addr:%p range[1,1]\n", buf);
    BufReadTest(buf, 1, 1);
    printf("[LmsFreeTest] double free error should be triggered, free addr:%p\n", buf);
    free(buf);
    printf("\n-------- LmsFreeTest End --------\n");
}

int main(int argc, char * const * argv)
{
    printf("\n############### Lms Test start ###############\n");
    char *tmp = (char *)malloc(5000);
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
    printf("\n############### Lms Test End ###############\n");
}