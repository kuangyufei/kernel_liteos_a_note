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

#ifndef _LOS_HWI_PRI_H
#define _LOS_HWI_PRI_H

#include "los_hwi.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_hwi
 * The hwi form does not contain exceptions for Aarch32
 */
/* 硬件中断表单异常数量定义 */
#define OS_HWI_FORM_EXC_NUM 0                               /* 异常中断数量，必须为0 */
#if OS_HWI_FORM_EXC_NUM != 0                                /* 条件编译：检查异常中断数量是否为0 */
#error "OS_HWI_FORM_EXC_NUM must be zero"                   /* 编译错误：异常中断数量必须为0 */
#endif

/* 硬件中断注册状态判断宏 */
#ifdef LOSCFG_NO_SHARED_IRQ                                 /* 条件编译：禁用中断共享功能时 */
#define HWI_IS_REGISTED(num)             ((&g_hwiForm[num])->pfnHook != NULL)  /* 判断中断是否注册：检查中断钩子函数是否为空 */
#else                                                        /* 条件编译：启用中断共享功能时 */
#define HWI_IS_REGISTED(num)             ((&g_hwiForm[num])->pstNext != NULL)  /* 判断中断是否注册：检查中断链表下一个节点是否为空 */
#endif
extern VOID OsHwiInit(VOID);
extern VOID OsIncHwiFormCnt(UINT16 cpuid, UINT32 index);
extern UINT32 OsGetHwiFormCnt(UINT16 cpuid, UINT32 index);
extern CHAR *OsGetHwiFormName(UINT32 index);
extern VOID OsInterrupt(UINT32 intNum);
extern VOID OsSyscallHandleInit(VOID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
#endif /* _LOS_HWI_PRI_H */
