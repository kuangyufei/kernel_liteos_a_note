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

#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include "los_printf.h"
#include "los_exc.h"
/**
 * @brief   获取环境变量的值
 * @param   name 环境变量名
 * @return  成功返回环境变量值的指针，当前未实现返回NULL
 */
char *getenv(const char *name)
{
    return NULL;  // 当前未实现环境变量功能，直接返回NULL
}

/**
 * @brief   设置随机数生成器的种子
 * @param   s 随机数种子值
 */
void srand(unsigned s)
{
    return srandom(s);  // 调用srandom函数设置随机数种子
}

/**
 * @brief   生成一个随机整数
 * @return  返回生成的随机整数
 */
int rand(void)
{
    return random();  // 调用random函数获取随机数
}

/**
 * @brief   终止当前进程（不执行清理操作）
 * @param   status 进程退出状态
 * @note    当前未支持，会打印错误信息并进入死循环
 */
void _exit(int status)
{
    PRINT_ERR("%s NOT SUPPORT\n", __FUNCTION__);  // 打印不支持的错误信息
    errno = ENOSYS;                               // 设置错误码为系统调用未实现
    while (1);                                    // 进入死循环，防止程序继续执行
}

/**
 * @brief   终止当前进程（执行清理操作）
 * @param   status 进程退出状态
 * @note    当前未支持，会打印错误信息并进入死循环
 */
void exit(int status)
{
    PRINT_ERR("%s NOT SUPPORT\n", __FUNCTION__);  // 打印不支持的错误信息
    errno = ENOSYS;                               // 设置错误码为系统调用未实现
    while (1);                                    // 进入死循环，防止程序继续执行
}

/**
 * @brief   异常终止当前进程
 * @note    调用LOS_Panic打印中止信息并进入死循环
 */
void abort(void)
{
    LOS_Panic("System was being aborted\n");  // 打印系统中止信息
    while (1);                                // 进入死循环，防止程序继续执行
}