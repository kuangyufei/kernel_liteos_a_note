/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific prior written
 * permission.
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

/* 
	链接脚本, 各板子按自己实际情况修改
	GNU 链接脚本语法 https://sourceware.org/binutils/docs/ld/LD-Index.html
*/
ENTRY(reset_vector) /*指定程序入口地址*/
INCLUDE board.ld // > ram 指放入ram这个地址范围中, ram在board.ld中定义
/* SECTIONS 是脚本中最重要的命令，所有的LD脚本都会有这个命令，用来指定如何将输入文件映射到输出文件等等 */
SECTIONS 
{		 
	 //节地址是指该节的VMA地址。如果改地未明确指定，连接器会在考虑严格对齐情况下，首先按照region参数分配内存，其次根据当前位置计数器向下分地址。
     _start = .; //特殊符号 .表示当前位置计数器,即紧挨着中断向量结束的位置 
    .set_sysinit_set : {
        __start_set_sysinit_set = ABSOLUTE(.); //
        KEEP (*(.set_sysinit_set))//所有输入文件中的 .set_sysinit_set 链接到此
        __stop_set_sysinit_set = ABSOLUTE(.);//定位结束 .set_sysinit_set 区结束位置
    } > ram

    .got ALIGN(0x4) : { *(.got.plt) *(.got) } > ram

    .gcc_except_table ALIGN (0x8) : { . = .; } > ram  .gcc_except_table : { KEEP(*(.gcc_except_table*)) }
    .exception_ranges ALIGN (0x8) : ONLY_IF_RW { *(.exception_ranges .exception_ranges*) } > ram

    .ARM.extab ALIGN(0x4) : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > ram

    /* .ARM.exidx is sorted, so has to go in its own output section.  */
    .ARM.exidx ALIGN(0x8) : { __exidx_start = .; *(.ARM.exidx* .gnu.linkonce.armexidx.*) ;__exidx_end = .;} > ram

    /* text/read-only data */
    .text ALIGN(0x1000) : { //代码区 按4K对齐
        __text_start = .; //当前位置为 __text_start 开始位置
        *(.text* .sram.text.glue_7* .gnu.linkonce.t.*) //*(.text)
    } > ram
	//重定向代码 Relocation ,包括 代码区和数据区的重定位
    .rel.text : { *(.rel.text) *(.rel.text.*) *(.rel.gnu.linkonce.t*) } > ram
    .rela.text : { *(.rela.text) *(.rela.text.*) *(.rela.gnu.linkonce.t*) } > ram
    .rel.data : { *(.rel.data) *(.rel.data.*) *(.rel.gnu.linkonce.d*) } > ram
    .rela.data : { *(.rela.data) *(.rela.data.*) *(.rela.gnu.linkonce.d*) } > ram
    .rel.rodata : { *(.rel.rodata) *(.rel.rodata.*) *(.rel.gnu.linkonce.r*) } > ram
    .rela.rodata : { *(.rela.rodata) *(.rela.rodata.*) *(.rela.gnu.linkonce.r*) } > ram
    .rel.got : { *(.rel.got) } > ram
    .rela.got : { *(.rela.got) } > ram
    .rel.ctors : { *(.rel.ctors) } > ram
    .rela.ctors : { *(.rela.ctors) } > ram
    .rel.dtors : { *(.rel.dtors) } > ram
    .rela.dtors : { *(.rela.dtors) } > ram
    .rel.init : { *(.rel.init) } > ram
    .rela.init : { *(.rela.init) } > ram
    .rel.fini : { *(.rel.fini) } > ram
    .rela.fini : { *(.rela.fini) } > ram
    .rel.bss : { *(.rel.bss) } > ram
    .rela.bss : { *(.rela.bss) } > ram
    .rel.plt : { *(.rel.plt) } > ram
    .rela.plt : { *(.rela.plt) } > ram
    .rel.dyn : { *(.rel.dyn) } > ram

    .dummy_post_text : {
        __text_end = .; //代码区结束位置
    } > ram

    .rodata ALIGN(0x1000) : {
        __rodata_start = .; // 只读数据区开始位置
        __kernel_init_level_0 = ABSOLUTE(.);
        KEEP(*( SORT (.rodata.init.kernel.0.*)));
        __kernel_init_level_1 = ABSOLUTE(.);
        KEEP(*( SORT (.rodata.init.kernel.1.*)));
        __kernel_init_level_2 = ABSOLUTE(.);
        KEEP(*( SORT (.rodata.init.kernel.2.*)));
        __kernel_init_level_3 = ABSOLUTE(.);
        KEEP(*( SORT (.rodata.init.kernel.3.*)));
        __kernel_init_level_4 = ABSOLUTE(.);
        KEEP(*( SORT (.rodata.init.kernel.4.*)));
        __kernel_init_level_5 = ABSOLUTE(.);
        KEEP(*( SORT (.rodata.init.kernel.5.*)));
        __kernel_init_level_6 = ABSOLUTE(.);
        KEEP(*( SORT (.rodata.init.kernel.6.*)));
        __kernel_init_level_7 = ABSOLUTE(.);
        KEEP(*( SORT (.rodata.init.kernel.7.*)));
        __kernel_init_level_8 = ABSOLUTE(.);
        KEEP(*( SORT (.rodata.init.kernel.8.*)));
        __kernel_init_level_9 = ABSOLUTE(.);
        KEEP(*( SORT (.rodata.init.kernel.9.*)));
        __kernel_init_level_10 = ABSOLUTE(.);
        *(.rodata .rodata.* .gnu.linkonce.r.*)
        __exc_table_start = .;  //异常表开始位置
        KEEP(*(__exc_table))
        __exc_table_end = .;	//异常表结束位置
    } > ram

    /*
     * extra linker scripts tend to insert sections just after .rodata,
     * so we want to make sure this symbol comes after anything inserted above,
     * but not aligned to the next section necessarily.
     */
    .dummy_post_rodata : {
        _hdf_drivers_start = .;
        KEEP(*(.hdf.driver)) // 统一驱动框架也放在了只读区
        _hdf_drivers_end = .;
        __rodata_end = .; //数据只读区结束
    } > ram

    .data ALIGN(0x1000) : {
        /* writable data  */
        __ram_data_start = .; //可写入数据开始位置
        __vdso_data_start = LOADADDR(.data); // vdso区 (virtual dynamic shared object)开始位置 
        KEEP(*(.data.vdso.datapage)) //vdso由数据区+代码区两部分组成, 可翻看 鸿蒙内核源码分析(vdso篇) 
        . = ALIGN(0x1000);//vdso 的特性是 代码在内核区, 但运行却在用户区
        KEEP(*(.data.vdso.text))//vdso 的代码区
        . = ALIGN(0x1000);//按4K对齐,因啥要按4K对齐,因为需要页表映射,而一页为4K
        __vdso_text_end = .;//vdso区结束位置
        *(.data .data.* .gnu.linkonce.d.*)
        . = ALIGN(0x4);
        KEEP(*( SORT (.liteos.table.*)));
    } > ram

    .ctors : ALIGN(0x4) { //constructor属性 全局构造
        __ctor_list__ = .;
        KEEP (*(.ctors .init_array))
        __ctor_end__ = .;
    } > ram
    .dtors : ALIGN(0x4) { //destructor属性 全局析构
        __dtor_list__ = .;
        KEEP (*(.dtors .fini_array))
        __dtor_end__ = .;
    } > ram
    /*
     * extra linker scripts tend to insert sections just after .data,
     * so we want to make sure this symbol comes after anything inserted above,
     * but not aligned to the next section necessarily.
     */
    .dummy_post_data : {
        __ram_data_end = .; //可写入数据区结束
    } > ram
	//这里指的是 init 应用程度的位置
    .user_init USER_INIT_VM_START : ALIGN(0x1000) {//开始地址设为 USER_INIT_VM_START = 0x1000000;
        . = ALIGN(0x4);
        __user_init_load_addr = LOADADDR(.user_init);//应用程序的加载地址
        __user_init_entry = .;//应用程序的入口地址
        KEEP(libuserinit.O (.user.entry))
        KEEP(libuserinit.O (.user.text))
        KEEP(libuserinit.O (.user.rodata))
        . = ALIGN(0X4);
        __user_init_data = .;//设置数据段开始位置 __user_init_data
        KEEP(libuserinit.O (.user.data))
        . = ALIGN(0X4);
        __user_init_bss = .;//init 进程的 bss开始位置
        KEEP(libuserinit.O (.user.bss))
        . = ALIGN(0x1000);
        __user_init_end = .; //init 进程结束位置
    } > user_ram AT > ram

    __user_init_size = __user_init_end - __user_init_entry; //计算init进程占用大小

    /* uninitialized data (in same segment as writable data) | 未初始化数据 */
    .bss : {
        . = ALIGN(0x800); //当前位置按 0x800对齐
        __int_stack_start = .; //内核栈开始位置
        *(.int_stack);
        . = ALIGN(0x4);//4字节对齐
        KEEP(*(.bss.prebss.*))
        . = ALIGN(0x8);
        __bss_start = .;	//将当前位置给__bss_start,将所有的目标*(.bss .bss.*) .. 链接到 .bss中
        *(.bss .bss.*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(0x8);
        __bss_end = .;
    } > ram

    . = ALIGN(0x1000);
    _end = .;
    /* mmu temp page table(sys aviliable mem is start with __bss_end) */
    . = ALIGN(0x4000);
    __mmu_ttlb_begin = .;//临时页表开始位置

    /* Strip unnecessary stuff */
    /DISCARD/ 0 : { *(.comment .note) } > ram //过滤不需要的块
}
