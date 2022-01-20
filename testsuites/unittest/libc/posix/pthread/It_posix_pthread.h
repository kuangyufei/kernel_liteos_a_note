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
#ifndef _IT_POSIX_PTHREAD_H
#define _IT_POSIX_PTHREAD_H

#include "sched.h"
#include "signal.h"
#include "semaphore.h"
#include "sched.h"
#include "osTest.h"
#include "pthread.h"
#include "limits.h"
#include "unistd.h"
#include "mqueue.h"
#include "signal.h"
#include "sys/time.h"

#ifndef VERBOSE
#define VERBOSE 1
#endif

/* Some routines are part of the XSI Extensions */

#define LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE (0x6000)
#define LOS_HwiCreate(ID, prio, mode, Func, arg) (-1)
#define HalIrqMask(ID)
#define TEST_TEST_HwiDelete(ID, NULL)
#define TEST_HwiTrigger(HWI_NUM_TEST)
#define LOS_TaskLock()
#define LOS_TaskUnlock()
#define LOS_MS2Tick(ms) (ms / 10)
#define OS_TASK_PRIORITY_HIGHEST 0
#define OS_TASK_PRIORITY_LOWEST 31

#define PTHREAD_NO_ERROR 0
#define PTHREAD_IS_ERROR (-1)
#define PTHREAD_SIGNAL_SUPPORT 0 /* 0 means that not support the signal */
#define PTHREAD_PRIORITY_TEST 20
#define PTHREAD_DEFAULT_STACK_SIZE (LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE)
#define PTHREAD_KEY_NUM 10
#define THREAD_NUM 3
#define PTHREAD_TIMEOUT (THREAD_NUM * 2)
#define PTHREAD_INTHREAD_TEST 0 /* Control going to or is already for Thread */
#define PTHREAD_INMAIN_TEST 1   /* Control going to or is already for Main */
#define INVALID_PSHARED_VALUE (-100)
#define NUM_OF_CONDATTR 10
#define RUNTIME 5
#define PTHREAD_THREADS_NUM 3
#define TCOUNT 5      // Number of single-threaded polling
#define COUNT_LIMIT 7 // The number of times the signal is sent
#define HIGH_PRIORITY 5
#define LOW_PRIORITY 10
#define PTHREAD_EXIT_VALUE ((void *)100) /* The return code of the thread when using pthread_exit(). */

#define PTHREAD_EXISTED_NUM TASK_EXISTED_NUM
#define PTHREAD_EXISTED_SEM_NUM SEM_EXISTED_NUM

/* We are testing conformance to IEEE Std 1003.1, 2003 Edition */
#define _POSIX_C_SOURCE 200112L

#define uart_printf_func printf

/* The value below shall be >= to the # of CPU on the test architecture */
#define NCPU (4)

#define PRIORITY_OTHER (-1)
#define PRIORITY_FIFO 20
#define PRIORITY_RR 20

#define PTHREAD_TEST_BUG printf

#define CASE(det, expl, scp, spa, sco, sta, gua, ssi, desc, res)         \
    {                                                                    \
        { 0 }, det, expl, scp, spa, sco, sta, gua, ssi, desc, NULL, res, \
        {                                                                \
            0                                                            \
        }                                                                \
    }
#define CASE_POS(det, expl, scp, spa, sco, sta, gua, ssi, desc) CASE(det, expl, scp, spa, sco, sta, gua, ssi, desc, 0)
#define CASE_NEG(det, expl, scp, spa, sco, sta, gua, ssi, desc) CASE(det, expl, scp, spa, sco, sta, gua, ssi, desc, 1)
#define CASE_UNK(det, expl, scp, spa, sco, sta, gua, ssi, desc) CASE(det, expl, scp, spa, sco, sta, gua, ssi, desc, 2)

struct params {
    INT32 policy;
    INT32 priority;
    char *policy_label;
    INT32 status;
};

typedef struct {
    /*
     * Object to hold the given configuration,
     * and which will be used to create the threads
     */
    pthread_attr_t ta;

    /* General parameters */
    /* 0 => joinable; 1 => detached */
    INT32 detached;

    /* Scheduling parameters */
    /*
     * 0 => sched policy is inherited;
     * 1 => sched policy from the attr param
     */
    INT32 explicitsched;
    /* 0 => default; 1=> SCHED_FIFO; 2=> SCHED_RR */
    INT32 schedpolicy;
    /*
     * 0 => default sched param;
     * 1 => max value for sched param;
     * -1 => min value for sched param
     */
    INT32 schedparam;
    /*
     * 0 => default contension scope;
     * 1 => alternative contension scope
     */
    INT32 altscope;

    /* Stack parameters */
    /* 0 => system manages the stack; 1 => stack is provided */
    INT32 altstack;
    /*
     * 0 => default guardsize;
     * 1=> guardsize is 0;
     * 2=> guard is 1 page
     * -- this setting only affect system stacks (not user's).
     */
    INT32 guard;
    /*
     * 0 => default stack size;
     * 1 => stack size specified (min value)
     * -- ignored when stack is provided
     */
    INT32 altsize;

    /* Additionnal information */
    /* object description */
    char *descr;
    /* Stores the stack start when an alternate stack is required */
    void *bottom;
    /*
     * This thread creation is expected to:
     * 0 => succeed; 1 => fail; 2 => unknown
     */
    INT32 result;
    /*
     * This semaphore is used to signal the end of
     * the detached threads execution
     */
    sem_t sem;
} __scenario;

#define NSCENAR 10 // (sizeof(scenarii)/sizeof(scenarii[0]))

extern __scenario g_scenarii[];

extern pthread_key_t g_key;
extern pthread_key_t g_key1;
extern pthread_key_t g_key2;
extern pthread_key_t g_pthreadKeyTest[PTHREAD_KEY_NUM];
extern pthread_t g_newTh;
extern pthread_t g_newTh2;
extern UINT32 g_taskMaxNum;
extern pthread_once_t g_onceControl;
extern pthread_cond_t g_pthreadCondTest1;
extern pthread_mutex_t g_pthreadMutexTest1;
extern pthread_mutex_t g_pthreadMutexTest2;
extern INT32 g_startNum;
extern INT32 g_wakenNum;
extern INT32 g_t1Start;
extern INT32 g_signaled;
extern INT32 g_wokenUp;
extern INT32 g_lowDone;
extern INT32 g_pthreadSem;
extern INT32 g_pthreadScopeValue;
extern INT32 g_pthreadSchedInherit;
extern INT32 g_pthreadSchedPolicy;

extern sem_t g_pthreadSem1;
extern sem_t g_pthreadSem2;

extern pthread_t g_pthreadTestTh;

extern INT32 g_iCunitErrCode;
extern INT32 g_iCunitErrLineNo;

#ifdef LOSCFG_AARCH64
#define PTHREAD_STACK_MIN_TEST (PTHREAD_STACK_MIN * 3)
#else
#define PTHREAD_STACK_MIN_TEST PTHREAD_STACK_MIN
#endif

extern pthread_t g_Test_new_th;

struct testdata {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};
extern struct testdata g_td;


extern unsigned int sleep(unsigned int seconds);
extern unsigned int alarm(unsigned int seconds);

extern int map_errno(UINT32 err);
extern long sysconf(int name);
extern void posix_signal_start(void);

VOID ScenarInit(VOID);
VOID ScenarFini(VOID);
pthread_t TestPthreadSelf(void);

extern UINT32 PosixPthreadInit(pthread_attr_t *attr, INT32 pri);

#if defined(LOSCFG_USER_TEST_SMOKE)
VOID ItPosixPthread003(VOID);
VOID ItPosixPthread004(VOID);
VOID ItPosixPthread005(VOID);
VOID ItPosixPthread006(VOID);
VOID IT_POSIX_PTHREAD_009(VOID);
VOID ItPosixPthread018(VOID);
VOID ItPosixPthread019(VOID);
VOID ItPosixPthread020(VOID);
VOID ItPosixPthread021(VOID);
VOID ItPosixPthread022(VOID);
#endif

#if defined(LOSCFG_USER_TEST_FULL)
VOID ItPosixPthread001(VOID);
VOID ItPosixPthread002(VOID);
VOID ItPosixPthread007(VOID);
VOID IT_POSIX_PTHREAD_008(VOID);
VOID ItPosixPthread010(VOID);
VOID ItPosixPthread011(VOID);
VOID ItPosixPthread013(VOID);
VOID ItPosixPthread023(VOID);
VOID ItPosixPthread025(VOID);
VOID ItPosixPthread026(VOID);
VOID ItPosixPthread027(VOID);
VOID ItPosixPthread028(VOID);
VOID ItPosixPthread029(VOID);
VOID ItPosixPthread030(VOID);
VOID ItPosixPthread031(VOID);
VOID IT_POSIX_PTHREAD_032(VOID);
VOID IT_POSIX_PTHREAD_033(VOID);
VOID ItPosixPthread034(VOID);
VOID ItPosixPthread035(VOID);
VOID ItPosixPthread039(VOID);
VOID ItPosixPthread040(VOID);
VOID IT_POSIX_PTHREAD_041(VOID);
VOID ItPosixPthread042(VOID);
VOID ItPosixPthread044(VOID);
VOID ItPosixPthread045(VOID);
VOID ItPosixPthread046(VOID);
VOID IT_POSIX_PTHREAD_047(VOID);
VOID IT_POSIX_PTHREAD_048(VOID);
VOID IT_POSIX_PTHREAD_049(VOID);
VOID IT_POSIX_PTHREAD_050(VOID);
VOID ItPosixPthread051(VOID);
VOID ItPosixPthread052(VOID);
VOID ItPosixPthread053(VOID);
VOID ItPosixPthread054(VOID);
VOID ItPosixPthread055(VOID);
VOID IT_POSIX_PTHREAD_056(VOID);
VOID ItPosixPthread057(VOID);
VOID IT_POSIX_PTHREAD_058(VOID);
VOID ItPosixPthread059(VOID);
VOID ItPosixPthread060(VOID);
VOID ItPosixPthread061(VOID);
VOID ItPosixPthread062(VOID);
VOID ItPosixPthread063(VOID);
VOID ItPosixPthread064(VOID);
VOID ItPosixPthread066(VOID);
VOID IT_POSIX_PTHREAD_068(VOID);
VOID ItPosixPthread069(VOID);
VOID ItPosixPthread070(VOID);
VOID ItPosixPthread071(VOID);
VOID ItPosixPthread072(VOID);
VOID ItPosixPthread073(VOID);
VOID ItPosixPthread074(VOID);
VOID IT_POSIX_PTHREAD_075(VOID);
VOID ItPosixPthread078(VOID);
VOID ItPosixPthread079(VOID);
VOID ItPosixPthread080(VOID);
VOID ItPosixPthread081(VOID);
VOID ItPosixPthread082(VOID);
VOID ItPosixPthread083(VOID);
VOID ItPosixPthread084(VOID);
VOID ItPosixPthread085(VOID);
VOID ItPosixPthread087(VOID);
VOID ItPosixPthread088(VOID);
VOID ItPosixPthread089(VOID);
VOID ItPosixPthread090(VOID);
VOID ItPosixPthread091(VOID);
VOID ItPosixPthread092(VOID);
VOID ItPosixPthread094(VOID);
VOID ItPosixPthread095(VOID);
VOID IT_POSIX_PTHREAD_098(VOID);
VOID IT_POSIX_PTHREAD_101(VOID);
VOID IT_POSIX_PTHREAD_102(VOID);
VOID IT_POSIX_PTHREAD_103(VOID);
VOID IT_POSIX_PTHREAD_105(VOID);
VOID ItPosixPthread106(VOID);
VOID ItPosixPthread107(VOID);
VOID IT_POSIX_PTHREAD_108(VOID);
VOID IT_POSIX_PTHREAD_110(VOID);
VOID IT_POSIX_PTHREAD_112(VOID);
VOID ItPosixPthread116(VOID);
VOID ItPosixPthread121(VOID);
VOID ItPosixPthread123(VOID);
VOID ItPosixPthread124(VOID);
VOID ItPosixPthread125(VOID);
VOID ItPosixPthread127(VOID);
VOID IT_POSIX_PTHREAD_128(VOID);
VOID ItPosixPthread129(VOID);
VOID ItPosixPthread132(VOID);
VOID ItPosixPthread133(VOID);
VOID ItPosixPthread134(VOID);
VOID ItPosixPthread136(VOID);
VOID ItPosixPthread138(VOID);
VOID ItPosixPthread141(VOID);
VOID ItPosixPthread142(VOID);
VOID ItPosixPthread144(VOID);
VOID IT_POSIX_PTHREAD_150(VOID);
VOID ItPosixPthread152(VOID);
VOID ItPosixPthread154(VOID);
VOID ItPosixPthread166(VOID);
VOID ItPosixPthread167(VOID);
VOID ItPosixPthread173(VOID);
VOID ItPosixPthread175(VOID);
VOID ItPosixPthread176(VOID);
VOID ItPosixPthread177(VOID);
VOID ItPosixPthread182(VOID);
VOID ItPosixPthread185(VOID);
VOID ItPosixPthread186(VOID);
VOID ItPosixPthread187(VOID);
VOID ItPosixPthread188(VOID);
VOID ItPosixPthread193(VOID);
VOID ItPosixPthread194(VOID);
VOID IT_POSIX_PTHREAD_197(VOID);
VOID IT_POSIX_PTHREAD_198(VOID);
VOID ItPosixPthread200(VOID);
VOID ItPosixPthread203(VOID);
VOID ItPosixPthread204(VOID);
VOID ItPosixPthread205(VOID);
VOID ItPosixPthread206(VOID);
VOID IT_POSIX_PTHREAD_208(VOID);
VOID ItPosixPthread209(VOID);
VOID IT_POSIX_PTHREAD_211(VOID);
VOID ItPosixPthread213(VOID);
VOID IT_POSIX_PTHREAD_214(VOID);
VOID IT_POSIX_PTHREAD_215(VOID);
VOID ItPosixPthread217(VOID);
VOID ItPosixPthread218(VOID);
VOID ItPosixPthread219(VOID);
VOID ItPosixPthread221(VOID);
VOID ItPosixPthread224(VOID);
VOID ItPosixPthread226(VOID);
VOID ItPosixPthread233(VOID);
VOID IT_POSIX_PTHREAD_237(VOID);
VOID ItPosixPthread238(VOID);
VOID ItPosixPthread239(VOID);
VOID ItPosixPthread240(VOID);
VOID ItPosixPthread241(VOID);
VOID IT_POSIX_PTHREAD_246(VOID);
#endif

#if defined(LOSCFG_USER_TEST_PRESSURE)
VOID ItPosixPthread065(VOID);
#endif
#if defined(LOSCFG_USER_TEST_LLT)
VOID LltPosixPthread001(VOID);
VOID LltPosixPthread002(VOID);
VOID LltPosixPthread003(VOID);
VOID LltPosixPthread004(VOID);
VOID ItPosixPthread012(VOID);
VOID ItPosixPthread014(VOID);
VOID ItPosixPthread015(VOID);
VOID ItPosixPthread016(VOID);
VOID ItPosixPthread017(VOID);
VOID ItPosixPthread024(VOID);
VOID ItPosixPthread036(VOID);
VOID ItPosixPthread037(VOID);
VOID ItPosixPthread038(VOID);
VOID ItPosixPthread043(VOID);
VOID ItPosixPthread067(VOID);
VOID ItPosixPthread076(VOID);
VOID ItPosixPthread077(VOID);
VOID ItPosixPthread086(VOID);
VOID ItPosixPthread090(VOID);
VOID ItPosixPthread093(VOID);
VOID ItPosixPthread096(VOID);
VOID ItPosixPthread097(VOID);
VOID ItPosixPthread099(VOID);
VOID ItPosixPthread100(VOID);
VOID ItPosixPthread104(VOID);
VOID ItPosixPthread109(VOID);
VOID ItPosixPthread111(VOID);
VOID ItPosixPthread113(VOID);
VOID ItPosixPthread114(VOID);
VOID ItPosixPthread115(VOID);
VOID ItPosixPthread169(VOID);
VOID ItPosixPthread170(VOID);
VOID ItPosixPthread172(VOID);
VOID ItPosixPthread179(VOID);
VOID ItPosixPthread180(VOID);
VOID ItPosixPthread181(VOID);
VOID ItPosixPthread184(VOID);
VOID ItPosixPthread190(VOID);
VOID ItPosixPthread191(VOID);
VOID ItPosixPthread199(VOID);
VOID ItPosixPthread201(VOID);
VOID ItPosixPthread202(VOID);
#endif

#endif /* _IT_POSIX_PTHREAD_H */
