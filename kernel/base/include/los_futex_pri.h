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
 * @file    los_futex_pri.h
 * @brief
 * @link
   @verbatim
	FUTEX_WAIT
		这个操作用来检测有uaddr指向的futex是否包含关心的数值val，如果是，则继续sleep直到FUTEX_WAKE操作触发。
		加载futex的操作是原子的。这个加载，从比较关心的数值，到开始sleep，都是原子的，与另外一个对于同一个
		futex的操作是线性的，串行的，严格按照顺序来执行的。如果线程开始sleep，就表示有一个waiter在futex上。
		如果futex的值不匹配，回调直接返回失败，错误代码是EAGAIN。

		与期望值对比的目的是为了防止丢失唤醒的操作。如果另一个线程在基于前面的数值阻塞调用之后，修改了这个值，
		另一个线程在数值改变之后，调用FUTEX_WAIT之前执行了FUTEX_WAKE操作，这个调用的线程就会观察到数值变换并且无法唤醒。
		这里的意思是，调用FUTEX_WAIT需要做上面的一个操作，就是检测一下这个值是不是我们需要的，如果不是就等待，
		如果是就直接运行下去。之所以检测是为了避免丢失唤醒，也就是防止一直等待下去，比如我们在调用FUTEX_WAIT之前，
		另一个线程已经调用了FUTEX_WAKE，那么就不会有线程调用FUTEX_WAKE，调用FUTEX_WAIT的线程就永远等不到信号了，也就永远唤醒不了了。

		如果timeout不是NULL，就表示指向了一个特定的超时时钟。这个超时间隔使用系统时钟的颗粒度四舍五入，
		可以保证触发不会比定时的时间早。默认情况通过CLOCK_MONOTONIC测量，但是从Linux 4.5开始，可以在futex_op中设置
		FUTEX_CLOCK_REALTIME使用CLOCK_REALTIME测量。如果timeout是NULL，将会永远阻塞。

		注意：对于FUTEX_WAIT，timeout是一个关联的值。与其他的futex设置不同，timeout被认为是一个绝对值。
		使用通过FUTEX_BITSET_MATCH_ANY特殊定义的val3传入FUTEX_WAIT_BITSET可以获得附带timeout的FUTEX_WAIT的值。		
   @endverbatim	
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2025-07-11
 */

#ifndef _LOS_FUTEX_PRI_H
#define _LOS_FUTEX_PRI_H
#include "los_list.h"

/**
 * @brief FUTEX操作类型定义
 * @details 定义了FUTEX（快速用户空间互斥体）支持的各类操作命令
 */
#define FUTEX_WAIT        0   ///< 等待操作：阻塞等待FUTEX值变化
#define FUTEX_WAKE        1   ///< 唤醒操作：唤醒一个或多个等待的线程
#define FUTEX_REQUEUE     3   ///< 重排队操作：将等待线程从一个FUTEX转移到另一个
#define FUTEX_WAKE_OP     5   ///< 唤醒并操作：唤醒线程并对FUTEX值执行原子操作
#define FUTEX_LOCK_PI     6   ///< PI锁定操作：获取优先级继承锁
#define FUTEX_UNLOCK_PI   7   ///< PI解锁操作：释放优先级继承锁
#define FUTEX_TRYLOCK_PI  8   ///< PI尝试锁定：非阻塞方式尝试获取优先级继承锁
#define FUTEX_WAIT_BITSET 9   ///< 位集等待：带位掩码的等待操作

/**
 * @brief FUTEX标志定义
 * @details 用于指定FUTEX的属性和行为特征
 */
#define FUTEX_PRIVATE     128 ///< 私有FUTEX标志：仅本进程内可见
#define FUTEX_MASK        0x3U ///< FUTEX操作掩码：用于提取操作类型（十六进制0x3U对应十进制3）

/**
 * @brief FUTEX节点结构体
 * @details 描述FUTEX系统中的一个节点，用于管理等待线程队列和相关属性
 */
typedef struct {
    UINTPTR      key;           ///< FUTEX键值：私有FUTEX使用虚拟地址，共享FUTEX使用物理地址
    UINT32       index;         ///< 哈希桶索引：用于FUTEX哈希表中的定位
    UINT32       pid;           ///< 进程ID：私有FUTEX存储所属进程ID，共享FUTEX固定为OS_INVALID(-1)
    LOS_DL_LIST  pendList;      ///< 等待链表：指向线程控制块(TCB)中的等待链表
    LOS_DL_LIST  queueList;     ///< 阻塞队列：因该FUTEX而阻塞的线程链表
    LOS_DL_LIST  futexList;     ///< FUTEX链表：指向下一个FUTEX节点，用于哈希冲突解决
} FutexNode;

extern UINT32 OsFutexInit(VOID);
extern VOID OsFutexNodeDeleteFromFutexHash(FutexNode *node, BOOL isDeleteHead, FutexNode **headNode, BOOL *queueFlags);
extern INT32 OsFutexWake(const UINT32 *userVaddr, UINT32 flags, INT32 wakeNumber);
extern INT32 OsFutexWait(const UINT32 *userVaddr, UINT32 flags, UINT32 val, UINT32 absTime);
extern INT32 OsFutexRequeue(const UINT32 *userVaddr, UINT32 flags, INT32 wakeNumber,
                            INT32 count, const UINT32 *newUserVaddr);
#endif
