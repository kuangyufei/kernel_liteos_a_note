/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
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

#define _GNU_SOURCE
#include "los_syscall.h"
#include "los_task_pri.h"
#include "los_process_pri.h"
#include "los_printf.h"
#include "time.h"
#include "utime.h"
#include "poll.h"
#include "mqueue.h"
#include "los_futex_pri.h"
#include "sys/times.h"
#ifdef LOSCFG_FS_VFS
#include "fs/fs.h"
#include "fs/file.h"
#endif
#include "dirent.h"
#include "fcntl.h"
#include "unistd.h"

#include "sys/mount.h"
#include "sys/resource.h"
#include "sys/mman.h"
#include "sys/uio.h"
#include "sys/prctl.h"
#include "sys/socket.h"
#include "sys/utsname.h"
#include "poll.h"
#include "sys/uio.h"
#ifdef LOSCFG_SHELL
#include "shmsg.h"
#endif
#ifdef LOSCFG_SECURITY_CAPABILITY
#include "capability_api.h"
#endif
#include "sys/shm.h"

//500以后是LiteOS自定义的系统调用，与ARM EABI不兼容
#define SYS_CALL_NUM    (__NR_syscallend + 1) // __NR_syscallend = 500 + customized syscalls
#define NARG_BITS       4	//参数占用位,这里可以看出系统调用的最大参数个数为 2^4 = 16个参数.
#define NARG_MASK       0x0F//参数掩码
#define NARG_PER_BYTE   2	//每个字节两个参数

typedef UINT32 (*SyscallFun1)(UINT32);//一个参数的注册函数
typedef UINT32 (*SyscallFun3)(UINT32, UINT32, UINT32);//1,3,5,7 代表参数的个数
typedef UINT32 (*SyscallFun5)(UINT32, UINT32, UINT32, UINT32, UINT32);
typedef UINT32 (*SyscallFun7)(UINT32, UINT32, UINT32, UINT32, UINT32, UINT32, UINT32);

static UINTPTR g_syscallHandle[SYS_CALL_NUM] = {0};	//系统调用入口函数注册
static UINT8 g_syscallNArgs[(SYS_CALL_NUM + 1) / NARG_PER_BYTE] = {0};//保存系统调用对应的参数数量

//系统调用初始化,完成对系统调用的注册
void SyscallHandleInit(void)
{
#define SYSCALL_HAND_DEF(id, fun, rType, nArg)                                             \
    if ((id) < SYS_CALL_NUM) {                                                             \
        g_syscallHandle[(id)] = (UINTPTR)(fun);                                            \
        g_syscallNArgs[(id) / NARG_PER_BYTE] |= ((id) & 1) ? (nArg) << NARG_BITS : (nArg); \
    }                                                                                      \

    #include "syscall_lookup.h"
#undef SYSCALL_HAND_DEF
}

/* The SYSCALL ID is in R7 on entry.  Parameters follow in R0..R6 */
/******************************************************************
由汇编调用,见于 los_hw_exc.s    / BLX    OsArmA32SyscallHandle
SYSCALL是产生系统调用时触发的信号,R7寄存器存放具体的系统调用ID,也叫系统调用号
regs:参数就是所有寄存器
注意:本函数在用户态和内核态下都可能被调用到
//MOV     R0, SP @获取SP值,R0将作为OsArmA32SyscallHandle的参数
******************************************************************/
LITE_OS_SEC_TEXT UINT32 *OsArmA32SyscallHandle(UINT32 *regs)
{
    UINT32 ret;
    UINT8 nArgs;
    UINTPTR handle;
    UINT32 cmd = regs[REG_R7];//C7寄存器记录了触发了具体哪个系统调用
	
    if (cmd >= SYS_CALL_NUM) {//系统调用的总数
        PRINT_ERR("Syscall ID: error %d !!!\n", cmd);
        return regs;
    }

    if (cmd == __NR_sigreturn) {//收到 __NR_sigreturn 信号
        OsRestorSignalContext(regs);//恢复信号上下文
        return regs;
    }

    handle = g_syscallHandle[cmd];//拿到系统调用的注册函数,类似 SysRead 
    nArgs = g_syscallNArgs[cmd / NARG_PER_BYTE]; /* 4bit per nargs */
    nArgs = (cmd & 1) ? (nArgs >> NARG_BITS) : (nArgs & NARG_MASK);//获取参数个数
    if ((handle == 0) || (nArgs > ARG_NUM_7)) {//系统调用必须有参数且参数不能大于8个
        PRINT_ERR("Unsupport syscall ID: %d nArgs: %d\n", cmd, nArgs);
        regs[REG_R0] = -ENOSYS;
        return regs;
    }
	//regs[0-6] 记录系统调用的参数,这也是由R7寄存器保存系统调用号的原因
    switch (nArgs) {//参数的个数 
        case ARG_NUM_0:
        case ARG_NUM_1:
            ret = (*(SyscallFun1)handle)(regs[REG_R0]);//执行系统调用,类似 SysUnlink(pathname);
            break;
        case ARG_NUM_2://如何是两个参数的系统调用,这里传三个参数也没有问题,因被调用函数不会去取用R2值
        case ARG_NUM_3:
            ret = (*(SyscallFun3)handle)(regs[REG_R0], regs[REG_R1], regs[REG_R2]);//类似 SysExecve(fileName, argv, envp);
            break;
        case ARG_NUM_4:
        case ARG_NUM_5:
            ret = (*(SyscallFun5)handle)(regs[REG_R0], regs[REG_R1], regs[REG_R2], regs[REG_R3],
                                         regs[REG_R4]);
            break;
        default:	//7个参数的情况
            ret = (*(SyscallFun7)handle)(regs[REG_R0], regs[REG_R1], regs[REG_R2], regs[REG_R3],
                                         regs[REG_R4], regs[REG_R5], regs[REG_R6]);
    }

    regs[REG_R0] = ret;//R0保存系统调用返回值
    OsSaveSignalContext(regs);//保存信号上下文现场

    /* Return the last value of curent_regs.  This supports context switches on return from the exception.
     * That capability is only used with theSYS_context_switch system call.
     */
    return regs;//返回寄存器的值
}
