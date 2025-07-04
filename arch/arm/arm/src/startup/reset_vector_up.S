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

#define  ASSEMBLY
#include "arch_config.h"
#include "los_vm_boot.h"
#include "los_vm_zone.h"
#include "los_mmu_descriptor_v6.h"
#undef ASSEMBLY

/*!
* @brief
* @verbatim 
    单CPU下异常向量表

    指令名		功能描述
    DMB		数据存储器隔离。DMB 指令保证： 仅当所有在它前面的存储器访问操作
            都执行完毕后，才提交(commit)在它后面的存储器访问操作。
            
    DSB		数据同步隔离。比 DMB 严格： 仅当所有在它前面的存储器访问操作
            都执行完毕后，才执行在它后面的指令（亦即任何指令都要等待存储器访问操作）
            
    ISB		指令同步隔离。最严格：它会清洗流水线，以保证所有它前面的指令都执
            行完毕之后，才执行它后面的指令。
* @endverbatim 
*/
    .global __exc_stack_top @ 声明全局符号 __exc_stack_top，注意这个 top 表示栈的地址高位，即栈底
    .global __svc_stack_top @ 声明全局符号 __svc_stack_top
    .global __exc_stack @ 声明全局符号 __exc_stack
    .global __svc_stack @ 声明全局符号 __svc_stack

    .extern __bss_start
    .extern __bss_end
    .extern hal_clock_initialize_start
    .extern _osExceptFiqHdl
    .extern _osExceptAddrAbortHdl
    .extern _osExceptDataAbortHdl
    .extern _osExceptPrefetchAbortHdl
    .extern _osExceptSwiHdl
    .extern _osExceptUndefInstrHdl
    .extern __stack_chk_guard_setup
    .extern g_firstPageTable
    .extern g_mmuJumpPageTable
    .extern g_archMmuInitMapping

    .equ MPIDR_CPUID_MASK, 0xffU @ 定义常量 MPIDR_CPUID_MASK 为 0xffU

    .fpu neon-vfpv4 @ 指定使用 neon-vfpv4 浮点运算单元
    .syntax unified @ 指定使用统一的汇编语法
    .arch armv7-a @ 指定使用 ARMv7-A 架构
    .arm @ 指定使用 ARM 指令集
/* 设置异常模式栈的SP 参数1为栈底, 参数2为栈大小 r11 存放的是 cpu id */ @ 设置异常模式栈的栈指针，参数 1 为栈底，参数 2 为栈大小，r11 存 CPU ID
/* param0 is stack bottom, param1 is stack size, r11 hold cpu id */ @ 参数 0 是栈底，参数 1 是栈大小，r11 保存 CPU ID
.macro EXC_SP_SET param0, param1 @ 定义设置异常模式栈 SP 的宏
    ldr    r1, =\param0 @ r1 = 栈底地址
    mov    r0, \param1 @ r0 = 栈大小
    bl     sp_set @ 跳转到 sp_set 函数设置 SP
.endm

/* param0 is stack top, param1 is stack size, param2 is magic num */ @ 参数 0 是栈顶，参数 1 是栈大小，参数 2 是魔法数字
.macro STACK_MAGIC_SET param0, param1, param2 @ 定义设置栈魔法数字的宏
    ldr     r0, =\param0 @ r0 = 栈顶地址
    mov     r1, \param1 @ r1 = 栈大小
    ldr     r2, =\param2 @ r2 = 魔法数字
    bl      excstack_magic @ 跳转到 excstack_magic 函数设置魔法数字
.endm

    .code   32 @ 指定使用 32 位指令集
    .section ".vectors","ax" @ 定义 .vectors 代码段，具有可执行和可读写属性

__exception_handlers:
    /*
    *Assumption:  ROM code has these vectors at the hardware reset address. @ 假设：ROM 代码在硬件复位地址处有这些向量
    *A simple jump removes any address-space dependencies [i.e. safer] @ 简单跳转可消除任何地址空间依赖（更安全）
    */
    b   reset_vector @ 跳转到 reset_vector 处理硬件复位
    b   _osExceptUndefInstrHdl @ 跳转到 _osExceptUndefInstrHdl 处理未定义指令异常
    b   _osExceptSwiHdl @ 跳转到 _osExceptSwiHdl 处理软件中断异常
    b   _osExceptPrefetchAbortHdl @ 跳转到 _osExceptPrefetchAbortHdl 处理预取异常
    b   _osExceptDataAbortHdl @ 跳转到 _osExceptDataAbortHdl 处理数据异常
    b   _osExceptAddrAbortHdl @ 跳转到 _osExceptAddrAbortHdl 处理地址异常
    b   OsIrqHandler @ 跳转到 OsIrqHandler 处理普通中断
    b   _osExceptFiqHdl @ 跳转到 _osExceptFiqHdl 处理快速中断

    /* Startup code which will get the machine into supervisor mode */ @ 启动代码，将机器置于管理模式
    .global reset_vector @ 声明全局符号 reset_vector
    .type   reset_vector,function @ 指定 reset_vector 为函数类型
reset_vector: @ 单核 CPU 时，鸿蒙开机代码
    /* do some early cpu setup: i/d cache disable, mmu disabled */ @ 进行一些早期 CPU 设置：禁用指令/数据缓存和 MMU
    mrc     p15, 0, r0, c1, c0, 0 @ 从协处理器 15 的寄存器 c1, c0, 0 读取系统控制寄存器值到 r0
    bic     r0, #(1 << 12)          @ 清除 r0 中第 12 位，禁用指令缓存
    bic     r0, #(1 << 2)           @ 清除 r0 中第 2 位，禁用数据缓存
    bic     r0, #(1 << 0)           @ 清除 r0 中第 0 位，禁用 MMU
    mcr     p15, 0, r0, c1, c0, 0 @ 将修改后的 r0 值写回协处理器 15 的寄存器 c1, c0, 0

    /* enable fpu+neon */ @ 启用浮点运算单元和 NEON 指令集
#ifndef LOSCFG_TEE_ENABLE @ 如果未定义 LOSCFG_TEE_ENABLE
    MRC    p15, 0, r0, c1, c1, 2 @ 从协处理器 15 的寄存器 c1, c1, 2 读取浮点控制寄存器值到 r0
    ORR    r0, r0, #0xC00 @ r0 = r0 | 0xC00，启用浮点运算单元
    BIC    r0, r0, #0xC000 @ 清除 r0 中第 14、15 位
    MCR    p15, 0, r0, c1, c1, 2 @ 将修改后的 r0 值写回协处理器 15 的寄存器 c1, c1, 2

    LDR    r0, =(0xF << 20) @ 将立即数 0xF << 20 加载到 r0
    MCR    p15, 0, r0, c1, c0, 2 @ 将 r0 值写回协处理器 15 的寄存器 c1, c0, 2
    ISB @ 执行指令同步屏障
#endif
    MOV    r3, #0x40000000 @ 将立即数 0x40000000 赋给 r3
    VMSR   FPEXC, r3 @ 将 r3 的值写入浮点运算异常控制寄存器

    /* r11: delta of physical address and virtual address */ @ r11 存储物理地址和虚拟地址的偏移量
    adr     r11, pa_va_offset @ 获取 pa_va_offset 标签的地址到 r11
    ldr     r0, [r11] @ 从 r11 指向的地址加载值到 r0
    sub     r11, r11, r0 @ r11 = r11 - r0，计算物理地址和虚拟地址的偏移量

    /* if we need to relocate to proper location or not */ @ 判断是否需要重新定位到正确位置
    adr     r4, __exception_handlers            @ r4 = __exception_handlers 的加载地址
    ldr     r5, =SYS_MEM_BASE                   @ r5 = 物理地址基址
    subs    r12, r4, r5                         @ r12 = r4 - r5，计算加载地址和物理地址的偏移量
    beq     reloc_img_to_bottom_done            @ 若偏移量为 0，跳转到 reloc_img_to_bottom_done

    /* we need to relocate image at the bottom of physical address */ @ 需要将镜像重新定位到物理地址底部
    ldr     r7, =__exception_handlers           @ r7 = __exception_handlers 的链接地址
    ldr     r6, =__bss_start                    @ r6 = __bss_start 的链接地址
    sub     r6, r7                              @ r6 = r6 - r7，计算链接地址范围
    add     r6, r4                              @ r6 = r6 + r4，得到加载地址结束位置

reloc_img_to_bottom_loop:
    ldr     r7, [r4], #4 @ 从 r4 指向地址加载 4 字节数据到 r7，然后 r4 加 4
    str     r7, [r5], #4 @ 将 r7 的值存储到 r5 指向地址，然后 r5 加 4
    cmp     r4, r6 @ 比较 r4 和 r6 的值
    bne     reloc_img_to_bottom_loop @ 若不相等，跳转到 reloc_img_to_bottom_loop 继续复制
    sub     pc, r12 @ pc = pc - r12，跳转到正确地址
    nop @ 空操作
    sub     r11, r11, r12                       @ r11 = r11 - r12，更新物理地址和虚拟地址的偏移量

reloc_img_to_bottom_done:
#ifdef LOSCFG_KERNEL_MMU @ 如果定义了 LOSCFG_KERNEL_MMU
    ldr     r4, =g_firstPageTable               @ r4 = 一级页表的物理地址
    add     r4, r4, r11 @ r4 = r4 + r11，得到正确的物理地址
    mov     r0, r4 @ r0 = r4
    mov     r1, #0 @ r1 = 0
    mov     r2, #MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS @ r2 = 一级小页表项数量
    bl      memset_optimized                    @ 调用 memset_optimized 函数将页表清零

    ldr     r5, =g_archMmuInitMapping @ r5 = MMU 初始化映射函数地址
    add     r5, r5, r11 @ r5 = r5 + r11，得到正确地址
init_mmu_loop:
    ldmia   r5!, {r6-r10}                       @ 从 r5 指向地址加载 5 个寄存器值到 r6 - r10，r5 自增
    cmp     r8, 0                               @ 比较 r8 和 0，判断大小是否为 0
    beq     init_mmu_done @ 若为 0，MMU 初始化完成，跳转到 init_mmu_done
    bl      page_table_build @ 调用 page_table_build 函数构建页表
    b       init_mmu_loop @ 跳转到 init_mmu_loop 继续初始化
init_mmu_done:
    orr     r8, r4, #MMU_TTBRx_FLAGS            @ r8 = r4 | MMU_TTBRx_FLAGS，设置页表缓存属性
    ldr     r4, =g_mmuJumpPageTable             @ r4 = MMU 跳转页表的虚拟地址
    add     r4, r4, r11 @ r4 = r4 + r11，得到正确虚拟地址
    ldr     r4, [r4] @ 从 r4 指向地址加载值到 r4
    add     r4, r4, r11                         @ r4 = r4 + r11，得到正确物理地址

    /* build 1M section mapping, in order to jump va during turing on mmu:pa == pa, va == pa */ @ 构建 1M 段映射，便于开启 MMU 时跳转虚拟地址
    mov     r6, pc @ r6 = 当前程序计数器值
    mov     r7, r6                              @ r7 = r6，r7 保存物理地址（MB 对齐）
    lsr     r6, r6, #20                         @ r6 = r6 >> 20，得到虚拟地址一级页表索引
    ldr     r10, =MMU_DESCRIPTOR_KERNEL_L1_PTE_FLAGS @ r10 = 内核一级页表项标志
    add     r12, r10, r6, lsl #20               @ r12 = r10 + (r6 << 20)，得到物理地址 | 标志位
    str     r12, [r4, r7, lsr #(20 - 2)]        @ 跳转页表[物理地址索引] = 页表项
    rsb     r7, r11, r6, lsl #20                @ r7 = (r6 << 20) - r11，得到虚拟地址
    str     r12, [r4, r7, lsr #(20 - 2)]        @ 跳转页表[虚拟地址索引] = 页表项

    bl      mmu_setup                           @ 调用 mmu_setup 函数设置 MMU
#endif
    /* get cpuid and keep it in r11 */ @ 获取 CPU ID 并保存到 r11
    mrc     p15, 0, r11, c0, c0, 5 @ 从协处理器 15 的寄存器 c0, c0, 5 读取 MPIDR 寄存器值到 r11
    and     r11, r11, #MPIDR_CPUID_MASK @ r11 = r11 & MPIDR_CPUID_MASK，提取 CPU ID
    cmp     r11, #0 @ 比较 r11 和 0
    bne     excstatck_loop_done @ 若不为 0，跳转到 excstatck_loop_done

excstatck_loop:	@ 清除中断和异常堆栈，并设置魔法数字检查溢出
    /* clear out the interrupt and exception stack and set magic num to check the overflow */ @ 清除中断和异常堆栈，并设置魔法数字检查溢出
    ldr     r0, =__svc_stack @ r0 = __svc_stack 地址
    ldr     r1, =__exc_stack_top @ r1 = __exc_stack_top 地址
    bl      stack_init @ 调用 stack_init 函数初始化栈

    STACK_MAGIC_SET __svc_stack, #OS_EXC_SVC_STACK_SIZE, OS_STACK_MAGIC_WORD @ 设置 SVC 栈魔法数字
    STACK_MAGIC_SET __exc_stack, #OS_EXC_STACK_SIZE, OS_STACK_MAGIC_WORD @ 设置异常栈魔法数字

excstatck_loop_done:
warm_reset:
    /* initialize CPSR (machine state register) */ @ 初始化 CPSR 寄存器
    mov    r0, #(CPSR_IRQ_DISABLE|CPSR_FIQ_DISABLE|CPSR_SVC_MODE) @ r0 = 禁止中断并进入 SVC 模式的值
    msr    cpsr, r0 @ 将 r0 的值写入 CPSR 寄存器

    /* Note: some functions in LIBGCC1 will cause a "restore from SPSR"!! */ @ 注意：LIBGCC1 中的一些函数会导致“从 SPSR 恢复”
    msr    spsr, r0 @ 保存程序状态寄存器值到 SPSR，用于后续恢复
	@设置SVC栈,每个CPU都有自己的SVC栈 @ 设置 SVC 栈，每个 CPU 有自己的栈
    /* set svc stack, every cpu has OS_EXC_SVC_STACK_SIZE stack */ @ 设置 SVC 栈，每个 CPU 有 OS_EXC_SVC_STACK_SIZE 大小的栈
    ldr    r0, =__svc_stack_top @ r0 = __svc_stack_top 地址
    mov    r2, #OS_EXC_SVC_STACK_SIZE @ r2 = OS_EXC_SVC_STACK_SIZE
    mul    r2, r2, r11 @ r2 = OS_EXC_SVC_STACK_SIZE * r11（r11 存 CPU ID）
    sub    r0, r0, r2 @ r0 = r0 - r2，计算 SP 位置
    mov    sp, r0 @ 设置栈指针 SP

    /* enable fpu+neon */ @ 启用浮点运算单元和 NEON 指令集
    MRC    p15, 0, r0, c1, c1, 2 @ 从协处理器 15 的寄存器 c1, c1, 2 读取浮点控制寄存器值到 r0
    ORR    r0, r0, #0xC00 @ r0 = r0 | 0xC00，启用浮点运算单元
    BIC    r0, r0, #0xC000 @ 清除 r0 中第 14、15 位
    MCR    p15, 0, r0, c1, c1, 2 @ 将修改后的 r0 值写回协处理器 15 的寄存器 c1, c1, 2

    LDR    r0, =(0xF << 20) @ 将立即数 0xF << 20 加载到 r0
    MCR    p15, 0, r0, c1, c0, 2 @ 将 r0 值写回协处理器 15 的寄存器 c1, c0, 2

    MOV    r3, #0x40000000 @ 将立即数 0x40000000 赋给 r3
    VMSR   FPEXC, r3 @ 将 r3 的值写入浮点运算异常控制寄存器

    LDR    r0, =__exception_handlers @ r0 = __exception_handlers 地址
    MCR    p15, 0, r0, c12, c0, 0 @ 将 r0 的值写回协处理器 15 的寄存器 c12, c0, 0

clear_bss:
    ldr    r0, =__bss_start @ r0 = __bss_start 地址
    ldr    r2, =__bss_end @ r2 = __bss_end 地址
    mov    r1, #0 @ r1 = 0
    sub    r2, r2, r0 @ r2 = r2 - r0，计算 BSS 段长度
    bl     memset @ 调用 memset 函数将 BSS 段清零

#if defined(LOSCFG_CC_STACKPROTECTOR_ALL) || \
    defined(LOSCFG_CC_STACKPROTECTOR_STRONG) || \
    defined(LOSCFG_CC_STACKPROTECTOR) @ 如果定义了栈保护相关宏
    bl     __stack_chk_guard_setup @ 调用 __stack_chk_guard_setup 函数设置栈保护
#endif

#ifdef LOSCFG_GDB_DEBUG @ 如果定义了 LOSCFG_GDB_DEBUG
    /* GDB_START - generate a compiled_breadk,This function will get GDB stubs started, with a proper environment */ @ GDB_START 函数用于启动 GDB 调试，创建合适环境
    bl     GDB_START @ 调用 GDB_START 函数
    .word  0xe7ffdeff @ 存储立即数 0xe7ffdeff
#endif

    bl     main @ 跳转到 main 函数执行
_start_hang: @ 进入死循环
    b      _start_hang @ 跳转到 _start_hang 自身

/******************************************************************************
ARM协处理器CP15寄存器 @ ARM 协处理器 CP15 寄存器说明

MCR{cond}     coproc，opcode1，Rd，CRn，CRm，opcode2 @ MCR 指令格式
MRC {cond}    coproc，opcode1，Rd，CRn，CRm，opcode2 @ MRC 指令格式
coproc       指令操作的协处理器名.标准名为pn,n,为0~15 @ 协处理器名称，标准为 pn，n 范围 0 - 15
opcode1      协处理器的特定操作码. 对于CP15寄存器来说，opcode1永远为0，不为0时，操作结果不可预知 @ 协处理器操作码，CP15 寄存器中 opcode1 恒为 0
CRd          作为目标寄存器的协处理器寄存器. @ 目标协处理器寄存器
CRn          存放第1个操作数的协处理器寄存器. @ 存放第一个操作数的协处理器寄存器
CRm          存放第2个操作数的协处理器寄存器. （用来区分同一个编号的不同物理寄存器，当不需要提供附加信息时，指定为C0） @ 存放第二个操作数的协处理器寄存器，无附加信息时指定为 C0
opcode2      可选的协处理器特定操作码.                （用来区分同一个编号的不同物理寄存器，当不需要提供附加信息时，指定为0） @ 可选操作码，无附加信息时指定为 0

C2 :存储保护和控制 地址转换表基地址 @ C2 寄存器：存储保护和控制，地址转换表基地址
C3 :存储保护和控制 域访问控制位 @ C3 寄存器：存储保护和控制，域访问控制位
C7 :高速缓存和写缓存控制 @ C7 寄存器：高速缓存和写缓存控制
C8 :TLB 控制 @ C8 寄存器：TLB 控制
C9 :高速缓存锁定 @ C9 寄存器：高速缓存锁定
C10:TLB 锁定 @ C10 寄存器：TLB 锁定
******************************************************************************/
#ifdef LOSCFG_KERNEL_MMU @ 如果定义了 LOSCFG_KERNEL_MMU
mmu_setup: @ 安装 MMU
    mov     r12, #0 @ r12 = 0，为后续操作做准备，CP15 协处理器操作需通过寄存器赋值								
    mcr     p15, 0, r12, c8, c7, 0              @ 设置 C8、C7 = 0 控制 TLB，使映射无效
    isb @ 执行指令同步屏障

    mcr     p15, 0, r12, c2, c0, 2              @ 初始化 C2 寄存器
    isb @ 执行指令同步屏障

    orr     r12, r4, #MMU_TTBRx_FLAGS @ r12 = r4 | MMU_TTBRx_FLAGS，设置页表属性
    mcr     p15, 0, r12, c2, c0, 0              @ 设置临时页表属性
    isb @ 执行指令同步屏障

    mov     r12, #0x7                           @ r12 = 0x7（0b0111）
    mcr     p15, 0, r12, c3, c0, 0              @ 设置 DACR 寄存器为 0b0111，设置客户端和管理域
    isb @ 执行指令同步屏障

    mrc     p15, 0, r12, c1, c0, 0 @ 从协处理器 15 的寄存器 c1, c0, 0 读取系统控制寄存器值到 r12
    bic     r12, #(1 << 29 | 1 << 28)           @ 清除 r12 中第 28、29 位，禁用 TRE/AFE
    orr     r12, #(1 << 0)                      @ 设置 r12 中第 0 位，启用 MMU
    bic     r12, #(1 << 1) @ 清除 r12 中第 1 位
    orr     r12, #(1 << 2)                      @ 设置 r12 中第 2 位，启用数据缓存
    orr     r12, #(1 << 12)                     @ 设置 r12 中第 12 位，启用指令缓存
    mcr     p15, 0, r12, c1, c0, 0              @ 设置 SCTLR 寄存器，开启 MMU，启用 I/D 缓存，禁用 TRE/AFE
    isb @ 执行指令同步屏障

    ldr     pc,  =1f                            @ 跳转到标号 1，转换到虚拟地址
1:
    mcr     p15, 0, r8, c2, c0, 0               @ 跳转到 C2 寄存器保存的基地址（页表）
    isb @ 执行指令同步屏障

    mov     r12, #0 @ r12 = 0
    mcr     p15, 0, r12, c8, c7, 0 @ 设置 C8、C7 = 0 控制 TLB，使映射无效
    isb @ 执行指令同步屏障

    sub     lr,  r11                            @ lr = lr - r11，调整链接寄存器，考虑物理地址和虚拟地址偏移量
    bx      lr @ 跳转到链接寄存器 lr 指向的地址
#endif
    .code  32 @ 指定使用 32 位指令集

    .global reset_platform @ 声明全局符号 reset_platform
    .type   reset_platform,function @ 指定 reset_platform 为函数类型
reset_platform:
#ifdef A7SEM_HAL_ROM_MONITOR @ 如果定义了 A7SEM_HAL_ROM_MONITOR
    /* initialize CPSR (machine state register) */ @ 初始化 CPSR 寄存器
    mov    r0, #(CPSR_IRQ_DISABLE|CPSR_FIQ_DISABLE|CPSR_SVC_MODE) @ r0 = 禁止中断并进入 SVC 模式的值
    msr    cpsr, r0 @ 将 r0 的值写入 CPSR 寄存器
    b      warm_reset @ 跳转到 warm_reset
#else
    mov    r0, #0 @ r0 = 0
    mov    pc, r0   @ 跳转到复位向量
#endif

/*
 * set sp for current cpu
 * r1 is stack bottom, r0 is stack size, r11 hold cpu id
 */@设置当前CPU的SP, r1为栈底, r0为栈大小 r11 为 cpu id @ 设置当前 CPU 的栈指针，r1 是栈底，r0 是栈大小，r11 是 CPU ID
sp_set:
    mul    r3, r0, r11 	@ r3 = r0 * r11，计算偏移量
    sub    r2, r1, r3 	@ r2 = r1 - r3，计算当前 CPU 的 SP 位置，栈底地址高于栈顶
    mov    sp, r2 		@ sp = r2，设置栈指针
    bx     lr          @ 跳转到链接寄存器 lr 指向的地址继续执行

/*
 * r4: page table base address
 * r6: physical address
 * r7: virtual address
 * r8: sizes
 * r10: flags
 * r9 and r12 will be used as variable
 */
#ifdef LOSCFG_KERNEL_MMU @ 如果定义了 LOSCFG_KERNEL_MMU
page_table_build:
    mov     r10, r6 @ r10 = r6
    bfc     r10, #20, #12                       @ 清除 r10 中第 20 - 31 位，得到物理地址对 MB 取余结果
    add     r8, r8, r10 @ r8 = r8 + r10
    add     r8, r8, #(1 << 20) @ r8 = r8 + (1 << 20)
    sub     r8, r8, #1 @ r8 = r8 - 1
    lsr     r6, #20                             @ r6 = r6 >> 20，得到物理地址除以 MB 的结果
    lsr     r7, #20                             @ r7 = r7 >> 20，得到虚拟地址除以 MB 的结果
    lsr     r8, #20                             @ r8 = r8 >> 20，得到向上取整后的大小（MB 为单位）

page_table_build_loop:
    orr     r12, r9, r6, lsl #20                @ r12 = r9 | (r6 << 20)，得到标志位 | 物理地址
    str     r12, [r4, r7, lsl #2]               @ 页表[l1Index] = 物理地址 | 标志位
    add     r6, #1                              @ 物理地址加 1
    add     r7, #1                              @ 一级页表索引加 1
    subs    r8, #1                              @ 大小减 1
    bne     page_table_build_loop @ 若大小不为 0，跳转到 page_table_build_loop 继续
    bx      lr @ 跳转到链接寄存器 lr 指向的地址
#endif
/*
 * init stack to initial value
 * r0 is stack mem start, r1 is stack mem end
 */
stack_init:
    ldr     r2, =OS_STACK_INIT @ r2 = OS_STACK_INIT 值
    ldr     r3, =OS_STACK_INIT @ r3 = OS_STACK_INIT 值
    /* Main loop sets 32 bytes at a time. */ @ 主循环每次设置 32 字节
stack_init_loop:
    .irp    offset, #0, #8, #16, #24 @ 循环处理偏移量
    strd    r2, r3, [r0, \offset] @ 将 r2 和 r3 的值存储到 r0 + offset 指向的地址
    .endr
    add     r0, #32 @ r0 加 32
    cmp     r0, r1 @ 比较 r0 和 r1 的值
    blt     stack_init_loop @ 若 r0 < r1，跳转到 stack_init_loop 继续
    bx      lr @ 跳转到链接寄存器 lr 指向的地址

pa_va_offset:
    .word   . @ 存储当前地址

/*
 * set magic num to stack top for all cpu
 * r0 is stack top, r1 is stack size, r2 is magic num
 */
excstack_magic:
    mov     r3, #0 @ r3 = 0
excstack_magic_loop:
    str     r2, [r0] @ 将 r2 的值存储到 r0 指向的地址
    add     r0, r0, r1 @ r0 = r0 + r1
    add     r3, r3, #1 @ r3 = r3 + 1
    cmp     r3, #CORE_NUM @ 比较 r3 和 CORE_NUM 的值
    blt     excstack_magic_loop @ 若 r3 < CORE_NUM，跳转到 excstack_magic_loop 继续
    bx      lr @ 跳转到链接寄存器 lr 指向的地址

#ifdef LOSCFG_KERNEL_MMU @ 如果定义了 LOSCFG_KERNEL_MMU
memset_optimized:
    mov     r3, r0 @ r3 = r0
    vdup.8  q0, r1 @ 将 r1 的值复制到 8 字节向量寄存器 q0
    vmov    q1, q0 @ q1 = q0
    vmov    q2, q0 @ q2 = q0
    vmov    q3, q0 @ q3 = q0
memset_optimized_loop:
    subs    r2, #64 @ r2 = r2 - 64
    vstmia  r3!, {d0 - d7} @ 将向量寄存器 d0 - d7 的值存储到 r3 指向地址，r3 自增
    bge     memset_optimized_loop @ 若 r2 >= 0，跳转到 memset_optimized_loop 继续
    bx      lr @ 跳转到链接寄存器 lr 指向的地址
#endif
init_done:
    .long  0xDEADB00B @ 存储立即数 0xDEADB00B

    .code  32 @ 指定使用 32 位指令集
    .data @ 定义数据段

init_flag:
    .balign 4 @ 按 4 字节对齐
    .long   0 @ 存储立即数 0

    /*
    * Temporary interrupt stack
    */
    .section ".int_stack", "wa", %nobits @ 定义 .int_stack 段，可写、可分配，无实际数据
    .align  3 @ 按 8 字节对齐

__svc_stack:
    .space OS_EXC_SVC_STACK_SIZE * CORE_NUM @ 分配 OS_EXC_SVC_STACK_SIZE * CORE_NUM 字节空间
__svc_stack_top:

__exc_stack:
    .space OS_EXC_STACK_SIZE * CORE_NUM @ 分配 OS_EXC_STACK_SIZE * CORE_NUM 字节空间
__exc_stack_top:
