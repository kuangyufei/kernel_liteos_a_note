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

#include "los_stackinfo_pri.h"
#include "los_printf_pri.h"
#include "los_config.h"
#include "securec.h"
#ifdef LOSCFG_SHELL
#include "shcmd.h"
#include "shell.h"
#endif

/**
 * @file los_stackinfo.c
 * @brief 栈内容
 * @verbatim
    @note_pic OsExcStackInfo 各个CPU栈布局图,其他栈也是一样,CPU各核硬件栈都是紧挨着
    __undef_stack(SMP)
    +-------------------+ <--- cpu1 top
    |                   |
    |  CPU core1        |
    |                   |
    +--------------------<--- cpu2 top
    |                   |
    |  cpu core 2       |
    |                   |
    +--------------------<--- cpu3 top
    |                   |
    |  cpu core 3       |
    |                   |
    +--------------------<--- cpu4 top
    |                   |
    |  cpu core 4       |
    |                   |
    +-------------------+
 * @endverbatim
 */

/**
 * @brief 栈信息全局指针
 * @details 指向已注册的栈信息数组，用于异常处理时获取栈状态
 */
const StackInfo *g_stackInfo = NULL;

/**
 * @brief 栈信息数量
 * @details 记录已注册的栈信息条目数量
 */
UINT32 g_stackNum;

/**
 * @brief 获取栈水位线（栈使用峰值）
 * @param[in] stackBottom 栈底指针（高地址端）
 * @param[in] stackTop 栈顶指针（低地址端）
 * @param[out] peakUsed 输出参数，栈使用峰值（字节数）
 * @return 操作结果
 * @retval LOS_OK 成功获取水位线
 * @retval LOS_NOK 栈顶魔术字被篡改，栈溢出
 * @details 通过扫描栈顶初始化标记（OS_STACK_INIT）计算栈使用情况，栈生长方向为由高地址向低地址
 */
UINT32 OsStackWaterLineGet(const UINTPTR *stackBottom, const UINTPTR *stackTop, UINT32 *peakUsed)
{
    UINT32 size;                                                          // 已使用栈空间大小
    const UINTPTR *tmp = NULL;                                            // 栈扫描临时指针
    if (*stackTop == OS_STACK_MAGIC_WORD) {                               // 检查栈顶魔术字(0xCCCCCCCC)是否完好
        tmp = stackTop + 1;                                               // 跳过魔术字，从下一个地址开始扫描
        // 遍历栈空间，查找第一个非初始化标记(0xCACACACA)的位置
        while ((tmp < stackBottom) && (*tmp == OS_STACK_INIT)) {
            tmp++;
        }
        // 计算已使用栈空间 = 栈底地址 - 第一个非初始化标记地址
        size = (UINT32)((UINTPTR)stackBottom - (UINTPTR)tmp);
        // 若已使用空间不为0，需加上栈顶指针本身占用的空间(sizeof(CHAR *))
        *peakUsed = (size == 0) ? size : (size + sizeof(CHAR *));
        return LOS_OK;
    } else {
        *peakUsed = OS_INVALID_WATERLINE;                                 // 魔术字被篡改，返回无效水位线
        return LOS_NOK;
    }
}

/**
 * @brief 异常情况下的栈检查
 * @details 遍历所有已注册栈信息，检查各CPU栈顶魔术字是否完好，判断是否发生栈溢出
 */
VOID OsExcStackCheck(VOID)
{
    UINT32 index;                                                         // 栈信息索引
    UINT32 cpuid;                                                         // CPU核心ID
    UINTPTR *stackTop = NULL;                                             // 当前检查的栈顶指针

    if (g_stackInfo == NULL) {                                            // 未注册栈信息，直接返回
        return;
    }
    // 遍历所有栈信息条目
    for (index = 0; index < g_stackNum; index++) {
        // 遍历所有CPU核心
        for (cpuid = 0; cpuid < LOSCFG_KERNEL_CORE_NUM; cpuid++) {
            // 计算当前CPU的栈顶地址 = 基础栈顶地址 + CPUID * 栈大小
            stackTop = (UINTPTR *)((UINTPTR)g_stackInfo[index].stackTop + cpuid * g_stackInfo[index].stackSize);
            if (*stackTop != OS_STACK_MAGIC_WORD) {                       // 检查栈顶魔术字是否被篡改
                PRINT_ERR("cpu:%u %s overflow , magic word changed to 0x%x\n",
                          LOSCFG_KERNEL_CORE_NUM - 1 - cpuid, g_stackInfo[index].stackName, *stackTop);
            }
        }
    }
}

/**
 * @brief 打印异常时的栈信息
 * @details 输出所有已注册栈的详细信息，包括栈名称、CPU ID、地址范围、总大小和已使用大小，并执行栈检查
 */
VOID OsExcStackInfo(VOID)
{
    UINT32 index;                                                         // 栈信息索引
    UINT32 cpuid;                                                         // CPU核心ID
    UINT32 size;                                                          // 栈使用大小
    UINTPTR *stackTop = NULL;                                             // 栈顶指针
    UINTPTR *stack = NULL;                                                // 栈底指针

    if (g_stackInfo == NULL) {                                            // 未注册栈信息，直接返回
        return;
    }

    // 打印栈信息表头
    PrintExcInfo("\n stack name    cpu id     stack addr     total size   used size\n"
                 " ----------    ------     ---------      --------     --------\n");
    // 遍历所有栈信息条目
    for (index = 0; index < g_stackNum; index++) {
        // 遍历所有CPU核心
        for (cpuid = 0; cpuid < LOSCFG_KERNEL_CORE_NUM; cpuid++) {
            // 计算当前CPU的栈顶地址
            stackTop = (UINTPTR *)((UINTPTR)g_stackInfo[index].stackTop + cpuid * g_stackInfo[index].stackSize);
            // 计算栈底地址 = 栈顶地址 + 栈大小（栈从高地址向低地址生长）
            stack = (UINTPTR *)((UINTPTR)stackTop + g_stackInfo[index].stackSize);
            (VOID)OsStackWaterLineGet(stack, stackTop, &size);           // 获取栈使用大小

            // 打印栈详细信息：名称、CPU ID、栈顶地址、总大小、已使用大小
            PrintExcInfo("%11s      %-5d    %-10p     0x%-8x   0x%-4x\n", g_stackInfo[index].stackName,
                         LOSCFG_KERNEL_CORE_NUM - 1 - cpuid, stackTop, g_stackInfo[index].stackSize, size);
        }
    }

    OsExcStackCheck();                                                    // 打印完成后执行栈溢出检查
}

/**
 * @brief 注册栈信息
 * @param[in] stackInfo 栈信息数组指针
 * @param[in] stackNum 栈信息条目数量
 * @details 将栈信息注册到全局变量，供异常处理时使用
 */
VOID OsExcStackInfoReg(const StackInfo *stackInfo, UINT32 stackNum)
{
    g_stackInfo = stackInfo;                                              // 全局指针指向栈信息数组
    g_stackNum = stackNum;                                                // 记录栈信息条目数量
}

/**
 * @brief 初始化任务栈
 * @param[in] stacktop 栈顶指针
 * @param[in] stacksize 栈大小（字节）
 * @details 将栈空间初始化为OS_STACK_INIT(0xCACACACA)，并在栈顶设置魔术字OS_STACK_MAGIC_WORD(0xCCCCCCCC)
 */
VOID OsStackInit(VOID *stacktop, UINT32 stacksize)
{
    /* initialize the task stack, write magic num to stack top */
    errno_t ret = memset_s(stacktop, stacksize, (INT32)OS_STACK_INIT, stacksize);//清一色填 0xCACACACA
    if (ret == EOK) {
    *((UINTPTR *)stacktop) = OS_STACK_MAGIC_WORD;//0xCCCCCCCCC 中文就是"烫烫烫烫" 这几个字懂点计算机的人都不会陌生了.
    }
}

#ifdef LOSCFG_SHELL_CMD_DEBUG
SHELLCMD_ENTRY(stack_shellcmd, CMD_TYPE_EX, "stack", 1, (CmdCallBackFunc)OsExcStackInfo);//采用shell命令静态注册方式
#endif
