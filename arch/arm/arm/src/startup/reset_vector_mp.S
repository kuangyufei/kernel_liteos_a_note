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

/*
.global symbol: 定义一个全局符号, 通常是为 ld/链接器 使用.
.macro: 定义一段宏代码, .macro 表示代码的开始, .endm 表示代码的结束.
.ascii "string": 定义一个字符串并为之分配空间.
.byte expressions: 定义一个字节, 并为之分配空间
.short expressions: 定义一个短整型,  并为之分配空间
.word expressions: 定义一个字,并为之分配空间, 4bytes
.extern symbol 使用外部定义
.equ 相当于 #define
.abort 停止汇编
.comm symbol, length:在bss段申请一段命名空间,该段空间的名称叫symbol, 长度为length. Ld连接器在连接会为它留出空间
*/
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

    .equ MPIDR_CPUID_MASK, 0xffU //#define MPIDR_CPUID_MASK 0xffU

    .fpu neon-vfpv4 //支持的浮点处理器
    .syntax unified
    .arch armv7-a //支持芯片指令集
    .arm //支持CPU架构
    
/* 参数顺序:栈底,栈大小,r12:保存了cpuid */
/* param0 is stack bottom, param1 is stack size, r12 hold cpu id */
.macro EXC_SP_SET param0, param1
    ldr    r1, =\param0 
    mov    r0, \param1
    bl     sp_set
.endm

/* param0 is stack top, param1 is stack size, param2 is magic num */
.macro STACK_MAGIC_SET param0, param1, param2
    ldr     r0, =\param0
    mov     r1, \param1
    ldr     r2, =\param2
    bl      excstack_magic
.endm

    .code   32               /* 用arm指令集 */
    .section ".vectors","ax" /* 指定以下代码段必须存放在.vectors段里，“ax”表示该段可执行并且可分配 */

    .global __exception_handlers /* 代表全局函数 */
__exception_handlers: //加载基地址
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
    mcr     p15, 0, r0, c13, c0, 4	//复位线程标识符寄存器TPIDRPRW , 不复位将导致系统不能启动
    /* do some early cpu setup: i/d cache disable, mmu disabled */
    mrc     p15, 0, r0, c1, c0, 0	//System Control Register-SCTLR | 读取系统控制寄存器内容
    bic     r0, #(1<<12)			//禁用指令缓存功能
    bic     r0, #(1<<2 | 1<<0)		//禁用数据和TLB的缓存功能(bit2) | mmu功能(bit0)
    mcr     p15, 0, r0, c1, c0, 0	//写系统控制寄存器

    /* enable fpu+neon 一些系统寄存器的操作
    | 使能浮点运算(floating point unit)和 NEON就是一种基于SIMD思想的ARM技术，相比于ARMv6或之前的架构，
    NEON结合了64-bit和128-bit的SIMD指令集，提供128-bit宽的向量运算(vector operations)*/
#ifndef LOSCFG_TEE_ENABLE        //Trusted Execution Environment   可信执行环境
    MRC    p15, 0, r0, c1, c1, 2 //非安全模式访问寄存器 (Non-Secure Access Control Register - NSACR)
    ORR    r0, r0, #0xC00        //使能安全和非安全访问协处理器10和11(Coprocessor 10和11)
    BIC    r0, r0, #0xC000       //设置bit15为0，不会影响修改CPACR.ASEDIS寄存器位（控制Advanced SIMD功能）| bit14 reserved
    MCR    p15, 0, r0, c1, c1, 2

    LDR    r0, =(0xF << 20)      //允许在EL0和EL1下，访问协处理器10和11(控制Floating-point和Advanced SIMD特性)
    MCR    p15, 0, r0, c1, c0, 2
    ISB
#endif
    MOV    r3, #0x40000000	    //EN, bit[30] 设置FPEXC的EN位来使能FPU
    VMSR   FPEXC, r3			//浮点异常控制寄存器 (Floating-Point Exception Control register | B4.1.57) 

    /* r11: delta of physical address and virtual address | 内核运行虚拟地址和物理地址之间的差值 */
    adr     r11, pa_va_offset //获取pa_va_offset变量物理地址，由于这时候mmu已经被关闭，所以这个值就表示pa_va_offset变量的物理地址。
                              /*adr 是一条小范围的地址读取伪指令,它将基于PC的相对偏移的地址值读到目标寄存器中。
                               *编译源程序时，汇编器首先计算当前PC值（当前指令位置）到exper的距离,然后用一条ADD或者SUB指令替换这条伪指令，
                               *例如:ADD register,PC,#offset_to_exper 注意，标号exper与指令必须在同一代码段
                               */
    ldr     r0, [r11]		  //r0 = *r11 获取pa_va_offset变量虚拟地址
    sub     r11, r11, r0	  //物理地址-虚拟地址 = 映射偏移量 放入r11

    mrc     p15, 0, r12, c0, c0, 5      /* Multiprocessor Affinity Register-MPIDR */
    and     r12, r12, #MPIDR_CPUID_MASK //掩码过滤
    cmp     r12, #0	                    //主控核0判断
    bne     secondary_cpu_init	//初始化CPU次核
	/*
	 * adr是小范围的地址读取伪指令,它将基于PC寄存器相对偏移的地址值读取到寄存器中，
	 * 例如: 0x00000004 	 : adr     r4, __exception_handlers
	 * 则此时PC寄存器的值为: 0x00000004 + 8(在三级流水线时,PC和执行地址相差8)，
     * adr指令和标识__exception_handlers的地址相对固定，二者偏移量若为offset,
	 * 最后r4 = (0x00000004 + 8) + offset
	*/

    /* if we need to relocate to proper location or not | 如果需要重新安装到合适的位置*/
    adr     r4, __exception_handlers            /* r4: base of load address | 加载基址*/
    ldr     r5, =SYS_MEM_BASE                   /* r5: base of physical address | 物理基址*/
    subs    r12, r4, r5                         /* r12: delta of load address and physical address | 二者偏移量*/
    beq     reloc_img_to_bottom_done            /* if we load image at the bottom of physical address | 不相等就需要重定位 */
	
    /* we need to relocate image at the bottom of physical address | 需要知道拷贝的大小*/
    ldr     r7, =__exception_handlers           /* r7: base of linked address (or vm address) | 链接地址基地址*/
    ldr     r6, =__bss_start                    /* r6: end of linked address (or vm address)，由于目前阶段有用的数据是中断向量表+代码段+只读数据段+数据段，
											       所以只需复制[__exception_handlers,__bss_start]这段数据到内存基址处 */
    sub     r6, r7                              /* r6: delta of linked address (or vm address) | 内核镜像大小 */
    add     r6, r4                              /* r6: end of load address | 说明需拷贝[ r4,r4+r6 ] 区间内容到 [ r5,r5+r6 ]*/

reloc_img_to_bottom_loop://重定位镜像到内核物理内存基地址,将内核从加载地址拷贝到内存基址处
    ldr     r7, [r4], #4	// 类似C语言 *r5 = *r4 , r4++ , r5++ 
    str     r7, [r5], #4	// #4 代表32位的指令长度,此时在拷贝内核代码区内容
    cmp     r4, r6          /* 拷贝完成条件. r4++ 直到等于r6 (加载结束地址) 完成拷贝动作 */
    bne     reloc_img_to_bottom_loop
    sub     pc, r12                             /* 重新校准pc寄存器, 无缝跳到了拷贝后的指令地址处执行 r12是重定位镜像前内核加载基地址和内核物理内存基地址的差值 */
    nop		// 注意执行完成sub       pc, r12后,新的PC寄存器也指向了 	nop ,nop是伪汇编指令,等同于 mov r0 r0 通常用于控制时序的目的，强制内存对齐，防止流水线灾难，占据分支指令延迟						
    sub     r11, r11, r12                       /* r11: eventual address offset | 最终地址映射偏移量, 用于MMU打开后使用 */
//内核总大小 __bss_start - __exception_handlers
reloc_img_to_bottom_done:
#ifdef LOSCFG_KERNEL_MMU 
    ldr     r4, =g_firstPageTable               /* r4: physical address of translation table and clear it
												   内核页表是用数组g_firstPageTable存储 见于los_arch_mmu.c */
    add     r4, r4, r11                         //计算g_firstPageTable页表物理地址
    mov     r0, r4								//因为默认r0 将作为memset_optimized的第一个参数
    mov     r1, #0								//第二个参数,清0
    mov     r2, #MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS //第三个参数是L1表的长度
    bl      memset_optimized                    /* optimized memset since r0 is 64-byte aligned | 将内核页表空间清零*/

    ldr     r5, =g_archMmuInitMapping	        //记录映射关系表
    add     r5, r5, r11                         //获取g_archMmuInitMapping的物理地址
init_mmu_loop:	                                //初始化内核页表
    ldmia   r5!, {r6-r10}                       /* r6 = phys, r7 = virt, r8 = size, r9 = mmu_flags, r10 = name | 传参: 物理地址、虚拟地址、映射大小、映射属性、名称*/
    cmp     r8, 0                               /* if size = 0, the mmu init done | 完成条件 */
    beq     init_mmu_done		                //标志寄存器中Z标志位等于零时跳转到 	init_mmu_done处执行
    bl      page_table_build	                //创建页表
    b       init_mmu_loop						//循环继续
init_mmu_done:
    orr     r8, r4, #MMU_TTBRx_FLAGS            /* r8 = r4 and set cacheable attributes on translation walk | 设置缓存*/
    ldr     r4, =g_mmuJumpPageTable             /* r4: jump pagetable vaddr | 页表虚拟地址*/
    add     r4, r4, r11				
    ldr     r4, [r4]
    add     r4, r4, r11                         /* r4: jump pagetable paddr | 页表物理地址*/

    /* build 1M section mapping, in order to jump va during turing on mmu:pa == pa, va == pa */
    /* 从当前PC开始建立1MB空间的段映射，分别建立物理地址和虚拟地址方式的段映射页表项
     * 内核临时页表在系统 使能mmu -> 切换到虚拟地址运行 这段时间使用
     */
    mov     r6, pc
    mov     r7, r6                              /* r7: pa (MB aligned)*/
    lsr     r6, r6, #20                         /* r6: pa l1 index */
    ldr     r10, =MMU_DESCRIPTOR_KERNEL_L1_PTE_FLAGS
    add     r12, r10, r6, lsl #20               /* r12: pa |flags */
    str     r12, [r4, r7, lsr #(20 - 2)]        /* jumpTable[paIndex] = pt entry */
    rsb     r7, r11, r6, lsl #20                /* r7: va */
    str     r12, [r4, r7, lsr #(20 - 2)]        /* jumpTable[vaIndex] = pt entry */

    bl      mmu_setup                           /* set up the mmu | 内核映射表已经创建好了,此时可以启动MMU工作了*/
#endif
    /* clear out the interrupt and exception stack and set magic num to check the overflow 
    |exc_stack|地址高位
    |svc_stack|地址低位
	清除中断和异常堆栈并设置magic num检查溢出 */
    ldr     r0, =__svc_stack	    //stack_init的第一个参数 __svc_stack表示栈顶
    ldr     r1, =__exc_stack_top	//stack_init的第二个参数 __exc_stack_top表示栈底, 这里会有点绕, top表高地址位
    bl      stack_init              //初始化各个cpu不同模式下的栈空间
	//设置各个栈顶魔法数字
    STACK_MAGIC_SET __svc_stack, #OS_EXC_SVC_STACK_SIZE, OS_STACK_MAGIC_WORD //中断栈底设成"烫烫烫烫烫烫"
    STACK_MAGIC_SET __exc_stack, #OS_EXC_STACK_SIZE, OS_STACK_MAGIC_WORD     //异常栈底设成"烫烫烫烫烫烫"

warm_reset: //热启动 Warm Reset, warm reboot, soft reboot, 在不关闭电源的情况，由软件控制重启计算机
    /* initialize CPSR (machine state register) */
    mov    r0, #(CPSR_IRQ_DISABLE|CPSR_FIQ_DISABLE|CPSR_SVC_MODE) /* 禁止IRQ中断 | 禁止FIQ中断 | 管理模式-操作系统使用的保护模式 */
    msr    cpsr, r0	//设置CPSR寄存器

    /* Note: some functions in LIBGCC1 will cause a "restore from SPSR"!! */
    msr    spsr, r0 //设置SPSR寄存器

    /* get cpuid and keep it in r12 */
    mrc     p15, 0, r12, c0, c0, 5		//R12保存CPUID 
    and     r12, r12, #MPIDR_CPUID_MASK //掩码操作获取当前cpu id

    /* set svc stack, every cpu has OS_EXC_SVC_STACK_SIZE stack | 设置 SVC栈 */
    ldr    r0, =__svc_stack_top //注意这是栈底,高地址位
    mov    r2, #OS_EXC_SVC_STACK_SIZE //栈大小
    mul    r2, r2, r12 
    sub    r0, r0, r2                   /* 算出当前core的中断栈栈顶位置，写入所属core的sp */
    mov    sp, r0

    LDR    r0, =__exception_handlers    
    MCR    p15, 0, r0, c12, c0, 0       /* Vector Base Address Register - VBAR */

    cmp    r12, #0						//CPU是否为主核
    bne    cpu_start                    //不相等就跳到从核处理分支

clear_bss:	                            //主核处理.bss段清零
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

    bl     main                //带LR的子程序跳转, LR = pc - 4, 执行C层main函数

_start_hang:                   //悬停,相当于死循环,不再往下走了.
    b      _start_hang
#ifdef LOSCFG_KERNEL_MMU
mmu_setup:	//启动MMU工作
    mov     r12, #0                             /* TLB Invalidate All entries - TLBIALL */
    mcr     p15, 0, r12, c8, c7, 0              /* Set c8 to control the TLB and set the mapping to invalid */
    isb
    mcr     p15, 0, r12, c2, c0, 2              /* Translation Table Base Control Register(TTBCR) = 0x0
                                                   [31] :0 - Use the 32-bit translation system(虚拟地址是32位)
                                                   [5:4]:0 - use TTBR0和TTBR1
                                                   [2:0]:0 - TTBCR.N为0；
                                                   例如：TTBCR.N为0，TTBR0[31:14-0] | VA[31-0:20] | descriptor-type[1:0]组成32位页表描述符的地址,
                                                         VA[31:20]可以覆盖4GB的地址空间，所以TTBR0页表是16KB，不使用TTBR1;
                                                   例如：TTBCR.N为1，TTBR0[31:14-1] | VA[31-1:20] | descriptor-type[1:0]组成32位页表描述符的地址,
                                                         VA[30:20]可以覆盖2GB的地址空间，所以TTBR0页表是8KB，TTBR1页表是8KB(页表地址必须16KB对齐);
                                                */
    isb
    orr     r12, r4, #MMU_TTBRx_FLAGS			//将临时页表属性[6:0]和基地址[31:14]放到r12
    mcr     p15, 0, r12, c2, c0, 0              /* Set attributes and set temp page table */
    isb
    mov     r12, #0x7                           /* 0b0111 */
    mcr     p15, 0, r12, c3, c0, 0              /* Set DACR with 0b0111, client and manager domian */
    isb
    mrc    p15, 0, r12, c1, c0, 1               /* ACTLR, Auxlliary Control Register */
    orr    r12, r12, #(1 << 6)                  /* SMP, Enables coherent requests to the processor. */
    orr    r12, r12, #(1 << 2)                  /* Enable D-side prefetch */
    orr    r12, r12, #(1 << 11)                 /* Global BP Enable bit */
    mcr    p15, 0, r12, c1, c0, 1               /* ACTLR, Auxlliary Control Register */
    dsb
    /*
	 * 开始使能MMU，使用的是内核临时页表，这时cpu访问内存不管是取指令还是访问数据都是需要经过mmu来翻译，
	 * 但是在mmu使能之前cpu使用的都是内核的物理地址，即使现在使能了mmu，cpu访问的地址值还是内核的物理地址值(这里仅仅从数值上来看)，
	 * 而又由于mmu使能了，所以cpu会把这个值当做虚拟地址的值到页表中去找其对应的物理地址来访问。
	 * 所以现在明白了为什么要在内核临时页表里建立一个内核物理地址和虚拟地址一一映射的页表项了吧，因为建立了一一映射，
	 * cpu访问的地址经过mmu翻译得到的还是和原来一样的值，这样在cpu真正使用虚拟地址之前也能正常运行。
     */
    mrc     p15, 0, r12, c1, c0, 0
    bic     r12, #(1 << 29 | 1 << 28)           /* disable access flag[bit29]，ap[0]是访问权限位，支持全部的访问权限类型
                                                   disable TEX remap[bit28]，使用TEX[2:0]与C Bbit控制memory region属性 */
    orr     r12, #(1 << 0)                      /* mmu enable */
    bic     r12, #(1 << 1)
    orr     r12, #(1 << 2)                     /* D cache enable */
    orr     r12, #(1 << 12)                    /* I cache enable */
    mcr     p15, 0, r12, c1, c0, 0              /* Set SCTLR with r12: Turn on the MMU, I/D cache Disable TRE/AFE */
    isb
    ldr     pc,  =1f                            /* Convert to VA | 1表示标号，f表示forward(往下) - pc值取往下标识符“1”的虚拟地址(跳转到标识符“1”处)
                                                   因为之前已经在内核临时页表中建立了内核虚拟地址和物理地址的映射关系，所以接下来cpu切换到虚拟地址空间 */
1:
    mcr     p15, 0, r8, c2, c0, 0               /* Go to the base address saved in C2: Jump to the page table */
    isb                                         //r8中保存的是内核L1页表基地址和flags，r8写入到TTBR0实现临时页表和内核页表的切换
    mov     r12, #0
    mcr     p15, 0, r12, c8, c7, 0              /* TLB Invalidate All entries - TLBIALL(Invalidate all EL1&0 regime stage 1 and 2 TLB entries) */
    isb
    sub     lr,  r11                            /* adjust lr with delta of physical address and virtual address | 
                                                   lr中保存的是mmu使能之前返回地址的物理地址值，这时需要转换为虚拟地址，转换算法也很简单，虚拟地址 = 物理地址 - r11 */
    bx      lr                                  //返回
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
cpu_start: //由CPU从核启动系统
#ifdef LOSCFG_KERNEL_MMU
    ldr     r4, =g_firstPageTable               /* r4 = physical address of translation table and clear it */
    add     r4, r4, r11
    orr     r8, r4, #MMU_TTBRx_FLAGS            //内核页表物理基址值或将mmu属性标志后的值保存在r8，这个值也就是页表寄存器的初始化值

    ldr     r4, =g_mmuJumpPageTable             /* r4 = tt_trampoline vaddr */
    add     r4, r4, r11
    ldr     r4, [r4]
    add     r4, r4, r11                         /* r4 = tt_trampoline paddr */

    bl     mmu_setup                            /* 安装MMU */
#endif

    bl     HalSecondaryCpuStart	                //跳到从核CPU执行
    b      .                                    //注意 b . 指在当前地址运行,相当于死循环

secondary_cpu_init:                         	/* 从核初始化 */
#ifdef LOSCFG_TEE_ENABLE
    /* enable fpu+neon */
    ldr    r0, =0x60c00                         /* [11:10]使能安全和非安全访问协处理器10和11(Coprocessor 10和11)
                                                   [18:16]IMP DEF = 0x6 */
    MCR    p15, 0, r0, c1, c1, 2

    LDR    r0, =(0xF << 20)                     //允许在EL0和EL1下，访问协处理器10和11(控制Floating-point和Advanced SIMD特性)
    MCR    p15, 0, r0, c1, c0, 2
    cps    #0x16                                //CPSR寄存器的bit4-0更改为0x16,让处理器进入Monitor模式
    mov    r1, #0x25                            /* [5]:1 来自安全态的外部异常通过CPSA.A=1时屏蔽掉
                                                 * [2]:1 在monitor模式下采用FIQ异常
                                                 * [0]:1 cpu在非安全态 */
    mcr    p15, 0, r1, c1, c1, 0                //Secure Configuration Register - SCR
    cps    #0x13                                //CPSR寄存器的bit4-0更改为0x13,让处理器进入svc(supervisor)模式
#endif
    bl      warm_reset                          //热启动

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
/* 页表映射时，实际pa地址需要向下对齐到MB，size需要向上对齐到MB，这样才能保证映射范围包含pa~pa+size；下面是具体的计算过程 */
page_table_build: //创建页表
    mov     r10, r6
    bfc     r10, #20, #12                       /* r9: pa % MB */
    add     r8, r8, r10                         //size + 物理地址的低20bit位
    add     r8, r8, #(1 << 20)
    sub     r8, r8, #1
    lsr     r6, #20                             /* r6 = physical address/MB */
    lsr     r7, #20                             /* r7 = virtual address/MB */
    lsr     r8, #20                             /* r8 = roundup(size, MB) */

page_table_build_loop:
    orr     r12, r9, r6, lsl #20                /* r12: flags | physAddr */
    str     r12, [r4, r7, lsl #2]               /* gPgTable[l1Index] = physAddr | flags */
                                                /* r7是va的l1index, [r7, lsl #2]表示每次地址偏移4，例如r7是1表示l1index是1；
                                                  页表项的存储地址为r4+(1<<2)，因为每个页表项是4个字节数据 */
    add     r6, #1                              /* physAddr+ */
    add     r7, #1                              /* l1Index++ */
    subs    r8, #1                              /* sizes-- */
    bne     page_table_build_loop
    bx      lr
#endif
/*
 * init stack to initial value | 初始化栈
 * r0 is stack mem start, r1 is stack mem end | 初始化范围 [ r0 = __svc_stack , r1 = __exc_stack_top ]
 */
stack_init:
    ldr     r2, =OS_STACK_INIT	//0xCACACACA
    ldr     r3, =OS_STACK_INIT
    /* Main loop sets 32 bytes at a time. | 主循环一次设置 32 个字节*/
stack_init_loop:
    .irp    offset, #0, #8, #16, #24
    strd    r2, r3, [r0, \offset]    /* 等价于strd r2, r3, [r0, 0], strd r2, r3, [r0, 8], ... , strd r2, r3, [r0, 24] */
    .endr
    add     r0, #32			//加跳32个字节,说明在地址范围上 r1 > r0 ==> __exc_stack_top > __svc_stack
    cmp     r0, r1			//是否到栈底
    blt     stack_init_loop
    bx      lr

/*
 * 内核的运行地址和物理地址是什么意思呢？
 * 内核的运行地址就是内核设计者希望内核运行时在内存中的位置，这个地址在内核源码中有地方可以配置，(见链接脚本board.ld.S)
 * 并且链接脚本里也会用到这个地址，编译代码时所用到的跟地址相关的值都是以内核运行基址为基础进行计算的。
*/
pa_va_offset:	//物理地址和虚拟地址偏移量
    .word   .   ////定义一个4字节的pa_va_offset 变量， 链接器生成一个链接地址， . 表示 pa_va_offset = 链接地址 举例: 在地址 0x17321796 中保存了 0x17321796 值

/*
 * set magic num to stack bottom for all cpu | 给所有CPU核设置魔法数字到栈底
 * r0 is stack bottom, r1 is stack size, r2 is magic num
 */
excstack_magic:
    mov     r3, #0 //r3 = 0
excstack_magic_loop:
    str     r2, [r0]   //栈顶设置魔法数字
    add     r0, r0, r1 //定位到栈底
    add     r3, r3, #1 //r3++
    cmp     r3, #CORE_NUM //栈空间等分成core_num个空间，所以每个core的栈顶需要magic num
    blt     excstack_magic_loop
    bx      lr

#ifdef LOSCFG_KERNEL_MMU
memset_optimized:	//初始化页表
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
    * Temporary interrupt stack
    */
    .section ".int_stack", "wa", %nobits //见 liteos.ld 第 152 行
    .align  3
//CPU各个模式下的栈空间(目前是两个模式,早先代码是6个模式都有自己的栈空间),每个CPU核都有自己的中断栈和异常栈
/*
 * svc栈空间的分布布局
 *
    ------------------------  __svc_stack_top
    +     CORE_NUM - 1     +
    ------------------------
    +     CORE_NUM - 2     +      ^
    ------------------------      ^
    +         ...          +      ^
    ------------------------
    +          0           +
    ------------------------  __svc_stack
 */
__svc_stack:
    .space OS_EXC_SVC_STACK_SIZE * CORE_NUM //中断栈 8K
__svc_stack_top:

__exc_stack:
    .space OS_EXC_STACK_SIZE * CORE_NUM //异常栈 4K
__exc_stack_top:
