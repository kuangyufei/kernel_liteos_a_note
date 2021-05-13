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

#ifndef IT_POSIX_PTHREAD_H
#define IT_POSIX_PTHREAD_H

#include "sched.h"
#include "signal.h"
#include "semaphore.h"
#include "sched.h"
#include "osTest.h"
#include "pthread.h"
#include "pprivate.h"
#include "limits.h"
#include "unistd.h"
#include "mqueue.h"
#include "signal.h"

#ifndef VERBOSE
#define VERBOSE 1
#endif

/* Some routines are part of the XSI Extensions */
#ifndef WITHOUT_XOPEN
#define _XOPEN_SOURCE 600
#endif

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

#define PTHREAD_MUTEX_RECURSIVE 0
#define PTHREAD_MUTEX_ERRORCHECK 0

#define uart_printf_func dprintf

#define PRIORITY_OTHER (-1)
#define PRIORITY_FIFO 20
#define PRIORITY_RR 20

#define PTHREAD_TEST_BUG dprintf

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
    /* 0, joinable; 1, detached */
    INT32 detached;

    /* Scheduling parameters */
    /*
     * 0, sched policy is inherited;
     * 1, sched policy from the attr param
     */
    INT32 explicitsched;
    /* 0, default; 1, SCHED_FIFO; 2, SCHED_RR */
    INT32 schedpolicy;
    /*
     * 0, default sched param;
     * 1, max value for sched param;
     * -1, min value for sched param
     */
    INT32 schedparam;
    /*
     * 0, default contension scope;
     * 1, alternative contension scope
     */
    INT32 altscope;

    /* Stack parameters */
    /* 0, system manages the stack; 1, stack is provided */
    INT32 altstack;
    /*
     * 0, default guardsize;
     * 1, guardsize is 0;
     * 2, guard is 1 page
     * -- this setting only affect system stacks (not user's).
     */
    INT32 guard;
    /*
     * 0, default stack size;
     * 1, stack size specified (min value)
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
     * 0, succeed; 1, fail; 2, unknown
     */
    INT32 result;
    /*
     * This semaphore is used to signal the end of
     * the detached threads execution
     */
    sem_t sem;
} ScenarIo;

#define NSCENAR 10 // (sizeof(g_scenarii)/sizeof(g_scenarii[0]))

extern ScenarIo g_scenarii[];

extern _pthread_data *pthread_get_self_data(void);
extern _pthread_data *pthread_get_data(pthread_t id);
extern pthread_key_t g_key;
extern pthread_key_t g_pthreadKeyTest[PTHREAD_KEY_NUM];
extern pthread_t g_newTh;
extern pthread_t g_newTh2;
extern UINT32 g_taskMaxNum;
extern pthread_once_t g_onceControl;
extern pthread_cond_t g_pthreadCondTest1;
extern pthread_mutex_t g_pthreadMutexTest1;
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

extern pthread_t g_pthreadTestTh;

#ifdef LOSCFG_AARCH64
#define PTHREAD_STACK_MIN_TEST (PTHREAD_STACK_MIN * 3)
#else
#define PTHREAD_STACK_MIN_TEST PTHREAD_STACK_MIN
#endif

static pthread_t g_testNewTh;

static struct testdata {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} g_td;

extern unsigned int sleep(unsigned int seconds);
extern unsigned int alarm(unsigned int seconds);

extern long sysconf(int name);

VOID ScenarInit(VOID);
VOID ScenarFini(VOID);
pthread_t TestPthreadSelf(void);

extern UINT32 PosixPthreadInit(pthread_attr_t *attr, INT32 pri);

#if defined(LOSCFG_TEST_SMOKE)
VOID ItPosixPthread003(VOID);
VOID ItPosixPthread004(VOID);
VOID ItPosixPthread005(VOID);
VOID ItPosixPthread006(VOID);
VOID ItPosixPthread009(VOID);
VOID ItPosixPthread018(VOID);
VOID ItPosixPthread019(VOID);
VOID ItPosixPthread021(VOID);
#endif

#if defined(LOSCFG_TEST_FULL)
VOID ItPosixPthread001(VOID);
VOID ItPosixPthread002(VOID);
VOID ItPosixPthread007(VOID);
VOID ItPosixPthread008(VOID);
VOID ItPosixPthread010(VOID);
VOID ItPosixPthread011(VOID);
VOID ItPosixPthread013(VOID);
VOID ItPosixPthread023(VOID);
VOID ItPosixPthread028(VOID);
VOID ItPosixPthread029(VOID);
VOID ItPosixPthread030(VOID);
VOID ItPosixPthread031(VOID);
VOID ItPosixPthread032(VOID);
VOID ItPosixPthread033(VOID);
VOID ItPosixPthread034(VOID);
VOID ItPosixPthread035(VOID);
VOID ItPosixPthread039(VOID);
VOID ItPosixPthread040(VOID);
VOID ItPosixPthread041(VOID);
VOID ItPosixPthread042(VOID);
VOID ItPosixPthread044(VOID);
VOID ItPosixPthread045(VOID);
VOID ItPosixPthread046(VOID);
VOID ItPosixPthread048(VOID);
VOID ItPosixPthread049(VOID);
VOID ItPosixPthread050(VOID);
VOID ItPosixPthread051(VOID);
VOID ItPosixPthread056(VOID);
VOID ItPosixPthread057(VOID);
VOID ItPosixPthread058(VOID);
VOID ItPosixPthread060(VOID);
VOID ItPosixPthread066(VOID);
VOID ItPosixPthread068(VOID);
VOID ItPosixPthread069(VOID);
VOID ItPosixPthread071(VOID);
VOID ItPosixPthread072(VOID);
VOID ItPosixPthread073(VOID);
VOID ItPosixPthread074(VOID);
VOID ItPosixPthread075(VOID);
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
VOID ItPosixPthread092(VOID);
VOID ItPosixPthread095(VOID);
VOID ItPosixPthread098(VOID);
VOID ItPosixPthread101(VOID);
VOID ItPosixPthread102(VOID);
VOID ItPosixPthread103(VOID);
VOID ItPosixPthread107(VOID);
VOID ItPosixPthread108(VOID);
VOID ItPosixPthread110(VOID);
VOID ItPosixPthread112(VOID);
VOID ItPosixPthread116(VOID);
VOID ItPosixPthread121(VOID);
VOID ItPosixPthread123(VOID);
VOID ItPosixPthread124(VOID);
VOID ItPosixPthread125(VOID);
VOID ItPosixPthread127(VOID);
VOID ItPosixPthread128(VOID);
VOID ItPosixPthread129(VOID);
VOID ItPosixPthread132(VOID);
VOID ItPosixPthread133(VOID);
VOID ItPosixPthread134(VOID);
VOID ItPosixPthread136(VOID);
VOID ItPosixPthread138(VOID);
VOID ItPosixPthread141(VOID);
VOID ItPosixPthread142(VOID);
VOID ItPosixPthread144(VOID);
VOID ItPosixPthread150(VOID);
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
VOID ItPosixPthread197(VOID);
VOID ItPosixPthread198(VOID);
VOID ItPosixPthread200(VOID);
VOID ItPosixPthread204(VOID);
VOID ItPosixPthread205(VOID);
VOID ItPosixPthread206(VOID);
VOID ItPosixPthread208(VOID);
VOID ItPosixPthread209(VOID);
VOID ItPosixPthread211(VOID);
VOID ItPosixPthread213(VOID);
VOID ItPosixPthread214(VOID);
VOID ItPosixPthread215(VOID);
VOID ItPosixPthread217(VOID);
VOID ItPosixPthread218(VOID);
VOID ItPosixPthread219(VOID);
VOID ItPosixPthread221(VOID);
VOID ItPosixPthread224(VOID);
VOID ItPosixPthread226(VOID);
VOID ItPosixPthread233(VOID);
VOID ItPosixPthread237(VOID);
VOID ItPosixPthread238(VOID);
VOID ItPosixPthread239(VOID);
VOID ItPosixPthread240(VOID);
VOID ItPosixPthread241(VOID);
VOID ItPosixPthread246(VOID);
#endif

#endif /* IT_POSIX_PTHREAD_H */
