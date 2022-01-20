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
#ifndef IT_POSIX_QUEUE_H
#define IT_POSIX_QUEUE_H

#include <stdlib.h>
#include <mqueue.h>
#include <los_typedef.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <osTest.h>
#include <sys/utsname.h>

#define MAXMSG5 5
#define MSGLEN 10
#define MAXMSG 10

const int MQUEUE_SHORT_ARRAY_LENGTH  = 10; // = strlen(MQUEUE_SEND_STRING_TEST)
const int MQUEUE_STANDARD_NAME_LENGTH  = 50;

#define MQUEUE_NO_ERROR 0
#define MQUEUE_IS_ERROR (-1)
#define MQUEUE_PTHREAD_PRIORITY_TEST1 3
#define MQUEUE_PTHREAD_PRIORITY_TEST2 4
#define MQUEUE_PATH_MAX_TEST PATH_MAX
#define MQUEUE_NAME_MAX_TEST NAME_MAX
#define MQUEUE_SEND_STRING_TEST "0123456789"
#define MQUEUE_PTHREAD_NUM_TEST 5
#define MQUEUE_PRIORITY_TEST 0
#define MQUEUE_TIMEOUT_TEST 7
#define MQUEUE_PRIORITY_NUM_TEST 3
#define MQUEUE_MAX_NUM_TEST (LOSCFG_BASE_IPC_QUEUE_CONFIG - QUEUE_EXISTED_NUM)
#define MQ_MAX_MSG_NUM 16
#define MQ_MAX_MSG_LEN 64
#define HWI_NUM_TEST 1
#define HWI_NUM_TEST1 2
#define LOS_WAIT_FOREVER 0XFFFFFFFF

typedef VOID (*HWI_PROC_FUNC)(VOID *pParm);

#define MQ_VALID_MAGIC 0x6db256c1
const int  LOSCFG_BASE_IPC_QUEUE_CONFIG = 1024;

#ifdef __LP64__
#define PER_ADDED_VALUE 8
#else
#define PER_ADDED_VALUE 4
#endif

typedef UINT32 TSK_HANDLE_T;
using SEM_HANDLE_T = UINT32;
extern SEM_HANDLE_T g_mqueueSem;

static TSK_HANDLE_T g_mqueueTaskPID;
extern CHAR g_gqname[MQUEUE_STANDARD_NAME_LENGTH];
extern CHAR g_mqueueName[LOSCFG_BASE_IPC_QUEUE_CONFIG + 1][MQUEUE_STANDARD_NAME_LENGTH];
extern mqd_t g_mqueueId[LOSCFG_BASE_IPC_QUEUE_CONFIG + 1];

extern CHAR *g_mqueueMsessage[MQUEUE_SHORT_ARRAY_LENGTH];
extern mqd_t g_messageQId;
extern mqd_t g_gqueue;

extern unsigned long MsecsToJiffies(const unsigned int m);

extern VOID ItSuite_Posix_Mqueue(VOID);
extern UINT32 LosCurTaskIDGet();
extern int LOS_AtomicInc(const volatile unsigned int *num);
extern int LosSemDelete(SEM_HANDLE_T num);
extern int LosSemCreate(int num, const SEM_HANDLE_T *hdl);
extern UINT32 PosixPthreadInit(pthread_attr_t *attr, int pri);
extern void LOS_TaskUnlock();
extern void LOS_TaskLock();
extern int LosSemPost(SEM_HANDLE_T);
extern int LosSemPend(SEM_HANDLE_T hdl, int num);
extern int SemPost(SEM_HANDLE_T);
extern int SemPend(SEM_HANDLE_T hdl, int num);
extern int LOS_HwiCreate(int hwiNum, int hwiPrio, int hwiMode, HWI_PROC_FUNC hwiHandler, int *irqParam);
extern UINT64 JiffiesToTick(unsigned long j);
extern int HalIrqMask(int num);
extern UINT32 PosixPthreadDestroy(pthread_attr_t *attr, pthread_t thread);

#define LOS_TaskLock()
#define LOS_TaskUnlock()
#define LOS_AtomicInc(a) (++*(a))
#define TEST_TEST_HwiDelete(ID, NULL)

#if defined(LOSCFG_USER_TEST_SMOKE)
VOID ItPosixQueue001(VOID);
VOID ItPosixQueue003(VOID);
VOID ItPosixQueue028(VOID);
VOID ItPosixQueue062(VOID);
VOID ItPosixQueue053(VOID);
VOID ItPosixQueue144(VOID);
#endif

#if defined(LOSCFG_USER_TEST_FULL)
VOID ItPosixQueue002(VOID);
VOID ItPosixQueue004(VOID);
VOID ItPosixQueue005(VOID);
VOID ItPosixQueue007(VOID);
VOID ItPosixQueue008(VOID);
VOID ItPosixQueue010(VOID);
VOID ItPosixQueue011(VOID);
VOID ItPosixQueue012(VOID);
VOID ItPosixQueue013(VOID);
VOID ItPosixQueue014(VOID);
VOID ItPosixQueue015(VOID);
VOID ItPosixQueue016(VOID);
VOID ItPosixQueue017(VOID);
VOID ItPosixQueue018(VOID);
VOID ItPosixQueue019(VOID);
VOID ItPosixQueue020(VOID);
VOID ItPosixQueue021(VOID);
VOID ItPosixQueue025(VOID);
VOID ItPosixQueue026(VOID);
VOID ItPosixQueue027(VOID);
VOID ItPosixQueue030(VOID);
VOID ItPosixQueue031(VOID);
VOID ItPosixQueue032(VOID);
VOID ItPosixQueue033(VOID);
VOID ItPosixQueue036(VOID);
VOID ItPosixQueue037(VOID);
VOID ItPosixQueue038(VOID);
VOID ItPosixQueue039(VOID);
VOID ItPosixQueue040(VOID);
VOID ItPosixQueue041(VOID);
VOID ItPosixQueue042(VOID);
VOID ItPosixQueue044(VOID);
VOID ItPosixQueue046(VOID);
VOID ItPosixQueue047(VOID);
VOID ItPosixQueue048(VOID);
VOID ItPosixQueue049(VOID);
VOID ItPosixQueue050(VOID);
VOID ItPosixQueue052(VOID);
VOID ItPosixQueue054(VOID);
VOID ItPosixQueue055(VOID);
VOID ItPosixQueue056(VOID);
VOID ItPosixQueue057(VOID);
VOID ItPosixQueue058(VOID);
VOID ItPosixQueue060(VOID);
VOID ItPosixQueue061(VOID);
VOID ItPosixQueue063(VOID);
VOID ItPosixQueue064(VOID);
VOID ItPosixQueue065(VOID);
VOID ItPosixQueue066(VOID);
VOID ItPosixQueue067(VOID);
VOID ItPosixQueue069(VOID);
VOID ItPosixQueue070(VOID);
VOID ItPosixQueue071(VOID);
VOID ItPosixQueue072(VOID);
VOID ItPosixQueue073(VOID);
VOID ItPosixQueue074(VOID);
VOID ItPosixQueue075(VOID);
VOID ItPosixQueue076(VOID);
VOID ItPosixQueue077(VOID);
VOID ItPosixQueue078(VOID);
VOID ItPosixQueue079(VOID);
VOID ItPosixQueue080(VOID);
VOID ItPosixQueue081(VOID);
VOID ItPosixQueue082(VOID);
VOID ItPosixQueue083(VOID);
VOID ItPosixQueue084(VOID);
VOID ItPosixQueue085(VOID);
VOID ItPosixQueue086(VOID);
VOID ItPosixQueue087(VOID);
VOID ItPosixQueue088(VOID);
VOID ItPosixQueue089(VOID);
VOID ItPosixQueue090(VOID);
VOID ItPosixQueue091(VOID);
VOID ItPosixQueue093(VOID);
VOID ItPosixQueue094(VOID);
VOID ItPosixQueue095(VOID);
VOID ItPosixQueue096(VOID);
VOID ItPosixQueue097(VOID);
VOID ItPosixQueue098(VOID);
VOID ItPosixQueue100(VOID);
VOID ItPosixQueue101(VOID);
VOID ItPosixQueue102(VOID);
VOID ItPosixQueue103(VOID);
VOID ItPosixQueue104(VOID);
VOID ItPosixQueue106(VOID);
VOID ItPosixQueue108(VOID);
VOID ItPosixQueue109(VOID);
VOID ItPosixQueue110(VOID);
VOID ItPosixQueue111(VOID);
VOID ItPosixQueue112(VOID);
VOID ItPosixQueue113(VOID);
VOID ItPosixQueue114(VOID);
VOID ItPosixQueue115(VOID);
VOID ItPosixQueue116(VOID);
VOID ItPosixQueue117(VOID);
VOID ItPosixQueue118(VOID);
VOID ItPosixQueue119(VOID);
VOID ItPosixQueue120(VOID);
VOID ItPosixQueue121(VOID);
VOID ItPosixQueue122(VOID);
VOID ItPosixQueue123(VOID);
VOID ItPosixQueue124(VOID);
VOID ItPosixQueue125(VOID);
VOID ItPosixQueue126(VOID);
VOID ItPosixQueue127(VOID);
VOID ItPosixQueue128(VOID);
VOID ItPosixQueue129(VOID);
VOID ItPosixQueue130(VOID);
VOID ItPosixQueue133(VOID);
VOID ItPosixQueue134(VOID);
VOID ItPosixQueue136(VOID);
VOID ItPosixQueue143(VOID);
VOID ItPosixQueue145(VOID);
VOID ItPosixQueue146(VOID);
VOID ItPosixQueue147(VOID);
VOID ItPosixQueue148(VOID);
VOID ItPosixQueue149(VOID);
VOID ItPosixQueue150(VOID);
VOID ItPosixQueue151(VOID);
VOID ItPosixQueue152(VOID);
VOID ItPosixQueue153(VOID);
VOID ItPosixQueue154(VOID);
VOID ItPosixQueue155(VOID);
VOID ItPosixQueue156(VOID);
VOID ItPosixQueue160(VOID);
VOID ItPosixQueue161(VOID);
VOID ItPosixQueue162(VOID);
VOID ItPosixQueue163(VOID);
VOID ItPosixQueue164(VOID);
VOID ItPosixQueue165(VOID);
VOID ItPosixQueue166(VOID);
VOID ItPosixQueue168(VOID);
VOID ItPosixQueue169(VOID);
VOID ItPosixQueue173(VOID);
VOID ItPosixQueue175(VOID);
VOID ItPosixQueue176(VOID);
VOID ItPosixQueue187(VOID);

VOID ItPosixQueue200(VOID);
VOID ItPosixQueue201(VOID);
VOID ItPosixQueue202(VOID);
VOID ItPosixQueue203(VOID);
VOID ItPosixQueue204(VOID);

VOID ItPosixQueue205(VOID);
VOID ItPosixQueue206(VOID);
VOID ItPosixQueue207(VOID);
VOID ItPosixQueue208(VOID);
VOID ItPosixQueue209(VOID);
#endif
#endif
