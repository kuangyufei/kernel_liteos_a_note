/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
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

#ifndef _LOS_SWTMR_PRI_H
#define _LOS_SWTMR_PRI_H

#include "los_swtmr.h"
#include "los_spinlock.h"

#ifdef LOSCFG_SECURITY_VID
#include "vid_api.h"
#else
#define MAX_INVALID_TIMER_VID OS_SWTMR_MAX_TIMERID //最大支持的软件定时器数量 <65535
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_swtmr_pri
 * Software timer state
 */
enum SwtmrState {	//定时器状态
    OS_SWTMR_STATUS_UNUSED,     /**< The software timer is not used.    */	//定时器未使用,系统在定时器模块初始化时，会将系统中所有定时器资源初始化成该状态。
    OS_SWTMR_STATUS_CREATED,    /**< The software timer is created.     */	//定时器创建后未启动，或已停止.定时器创建后，不处于计数状态时，定时器将变成该状态。
    OS_SWTMR_STATUS_TICKING     /**< The software timer is timing.      */	//定时器处于计数状态,在定时器创建后调用LOS_SwtmrStart接口启动，定时器将变成该状态，是定时器运行时的状态。
};

/**
 * @ingroup los_swtmr_pri
 * Structure of the callback function that handles software timer timeout
 */
typedef struct {//处理软件定时器超时的回调函数的结构体
    SWTMR_PROC_FUNC handler;    /**< Callback function that handles software timer timeout  */	//处理软件定时器超时的回调函数
    UINTPTR arg;                /**< Parameter passed in when the callback function
                                     that handles software timer timeout is called */	//调用处理软件计时器超时的回调函数时传入的参数
} SwtmrHandlerItem;

/**
 * @ingroup los_swtmr_pri
 * Type of the pointer to the structure of the callback function that handles software timer timeout
 */	//指向处理软件计时器超时的回调函数结构的指针的类型
typedef SwtmrHandlerItem *SwtmrHandlerItemPtr;

extern SWTMR_CTRL_S *g_swtmrCBArray;//软件定时器数组,后续统一注解为定时器池

extern SortLinkAttribute g_swtmrSortLink; /* The software timer count list */	//软件计时器计数链表	
//通过参数ID找到对应定时器描述体
#define OS_SWT_FROM_SID(swtmrID) ((SWTMR_CTRL_S *)g_swtmrCBArray + ((swtmrID) % LOSCFG_BASE_CORE_SWTMR_LIMIT))

/**
 * @ingroup los_swtmr_pri
 * @brief Scan a software timer.
 *
 * @par Description:
 * <ul>
 * <li>This API is used to scan a software timer when a Tick interrupt occurs and determine whether
 * the software timer expires.</li>
 * </ul>
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @param  None.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_swtmr_pri.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_SwtmrStop
 */
extern VOID OsSwtmrScan(VOID);
extern UINT32 OsSwtmrInit(VOID);
extern VOID OsSwtmrTask(VOID);
extern VOID OsSwtmrRecycle(UINT32 processID);
extern SPIN_LOCK_S g_swtmrSpin;
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_SWTMR_PRI_H */
