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

#include "errno.h"
#include "los_errno.h"
#include "los_task_pri.h"

/* the specific errno get or set in interrupt service routine */
/**
 * @brief   ISR(中断服务程序)环境下的全局错误码变量
 * @note    在中断上下文中使用，避免与任务上下文的错误码冲突
 */
static int errno_isr;

/**
 * @brief   获取当前上下文的错误码指针
 * @return  指向当前上下文错误码的指针
 * @note    根据当前是否在中断环境中返回不同的错误码存储位置
 */
int *__errno_location(void)
{
    LosTaskCB *runTask = NULL;  // 当前运行任务的控制块指针

    // 判断当前是否处于非中断状态(任务上下文)
    if (OS_INT_INACTIVE) {
        runTask = OsCurrTaskGet();  // 获取当前运行的任务控制块
        return &runTask->errorNo;   // 返回任务控制块中的错误码地址
    } else {
        return &errno_isr;          // 返回中断环境下的全局错误码地址
    }
}

/**
 * @brief   __errno_location函数的弱别名
 * @note    提供与POSIX标准兼容的错误码访问接口，实际调用__errno_location
 */
int *__errno(void) __attribute__((__weak__, __alias__("__errno_location")));
