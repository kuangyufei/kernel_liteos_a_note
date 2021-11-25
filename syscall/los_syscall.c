/*!
 * @file    los_syscall.c
 * @brief
 * @link kernel-small-bundles-system http://weharmonyos.com/openharmony/zh-cn/device-dev/kernel/kernel-small-bundles-system.html @endlink
   @verbatim
   OpenHarmony LiteOS-A实现了用户态与内核态的区分隔离，用户态程序不能直接访问内核资源，
   而系统调用则为用户态程序提供了一种访问内核资源、与内核进行交互的通道。

   新增系统调用的典型开发流程如下：
   
	   1. 在LibC库中确定并添加新增的系统调用号。
	   2. 在LibC库中新增用户态的函数接口声明及实现。
	   3. 在内核系统调用头文件中确定并添加新增的系统调用号及对应内核处理函数的声明。
	   4. 在内核中新增该系统调用对应的内核处理函数。

   如图所示，用户程序通过调用System API（系统API，通常是系统提供的POSIX接口）进行内核资源访问与交互请求，POSIX接口内部会触发SVC/SWI异常，
   完成系统从用户态到内核态的切换，然后对接到内核的Syscall Handler（系统调用统一处理接口）进行参数解析，最终分发至具体的内核处理函数。
   @endverbatim
 * @image html https://gitee.com/weharmonyos/resources/raw/master/37/sys_call.png   
 * @attention 系统调用提供基础的用户态程序与内核的交互功能，不建议开发者直接使用系统调用接口，推荐使用内核提供的对外POSIX接口，
	 若需要新增系统调用接口，详见开发指导。内核向用户态提供的系统调用接口清单详见kernel/liteos_a/syscall/syscall_lookup.h，
	 内核相应的系统调用对接函数清单详见kernel/liteos_a/syscall/los_syscall.h。
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-11-19
 */
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

#define _GNU_SOURCE
#ifdef LOSCFG_FS_VFS
#include "fs/file.h"
#endif
#include "los_init.h"
#include "los_signal.h"
#include "los_syscall.h"
#include "los_task_pri.h"
#include "los_process_pri.h"
#include "los_hw_pri.h"
#include "los_printf.h"
#include "time.h"
#include "utime.h"
#include "poll.h"
#include "mqueue.h"
#include "los_futex_pri.h"
#include "sys/times.h"
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

//系统调用初始化,完成对系统调用的注册,将系统调用添加到全局变量中 g_syscallHandle,g_syscallNArgs
void OsSyscallHandleInit(void)
{
#define SYSCALL_HAND_DEF(id, fun, rType, nArg)                                             \
    if ((id) < SYS_CALL_NUM) {                                                             \
        g_syscallHandle[(id)] = (UINTPTR)(fun);                                            \
        g_syscallNArgs[(id) / NARG_PER_BYTE] |= ((id) & 1) ? (nArg) << NARG_BITS : (nArg); \
    }                                                                                      \

    #include "syscall_lookup.h"
#undef SYSCALL_HAND_DEF
}

LOS_MODULE_INIT(OsSyscallHandleInit, LOS_INIT_LEVEL_KMOD_EXTENDED);//注册系统调用模块
/* The SYSCALL ID is in R7 on entry.  Parameters follow in R0..R6 */

/**
 * @brief 
    @verbatim
    由汇编调用,见于 los_hw_exc.s    / BLX    OsArmA32SyscallHandle
    SYSCALL是产生系统调用时触发的信号,R7寄存器存放具体的系统调用ID,也叫系统调用号
    regs:参数就是所有寄存器
    注意:本函数在用户态和内核态下都可能被调用到
    //MOV     R0, SP @获取SP值,R0将作为OsArmA32SyscallHandle的参数
	@endverbatim
 * @param regs 
 * @return VOID 
 */
VOID OsArmA32SyscallHandle(TaskContext *regs)
{
    UINT32 ret;
    UINT8 nArgs;
    UINTPTR handle;
    UINT32 cmd = regs->reserved2;
	
    if (cmd >= SYS_CALL_NUM) {//系统调用的总数
        PRINT_ERR("Syscall ID: error %d !!!\n", cmd);
        return;
    }

    handle = g_syscallHandle[cmd];//拿到系统调用的注册函数,类似 SysRead 
    nArgs = g_syscallNArgs[cmd / NARG_PER_BYTE]; /* 4bit per nargs */
    nArgs = (cmd & 1) ? (nArgs >> NARG_BITS) : (nArgs & NARG_MASK);//获取参数个数
    if ((handle == 0) || (nArgs > ARG_NUM_7)) {//系统调用必须有参数且参数不能大于8个
        PRINT_ERR("Unsupported syscall ID: %d nArgs: %d\n", cmd, nArgs);
        regs->R0 = -ENOSYS;
        return;
    }
	//regs[0-6] 记录系统调用的参数,这也是由R7寄存器保存系统调用号的原因
    OsSigIntLock();//禁止响应信号
    switch (nArgs) {//参数的个数 
        case ARG_NUM_0:
        case ARG_NUM_1:
            ret = (*(SyscallFun1)handle)(regs->R0);
            break;
        case ARG_NUM_2:
        case ARG_NUM_3:
            ret = (*(SyscallFun3)handle)(regs->R0, regs->R1, regs->R2);
            break;
        case ARG_NUM_4:
        case ARG_NUM_5:
            ret = (*(SyscallFun5)handle)(regs->R0, regs->R1, regs->R2, regs->R3, regs->R4);
            break;
        default:
            ret = (*(SyscallFun7)handle)(regs->R0, regs->R1, regs->R2, regs->R3, regs->R4, regs->R5, regs->R6);
    }

    regs->R0 = ret;//系统返回值,保存在R0寄存器
    OsSigIntUnlock();//打开响应信号

    return;
}
