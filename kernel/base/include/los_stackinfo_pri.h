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

#ifndef _LOS_STACK_INFO_PRI_H
#define _LOS_STACK_INFO_PRI_H

#include "los_typedef.h"
#include "arch_config.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @brief 栈信息结构体
 * @details 存储任务或线程的栈相关信息，包括栈顶地址、大小和名称
 * @attention 该结构体通常用于任务创建和栈监控功能
 */
typedef struct {
    VOID *stackTop;       /* 栈顶地址，指向栈空间的最高地址 */
    UINT32 stackSize;     /* 栈大小（单位：字节），表示栈空间的总容量 */
    CHAR *stackName;      /* 栈名称，用于标识不同任务的栈，便于调试 */
} StackInfo; 

/**
 * @brief 无效的水位线值
 * @details 表示栈水位线（已使用栈空间标记）无效或未初始化
 * @note 十六进制值0xFFFFFFFF对应十进制4294967295
 */
#define OS_INVALID_WATERLINE 0xFFFFFFFF

/**
 * @brief 栈魔术字校验宏
 * @details 检查栈顶的魔术字是否有效，用于检测栈溢出
 * @param[in] topstack 栈顶地址指针
 * @return UINTPTR - 1表示魔术字有效，0表示无效
 * @note 魔术字OS_STACK_MAGIC_WORD通常定义为0xAAAAAAAA或类似特殊值，用于栈完整性校验
 */
#define OS_STACK_MAGIC_CHECK(topstack) (*(UINTPTR *)(topstack) == OS_STACK_MAGIC_WORD)  /* 1:magic valid 0:invalid */

extern VOID OsExcStackInfo(VOID);
extern VOID OsExcStackInfoReg(const StackInfo *stackInfo, UINT32 stackNum);
extern VOID OsStackInit(VOID *stacktop, UINT32 stacksize);

/**
 * @ingroup  los_task
 * @brief Get stack waterline.
 *
 * @par Description:
 * This API is used to get stack waterline size and check stack whether overflow.
 *
 * @attention None
 *
 * @param  stackBottom [IN]  Type #const UINTPTR * pointer to stack bottom.
 * @param  stackTop    [IN]  Type #const UINTPTR * pointer to stack top.
 * @param  peakUsed    [OUT] Type #UINT32 * stack waterline.
 *
 * @retval #LOS_NOK        stack overflow
 * @retval #LOS_OK         stack is normal, not overflow
 * @par Dependency:
 * <ul><li>los_stackinfo_pri.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
extern UINT32 OsStackWaterLineGet(const UINTPTR *stackBottom, const UINTPTR *stackTop, UINT32 *peakUsed);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_STACK_INFO_PRI_H */