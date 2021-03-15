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
#ifdef LOSCFG_KERNEL_SMP_LOCKDEP
    .extern   OsLockDepCheckOut/*使用外部定义的函数*/
#endif
    .extern   g_taskSpin
    .extern   g_percpu
    .global   OsStartToRun 		/*任务开始执行  告诉 OsStartToRun()在本文件实现,将被外部函数调用,见于OsStart()   */
    .global   OsTaskSchedule	/*任务调度,见于OsSchedResched()*/
    .global   OsIrqHandler		/*硬中断处理*/
    .global   ArchSpinUnlock	/*自旋锁解锁*/
    .global   OsSchedToUserSpinUnlock /*尚未实现*/
	/* @note_why 为何要重新定义OS_TASK_STATUS_RUNNING? */
    .equ OS_TASK_STATUS_RUNNING,       0x0010U 	/* .equ用于把常量值设置为可以在文本段中使用的符号 #define OS_TASK_STATUS_RUNNING 0x0010U */
    .equ OS_PERCPU_STRUCT_SIZE,        0x28U 	/*宏定义 #define OS_PERCPU_STRUCT_SIZE 0x28U*/
    .equ OS_PERCPU_TASK_LOCK_OFFSET,   0x14U	/*宏定义 #define OS_PERCPU_TASK_LOCK_OFFSET 0x14U*/
    .fpu vfpv4 /* .fpu @note_why 尚未知这句话的含义 */
/* 此宏用于对齐和不对齐8字节边界上的堆栈以符合ABI */
/* macros to align and unalign the stack on 8 byte boundary for ABI compliance */
.macro STACK_ALIGN, reg /* 栈对齐*/ 
    MOV     \reg, sp 	@reg=sp
    TST     SP, #4		@来检查是否设置了特定的位
    SUBEQ   SP, #4		@表示相等时相减
    PUSH    { \reg }
.endm

.macro STACK_RESTORE, reg /*栈恢复*/ 
    POP     { \reg } 
    MOV     sp, \reg 
.endm
/* FPU(floating-point processor unit)浮点运算单元*/
/* macros to save and restore fpu regs */ 
.macro PUSH_FPU_REGS reg1 /* 保存fpu寄存器 */ 
#if !defined(LOSCFG_ARCH_FPU_DISABLE) @FPU使能
    VMRS    \reg1, FPEXC
    PUSH    {\reg1}
    VMRS    \reg1, FPSCR
    PUSH    {\reg1}
#if defined(LOSCFG_ARCH_FPU_VFP_D32)
    VPUSH   {D16-D31}
#endif
    VPUSH   {D0-D15}
#endif
.endm

.macro POP_FPU_REGS reg1 /* 恢复fpu寄存器 */ 
#if !defined(LOSCFG_ARCH_FPU_DISABLE)
    VPOP    {D0-D15}
#if defined(LOSCFG_ARCH_FPU_VFP_D32)
    VPOP    {D16-D31}
#endif
    POP     {\reg1}
    VMSR    FPSCR, \reg1
    POP     {\reg1}
    VMSR    FPEXC, \reg1
#endif
.endm

/* R0: new task */
OsStartToRun:

    MSR     CPSR_c, #(CPSR_INT_DISABLE | CPSR_SVC_MODE)  @禁止中断并切到管理模式

    LDRH    R1, [R0, #4] 	@将存储器地址为R0+4 的低16位数据读入寄存器R1，并将R1 的高16 位清零
    ORR     R1, #OS_TASK_STATUS_RUNNING @或指令 R1=R1|OS_TASK_STATUS_RUNNING
    STRH    R1, [R0, #4] 	@将寄存器R1中的低16位写入以R0+4地址的存储器中

    /* R0 is new task, save it on tpidrprw */
    MCR     p15, 0, R0, c13, c0, 4 @ C13=R0
    ISB 	@指令同步屏障，清除流水线并且确保在新指令执行时，之前的指令都已经执行完毕。

    VPUSH   {S0}                                     /* fpu */
    VPOP    {S0}
    VPUSH   {D0}
    VPOP    {D0}

    B       OsTaskContextLoad @加载任务的上下文

/*
 * R0: new task
 * R1: run task
 */
OsTaskSchedule:			/*任务调度,OsTaskSchedule的目的是将寄存器值按TaskContext的格式保存起来*/
    MRS      R2, CPSR 	/*MRS 指令用于将特殊寄存器(如 CPSR 和 SPSR)中的数据传递给通用寄存器，要读取特殊寄存器的数据只能使用 MRS 指令*/
    STMFD    SP!, {LR}	/*返回地址入栈,对应TaskContext->PC(R15寄存器)*/
    STMFD    SP!, {LR}	/*再次入栈对应,对应TaskContext->LR(R14寄存器)*/
    /* jump sp */
    SUB      SP, SP, #4 /* 跳的目的是为了,对应TaskContext->SP(R13寄存器)*/
	
    /* push r0-r12*/
    STMFD    SP!, {R0-R12} 	 @对应TaskContext->R[GEN_REGS_NUM](R0~R12寄存器)。
    STMFD    SP!, {R2}		/*R2 入栈 对应TaskContext->regPSR*/

    /* 8 bytes stack align */
    SUB      SP, SP, #4		@栈对齐,对应TaskContext->resved

    /* save fpu registers */
    PUSH_FPU_REGS   R2	/*保存fpu寄存器*/

    /* store sp on running task */
    STR     SP, [R1] @在运行的任务栈中保存SP,即runTask->stackPointer = sp

OsTaskContextLoad: @加载上下文
    /* clear the flag of ldrex */ @LDREX 可从内存加载数据,如果物理地址有共享TLB属性，则LDREX会将该物理地址标记为由当前处理器独占访问，并且会清除该处理器对其他任何物理地址的任何独占访问标记。
    CLREX @清除ldrex指令的标记

    /* switch to new task's sp */
    LDR     SP, [R0] @ 即:sp = new task->stackPointer

    /* restore fpu registers */
    POP_FPU_REGS    R2 @恢复fpu寄存器,这里用了汇编宏R2是宏的参数

    /* 8 bytes stack align */
    ADD     SP, SP, #4 @栈对齐

    LDMFD   SP!, {R0}  @此时SP!位置保存的是CPSR的内容,弹出到R0
    MOV     R4, R0	@R4=R0,将CPSR保存在r4, 将在OsKernelTaskLoad中保存到SPSR 
    AND     R0, R0, #CPSR_MASK_MODE @R0 =R0&CPSR_MASK_MODE ,目的是清除高16位
    CMP     R0, #CPSR_USER_MODE @R0 和 用户模式比较
    BNE     OsKernelTaskLoad @非用户模式则跳转到OsKernelTaskLoad执行,跳出

#ifdef LOSCFG_KERNEL_SMP
#ifdef LOSCFG_KERNEL_SMP_LOCKDEP
    /* 8 bytes stack align */
    SUB     SP, SP, #4 @sp = sp -4
    LDR     R0, =g_taskSpin @R0 = g_taskSpin
    BL      OsLockDepCheckOut @带链接的跳转指令。指令将下一条指令的地址拷贝到R14(即LR)链接寄存器中，然后跳转到指定地址运行程序
    ADD     SP, SP, #4 @sp=sp+4
#endif
    /* R0 = &g_taskSpin.rawLock */
    LDR     R0, =g_taskSpin @ R0 = &g_taskSpin.rawLock 将作为 ArchSpinUnlock的参数
    BL      ArchSpinUnlock 	@释放自旋锁g_taskSpin
    LDR     R2, =g_percpu 
    MRC     P15, 0, R3, C0, C0, 5 @获取CPUID给R3
    UXTB    R3, R3 @字节被无符号扩展到32位（高24位清0）
    MOV     R1, #OS_PERCPU_STRUCT_SIZE @R1 = sizeof(Percpu)
    MLA     R3, R1, R3, R2 @R3=R1*R3 
    MOV     R2, #0
    STR     R2, [R3, #OS_PERCPU_TASK_LOCK_OFFSET]
#endif

    MVN     R3, #CPSR_INT_DISABLE @按位取反 R3 = 0x3F
    AND     R4, R4, R3		@使能中断
    MSR     SPSR_cxsf, R4	@修改spsr值

    /* restore r0-r12, lr */
    LDMFD   SP!, {R0-R12}	@恢复寄存器值
    LDMFD   SP, {R13, R14}^	@恢复SP和LR的值,注意此时SP值已经变了,CPU换地方上班了.
    ADD     SP, SP, #(2 * 4)@sp = sp + 8
    LDMFD   SP!, {PC}^		@恢复PC寄存器值,如此一来 SP和PC都有了新值,完成了上下文切换.完美!

OsKernelTaskLoad: 			@内核任务的加载
    MSR     SPSR_cxsf, R4 	@将R4整个写入到程序状态保存寄存器
    /* restore r0-r12, lr */
    LDMFD   SP!, {R0-R12} 	@出栈,依次保存到 R0-R12,其实就是恢复现场
    ADD     SP, SP, #4 		@sp=SP+4
    LDMFD   SP!, {LR, PC}^ 	@返回地址赋给pc指针,直接跳出.

OsIrqHandler:	@硬中断处理,此时已切换到硬中断栈
    SUB     LR, LR, #4

    /* push r0-r3 to irq stack */
    STMFD   SP, {R0-R3}		@r0-r3寄存器入 irq 栈
    SUB     R0, SP, #(4 * 4)@r0 = sp - 16
    MRS     R1, SPSR		@获取程序状态控制寄存器
    MOV     R2, LR			@r2=lr

    /* disable irq, switch to svc mode */@超级用户模式(SVC 模式)，主要用于 SWI(软件中断)和 OS(操作系统)。
    CPSID   i, #0x13				@切换到SVC模式,此处一切换,后续指令将入SVC的栈
									@CPSID i为关中断指令,对应的是CPSIE
    /* push spsr and pc in svc stack */
    STMFD   SP!, {R1, R2} @实际是将 SPSR,和LR入栈,入栈顺序为 R1,R2,SP自增
    STMFD   SP, {LR}	  @LR再入栈,SP不自增

    AND     R3, R1, #CPSR_MASK_MODE	@获取CPU的运行模式
    CMP     R3, #CPSR_USER_MODE		@中断是否发生在用户模式
    BNE     OsIrqFromKernel			@中断不发生在用户模式下则跳转到OsIrqFromKernel

    /* push user sp, lr in svc stack */
    STMFD   SP, {R13, R14}^ 		@sp和LR入svc栈

OsIrqFromKernel:	@从内核发起中断
    /* from svc not need save sp and lr */@svc模式下发生的中断不需要保存sp和lr寄存器值
    SUB     SP, SP, #(2 * 4)	@申请8个栈空间

    /* pop r0-r3 from irq stack*/
    LDMFD   R0, {R0-R3}		    @从R0位置依次出栈

    /* push caller saved regs as trashed regs in svc stack */
    STMFD   SP!, {R0-R3, R12}	@R0~R3入栈,R12入栈

    /* 8 bytes stack align */
    SUB     SP, SP, #4			@栈对齐

    /*
     * save fpu regs in case in case those been
     * altered in interrupt handlers.
     */
    PUSH_FPU_REGS   R0 @保存fpu regs，以防中断处理程序中的fpu regs被修改。
#ifdef LOSCFG_IRQ_USE_STANDALONE_STACK @是否使用了独立的IRQ栈
    PUSH    {R4}	@R4先入栈保存,接下来要切换栈,需保存现场
    MOV     R4, SP	@R4=SP
    EXC_SP_SET __svc_stack_top, OS_EXC_SVC_STACK_SIZE, R1, R2 @切换到svc栈
#endif
	/*BLX 带链接和状态切换的跳转*/
    BLX     HalIrqHandler /* 调用硬中断处理程序,无参 ,说明HalIrqHandler在svc栈中执行 */

#ifdef LOSCFG_IRQ_USE_STANDALONE_STACK @是否使用了独立的IRQ栈
    MOV     SP, R4	@恢复现场,sp = R4 
    POP     {R4}	@弹出R4
#endif

    /* process pending signals */ 	@处理挂起信号
    BL      OsTaskProcSignal 		@跳转至C代码 

    /* check if needs to schedule */@检查是否需要调度
    CMP     R0, #0	@是否需要调度,R0为参数保存值
    BLNE    OsSchedPreempt @不相等,即R0非0,一般是 1

    MOV     R0,SP	@ OsSaveSignalContextIrq 参数1来源
    MOV     R1,R7	@ OsSaveSignalContextIrq 参数2来源
    BL      OsSaveSignalContextIrq @跳转至C代码 

    /* restore fpu regs */
    POP_FPU_REGS    R0 @恢复fpu寄存器值

    ADD     SP, SP, #4 @sp = sp + 4 

OsIrqContextRestore:	@恢复硬中断环境
    LDR     R0, [SP, #(4 * 7)]	@读取 SP+28 位置数据给R0
    MSR     SPSR_cxsf, R0		@恢复spsr
    AND     R0, R0, #CPSR_MASK_MODE @掩码找出当前工作模式
    CMP     R0, #CPSR_USER_MODE	@是否为用户模式?

    LDMFD   SP!, {R0-R3, R12}	@从SP位置依次出栈

    BNE     OsIrqContextRestoreToKernel @CPU工作在非用户模式

    /* load user sp and lr, and jump cpsr */
    LDMFD   SP, {R13, R14}^
    ADD     SP, SP, #(3 * 4)

    /* return to user mode */
    LDMFD   SP!, {PC}^ @回到用户模式

OsIrqContextRestoreToKernel:
    /* svc mode not load sp */
    ADD     SP, SP, #4
    LDMFD   SP!, {LR}
    /* jump cpsr and return to svc mode */
    ADD     SP, SP, #4
    LDMFD   SP!, {PC}^

FUNCTION(ArchSpinLock)	@非要拿到锁
	mov 	r1, #1		@r1=1
1:						@循环的作用,因SEV是广播事件.不一定lock->rawLock的值已经改变了
	ldrex	r2, [r0]	@r0 = &lock->rawLock, 即 r2 = lock->rawLock
	cmp 	r2, #0		@r2和0比较
	wfene				@不相等时,说明资源被占用,CPU核进入睡眠状态
	strexeq r2, r1, [r0]@此时CPU被重新唤醒,尝试令lock->rawLock=1,成功写入则r2=0
	cmpeq	r2, #0		@再来比较r2是否等于0,如果相等则获取到了锁
	bne 	1b			@如果不相等,继续进入循环
	dmb 				@用DMB指令来隔离，以保证缓冲中的数据已经落实到RAM中
	bx		lr			@此时是一定拿到锁了,跳回调用ArchSpinLock函数



FUNCTION(ArchSpinTrylock)	@尝试拿锁
	mov 	r1, #1			@r1=1
	mov 	r2, r0			@r2 = r0	   
	ldrex	r0, [r2]		@r2 = &lock->rawLock, 即 r0 = lock->rawLock
	cmp 	r0, #0			@r0和0比较
	strexeq r0, r1, [r2]	@尝试令lock->rawLock=1,成功写入则r0=0,否则 r0 =1
	dmb 					@数据存储隔离，以保证缓冲中的数据已经落实到RAM中
	bx		lr				@跳回调用ArchSpinLock函数



FUNCTION(ArchSpinUnlock)	@释放锁
	mov 	r1, #0			@r1=0				
	dmb 					@数据存储隔离，以保证缓冲中的数据已经落实到RAM中
	str 	r1, [r0]		@令lock->rawLock = 0
	dsb 					@数据同步隔离
	sev 					@给各CPU广播事件,唤醒沉睡的CPU们
	bx		lr				@跳回调用ArchSpinLock函数


