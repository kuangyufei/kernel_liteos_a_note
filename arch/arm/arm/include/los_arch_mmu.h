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
/*!
 * @file    los_arch_mmu.h
 * @brief
 *
 * \n
 * @link https://blog.csdn.net/kuangyufei/article/details/109032636 
 * \n 	https://blog.csdn.net/qq_38410730/article/details/81036768
 * @verbatim
	 https://blog.csdn.net/kuangyufei/article/details/109032636
	 https://blog.csdn.net/qq_38410730/article/details/81036768
	 虚拟内存空间中的地址叫做“虚拟地址”；而实际物理内存空间中的地址叫做“实际物理地址”或“物理地址”。处理器
	 运算器和应用程序设计人员看到的只是虚拟内存空间和虚拟地址，而处理器片外的地址总线看到的只是物理地址空间和物理地址。
	 MMU是 MemoryManagementUnit 的缩写即，内存管理单元. 
	 MMU 的作用：
	 1. 将虚拟地址翻译成为物理地址，然后访问实际的物理地址
	 2. 访问权限控制
	 当处理器试图访问一个虚存页面时，首先到页表中去查询该页是否已映射到物理页框中，并记录在页表中。如果在，
	 则MMU会把页码转换成页框码，并加上虚拟地址提供的页内偏移量形成物理地址后去访问物理内存；如果不在，
	 则意味着该虚存页面还没有被载入内存，这时MMU就会通知操作系统：发生了一个页面访问错误（页面错误），
	 接下来系统会启动所谓的“请页”机制，即调用相应的系统操作函数，判断该虚拟地址是否为有效地址。
	 
	 如果是有效的地址，就从虚拟内存中将该地址指向的页面读入到内存中的一个空闲页框中，并在页表中添加上
	 相对应的表项，最后处理器将从发生页面错误的地方重新开始运行；如果是无效的地址，则表明进程在试图访问
	 一个不存在的虚拟地址，此时操作系统将终止此次访问。
 * @endverbatim
 *
 * \n
 * @version 
  * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2025-07-09
 *
 * @history
 *
 */
/**
 * @defgroup los_arch_mmu architecture mmu
 * @ingroup kernel
 */

#ifndef __LOS_ARCH_MMU_H__
#define __LOS_ARCH_MMU_H__

#include "los_typedef.h"
#include "los_vm_phys.h"
#ifndef LOSCFG_PAGE_TABLE_FINE_LOCK
#include "los_spinlock.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @brief ARM架构MMU（内存管理单元）控制块结构
 * @details 该结构用于管理ARM处理器的内存管理单元相关资源，包括页表基地址、ASID和页表链表等
 */
typedef struct ArchMmu {
#ifndef LOSCFG_PAGE_TABLE_FINE_LOCK
    SPIN_LOCK_S         lock;           /**< 页表项修改自旋锁，用于多核心环境下的页表操作同步 */
#endif
    VADDR_T             *virtTtb;       /**< 转换表基址虚拟地址，指向页表的虚拟内存起始位置 */
    PADDR_T             physTtb;        /**< 转换表基址物理地址，硬件MMU实际使用的物理页表基地址 */
    UINT32              asid;           /**< 地址空间标识符(Address Space Identifier)，用于TLB上下文隔离 */
    LOS_DL_LIST         ptList;         /**< 页表虚拟内存页面链表，管理所有已分配的页表页面节点 */
} LosArchMmu;

BOOL OsArchMmuInit(LosArchMmu *archMmu, VADDR_T *virtTtb);
STATUS_T LOS_ArchMmuQuery(const LosArchMmu *archMmu, VADDR_T vaddr, PADDR_T *paddr, UINT32 *flags);
STATUS_T LOS_ArchMmuUnmap(LosArchMmu *archMmu, VADDR_T vaddr, size_t count);
STATUS_T LOS_ArchMmuMap(LosArchMmu *archMmu, VADDR_T vaddr, PADDR_T paddr, size_t count, UINT32 flags);
STATUS_T LOS_ArchMmuChangeProt(LosArchMmu *archMmu, VADDR_T vaddr, size_t count, UINT32 flags);
STATUS_T LOS_ArchMmuMove(LosArchMmu *archMmu, VADDR_T oldVaddr, VADDR_T newVaddr, size_t count, UINT32 flags);
VOID LOS_ArchMmuContextSwitch(LosArchMmu *archMmu);
STATUS_T LOS_ArchMmuDestroy(LosArchMmu *archMmu);
VOID OsArchMmuInitPerCPU(VOID);
VADDR_T *OsGFirstTableGet(VOID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* __LOS_ARCH_MMU_H__ */

