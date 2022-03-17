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
#include "stdio.h"
#include <climits>
#include <gtest/gtest.h>


#include "It_posix_pthread.h"

pthread_key_t g_key;
pthread_key_t g_key1;
pthread_key_t g_key2;
pthread_key_t g_pthreadKeyTest[PTHREAD_KEY_NUM];
pthread_t g_newTh;
pthread_t g_newTh2;
UINT32 g_taskMaxNum = LOSCFG_BASE_CORE_TSK_CONFIG;
pthread_once_t g_onceControl = PTHREAD_ONCE_INIT;
pthread_cond_t g_pthreadCondTest1 = PTHREAD_COND_INITIALIZER;
pthread_mutex_t g_pthreadMutexTest1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_pthreadMutexTest2 = PTHREAD_MUTEX_INITIALIZER;
INT32 g_startNum = 0;
INT32 g_wakenNum = 0;
INT32 g_t1Start = 0;
INT32 g_signaled = 0;
INT32 g_wokenUp = -1;
INT32 g_lowDone = -1;
INT32 g_pthreadSem = 0; /* Manual semaphore */
INT32 g_pthreadScopeValue = 0;
INT32 g_pthreadSchedInherit = 0;
INT32 g_pthreadSchedPolicy = 0;
struct testdata g_td;

sem_t g_pthreadSem1;
sem_t g_pthreadSem2;

__scenario g_scenarii[] = {
    CASE_POS(0, 0, 0, 0, 0, 0, 0, 0, (char *)"default"),
    CASE_POS(1, 0, 0, 0, 0, 0, 0, 0, (char *)"detached"),
    CASE_POS(0, 1, 0, 0, 0, 0, 0, 0, (char *)"Explicit sched"),
    CASE_UNK(0, 0, 1, 0, 0, 0, 0, 0, (char *)"FIFO Policy"),
    CASE_UNK(0, 0, 2, 0, 0, 0, 0, 0, (char *)"RR Policy"),
    CASE_UNK(0, 0, 0, 1, 0, 0, 0, 0, (char *)"Max sched param"),
    CASE_UNK(0, 0, 0, -1, 0, 0, 0, 0, (char *)"Min sched param"),
    CASE_POS(0, 0, 0, 0, 1, 0, 0, 0, (char *)"Alternative contension scope"),
    CASE_POS(0, 0, 0, 0, 0, 1, 0, 0, (char *)"Alternative stack"),
    CASE_POS(0, 0, 0, 0, 0, 0, 1, 0, (char *)"No guard size"),
    CASE_UNK(0, 0, 0, 0, 0, 0, 2, 0, (char *)"1p guard size"),
    CASE_POS(0, 0, 0, 0, 0, 0, 0, 1, (char *)"Min stack size"),
    /* Stack play */
    CASE_POS(0, 0, 0, 0, 0, 0, 1, 1, (char *)"Min stack size, no guard"),
    CASE_UNK(0, 0, 0, 0, 0, 0, 2, 1, (char *)"Min stack size, 1p guard"),
    CASE_POS(1, 0, 0, 0, 0, 1, 0, 0, (char *)"Detached, Alternative stack"),
    CASE_POS(1, 0, 0, 0, 0, 0, 1, 1, (char *)"Detached, Min stack size, no guard"),
    CASE_UNK(1, 0, 0, 0, 0, 0, 2, 1, (char *)"Detached, Min stack size, 1p guard"),
};

pthread_t g_pthreadTestTh;

VOID ScenarInit(VOID)
{
    INT32 ret = 0;
    UINT32 i;
    INT32 old;
    long pagesize, minstacksize;
    long tsa, tss, tps;

    pagesize = sysconf(_SC_PAGESIZE);
    minstacksize = sysconf(_SC_THREAD_STACK_MIN);
    tsa = sysconf(_SC_THREAD_ATTR_STACKADDR);
    tss = sysconf(_SC_THREAD_ATTR_STACKSIZE);
    tps = sysconf(_SC_THREAD_PRIORITY_SCHEDULING);

    if (minstacksize % pagesize)
        ICUNIT_ASSERT_EQUAL_VOID(1, 0, errno);

    for (i = 0; i < NSCENAR; i++) {
        ret = pthread_attr_init(&g_scenarii[i].ta);
        ICUNIT_ASSERT_EQUAL_VOID(ret, PTHREAD_NO_ERROR, ret);

        if (g_scenarii[i].detached == 1) {
            ret = pthread_attr_setdetachstate(&g_scenarii[i].ta, PTHREAD_CREATE_DETACHED);
            ICUNIT_TRACK_EQUAL(ret, PTHREAD_NO_ERROR, ret);

            ret = pthread_attr_getdetachstate(&g_scenarii[i].ta, &old);
            ICUNIT_TRACK_EQUAL(ret, PTHREAD_NO_ERROR, ret);
            ICUNIT_TRACK_EQUAL(old, PTHREAD_CREATE_JOINABLE, old);
        }

        /* Sched related attributes */
        /*
         * This routine is dependent on the Thread Execution
         * Scheduling option
         */
        if (tps > 0) {
            if (g_scenarii[i].explicitsched == 1)
                ret = pthread_attr_setinheritsched(&g_scenarii[i].ta, PTHREAD_EXPLICIT_SCHED);
            else
                ret = pthread_attr_setinheritsched(&g_scenarii[i].ta, PTHREAD_INHERIT_SCHED);

            ICUNIT_TRACK_EQUAL(ret, PTHREAD_NO_ERROR, ret);
        }

        if (tps > 0) {
            if (g_scenarii[i].schedpolicy == 1)
                ret = pthread_attr_setschedpolicy(&g_scenarii[i].ta, SCHED_FIFO);
            if (g_scenarii[i].schedpolicy == 2)
                ret = pthread_attr_setschedpolicy(&g_scenarii[i].ta, SCHED_RR);
            ICUNIT_ASSERT_EQUAL_VOID(ret, PTHREAD_NO_ERROR, ret);

            if (g_scenarii[i].schedparam != 0) {
                struct sched_param sp;

                ret = pthread_attr_getschedpolicy(&g_scenarii[i].ta, &old);
                ICUNIT_ASSERT_EQUAL_VOID(ret, PTHREAD_NO_ERROR, ret);

                if (g_scenarii[i].schedparam == 1)
                    sp.sched_priority = sched_get_priority_min(old);
                if (g_scenarii[i].schedparam == -1)
                    sp.sched_priority = sched_get_priority_max(old);

                ret = pthread_attr_setschedparam(&g_scenarii[i].ta, &sp);
                ICUNIT_ASSERT_EQUAL_VOID(ret, PTHREAD_NO_ERROR, ret);
            }

            if (tps > 0) {
                ret = pthread_attr_getscope(&g_scenarii[i].ta, &old);
                ICUNIT_ASSERT_EQUAL_VOID(ret, PTHREAD_NO_ERROR, ret);

                if (g_scenarii[i].altscope != 0) {
                    if (old == PTHREAD_SCOPE_PROCESS)
                        old = PTHREAD_SCOPE_SYSTEM;
                    else
                        old = PTHREAD_SCOPE_PROCESS;

                    ret = pthread_attr_setscope(&g_scenarii[i].ta, old);
                }
            }

            if ((tss > 0) && (tsa > 0)) {
                if (g_scenarii[i].altstack != 0) {
                    g_scenarii[i].bottom = malloc(minstacksize + pagesize);
                    ICUNIT_TRACK_NOT_EQUAL(g_scenarii[i].bottom, NULL, g_scenarii[i].bottom);
                }
            }


            if (tss > 0) {
                if (g_scenarii[i].altsize != 0) {
                    ret = pthread_attr_setstacksize(&g_scenarii[i].ta, minstacksize);
                    ICUNIT_ASSERT_EQUAL_VOID(ret, PTHREAD_NO_ERROR, ret);
                }
            }

            ret = sem_init(&g_scenarii[i].sem, 0, 0);
            ICUNIT_ASSERT_EQUAL_VOID(ret, PTHREAD_NO_ERROR, ret);
        }
    }
}
/*
 * This function will free all resources consumed
 * in the scenar_init() routine
 */
VOID ScenarFini(VOID)
{
    INT32 ret = 0;
    UINT32 i;

    for (i = 0; i < NSCENAR; i++) {
        if (g_scenarii[i].bottom != NULL)
            free(g_scenarii[i].bottom);

        ret = sem_destroy(&g_scenarii[i].sem);
        ICUNIT_ASSERT_EQUAL_VOID(ret, PTHREAD_NO_ERROR, ret);

        ret = pthread_attr_destroy(&g_scenarii[i].ta);
        ICUNIT_ASSERT_EQUAL_VOID(ret, PTHREAD_NO_ERROR, ret);
    }
}

/*
 * return value of pthread_self() is 0 when
 * pthread create from LOS_TaskCreate()
 */
pthread_t TestPthreadSelf(void)
{
    pthread_t tid = pthread_self();
    return tid;
}
UINT32 TaskCountGetTest(VOID)
{
    // not implemented
    return 0;
}
using namespace testing::ext;
namespace OHOS {
class PosixPthreadTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
};

#if defined(LOSCFG_USER_TEST_SMOKE)
/**
 * @tc.name: IT_POSIX_PTHREAD_003
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread003, TestSize.Level0)
{
    ItPosixPthread003();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_004
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread004, TestSize.Level0)
{
    ItPosixPthread004();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_005
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread005, TestSize.Level0)
{
    ItPosixPthread005(); // pthread_cancel
}

/**
 * @tc.name: IT_POSIX_PTHREAD_006
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread006, TestSize.Level0)
{
    ItPosixPthread006();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_018
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread018, TestSize.Level0)
{
    ItPosixPthread018();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_019
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread019, TestSize.Level0)
{
    ItPosixPthread019();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_020
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread020, TestSize.Level0)
{
    ItPosixPthread020(); // pthread_key_delete
}

/**
 * @tc.name: IT_POSIX_PTHREAD_021
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread021, TestSize.Level0)
{
    ItPosixPthread021();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_022
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread022, TestSize.Level0)
{
    ItPosixPthread022(); // pthread_cancel
}

#endif

#if defined(LOSCFG_USER_TEST_FULL)
/**
 * @tc.name: IT_POSIX_PTHREAD_001
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread001, TestSize.Level0)
{
    ItPosixPthread001();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_002
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread002, TestSize.Level0)
{
    ItPosixPthread002();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_007
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread007, TestSize.Level0)
{
    ItPosixPthread007();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_010
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread010, TestSize.Level0)
{
    ItPosixPthread010();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_011
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread011, TestSize.Level0)
{
    ItPosixPthread011();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_013
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread013, TestSize.Level0)
{
    ItPosixPthread013();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_023
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread023, TestSize.Level0)
{
    ItPosixPthread023();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_025
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread025, TestSize.Level0)
{
    ItPosixPthread025();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_026
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread026, TestSize.Level0)
{
    ItPosixPthread026();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_027
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread027, TestSize.Level0)
{
    ItPosixPthread027();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_028
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread028, TestSize.Level0)
{
    ItPosixPthread028();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_029
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread029, TestSize.Level0)
{
    ItPosixPthread029();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_030
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread030, TestSize.Level0)
{
    ItPosixPthread030(); // pthread_cancel
}

/**
 * @tc.name: IT_POSIX_PTHREAD_031
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread031, TestSize.Level0)
{
    ItPosixPthread031();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_034
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread034, TestSize.Level0)
{
    ItPosixPthread034();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_035
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread035, TestSize.Level0)
{
    ItPosixPthread035();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_039
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread039, TestSize.Level0)
{
    ItPosixPthread039(); // pthread_cancel
}

/**
 * @tc.name: IT_POSIX_PTHREAD_040
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread040, TestSize.Level0)
{
    ItPosixPthread040();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_042
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread042, TestSize.Level0)
{
    ItPosixPthread042();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_044
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread044, TestSize.Level0)
{
    ItPosixPthread044();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_045
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread045, TestSize.Level0)
{
    ItPosixPthread045();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_046
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread046, TestSize.Level0)
{
    ItPosixPthread046();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_051
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread051, TestSize.Level0)
{
    ItPosixPthread051(); // pthread_cancel
}

/**
 * @tc.name: IT_POSIX_PTHREAD_052
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread052, TestSize.Level0)
{
    ItPosixPthread052(); // pthread_key_delete
}

/**
 * @tc.name: IT_POSIX_PTHREAD_053
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread053, TestSize.Level0)
{
    ItPosixPthread053(); // pthread_key_delete
}

/**
 * @tc.name: IT_POSIX_PTHREAD_054
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread054, TestSize.Level0)
{
    ItPosixPthread054(); // pthread_key_delete
}

/**
 * @tc.name: IT_POSIX_PTHREAD_055
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread055, TestSize.Level0)
{
    ItPosixPthread055(); // pthread_key_delete and pthread_cancel
}

/**
 * @tc.name: IT_POSIX_PTHREAD_057
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread057, TestSize.Level0)
{
    ItPosixPthread057(); // pthread_cancel
}

/**
 * @tc.name: IT_POSIX_PTHREAD_059
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread059, TestSize.Level0)
{
    ItPosixPthread059(); // pthread_cancel
}

/**
 * @tc.name: IT_POSIX_PTHREAD_060
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread060, TestSize.Level0)
{
    ItPosixPthread060(); // pthread_cancel
}

/**
 * @tc.name: IT_POSIX_PTHREAD_069
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread069, TestSize.Level0)
{
    ItPosixPthread069();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_070
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread070, TestSize.Level0)
{
    ItPosixPthread070(); // bug:pthread_cond_init
}

/**
 * @tc.name: IT_POSIX_PTHREAD_071
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread071, TestSize.Level0)
{
    ItPosixPthread071();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_072
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread072, TestSize.Level0)
{
    ItPosixPthread072(); // pthread_cond_destroy doesn't fully support
}

/**
 * @tc.name: IT_POSIX_PTHREAD_073
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread073, TestSize.Level0)
{
    ItPosixPthread073();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_074
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread074, TestSize.Level0)
{
    ItPosixPthread074();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_078
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread078, TestSize.Level0)
{
    ItPosixPthread078();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_079
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread079, TestSize.Level0)
{
    ItPosixPthread079();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_080
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread080, TestSize.Level0)
{
    ItPosixPthread080(); // pthread_cond
}

/**
 * @tc.name: IT_POSIX_PTHREAD_081
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread081, TestSize.Level0)
{
    ItPosixPthread081(); // pthread_cond
}

/**
 * @tc.name: IT_POSIX_PTHREAD_082
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread082, TestSize.Level0)
{
    ItPosixPthread082(); // pthread_cond
}

/**
 * @tc.name: IT_POSIX_PTHREAD_083
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread083, TestSize.Level0)
{
    ItPosixPthread083(); // pthread_cond
}

/**
 * @tc.name: IT_POSIX_PTHREAD_084
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread084, TestSize.Level0)
{
    ItPosixPthread084(); // pthread_cond
}

/**
 * @tc.name: IT_POSIX_PTHREAD_085
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread085, TestSize.Level0)
{
    ItPosixPthread085(); // pthread_cond
}

/**
 * @tc.name: IT_POSIX_PTHREAD_087
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread087, TestSize.Level0)
{
    ItPosixPthread087(); // pthread_cond
}

/**
 * @tc.name: IT_POSIX_PTHREAD_088
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread088, TestSize.Level0)
{
    ItPosixPthread088(); // pthread_cond
}

/**
 * @tc.name: IT_POSIX_PTHREAD_089
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread089, TestSize.Level0)
{
    ItPosixPthread089(); // pthread_cond
}

/**
 * @tc.name: IT_POSIX_PTHREAD_090
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread090, TestSize.Level0)
{
    ItPosixPthread090(); // pthread_cond
}

/**
 * @tc.name: IT_POSIX_PTHREAD_092
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread092, TestSize.Level0)
{
    ItPosixPthread092(); // pthread_cond
}

/**
 * @tc.name: IT_POSIX_PTHREAD_091
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread091, TestSize.Level0)
{
    ItPosixPthread091(); // pthread_cancel
}

#ifndef LOSCFG_USER_TEST_SMP
/**
 * @tc.name: IT_POSIX_PTHREAD_094
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread094, TestSize.Level0)
{
    ItPosixPthread094(); // pthread_cancel
}
#endif

/**
 * @tc.name: IT_POSIX_PTHREAD_095
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread095, TestSize.Level0)
{
    ItPosixPthread095(); // pthread_cancel
}

/**
 * @tc.name: IT_POSIX_PTHREAD_106
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread106, TestSize.Level0)
{
    ItPosixPthread106(); // pthread_cancel
}

/**
 * @tc.name: IT_POSIX_PTHREAD_107
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread107, TestSize.Level0)
{
    ItPosixPthread107();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_116
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread116, TestSize.Level0)
{
    ItPosixPthread116();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_123
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread123, TestSize.Level0)
{
    ItPosixPthread123();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_124
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread124, TestSize.Level0)
{
    ItPosixPthread124();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_125
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread125, TestSize.Level0)
{
    ItPosixPthread125();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_127
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread127, TestSize.Level0)
{
    ItPosixPthread127();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_129
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread129, TestSize.Level0)
{
    ItPosixPthread129();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_132
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread132, TestSize.Level0)
{
    ItPosixPthread132();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_133
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread133, TestSize.Level0)
{
    ItPosixPthread133();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_134
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread134, TestSize.Level0)
{
    ItPosixPthread134();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_136
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread136, TestSize.Level0)
{
    ItPosixPthread136();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_138
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread138, TestSize.Level0)
{
    ItPosixPthread138();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_141
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread141, TestSize.Level0)
{
    ItPosixPthread141();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_142
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread142, TestSize.Level0)
{
    ItPosixPthread142();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_144
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread144, TestSize.Level0)
{
    ItPosixPthread144(); // pthread_cancel
}

/**
 * @tc.name: IT_POSIX_PTHREAD_152
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread152, TestSize.Level0)
{
    ItPosixPthread152();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_154
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread154, TestSize.Level0)
{
    ItPosixPthread154();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_166
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread166, TestSize.Level0)
{
    ItPosixPthread166();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_167
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread167, TestSize.Level0)
{
    ItPosixPthread167();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_173
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread173, TestSize.Level0)
{
    ItPosixPthread173();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_175
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread175, TestSize.Level0)
{
    ItPosixPthread175();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_176
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread176, TestSize.Level0)
{
    ItPosixPthread176();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_177
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread177, TestSize.Level0)
{
    ItPosixPthread177();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_182
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread182, TestSize.Level0)
{
    ItPosixPthread182();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_185
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread185, TestSize.Level0)
{
    ItPosixPthread185();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_186
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread186, TestSize.Level0)
{
    ItPosixPthread186();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_187
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread187, TestSize.Level0)
{
    ItPosixPthread187();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_188
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread188, TestSize.Level0)
{
    ItPosixPthread188();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_193
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread193, TestSize.Level0)
{
    ItPosixPthread193();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_194
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread194, TestSize.Level0)
{
    ItPosixPthread194();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_200
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread200, TestSize.Level0)
{
    ItPosixPthread200();
}

/* *
 * @tc.name: IT_POSIX_PTHREAD_203
 * @tc.desc: function for pthread concurrency
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread203, TestSize.Level0)
{
    ItPosixPthread203();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_204
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread204, TestSize.Level0)
{
    ItPosixPthread204();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_205
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread205, TestSize.Level0)
{
    ItPosixPthread205();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_206
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread206, TestSize.Level0)
{
    ItPosixPthread206();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_209
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread209, TestSize.Level0)
{
    ItPosixPthread209();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_213
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread213, TestSize.Level0)
{
    ItPosixPthread213();
}

/**
 * @tc.name: IT_POSIX_PTHREAD_217
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread217, TestSize.Level0)
{
    ItPosixPthread217(); // phthread_key_create
}

/**
 * @tc.name: IT_POSIX_PTHREAD_218
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread218, TestSize.Level0)
{
    ItPosixPthread218(); // phthread_key_create
}

/**
 * @tc.name: IT_POSIX_PTHREAD_224
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread224, TestSize.Level0)
{
    ItPosixPthread224(); // pthread_key_create
}

/**
 * @tc.name: IT_POSIX_PTHREAD_226
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread226, TestSize.Level0)
{
    ItPosixPthread226(); // pthread_key
}

/**
 * @tc.name: IT_POSIX_PTHREAD_233
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread233, TestSize.Level0)
{
    ItPosixPthread233(); // pthread_key and pthread_cancel
}

/**
 * @tc.name: IT_POSIX_PTHREAD_238
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread238, TestSize.Level0)
{
    ItPosixPthread238(); // pthread_cancel
}

/**
 * @tc.name: IT_POSIX_PTHREAD_239
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread239, TestSize.Level0)
{
    ItPosixPthread239(); // pthread_cancel
}

/**
 * @tc.name: IT_POSIX_PTHREAD_240
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread240, TestSize.Level0)
{
    ItPosixPthread240(); // pthread_cancel
}

/**
 * @tc.name: IT_POSIX_PTHREAD_241
 * @tc.desc: function for PosixPthreadTest
 * @tc.type: FUNC
 */
HWTEST_F(PosixPthreadTest, ItPosixPthread241, TestSize.Level0)
{
    ItPosixPthread241(); // pthread_cancel
}
#endif

} // namespace OHOS
