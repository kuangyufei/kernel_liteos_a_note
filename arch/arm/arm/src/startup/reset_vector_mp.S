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
#define  ASSEMBLY
#include "arch_config.h"
#include "los_vm_boot.h"
#include "los_vm_zone.h"
#include "los_mmu_descriptor_v6.h"
#undef ASSEMBLY


    .global __exc_stack_top
    .global __irq_stack_top
    .global __fiq_stack_top
    .global __svc_stack_top
    .global __abt_stack_top
    .global __undef_stack_top
    .global __exc_stack
    .global __irq_stack
    .global __fiq_stack
    .global __svc_stack
    .global __abt_stack
    .global __undef_stack

    .extern __bss_start
    .extern __bss_end
    .extern hal_clock_initialize_start
    .extern los_bss_init
    .extern _osExceptFiqHdl
    .extern _osExceptAddrAbortHdl
    .extern _osExceptDataAbortHdl
    .extern _osExceptPrefetchAbortHdl
    .extern _osExceptSwiHdl
    .extern _osExceptUndefInstrHdl
    .extern __stack_chk_guard_setup
    .extern g_firstPageTable 	/*一级页表,section(".bss.prebss.translation_table") UINT8 g_firstPageTable[MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS]*/
    .extern g_mmuJumpPageTable	/*临时页表,仅用于系统开机阶段使用*/

    .equ MPIDR_CPUID_MASK, 0xffU

    .fpu vfpv4
    .arm
/* 参数顺序:栈底,栈大小,r12:保存了cpuid*/
/* param0 is stack bottom, param1 is stack size, r12 hold cpu id */
.macro EXC_SP_SET param0, param1
    ldr    r1, =\param0 
    mov    r0, \param1
    bl     sp_set
.endm
/* 参数顺序:栈顶,栈大小,魔法数字*/
/* param0 is stack top, param1 is stack size, param2 is magic num */
.macro STACK_MAGIC_SET param0, param1, param2
    ldr     r0, =\param0
    mov     r1, \param1
    ldr     r2, =\param2
    bl      excstack_magic @初始化栈
.endm
/* 参数顺序:物理地址,虚拟地址,大小,标签*/
/* param0 is physical address, param1 virtual address, param2 is sizes, param3 is flag */
.macro PAGE_TABLE_SET param0, param1, param2, param3
    ldr     r6, =\param0
    ldr     r7, =\param1
    ldr     r8, =\param2
    ldr     r10, =\param3
    bl      page_table_build @创建页表
.endm
    .code   32
    .section ".vectors","ax"

    .global __exception_handlers
__exception_handlers:@异常处理
    /* 
    *Assumption:  ROM code has these vectors at the hardware reset address.
    *A simple jump removes any address-space dependencies [i.e. safer]
    */
    b   reset_vector 			@开机代码,此句被定位在零起始地址
    b   _osExceptUndefInstrHdl 	@CPU碰到不认识的指令入口
    b   _osExceptSwiHdl			@软中断入口
    b   _osExceptPrefetchAbortHdl	@取指异常入口
    b   _osExceptDataAbortHdl		@数据异常入口
    b   _osExceptAddrAbortHdl		@地址异常入口
    b   OsIrqHandler				@硬中断入口
    b   _osExceptFiqHdl				@快中断入口

    /* Startup code which will get the machine into supervisor mode */
    .global reset_vector
    .type   reset_vector,function
reset_vector: //鸿蒙开机代码
    /* clear register TPIDRPRW */
    mov     r0, #0					@r0 = 0
    mcr     p15, 0, r0, c13, c0, 4 	@c0,c13 = 0, C13为进程标识符 含义见 ARM720T.PDF 第64页
    /* do some early cpu setup: i/d cache disable, mmu disabled */ @禁用MMU, i/d缓存
    mrc     p15, 0, r0, c1, c0, 0  	@r0 = c1 ,c1寄存器详细解释见第64页
    bic     r0, #(1<<12) 			@位清除指令,清除r0的第11位
    bic     r0, #(1<<2 | 1<<0)		@清除第0和2位 ,禁止 MMU和缓存 0位:MMU enable/disable 2位:Cache enable/disable
    mcr     p15, 0, r0, c1, c0, 0 	@c1=r0 
									@adr指令 寄存器小范围的转变
    /* r11: delta of physical address and virtual address */@物理地址和虚拟地址的增量
    adr     r11, pa_va_offset @将基于PC相对偏移的地址pa_va_offset值读取到寄存器R11中
    ldr     r0, [r11]		  @r0 = *r11 
    sub     r11, r11, r0	  @r11 = r11 - r0	

    mrc     p15, 0, r12, c0, c0, 5              /* r12: get cpuid */ @获取CPUID
    and     r12, r12, #MPIDR_CPUID_MASK @r12经过掩码过滤
    cmp     r12, #0	@ cpu id 和 0 比较
    bne     secondary_cpu_init @不是0号主CPU则调用secondary_cpu_init
	/*
	adr是小范围的地址读取伪指令,它将基于PC寄存器相对偏移的地址值读取到寄存器中
	例如: 0x00000004 	 : adr     r4, __exception_handlers
	则此时PC寄存器的值为: 0x00000004 + 8(在三级流水线时,PC和执行地址相差8)
	最后r4 = (0x00000004 + 8) + __exception_handlers
	*/
    /* if we need to relocate to proper location or not */ @如果需要重新安装到合适的位置
    adr     r4, __exception_handlers            /* r4: base of load address */ @r4记录加载基地址,即:各异常的跳转指令
    ldr     r5, =SYS_MEM_BASE                   /* r5: base of physical address */@r5获得物理基地址 SYS_MEM_BASE = 0x80000000
												@subs中的s表示把进位结果写入CPSR r12 = r4-r5
	subs    r12, r4, r5                         /* r12: delta of load address and physical address */ 
    beq     reloc_img_to_bottom_done            /* if we load image at the bottom of physical address */@物理地址底部加载镜像

    /* we need to relocate image at the bottom of physical address */ @在物理地址的底部重新定位镜像
    ldr     r7, =__exception_handlers           /* r7: base of linked address (or vm address) */ @链接地址基地址
    ldr     r6, =__bss_start                    /* r6: end of linked address (or vm address) */@链接地址结束地址
    sub     r6, r7                              /* r6: delta of linked address (or vm address) */@链接地址范围
    add     r6, r4                              /* r6: end of load address */@加载地址结束地址

reloc_img_to_bottom_loop:
    ldr     r7, [r4], #4
    str     r7, [r5], #4
    cmp     r4, r6
    bne     reloc_img_to_bottom_loop
    sub     pc, r12
    nop
    sub     r11, r11, r12                       /* r11: eventual address offset */

reloc_img_to_bottom_done:
    ldr     r4, =g_firstPageTable               /* r4: physical address of translation table and clear it */ @获取页面地址
    add     r4, r4, r11 		@r4 = r4 + r11
    bl      page_table_clear	@清除页表
	@设置页表 /*PAGE_TABLE_SET的参数顺序:物理地址,虚拟地址,大小,标签*/
    PAGE_TABLE_SET SYS_MEM_BASE, KERNEL_VMM_BASE, KERNEL_VMM_SIZE, MMU_DESCRIPTOR_KERNEL_L1_PTE_FLAGS
    PAGE_TABLE_SET SYS_MEM_BASE, UNCACHED_VMM_BASE, UNCACHED_VMM_SIZE, MMU_INITIAL_MAP_STRONGLY_ORDERED
    PAGE_TABLE_SET PERIPH_PMM_BASE, PERIPH_DEVICE_BASE, PERIPH_DEVICE_SIZE, MMU_INITIAL_MAP_DEVICE
    PAGE_TABLE_SET PERIPH_PMM_BASE, PERIPH_CACHED_BASE, PERIPH_CACHED_SIZE, MMU_DESCRIPTOR_KERNEL_L1_PTE_FLAGS
    PAGE_TABLE_SET PERIPH_PMM_BASE, PERIPH_UNCACHED_BASE, PERIPH_UNCACHED_SIZE, MMU_INITIAL_MAP_STRONGLY_ORDERED

    orr     r8, r4, #MMU_TTBRx_FLAGS            /* r8 = r4 and set cacheable attributes on translation walk */
    ldr     r4, =g_mmuJumpPageTable             /* r4: jump pagetable vaddr */
    add     r4, r4, r11
    ldr     r4, [r4]
    add     r4, r4, r11                         /* r4: jump pagetable paddr */
    bl      page_table_clear	/* 执行清除操作 */
	/* L1页表项有三种描述格式,创建SECTION页表项,指向1M节的页表项*/
    /* build 1M section mapping, in order to jump va during turing on mmu:pa == pa, va == pa */
    mov     r6, pc
    mov     r7, r6                              /* r7: pa (MB aligned)*/
    lsr     r6, r6, #20                         /* r6: va l1 index */
    ldr     r10, =MMU_DESCRIPTOR_KERNEL_L1_PTE_FLAGS
    add     r12, r10, r6, lsl #20               /* r12: pa |flags */
    str     r12, [r4, r7, lsr #(20 - 2)]        /* jumpTable[paIndex] = pt entry */
    rsb     r7, r11, r6, lsl #20                /* r7: va */
    str     r12, [r4, r7, lsr #(20 - 2)]        /* jumpTable[vaIndex] = pt entry */

    bl      _bootaddr_setup	@安装引导地址

    bl      mmu_setup                           /* set up the mmu */@安装MMU
	/* 清除中断和异常堆栈，并设置magic num来检查溢出 */
    /* clear out the interrupt and exception stack and set magic num to check the overflow */
    ldr     r0, =__undef_stack
    ldr     r1, =__exc_stack_top
    bl      stack_init @初始化栈
	@设置各个栈顶魔法数字
    STACK_MAGIC_SET __undef_stack, #OS_EXC_UNDEF_STACK_SIZE, OS_STACK_MAGIC_WORD 
    STACK_MAGIC_SET __abt_stack, #OS_EXC_ABT_STACK_SIZE, OS_STACK_MAGIC_WORD
    STACK_MAGIC_SET __irq_stack, #OS_EXC_IRQ_STACK_SIZE, OS_STACK_MAGIC_WORD
    STACK_MAGIC_SET __fiq_stack, #OS_EXC_FIQ_STACK_SIZE, OS_STACK_MAGIC_WORD
    STACK_MAGIC_SET __svc_stack, #OS_EXC_SVC_STACK_SIZE, OS_STACK_MAGIC_WORD
    STACK_MAGIC_SET __exc_stack, #OS_EXC_STACK_SIZE, OS_STACK_MAGIC_WORD

warm_reset: @热启动,初始化CPU各异常工作模式环境
    /* initialize interrupt/exception environments */
    mov    r0, #(CPSR_IRQ_DISABLE |CPSR_FIQ_DISABLE|CPSR_IRQ_MODE)
    msr    cpsr, r0 @设置为普通中断模式和栈
    EXC_SP_SET __irq_stack_top, #OS_EXC_IRQ_STACK_SIZE

    mov    r0, #(CPSR_IRQ_DISABLE|CPSR_FIQ_DISABLE|CPSR_UNDEF_MODE)
    msr    cpsr, r0 @设置为未定义模式和栈
    EXC_SP_SET __undef_stack_top, #OS_EXC_UNDEF_STACK_SIZE

    mov    r0, #(CPSR_IRQ_DISABLE|CPSR_FIQ_DISABLE|CPSR_ABT_MODE)
    msr    cpsr, r0 @设置为终止模式和栈
    EXC_SP_SET __abt_stack_top, #OS_EXC_ABT_STACK_SIZE

    mov    r0, #(CPSR_IRQ_DISABLE|CPSR_FIQ_DISABLE|CPSR_FIQ_MODE)
    msr    cpsr, r0 @设置为快中断模式和栈
    EXC_SP_SET __fiq_stack_top, #OS_EXC_FIQ_STACK_SIZE

    /* initialize CPSR (machine state register) */
    mov    r0, #(CPSR_IRQ_DISABLE|CPSR_FIQ_DISABLE|CPSR_SVC_MODE)
    msr    cpsr, r0 @设置为管理模式和栈

    /* Note: some functions in LIBGCC1 will cause a "restore from SPSR"!! */
    msr    spsr, r0 @同步设置spsr

    /* get cpuid and keep it in r12 */
    mrc     p15, 0, r12, c0, c0, 5		@R12保存CPUID 
    and     r12, r12, #MPIDR_CPUID_MASK @掩码操作获取当前cpu id

    /* set svc stack, every cpu has OS_EXC_SVC_STACK_SIZE stack *//*设置每一个SVC栈,每个CPU都有独立的 SVC栈*/
    ldr    r0, =__svc_stack_top	@栈底位置,注意这里虽然使用了top,但实际是栈底
    mov    r2, #OS_EXC_SVC_STACK_SIZE @栈大小
    mul    r2, r2, r12 	@定位到栈位置,CPU SVC栈是连在一块的
    sub    r0, r0, r2  	@计算栈顶位置,使用sub也说明了栈底大于栈顶	
    mov    sp, r0 		@栈顶位置

    /* enable fpu+neon */
#ifndef LOSCFG_TEE_ENABLE
    MRC    p15, 0, r0, c1, c1, 2 
    ORR    r0, r0, #0xC00
    BIC    r0, r0, #0xC000
    MCR    p15, 0, r0, c1, c1, 2

    LDR    r0, =(0xF << 20)
    MCR    p15, 0, r0, c1, c0, 2
#endif

    MOV    r3, #0x40000000
    VMSR   FPEXC, r3

    LDR    r0, =__exception_handlers
    MCR    p15, 0, r0, c12, c0, 0 @c12=r0

    cmp    r12, #0	@是否为主CPU
    bne    cpu_start @跳转到cpu_start运行

clear_bss:	@清楚bss
    ldr    r1, =__bss_start @bbs开始地址
    ldr    r2, =__bss_end	@bss结束地址
    mov    r0, #0	@r0 = 0

bss_loop:	
    cmp    r1, r2	@r1和r2比较
    strlo  r0, [r1], #4 @ *r1 = r0 ,r1 = r1 +4 其实就是将r1慢慢清零
    blo    bss_loop @循环

#if defined(LOSCFG_CC_STACKPROTECTOR_ALL) || \
    defined(LOSCFG_CC_STACKPROTECTOR_STRONG) || \
    defined(LOSCFG_CC_STACKPROTECTOR)
    bl     __stack_chk_guard_setup
#endif

#ifdef LOSCFG_GDB_DEBUG
    /* GDB_START - generate a compiled_breadk,This function will get GDB stubs started, with a proper environment */
    bl     GDB_START
    .word  0xe7ffdeff
#endif

    bl     main  @带LR的子程序跳转, LR = pc - 4 ,执行C层main函数

_start_hang: @悬停 ,相当于死循环,不再往下走了.
    b      _start_hang

mmu_setup:			@安装MMU,初始化MMU操作
    mov     r12, #0 							@ r12 = 0的目的是 将c8设置为控制TLB，并将映射设置为无效
    mcr     p15, 0, r12, c8, c7, 0              /* Set c8 to control the TLB and set the mapping to invalid */
    isb											@C8为TLB控制,C7位高速缓存控制
    mcr     p15, 0, r12, c2, c0, 2              /* Initialize the c2 register */
    isb											@初始化C2 地址转换表基地址
    orr     r12, r4, #MMU_TTBRx_FLAGS			@设置属性和设置临时页表
    mcr     p15, 0, r12, c2, c0, 0              /* Set attributes and set temp page table */
    isb											@初始化C3 设置DACR为0b0111
    mov     r12, #0x7                           /* 0b0111 */
    mcr     p15, 0, r12, c3, c0, 0              /* Set DACR with 0b0111, client and manager domian */
    isb											@辅助控制寄存器
    mrc    p15, 0, r12, c1, c0, 1               /* ACTLR, Auxlliary Control Register */
    orr    r12, r12, #(1 << 6)                  /* SMP, Enables coherent requests to the processor. */
    orr    r12, r12, #(1 << 2)                  /* Enable D-side prefetch */
    orr    r12, r12, #(1 << 11)                 /* Global BP Enable bit */
    mcr    p15, 0, r12, c1, c0, 1               /* ACTLR, Auxlliary Control Register */
    dsb
    mrc     p15, 0, r12, c1, c0, 0
    bic     r12, #(1 << 29 | 1 << 28)
    orr     r12, #(1 << 0)
    bic     r12, #(1 << 1)
    orr     r12, #(1 << 2)
    orr     r12, #(1 << 12)
    mcr     p15, 0, r12, c1, c0, 0              /* Set SCTLR with r12: Turn on the MMU, I/D cache Disable TRE/AFE */
    isb
    ldr     pc,  =1f                            /* Convert to VA */
1:
    mcr     p15, 0, r8, c2, c0, 0               /* Go to the base address saved in C2: Jump to the page table */
    isb
    mov     r12, #0
    mcr     p15, 0, r12, c8, c7, 0
    isb
    sub     lr,  r11                            /* adjust lr with delta of physical address and virtual address */
    bx      lr

    .code  32

    .global reset_platform
    .type   reset_platform,function
reset_platform:		@平台复位
#ifdef A7SEM_HAL_ROM_MONITOR
    /* initialize CPSR (machine state register) */
    mov    r0, #(CPSR_IRQ_DISABLE|CPSR_FIQ_DISABLE|CPSR_SVC_MODE) @禁止中断和切换到SVC模式
    msr    cpsr, r0		@修改CPSR寄存器值
    b      warm_reset 	@跳转到热启动
#else
    mov    r0, #0
    mov    pc, r0   // Jump to reset vector @跳到重置向量表处
#endif
cpu_start:	/* CPU 启动 */
    ldr     r4, =g_firstPageTable               /* r4 = physical address of translation table and clear it */
    add     r4, r4, r11
    orr     r8, r4, #MMU_TTBRx_FLAGS

    ldr     r4, =g_mmuJumpPageTable             /* r4 = tt_trampoline vaddr */
    add     r4, r4, r11
    ldr     r4, [r4]
    add     r4, r4, r11                         /* r4 = tt_trampoline paddr */

    bl     mmu_setup			/* 安装MMU */
    bl     secondary_cpu_start	/* 启动次级CPU */
    b      . //注意 b . 就会跳转到当前地址,相当于死循环

secondary_cpu_init:	/* 次级CPU初始化 */
#ifdef LOSCFG_TEE_ENABLE
    /* enable fpu+neon */
    ldr    r0, =0x60c00
    MCR    p15, 0, r0, c1, c1, 2

    LDR    r0, =(0xF << 20)
    MCR    p15, 0, r0, c1, c0, 2
    cps    #0x16
    mov    r1, #0x25
    mcr    p15, 0, r1, c1, c1, 0
    cps    #0x13
#endif
    bl      warm_reset @热启动

/*
 * set sp for current cpu
 * r1 is stack bottom, r0 is stack size, r12 hold cpu id
 */
sp_set: /* 设置当前cpu 栈顶指针 ,r1是栈底 ,r0是栈大小,r12是cpuid*/
    mrc    p15, 0, r12, c0, c0, 5 @从协处理器取出cpuid
    and    r12, r12, #MPIDR_CPUID_MASK @掩码获取当前运行cpu id
    mul    r3, r0, r12 @ r3 = r0 * r12 计算出偏移量
    sub    r2, r1, r3  @ r2 = r1 - r3 算出cpuid 对应的栈顶位置
    mov    sp, r2	   @ 将栈顶指针sp移到栈顶位置.
    bx     lr          /* set sp */ @跳回到LR执行

/*
 * r4: page table base address
 * r5 and r6 will be used as variable
 */
page_table_clear:	@清空页表,一页一页来
    mov     r5, #0
    mov     r6, #0
0:
    str     r5, [r4, r6, lsl #2]
    add     r6, #1
    cmp     r6, #0x1000                         /* r6 < 4096 */
    blt     0b
    bx      lr

/*
 * r4: page table base address 页表基地址
 * r6: physical address		   物理地址
 * r7: virtual address		   虚拟地址
 * r8: sizes				   大小(4K)
 * r10: flags				   标签
 * r9 and r12 will be used as variable 作为变量使用
 */
page_table_build:	@创建页面
    mov     r9, r6
    bfc     r9, #20, #12                        /* r9: pa % MB */
    add     r8, r8, r9
    add     r8, r8, #(1 << 20)
    sub     r8, r8, #1
    lsr     r6, #20                             /* r6 = physical address / MB */
    lsr     r7, #20                             /* r7 = virtual address / MB */
    lsr     r8, #20                             /* r8 = roundup(size, MB) */

page_table_build_loop:
    orr     r12, r10, r6, lsl #20               /* r12: flags | physAddr */
    str     r12, [r4, r7, lsl #2]               /* gPgTable[l1Index] = physAddr | flags */
    add     r6, #1                              /* physAddr+ */
    add     r7, #1                              /* l1Index++ */
    subs    r8, #1                              /* sizes-- */
    bne     page_table_build_loop
    bx      lr

/*
 * init stack to initial value 
 * r0 is stack mem start, r1 is stack mem end
 *///初始化栈,前置条件:r0和r1分别为栈顶和栈底
stack_init:@初始化栈,注意这里执行完 stack_init后会接着执行stack_init_loop,因为pc寄存器会一直往下走,直到有指令令它改变走向
    ldr     r2, =OS_STACK_INIT	@魔法数字
    ldr     r3, =OS_STACK_INIT	@魔法数字
    /* Main loop sets 32 bytes at a time. */
stack_init_loop:@循环初始化栈
    .irp    offset, #0, #8, #16, #24 @等价于 strd    r2, r3, [r0, 0],...,strd    r2, r3, [r0, 24]
    strd    r2, r3, [r0, \offset]
    .endr
    add     r0, #32
    cmp     r0, r1
    blt     stack_init_loop
    bx      lr @ mov pc lr 改变走向

pa_va_offset:	@物理地址和虚拟地址偏移量
    .word   .   @表示无偏移

/*
 * set magic num to stack top for all cpu
 * r0 is stack top, r1 is stack size, r2 is magic num
 */
excstack_magic:	@开始设置魔法数字
    mov     r3, #0 @用作记录CPU id
excstack_magic_loop:@给所有CPU栈顶位置设置魔法数字
    str     r2, [r0] @栈顶设置魔法数字  ,等同于  *r0 = r2
    add     r0, r0, r1 @定位到栈底
    add     r3, r3, #1 @r3++
    cmp     r3, #CORE_NUM @比较cpu个数 
    blt     excstack_magic_loop @循环来
    bx      lr @跳回 mov pc lr

/*
 * 0xe51ff004 = "ldr  pc, [pc, #-4]" 
 * next addr value will be the real booting addr 下一个addr将是真正的引导addr
 */
_bootaddr_setup:@引导程序 另外 0xE28FE004 = "add lr,pc,#4"
    mov     r0, #0
    ldr     r1, =0xe51ff004 @等同于 MOV r1, #0xe51ff004   ,
    str     r1, [r0] @等同于 *r0 = r1

    add     r0, r0, #4	@r0 = r0 + 4
    ldr     r1, =SYS_MEM_BASE	@r1 = 0x80000000 物理基地址 SYS_MEM_BASE = 0x80000000
    str     r1, [r0] @等同于 *r0 = r1

    dsb @数据同步
    isb @指令同步

    bx      lr @跳回调用处

init_done:
    .long  0xDEADB00B

    .code  32
    .data

init_flag:
    .balign 4
    .long   0

    /*
    * Temporary interrupt stack
    */
    .section ".int_stack", "wa", %nobits
    .align  3
@六种特权模式申请对应的栈运行空间
__undef_stack:
    .space OS_EXC_UNDEF_STACK_SIZE * CORE_NUM 
__undef_stack_top:

__abt_stack:
    .space OS_EXC_ABT_STACK_SIZE * CORE_NUM
__abt_stack_top:

__irq_stack:
    .space OS_EXC_IRQ_STACK_SIZE * CORE_NUM //64
__irq_stack_top:

__fiq_stack:
    .space OS_EXC_FIQ_STACK_SIZE * CORE_NUM
__fiq_stack_top:

__svc_stack:
    .space OS_EXC_SVC_STACK_SIZE * CORE_NUM
__svc_stack_top:

__exc_stack:
    .space OS_EXC_STACK_SIZE * CORE_NUM
__exc_stack_top:
