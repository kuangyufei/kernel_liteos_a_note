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

#include "asm.h"
#include "arch_config.h"
/******************************************************************************
ARM的指令系统中关于栈指令的内容比较容易引起迷惑，这是因为准确描述一个栈的特点需要两个参数：
	栈地址的增长方向：ARM将向高地址增长的栈称为递增栈（Descendent Stack），
		将向低地址增长的栈称为递减栈（Acendant Stack）
	栈指针的指向位置：ARM将栈指针指向栈顶元素位置的栈称为满栈（Full Stack），
		将栈指针指向即将入栈的元素位置的栈称为空栈（Empty Stack）

栈类型
	根据栈地址增长方向雨栈指针指向位置的不同，自然可以将栈分为四类：

			递增栈	   	递减栈
	空栈		EA栈		ED栈
	满栈		FA栈		FD栈 
	
栈指令
	栈的操作指令无非两种：入栈和出栈，由于ARM描述了四种不同类型的栈，因此对应的栈指令一共有8条。

			入栈		出栈
	EA栈		STMEA	LDMEA
	ED栈		STMED	LDMED
	FA栈		STMFA	LDMFA
	FD栈		STMFD	LDMFD
	
	这些指令具有相似的前缀：
	STM：（STore Multiple data）表示存储数据，即入栈。
	LDM：（LoaD Multiple data）表示加载数据，即出栈。
	一般情况下，可以将栈操作指令分解为两步微指令：数据存取和栈指针移动。这两步操作的先后顺序和栈指针的移动方式由栈的类型决定。
	STMFD	SP减少	写[SP] STMDB
	LDMFD	读[SP] SP增加	LDMIA

参考
	https://www.cnblogs.com/fanzhidongyzby/p/5250116.html

用栈方式图 @note_pic
			-----------------<-------------------  高地址 函数 A
			|		PC		|					|
			|---------------|					|	||
			|		LR		|					|	||
			|---------------|					|	||
			|		SP		|					|	||
			|---------------|					|	\/
			|		FP		|					|
			|---------------|					|
			|	参数1 		|					|
			|---------------|					|			
			|	参数2			|					|
			|---------------|					|
			|	变量1			|					|
			|---------------|<----------|		|	函数A调用B  
			|		PC		|			|		|
			|---------------|			|		|
			|		LR		|			|		|
			|---------------|			|		|
			|		SP		|-----------|		|
			|---------------|					|
			|		FP		|-------------------|
			|---------------|
			|	参数1 		|
			|---------------|
			|	参数2			|
			|---------------|
			|	变量1			|
			|---------------|<------SP
			|	变量2			|
			|---------------|
			|---------------|						低地址	
			
LDMFD   SP!, {PC}^
LDM/STR架构中{∧}为可选后缀,当指令为LDM且寄存器列表中包含R15（PC）,
选用该后缀时表示:除了正常的数据传送之外,还将SPSR复制到CPSR（将备份的程序状态寄存器SPCR恢复到当前程序状态寄存器CPSR）。
******************************************************************************/
    .extern   OsSaveSignalContext        @ 外部函数：保存信号上下文
    .extern   OsSchedToUserReleaseLock   @ 外部函数：调度到用户态时释放锁
    .global   OsTaskSchedule             @ 全局函数：任务调度入口
    .global   OsTaskContextLoad          @ 全局函数：任务上下文加载
    .global   OsIrqHandler               @ 全局函数：中断处理入口

    .fpu vfpv4                           @ 启用VFPv4浮点协处理器支持

/* 栈对齐与恢复宏：确保8字节栈对齐以符合ARM ABI规范 */
.macro STACK_ALIGN, reg                  @ 栈对齐宏
    MOV     \reg, sp
    TST     SP, #4                       @ 检查栈是否已8字节对齐（最低2位是否为0b100）
    SUBEQ   SP, #4                       @ 若未对齐（SP[2]为0），减去4字节使其对齐
    PUSH    { \reg }
.endm

.macro STACK_RESTORE, reg
    POP     { \reg }
    MOV     sp, \reg
.endm

/* macros to save and restore fpu regs */
.macro PUSH_FPU_REGS reg1
#if !defined(LOSCFG_ARCH_FPU_DISABLE)
    VMRS    \reg1, FPEXC
    PUSH    {\reg1}
    VMRS    \reg1, FPSCR
    PUSH    {\reg1}
eg1}                      @ 保存FPU状态
#if defined(LOSCFG_ARCH_FPU_VFP_D32)      @ 若支持32个双精度寄存器
    VPUSH   {D16-D31}                    @ 保存扩展FPU寄存器(D16-D31)
#endif
    VPUSH   {D0-D15}                     @ 保存基本FPU寄存器(D0-D15)
#endif
.endm

.macro POP_FPU_REGS reg1                 @ 恢复FPU寄存器宏
#if !defined(LOSCFG_ARCH_FPU_DISABLE)     @ 若未禁用FPU
    VPOP    {D0-D15}                     @ 恢复基本FPU寄存器(D0-D15)
#if defined(LOSCFG_ARCH_FPU_VFP_D32)      @ 若支持32个双精度寄存器
    VPOP    {D16-D31}                    @ 恢复扩展FPU寄存器(D16-D31)
#endif
    POP     {\reg1}
    VMSR    FPSCR, \reg1
    POP     {\reg1}
    VMSR    FPEXC, \reg1
#endif
.endm

/*
 * 任务调度核心函数：执行任务上下文切换
 * 参数：
 *   R0: 新任务控制块指针
 *   R1: 当前运行任务控制块指针
 */
OsTaskSchedule:
    MRS      R2, CPSR                    @ 读取当前程序状态寄存器
    STMFD    SP!, {R2}                   @ 保存CPSR到栈
    STMFD    SP!, {LR}                   @ 保存链接寄存器
    STMFD    SP!, {LR}                   @ 预留用户态LR位置
    STMFD    SP!, {R12}                  @ 保存R12寄存器

    /* 为用户态R0-R3和USP/ULR预留空间 */
    SUB      SP, SP, #(8 * 4)            @ 栈指针减去32字节(8个寄存器×4字节)

    /* 保存非volatile寄存器(R4-R11) */
    STMFD    SP!, {R4-R11}               @ 批量存储R4-R11到栈

    /* 保存FPU寄存器 */
    PUSH_FPU_REGS   R2                   @ 调用FPU寄存器保存宏

    /* 将当前栈指针保存到运行任务的控制块 */
    STR     SP, [R1]                     @ 存储SP到当前任务的上下文指针

OsTaskContextLoad:                       @ 加载新任务上下文
    /* 清除独占访问标志，防止多CPU同步问题 */
    CLREX                                @ 清除本地监视器的独占访问状态

    /* 切换到新任务的栈指针 */
    LDR     SP, [R0]                     @ 从新任务控制块加载SP

    /* 恢复FPU寄存器 */
    POP_FPU_REGS    R2                   @ 调用FPU寄存器恢复宏

    LDMFD   SP!, {R4-R11}                @ 恢复非volatile寄存器(R4-R11)
    LDR     R3, [SP, #(11 * 4)]          @ 加载保存的CPSR值
    AND     R0, R3, #CPSR_MASK_MODE      @ 提取模式位(低5位)
    CMP     R0, #CPSR_USER_MODE          @ 判断是否切换到用户模式
    BNE     OsKernelTaskLoad             @ 若为内核模式，跳转到内核任务加载

    /* 用户模式任务恢复处理 */
    MVN     R2, #CPSR_INT_DISABLE        @ 构建中断使能掩码(bit7=0)
    AND     R3, R3, R2                   @ 清除CPSR中的中断禁止位(I=0)
    STR     R3, [SP, #(11 * 4)]          @ 更新栈上的CPSR值

#ifdef LOSCFG_KERNEL_SMP                 @ 若启用SMP多核支持
    BL      OsSchedToUserReleaseLock     @ 调用SMP调度释放锁函数
#endif

    /* 调整栈并恢复用户态寄存器 */
    ADD     SP, SP, #(2 * 4)             @ 跳过预留的用户态LR位置
    LDMFD   SP, {R13, R14}^              @ 恢复用户态SP和LR(带^表示用户模式)
    ADD     SP, SP, #(2 * 4)             @ 调整栈指针跳过已恢复的USP/ULR
    LDMFD   SP!, {R0-R3, R12, LR}        @ 恢复volatile寄存器
    RFEIA   SP!                          @ 从异常返回并恢复CPSR，跳转到新任务

OsKernelTaskLoad:                        @ 内核模式任务加载
    ADD     SP, SP, #(4 * 4)             @ 跳过用户态寄存器预留空间
    LDMFD   SP!, {R0-R3, R12, LR}        @ 恢复volatile寄存器
    RFEIA   SP!                          @ 从异常返回并恢复CPSR

/*
 * 中断处理入口函数：响应硬件中断请求
 * 遵循ARM异常处理流程，保存上下文并调用中断服务例程
 */
OsIrqHandler:
    SUB     LR, LR, #4                   @ 调整LR(IRQ模式下LR指向中断返回地址+4)

    /* 保存PC和CPSR到SVC模式栈，ARMv6及以上支持 */
    SRSFD   #0x13!                       @ 将当前状态(PC和CPSR)保存到SVC模式栈
    /* 禁用IRQ，切换到SVC模式(0x13) */
    CPSID   i, #0x13                     @ 更改处理器模式并禁用中断

#ifdef LOSCFG_KERNEL_PERF                @ 若启用性能分析
    PUSH    {R0-R3, R12, LR}             @ 保存寄存器
    MOV     R0, LR                       @ 传递LR作为参数
    MOV     R1, FP                       @ 传递FP作为参数
    BL      OsPerfSetIrqRegs             @ 调用性能分析寄存器设置函数
    POP     {R0-R3, R12, LR}             @ 恢复寄存器
#endif

    /* 保存通用寄存器上下文 */
    STMFD   SP!, {R0-R3, R12, LR}        @ 保存volatile寄存器
    STMFD   SP, {R13, R14}^              @ 保存用户态SP和LR到栈
    SUB     SP, SP, #(4 * 4)             @ 调整栈指针预留空间
    STR     R4, [SP, #0]                 @ 保存R4寄存器

    /*
     * 保存FPU寄存器，防止中断处理程序修改浮点状态
     */
    PUSH_FPU_REGS   R0                   @ 调用FPU寄存器保存宏

    MOV     R4, SP                       @ 保存当前栈指针
    EXC_SP_SET __svc_stack_top, OS_EXC_SVC_STACK_SIZE, R1, R2 @ 设置异常栈

    BLX     HalIrqHandler                @ 调用硬件抽象层中断处理函数

    MOV     SP, R4                       @ 恢复栈指针

    /* 处理挂起的信号 */
    BLX     OsTaskProcSignal             @ 调用任务信号处理函数
    BLX     OsSchedIrqEndCheckNeedSched  @ 检查中断结束后是否需要调度

    /* 恢复FPU寄存器 */
    POP_FPU_REGS R0                      @ 调用FPU寄存器恢复宏
    LDR   R4, [SP, #0]                   @ 恢复R4寄存器

#ifdef LOSCFG_KERNEL_VM                  @ 若启用虚拟内存
    /* 获取CPSR以确定中断触发时的系统模式 */
    LDR     R3, [SP, #(11 * 4)]          @ 加载保存的CPSR
    AND     R1, R3, #CPSR_MASK_MODE      @ 提取模式位
    CMP     R1, #CPSR_USER_MODE          @ 判断是否为用户模式
    BNE     1f                           @ 若非用户模式，跳转到标签1

    MOV     R0, SP                       @ 传递当前SP作为参数
    STR     R7, [SP, #0]                 @ 保存R7寄存器
    /* 栈指针减去IrqContext结构体大小 */
    SUB     SP, SP, #(12 * 4)            @ 预留12个寄存器空间
    MOV     R1, SP                       @ 传递新SP作为参数
    BLX     OsSaveSignalContext          @ 调用保存信号上下文函数
    MOV     SP, R0                       @ 恢复栈指针
1:                                       @ 标签1：非用户模式处理路径
#endif
    ADD     SP, SP, #(2 * 4)             @ 调整栈指针
    /* 加载用户态SP和LR，并恢复CPSR */
    LDMFD   SP, {R13, R14}^              @ 恢复用户态SP和LR
    ADD     SP, SP, #(2 * 4)             @ 调整栈指针
    LDMFD   SP!, {R0-R3, R12, LR}        @ 恢复volatile寄存器
    RFEIA   SP!                          @ 从异常返回，恢复执行

FUNCTION(ArchSpinLock)	@非要拿到锁
	mov 	r1, #1		@r1=1
1:						@循环的作用,因SEV是广播事件.不一定lock->rawLock的值已经改变了
	ldrex	r2, [r0]	@r0 = &lock->rawLock, 即 r2 = lock->rawLock
	cmp 	r2, #0		@r2和0比较
	wfene				@不相等时,说明资源被占用,CPU核进入睡眠状态
	strexeq r2, r1, [r0]@此时CPU被重新唤醒,尝试令lock->rawLock=1,成功写入则r2=0
	cmpeq	r2, #0		@再来比较r2是否等于0,如果相等则获取到了锁
	bne 	1b			@如果不相等,继续进入循环
	dmb 				@用DMB指令来隔离，以保证缓冲中的数据已经落地到RAM中
	bx		lr			@此时是一定拿到锁了,跳回调用ArchSpinLock函数



FUNCTION(ArchSpinTrylock)	@尝试拿锁
	mov 	r1, #1			@r1=1
	mov 	r2, r0			@r2 = r0	   
	ldrex	r0, [r2]		@r2 = &lock->rawLock, 即 r0 = lock->rawLock
	cmp 	r0, #0			@r0和0比较
	strexeq r0, r1, [r2]	@尝试令lock->rawLock=1,成功写入则r0=0,否则 r0 =1
	dmb 					@数据存储隔离，以保证缓冲中的数据已经落地到RAM中
	bx		lr				@跳回调用ArchSpinLock函数



FUNCTION(ArchSpinUnlock)	@释放锁
	mov 	r1, #0			@r1=0				
	dmb 					@数据存储隔离，以保证缓冲中的数据已经落实到RAM中
	str 	r1, [r0]		@令lock->rawLock = 0
	dsb 					@数据同步隔离
	sev 					@sev为发送事件指令,这种事件指的是CPU核与核之间的事件,广播事件给各个CPU核
	bx		lr				@跳回调用ArchSpinLock函数


