/*!
 * @file  los_vm_iomap.c
 * @brief DMA
 * @link
   @verbatim
   直接内存访问
		直接内存访问（Direct Memory Access，DMA）是计算机科学中的一种内存访问技术。它允许某些电脑内部的
		硬件子系统（电脑外设），可以独立地直接读写系统内存，而不需中央处理器（CPU）介入处理
		在同等程度的处理器负担下，DMA是一种快速的数据传送方式。很多硬件的系统会使用DMA，包含硬盘控制器、
		绘图显卡、网卡和声卡。

		DMA是所有现代电脑的重要特色，它允许不同速度的硬件设备来沟通，而不需要依于中央处理器的大量中断负载。
		否则，中央处理器需要从来源把每一片段的资料复制到寄存器，然后把它们再次写回到新的地方。在这个时间中，
		中央处理器对于其他的工作来说就无法使用。

		DMA传输常使用在将一个内存区从一个设备复制到另外一个。当中央处理器初始化这个传输动作，传输动作本身是
		由DMA控制器来实行和完成。典型的例子就是移动一个外部内存的区块到芯片内部更快的内存去。像是这样的操作
		并没有让处理器工作拖延，使其可以被重新调度去处理其他的工作。DMA传输对于高性能嵌入式系统算法和网络是
		很重要的。 举个例子，个人电脑的ISA DMA控制器拥有8个DMA通道，其中的7个通道是可以让计算机的中央处理器所利用。
		每一个DMA通道有一个16位地址寄存器和一个16位计数寄存器。要初始化资料传输时，设备驱动程序一起设置DMA通道的
		地址和计数寄存器，以及资料传输的方向，读取或写入。然后指示DMA硬件开始这个传输动作。当传输结束的时候，
		设备就会以中断的方式通知中央处理器。

		"分散-收集"（Scatter-gather）DMA允许在一次单一的DMA处理中传输资料到多个内存区域。相当于把多个简单的DMA要求
		串在一起。同样，这样做的目的是要减轻中央处理器的多次输出输入中断和资料复制任务。 
		DRQ意为DMA要求；DACK意为DMA确认。这些符号一般在有DMA功能的电脑系统硬件概要上可以看到。
	    它们表示了介于中央处理器和DMA控制器之间的电子信号传输线路。
	   
   缓存一致性问题
		DMA会导致缓存一致性问题。想像中央处理器带有缓存与外部内存的情况，DMA的运作则是去访问外部内存，
		当中央处理器访问外部内存某个地址的时候，暂时先将新的值写入缓存中，但并未将外部内存的资料更新，
		若在缓存中的资料尚未更新到外部内存前发生了DMA，则DMA过程将会读取到未更新的资料。
		相同的，如果外部设备写入新的值到外部内存内，则中央处理器若访问缓存时则会访问到尚未更新的资料。
		这些问题可以用两种方法来解决：

		缓存同调系统（Cache-coherent system）：以硬件方法来完成，当外部设备写入内存时以一个信号来通知
			缓存控制器某内存地址的值已经过期或是应该更新资料。
		非同调系统（Non-coherent system）：以软件方法来完成，操作系统必须确认缓存读取时，DMA程序已经
			开始或是禁止DMA发生。
		第二种的方法会造成DMA的系统负担。
   @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2022-04-02
 */

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

#include "los_vm_iomap.h"
#include "los_printf.h"
#include "los_vm_zone.h"
#include "los_vm_common.h"
#include "los_vm_map.h"
#include "los_memory.h"

/// 分配DMA空间
VOID *LOS_DmaMemAlloc(DMA_ADDR_T *dmaAddr, size_t size, size_t align, enum DmaMemType type)
{
    VOID *kVaddr = NULL;

    if (size == 0) {
        return NULL;
    }

    if ((type != DMA_CACHE) && (type != DMA_NOCACHE)) {
        VM_ERR("The dma type = %d is not supported!", type);
        return NULL;
    }

#ifdef LOSCFG_KERNEL_VM
    kVaddr = LOS_KernelMallocAlign(size, align);//不走内存池方式, 直接申请物理页
#else
    kVaddr = LOS_MemAllocAlign(OS_SYS_MEM_ADDR, size, align);//从内存池中申请
#endif
    if (kVaddr == NULL) {
        VM_ERR("failed, size = %u, align = %u", size, align);
        return NULL;
    }

    if (dmaAddr != NULL) {
        *dmaAddr = (DMA_ADDR_T)LOS_PaddrQuery(kVaddr);//查询物理地址, DMA直接将数据灌到物理地址
    }

    if (type == DMA_NOCACHE) {//无缓存模式 , 计算新的虚拟地址
        kVaddr = (VOID *)VMM_TO_UNCACHED_ADDR((UINTPTR)kVaddr);
    }

    return kVaddr;
}
/// 释放 DMA指针
VOID LOS_DmaMemFree(VOID *vaddr)
{
    UINTPTR addr;

    if (vaddr == NULL) {
        return;
    }
    addr = (UINTPTR)vaddr;
	// 未缓存区
    if ((addr >= UNCACHED_VMM_BASE) && (addr < UNCACHED_VMM_BASE + UNCACHED_VMM_SIZE)) {
        addr = UNCACHED_TO_VMM_ADDR(addr); //转换成未缓存区地址
#ifdef LOSCFG_KERNEL_VM
        LOS_KernelFree((VOID *)addr);// 
#else
        LOS_MemFree(OS_SYS_MEM_ADDR, (VOID *)addr);//内存池方式释放
#endif
    } else if ((addr >= KERNEL_VMM_BASE) && (addr < KERNEL_VMM_BASE + KERNEL_VMM_SIZE)) {
#ifdef LOSCFG_KERNEL_VM
        LOS_KernelFree((VOID *)addr);
#else
        LOS_MemFree(OS_SYS_MEM_ADDR, (VOID *)addr);
#endif
    } else {
        VM_ERR("addr %#x not in dma area!!!", vaddr);
    }
    return;
}
/// 将DMA虚拟地址转成物理地址
DMA_ADDR_T LOS_DmaVaddrToPaddr(VOID *vaddr)
{
    return (DMA_ADDR_T)LOS_PaddrQuery(vaddr);
}

