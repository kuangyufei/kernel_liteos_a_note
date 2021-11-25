/*!
 * @file    los_asid.c
 * @brief
 * @link http://weharmonyos.com/blog/14.html
 * @verbatim
	asid(Adress Space ID) 进程标识符，属于CP15协处理器的C13号寄存器，ASID可用来唯一标识进程，并为进程提供地址空间保护。
	当TLB试图解析虚拟页号时，它确保当前运行进程的ASID与虚拟页相关的ASID相匹配。如果不匹配，那么就作为TLB失效。
	除了提供地址空间保护外，ASID允许TLB同时包含多个进程的条目。如果TLB不支持独立的ASID，每次选择一个页表时（例如，上下文切换时），
	TLB就必须被冲刷（flushed）或删除，以确保下一个进程不会使用错误的地址转换。

	TLB页表中有一个bit来指明当前的entry是global(nG=0，所有process都可以访问)还是non-global(nG=1，only本process允许访问)。
	如果是global类型，则TLB中不会tag ASID；如果是non-global类型，则TLB会tag上ASID，
	且MMU在TLB中查询时需要判断这个ASID和当前进程的ASID是否一致，只有一致才证明这条entry当前process有权限访问。

	看到了吗？如果每次mmu上下文切换时，把TLB全部刷新已保证TLB中全是新进程的映射表，固然是可以，但效率太低了！！！
	进程的切换其实是秒级亚秒级的，地址的虚实转换是何等的频繁啊，怎么会这么现实呢，
	真实的情况是TLB中有很多很多其他进程占用的物理内存的记录还在，当然他们对物理内存的使用权也还在。
	所以当应用程序 new了10M内存以为是属于自己的时候，其实在内核层面根本就不属于你，还是别人在用，
	只有你用了1M的那一瞬间真正1M物理内存才属于你，而且当你的进程被其他进程切换后，很大可能你用的那1M也已经不在物理内存中了，
	已经被置换到硬盘上了。明白了吗？只关注应用开发的同学当然可以说这关我鸟事，给我的感觉有就行了，但想熟悉内核的同学就必须要明白，
	这是每分每秒都在发生的事情。
 * @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-11-16
 *
 * @history
 *
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

/**
 * @defgroup los_asid mmu address space id
 * @ingroup kernel
 */

#include "los_asid.h"
#include "los_bitmap.h"
#include "los_spinlock.h"
#include "los_mmu_descriptor_v6.h"


#ifdef LOSCFG_KERNEL_VM

STATIC SPIN_LOCK_INIT(g_cpuAsidLock); ///< asid专属自旋锁
STATIC UINTPTR g_asidPool[BITMAP_NUM_WORDS(1UL << MMU_ARM_ASID_BITS)]; ///< 地址空间ID池 , 2^8 = 256 个

/* allocate and free asid | 分配地址空间ID */
status_t OsAllocAsid(UINT32 *asid)
{
    UINT32 flags;
    LOS_SpinLockSave(&g_cpuAsidLock, &flags);
    UINT32 firstZeroBit = LOS_BitmapFfz(g_asidPool, 1UL << MMU_ARM_ASID_BITS);//找到第一个0位,即可分配位
    if (firstZeroBit >= 0 && firstZeroBit < (1UL << MMU_ARM_ASID_BITS)) {
        LOS_BitmapSetNBits(g_asidPool, firstZeroBit, 1);//设为已分配
        *asid = firstZeroBit;//由参数带走
        LOS_SpinUnlockRestore(&g_cpuAsidLock, flags);
        return LOS_OK;
    }

    LOS_SpinUnlockRestore(&g_cpuAsidLock, flags);
    return firstZeroBit;
}


/// 释放 asid
VOID OsFreeAsid(UINT32 asid)
{
    UINT32 flags;
    LOS_SpinLockSave(&g_cpuAsidLock, &flags);
    LOS_BitmapClrNBits(g_asidPool, asid, 1);//清空对应位,可继续分配给其他进程
    LOS_SpinUnlockRestore(&g_cpuAsidLock, flags);
}
#endif

