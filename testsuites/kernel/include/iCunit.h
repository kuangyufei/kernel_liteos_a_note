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
#include "los_spinlock.h"

#ifdef TST_DRVPRINT
#include "VOS_typdef.h"
#include "uartdriver.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

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

#define FUNCTION_TEST (1 << 0)

#define PRESSURE_TEST (1 << 1)

#define PERFORMANCE_TEST (1 << 2)

#define TEST_MODE (FUNCTION_TEST)

#define TEST_LESSER_MEM NO


typedef iUINT32 (*CASE_FUNCTION)(void);

typedef struct {
    const iCHAR *pcCaseID;
    CASE_FUNCTION pstCaseFunc;
    iUINT16 testcase_layer;
    iUINT16 testcase_module;
    iUINT16 testcase_level;
    iUINT16 testcase_type;
    iiUINT32 retCode;
    iUINT16 errLine;
} ICUNIT_CASE_S;

typedef struct {
    iUINT16 uwCaseCnt;
    iCHAR *pcSuitID;
    iCHAR *pucFilename;
    ICUNIT_CASE_S *pstCaseList;
    iUINT16 passCnt;
    iUINT16 failCnt;
} ICUNIT_SUIT_S;

typedef enum {
    TEST_TASK = 0,
    TEST_MEM,
    TEST_VM,
    TEST_SEM,
    TEST_MUX,
    TEST_RWLOCK,
    TEST_EVENT,
    TEST_QUE,
    TEST_SWTMR,
    TEST_HWI,
    TEST_MP,
    TEST_ATO,
    TEST_CPUP,
    TEST_SCATTER,
    TEST_RUNSTOP,
    TEST_TIMER,
    TEST_MMU,
    TEST_ROBIN,
    TEST_LIBC,
    TEST_WAIT,
    TEST_VFAT,
    TEST_JFFS,
    TEST_RAMFS,
    TEST_NFS,
    TEST_PROC,
    TEST_FS,
    TEST_UART,
    TEST_PTHREAD,
    TEST_COMP,
    TEST_HWI_HALFBOTTOM,
    TEST_WORKQ,
    TEST_WAKELOCK,
    TEST_TIMES,
    TEST_LIBM,
    TEST_SUPPORT,
    TEST_STL,
    TEST_MAIL,
    TEST_MSG,
    TEST_CP,
    TEST_SIGNAL,
    TEST_SCHED,
    TEST_MTDCHAR,
    TEST_TIME,
    TEST_WRITE,
    TEST_READ,
    TEST_DYNLOAD,
    TEST_REGISTER,
    TEST_UNAME,
    TEST_ERR,
    TEST_CMD,
    TEST_TICKLESS,
    TEST_TRACE,
    TEST_UNALIGNACCESS,
    TEST_EXC,
    TEST_REQULATOR,
    TEST_DEVFREQ,
    TEST_CPUFREQ,
    TEST_MISC,
#if defined(LOSCFG_3RDPARTY_TEST)
    TEST_THTTPD,
    TEST_BIDIREFC,
    TEST_CJSON,
    TEST_CURL,
    TEST_FFMPEG,
    TEST_FREETYPE,
    TEST_INIPARSER,
    TEST_JSONCPP,
    TEST_LIBICONV,
    TEST_LIBJPEG,
    TEST_LIBPNG,
    TEST_OPENEXIF,
    TEST_OPENSSL,
    TEST_OPUS,
    TEST_SQLITE,
    TEST_TINYXML,
    TEST_XML2,
    TEST_ZBAR,
    TEST_HARFBUZZ,
#endif
    TEST_DRIVERBASE,
    TEST_UDP,
    TEST_TCP,
    TEST_MODULE_ALL,
} LiteOS_test_module;

typedef enum {
    TEST_LOS = 0,
    TEST_POSIX,
    TEST_LIB,
    TEST_VFS,
    TEST_EXTEND,
    TEST_PARTITION,
    TEST_CPP,
    TEST_SHELL,
    TEST_LINUX,
    TEST_USB,
#if defined(LOSCFG_3RDPARTY_TEST)
    TEST_3RDPARTY,
#endif
    TEST_DRIVERFRAME,
    TEST_NET_LWIP,
    TEST_LAYER_ALL,
} LiteOS_test_layer;

typedef enum {
    TEST_LEVEL0 = 0,
    TEST_LEVEL1,
    TEST_LEVEL2,
    TEST_LEVEL3,
    TEST_LEVEL4,
    TEST_LEVEL_ALL,
} LiteOS_test_level;

typedef enum {
    TEST_FUNCTION = 0,
    TEST_PRESSURE,
    TEST_PERFORMANCE,
    TEST_TYPE_ALL,
} LiteOS_test_type;

typedef enum {
    TEST_SEQUENCE = 0,
    TEST_RANDOM
} LiteOS_test_sequence;

extern iUINT16 g_iCunitErrLineNo;
extern iiUINT32 g_iCunitErrCode;
extern void ICunitSaveErr(iiUINT32 line, iiUINT32 retCode);

#define ICUNIT_UNINIT 0x0EF00000
#define ICUNIT_OPENFILE_FAILED 0x0EF00001
#define ICUNIT_ALLOC_FAIL 0x0EF00002
#define ICUNIT_SUIT_FULL 0x0EF00002
#define ICUNIT_CASE_FULL 0x0EF00003
#define ICUNIT_SUIT_ALL 0x0EF0FFFF

#define ICUNIT_SUCCESS 0x00000000

#if 1

#define ICUNIT_TRACK_EQUAL(param, g_value, retcode)     \
    do {                                                \
        if ((param) != (g_value)) {                     \
            ICunitSaveErr(__LINE__, (iiUINT32)retcode); \
        }                                               \
    } while (0)


#define ICUNIT_TRACK_NOT_EQUAL(param, g_value, retcode) \
    do {                                                \
        if ((param) == (g_value)) {                     \
            ICunitSaveErr(__LINE__, (iiUINT32)retcode); \
        }                                               \
    } while (0)

#define ICUNIT_ASSERT_NOT_EQUAL_NULL(param, g_value, retcode) \
    do {                                                      \
        if ((param) == (g_value)) {                           \
            ICunitSaveErr(__LINE__, (iiUINT32)retcode);       \
            return NULL;                                      \
        }                                                     \
    } while (0)

#define ICUNIT_ASSERT_EQUAL_NULL(param, g_value, retcode) \
    do {                                                  \
        if ((param) != (g_value)) {                       \
            ICunitSaveErr(__LINE__, (iiUINT32)retcode);   \
            return NULL;                                  \
        }                                                 \
    } while (0)

#define ICUNIT_ASSERT_EQUAL_VOID(param, g_value, retcode) \
    do {                                                  \
        if ((param) != (g_value)) {                       \
            ICunitSaveErr(__LINE__, (iiUINT32)retcode);   \
            return;                                       \
        }                                                 \
    } while (0)

#define ICUNIT_ASSERT_NOT_EQUAL_VOID(param, g_value, retcode) \
    do {                                                      \
        if ((param) == (g_value)) {                           \
            ICunitSaveErr(__LINE__, (iiUINT32)retcode);       \
            return;                                           \
        }                                                     \
    } while (0)

#define ICUNIT_ASSERT_EQUAL(param, g_value, retcode)    \
    do {                                                \
        if ((param) != (g_value)) {                     \
            ICunitSaveErr(__LINE__, (iiUINT32)retcode); \
            return 1;                                   \
        }                                               \
    } while (0)

#define ICUNIT_ASSERT_NOT_EQUAL(param, g_value, retcode) \
    do {                                                 \
        if ((param) == (g_value)) {                      \
            ICunitSaveErr(__LINE__, (iiUINT32)retcode);  \
            return 1;                                    \
        }                                                \
    } while (0)

#define ICUNIT_ASSERT_WITHIN_EQUAL(param, value1, value2, retcode) \
    do {                                                           \
        if ((param) < (value1) || (param) > (value2)) {            \
            ICunitSaveErr(__LINE__, (iiUINT32)retcode);            \
            return 1;                                              \
        }                                                          \
    } while (0)

#define ICUNIT_ASSERT_WITHIN_EQUAL_VOID(param, value1, value2, retcode) \
    do {                                                                \
        if ((param) < (value1) || (param) > (value2)) {                 \
            ICunitSaveErr(__LINE__, (iiUINT32)retcode);                 \
            return;                                                     \
        }                                                               \
    } while (0)

#define ICUNIT_ASSERT_WITHIN_EQUAL_NULL(param, value1, value2, retcode) \
    do {                                                                \
        if ((param) < (value1) || (param) > (value2)) {                 \
            ICunitSaveErr(__LINE__, (iiUINT32)retcode);                 \
            return;                                                     \
        }                                                               \
    } while (0)

#define ICUNIT_ASSERT_SIZE_STRING_EQUAL(str1, str2, strsize, retcode) \
    do {                                                              \
        if (strncmp((str1), (str2), (strsize)) != 0) {                \
            ICunitSaveErr(__LINE__, (iiUINT32)retcode);               \
            return 1;                                                 \
        }                                                             \
    } while (0)

#define ICUNIT_ASSERT_EQUAL_TIME(param, g_value, retcode, label)  \
    do {                                                          \
        if ((param) < (g_value - 1) || (param) > (g_value + 1)) { \
            ICunitSaveErr(__LINE__, (iiUINT32)retcode);           \
            goto label;                                           \
        }                                                         \
    } while (0)

#define ICUNIT_ASSERT_STRING_EQUAL(str1, str2, retcode) \
    do {                                                \
        if (strcmp(str1, str2) != 0) {                  \
            ICunitSaveErr(__LINE__, (iiUINT32)retcode); \
            return 1;                                   \
        }                                               \
    } while (0)

#define ICUNIT_ASSERT_STRING_EQUAL_VOID(str1, str2, retcode) \
    do {                                                     \
        if (strcmp(str1, str2) != 0) {                       \
            ICunitSaveErr(__LINE__, (iiUINT32)retcode);      \
            return;                                          \
        }                                                    \
    } while (0)

#define ICUNIT_ASSERT_STRING_NOT_EQUAL(str1, str2, retcode) \
    do {                                                    \
        if (strcmp(str1, str2) == 0) {                      \
            ICunitSaveErr(__LINE__, (iiUINT32)retcode);     \
            return 1;                                       \
        }                                                   \
    } while (0)

#define ICUNIT_GOTO_EQUAL(param, g_value, retcode, label) \
    do {                                                  \
        if ((param) != (g_value)) {                       \
            ICunitSaveErr(__LINE__, (iiUINT32)retcode);   \
            goto label;                                   \
        }                                                 \
    } while (0)

#define ICUNIT_GOTO_EQUAL_IN(param, value1, value2, retcode, label) \
    do {                                                            \
        if (((param) != (value1)) && ((param) != (value2))) {       \
            ICunitSaveErr(__LINE__, (iiUINT32)retcode);             \
            goto label;                                             \
        }                                                           \
    } while (0)

#define ICUNIT_GOTO_NOT_EQUAL(param, g_value, retcode, label) \
    do {                                                      \
        if ((param) == (g_value)) {                           \
            ICunitSaveErr(__LINE__, (iiUINT32)retcode);       \
            goto label;                                       \
        }                                                     \
    } while (0)

#define ICUNIT_GOTO_WITHIN_EQUAL(param, value1, value2, retcode, label) \
    do {                                                                \
        if ((param) < (value1) || (param) > (value2)) {                 \
            ICunitSaveErr(__LINE__, (iiUINT32)retcode);                 \
            goto label;                                                 \
        }                                                               \
    } while (0)

#define ICUNIT_GOTO_STRING_EQUAL(str1, str2, retcode, label) \
    do {                                                     \
        if (strcmp(str1, str2) != 0) {                       \
            ICunitSaveErr(__LINE__, (iiUINT32)retcode);      \
            goto label;                                      \
        }                                                    \
    } while (0)

#define ICUNIT_GOTO_STRING_NOT_EQUAL(str1, str2, retcode, label) \
    do {                                                         \
        if (strcmp(str1, str2) == 0) {                           \
            ICunitSaveErr(__LINE__, (iiUINT32)retcode);          \
            goto label;                                          \
        }                                                        \
    } while (0)

#else

#define ICUNIT_TRACK_EQUAL(param, g_value, retcode)                                         \
    do {                                                                                    \
        if ((param) != (g_value)) {                                                         \
            g_iCunitErrLineNo = (g_iCunitErrLineNo == 0) ? __LINE__ : g_iCunitErrLineNo;    \
            g_iCunitErrCode = (g_iCunitErrCode == 0) ? (iiUINT32)retcode : g_iCunitErrCode; \
        }                                                                                   \
    } while (0)

#define ICUNIT_TRACK_NOT_EQUAL(param, g_value, retcode)                                     \
    do {                                                                                    \
        if ((param) == (g_value)) {                                                         \
            g_iCunitErrLineNo = (g_iCunitErrLineNo == 0) ? __LINE__ : g_iCunitErrLineNo;    \
            g_iCunitErrCode = (g_iCunitErrCode == 0) ? (iiUINT32)retcode : g_iCunitErrCode; \
        }                                                                                   \
    } while (0)

#define ICUNIT_ASSERT_EQUAL_VOID(param, g_value, retcode)                                   \
    do {                                                                                    \
        if ((param) != (g_value)) {                                                         \
            g_iCunitErrLineNo = (g_iCunitErrLineNo == 0) ? __LINE__ : g_iCunitErrLineNo;    \
            g_iCunitErrCode = (g_iCunitErrCode == 0) ? (iiUINT32)retcode : g_iCunitErrCode; \
            return;                                                                         \
        }                                                                                   \
    } while (0)

#define ICUNIT_ASSERT_NOT_EQUAL_VOID(param, g_value, retcode)                               \
    do {                                                                                    \
        if ((param) == (g_value)) {                                                         \
            g_iCunitErrLineNo = (g_iCunitErrLineNo == 0) ? __LINE__ : g_iCunitErrLineNo;    \
            g_iCunitErrCode = (g_iCunitErrCode == 0) ? (iiUINT32)retcode : g_iCunitErrCode; \
            return;                                                                         \
        }                                                                                   \
    } while (0)
#define ICUNIT_ASSERT_EQUAL(param, g_value, retcode)                                        \
    do {                                                                                    \
        if ((param) != (g_value)) {                                                         \
            g_iCunitErrLineNo = (g_iCunitErrLineNo == 0) ? __LINE__ : g_iCunitErrLineNo;    \
            g_iCunitErrCode = (g_iCunitErrCode == 0) ? (iiUINT32)retcode : g_iCunitErrCode; \
            return 1;                                                                       \
        }                                                                                   \
    } while (0)

#define ICUNIT_ASSERT_NOT_EQUAL(param, g_value, retcode)                                    \
    do {                                                                                    \
        if ((param) == (g_value)) {                                                         \
            g_iCunitErrLineNo = (g_iCunitErrLineNo == 0) ? __LINE__ : g_iCunitErrLineNo;    \
            g_iCunitErrCode = (g_iCunitErrCode == 0) ? (iiUINT32)retcode : g_iCunitErrCode; \
            return 1;                                                                       \
        }                                                                                   \
    } while (0)

#define ICUNIT_ASSERT_STRING_EQUAL(str1, str2, retcode)                                     \
    do {                                                                                    \
        if (strcmp(str1, str2) != 0) {                                                      \
            g_iCunitErrLineNo = (g_iCunitErrLineNo == 0) ? __LINE__ : g_iCunitErrLineNo;    \
            g_iCunitErrCode = (g_iCunitErrCode == 0) ? (iiUINT32)retcode : g_iCunitErrCode; \
            return 1;                                                                       \
        }                                                                                   \
    } while (0)

#define ICUNIT_ASSERT_STRING_NOT_EQUAL(str1, str2, retcode)                                 \
    do {                                                                                    \
        if (strcmp(str1, str2) == 0) {                                                      \
            g_iCunitErrLineNo = (g_iCunitErrLineNo == 0) ? __LINE__ : g_iCunitErrLineNo;    \
            g_iCunitErrCode = (g_iCunitErrCode == 0) ? (iiUINT32)retcode : g_iCunitErrCode; \
            return 1;                                                                       \
        }                                                                                   \
    } while (0)

#define ICUNIT_GOTO_EQUAL(param, g_value, retcode, label)                                   \
    do {                                                                                    \
        if ((param) != (g_value)) {                                                         \
            g_iCunitErrLineNo = (g_iCunitErrLineNo == 0) ? __LINE__ : g_iCunitErrLineNo;    \
            g_iCunitErrCode = (g_iCunitErrCode == 0) ? (iiUINT32)retcode : g_iCunitErrCode; \
            goto label;                                                                     \
        }                                                                                   \
    } while (0)

#define ICUNIT_GOTO_NOT_EQUAL(param, g_value, retcode, label)                               \
    do {                                                                                    \
        if ((param) == (g_value)) {                                                         \
            g_iCunitErrLineNo = (g_iCunitErrLineNo == 0) ? __LINE__ : g_iCunitErrLineNo;    \
            g_iCunitErrCode = (g_iCunitErrCode == 0) ? (iiUINT32)retcode : g_iCunitErrCode; \
            goto label;                                                                     \
        }                                                                                   \
    } while (0)

#define ICUNIT_GOTO_STRING_EQUAL(str1, str2, retcode, label)                                \
    do {                                                                                    \
        if (strcmp(str1, str2) != 0) {                                                      \
            g_iCunitErrLineNo = (g_iCunitErrLineNo == 0) ? __LINE__ : g_iCunitErrLineNo;    \
            g_iCunitErrCode = (g_iCunitErrCode == 0) ? (iiUINT32)retcode : g_iCunitErrCode; \
            goto label;                                                                     \
        }                                                                                   \
    } while (0)

#define ICUNIT_GOTO_STRING_NOT_EQUAL(str1, str2, retcode, label)                            \
    do {                                                                                    \
        if (strcmp(str1, str2) == 0) {                                                      \
            g_iCunitErrLineNo = (g_iCunitErrLineNo == 0) ? __LINE__ : g_iCunitErrLineNo;    \
            g_iCunitErrCode = (g_iCunitErrCode == 0) ? (iiUINT32)retcode : g_iCunitErrCode; \
            goto label;                                                                     \
        }                                                                                   \
    } while (0)
#endif

#if (LOSCFG_KERNEL_SMP == YES)
extern SPIN_LOCK_S g_testSuitSpin;
#define TESTSUIT_LOCK(state) LOS_SpinLockSave(&g_testSuitSpin, &(state))
#define TESTSUIT_UNLOCK(state) LOS_SpinUnlockRestore(&g_testSuitSpin, state)
#endif

extern iUINT32 iCunitAddSuit_F(iCHAR *suitName, iCHAR *pfileName);
#define iCunitAddSuit(suitName) iCunitAddSuit_F(suitName, __FILE__)
extern iUINT32 ICunitAddCase(const iCHAR *caseName, CASE_FUNCTION caseFunc, iUINT16 testcaseLayer,
    iUINT16 testcaseModule, iUINT16 testcaseLevel, iUINT16 testcaseType);

extern iUINT32 ICunitRunTestOne(const char *tcId);
extern INT32 ICunitRunTestArray(const char *tcSequence, const char *tcNum, const char *tcLayer, const char *tcModule,
    const char *tcLevel, const char *tcType);
extern iUINT32 ICunitRunTestArraySequence(iUINT32 testcaseNum, iUINT32 testcaseLayer, iUINT32 testcaseModule,
    iUINT32 testcaseLevel, iUINT32 testcaseType);
extern iUINT32 ICunitRunTestArrayRandom(iUINT32 testcaseNum, iUINT32 testcaseLayer, iUINT32 testcaseModule,
    iUINT32 testcaseLevel, iUINT32 testcaseType);
extern iUINT32 ICunitRunTestcaseSatisfied(ICUNIT_CASE_S *testCase, iUINT32 testcaseLayer, iUINT32 testcaseModule,
    iUINT32 testcaseLevel, iUINT32 testcaseType);

extern iUINT32 ICunitInit(void);
extern iUINT32 ICunitRunSingle(ICUNIT_CASE_S *psubCase);
extern iUINT32 ICunitRunF(ICUNIT_CASE_S *psubCase);

extern iUINT32 iCunitPrintReport(void);


#define TEST_ADD_CASE(name, casefunc, testcase_layer, testcase_module, testcase_level, testcase_type)         \
do     {                                                                                                         \
        iUINT32 uwRet = 1;                                                                                    \
        uwRet = ICunitAddCase(name, (CASE_FUNCTION)casefunc, testcase_layer, testcase_module, testcase_level, \
            testcase_type);                                                                                   \
        ICUNIT_ASSERT_EQUAL_VOID(uwRet, ICUNIT_SUCCESS, uwRet);                                               \
    } while (0)

#define TEST_RUN_SUITE()                                                   \
do     {                                                                      \
        UINT32 uiRet;                                                      \
        uiRet = iCunitRun();                                               \
        ICUNIT_ASSERT_NOT_EQUAL_VOID(uiRet, ICUNIT_UNINIT, ICUNIT_UNINIT); \
    } while (0)

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */

#endif /* _UNI_ICUNIT_H */