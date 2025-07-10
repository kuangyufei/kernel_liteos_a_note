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

#ifndef _ARCH_CONFIG_H
#define _ARCH_CONFIG_H

/**
 * @brief ARM处理器一共有7种工作模式
 * @verbatim 
	CPSR(current program status register)当前程序的状态寄存器
		CPSR有4个8位区域：标志域（F）、状态域（S）、扩展域（X）、控制域（C）
		32 位的程序状态寄存器可分为4 个域：
			1) 位[31：24]为条件标志位域，用f 表示；
			2) 位[23：16]为状态位域，用s 表示；
			3) 位[15：8]为扩展位域，用x 表示； 
			4) 位[7：0]为控制位域，用c 表示；

	CPSR和其他寄存器不一样,其他寄存器是用来存放数据的,都是整个寄存器具有一个含义.
	而CPSR寄存器是按位起作用的,也就是说,它的每一位都有专门的含义,记录特定的信息.

	CPSR的低8位（包括I、F、T和M[4：0]）称为控制位，程序无法修改,
	除非CPU运行于特权模式下,程序才能修改控制位

	N、Z、C、V均为条件码标志位。它们的内容可被算术或逻辑运算的结果所改变，
	并且可以决定某条指令是否被执行!意义重大!

	CPSR的第31位是 N，符号标志位。它记录相关指令执行后,其结果是否为负.
		如果为负 N = 1,如果是非负数 N = 0.
	CPSR的第30位是Z，0标志位。它记录相关指令执行后,其结果是否为0.
		如果结果为0.那么Z = 1.如果结果不为0,那么Z = 0.
	CPSR的第29位是C，进位标志位(Carry)。一般情况下,进行无符号数的运算。
		加法运算：当运算结果产生了进位时（无符号数溢出），C=1，否则C=0。
		减法运算（包括CMP）：当运算时产生了借位时（无符号数溢出），C=0，否则C=1。
	CPSR的第28位是V，溢出标志位(Overflow)。在进行有符号数运算的时候，
		如果超过了机器所能标识的范围，称为溢出。

	MSR{条件} 程序状态寄存器(CPSR 或SPSR)_<域>，操作数
		MSR 指令用于将操作数的内容传送到程序状态寄存器的特定域中
		示例如下：    
		MSR CPSR，R0   @传送R0 的内容到CPSR
		MSR SPSR，R0   @传送R0 的内容到SPSR
		MSR CPSR_c，R0 @传送R0 的内容到CPSR，但仅仅修改CPSR中的控制位域
		
	MRS{条件} 通用寄存器，程序状态寄存器(CPSR 或SPSR)
		MRS 指令用于将程序状态寄存器的内容传送到通用寄存器中。该指令一般用在以下两种情况：
			1) 当需要改变程序状态寄存器的内容时，可用MRS 将程序状态寄存器的内容读入通用寄存器，修改后再写回程序状态寄存器。
			2) 当在异常处理或进程切换时，需要保存程序状态寄存器的值，可先用该指令读出程序状态寄存器的值，然后保存。
		示例如下：
		MRS R0，CPSR   @传送CPSR 的内容到R0
		MRS R0，SPSR   @传送SPSR 的内容到R0
					@MRS指令是唯一可以直接读取CPSR和SPSR寄存器的指令

	SPSR（saved program status register）程序状态保存寄存器.
		每一种模式下都有一个状态寄存器SPSR，用于保存CPSR的状态，
		以便异常返回后恢复异常发生时的工作状态。用户模式和系统模式不是异常状态，
		所以没有SPSR，在这两种模式下访问SPSR，将产生不可预知的后果。
		1、SPSR 为 CPSR 中断时刻的副本，退出中断后，将SPSR中数据恢复到CPSR中。
		2、用户模式和系统模式下SPSR不可用,所以SPSR寄存器只有5个
 * @endverbatim
 */
/**
 * @brief CPSR (Current Program Status Register) 状态位掩码定义
 * @note ARM处理器状态寄存器用于控制处理器模式、中断使能状态等
 */
#define CPSR_INT_DISABLE         0xC0  /* 禁用FIQ和IRQ中断（二进制11000000，十进制192） */
#define CPSR_IRQ_DISABLE         0x80  /* 禁用IRQ中断（二进制10000000，十进制128） */
#define CPSR_FIQ_DISABLE         0x40  /* 禁用FIQ中断（二进制01000000，十进制64） */
#define CPSR_THUMB_ENABLE        0x20  /* 使能Thumb模式（二进制00100000，十进制32） */
#define CPSR_USER_MODE           0x10  /* 用户模式（十进制16） */
#define CPSR_FIQ_MODE            0x11  /* 快速中断模式（十进制17） */
#define CPSR_IRQ_MODE            0x12  /* 外部中断模式（十进制18） */
#define CPSR_SVC_MODE            0x13  /* 管理模式（十进制19） */
#define CPSR_ABT_MODE            0x17  /* 数据访问终止模式（十进制23） */
#define CPSR_UNDEF_MODE          0x1B  /* 未定义指令模式（十进制27） */
#define CPSR_MASK_MODE           0x1F  /* 模式位掩码（取低5位，十进制31） */

/**
 * @brief 异常类型ID定义
 * @note 用于标识不同类型的处理器异常
 */
#define OS_EXCEPT_RESET          0x00  /* 复位异常 */
#define OS_EXCEPT_UNDEF_INSTR    0x01  /* 未定义指令异常 */
#define OS_EXCEPT_SWI            0x02  /* 软件中断异常（系统调用） */
#define OS_EXCEPT_PREFETCH_ABORT 0x03  /* 预取指令终止异常 */
#define OS_EXCEPT_DATA_ABORT     0x04  /* 数据访问终止异常 */
#define OS_EXCEPT_FIQ            0x05  /* 快速中断请求异常 */
#define OS_EXCEPT_ADDR_ABORT     0x06  /* 地址终止异常 */
#define OS_EXCEPT_IRQ            0x07  /* 外部中断请求异常 */

/**
 * @brief 核心数量定义
 * @note 根据SMP（对称多处理）配置决定系统核心数
 */
#ifdef LOSCFG_KERNEL_SMP                /* 若启用SMP多核心配置 */
#define CORE_NUM                 LOSCFG_KERNEL_SMP_CORE_NUM  /* 使用配置的核心数 */
#else                                   /* 单核心配置 */
#define CORE_NUM                 1                           /* 默认核心数为1 */
#endif

/* Initial bit32 stack value. */
#define OS_STACK_INIT            0xCACACACA	///< 栈内区域初始化成 0b 1010 1010 1010
/* Bit32 stack top magic number. */
#define OS_STACK_MAGIC_WORD      0xCCCCCCCC ///< 用于栈底值,可标识为栈是否被溢出过,神奇的 "烫烫烫烫"的根源所在! 0b 1100 1100 1100

/**
 * @brief 
 * @verbatim 
	@note_pic
	*	鸿蒙虚拟内存-栈空间运行时图 
	*	鸿蒙源码分析系列篇: 			https://blog.csdn.net/kuangyufei 
							https://my.oschina.net/u/3751245						

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
	|  0xC32F9158       |
	+-------------------+0x000000FFB
	|  0x17321796        |
	+-------------------+0x000001000 <---- stack bottom
 * @endverbatim
 */
/**
 * @brief 异常模式栈大小定义
 * @note 根据调试配置和异常类型定义不同的栈空间大小（单位：字节）
 */
#ifdef LOSCFG_GDB                /* 若启用GDB调试 */
#define OS_EXC_UNDEF_STACK_SIZE  512  /* 未定义指令异常栈大小（512字节） */
#define OS_EXC_ABT_STACK_SIZE    512  /* 终止异常栈大小（512字节） */
#else                               /* 非调试模式 */
#define OS_EXC_UNDEF_STACK_SIZE  40   /* 未定义指令异常栈大小（40字节） */
#define OS_EXC_ABT_STACK_SIZE    40   /* 终止异常栈大小（40字节） */
#endif
#define OS_EXC_FIQ_STACK_SIZE    64   /* 快速中断模式栈大小（64字节） */
#define OS_EXC_IRQ_STACK_SIZE    64   /* 外部中断模式栈大小（64字节） */
#define OS_EXC_SVC_STACK_SIZE    0x2000  /* 管理模式栈大小（8192字节） */
#define OS_EXC_STACK_SIZE        0x1000  /* 通用异常栈大小（4096字节） */

/**
 * @brief ARM寄存器索引定义
 * @note 用于标识寄存器在上下文保存/恢复时的数组索引位置
 */
#define REG_R0   0   /* R0通用寄存器索引 */
#define REG_R1   1   /* R1通用寄存器索引 */
#define REG_R2   2   /* R2通用寄存器索引 */
#define REG_R3   3   /* R3通用寄存器索引 */
#define REG_R4   4   /* R4通用寄存器索引 */
#define REG_R5   5   /* R5通用寄存器索引 */
#define REG_R6   6   /* R6通用寄存器索引 */
#define REG_R7   7   /* R7通用寄存器索引 */
#define REG_R8   8   /* R8通用寄存器索引 */
#define REG_R9   9   /* R9通用寄存器索引 */
#define REG_R10  10  /* R10通用寄存器索引 */
#define REG_R11  11  /* R11通用寄存器索引（FP帧指针） */
#define REG_R12  12  /* R12通用寄存器索引（IP内部过程调用寄存器） */
#define REG_R13  13  /* R13寄存器索引（SP栈指针） */
#define REG_R14  14  /* R14寄存器索引（LR链接寄存器） */
#define REG_R15  15  /* R15寄存器索引（PC程序计数器） */
#define REG_CPSR 16  /* CPSR状态寄存器索引 */
#define REG_SP   REG_R13  /* 栈指针寄存器（R13别名） */
#define REG_LR   REG_R14  /* 链接寄存器（R14别名） */
#define REG_PC   REG_R15  /* 程序计数器（R15别名） */
#endif