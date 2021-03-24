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

    .extern   g_losTask
    .extern   g_intCount
    .extern   g_curNestCount
    .extern   OsExcHandleEntry 		@异常处理入口		
    .extern   __svc_stack_top		@管理栈顶位置 操作系统使用的保护模式
    .extern   __exc_stack_top		@任务栈顶位置
    .extern   __stack_chk_guard
    .extern   OsRandomStackGuard
#ifdef LOSCFG_GDB
    .extern   OsUndefIncExcHandleEntry
#if __LINUX_ARM_ARCH__ >= 7
    .extern   OsPrefetchAbortExcHandleEntry
    .extern   OsDataAbortExcHandleEntry
#endif
#endif
    .extern OsArmSharedPageFault
    .extern OsArmA32SyscallHandle 
    .extern LOS_Exit

    .global   _osExceptFiqHdl
    .global   _osExceptAddrAbortHdl
    .global   _osExceptDataAbortHdl
    .global   _osExceptPrefetchAbortHdl
    .global   _osExceptSwiHdl
    .global   _osExceptUndefInstrHdl
    .global   __stack_chk_guard_setup


    .fpu vfpv4

.macro PUSH_FPU_REGS reg1
#if !defined(LOSCFG_ARCH_FPU_DISABLE)
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

.macro POP_FPU_REGS reg1
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

#ifdef LOSCFG_GDB
.macro GDB_HANDLE fun
    SUB     SP, SP, #12

    STMFD   SP!, {R0-R12}
    MRS     R1, SPSR
    STMFD   SP!, {R1}  @save spsr

    ADD     R0, SP, #14 * 4
    MOV     R3, LR  @save pc

    MRS     R1, CPSR
    MRS     R2, SPSR
    MOV     R4, SP

    ORR     R2, R2, #(CPSR_INT_DISABLE)
    MSR     CPSR_c, R2

    STR     SP, [R0]  @SP
    STR     LR, [R0, #4]  @LR
    STR     R3, [R0, #8]  @PC

    ORR     R1, R1, #(CPSR_INT_DISABLE)
    BIC     R1, R1, #OS_PSR_THUMB
    MSR     CPSR_c, R1
    MOV     R0, R4

    BL \fun

    ADD     SP, SP, #4
    LDMFD   SP!, {R0-R12}

    MOV     R0, SP
    ADD     SP, SP, #8

    LDR     R1, [R0, #8]  @get pc
    STMFD   SP!, {R1}

    AND     R1, R1, #0x03
    CMP     R1, #0
    BEQ     1f
    LDR     R1, [R0, #-14 * 4]
    ORR     R1, R1, #OS_PSR_THUMB
    B       2f
1:
    LDR     R1, [R0, #-14 * 4]

2:
    MSR     SPSR, R1

    LDR     R1, [R0, #-12 * 4]  @get R1
    STMFD   SP!, {R1}
    LDR     R1, [R0,#-13 * 4]  @get R0
    STMFD   SP!, {R1}

    LDMFD   SP!, {R0-R1, PC}^
.endm
#endif

@ Description: Stack-Protector Init @初始化堆栈保护器
__stack_chk_guard_setup:
    PUSH    {FP, LR}	@FP,LR入栈
    BL      OsRandomStackGuard
    LDR     R1, =__stack_chk_guard @0xd00a0dff
    MOV     R3, R0
    ORR     R2, R3, #0X80000000
    STR     R2, [R1]
    POP     {FP, PC}

@ Description: Undefined instruction exception handler
_osExceptUndefInstrHdl:@出现未定义的指令处理
#ifdef LOSCFG_GDB
    GDB_HANDLE OsUndefIncExcHandleEntry
#else
                                                              @ LR offset to return from this exception:  0.
    STMFD   SP, {R0-R7}                                       @ Push working registers, but don`t change SP.

    MOV     R0, #OS_EXCEPT_UNDEF_INSTR                        @ Set exception ID to OS_EXCEPT_UNDEF_INSTR.

    B       _osExceptDispatch                                 @ Branch to global exception handler.

#endif
/*
STMIB（地址先增而后完成操作）、STMFA（满递增堆栈）；
STMIA（完成操作而后地址递增）、STMEA（空递增堆栈）；
STMDB（地址先减而后完成操作）、STMFD（满递减堆栈）；
STMDA（完成操作而后地址递减）、STMED（空递减堆栈）。
*/

@ Description: Software interrupt exception handler
_osExceptSwiHdl: @软中断异常处理
	@保存任务上下文(TaskContext) 开始... 一定要对照TaskContext来理解
    SUB     SP, SP, #(4 * 16)	@先申请16个栈空间用于处理本次软中断
    STMIA   SP, {R0-R12}		@TaskContext.R[GEN_REGS_NUM] STMIA从左到右执行,先放R0 .. R12
    MRS     R3, SPSR			@读取本模式下的SPSR值
    MOV     R4, LR				@保存回跳寄存器LR

    AND     R1, R3, #CPSR_MASK_MODE                          @ Interrupted mode 获取中断模式
    CMP     R1, #CPSR_USER_MODE                              @ User mode	是否为用户模式
    BNE     OsKernelSVCHandler                               @ Branch if not user mode 非用户模式下跳转
	@ 当为用户模式时,获取SP和LR寄出去值
    @ we enter from user mode, we need get the values of  USER mode r13(sp) and r14(lr).
    @ stmia with ^ will return the user mode registers (provided that r15 is not in the register list).
    MOV     R0, SP											 @获取SP值,R0将作为OsArmA32SyscallHandle的参数
    STMFD   SP!, {R3}                                        @ Save the CPSR 入栈保存CPSR值 => TaskContext.regPSR
    ADD     R3, SP, #(4 * 17)                                @ Offset to pc/cpsr storage 跳到PC/CPSR存储位置
    STMFD   R3!, {R4}                                        @ Save the CPSR and r15(pc) 保存LR寄存器 => TaskContext.PC
    STMFD   R3, {R13, R14}^                                  @ Save user mode r13(sp) and r14(lr) 从右向左 保存 => TaskContext.LR和SP
    SUB     SP, SP, #4										 @ => TaskContext.resved
    PUSH_FPU_REGS R1	@保存中断模式(用户模式模式)											
	@保存任务上下文(TaskContext) 结束
    MOV     FP, #0                                           @ Init frame pointer
    CPSIE   I	@开中断,表明在系统调用期间可响应中断
    BLX     OsArmA32SyscallHandle	/*交给C语言处理系统调用*/
    CPSID   I	@执行后续指令前必须先关中断
	@恢复任务上下文(TaskContext) 开始
    POP_FPU_REGS R1											 @弹出FP值给R1
    ADD     SP, SP,#4										 @ 定位到保存旧SPSR值的位置
    LDMFD   SP!, {R3}                                        @ Fetch the return SPSR 弹出旧SPSR值
    MSR     SPSR_cxsf, R3                                    @ Set the return mode SPSR 恢复该模式下的SPSR值

    @ we are leaving to user mode, we need to restore the values of USER mode r13(sp) and r14(lr).
    @ ldmia with ^ will return the user mode registers (provided that r15 is not in the register list)

    LDMFD   SP!, {R0-R12}									 @恢复R0-R12寄存器
    LDMFD   SP, {R13, R14}^                                  @ Restore user mode R13/R14 恢复用户模式的R13/R14寄存器
    ADD     SP, SP, #(2 * 4)								 @定位到保存旧PC值的位置
    LDMFD   SP!, {PC}^                                       @ Return to user 切回用户模式运行
	@恢复任务上下文(TaskContext) 结束

OsKernelSVCHandler:@主要目的是保存ExcContext中除(R0~R12)的其他寄存器
    ADD     R0, SP, #(4 * 16)	@跳转到保存PC,LR,SP的位置,此时R0位置刚好是SP的位置
    MOV     R5, R0				@由R5记录SP位置,因为R0要暂时充当SP寄存器来使用
    STMFD   R0!, {R4}                                        @ Store PC => ExcContext.PC
    STMFD   R0!, {R4}										 @ 相当于保存了=>  ExcContext.LR
    STMFD   R0!, {R5}										 @ 相当于保存了=>  ExcContext.SP

    STMFD   SP!, {R3}                                        @ Push task`s CPSR (i.e. exception SPSR). =>ExcContext.regPSR
    SUB     SP, SP, #(4 * 2)                                 @ user sp and lr => =>ExcContext.USP,ULR

    MOV     R0, #OS_EXCEPT_SWI                               @ Set exception ID to OS_EXCEPT_SWI.
															 @ 设置异常ID为软中断
    B       _osExceptionSwi                                  @ Branch to global exception handler.
															 @ 跳到全局异常处理
@ Description: Prefectch abort exception handler
_osExceptPrefetchAbortHdl: @预取异常处理
#ifdef LOSCFG_GDB
#if __LINUX_ARM_ARCH__ >= 7
    GDB_HANDLE OsPrefetchAbortExcHandleEntry
#endif
#else
    SUB     LR, LR, #4                                       @ LR offset to return from this exception: -4.
    STMFD   SP, {R0-R7}                                      @ Push working registers, but don`t change SP.
    MOV     R5, LR
    MRS     R1, SPSR

    MOV     R0, #OS_EXCEPT_PREFETCH_ABORT                    @ Set exception ID to OS_EXCEPT_PREFETCH_ABORT.

    AND     R4, R1, #CPSR_MASK_MODE                          @ Interrupted mode
    CMP     R4, #CPSR_USER_MODE                              @ User mode
    BEQ     _osExcPageFault                                  @ Branch if user mode

_osKernelExceptPrefetchAbortHdl:
    MOV     LR, R5
    B       _osExceptDispatch                                @ Branch to global exception handler.
#endif

@ Description: Data abort exception handler
_osExceptDataAbortHdl: @数据异常处理,缺页就属于数据异常
#ifdef LOSCFG_GDB
#if __LINUX_ARM_ARCH__ >= 7
    GDB_HANDLE OsDataAbortExcHandleEntry
#endif
#else
    SUB     LR, LR, #8                                       @ LR offset to return from this exception: -8.
    STMFD   SP, {R0-R7}                                      @ Push working registers, but don`t change SP.
    MOV     R5, LR
    MRS     R1, SPSR

    MOV     R0, #OS_EXCEPT_DATA_ABORT                        @ Set exception ID to OS_EXCEPT_DATA_ABORT.

    B     _osExcPageFault @跳到缺页异常处理
#endif

@ Description: Address abort exception handler
_osExceptAddrAbortHdl: @地址异常处理
    SUB     LR, LR, #8                                       @ LR offset to return from this exception: -8.
    STMFD   SP, {R0-R7}                                      @ Push working registers, but don`t change SP.

    MOV     R0, #OS_EXCEPT_ADDR_ABORT                        @ Set exception ID to OS_EXCEPT_ADDR_ABORT.

    B       _osExceptDispatch                                @ Branch to global exception handler.

@ Description: Fast interrupt request exception handler
_osExceptFiqHdl: @快中断异常处理
    SUB     LR, LR, #4                                       @ LR offset to return from this exception: -4.
    STMFD   SP, {R0-R7}                                      @ Push working registers.
															 @ R0~R7入栈保存
    MOV     R0, #OS_EXCEPT_FIQ                               @ Set exception ID to OS_EXCEPT_FIQ.
															 @ 设置参数异常类型,将作为参数传给_osExceptDispatch
    B       _osExceptDispatch                                @ Branch to global exception handler.

_osExcPageFault:  @缺页中断处理函数 
    SUB     R3, SP, #(8 * 4)                                 @ Save the start address of working registers.
    MSR     CPSR_c, #(CPSR_INT_DISABLE | CPSR_SVC_MODE)      @ Switch to SVC mode, and disable all interrupts
    MOV     R2, SP
    @按 ExcContext 格式开始保存现场 因为 OsArmSharedPageFault 第二个参数就是 ExcContext
    STMFD   SP!, {R5}                                        @ Push original PC
    STMFD   SP!, {LR}                                        @ Push original svc LR
    STMFD   SP!, {R2}                                        @ Push original svc SP
    STMFD   SP!, {R8-R12}                                    @ Push original R12-R8,
    LDMFD   R3!, {R4-R11}                                    @ Move original R7-R0 from exception stack to original stack.
    STMFD   SP!, {R4-R11}
    STMFD   SP!, {R1}
    SUB     SP, SP, #8
    STMIA   SP, {R13, R14}^                                  @ Save user mode r13(sp) and r14(lr)
	@按ExcContext格式完成保存现场
    MOV     R4, SP @R4指向SP即 ExcContext开始位置
    BIC     SP, SP, #7
    PUSH_FPU_REGS R1

    CMP     R0, #OS_EXCEPT_DATA_ABORT @从正确的地址中取数据发生异常
    BNE     1f
    MRC     P15, 0, R2, C6, C0, 0 	@参数3 UINT32 far 异常地址寄存器(Fault Address Register -FSR)
    MRC     P15, 0, R3, C5, C0, 0	@参数4 UINT32 fsr 异常状态寄存器(Fault Status Register -FSR)
    B       2f
1:  MRC     P15, 0, R2, C6, C0, 2
    MRC     P15, 0, R3, C5, C0, 1

2:  MOV     R1, R4 @参数R1 即 ExcContext开始位置
    MOV     R5, R0
    MOV     R8, R2
    MOV     R9, R3
    CPSIE   I @禁止中断
    BLX     OsArmSharedPageFault @缺页中断处理参数(UINT32 excType, ExcContext *frame, UINT32 far, UINT32 fsr)
    CPSID   I @恢复中断

    POP_FPU_REGS R1
    MOV     SP, R4
    CMP     R0, #0
    BEQ     _OsExcReturn @异常返回

    MOV     R0, R5                                           @ exc type
    B       _osExceptionSwi 		@跳到软中断执行,系统调用就是通过软中断实现

@ Description: Exception handler
@ Parameter  : R0     Exception Type 异常类型
@ Regs Hold  : R3     Exception`s CPSR 发生异常时的CPSR值 @note_thinking 鸿蒙官方的注释错了,应该是Regs Hold: R2
_osExceptDispatch: @处理异常分发
    MRS     R2, SPSR                                         @ Save CPSR before exception.先用R2保存CPSR
    MOV     R1, LR                                           @ Save PC before exception.  在用R1保存PC寄存器
    SUB     R3, SP, #(8 * 4)                                 @ Save the start address of working registers.
															 @ 保存工作寄存器的起始地址,R0~R7八个寄存器
    MSR     CPSR_c, #(CPSR_INT_DISABLE | CPSR_SVC_MODE)      @ Switch to SVC mode, and disable all interrupts
    														 @ 切换到SVC模式,屏蔽掉所有中断
    MOV     R5, SP											 @ R5 = SP ,保存SP位置
    EXC_SP_SET __exc_stack_top, OS_EXC_STACK_SIZE, R6, R7	 @ 切换到当前CPU的SVC模式栈处理,R6,R7用于记录CPUid和偏移量
	@开始 保存异常上下文(ExcContext)值, 顺序是 USP,ULR,SPSR,R0~R15
    STMFD   SP!, {R1}                                        @ Push Exception PC 保存上一个工作模式的PC => ExcContext.PC
    STMFD   SP!, {LR}                                        @ Push SVC LR	保存上一个工作模式的LR => ExcContext.LR
    STMFD   SP!, {R5}                                        @ Push SVC SP  保存上一个工作模式的SP => ExcContext.SP
    STMFD   SP!, {R8-R12}                                    @ Push original R12-R8,保存上一个工作模式的R8-R12 => ExcContext.R8 - R12
    LDMFD   R3!, {R4-R11}                                    @ Move original R7-R0 from exception stack to original stack.
    														 @ 将保存在上一个工作模式的R0~R7取出
    STMFD   SP!, {R4-R11}								     @ 将上一个工作模式的R0~R7保存到新的栈中 => ExcContext.R0 - R7
    STMFD   SP!, {R2}                                        @ Push task`s CPSR (i.e. exception SPSR). => ExcContext.regCPSR
	@除了ExcContext.USP,ULR两个值,保存异常上下文(ExcContext)其他值完成.							 
    CMP     R0, #OS_EXCEPT_DATA_ABORT 		@是数据异常吗?
    BNE     1f 								@不是跳到 锚点1处
    MRC     P15, 0, R8, C6, C0, 0 			@R8=C6(内存失效的地址) 0(访问数据失效)
    MRC     P15, 0, R9, C5, C0, 0 			@R9=C5(内存失效的状态) 0(无效整个指令cache)
    B       3f 								@跳到锚点3处执行
1:  CMP     R0, #OS_EXCEPT_PREFETCH_ABORT 	@是预取异常吗?
    BNE     2f 								@不是,跳到 锚点2: 处运行
    MRC     P15, 0, R8, C6, C0, 2 			@R8=C6(内存失效的地址) 2(访问指令失效)
    MRC     P15, 0, R9, C5, C0, 1 			@R9=C5(内存失效的状态) 1(虚拟地址)
    B       3f 								@直接跳到 锚点3: 处执行
2:  MOV     R8, #0							@R8=0
    MOV     R9, #0							@R9=0
				@可看出异常过后,R8,R9的值发生变化,获取协处理对应的数据.
3:  AND     R2, R2, #CPSR_MASK_MODE			@获取当前工作模式 
    CMP     R2, #CPSR_USER_MODE             @ User mode 是否为用户模式
    BNE     4f 								@不是用户模式,跳到 锚点4: 处运行
    STMFD   SP, {R13, R14}^                 @ save user mode sp and lr 保存用户模式的SP和LR
4:											@保存 => ExcContext.USP,ULR
    SUB     SP, SP, #(4 * 2) 				@非用户模式下不需要保存 USP,ULR,所以跳过2个空间
	@填充好ExcContext.USP,ULR两个值,R8,R9 获得异常的信息,将接着往下执行
_osExceptionSwi: 				@异常软处理
    MOV     R1, SP                                            @ The second argument to the exception
															  @ 异常的第二个参数,第一个参数是R0
    MRC     P15, 0, R4, C0, C0, 5							  @ R4获取当前cpu id
    AND     R4, R4, #MPIDR_CPUID_MASK                         @ Get Current cpu id
    LSL     R2, R4, #2			@(Logic Shift Left)逻辑左移指令		R2 = R4<<2	@note_why 没看明白这句话的含义							 
    LDR     R3, =g_curNestCount                               @ if(g_curNestCount > 0) dump to _osExceptionGetSP 嵌套异常
    							@将g_curNestCount的地址存入R3
    ADD     R3, R3, R2			@ R3 = R3 + R2,
    LDR     R4, [R3]			@ R4 = *R3

    CMP     R4, #0				@R4 和 0对比
    BNE     _osExceptionGetSP	@(g_curNestCount > 0)说明有嵌套异常,必须先处理异常
	@判断异常发生在任务堆栈或系统堆栈中
    LDR     R3, =g_intCount                                   @ Judge the exception is occur in task stack or system stack
    ADD     R3, R3, R2
    LDR     R4, [R3] @获取g_intCount[ArchCurrCpuid()]的值
	@软中断的优先级要低于硬中断,这里判断是否有硬中断发生,有则需先处理硬中断
    CMP     R4, #0                                            @ if (g_intCount[ArchCurrCpuid()] > 0) 当前有中断要处理
    BNE     _osExceptionGetSP                                 @ can not switch svc stack 有中断要处理,必须先处理异常
	@切换到SVC栈中处理未定义的异常
    EXC_SP_SET __svc_stack_top, OS_EXC_SVC_STACK_SIZE, R6, R7 @ Switch to unified exception stack.
    ADD     R4, R4, #1
    STR     R4, [R3]

_osExceptionGetSP:@处理异常 R1=sp
    MOV     R2, R8                                            @ far 从CP15的c6获取 异常地址寄存器(Fault Address Register -FSR)
    MOV     R3, R9                                            @ fsr 从CP15的c5获取 异常状态寄存器(Fault Status Register -FSR)
    LDR     R5, =OsExcHandleEntry                             @ OsExcHandleEntry(UINT32 excType, ExcContext * excBufAddr,UINT32 far, UINT32 fsr)
    BX      R5						@LDR为加载指令把OsExcHandleEntry的地址放入R5，BX为带分支的跳转指令,去执行OsExcHandleEntry						
	
_OsExcReturn:@异常返回
    LDR     R0, [SP, #(2 * 4)]
    AND     R0, R0,  #CPSR_MASK_MODE
    CMP     R0, #CPSR_USER_MODE                               @ User mode
    BNE     _OsExcReturnToKernel 	@非用户模式跳转执行
    LDMFD   SP, {R13, R14}^                                   @ load user mode sp and lr												  
															  @恢复sp和lr的值 < = ExcContext.USP,ULR 回到用户栈继续运行
_OsExcReturnToKernel: @内核模式的异常返回
    ADD     SP, SP, #(2 * 4)								  @先跳开ExcContext.USP和ULR
    LDMFD   SP!, {R1}										  @取出R1 = ExcContext.SPSR
    MSR     SPSR_cxsf, R1                                     @ Set the return mode SPSR < = ExcContext.regCPSR
    LDMFD   SP!, {R0-R12}									  @依次恢复 R0~R12 < = ExcContext.R0 - R12
    ADD     SP, SP, #4										  @跳过ExcContext.SP
    LDMFD   SP!, {LR, PC}^									  @恢复LR和PC的值 < = ExcContext.LR ,PC

    .end
