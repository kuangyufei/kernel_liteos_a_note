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
/***********************************************
本文件经过
python build.py ipcamera_hi3516dv300 

编译后生成 board.ld,内容如下:
#include "los_vm_zone.h"

#define TEXT_BASE  KERNEL_VADDR_BASE //代码区为内核起始地址

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)

MEMORY
{
    ram : ORIGIN = KERNEL_VADDR_BASE, LENGTH = KERNEL_VADDR_SIZE
    sram : ORIGIN = 0x40000000, LENGTH = 0x1000
    user_ram : ORIGIN = 0x1000000, LENGTH = 0x100000
}
SECTIONS
{
    /DISCARD/ : { *(.comment .note) }

    .ram_vectors TEXT_BASE : {
        __ram_vectors_vma = .;
        KEEP (*(.vectors))
    } > ram
    __ram_vectors_lma = LOADADDR(.ram_vectors);
}

USER_INIT_VM_START = 0x1000000;
***********************************************/
/* 
https://www.jianshu.com/p/42823b3b7c8e
https://sourceware.org/binutils/docs/ld/MEMORY.html
MEMORY：内存布局,描述板上的存储器位置
语法为
NAME1 [(ATTR)] : ORIGIN = ORIGIN1, LENGTH = LEN2

NAME ：存储区域的名字。（自己可以随意命名）
ATTR ：定义该存储区域的属性。ATTR属性内可以出现以下7 个字符：
	R 只读section
	W 读/写section
	X 可执行section
	A 可分配的section
	I 初始化了的section
	L 同I
	! 不满足该字符之后的任何一个属性的section
ORIGIN ：关键字，区域的开始地址，可简写成org 或o
LENGTH ：关键字，区域的大小，可简写成len 或l
*/

#include "los_vm_zone.h"
#define TEXT_BASE  KERNEL_VADDR_BASE //代码区为内核起始地址

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)

MEMORY //链接器的默认配置允许分配所有可用内存,描述链接器可以使用哪些内存区域
{ //ram,sram为存储区域的名字,可以随意取
    ram : ORIGIN = KERNEL_VADDR_BASE, LENGTH = KERNEL_VADDR_SIZE //内核运行空间范围
    sram : ORIGIN = 0x40000000, LENGTH = 0x1000 
    user_ram : ORIGIN = 0x1000000, LENGTH = 0x100000 //用户运行空间范围 USER_ASPACE_BASE ,此大小不是真正最后映射到用户空间的大小
}
SECTIONS
{
    /DISCARD/ : { *(.comment .note) } //过滤掉所有输入文件的 .comment .note 段

    .ram_vectors TEXT_BASE : { //内核中断向量区开始位置,此处用 TEXT_BASE 宏有误导嫌疑, 因为真正的(.text)并不在此 @notethinking 
        __ram_vectors_vma = .; //定位到当前位置,即TEXT_BASE处
        KEEP (*(.vectors)) //告诉链接器 强制保留所有输入文件中的 .vectors 节
    } > ram //中断向量是开机代码的位置 , 可翻看 鸿蒙内核源码分析(开机启动篇) 和 (中断管理篇)
    __ram_vectors_lma = LOADADDR(.ram_vectors);//启动时对向量的初始化,加载地址和链接地址一致,说明内核设计者希望从加载地址处运行指令
}
//LMA：加载存储地址，指加载到存储器的地址，即加载或烧写到哪里
//VMA：虚拟存储地址，也就是链接地址，即代码和数据运行的时候应在哪里
USER_INIT_VM_START = 0x1000000; //用户空间初始地址 
