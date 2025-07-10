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

/**
 * @brief 内存写入测试函数
 * @param buf 待写入的内存缓冲区指针
 * @param start 起始写入索引（含）
 * @param end 结束写入索引（含）
 * @note 将缓冲区从start到end的所有字节设置为字符'a'
 */
static void BufWriteTest(void *buf, int start, int end)
{
    for (int i = start; i <= end; i++) {
        ((char *)buf)[i] = 'a';  // 类型转换后按字节写入
    }
}

/**
 * @brief 内存读取测试函数
 * @param buf 待读取的内存缓冲区指针
 * @param start 起始读取索引（含）
 * @param end 结束读取索引（含）
 * @note 从缓冲区start到end读取所有字节到临时变量，用于检测越界访问
 */
static void BufReadTest(void *buf, int start, int end)
{
    char tmp;  // 临时变量存储读取结果
    for (int i = start; i <= end; i++) {
        tmp = ((char *)buf)[i];  // 类型转换后按字节读取
    }
}

/**
 * @brief malloc内存分配测试
 * @details 测试malloc分配内存后的越界读写错误检测
 *          - 读取越界：访问[-1, TEST_SIZE]范围
 *          - 写入越界：访问[0, TEST_SIZE]范围
 */
static void LmsMallocTest(void)
{
#define TEST_SIZE 16  // 测试内存大小：16字节
    printf("\n-------- LmsMallocTest Start --------\n");
    char *buf = (char *)malloc(TEST_SIZE);
    if (buf == NULL) {  // 内存分配失败检查
        return;
    }
    printf("[LmsMallocTest] read overflow & underflow error should be triggered, read range[-1, TEST_SIZE]\n");
    BufReadTest(buf, -1, TEST_SIZE);  // 触发读越界（下溢+上溢）
    printf("[LmsMallocTest] write overflow error should be triggered, write range[0, TEST_SIZE]\n");
    BufWriteTest(buf, 0, TEST_SIZE);  // 触发写越界（上溢）

    free(buf);  // 释放内存
    printf("\n-------- LmsMallocTest End --------\n");
}

/**
 * @brief realloc内存重分配测试
 * @details 测试realloc调整内存大小后的越界读写错误检测
 *          1. 初始分配TEST_SIZE字节并测试越界读
 *          2. 重分配为TEST_SIZE_MIN字节并测试新内存越界读
 */
static void LmsReallocTest(void)
{
#define TEST_SIZE     64   // 初始分配大小：64字节
#define TEST_SIZE_MIN 32   // 重分配后大小：32字节
    printf("\n-------- LmsReallocTest Start --------\n");
    char *buf = (char *)malloc(TEST_SIZE);
    printf("[LmsReallocTest] read overflow & underflow error should be triggered, read range[-1, TEST_SIZE]\n");
    BufReadTest(buf, -1, TEST_SIZE);  // 测试原始内存读越界
    char *buf1 = (char *)realloc(buf, TEST_SIZE_MIN);
    if (buf1 == NULL) {  // 重分配失败处理
        free(buf);
        return;
    }
    buf = NULL;  // 避免野指针
    printf("[LmsReallocTest] read overflow & underflow error should be triggered, read range[-1, TEST_SIZE_MIN]\n");
    BufReadTest(buf1, -1, TEST_SIZE_MIN);  // 测试重分配后内存读越界
    free(buf1);
    printf("\n-------- LmsReallocTest End --------\n");
}

/**
 * @brief calloc内存分配测试
 * @details 测试calloc分配内存后的越界读错误检测
 *          calloc(4,4)等价于分配16字节（4*4）并初始化为0
 */
static void LmsCallocTest(void)
{
#define TEST_SIZE 16  // 测试内存大小：16字节
    printf("\n-------- LmsCallocTest Start --------\n");
    char *buf = (char *)calloc(4, 4); /* 4: test size */
    if (buf == NULL) {
        return;
    }
    printf("[LmsCallocTest] read overflow & underflow error should be triggered, read range[-1, TEST_SIZE]\n");
    BufReadTest(buf, -1, TEST_SIZE);  // 触发读越界
    free(buf);
    printf("\n-------- LmsCallocTest End --------\n");
}

/**
 * @brief valloc内存分配测试
 * @details 测试valloc分配页对齐内存后的越界读错误检测
 *          valloc分配内存大小为系统页大小的整数倍（此处TEST_SIZE=4096）
 */
static void LmsVallocTest(void)
{
#define TEST_SIZE 4096  // 测试内存大小：4096字节（页大小）
    printf("\n-------- LmsVallocTest Start --------\n");
    char *buf = (char *)valloc(TEST_SIZE);
    if (buf == NULL) {
        return;
    }
    printf("[LmsVallocTest] read overflow & underflow error should be triggered, read range[-1, TEST_SIZE]\n");
    BufReadTest(buf, -1, TEST_SIZE);  // 触发读越界
    free(buf);
    printf("\n-------- LmsVallocTest End --------\n");
}

/**
 * @brief aligned_alloc内存对齐分配测试
 * @details 测试按指定对齐方式分配内存后的越界读错误检测
 *          aligned_alloc(align, size)要求size必须是align的整数倍
 */
static void LmsAlignedAllocTest(void)
{
#define TEST_ALIGN_SIZE 64  // 对齐大小：64字节
#define TEST_SIZE       128 // 分配大小：128字节（必须是64的倍数）
    printf("\n-------- LmsAlignedAllocTest Start --------\n");
    char *buf = (char *)aligned_alloc(TEST_ALIGN_SIZE, TEST_SIZE);
    if (buf == NULL) {
        return;
    }
    printf("[LmsAlignedAllocTest] read overflow & underflow error should be triggered, read range[-1,128]\n");
    BufReadTest(buf, -1, 128);  // 触发读越界
    free(buf);
    printf("\n-------- LmsAlignedAllocTest End --------\n");
}

/**
 * @brief memset内存初始化测试
 * @details 测试memset函数的缓冲区溢出错误检测
 *          故意设置超出分配大小的初始化长度（TEST_SIZE+1）
 */
static void LmsMemsetTest(void)
{
#define TEST_SIZE 32  // 分配内存大小：32字节
    printf("\n-------- LmsMemsetTest Start --------\n");
    char *buf = (char *)malloc(TEST_SIZE);
    if (buf == NULL) {
        return;
    }
    printf("[LmsMemsetTest] memset overflow & underflow error should be triggered, memset size:%d\n", TEST_SIZE + 1);
    memset(buf, 0, TEST_SIZE + 1);  // 触发缓冲区溢出（写入大小33字节）
    free(buf);
    printf("\n-------- LmsMemsetTest End --------\n");
}

/**
 * @brief memcpy内存拷贝测试
 * @details 测试memcpy函数的源缓冲区溢出错误检测
 *          故意设置超出目标缓冲区大小的拷贝长度（TEST_SIZE+1）
 */
static void LmsMemcpyTest(void)
{
#define TEST_SIZE 20  // 目标缓冲区大小：20字节
    printf("\n-------- LmsMemcpyTest Start --------\n");
    char *buf = (char *)malloc(TEST_SIZE);
    if (buf == NULL) {
        return;
    }
    char localBuf[32] = {0}; /* 32: test size */
    printf("[LmsMemcpyTest] memcpy overflow error should be triggered, memcpy size:%d\n", TEST_SIZE + 1);
    memcpy(buf, localBuf, TEST_SIZE + 1);  // 触发溢出（拷贝21字节）
    free(buf);
    printf("\n-------- LmsMemcpyTest End --------\n");
}

/**
 * @brief memmove内存移动测试
 * @details 测试memmove函数的内存重叠拷贝错误检测
 *          源地址和目标地址存在重叠区域
 */
static void LmsMemmoveTest(void)
{
#define TEST_SIZE 20  // 分配内存大小：20字节
    printf("\n-------- LmsMemmoveTest Start --------\n");
    char *buf = (char *)malloc(TEST_SIZE);
    if (buf == NULL) {
        return;
    }
    printf("[LmsMemmoveTest] memmove overflow error should be triggered\n");
    memmove(buf + 12, buf, 10); /* 12和10: 测试偏移和长度，造成内存重叠 */
    free(buf);
    printf("\n-------- LmsMemmoveTest End --------\n");
}

/**
 * @brief strcpy字符串拷贝测试
 * @details 测试strcpy函数的源字符串过长导致的溢出错误检测
 *          源字符串长度（含终止符）超过目标缓冲区大小
 */
static void LmsStrcpyTest(void)
{
#define TEST_SIZE 16  // 目标缓冲区大小：16字节
    printf("\n-------- LmsStrcpyTest Start --------\n");
    char *buf = (char *)malloc(TEST_SIZE);
    if (buf == NULL) {
        return;
    }
    char *testStr = "bbbbbbbbbbbbbbbbb";  // 长度17字节（含\0）
    printf("[LmsStrcpyTest] strcpy overflow error should be triggered, src string buf size:%d\n",
           (int)strlen(testStr) + 1);
    strcpy(buf, testStr);  // 触发字符串溢出
    free(buf);
    printf("\n-------- LmsStrcpyTest End --------\n");
}

/**
 * @brief strcat字符串拼接测试
 * @details 测试strcat函数的拼接后字符串过长导致的溢出错误检测
 *          源字符串+目标字符串长度（含终止符）超过目标缓冲区大小
 */
static void LmsStrcatTest(void)
{
#define TEST_SIZE 16  // 目标缓冲区大小：16字节
    printf("\n-------- LmsStrcatTest Start --------\n");
    char *buf = (char *)malloc(TEST_SIZE);
    if (buf == NULL) {
        return;
    }
    buf[0] = 'a';
    buf[1] = 'b';
    buf[2] = 0;  // 初始化目标字符串为"ab"
    char *testStr = "cccccccccccccc";  // 源字符串长度14字节
    printf("[LmsStrcatTest] strcat overflow error should be triggered, src string:%s dest string:%s"
        "total buf size:%d\n",
        testStr, buf, strlen(testStr) + strlen(buf) + 1);
    strcat(buf, testStr);  // 拼接后总长度17字节（2+14+1），触发溢出
    free(buf);
    printf("\n-------- LmsStrcatTest End --------\n");
}

/**
 * @brief 内存释放测试
 * @details 测试内存释放后的错误使用检测
 *          - 使用已释放内存（use-after-free）
 *          - 重复释放内存（double free）
 */
static void LmsFreeTest(void)
{
#define TEST_SIZE 16  // 测试内存大小：16字节
    printf("\n-------- LmsFreeTest Start --------\n");
    char *buf = (char *)malloc(TEST_SIZE);
    if (buf == NULL) {
        return;
    }
    printf("[LmsFreeTest] free size:%d\n", TEST_SIZE);
    free(buf);  // 首次释放
    printf("[LmsFreeTest] Use after free error should be triggered, read range[1,1]\n");
    BufReadTest(buf, 1, 1);  // 使用已释放内存
    printf("[LmsFreeTest] double free error should be triggered\n");
    free(buf);  // 重复释放
    printf("\n-------- LmsFreeTest End --------\n");
}

/**
 * @brief LMS内存安全测试主函数
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 0表示测试完成，-1表示初始化失败
 * @details 依次执行各类内存操作的安全测试，包括：
 *          malloc/realloc/calloc/valloc/aligned_alloc内存分配测试
 *          memset/memcpy/memmove内存操作测试
 *          strcpy/strcat字符串操作测试
 *          free内存释放测试
 */
int main(int argc, char * const *argv)
{
    (void)argc;  // 未使用参数
    (void)argv;  // 未使用参数
    printf("\n############### Lms Test start ###############\n");
    char *tmp = (char *)malloc(5000); /* 5000: 测试内存大小 */
    if (tmp == NULL) {
        return -1;  // 内存分配失败，测试初始化失败
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
    return 0;
}