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
    .extern   OsSaveSignalContext
    .extern   OsSchedToUserReleaseLock
    .global   OsTaskSchedule
    .global   OsTaskContextLoad
    .global   OsIrqHandler

    .fpu vfpv4

/* macros to align and unalign the stack on 8 byte boundary for ABI compliance */
.macro STACK_ALIGN, reg /* 栈对齐*/ 
    MOV     \reg, sp 	//reg=sp
    TST     SP, #4		//来检查是否设置了特定的位
    SUBEQ   SP, #4		//表示相等时相减
    PUSH    { \reg }
.endm

.macro STACK_RESTORE, reg /*栈恢复*/ 
    POP     { \reg } 
    MOV     sp, \reg 
.endm

/* macros to save and restore fpu regs */ 
.macro PUSH_FPU_REGS reg1 /* 保存fpu寄存器 */ 
#if !defined(LOSCFG_ARCH_FPU_DISABLE) //FPU使能
    VMRS    \reg1, FPEXC 	
    PUSH    {\reg1}			//对应 TaskContext->regFPEXC
    VMRS    \reg1, FPSCR	
    PUSH    {\reg1}			//对应 TaskContext->regFPSCR
#if defined(LOSCFG_ARCH_FPU_VFP_D32)
    VPUSH   {D16-D31}		//对应 TaskContext->D
#endif
    VPUSH   {D0-D15}		//对应 TaskContext->D
#endif
.endm

.macro POP_FPU_REGS reg1 /* 恢复fpu寄存器 */ 
#if !defined(LOSCFG_ARCH_FPU_DISABLE)
    VPOP    {D0-D15}	//对应 TaskContext->D
#if defined(LOSCFG_ARCH_FPU_VFP_D32)
    VPOP    {D16-D31}	//对应 TaskContext->D
#endif
    POP     {\reg1}
    VMSR    FPSCR, \reg1	//对应 TaskContext->regFPSCR
    POP     {\reg1}
    VMSR    FPEXC, \reg1	//对应 TaskContext->regFPEXC
#endif
.endm

/*
 * R0: new task
 * R1: run task
 */
OsTaskSchedule:			/*任务调度,OsTaskSchedule的目的是将寄存器值按TaskContext的格式保存起来*/
    MRS      R2, CPSR
    STMFD    SP!, {R2}
    STMFD    SP!, {LR}
    STMFD    SP!, {LR}
    STMFD    SP!, {R12}

    /* jump R0 - R3 USP, ULR reserved */
    SUB      SP, SP, #(8 * 4)

    /* push R4 - R11*/
    STMFD    SP!, {R4-R11}

    /* save fpu registers */
    PUSH_FPU_REGS   R2	/*保存fpu寄存器*/

    /* store sp on running task */
    STR     SP, [R1] //在运行的任务栈中保存SP,即*runTask->stackPointer = sp

OsTaskContextLoad: //加载上下文
    /* clear the flag of ldrex */ //LDREX 可从内存加载数据,如果物理地址有共享TLB属性，则LDREX会将该物理地址标记为由当前处理器独占访问，并且会清除该处理器对其他任何物理地址的任何独占访问标记。
    CLREX //清除ldrex指令的标记

    /* switch to new task's sp */
    LDR     SP, [R0] // 即:sp =  *task->stackPointer

    /* restore fpu registers */
    POP_FPU_REGS    R2 //恢复fpu寄存器,这里用了汇编宏R2是宏的参数

    LDMFD   SP!, {R4-R11}
    LDR     R3, [SP, #(11 * 4)]
    AND     R0, R3, #CPSR_MASK_MODE
    CMP     R0, #CPSR_USER_MODE
    BNE     OsKernelTaskLoad

    MVN     R2, #CPSR_INT_DISABLE
    AND     R3, R3, R2
    STR     R3, [SP, #(11 * 4)]

#ifdef LOSCFG_KERNEL_SMP
    BL      OsSchedToUserReleaseLock
#endif

    /* jump sp, reserved */
    ADD     SP, SP, #(2 * 4)
    LDMFD   SP, {R13, R14}^
    ADD     SP, SP, #(2 * 4)
    LDMFD   SP!, {R0-R3, R12, LR}
    RFEIA   SP!

OsKernelTaskLoad: 			//内核任务的加载
    ADD     SP, SP, #(4 * 4)
    LDMFD   SP!, {R0-R3, R12, LR}
    RFEIA   SP!
OsIrqHandler:
    SUB     LR, LR, #4	//记录译码指令地址,以防切换过程丢失指令.

    /* Save pc and cpsr to svc sp, ARMv6 and above support */
    SRSFD   #0x13!
    /* disable irq, switch to svc mode *///超级用户模式(SVC 模式)，主要用于 SWI(软件中断)和 OS(操作系统)。
    CPSID   i, #0x13
#ifdef LOSCFG_KERNEL_PERF
    PUSH    {R0-R3, R12, LR}
    MOV     R0, LR
    MOV     R1, FP
    BL      OsPerfSetIrqRegs
    POP     {R0-R3, R12, LR}
#endif

    STMFD   SP!, {R0-R3, R12, LR}
    STMFD   SP, {R13, R14}^
    SUB     SP, SP, #(4 * 4)
    STR     R4, [SP, #0]

    /*
     * save fpu regs in case in case those been
     * altered in interrupt handlers.
     */
    PUSH_FPU_REGS   R0

    MOV     R4, SP
    EXC_SP_SET __svc_stack_top, OS_EXC_SVC_STACK_SIZE, R1, R2

    BLX     HalIrqHandler

    MOV     SP, R4

    /* process pending signals */
    BLX     OsTaskProcSignal
    BLX     OsSchedIrqEndCheckNeedSched

    /* restore fpu regs */
    POP_FPU_REGS R0
    LDR   R4, [SP, #0]

#ifdef LOSCFG_KERNEL_VM
    /* Obtain the CPSR to determine the mode the system is in when the interrupt is triggered */
    LDR     R3, [SP, #(11 * 4)]
    AND     R1, R3, #CPSR_MASK_MODE
    CMP     R1, #CPSR_USER_MODE
    BNE     1f

    MOV     R0, SP
    STR     R7, [SP, #0]
    /* sp - sizeof(IrqContext) */
    SUB     SP, SP, #(12 * 4)
    MOV     R1, SP
    BLX     OsSaveSignalContext
    MOV     SP, R0
1:
#endif
    ADD     SP, SP, #(2 * 4)
    /* load user sp and lr, and jump cpsr */
    LDMFD   SP, {R13, R14}^
    ADD     SP, SP, #(2 * 4)
    LDMFD   SP!, {R0-R3, R12, LR}
    RFEIA   SP!

FUNCTION(ArchSpinLock)	//非要拿到锁
	mov 	r1, #1		//r1=1
1:						//循环的作用,因SEV是广播事件.不一定lock->rawLock的值已经改变了
	ldrex	r2, [r0]	//r0 = &lock->rawLock, 即 r2 = lock->rawLock
	cmp 	r2, #0		//r2和0比较
	wfene				//不相等时,说明资源被占用,CPU核进入睡眠状态
	strexeq r2, r1, [r0]//此时CPU被重新唤醒,尝试令lock->rawLock=1,成功写入则r2=0
	cmpeq	r2, #0		//再来比较r2是否等于0,如果相等则获取到了锁
	bne 	1b			//如果不相等,继续进入循环
	dmb 				//用DMB指令来隔离，以保证缓冲中的数据已经落实到RAM中
	bx		lr			//此时是一定拿到锁了,跳回调用ArchSpinLock函数



FUNCTION(ArchSpinTrylock)	//尝试拿锁
	mov 	r1, #1			//r1=1
	mov 	r2, r0			//r2 = r0	   
	ldrex	r0, [r2]		//r2 = &lock->rawLock, 即 r0 = lock->rawLock
	cmp 	r0, #0			//r0和0比较
	strexeq r0, r1, [r2]	//尝试令lock->rawLock=1,成功写入则r0=0,否则 r0 =1
	dmb 					//数据存储隔离，以保证缓冲中的数据已经落实到RAM中
	bx		lr				//跳回调用ArchSpinLock函数



FUNCTION(ArchSpinUnlock)	//释放锁
	mov 	r1, #0			//r1=0				
	dmb 					//数据存储隔离，以保证缓冲中的数据已经落实到RAM中
	str 	r1, [r0]		//令lock->rawLock = 0
	dsb 					//数据同步隔离
	sev 					//给各CPU广播事件,唤醒沉睡的CPU们
	bx		lr				//跳回调用ArchSpinLock函数


