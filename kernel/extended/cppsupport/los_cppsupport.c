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
 * @brief @file los_cppsupport.c
 * @verbatim 
	基本概念
		C++作为目前使用最广泛的编程语言之一，支持类、封装、重载等特性，是在C语言基础上
		开发的一种面向对象的编程语言。
	运作机制
		STL（Standard Template Library）标准模板库，是一些“容器”的集合，也是算法和其他
		一些组件的集合。其目的是标准化组件，使用标准化组件后可以不用重新开发，直接使用现成的组件。
	开发流程
		通过make menuconfig使能C++支持。
		使用C++特性之前，需要调用函数LOS_CppSystemInit，初始化C++构造函数。
		C函数与C++函数混合调用。在C++中调用C程序的函数，代码需加入C++包含的宏：
		#ifdef __cplusplus
		#if __cplusplus
		extern "C" {
		#endif /* __cplusplus * /
		#endif /* __cplusplus * /
		/* code * /
		...
		#ifdef __cplusplus
		#if __cplusplus
		}
		#endif /* __cplusplus * /
		#endif /* __cplusplus * /
 * @endverbatim
 */

#include "los_cppsupport.h"
#include "los_printf.h"


/**
 * @brief 初始化函数指针类型定义
 * @note 无参数、无返回值的初始化函数原型
 */
typedef VOID (*InitFunc)(VOID);

/**
 * @brief C++系统初始化函数，执行初始化函数数组
 * @param initArrayStart 初始化函数数组起始地址
 * @param initArrayEnd 初始化函数数组结束地址
 * @param flag 初始化标志（预留参数）
 * @return 总是返回0，表示成功
 */
LITE_OS_SEC_TEXT_MINOR INT32 LOS_CppSystemInit(UINTPTR initArrayStart, UINTPTR initArrayEnd, INT32 flag)
{
    UINTPTR *start     = (UINTPTR *)initArrayStart;  /* 初始化函数数组起始指针 */
    InitFunc initFunc   = NULL;                      /* 当前要执行的初始化函数 */

    for (; start != (UINTPTR *)initArrayEnd; ++start) {  // 遍历整个初始化函数数组
        initFunc = (InitFunc)(*start);                   // 将数组元素转换为初始化函数指针
        initFunc();                                      // 调用初始化函数
    }

    return 0;  /* 返回成功 */
}

