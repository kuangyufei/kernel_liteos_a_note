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
 * @date    2025-07-07
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
#ifdef LOSCFG_SHELL
#include "shmsg.h"
#endif
#ifdef LOSCFG_SECURITY_CAPABILITY
#include "capability_api.h"
#endif
#include "sys/shm.h"


// 系统调用总数 = 最大系统调用号 + 1
// __NR_syscallend是系统调用号的上限值，定义在系统调用头文件中
#define SYS_CALL_NUM    (__NR_syscallend + 1)
// 每个系统调用参数数量占用的位数 (4 bits = 0-15个参数)
#define NARG_BITS       4
// 参数数量的掩码 (用于提取低4位)
#define NARG_MASK       0x0F
// 每个字节可存储的参数数量信息个数 (1字节=8位，每个参数占4位，故可存2个)
#define NARG_PER_BYTE   2

// 系统调用函数指针类型定义 (不同参数个数)
typedef UINT32 (*SyscallFun1)(UINT32);       // 1个参数的系统调用函数类型
typedef UINT32 (*SyscallFun3)(UINT32, UINT32, UINT32); // 3个参数的系统调用函数类型
typedef UINT32 (*SyscallFun5)(UINT32, UINT32, UINT32, UINT32, UINT32); // 5个参数的系统调用函数类型
typedef UINT32 (*SyscallFun7)(UINT32, UINT32, UINT32, UINT32, UINT32, UINT32, UINT32); // 7个参数的系统调用函数类型

// 系统调用处理函数指针数组
// 索引为系统调用号，值为对应处理函数的地址
static UINTPTR g_syscallHandle[SYS_CALL_NUM] = {0};

// 系统调用参数数量数组 (紧凑存储)
// 每个字节存储2个系统调用的参数数量(低4位和高4位)
static UINT8 g_syscallNArgs[(SYS_CALL_NUM + 1) / NARG_PER_BYTE] = {0};

/**
 * @brief 系统调用处理函数和参数数量初始化
 * @details 从syscall_lookup.h文件中展开宏定义，初始化g_syscallHandle和g_syscallNArgs数组
 */
void OsSyscallHandleInit(void)
{
    // 系统调用处理函数定义宏
    // id: 系统调用号
    // fun: 处理函数名
    // rType: 返回值类型(未使用)
    // nArg: 参数数量
#define SYSCALL_HAND_DEF(id, fun, rType, nArg)                                             \
    if ((id) < SYS_CALL_NUM) {                                                             \
        g_syscallHandle[(id)] = (UINTPTR)(fun);                                            \
        // 根据系统调用号奇偶性，将参数数量存入字节的低4位或高4位
        g_syscallNArgs[(id) / NARG_PER_BYTE] |= ((id) & 1) ? (nArg) << NARG_BITS : (nArg); \
    }                                                                                      \

    // 包含系统调用定义表，展开SYSCALL_HAND_DEF宏进行初始化
    #include "syscall_lookup.h"
#undef SYSCALL_HAND_DEF  // 取消宏定义
}

LOS_MODULE_INIT(OsSyscallHandleInit, LOS_INIT_LEVEL_KMOD_EXTENDED);

/* The SYSCALL ID is in R7 on entry.  Parameters follow in R0..R6 */
/** @brief 
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
/**
 * @brief ARM 32位架构系统调用处理函数
 * @param regs 任务上下文寄存器，包含系统调用参数和返回值
 * @return 无
 * @note 负责解析系统调用ID、参数数量，调用相应处理函数并返回结果
 */
VOID OsArmA32SyscallHandle(TaskContext *regs)
{
    UINT32 ret;                  // 系统调用返回值
    UINT8 nArgs;                 // 系统调用参数数量
    UINTPTR handle;              // 系统调用处理函数指针
    UINT32 cmd = regs->reserved2; // 系统调用ID（从寄存器获取）

    // 检查系统调用ID是否超出有效范围
    if (cmd >= SYS_CALL_NUM) {
        PRINT_ERR("Syscall ID: error %d !!!\n", cmd);
        return;
    }

    // 获取系统调用处理函数和参数数量
    handle = g_syscallHandle[cmd];
    nArgs = g_syscallNArgs[cmd / NARG_PER_BYTE]; /* 每个字节存储2个系统调用的参数数量（4bit/个） */
    // 根据系统调用ID奇偶性提取高4位或低4位参数数量
    nArgs = (cmd & 1) ? (nArgs >> NARG_BITS) : (nArgs & NARG_MASK);
    // 检查处理函数是否存在及参数数量是否合法（最多7个参数）
    if ((handle == 0) || (nArgs > ARG_NUM_7)) {
        PRINT_ERR("Unsupported syscall ID: %d nArgs: %d\n", cmd, nArgs);
        regs->R0 = -ENOSYS;      // 设置返回值为"不支持的系统调用"
        return;
    }

    OsSigIntLock();              // 关闭中断，确保系统调用处理的原子性
    // 根据参数数量调用不同类型的系统调用处理函数
    switch (nArgs) {
        case ARG_NUM_0:          // 无参数或1个参数
        case ARG_NUM_1:
            ret = (*(SyscallFun1)handle)(regs->R0);
            break;
        case ARG_NUM_2:          // 2个参数或3个参数
        case ARG_NUM_3:
            ret = (*(SyscallFun3)handle)(regs->R0, regs->R1, regs->R2);
            break;
        case ARG_NUM_4:          // 4个参数或5个参数
        case ARG_NUM_5:
            ret = (*(SyscallFun5)handle)(regs->R0, regs->R1, regs->R2, regs->R3, regs->R4);
            break;
        default:                 // 6个或7个参数（默认情况）
            ret = (*(SyscallFun7)handle)(regs->R0, regs->R1, regs->R2, regs->R3, regs->R4, regs->R5, regs->R6);
    }

    regs->R0 = ret;              // 将系统调用返回值存入R0寄存器
    OsSigIntUnlock();            // 恢复中断

    return;
}
