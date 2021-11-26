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
#ifndef _UNI_ICUNIT_H
#define _UNI_ICUNIT_H

#include <string.h>
#include <stdio.h>
#include "los_typedef.h"
#include <climits>
#include <gtest/gtest.h>

typedef unsigned short iUINT16;
typedef unsigned int iUINT32;
typedef signed short iINT16;
typedef signed long iINT32;
typedef char iCHAR;
typedef void iVOID;

typedef unsigned long iiUINT32;


#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#define ICUNIT_SUCCESS 0x00000000

#define FUNCTION_TEST (1 << 0)

#define PRESSURE_TEST (1 << 1)

#define PERFORMANCE_TEST (1 << 2)

#define TEST_MODE (FUNCTION_TEST)

#define TEST_LESSER_MEM 0

#define TEST_ADD_CASE(string, TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION) \
    do {                                                                                  \
        UINT32 ret;                                                                       \
        ret = TestCase();                                                                 \
        ASSERT_EQ(ret, LOS_OK);                                                           \
    } while (0)

#if 1
/* *
 * 跟踪param和value不同，不做任何处理
 */
#define ICUNIT_TRACK_EQUAL(param, value, retcode)    \
    do {                                             \
        if ((param) != (value)) {                    \
            EXPECT_EQ((long)(value), (long)(param)); \
        }                                            \
    } while (0)

/* *
 * 跟踪param和value相同，不做任何处理
 */
#define ICUNIT_TRACK_NOT_EQUAL(param, value, retcode) \
    do {                                              \
        if ((param) == (value)) {                     \
            EXPECT_NE((long)(value), (long)(param));  \
        }                                             \
    } while (0)


#define ICUNIT_ASSERT_NOT_EQUAL_NULL(param, value, retcode) \
    do {                                                    \
        if ((param) == (value)) {                           \
            EXPECT_NE((long)(value), (long)(param));        \
            return NULL;                                    \
        }                                                   \
    } while (0)


#define ICUNIT_ASSERT_EQUAL_NULL(param, value, retcode) \
    do {                                                \
        if ((param) != (value)) {                       \
            EXPECT_EQ((long)(value), (long)(param));    \
            return NULL;                                \
        }                                               \
    } while (0)


/* *
 * 断言param和value相同，不同则return
 */
#define ICUNIT_ASSERT_EQUAL_VOID(param, value, retcode) \
    do {                                                \
        if ((param) != (value)) {                       \
            EXPECT_EQ((long)(value), (long)(param));    \
            return;                                     \
        }                                               \
    } while (0)


/* *
 * 断言param和value不同，相同则return
 */
#define ICUNIT_ASSERT_NOT_EQUAL_VOID(param, value, retcode) \
    do {                                                    \
        if ((param) == (value)) {                           \
            EXPECT_NE((long)(value), (long)(param));        \
            return;                                         \
        }                                                   \
    } while (0)

#define ICUNIT_ASSERT_EQUAL(param, value, retcode)       \
    do {                                                 \
        if ((param) != (value)) {                        \
            EXPECT_EQ((long)(param), (long)(value));     \
            return 1;                                    \
        }                                                \
    } while (0)


#define ICUNIT_ASSERT_TWO_EQUAL(param, value1, value2, retcode) \
    do {                                                        \
        if (((param) != (value1)) && ((param) != (value2))) {   \
            EXPECT_EQ((long)(value1), (long)(param));           \
            EXPECT_EQ((long)(value2), (long)(param));           \
            return 1;                                           \
        }                                                       \
    } while (0)

/* *
 * 判断param等于value1或value2，不等则跳转到label处
 */
#define ICUNIT_GOTO_TWO_EQUAL(param, value1, value2, retcode, label) \
    do {                                                             \
        if (((param) != (value1)) && ((param) != (value2))) {        \
            EXPECT_NE((long)(retcode), (long)(retcode));             \
            goto label;                                              \
        }                                                            \
    } while (0)

/* *
 * 断言param和value不同，相同则return
 */
#define ICUNIT_ASSERT_NOT_EQUAL(param, value, retcode) \
    do {                                               \
        if ((param) == (value)) {                      \
            EXPECT_NE(retcode, retcode);               \
            return 1;                                  \
        }                                              \
    } while (0)
/* *
 * 断言param不在value1和value2之间就return
 */
#define ICUNIT_ASSERT_WITHIN_EQUAL(param, value1, value2, retcode) \
    do {                                                           \
        if ((param) < (value1) || (param) > (value2)) {            \
            EXPECT_GE((long)(param), (long)(value1));              \
            EXPECT_LE((long)(param), (long)(value2));              \
            return 1;                                              \
        }                                                          \
    } while (0)


/* *
 * 断言param是否在一定范围内，不在则return
 */
#define ICUNIT_ASSERT_WITHIN_EQUAL_VOID(param, value1, value2, retcode) \
    do {                                                                \
        if ((param) < (value1) || (param) > (value2)) {                 \
            EXPECT_GE((long)(param), (long)(value1));                   \
            EXPECT_LE((long)(param), (long)(value2));                   \
            return;                                                     \
        }                                                               \
    } while (0)


/* *
 * 断言param是否在一定范围内，不在则return
 */
#define ICUNIT_ASSERT_WITHIN_EQUAL_NULL(param, value1, value2, retcode) \
    do {                                                                \
        if ((param) < (value1) || (param) > (value2)) {                 \
            EXPECT_GE((long)(param), (long)(value1));                   \
            EXPECT_LE((long)(param), (long)(value2));                   \
            return NULL;                                                \
        }                                                               \
    } while (0)


/* *
 * 断言指定个数str1和str2字符串内容相同，不同则return
 */

#define ICUNIT_ASSERT_SIZE_STRING_EQUAL(str1, str2, strsize, retcode) \
    do {                                                              \
        if (strncmp((str1), (str2), (strsize)) != 0) {                \
            EXPECT_NE((long)(retcode), (long)(retcode));              \
            return 1;                                                 \
        }                                                             \
    } while (0)


/* *
 * 判断param和value是否相同，不同则跳转到label处
 */
#define ICUNIT_ASSERT_EQUAL_TIME(param, value, retcode, label) \
    do {                                                       \
        if ((param) < (value - 1) || (param) > (value + 1)) {  \
            EXPECT_NE((long)(retcode), (long)(retcode));       \
            goto label;                                        \
        }                                                      \
    } while (0)


#if 1
/* *
 * 断言指定个数str1和str2字符串内容相同，不同则return
 */
#define ICUNIT_ASSERT_SIZE_STRING_EQUAL_VOID(str1, str2, size, retcode) \
    do {                                                                \
        if (strncmp(str1, str2, size) != 0) {                           \
            EXPECT_NE((long)(retcode), (long)(retcode));                \
            return;                                                     \
        }                                                               \
    } while (0)

/* *
 * 断言指定个数str1和str2字符串内容不同，相同则return
 */
#define ICUNIT_ASSERT_SIZE_STRING_NOT_EQUAL(str1, str2, size, retcode) \
    do {                                                               \
        if (strncmp(str1, str2, size) == 0) {                          \
            EXPECT_NE((long)(retcode), (long)(retcode));               \
            return 1;                                                  \
        }                                                              \
    } while (0)
#endif

/* *
 * 断言str1和str2字符串内容相同，不同则return
 */
#define ICUNIT_ASSERT_STRING_EQUAL(str1, str2, retcode)  \
    do {                                                 \
        if (strcmp(str1, str2) != 0) {                   \
            EXPECT_NE((long)(retcode), (long)(retcode)); \
            return 1;                                    \
        }                                                \
    } while (0)

/* *
 * 断言str1和str2字符串内容相同，不同则return
 */
#define ICUNIT_ASSERT_STRING_EQUAL_VOID(str1, str2, retcode) \
    do {                                                     \
        if (strcmp(str1, str2) != 0) {                       \
            EXPECT_NE((long)(retcode), (long)(retcode));     \
            return;                                          \
        }                                                    \
    } while (0)

#define ICUNIT_ASSERT_STRING_EQUAL_RET(str1, str2, retcode, ret) \
    do {                                                         \
        if (strcmp(str1, str2) != 0) {                           \
            EXPECT_NE((long)(retcode), (long)(retcode));         \
            return (ret);                                        \
        }                                                        \
    } while (0)

/* *
 * 断言str1和str2字符串内容不同，相同则return
 */
#define ICUNIT_ASSERT_STRING_NOT_EQUAL(str1, str2, retcode) \
    do {                                                    \
        if (strcmp(str1, str2) == 0) {                      \
            EXPECT_NE((long)(retcode), (long)(retcode));    \
            return 1;                                       \
        }                                                   \
    } while (0)

/* *
 * 判断param和value是否相同，不同则跳转到label处
 */
#define ICUNIT_GOTO_EQUAL(param, value, retcode, label) \
    do {                                                \
        if ((param) != (value)) {                       \
            EXPECT_EQ((long)(value), (long)(param));    \
            goto label;                                 \
        }                                               \
    } while (0)


#define ICUNIT_GOTO_EQUAL_IN(param, value1, value2, retcode, label) \
    do {                                                            \
        if (((param) != (value1)) && ((param) != (value2))) {       \
            EXPECT_NE((long)(retcode), (long)(retcode));            \
            goto label;                                             \
        }                                                           \
    } while (0)

/* *
 * 判断param和value是否不同，相同则跳转到label处
 */
#define ICUNIT_GOTO_NOT_EQUAL(param, value, retcode, label) \
    do {                                                    \
        if ((param) == (value)) {                           \
            EXPECT_NE((long)(value), (long)(param));        \
            goto label;                                     \
        }                                                   \
    } while (0)


#define ICUNIT_GOTO_WITHIN_EQUAL(param, value1, value2, retcode, label) \
    do {                                                                \
        if ((param) < (value1) || (param) > (value2)) {                 \
            EXPECT_GE((long)(param), (long)(value1));                   \
            EXPECT_LE((long)(param), (long)(value2));                   \
            goto label;                                                 \
        }                                                               \
    } while (0)


/* *
 * 判断str1和str2是否相同，不同则跳转到label处
 */
#define ICUNIT_GOTO_STRING_EQUAL(str1, str2, retcode, label) \
    do {                                                     \
        if (strcmp(str1, str2) != 0) {                       \
            EXPECT_NE((long)(retcode), (long)(retcode));     \
            goto label;                                      \
        }                                                    \
    } while (0)


/* *
 * 判断str1和str2是否不同，相同则跳转到label处
 */
#define ICUNIT_GOTO_STRING_NOT_EQUAL(str1, str2, retcode, label) \
    do {                                                         \
        if (strcmp(str1, str2) == 0) {                           \
            EXPECT_NE((long)(retcode), (long)(retcode));         \
            goto label;                                          \
        }                                                        \
    } while (0)

/* *
 * 跟踪param和value不同，不做任何处理
 */
#define ICUNIT_ASSERT_EQUAL_EXIT(param, value, exitcode) \
    do {                                                 \
        if ((param) != (value)) {                        \
            EXPECT_EQ((long)(value), (long)(param));     \
            exit(exitcode);                              \
        }                                                \
    } while (0)

#define ICUNIT_ASSERT_NOT_EQUAL_NULL_VOID(param, value, retcode) \
    do {                                                         \
        if ((param) == (value)) {                                \
            EXPECT_NE((long)(value), (long)(param));             \
            return NULL;                                         \
        }                                                        \
    } while (0)

#define ICUNIT_ASSERT_EQUAL_NULL_VOID(param, value, retcode) \
    do {                                                     \
        if ((param) != (value)) {                            \
            EXPECT_EQ((long)(value), (long)(param));         \
            return NULL;                                     \
        }                                                    \
    } while (0)

#endif

#endif /* _UNI_ICUNIT_H */
