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

#include "los_user_init.h"
#include "los_syscall.h"

#ifdef LOSCFG_KERNEL_SYSCALL

/* 系统调用中断值定义 - 用于触发用户态到内核态的切换 */
#define SYS_CALL_VALUE 0x900001  // SVC指令的立即数参数，内核据此识别用户态系统调用请求

/* 初始化程序路径定义 - 根据编译配置选择不同的init程序路径 */
#ifdef LOSCFG_QUICK_START                     // 快速启动模式配置项
LITE_USER_SEC_RODATA STATIC CHAR *g_initPath = "/dev/shm/init";  // 快速启动模式：使用共享内存中的init程序
#else                                        // 标准启动模式
LITE_USER_SEC_RODATA STATIC CHAR *g_initPath = "/bin/init";       // 标准模式：使用文件系统中的init程序
#endif

/* 系统调用封装函数 - 实现用户态到内核态的参数传递与调用触发 */
/* 
 * 功能：通过SVC指令触发系统调用，传递3个参数
 * 参数：nbr - 系统调用号（如__NR_execve）
 *       parm1/parm2/parm3 - 系统调用参数
 * 返回值：系统调用返回结果
 */
LITE_USER_SEC_TEXT STATIC UINT32 sys_call3(UINT32 nbr, UINT32 parm1, UINT32 parm2, UINT32 parm3)
{
    register UINT32 reg7 __asm__("r7") = (UINT32)(nbr);  // r7寄存器存储系统调用号
    register UINT32 reg2 __asm__("r2") = (UINT32)(parm3); // r2存储第三个参数
    register UINT32 reg1 __asm__("r1") = (UINT32)(parm2); // r1存储第二个参数
    register UINT32 reg0 __asm__("r0") = (UINT32)(parm1); // r0存储第一个参数

    /* 内联汇编实现SVC系统调用 */
    __asm__ __volatile__
    (
        "svc %1"                                  // 执行SVC指令，触发系统调用异常
        : "=r"(reg0)                              // 输出：r0寄存器保存系统调用返回值
        : "i"(SYS_CALL_VALUE), "r"(reg7), "r"(reg0), "r"(reg1), "r"(reg2)  // 输入：立即数SYS_CALL_VALUE，寄存器r7-r0
        : "memory", "r14"                         // 影响：内存状态和r14（链接寄存器）
    );

    return reg0;  // 返回系统调用结果
}

/* 用户模式入口函数 - 内核启动用户空间的第一个函数 */
LITE_USER_SEC_ENTRY VOID OsUserInit(VOID *args)  // args：内核传递给用户程序的参数（未使用）
{
#ifdef LOSCFG_KERNEL_DYNLOAD                     // 动态加载功能配置项
    /* 执行init程序 - 通过系统调用启动用户态初始化进程 */
    sys_call3(__NR_execve, (UINTPTR)g_initPath, 0, 0);  // 参数：路径、argv=NULL、envp=NULL
#endif
    /* 防止用户程序退出 - 若init启动失败则进入无限循环 */
    while (true) {
    }
}

#endif
