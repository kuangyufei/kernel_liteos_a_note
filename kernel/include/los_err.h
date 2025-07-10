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

/**
 * @defgroup los_err Error handling
 * @ingroup kernel
 */

#ifndef _LOS_ERR_H
#define _LOS_ERR_H

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_err
 * @brief Define the pointer to the error handling function.
 *
 * @par Description:
 * This API is used to define the pointer to the error handling function.
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @param  fileName  [IN] Log file that stores error information.
 * @param  lineNo    [IN] Line number of the erroneous line.
 * @param  errorNo   [IN] Error code.
 * @param  paraLen   [IN] Length of the input parameter pPara.
 * @param  para       [IN] User label of the error.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_err.h: the header file that contains the API declaration.</li></ul>
 * @see None.
 */
typedef VOID (*LOS_ERRORHANDLE_FUNC)(CHAR *fileName,
                                     UINT32 lineNo,
                                     UINT32 errorNo,
                                     UINT32 paraLen,
                                     VOID *para);

/**
 * @ingroup los_err
 * @brief Error handling function.
 *
 * @par Description:
 * This API is used to perform different operations according to error types.
 * @attention
 * <ul>
 * <li>None</li>
 * </ul>
 *
 * @param  fileName  [IN] Log file that stores error information.
 * @param  lineNo    [IN] Line number of the erroneous line which should not be OS_ERR_MAGIC_WORD.
 * @param  errorNo   [IN] Error code.
 * @param  paraLen   [IN] Length of the input parameter pPara.
 * @param  para      [IN] User label of the error.
 *
 * @retval LOS_OK The error is successfully processed.
 * @par Dependency:
 * <ul><li>los_err.h: the header file that contains the API declaration.</li></ul>
 * @see None
 */
extern UINT32 LOS_ErrHandle(CHAR *fileName, UINT32 lineNo,
                            UINT32 errorNo, UINT32 paraLen,
                            VOID *para);

/**
 * @ingroup los_err
 * @brief set Error handling function.
 *
 * @param  fun  [IN] the error handle function.
 */
extern VOID LOS_SetErrHandleHook(LOS_ERRORHANDLE_FUNC fun);

/**
 * @ingroup los_err
 * LiteOS内核模块ID枚举定义
 * 用于错误码分类和模块标识，每个模块分配唯一ID以便定位错误来源
 * 注意：枚举名存在拼写错误，正确应为 LOS_MODULE_ID @note_thinking
 */
enum LOS_MOUDLE_ID {
    LOS_MOD_SYS = 0x0,        /**< 系统模块，值为0x0（十进制0） */
    LOS_MOD_MEM = 0x1,        /**< 内存管理模块，值为0x1（十进制1） */
    LOS_MOD_TSK = 0x2,        /**< 任务管理模块，值为0x2（十进制2） */
    LOS_MOD_SWTMR = 0x3,      /**< 软件定时器模块，值为0x3（十进制3） */
    LOS_MOD_TICK = 0x4,       /**< 系统滴答模块，值为0x4（十进制4） */
    LOS_MOD_MSG = 0x5,        /**< 消息队列模块，值为0x5（十进制5） */
    LOS_MOD_QUE = 0x6,        /**< 队列管理模块，值为0x6（十进制6） */
    LOS_MOD_SEM = 0x7,        /**< 信号量模块，值为0x7（十进制7） */
    LOS_MOD_MBOX = 0x8,       /**< 静态内存管理模块，值为0x8（十进制8） */
    LOS_MOD_HWI = 0x9,        /**< 硬件中断模块，值为0x9（十进制9） */
    LOS_MOD_HWWDG = 0xa,      /**< 硬件看门狗模块，值为0xa（十进制10） */
    LOS_MOD_CACHE = 0xb,      /**< 缓存管理模块，值为0xb（十进制11） */
    LOS_MOD_HWTMR = 0xc,      /**< 硬件定时器模块，值为0xc（十进制12） */
    LOS_MOD_MMU = 0xd,        /**< 内存管理单元模块，值为0xd（十进制13） */

    LOS_MOD_LOG = 0xe,        /**< 日志模块，值为0xe（十进制14） */
    LOS_MOD_ERR = 0xf,        /**< 错误处理模块，值为0xf（十进制15） */

    LOS_MOD_EXC = 0x10,       /**< 异常处理模块，值为0x10（十进制16） */
    LOS_MOD_CSTK = 0x11,      /**< 调用栈模块，值为0x11（十进制17） */

    LOS_MOD_MPU = 0x12,       /**< 内存保护单元模块，值为0x12（十进制18） */
    LOS_MOD_NMHWI = 0x13,     /**< 不可屏蔽中断模块，值为0x13（十进制19） */
    LOS_MOD_TRACE = 0x14,     /**< 跟踪调试模块，值为0x14（十进制20） */
    LOS_MOD_KNLSTAT = 0x15,   /**< 内核状态模块，值为0x15（十进制21） */
    LOS_MOD_EVTTIME = 0x16,   /**< 事件时间模块，值为0x16（十进制22） */
    LOS_MOD_THRDCPUP = 0x17,  /**< 线程CPU使用率模块，值为0x17（十进制23） */
    LOS_MOD_IPC = 0x18,       /**< 进程间通信模块，值为0x18（十进制24） */
    LOS_MOD_STKMON = 0x19,    /**< 栈监控模块，值为0x19（十进制25） */
    LOS_MOD_TIMER = 0x1a,     /**< 定时器模块，值为0x1a（十进制26） */
    LOS_MOD_RESLEAKMON = 0x1b,/**< 资源泄漏监控模块，值为0x1b（十进制27） */
    LOS_MOD_EVENT = 0x1c,     /**< 事件模块，值为0x1c（十进制28） */
    LOS_MOD_MUX = 0X1d,       /**< 互斥锁模块，值为0x1d（十进制29，注意十六进制字母大写） */
    LOS_MOD_CPUP = 0x1e,      /**< CPU使用率模块，值为0x1e（十进制30） */
    LOS_MOD_HOOK = 0x1f,      /**< 钩子函数模块，值为0x1f（十进制31） */
    LOS_MOD_PERF = 0x20,      /**< 性能分析模块，值为0x20（十进制32） */
    LOS_MOD_PM = 0x21,        /**< 电源管理模块，值为0x21（十进制33） */
    LOS_MOD_SHELL = 0x31,     /**< Shell模块，值为0x31（十进制49） */
    LOS_MOD_DRIVER = 0x41,    /**< 驱动模块，值为0x41（十进制65） */
    LOS_MOD_BUTT              /**< 枚举结束标记，无实际业务含义，值为0x42（十进制66） */
};

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_ERR_H */
