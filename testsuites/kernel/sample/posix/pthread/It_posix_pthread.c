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

#include "It_posix_pthread.h"

#include "It_posix_pthread.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

pthread_key_t g_key;
pthread_key_t g_pthreadKeyTest[PTHREAD_KEY_NUM];
pthread_t g_newTh;
pthread_t g_newTh2;
pthread_once_t g_onceControl = PTHREAD_ONCE_INIT;
pthread_cond_t g_pthreadCondTest1 = PTHREAD_COND_INITIALIZER;
pthread_mutex_t g_pthreadMutexTest1 = PTHREAD_MUTEX_INITIALIZER;
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

ScenarIo g_scenarii[] = {
    CASE_POS(0, 0, 0, 0, 0, 0, 0, 0, "default"),
    CASE_POS(1, 0, 0, 0, 0, 0, 0, 0, "detached"),
    CASE_POS(0, 1, 0, 0, 0, 0, 0, 0, "Explicit sched"),
    CASE_UNK(0, 0, 1, 0, 0, 0, 0, 0, "FIFO Policy"),
    CASE_UNK(0, 0, 2, 0, 0, 0, 0, 0, "RR Policy"),
    CASE_UNK(0, 0, 0, 1, 0, 0, 0, 0, "Max sched param"),
    CASE_UNK(0, 0, 0, -1, 0, 0, 0, 0, "Min sched param"),
    CASE_POS(0, 0, 0, 0, 1, 0, 0, 0, "Alternative contension scope"),
    CASE_POS(0, 0, 0, 0, 0, 1, 0, 0, "Alternative stack"),
    CASE_POS(0, 0, 0, 0, 0, 0, 1, 0, "No guard size"),
    CASE_UNK(0, 0, 0, 0, 0, 0, 2, 0, "1p guard size"),
    CASE_POS(0, 0, 0, 0, 0, 0, 0, 1, "Min stack size"),
    /* Stack play */
    CASE_POS(0, 0, 0, 0, 0, 0, 1, 1, "Min stack size, no guard"),
    CASE_UNK(0, 0, 0, 0, 0, 0, 2, 1, "Min stack size, 1p guard"),
    CASE_POS(1, 0, 0, 0, 0, 1, 0, 0, "Detached, Alternative stack"),
    CASE_POS(1, 0, 0, 0, 0, 0, 1, 1, "Detached, Min stack size, no guard"),
    CASE_UNK(1, 0, 0, 0, 0, 0, 2, 1, "Detached, Min stack size, 1p guard"),
};

pthread_t g_pthreadTestTh;

VOID ScenarInit(VOID)
{
    INT32 ret;
    UINT32 i;
    INT32 old;
    long pagesize, minstacksize;
    long tsa, tss, tps;

    pagesize = sysconf(_SC_PAGESIZE);
    minstacksize = sysconf(_SC_THREAD_STACK_MIN);
    tsa = sysconf(_SC_THREAD_ATTR_STACKADDR);
    tss = sysconf(_SC_THREAD_ATTR_STACKSIZE);
    tps = sysconf(_SC_THREAD_PRIORITY_SCHEDULING);

    if (pagesize && minstacksize % pagesize) {
        ICUNIT_ASSERT_EQUAL_VOID(1, 0, errno);
    }

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
            if (g_scenarii[i].schedpolicy == 2) // 2, set SCHED_RR as sched mode.
                ret = pthread_attr_setschedpolicy(&g_scenarii[i].ta, SCHED_RR);
            ICUNIT_ASSERT_EQUAL_VOID(ret, PTHREAD_NO_ERROR, ret);

            if (g_scenarii[i].schedparam != 0) {
                struct sched_param sp;

                ret = pthread_attr_getschedpolicy(&g_scenarii[i].ta, &old);
                ICUNIT_ASSERT_EQUAL_VOID(ret, PTHREAD_NO_ERROR, ret);

                if (g_scenarii[i].schedparam == 1)
                    sp.sched_priority = sched_get_priority_max(old);
                if (g_scenarii[i].schedparam == -1)
                    sp.sched_priority = sched_get_priority_min(old);

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
    INT32 ret;
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
    if (tid == 0) {
        tid = ((LosTaskCB *)(OsCurrTaskGet()))->taskID;
    }
    return tid;
}

VOID ItSuitePosixPthread()
{
#if defined(LOSCFG_TEST_SMOKE)
    ItPosixPthread003();
    ItPosixPthread004();
    ItPosixPthread005();
    ItPosixPthread006();
    ItPosixPthread009();
    ItPosixPthread018();
    ItPosixPthread019();
    ItPosixPthread021();
#endif

#if defined(LOSCFG_TEST_FULL)
    ItPosixPthread001();
    ItPosixPthread002();
    ItPosixPthread007();
    ItPosixPthread008();
    ItPosixPthread010();
    ItPosixPthread011();
    ItPosixPthread013();
    ItPosixPthread023();
    ItPosixPthread028();
    ItPosixPthread029();
    ItPosixPthread030();
    ItPosixPthread031();
    ItPosixPthread032();
    ItPosixPthread033();
    ItPosixPthread034();
    ItPosixPthread035();
    ItPosixPthread039();
    ItPosixPthread040();
    ItPosixPthread041();
    ItPosixPthread042();
    ItPosixPthread044();
    ItPosixPthread045();
    ItPosixPthread046();
#if (LOSCFG_KERNEL_SMP != YES)
    ItPosixPthread047(); // pthread preemption, may not happen on smp
#endif
    ItPosixPthread048();
    ItPosixPthread049();
    ItPosixPthread050();
    ItPosixPthread051();
    ItPosixPthread056();
    ItPosixPthread057();
    ItPosixPthread058();
    ItPosixPthread060();
    ItPosixPthread066();
    ItPosixPthread068();
    ItPosixPthread069();
    ItPosixPthread071();
    ItPosixPthread072();
    ItPosixPthread073();
    ItPosixPthread074();
    ItPosixPthread075();
    ItPosixPthread078();
    ItPosixPthread079();
    ItPosixPthread080();
    ItPosixPthread081();
    ItPosixPthread082();
    ItPosixPthread083();
    ItPosixPthread084();
    ItPosixPthread085();
    ItPosixPthread087();
    ItPosixPthread088();
    ItPosixPthread089();
    ItPosixPthread092();
    ItPosixPthread095();
    ItPosixPthread098();
    ItPosixPthread101();
    ItPosixPthread102();
    ItPosixPthread103();
    ItPosixPthread107();
    ItPosixPthread108();
    ItPosixPthread110();
    ItPosixPthread112();
    ItPosixPthread116();
    ItPosixPthread121();
    ItPosixPthread123();
    ItPosixPthread124();
    ItPosixPthread125();
    ItPosixPthread127();
    ItPosixPthread128();
    ItPosixPthread129();
    ItPosixPthread132();
    ItPosixPthread133();
    ItPosixPthread134();
    ItPosixPthread136();
    ItPosixPthread138();
    ItPosixPthread141();
    ItPosixPthread142();
    ItPosixPthread144();
    ItPosixPthread150();
    ItPosixPthread152();
    ItPosixPthread154();
    ItPosixPthread166();
    ItPosixPthread167();
    ItPosixPthread173();
    ItPosixPthread175();
    ItPosixPthread176();
    ItPosixPthread177();
    ItPosixPthread182();
    ItPosixPthread185();
    ItPosixPthread186();
    ItPosixPthread187();
    ItPosixPthread188();
    ItPosixPthread193();
    ItPosixPthread194();
    ItPosixPthread197();
    ItPosixPthread198();
    ItPosixPthread200();
    ItPosixPthread204();
    ItPosixPthread205();
    ItPosixPthread206();
    ItPosixPthread208();
    ItPosixPthread209();
    ItPosixPthread211();
    ItPosixPthread213();
    ItPosixPthread214();
    ItPosixPthread215();
    ItPosixPthread217();
    ItPosixPthread218();
    ItPosixPthread219();
    ItPosixPthread221();
    ItPosixPthread224();
    ItPosixPthread226();
    ItPosixPthread233();
    ItPosixPthread237();
    ItPosixPthread238();
    ItPosixPthread239();
    ItPosixPthread240();
    ItPosixPthread241();
    ItPosixPthread246();
#endif
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
