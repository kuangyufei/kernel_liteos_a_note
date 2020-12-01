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

#ifndef _ARCH_CONFIG_H
#define _ARCH_CONFIG_H
//ARM处理器一共有7种工作模式
#include "menuconfig.h"
//CPSR 当前程序的状态寄存器
#define CPSR_INT_DISABLE         0xC0 /* Disable both FIQ and IRQ */	//禁止中断
#define CPSR_IRQ_DISABLE         0x80 /* IRQ disabled when =1 */		//只禁止IRQ 中断
#define CPSR_FIQ_DISABLE         0x40 /* FIQ disabled when =1 */		//禁止 FIQ中断
#define CPSR_THUMB_ENABLE        0x20 /* Thumb mode when   =1 */		//模式 1:CPU处于Thumb状态， 0:CPU处于ARM状态
#define CPSR_USER_MODE           0x10	//用户模式,除了用户模式，其余模式也叫特权模式,特权模式中除了系统模式以外的其余5种模式称为异常模式；
#define CPSR_FIQ_MODE            0x11	//快中断模式 用于高速数据传输或通道处理
#define CPSR_IRQ_MODE            0x12	//中断模式 用于通用的中断处理
#define CPSR_SVC_MODE            0x13	//管理模式 操作系统使用的保护模式
#define CPSR_ABT_MODE            0x17	//ABT模式 当数据或指令预取终止时进入该模式，用于虚拟存储及存储保护
#define CPSR_UNDEF_MODE          0x1B	//未定义模式（其他模式）当未定义的指令执行时进入该模式，用于支持硬件协处理器的软件仿真
#define CPSR_MASK_MODE           0x1F

/* Define exception type ID */		//ARM处理器一共有7种工作模式，除了用户和系统模式其余都叫异常工作模式
#define OS_EXCEPT_RESET          0x00	//重置功能，例如：开机就进入CPSR_SVC_MODE模式
#define OS_EXCEPT_UNDEF_INSTR    0x01	//未定义的异常，就是others
#define OS_EXCEPT_SWI            0x02	//软件定时器中断
#define OS_EXCEPT_PREFETCH_ABORT 0x03	//预取异常(取指异常), 指令三步骤: 取指,译码,执行, 
#define OS_EXCEPT_DATA_ABORT     0x04	//数据异常
#define OS_EXCEPT_FIQ            0x05	//快中断异常
#define OS_EXCEPT_ADDR_ABORT     0x06	//地址异常
#define OS_EXCEPT_IRQ            0x07	//普通中断异常

/* Define core num */
#ifdef LOSCFG_KERNEL_SMP
#define CORE_NUM                 LOSCFG_KERNEL_SMP_CORE_NUM //CPU 核数
#else
#define CORE_NUM                 1
#endif

/* Initial bit32 stack value. */
#define OS_STACK_INIT            0xCACACACA	//栈指针初始化值 0b 1010 1010 1010
/* Bit32 stack top magic number. */
#define OS_STACK_MAGIC_WORD      0xCCCCCCCC //用于栈顶值,可标识为栈是否被溢出过,神奇的 "烫烫烫烫"的根源所在! 0b 1100 1100 1100
/*************************************************************************** @note_pic
*	鸿蒙虚拟内存-栈空间运行时图 
*	鸿蒙源码分析系列篇: 			https://blog.csdn.net/kuangyufei 
						https://my.oschina.net/u/3751245
****************************************************************************						

+-------------------+0x00000000	<---- stack top == OS_STACK_MAGIC_WORD
|  0xCCCCCCCC       |
+-------------------+0x00000004 == OS_STACK_INIT
|  0xCACACACA       |
+-------------------+0x00000008
|  0xCACACACA       |
+-------------------+	//1.一个栈初始化时 栈顶为 0xCCCCCCCC 其余空间全为 0xCACACACA 这样很容易通过值得比较知道栈有没有溢出
|  0xCACACACA       |	//2.在虚拟地址的序列上 栈底是高于栈顶的
+-------------------+	//3.sp在初始位置是指向栈底的
|  0xCACACACA       |	//4.还有多少0xCACACACA就代表使用的最高峰 peak used
+-------------------+	//5.一旦栈顶不是0xCCCCCCCC,说明已经溢出了,检测栈的溢出就是通过 栈顶值是否等于0xCCCCCCCC
|  0xCACACACA       |
+-------------------+0x000000FF8 <--- sp
|  0xC32F9876       |
+-------------------+0x000000FFB
|  0x6543EB6        |
+-------------------+0x000001000 <---- stack bottom

*/

#ifdef LOSCFG_GDB
#define OS_EXC_UNDEF_STACK_SIZE  512
#define OS_EXC_ABT_STACK_SIZE    512
#else
#define OS_EXC_UNDEF_STACK_SIZE  40
#define OS_EXC_ABT_STACK_SIZE    40
#endif
#define OS_EXC_FIQ_STACK_SIZE    64
#define OS_EXC_IRQ_STACK_SIZE    64
#define OS_EXC_SVC_STACK_SIZE    0x2000
#define OS_EXC_STACK_SIZE        0x1000

#define REG_R0   0 
#define REG_R1   1
#define REG_R2   2
#define REG_R3   3
#define REG_R4   4
#define REG_R5   5
#define REG_R6   6
#define REG_R7   7
#define REG_R8   8
#define REG_R9   9
#define REG_R10  10
#define REG_R11  11
#define REG_R12  12
#define REG_R13  13
#define REG_R14  14
#define REG_R15  15
#define REG_CPSR 16 		//程序状态寄存器(current program status register) (当前程序状态寄存器)
#define REG_SP   REG_R13 	//堆栈指针 当不使用堆栈时，R13 也可以用做通用数据寄存器
#define REG_LR   REG_R14 	//连接寄存器。当执行子程序或者异常中断时，跳转指令会自动将当前地址存入LR寄存器中，当执行完子程 序或者中断后，根据LR中的值，恢复或者说是返回之前被打断的地址继续执行
#define REG_PC   REG_R15  	//指令寄存器
#endif
