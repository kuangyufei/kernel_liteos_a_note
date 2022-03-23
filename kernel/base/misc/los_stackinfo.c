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

const StackInfo *g_stackInfo = NULL;    ///< CPU所有工作模式的栈信息
UINT32 g_stackNum;  ///< CPU所有工作模式的栈数量
///获取栈的吃水线
UINT32 OsStackWaterLineGet(const UINTPTR *stackBottom, const UINTPTR *stackTop, UINT32 *peakUsed)
{
    UINT32 size;
    const UINTPTR *tmp = NULL;
    if (*stackTop == OS_STACK_MAGIC_WORD) {//栈顶值是否等于 magic 0xCCCCCCCC
        tmp = stackTop + 1;
        while ((tmp < stackBottom) && (*tmp == OS_STACK_INIT)) {//记录从栈顶到栈低有多少个连续的 0xCACACACA
            tmp++;
        }
        size = (UINT32)((UINTPTR)stackBottom - (UINTPTR)tmp);//剩余多少非0xCACACACA的栈空间
        *peakUsed = (size == 0) ? size : (size + sizeof(CHAR *));//得出高峰用值,还剩多少可用
        return LOS_OK;
    } else {
        *peakUsed = OS_INVALID_WATERLINE;//栈溢出了
        return LOS_NOK;
    }
}
///异常情况下的栈检查,主要就是检查栈顶值有没有被改写
VOID OsExcStackCheck(VOID)
{
    UINT32 index;
    UINT32 cpuid;
    UINTPTR *stackTop = NULL;

    if (g_stackInfo == NULL) {
        return;
    }
    for (index = 0; index < g_stackNum; index++) {
        for (cpuid = 0; cpuid < LOSCFG_KERNEL_CORE_NUM; cpuid++) {
            stackTop = (UINTPTR *)((UINTPTR)g_stackInfo[index].stackTop + cpuid * g_stackInfo[index].stackSize);
            if (*stackTop != OS_STACK_MAGIC_WORD) {// 只要栈顶内容不是 0xCCCCCCCCC 就是溢出了. 
                PRINT_ERR("cpu:%u %s overflow , magic word changed to 0x%x\n",
                          LOSCFG_KERNEL_CORE_NUM - 1 - cpuid, g_stackInfo[index].stackName, *stackTop);
            }
        }
    }
}

///打印栈的信息 把每个CPU的栈信息打印出来
VOID OsExcStackInfo(VOID)
{
    UINT32 index;
    UINT32 cpuid;
    UINT32 size;
    UINTPTR *stackTop = NULL;
    UINTPTR *stack = NULL;

    if (g_stackInfo == NULL) {
        return;
    }

    PrintExcInfo("\n stack name    cpu id     stack addr     total size   used size\n"
                 " ----------    ------     ---------      --------     --------\n");
    for (index = 0; index < g_stackNum; index++) {
        for (cpuid = 0; cpuid < LOSCFG_KERNEL_CORE_NUM; cpuid++) {//可以看出 各个CPU的栈是紧挨的的
            stackTop = (UINTPTR *)((UINTPTR)g_stackInfo[index].stackTop + cpuid * g_stackInfo[index].stackSize);
            stack = (UINTPTR *)((UINTPTR)stackTop + g_stackInfo[index].stackSize);
            (VOID)OsStackWaterLineGet(stack, stackTop, &size);//获取吃水线, 鸿蒙用WaterLine 这个词用的很妙

            PrintExcInfo("%11s      %-5d    %-10p     0x%-8x   0x%-4x\n", g_stackInfo[index].stackName,
                         LOSCFG_KERNEL_CORE_NUM - 1 - cpuid, stackTop, g_stackInfo[index].stackSize, size);
        }
    }

    OsExcStackCheck();//发生异常时栈检查
}

///注册栈信息
VOID OsExcStackInfoReg(const StackInfo *stackInfo, UINT32 stackNum)
{
    g_stackInfo = stackInfo;	//全局变量指向g_excStack
    g_stackNum = stackNum;
}

///task栈的初始化,设置固定的值. 0xcccccccc 和 0xcacacaca
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
