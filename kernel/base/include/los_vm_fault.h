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
 * @defgroup los_vm_fault vm fault definition
 * @ingroup kernel
 */

#ifndef __LOS_VM_FAULT_H__
#define __LOS_VM_FAULT_H__

#include "los_typedef.h"
#include "los_exc.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @brief 异常地址映射表结构体
 * @core 存储异常发生地址与修复地址的对应关系，用于虚拟内存故障处理
 * @attention 需确保excAddr与fixAddr的地址空间一致性
 */
typedef struct { 
    VADDR_T excAddr;       ///< 异常发生地址（虚拟地址）
    VADDR_T fixAddr;       ///< 修复地址，指向异常处理程序入口
} LosExcTable; 

/**
 * @defgroup vm_pagefault_flags 页故障标志位定义
 * @brief 描述页故障类型的标志集合，可通过位或运算组合使用
 * @{ 
 */
#define VM_MAP_PF_FLAG_WRITE       (1U << 0)  ///< 写操作触发页故障（0x01）
#define VM_MAP_PF_FLAG_USER        (1U << 1)  ///< 用户模式下触发页故障（0x02）
#define VM_MAP_PF_FLAG_INSTRUCTION (1U << 2)  ///< 取指令操作触发页故障（0x04）
#define VM_MAP_PF_FLAG_NOT_PRESENT (1U << 3)  ///< 页表项不存在导致故障（0x08）
/** @} */

/**
 * @brief 虚拟内存页故障处理函数
 * @param vaddr 触发故障的虚拟地址
 * @param flags 故障类型标志组合（见vm_pagefault_flags）
 * @param frame 异常上下文指针，包含故障发生时的CPU状态
 * @return STATUS_T 处理结果：LOS_OK表示成功，其他值表示失败
 * @note 该函数需在中断上下文安全执行，避免使用可能引起阻塞的操作
 */
STATUS_T OsVmPageFaultHandler(VADDR_T vaddr, UINT32 flags, ExcContext *frame);
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* __LOS_VM_FAULT_H__ */

