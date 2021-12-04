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


    .global __exc_stack_top
    .global __svc_stack_top
    .global __exc_stack
    .global __svc_stack

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
    .extern g_archMmuInitMapping
    .extern HalSecondaryCpuStart

    .equ MPIDR_CPUID_MASK, 0xffU

    .fpu neon-vfpv4
    .syntax unified
    .arch armv7-a
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
    bl      excstack_magic //初始化栈
.endm

    .code   32
    .section ".vectors","ax" //中断向量表在系统镜像中的位置

    .global __exception_handlers
__exception_handlers://异常处理
    /* 
    *Assumption:  ROM code has these vectors at the hardware reset address.
    *A simple jump removes any address-space dependencies [i.e. safer]
    */
    b   reset_vector 			//开机代码,此句被定位在零起始地址
    b   _osExceptUndefInstrHdl 	//CPU碰到不认识的指令入口
    b   _osExceptSwiHdl			//软中断入口
    b   _osExceptPrefetchAbortHdl	//取指异常入口
    b   _osExceptDataAbortHdl		//数据异常入口
    b   _osExceptAddrAbortHdl		//地址异常入口
    b   OsIrqHandler				//硬中断入口
    b   _osExceptFiqHdl				//快中断入口

    /* Startup code which will get the machine into supervisor mode */
    .global reset_vector
    .type   reset_vector,function

#ifdef LOSCFG_BOOTENV_RAM
__quickstart_args_start:
    .fill LOSCFG_BOOTENV_RAMSIZE,1,0
__quickstart_args_end:

.global OsGetArgsAddr
.type   OsGetArgsAddr,function

OsGetArgsAddr:
    ldr     r0, =__quickstart_args_start
    bx      lr
#endif

reset_vector: //鸿蒙开机代码
    /* clear register TPIDRPRW */
    mov     r0, #0					//r0 = 0
    mcr     p15, 0, r0, c13, c0, 4 	//c0,c13 = 0, C13为进程标识符 含义见 ARM720T.PDF 第64页
    /* do some early cpu setup: i/d cache disable, mmu disabled | 禁用MMU, i/d缓存*/
    mrc     p15, 0, r0, c1, c0, 0  	//r0 = c1 ,c1寄存器详细解释见第64页
    bic     r0, #(1<<12) 			//位清除指令,清除r0的第11位
    bic     r0, #(1<<2 | 1<<0)		//清除第0和2位 ,禁止 MMU和缓存 0位:MMU enable/disable 2位:Cache enable/disable
    mcr     p15, 0, r0, c1, c0, 0 	//c1=r0 
									//adr指令 寄存器小范围的转变
    /* enable fpu+neon 一些系统寄存器的操作
    | 使能浮点运算(floating point unit)和 NEON就是一种基于SIMD思想的ARM技术，相比于ARMv6或之前的架构，
    NEON结合了64-bit和128-bit的SIMD指令集，提供128-bit宽的向量运算(vector operations)*/
#ifndef LOSCFG_TEE_ENABLE // Trusted Execution Environment   可信执行环境
    MRC    p15, 0, r0, c1, c1, 2
    ORR    r0, r0, #0xC00
    BIC    r0, r0, #0xC000
    MCR    p15, 0, r0, c1, c1, 2

    LDR    r0, =(0xF << 20)
    MCR    p15, 0, r0, c1, c0, 2
    ISB
#endif
/*当内核被加载到内存中时，它的起始地址就叫做加载地址，因为内核是被uboot加载的，uboot中对mmu页表不会做太复杂的映射，
一般也就是一一映射，也就是物理地址和虚拟地址值是一样的，所以可以认为内核加载地址值是物理地址。*/
    MOV    r3, #0x40000000	//指定内核运行基地址 KERNEL_VADDR_BASE,也就是说将内核镜像文件放到这个位置.
    VMSR   FPEXC, r3	//VMRS/VMSR 在ARM寄存器与NEON和VFP系统寄存器之间传输内容
    /* r11: delta of physical address and virtual address | 内核运行虚拟地址和物理地址之间的差值 */
    adr     r11, pa_va_offset //获取pa_va_offset变量的当前运行地址，由于这时候mmu已经被关闭，所以这个值就表示pa_va_offset变量的物理地址
    ldr     r0, [r11]		  //r0 = *r11 获取pa_va_offset变量的虚拟地址
    sub     r11, r11, r0	  //r11 = r11 - r0	将pa_va_offset变量物理地址-虚拟地址得到差值

    mrc     p15, 0, r12, c0, c0, 5              /* r12: get cpuid | 获取CPUID*/ 
    and     r12, r12, #MPIDR_CPUID_MASK //r12经过掩码过滤
    cmp     r12, #0	// cpu id 和 0 比较
    bne     secondary_cpu_init //不是0号主CPU则调用secondary_cpu_init
	/*
	adr是小范围的地址读取伪指令,它将基于PC寄存器相对偏移的地址值读取到寄存器中
	例如: 0x00000004 	 : adr     r4, __exception_handlers
	则此时PC寄存器的值为: 0x00000004 + 8(在三级流水线时,PC和执行地址相差8)
	最后r4 = (0x00000004 + 8) + __exception_handlers
	*/
    /* if we need to relocate to proper location or not | 如果需要重新安装到合适的位置*/
    adr     r4, __exception_handlers            /* r4: base of load address | r4记录加载基地址,即:各异常的跳转指令*/
    ldr     r5, =SYS_MEM_BASE                   /* r5: base of physical address | r5获得物理内存基地址 SYS_MEM_BASE = 0x80000000*/
												//subs中的s表示把进位结果写入CPSR r12 = r4-r5
	subs    r12, r4, r5                         /* r12: delta of load address and physical address */ 
    beq     reloc_img_to_bottom_done            /* if we load image at the bottom of physical address | 不相等就需要重定位*/
	
    /* we need to relocate image at the bottom of physical address | 在物理地址的底部重新定位镜像*/
    ldr     r7, =__exception_handlers           /* r7: base of linked address (or vm address) | 链接地址基地址*/
    ldr     r6, =__bss_start                    /* r6: end of linked address (or vm address) 
											    | 链接地址结束地址,由于目前阶段有用的数据就是中断向量表+代码段+只读数据段+数据段，
											    所以只需要复制 [__exception_handlers,__bss_start]这么多数据到内存基址处就行了*/
    sub     r6, r7                              /* r6: delta of linked address (or vm address) | 链接地址范围*/
    add     r6, r4                              /* r6: end of load address | 加载地址结束地址*/

reloc_img_to_bottom_loop://循环处理重定位镜像到底部,将内核从加载地址拷贝到内存基址处
    ldr     r7, [r4], #4
    str     r7, [r5], #4
    cmp     r4, r6
    bne     reloc_img_to_bottom_loop
    sub     pc, r12
    nop
    sub     r11, r11, r12                       /* r11: eventual address offset */
//内核总大小 __bss_start - __exception_handlers
reloc_img_to_bottom_done://重定位镜像到底部完成后跳到新地址处运行并且校正r11的值
#ifdef LOSCFG_KERNEL_MMU
    ldr     r4, =g_firstPageTable               /* r4: physical address of translation table and clear it |
    											内核页表是用数组g_firstPageTable来表示 见于 los_arch_mmu.c*/
    add     r4, r4, r11`//内核页表虚拟地址+r11=内核页表物理地址，保存在r4中
    mov     r0, r4
    mov     r1, #0
    mov     r2, #MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS
    bl      memset_optimized                    /* optimized memset since r0 is 64-byte aligned | 将内核页表内存清零*/

    ldr     r5, =g_archMmuInitMapping	//获取映射基址,初始化映射,目的是为建立后续物理地址和虚拟地址之间的映射
    add     r5, r5, r11 //获取物理地址
init_mmu_loop:	//接着对内核页表进行清零，然后初始化页表
    ldmia   r5!, {r6-r10}                       /* r6 = phys, r7 = virt, r8 = size, r9 = mmu_flags, r10 = name | 物理地址、虚拟地址、映射大小、映射属性、名称*/
    cmp     r8, 0                               /* if size = 0, the mmu init done */
    beq     init_mmu_done		//标志寄存器中Z标志位等于零时跳转到 	init_mmu_done处执行
    bl      page_table_build	//创建页表	
    b       init_mmu_loop
init_mmu_done:
    orr     r8, r4, #MMU_TTBRx_FLAGS            /* r8 = r4 and set cacheable attributes on translation walk */
    ldr     r4, =g_mmuJumpPageTable             /* r4: jump pagetable vaddr */
    add     r4, r4, r11
    ldr     r4, [r4]
    add     r4, r4, r11                         /* r4: jump pagetable paddr */

    /* build 1M section mapping, in order to jump va during turing on mmu:pa == pa, va == pa */
    mov     r6, pc
    mov     r7, r6                              /* r7: pa (MB aligned)*/
    lsr     r6, r6, #20                         /* r6: va l1 index */
    ldr     r10, =MMU_DESCRIPTOR_KERNEL_L1_PTE_FLAGS
    add     r12, r10, r6, lsl #20               /* r12: pa |flags */
    str     r12, [r4, r7, lsr #(20 - 2)]        /* jumpTable[paIndex] = pt entry */
    rsb     r7, r11, r6, lsl #20                /* r7: va */
    str     r12, [r4, r7, lsr #(20 - 2)]        /* jumpTable[vaIndex] = pt entry */

    bl      mmu_setup                           /* set up the mmu | 设置mmu*/
#endif
    /* clear out the interrupt and exception stack and set magic num to check the overflow
    | 清除中断栈到异常栈的数据,并设置魔法数字为后续检查栈溢出 */
    ldr     r0, =__svc_stack	//中断栈的底部,从这里可以看出,中断栈处于最高位
    ldr     r1, =__exc_stack_top	//异常栈的顶部
    bl      stack_init //初始化各个cpu模式下的栈
	//设置各个栈顶魔法数字
    STACK_MAGIC_SET __svc_stack, #OS_EXC_SVC_STACK_SIZE, OS_STACK_MAGIC_WORD //中断栈底设成 烫烫烫烫烫烫
    STACK_MAGIC_SET __exc_stack, #OS_EXC_STACK_SIZE, OS_STACK_MAGIC_WORD //异常栈底设成 烫烫烫烫烫烫

warm_reset://热启动
    /* initialize CPSR (machine state register) */
    mov    r0, #(CPSR_IRQ_DISABLE|CPSR_FIQ_DISABLE|CPSR_SVC_MODE)
    msr    cpsr, r0 //设置为管理模式和栈

    /* Note: some functions in LIBGCC1 will cause a "restore from SPSR"!! */
    msr    spsr, r0 //同步设置spsr

    /* get cpuid and keep it in r12 */
    mrc     p15, 0, r12, c0, c0, 5		//R12保存CPUID 
    and     r12, r12, #MPIDR_CPUID_MASK //掩码操作获取当前cpu id

    /* set svc stack, every cpu has OS_EXC_SVC_STACK_SIZE stack | 设置SVC模式 中断栈,每个CPU核都有自己的中断栈 */
    ldr    r0, =__svc_stack_top
    mov    r2, #OS_EXC_SVC_STACK_SIZE
    mul    r2, r2, r12
    sub    r0, r0, r2
    mov    sp, r0

    LDR    r0, =__exception_handlers
    MCR    p15, 0, r0, c12, c0, 0

    cmp    r12, #0 //判断当前cpu是不是主核，如果是主核，则执行bss段清零，如果是从核则不需要执行bbs段清零
    bne    cpu_start //不等于 跳到从核处理	

clear_bss:	//主核时对 .bss段清零
    ldr    r0, =__bss_start
    ldr    r2, =__bss_end
    mov    r1, #0
    sub    r2, r2, r0
    bl     memset

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

    bl     main  //带LR的子程序跳转, LR = pc - 4 ,执行C层main函数

_start_hang: //悬停 ,相当于死循环,不再往下走了.
    b      _start_hang
#ifdef LOSCFG_KERNEL_MMU
mmu_setup:			//设置MMU,初始化MMU操作
    mov     r12, #0 							// r12 = 0的目的是 将c8设置为控制TLB，并将映射设置为无效
    mcr     p15, 0, r12, c8, c7, 0              /* Set c8 to control the TLB and set the mapping to invalid */
    isb											//C8为TLB控制,C7位高速缓存控制
    mcr     p15, 0, r12, c2, c0, 2              /* Initialize the c2 register */
    isb											//初始化C2 地址转换表基地址
    orr     r12, r4, #MMU_TTBRx_FLAGS			//将属性和临时页表设置到页表基址寄存器中,注意此处只是设置了内核临时页表基址，mmu还未使能
    mcr     p15, 0, r12, c2, c0, 0              /* Set attributes and set temp page table */
    isb											//初始化C3 设置DACR为0b0111
    mov     r12, #0x7                           /* 0b0111 */
    mcr     p15, 0, r12, c3, c0, 0              /* Set DACR with 0b0111, client and manager domian */
    isb											//辅助控制寄存器
    mrc    p15, 0, r12, c1, c0, 1               /* ACTLR, Auxlliary Control Register */
    orr    r12, r12, #(1 << 6)                  /* SMP, Enables coherent requests to the processor. */
    orr    r12, r12, #(1 << 2)                  /* Enable D-side prefetch */
    orr    r12, r12, #(1 << 11)                 /* Global BP Enable bit */
    mcr    p15, 0, r12, c1, c0, 1               /* ACTLR, Auxlliary Control Register */
    dsb
    /*
	开始使能MMU，使用的是内核临时页表，这时cpu访问内存不管是取指令还是访问数据都是需要经过mmu来翻译，
	但是在mmu使能之前cpu使用的都是内核的物理地址，即使现在使能了mmu，cpu访问的地址值还是内核的物理地址值(这里仅仅从数值上来看)，
	而又由于mmu使能了，所以cpu会把这个值当做虚拟地址的值到页表中去找其对应的物理地址来访问。
	所以现在明白了为什么要在内核临时页表里建立一个内核物理地址和虚拟地址一一映射的页表项了吧，因为建立了一一映射，
	cpu访问的地址经过mmu翻译得到的还是和原来一样的值，这样在cpu真正使用虚拟地址之前也能正常运行。
    */
    mrc     p15, 0, r12, c1, c0, 0	//使能mmu和i/d cache
    bic     r12, #(1 << 29 | 1 << 28)
    orr     r12, #(1 << 0)
    bic     r12, #(1 << 1)
    orr     r12, #(1 << 2)
    orr     r12, #(1 << 12)
    mcr     p15, 0, r12, c1, c0, 0              /* Set SCTLR with r12: Turn on the MMU, I/D cache Disable TRE/AFE */
    isb
    ldr     pc,  =1f                            /* Convert to VA | 强制将cpu切到虚拟地址工作，
    因为之前已经在内核临时页表中建立了内核虚拟地址和物理地址的映射关系，所以这时候cpu能正常工作*/
1:	//CPU将从此处开始运行
    mcr     p15, 0, r8, c2, c0, 0               /* Go to the base address saved in C2: Jump to the page table */
    isb		//清除流水线指令 r8中保存的是内核真正工作页表的初始化值，通过设置系统寄存器进行页表的切换
    mov     r12, #0
    mcr     p15, 0, r12, c8, c7, 0
    isb
    sub     lr,  r11                            /* adjust lr with delta of physical address and virtual address | 
    		lr中保存的是mmu使能之前返回地址的物理地址值，这时需要转换为虚拟地址，转换算法也很简单，虚拟地址 = 物理地址 - r11，*/
    bx      lr //然后返回即可
#endif
    .code  32

    .global reset_platform
    .type   reset_platform,function
reset_platform:		//平台复位
#ifdef A7SEM_HAL_ROM_MONITOR
    /* initialize CPSR (machine state register) */
    mov    r0, #(CPSR_IRQ_DISABLE|CPSR_FIQ_DISABLE|CPSR_SVC_MODE) //禁止中断和切换到SVC模式
    msr    cpsr, r0		//修改CPSR寄存器值
    b      warm_reset 	//跳转到热启动
#else
    mov    r0, #0
    mov    pc, r0   // Jump to reset vector //跳到重置向量表处
#endif
cpu_start:	/* CPU 启动 */
#ifdef LOSCFG_KERNEL_MMU
    ldr     r4, =g_firstPageTable               /* r4 = physical address of translation table and clear it */
    add     r4, r4, r11
    orr     r8, r4, #MMU_TTBRx_FLAGS //内核页表物理基址值或将mmu属性标志后的值保存在r8中，这个值也就是页表寄存器的初始化值
	//内核临时页表在系统使能mmu到使用虚拟地址运行这段期间使用。内核临时页表使用__mmu_ttlb_begin来表示，
	//其虚拟地址保存在g_mmuJumpPageTable这个指针中系统首先将内核临时页表清零，这个操作和之前清零内核页表是一样的
    ldr     r4, =g_mmuJumpPageTable             /* r4 = tt_trampoline vaddr */
    add     r4, r4, r11
    ldr     r4, [r4]
    add     r4, r4, r11                         /* r4 = tt_trampoline paddr */

    bl     mmu_setup			/* 安装MMU */
#endif

    bl     HalSecondaryCpuStart	//跳到次核CPU执行
    b      . //注意 b . 指在当前地址运行,相当于死循环

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
    bl      warm_reset //热启动

/*
 * set sp for current cpu
 * r1 is stack bottom, r0 is stack size, r12 hold cpu id
 */
sp_set: /* 设置当前cpu 栈顶指针 ,r1是栈底 ,r0是栈大小,r12是cpuid*/
    mrc    p15, 0, r12, c0, c0, 5 //从协处理器取出cpuid
    and    r12, r12, #MPIDR_CPUID_MASK //掩码获取当前运行cpu id
    mul    r3, r0, r12 // r3 = r0 * r12 计算出偏移量
    sub    r2, r1, r3  // r2 = r1 - r3 算出cpuid 对应的栈顶位置
    mov    sp, r2	   // 将栈顶指针sp移到栈顶位置.
    bx     lr          /* set sp | 跳回到LR执行*/

/*
 * r4: page table base address 页表所在位置
 * r6: physical address		   物理地址
 * r7: virtual address		   虚拟地址
 * r8: sizes				   大小(4K)
 * r10: flags				   标签
 * r9 and r12 will be used as variable 作为变量使用
 */
#ifdef LOSCFG_KERNEL_MMU
/**
 * 将物理地址、虚拟地址还有映射大小都对齐到1MB，因为内核页表是进行1MB段式映射，然后根据虚拟地址得到页表项索引，
 * 将物理地址和映射属性组合后填入对应的页表项,这是虚拟地址空间和物理地址空间的映射
*/
page_table_build:	//创建页表
    mov     r10, r6
    bfc     r10, #20, #12                       /* r9: pa % MB */
    add     r8, r8, r10
    add     r8, r8, #(1 << 20)
    sub     r8, r8, #1
    lsr     r6, #20                             /* r6 = physical address / MB */
    lsr     r7, #20                             /* r7 = virtual address / MB */
    lsr     r8, #20                             /* r8 = roundup(size, MB) */

page_table_build_loop:
    orr     r12, r9, r6, lsl #20                /* r12: flags | physAddr */
    str     r12, [r4, r7, lsl #2]               /* gPgTable[l1Index] = physAddr | flags */
    add     r6, #1                              /* physAddr+ */
    add     r7, #1                              /* l1Index++ */
    subs    r8, #1                              /* sizes-- */
    bne     page_table_build_loop
    bx      lr
#endif
/*
 * init stack to initial value 
 * r0 is stack mem start, r1 is stack mem end
 *///初始化栈,前置条件:r0和r1分别为栈顶和栈底
stack_init://初始化栈,注意这里执行完 stack_init后会接着执行stack_init_loop,因为pc寄存器会一直往下走,直到有指令令它改变走向
    ldr     r2, =OS_STACK_INIT	//魔法数字
    ldr     r3, =OS_STACK_INIT	//魔法数字
    /* Main loop sets 32 bytes at a time. */
stack_init_loop://循环初始化栈空间
    .irp    offset, #0, #8, #16, #24 //等价于 strd    r2, r3, [r0, 0],...,strd    r2, r3, [r0, 24]
    strd    r2, r3, [r0, \offset]
    .endr
    add     r0, #32
    cmp     r0, r1
    blt     stack_init_loop
    bx      lr // mov pc lr 改变走向

/*
 * 内核的运行地址和物理地址是什么意思呢？
 * 内核的运行地址就是内核设计者希望内核运行时在内存中的位置，这个地址在内核源码中有地方可以配置，(见链接脚本board.ld.S)
 * 并且链接脚本里也会用到这个地址，编译代码时所用到的跟地址相关的值都是以内核运行基址为基础进行计算的。
*/
pa_va_offset:	//物理地址和虚拟地址偏移量
    .word   .   //表示无偏移,开始之初,一一映射,无偏差

/*
 * set magic num to stack top for all cpu
 * r0 is stack top, r1 is stack size, r2 is magic num
 */
excstack_magic:	//开始设置魔法数字
    mov     r3, #0 //用作记录CPU id
excstack_magic_loop://给所有CPU栈顶位置设置魔法数字
    str     r2, [r0] //栈顶设置魔法数字  ,等同于  *r0 = r2
    add     r0, r0, r1 //定位到栈底
    add     r3, r3, #1 //r3++
    cmp     r3, #CORE_NUM //比较cpu个数 
    blt     excstack_magic_loop //循环来
    bx      lr //跳回调用处

#ifdef LOSCFG_KERNEL_MMU
memset_optimized:
    mov     r3, r0
    vdup.8  q0, r1
    vmov    q1, q0
    vmov    q2, q0
    vmov    q3, q0
memset_optimized_loop:
    subs    r2, #64
    vstmia  r3!, {d0 - d7}
    bge     memset_optimized_loop
    bx      lr
#endif
init_done:
    .long  0xDEADB00B

    .code  32
    .data

init_flag:
    .balign 4
    .long   0

    /*
    * Temporary interrupt stack | 临时中断栈
    */
    .section ".int_stack", "wa", %nobits //见 liteos.ld 第 152 行
    .align  3
//CPU各个模式下的栈空间(目前是两个模式,早先代码是6个模式都有自己的栈空间),每个CPU核都有自己的中断栈和异常栈
__svc_stack:
    .space OS_EXC_SVC_STACK_SIZE * CORE_NUM //中断栈 8K
__svc_stack_top:

__exc_stack:
    .space OS_EXC_STACK_SIZE * CORE_NUM //异常栈 4K
__exc_stack_top:
