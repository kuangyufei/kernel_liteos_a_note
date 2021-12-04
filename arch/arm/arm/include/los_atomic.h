/*!
 * @file    los_atomic.h
 * @brief 原子操作
 * @link atomic http://weharmonyos.com/openharmony/zh-cn/device-dev/kernel/kernel-small-basic-atomic.html @endlink
   @verbatim
   基本概念
	   在支持多任务的操作系统中，修改一块内存区域的数据需要“读取-修改-写入”三个步骤。
	   然而同一内存区域的数据可能同时被多个任务访问，如果在修改数据的过程中被其他任务打断，
	   就会造成该操作的执行结果无法预知。
	   使用开关中断的方法固然可以保证多任务执行结果符合预期，但这种方法显然会影响系统性能。
	   ARMv6架构引入了LDREX和STREX指令，以支持对共享存储器更缜密的非阻塞同步。由此实现的
	   原子操作能确保对同一数据的“读取-修改-写入”操作在它的执行期间不会被打断，即操作的原子性。
   
   使用场景
	   有多个任务对同一个内存数据进行加减或交换操作时，使用原子操作保证结果的可预知性。
   
   汇编指令
	   LDREX Rx, [Ry]
		   读取内存中的值，并标记对该段内存为独占访问：
		   读取寄存器Ry指向的4字节内存数据，保存到Rx寄存器中。
		   对Ry指向的内存区域添加独占访问标记。
   
	   STREX Rf, Rx, [Ry]
		   检查内存是否有独占访问标记，如果有则更新内存值并清空标记，否则不更新内存：
		   有独占访问标记
			   将寄存器Rx中的值更新到寄存器Ry指向的内存。
			   标志寄存器Rf置为0。
		   没有独占访问标记
			   不更新内存。
			   标志寄存器Rf置为1。
   
	   判断标志寄存器
		   标志寄存器为0时，退出循环，原子操作结束。
		   标志寄存器为1时，继续循环，重新进行原子操作。
   
   有多个任务对同一个内存数据进行加减或交换等操作时，使用原子操作保证结果的可预知性。

   volatile关键字在用C语言编写嵌入式软件里面用得很多，不使用volatile关键字的代码比使用volatile关键字的代码效率要高一些，
   但就无法保证数据的一致性。volatile的本意是告诉编译器，此变量的值是易变的，每次读写该变量的值时务必从该变量的内存地址中读取或写入，
   不能为了效率使用对一个“临时”变量的读写来代替对该变量的直接读写。编译器看到了volatile关键字，就一定会生成内存访问指令，
   每次读写该变量就一定会执行内存访问指令直接读写该变量。若是没有volatile关键字，编译器为了效率，
   只会在循环开始前使用读内存指令将该变量读到寄存器中，之后在循环内都是用寄存器访问指令来操作这个“临时”变量，
   在循环结束后再使用内存写指令将这个寄存器中的“临时”变量写回内存。在这个过程中，如果内存中的这个变量被别的因素
   （其他线程、中断函数、信号处理函数、DMA控制器、其他硬件设备）所改变了，就产生数据不一致的问题。另外，
   寄存器访问指令的速度要比内存访问指令的速度快，这里说的内存也包括缓存，也就是说内存访问指令实际上也有可能访问的是缓存里的数据，
   但即便如此，还是不如访问寄存器快的。缓存对于编译器也是透明的，编译器使用内存读写指令时只会认为是在读写内存，
   内存和缓存间的数据同步由CPU保证。
   
   @endverbatim
 * @attention  原子操作中，操作数及其结果不能超过函数所支持位数的最大值。目前原子操作接口只支持整型数据。
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-11-26
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
 * @defgroup los_atomic Atomic
 * @ingroup kernel
 */

#ifndef __LOS_ATOMIC_H__
#define __LOS_ATOMIC_H__

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

typedef volatile INT32 Atomic;	//原子数据包含两种类型Atomic（有符号32位数）与 Atomic64（有符号64位数）
typedef volatile INT64 Atomic64;

/**
 * @ingroup  los_atomic
 * @brief Atomic read. | 读取32bit原子数据
 *
 * @par Description:
 * This API is used to implement the atomic read and return the result value of the read.
 * @attention
 * <ul>
 * <li>The pointer v must not be NULL.</li>
 * </ul>
 *
 * @param  v         [IN] The reading pointer.
 *
 * @retval #INT32  The result value of the read.
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */	
STATIC INLINE INT32 LOS_AtomicRead(const Atomic *v)	
{
    return *(volatile INT32 *)v;    //读取内存数据
}

/**
 * @ingroup  los_atomic
 * @brief Atomic setting.
 *
 * @par Description:
 * This API is used to implement the atomic setting operation.
 * @attention
 * <ul>
 * <li>The pointer v must not be NULL.</li>
 * </ul>
 *
 * @param  v         [IN] The variable pointer to be setting.
 * @param  setVal    [IN] The value to be setting.
 *
 * @retval none.
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */	//写入内存数据
STATIC INLINE VOID LOS_AtomicSet(Atomic *v, INT32 setVal)	
{
    *(volatile INT32 *)v = setVal;
}

/**
 * @ingroup  los_atomic
 * @brief Atomic addition.
 *
 * @par Description:
 * This API is used to implement the atomic addition and return the result value of the augend.
 * @attention
 * <ul>
 * <li>The pointer v must not be NULL.</li>
 * <li>If the addtion result is not in the range of representable values for 32-bit signed integer,
 * an int integer overflow may occur to the return value</li>
 * </ul>
 *
 * @param  v         [IN] The augend pointer.
 * @param  addVal    [IN] The addend.
 *
 * @retval #INT32  The result value of the augend.
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */	//对内存数据做加法
STATIC INLINE INT32 LOS_AtomicAdd(Atomic *v, INT32 addVal)	
{
    INT32 val;
    UINT32 status;

    do {
        __asm__ __volatile__("ldrex   %1, [%2]\n"
                             "add   %1, %1, %3\n"
                             "strex   %0, %1, [%2]"
                             : "=&r"(status), "=&r"(val)
                             : "r"(v), "r"(addVal)
                             : "cc");
    } while (__builtin_expect(status != 0, 0));
	/****************************************************
	__builtin_expect是结束循环的判断语句，将最有可能执行的分支告诉编译器。
	这个指令的写法为：__builtin_expect(EXP， N)。
	意思是：EXP==N 的概率很大。
	综合理解__builtin_expect(status != 0， 0)
	说的是status = 0 的可能性很大，不成功就会重新来一遍，直到strex更新成(status == 0)为止.
	****************************************************/
    return val;
}

/**
 * @ingroup  los_atomic
 * @brief Atomic subtraction.
 *
 * @par Description:
 * This API is used to implement the atomic subtraction and return the result value of the minuend.
 * @attention
 * <ul>
 * <li>The pointer v must not be NULL.</li>
 * <li>If the subtraction result is not in the range of representable values for 32-bit signed integer,
 * an int integer overflow may occur to the return value</li>
 * </ul>
 *
 * @param  v         [IN] The minuend pointer.
 * @param  subVal    [IN] The subtrahend.
 *
 * @retval #INT32  The result value of the minuend.
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */	//对内存数据做减法
STATIC INLINE INT32 LOS_AtomicSub(Atomic *v, INT32 subVal)	
{
    INT32 val;
    UINT32 status;

    do {
        __asm__ __volatile__("ldrex   %1, [%2]\n"
                             "sub   %1, %1, %3\n"
                             "strex   %0, %1, [%2]"
                             : "=&r"(status), "=&r"(val)
                             : "r"(v), "r"(subVal)
                             : "cc");
    } while (__builtin_expect(status != 0, 0));

    return val;
}

/**
 * @ingroup  los_atomic
 * @brief Atomic addSelf.
 *
 * @par Description:
 * This API is used to implement the atomic addSelf.
 * @attention
 * <ul>
 * <li>The pointer v must not be NULL.</li>
 * <li>The value which v point to must not be INT_MAX to avoid integer overflow after adding 1.</li>
 * </ul>
 *
 * @param  v      [IN] The addSelf variable pointer.
 *
 * @retval none.
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */	//对内存数据加1
STATIC INLINE VOID LOS_AtomicInc(Atomic *v)	
{
    INT32 val;
    UINT32 status;

    do {
        __asm__ __volatile__("ldrex   %0, [%3]\n"
                             "add   %0, %0, #1\n"
                             "strex   %1, %0, [%3]"
                             : "=&r"(val), "=&r"(status), "+m"(*v)
                             : "r"(v)
                             : "cc");
    } while (__builtin_expect(status != 0, 0));
}

/**
 * @ingroup  los_atomic
 * @brief Atomic addSelf. | 对内存数据加1并返回运算结果
 *
 * @par Description:
 * This API is used to implement the atomic addSelf and return the result of addSelf.
 * @attention
 * <ul>
 * <li>The pointer v must not be NULL.</li>
 * <li>The value which v point to must not be INT_MAX to avoid integer overflow after adding 1.</li>
 * </ul>
 *
 * @param  v      [IN] The addSelf variable pointer.
 *
 * @retval #INT32 The return value of variable addSelf.
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
STATIC INLINE INT32 LOS_AtomicIncRet(Atomic *v)	
{
    INT32 val;
    UINT32 status;

    do {
        __asm__ __volatile__("ldrex   %0, [%3]\n"
                             "add   %0, %0, #1\n"
                             "strex   %1, %0, [%3]"
                             : "=&r"(val), "=&r"(status), "+m"(*v)
                             : "r"(v)
                             : "cc");
    } while (__builtin_expect(status != 0, 0));

    return val;
}

/**
 * @ingroup  los_atomic
 * @brief Atomic auto-decrement. | 对32bit原子数据做减1
 *
 * @par Description:
 * This API is used to implementating the atomic auto-decrement.
 * @attention
 * <ul>
 * <li>The pointer v must not be NULL.</li>
 * <li>The value which v point to must not be INT_MIN to avoid overflow after reducing 1.</li>
 * </ul>
 *
 * @param  v      [IN] The auto-decrement variable pointer.
 *
 * @retval none.
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
STATIC INLINE VOID LOS_AtomicDec(Atomic *v)	
{
    INT32 val;
    UINT32 status;

    do {
        __asm__ __volatile__("ldrex   %0, [%3]\n"
                             "sub   %0, %0, #1\n"
                             "strex   %1, %0, [%3]"
                             : "=&r"(val), "=&r"(status), "+m"(*v)
                             : "r"(v)
                             : "cc");
    } while (__builtin_expect(status != 0, 0));
}

/**
 * @ingroup  los_atomic
 * @brief Atomic auto-decrement. | 对内存数据减1并返回运算结果
 *
 * @par Description:
 * This API is used to implementating the atomic auto-decrement and return the result of auto-decrement.
 * @attention
 * <ul>
 * <li>The pointer v must not be NULL.</li>
 * <li>The value which v point to must not be INT_MIN to avoid overflow after reducing 1.</li>
 * </ul>
 *
 * @param  v      [IN] The auto-decrement variable pointer.
 *
 * @retval #INT32  The return value of variable auto-decrement.
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
STATIC INLINE INT32 LOS_AtomicDecRet(Atomic *v)	
{
    INT32 val;
    UINT32 status;

    do {
        __asm__ __volatile__("ldrex   %0, [%3]\n"
                             "sub   %0, %0, #1\n"
                             "strex   %1, %0, [%3]"
                             : "=&r"(val), "=&r"(status), "+m"(*v)
                             : "r"(v)
                             : "cc");
    } while (__builtin_expect(status != 0, 0));

    return val;
}

/**
 * @ingroup  los_atomic
 * @brief Atomic64 read. | 读取64bit原子数据
 *
 * @par Description:
 * This API is used to implement the atomic64 read and return the result value of the read.
 * @attention
 * <ul>
 * <li>The pointer v must not be NULL.</li>
 * </ul>
 *
 * @param  v         [IN] The reading pointer.
 *
 * @retval #INT64  The result value of the read.
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
STATIC INLINE INT64 LOS_Atomic64Read(const Atomic64 *v)	
{
    INT64 val;

    do {
        __asm__ __volatile__("ldrexd   %0, %H0, [%1]"
                             : "=&r"(val)
                             : "r"(v)
                             : "cc");
    } while (0);

    return val;
}

/**
 * @ingroup  los_atomic
 * @brief Atomic64 setting. | 写入64位内存数据
 *
 * @par Description:
 * This API is used to implement the atomic64 setting operation.
 * @attention
 * <ul>
 * <li>The pointer v must not be NULL.</li>
 * </ul>
 *
 * @param  v         [IN] The variable pointer to be setting.
 * @param  setVal    [IN] The value to be setting.
 *
 * @retval none.
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
STATIC INLINE VOID LOS_Atomic64Set(Atomic64 *v, INT64 setVal)	
{
    INT64 tmp;
    UINT32 status;

    do {
        __asm__ __volatile__("ldrexd   %1, %H1, [%2]\n"
                             "strexd   %0, %3, %H3, [%2]"
                             : "=&r"(status), "=&r"(tmp)
                             : "r"(v), "r"(setVal)
                             : "cc");
    } while (__builtin_expect(status != 0, 0));
}

/**
 * @ingroup  los_atomic
 * @brief Atomic64 addition. | 对64位内存数据做加法
 *
 * @par Description:
 * This API is used to implement the atomic64 addition and return the result value of the augend.
 * @attention
 * <ul>
 * <li>The pointer v must not be NULL.</li>
 * <li>If the addtion result is not in the range of representable values for 64-bit signed integer,
 * an int integer overflow may occur to the return value</li>
 * </ul>
 *
 * @param  v         [IN] The augend pointer.
 * @param  addVal    [IN] The addend.
 *
 * @retval #INT64  The result value of the augend.
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
STATIC INLINE INT64 LOS_Atomic64Add(Atomic64 *v, INT64 addVal)	
{
    INT64 val;
    UINT32 status;

    do {
        __asm__ __volatile__("ldrexd   %1, %H1, [%2]\n"
                             "adds   %Q1, %Q1, %Q3\n"
                             "adc   %R1, %R1, %R3\n"
                             "strexd   %0, %1, %H1, [%2]"
                             : "=&r"(status), "=&r"(val)
                             : "r"(v), "r"(addVal)
                             : "cc");
    } while (__builtin_expect(status != 0, 0));

    return val;
}

/**
 * @ingroup  los_atomic
 * @brief Atomic64 subtraction. | 对64位原子数据做减法
 *
 * @par Description:
 * This API is used to implement the atomic64 subtraction and return the result value of the minuend.
 * @attention
 * <ul>
 * <li>The pointer v must not be NULL.</li>
 * <li>If the subtraction result is not in the range of representable values for 64-bit signed integer,
 * an int integer overflow may occur to the return value</li>
 * </ul>
 *
 * @param  v         [IN] The minuend pointer.
 * @param  subVal    [IN] The subtrahend.
 *
 * @retval #INT64  The result value of the minuend.
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */	
STATIC INLINE INT64 LOS_Atomic64Sub(Atomic64 *v, INT64 subVal) 
{
    INT64 val;
    UINT32 status;

    do {
        __asm__ __volatile__("ldrexd   %1, %H1, [%2]\n"
                             "subs   %Q1, %Q1, %Q3\n"
                             "sbc   %R1, %R1, %R3\n"
                             "strexd   %0, %1, %H1, [%2]"
                             : "=&r"(status), "=&r"(val)
                             : "r"(v), "r"(subVal)
                             : "cc");
    } while (__builtin_expect(status != 0, 0));

    return val;
}

/**
 * @ingroup  los_atomic
 * @brief Atomic64 addSelf. | 对64位原子数据加1
 *
 * @par Description:
 * This API is used to implement the atomic64 addSelf .
 * @attention
 * <ul>
 * <li>The pointer v must not be NULL.</li>
 * <li>The value which v point to must not be INT64_MAX to avoid integer overflow after adding 1.</li>
 * </ul>
 *
 * @param  v      [IN] The addSelf variable pointer.
 *
 * @retval none.
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
STATIC INLINE VOID LOS_Atomic64Inc(Atomic64 *v) 
{
    INT64 val;
    UINT32 status;

    do {
        __asm__ __volatile__("ldrexd   %0, %H0, [%3]\n"
                             "adds   %Q0, %Q0, #1\n"
                             "adc   %R0, %R0, #0\n"
                             "strexd   %1, %0, %H0, [%3]"
                             : "=&r"(val), "=&r"(status), "+m"(*v)
                             : "r"(v)
                             : "cc");
    } while (__builtin_expect(status != 0, 0));
}

/**
 * @ingroup  los_atomic
 * @brief Atomic64 addSelf. | 对64位原子数据加1并返回运算结果
 *
 * @par Description:
 * This API is used to implement the atomic64 addSelf and return the result of addSelf.
 * @attention
 * <ul>
 * <li>The pointer v must not be NULL.</li>
 * <li>The value which v point to must not be INT64_MAX to avoid integer overflow after adding 1.</li>
 * </ul>
 *
 * @param  v      [IN] The addSelf variable pointer.
 *
 * @retval #INT64 The return value of variable addSelf.
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
STATIC INLINE INT64 LOS_Atomic64IncRet(Atomic64 *v) 
{
    INT64 val;
    UINT32 status;

    do {
        __asm__ __volatile__("ldrexd   %0, %H0, [%3]\n"
                             "adds   %Q0, %Q0, #1\n"
                             "adc   %R0, %R0, #0\n"
                             "strexd   %1, %0, %H0, [%3]"
                             : "=&r"(val), "=&r"(status), "+m"(*v)
                             : "r"(v)
                             : "cc");
    } while (__builtin_expect(status != 0, 0));

    return val;
}

/**
 * @ingroup  los_atomic
 * @brief Atomic64 auto-decrement. | 对64位原子数据减1
 *
 * @par Description:
 * This API is used to implementating the atomic64 auto-decrement.
 * @attention
 * <ul>
 * <li>The pointer v must not be NULL.</li>
 * <li>The value which v point to must not be INT64_MIN to avoid overflow after reducing 1.</li>
 * </ul>
 *
 * @param  v      [IN] The auto-decrement variable pointer.
 *
 * @retval none.
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
STATIC INLINE VOID LOS_Atomic64Dec(Atomic64 *v) 
{
    INT64 val;
    UINT32 status;

    do {
        __asm__ __volatile__("ldrexd   %0, %H0, [%3]\n"
                             "subs   %Q0, %Q0, #1\n"
                             "sbc   %R0, %R0, #0\n"
                             "strexd   %1, %0, %H0, [%3]"
                             : "=&r"(val), "=&r"(status), "+m"(*v)
                             : "r"(v)
                             : "cc");
    } while (__builtin_expect(status != 0, 0));
}

/**
 * @ingroup  los_atomic
 * @brief Atomic64 auto-decrement. | 对64位原子数据减1并返回运算结果
 *
 * @par Description:
 * This API is used to implementating the atomic64 auto-decrement and return the result of auto-decrement.
 * @attention
 * <ul>
 * <li>The pointer v must not be NULL.</li>
 * <li>The value which v point to must not be INT64_MIN to avoid overflow after reducing 1.</li>
 * </ul>
 *
 * @param  v      [IN] The auto-decrement variable pointer.
 *
 * @retval #INT64  The return value of variable auto-decrement.
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
STATIC INLINE INT64 LOS_Atomic64DecRet(Atomic64 *v) 
{
    INT64 val;
    UINT32 status;

    do {
        __asm__ __volatile__("ldrexd   %0, %H0, [%3]\n"
                             "subs   %Q0, %Q0, #1\n"
                             "sbc   %R0, %R0, #0\n"
                             "strexd   %1, %0, %H0, [%3]"
                             : "=&r"(val), "=&r"(status), "+m"(*v)
                             : "r"(v)
                             : "cc");
    } while (__builtin_expect(status != 0, 0));

    return val;
}

/**
 * @ingroup  los_atomic
 * @brief Atomic exchange for 8-bit variable. | 交换8位原子数据，原内存中的值以返回值的方式返回
 *
 * @par Description:
 * This API is used to implement the atomic exchange for 8-bit variable and
 * return the previous value of the atomic variable.
 * @attention
 * <ul>The pointer v must not be NULL.</ul>
 *
 * @param  v         [IN] The variable pointer.
 * @param  val       [IN] The exchange value.
 *
 * @retval #INT32       The previous value of the atomic variable
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
STATIC INLINE INT32 LOS_AtomicXchgByte(volatile INT8 *v, INT32 val)	
{
    INT32 prevVal;
    UINT32 status;

    do {
        __asm__ __volatile__("ldrexb   %0, [%3]\n"
                             "strexb   %1, %4, [%3]"
                             : "=&r"(prevVal), "=&r"(status), "+m"(*v)
                             : "r"(v), "r"(val)
                             : "cc");
    } while (__builtin_expect(status != 0, 0));

    return prevVal;
}

/**
 * @ingroup  los_atomic
 * @brief Atomic exchange for 16-bit variable. | 交换16位原子数据，原内存中的值以返回值的方式返回
 *
 * @par Description:
 * This API is used to implement the atomic exchange for 16-bit variable and
 * return the previous value of the atomic variable.
 * @attention
 * <ul>The pointer v must not be NULL.</ul>
 *
 * @param  v         [IN] The variable pointer.
 * @param  val       [IN] The exchange value.
 *
 * @retval #INT32       The previous value of the atomic variable
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
STATIC INLINE INT32 LOS_AtomicXchg16bits(volatile INT16 *v, INT32 val)	
{
    INT32 prevVal;
    UINT32 status;

    do {
        __asm__ __volatile__("ldrexh   %0, [%3]\n"
                             "strexh   %1, %4, [%3]"
                             : "=&r"(prevVal), "=&r"(status), "+m"(*v)
                             : "r"(v), "r"(val)
                             : "cc");
    } while (__builtin_expect(status != 0, 0));

    return prevVal;
}

/**
 * @ingroup  los_atomic
 * @brief Atomic exchange for 32-bit variable. | 交换32位原子数据，原内存中的值以返回值的方式返回
 *
 * @par Description:
 * This API is used to implement the atomic exchange for 32-bit variable
 * and return the previous value of the atomic variable.
 * @attention
 * <ul>The pointer v must not be NULL.</ul>
 *
 * @param  v         [IN] The variable pointer.
 * @param  val       [IN] The exchange value.
 *
 * @retval #INT32       The previous value of the atomic variable
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
STATIC INLINE INT32 LOS_AtomicXchg32bits(Atomic *v, INT32 val)	
{
    INT32 prevVal;
    UINT32 status;

    do {
        __asm__ __volatile__("ldrex   %0, [%3]\n"
                             "strex   %1, %4, [%3]"
                             : "=&r"(prevVal), "=&r"(status), "+m"(*v)
                             : "r"(v), "r"(val)
                             : "cc");
    } while (__builtin_expect(status != 0, 0));

    return prevVal;
}

/**
 * @ingroup  los_atomic
 * @brief Atomic exchange for 64-bit variable. | 交换64位原子数据，原内存中的值以返回值的方式返回
 *
 * @par Description:
 * This API is used to implement the atomic exchange for 64-bit variable
 * and return the previous value of the atomic variable.
 * @attention
 * <ul>The pointer v must not be NULL.</ul>
 *
 * @param  v         [IN] The variable pointer.
 * @param  val       [IN] The exchange value.
 *
 * @retval #INT64       The previous value of the atomic variable
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
STATIC INLINE INT64 LOS_AtomicXchg64bits(Atomic64 *v, INT64 val)	
{
    INT64 prevVal;
    UINT32 status;

    do {
        __asm__ __volatile__("ldrexd   %0, %H0, [%3]\n"
                             "strexd   %1, %4, %H4, [%3]"
                             : "=&r"(prevVal), "=&r"(status), "+m"(*v)
                             : "r"(v), "r"(val)
                             : "cc");
    } while (__builtin_expect(status != 0, 0));

    return prevVal;
}

/**
 * @ingroup  los_atomic
 * @brief Atomic exchange for 8-bit variable with compare. | 比较并交换8位原子数据，返回比较结果
 *
 * @par Description:
 * This API is used to implement the atomic exchange for 8-bit variable, if the value of variable is equal to oldVal.
 * @attention
 * <ul>The pointer v must not be NULL.</ul>
 *
 * @param  v           [IN] The variable pointer.
 * @param  val         [IN] The new value.
 * @param  oldVal      [IN] The old value.
 *
 * @retval TRUE  The previous value of the atomic variable is not equal to oldVal.
 * @retval FALSE The previous value of the atomic variable is equal to oldVal.
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
STATIC INLINE BOOL LOS_AtomicCmpXchgByte(volatile INT8 *v, INT32 val, INT32 oldVal)	
{
    INT32 prevVal;
    UINT32 status;

    do {
        __asm__ __volatile__("ldrexb %0, [%3]\n"
                             "mov %1, #0\n"
                             "teq %0, %4\n"
                             "strexbeq %1, %5, [%3]"
                             : "=&r"(prevVal), "=&r"(status), "+m"(*v)
                             : "r"(v), "r"(oldVal), "r"(val)
                             : "cc");
    } while (__builtin_expect(status != 0, 0));

    return prevVal != oldVal;
}

/**
 * @ingroup  los_atomic
 * @brief Atomic exchange for 16-bit variable with compare. | 比较并交换16位原子数据，返回比较结果
 *
 * @par Description:
 * This API is used to implement the atomic exchange for 16-bit variable, if the value of variable is equal to oldVal.
 * @attention
 * <ul>The pointer v must not be NULL.</ul>
 *
 * @param  v           [IN] The variable pointer.
 * @param  val         [IN] The new value.
 * @param  oldVal      [IN] The old value.
 *
 * @retval TRUE  The previous value of the atomic variable is not equal to oldVal.
 * @retval FALSE The previous value of the atomic variable is equal to oldVal.
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
STATIC INLINE BOOL LOS_AtomicCmpXchg16bits(volatile INT16 *v, INT32 val, INT32 oldVal)	
{
    INT32 prevVal;
    UINT32 status;

    do {
        __asm__ __volatile__("ldrexh %0, [%3]\n"
                             "mov %1, #0\n"
                             "teq %0, %4\n"
                             "strexheq %1, %5, [%3]"
                             : "=&r"(prevVal), "=&r"(status), "+m"(*v)
                             : "r"(v), "r"(oldVal), "r"(val)
                             : "cc");
    } while (__builtin_expect(status != 0, 0));

    return prevVal != oldVal;
}

/**
 * @ingroup  los_atomic
 * @brief Atomic exchange for 32-bit variable with compare. | 比较并交换32位原子数据，返回比较结果
 *
 * @par Description:
 * This API is used to implement the atomic exchange for 32-bit variable, if the value of variable is equal to oldVal.
 * @attention
 * <ul>The pointer v must not be NULL.</ul>
 *
 * @param  v           [IN] The variable pointer.
 * @param  val         [IN] The new value.
 * @param  oldVal      [IN] The old value.
 *
 * @retval TRUE  The previous value of the atomic variable is not equal to oldVal.
 * @retval FALSE The previous value of the atomic variable is equal to oldVal.
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
STATIC INLINE BOOL LOS_AtomicCmpXchg32bits(Atomic *v, INT32 val, INT32 oldVal)	
{
    INT32 prevVal;
    UINT32 status;

    do {
        __asm__ __volatile__("ldrex %0, [%3]\n"
                             "mov %1, #0\n"
                             "teq %0, %4\n"
                             "strexeq %1, %5, [%3]"
                             : "=&r"(prevVal), "=&r"(status), "+m"(*v)
                             : "r"(v), "r"(oldVal), "r"(val)
                             : "cc");
    } while (__builtin_expect(status != 0, 0));

    return prevVal != oldVal;
}

/**
 * @ingroup  los_atomic
 * @brief Atomic exchange for 64-bit variable with compare. | 比较并交换64位原子数据，返回比较结果
 *
 * @par Description:
 * This API is used to implement the atomic exchange for 64-bit variable, if the value of variable is equal to oldVal.
 * @attention
 * <ul>The pointer v must not be NULL.</ul>
 *
 * @param  v           [IN] The variable pointer.
 * @param  val         [IN] The new value.
 * @param  oldVal      [IN] The old value.
 *
 * @retval TRUE  The previous value of the atomic variable is not equal to oldVal.
 * @retval FALSE The previous value of the atomic variable is equal to oldVal.
 * @par Dependency:
 * <ul><li>los_atomic.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
STATIC INLINE BOOL LOS_AtomicCmpXchg64bits(Atomic64 *v, INT64 val, INT64 oldVal) 
{
    INT64 prevVal;
    UINT32 status;

    do {
        __asm__ __volatile__("ldrexd   %0, %H0, [%3]\n"
                             "mov   %1, #0\n"
                             "teq   %0, %4\n"
                             "teqeq   %H0, %H4\n"
                             "strexdeq   %1, %5, %H5, [%3]"
                             : "=&r"(prevVal), "=&r"(status), "+m"(*v)
                             : "r"(v), "r"(oldVal), "r"(val)
                             : "cc");
    } while (__builtin_expect(status != 0, 0));

    return prevVal != oldVal;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* __LOS_ATOMIC_H__ */
