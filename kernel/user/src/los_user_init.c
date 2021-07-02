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

#define SYS_CALL_VALUE 0x900001

//加载init进程

#ifdef LOSCFG_QUICK_START
LITE_USER_SEC_RODATA STATIC CHAR *g_initPath = "/dev/shm/init";
#else
LITE_USER_SEC_RODATA STATIC CHAR *g_initPath = "/bin/init";//由Init_lite在编译后,生成
#endif
//将 sys_call3 链接在 section(".user.text")段
LITE_USER_SEC_TEXT STATIC UINT32 sys_call3(UINT32 nbr, UINT32 parm1, UINT32 parm2, UINT32 parm3)
{
    register UINT32 reg7 __asm__("r7") = (UINT32)(nbr); //系统调用号给了R7寄存器
    register UINT32 reg2 __asm__("r2") = (UINT32)(parm3);//R2 = 参数3
    register UINT32 reg1 __asm__("r1") = (UINT32)(parm2);//R1 = 参数2
    register UINT32 reg0 __asm__("r0") = (UINT32)(parm1);//R0 = 参数1
    
	//SVC指令会触发一个特权调用异常。这为非特权软件调用操作系统或其他只能在PL1级别访问的系统组件提供了一种机制。
    __asm__ __volatile__
    (
        "svc %1" //管理模式（svc）      ［10011］：操作系统使用的保护模式
        : "=r"(reg0)	//输出寄存器为R0
        : "i"(SYS_CALL_VALUE), "r"(reg7), "r"(reg0), "r"(reg1), "r"(reg2)
        : "memory", "r14"
    );
	//相当于执行了 reset_vector_mp.S 中的 向量表0x08对应的 _osExceptSwiHdl 
    return reg0;//reg0的值将在汇编中改变.
}

LITE_USER_SEC_ENTRY VOID OsUserInit(VOID *args)
{
#ifdef LOSCFG_KERNEL_DYNLOAD
    sys_call3(__NR_execve, (UINTPTR)g_initPath, 0, 0);//发起系统调用,陷入内核态,对应 SysExecve ,加载elf运行
#endif
    while (true) {
    }
}
#endif
