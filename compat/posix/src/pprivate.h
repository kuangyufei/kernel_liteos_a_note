/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
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

#ifndef _HWLITEOS_POSIX_PPRIVATE_H
#define _HWLITEOS_POSIX_PPRIVATE_H

#include "los_process.h"
#include "pthread.h"
#include "sys/types.h"
#include "los_sem_pri.h"
#include "los_task_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**************************************************************************
posix 线程属性结构体

	typedef struct __pthread_attr_s {
	  unsigned int detachstate;			//线程的分离状态
	  unsigned int schedpolicy;			//线程调度策略 
	  struct sched_param schedparam;	//线程的调度参数
	  unsigned int inheritsched;		//线程的继承性 
	  unsigned int scope;				//线程的作用域
	  unsigned int stackaddr_set;		//线程的栈地址设置
	  void* stackaddr;					//线程栈的位置 
	  unsigned int stacksize_set;		//线程栈的设置
	  size_t stacksize;					//线程栈的大小
	} pthread_attr_t;

***************************************************************************/

#define PTHREAD_DATA_NAME_MAX 20
/*
 * Thread control data structure			//线程控制数据结构
 * Per-thread information needed by POSIX	//POSIX所需的每个线程信息
 */
typedef struct {
    pthread_attr_t      attr; /* Current thread attributes *///当前线程属性
    pthread_t           id; /* My thread ID */				//线程PID
    LosTaskCB           *task; /* pointer to Huawei LiteOS thread object *///指向华为 轻内核线程对象实体
    CHAR                name[PTHREAD_DATA_NAME_MAX]; /* name string for debugging */ //名称用于调试
    UINT8               state; /* Thread state */			//线程的状态
    UINT8               cancelstate; /* Cancel state of thread */	//线程的取消状态
    volatile UINT8      canceltype; /* Cancel type of thread */		//线程的取消类型
    volatile UINT8      canceled; /* pending cancel flag */			//阻塞取消标识
    struct pthread_cleanup_buffer *cancelbuffer; /* stack of cleanup buffers */	//清空栈buf
    UINT32              freestack; /* stack malloced, must be freed */	//堆栈，必须释放
    UINT32              stackmem; /* base of stack memory area only valid if freestack == true *///堆栈内存区域的有效范围
    VOID                **thread_data; /* Per-thread data table pointer *///指向每个线程的数据表的指针
} _pthread_data;

/*
 * Values for the state field. These are solely concerned with the
 * states visible to POSIX. The thread's run state is stored in the
 * struct _pthread_data about thread object.
 * Note: numerical order here is important, do not rearrange.
 */
#define PTHREAD_STATE_FREE       0  /* This structure is free for reuse */		//空闲状态,可以反复使用
#define PTHREAD_STATE_DETACHED   1  /* The thread is running but detached */	//线程正在运行，但已分离
#define PTHREAD_STATE_RUNNING    2  /* The thread is running and will wait to join when it exits */	//线程正在运行，退出时将等待加入
#define PTHREAD_STATE_JOIN       3  /* The thread has exited and is waiting to be joined */ //线程已退出，正在等待加入
#define PTHREAD_STATE_EXITED     4  /* The thread has exited and is ready to be reaped */	//线已退出，准备收割
#define PTHREAD_STATE_ALRDY_JOIN 5  /* The thread state is in join */			//线程状态为联结


#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
