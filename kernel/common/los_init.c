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

#include "los_init_pri.h"
#include "los_atomic.h"
#include "los_config.h"
#include "los_hw.h"
#include "los_printf.h"
#include "los_spinlock.h"
#include "los_typedef.h"

#ifdef LOS_INIT_DEBUG
#include "los_sys_pri.h"
#include "los_tick.h"
#endif
/**
 * @file los_init.c
 * @brief 内核初始化框架实现，负责按级别调度模块初始化流程
 */

/**
 * @def OS_INIT_LEVEL_REG(kernel, 10, g_kernInitLevelList)
 * @brief 注册内核初始化级别标签
 * @param[in] kernel 初始化框架名称
 * @param[in] 10 初始化级别总数
 * @param[in] g_kernInitLevelList 初始化级别列表指针
 * @note 该宏定义用于声明内核初始化级别表，系统将按级别顺序执行初始化钩子
 */
OS_INIT_LEVEL_REG(kernel, 10, g_kernInitLevelList);

/**
 * @var g_initCurrentLevel
 * @brief 当前初始化级别全局变量
 * @details 记录系统正在执行的初始化级别，用于多核同步
 * @note 类型为volatile UINT32，确保多核心间可见性，初始值为OS_INVALID_VALUE
 */
STATIC volatile UINT32 g_initCurrentLevel = OS_INVALID_VALUE;

/**
 * @var g_initCurrentModule
 * @brief 当前初始化模块指针
 * @details 指向当前正在初始化的模块信息结构体，用于遍历级别内所有模块
 * @note 类型为volatile struct ModuleInitInfo*，确保多核心间操作的原子性
 */
STATIC volatile struct ModuleInitInfo *g_initCurrentModule = 0;

/**
 * @var g_initCount
 * @brief 初始化计数器
 * @details 用于多核环境下同步初始化进度，记录已完成当前级别初始化的核心数量
 * @note 类型为Atomic，确保原子操作
 */
STATIC Atomic g_initCount = 0;

/**
 * @var g_initLock
 * @brief 初始化自旋锁
 * @details 保护初始化过程中的共享资源访问，防止多核心竞争
 * @note 使用SPIN_LOCK_INIT宏静态初始化
 */
STATIC SPIN_LOCK_INIT(g_initLock);

/**
 * @brief 初始化级别调用函数
 * @details 按指定级别执行对应模块的初始化钩子，支持多核同步和调试信息输出
 * @param[in] name 初始化框架名称（如"Kernel"）
 * @param[in] level 当前初始化级别
 * @param[in] initLevelList 初始化级别列表，每个级别包含多个模块初始化信息
 * @note 建议每个启动框架封装自己的调用接口
 */
STATIC VOID InitLevelCall(const CHAR *name, const UINT32 level, struct ModuleInitInfo *initLevelList[])
{
    struct ModuleInitInfo *module = NULL;  // 当前正在处理的模块信息结构体指针
#ifdef LOS_INIT_DEBUG
    UINT64 startNsec, endNsec;             // 调试用：模块初始化开始/结束时间（纳秒）
    UINT64 totalTime = 0;                  // 调试用：当前级别总初始化时间
    UINT64 singleTime;                     // 调试用：单个模块初始化时间
    UINT32 ret = LOS_OK;                   // 调试用：模块初始化返回值
#endif

    // 主核心（CPU0）负责初始化级别控制，从核心等待主核心设置当前级别
    if (ArchCurrCpuid() == 0) {
#ifdef LOS_INIT_DEBUG
        // 调试模式下打印级别初始化开始信息
        PRINTK("-------- %s Module Init... level = %u --------\n", name, level);
#endif
        g_initCurrentLevel = level;        // 设置当前初始化级别
        g_initCurrentModule = initLevelList[level];  // 指向当前级别第一个模块
    } else {
        // 从核心等待主核心完成当前级别设置
        while (g_initCurrentLevel < level) {
        }
    }

    // 循环处理当前级别所有模块
    do {
        LOS_SpinLock(&g_initLock);          // 获取自旋锁，保护共享变量访问
        // 检查是否已处理完当前级别所有模块
        if (g_initCurrentModule >= initLevelList[level + 1]) {
            LOS_SpinUnlock(&g_initLock);    // 释放自旋锁
            break;                          // 退出循环
        }
        module = (struct ModuleInitInfo *)g_initCurrentModule;  // 获取当前模块
        g_initCurrentModule++;              // 指向下一个模块
        LOS_SpinUnlock(&g_initLock);        // 释放自旋锁

        // 如果模块存在初始化钩子，则执行
        if (module->hook != NULL) {
#ifdef LOS_INIT_DEBUG
            startNsec = LOS_CurrNanosec();   // 记录模块初始化开始时间
            ret = (UINT32)module->hook();    // 执行模块初始化钩子
            endNsec = LOS_CurrNanosec();     // 记录模块初始化结束时间
            singleTime = endNsec - startNsec;  // 计算单个模块初始化耗时
            totalTime += singleTime;         // 累加至级别总耗时
            // 打印模块初始化信息
            PRINTK("Starting %s module consumes %llu ns. Run on cpu %u\n", module->name, singleTime, ArchCurrCpuid());
#else
            module->hook();                  // 非调试模式直接执行钩子
#endif
        }

#ifdef LOS_INIT_DEBUG
        // 调试模式下检查模块初始化结果
        if (ret != LOS_OK) {
            PRINT_ERR("%s initialization failed at module %s, function addr at 0x%x, ret code is %u\n",
                      name, module->name, module->hook, ret);
        }
#endif
    } while (1);  // 无限循环，通过内部break退出

    // 当初始化级别大于等于内核模块任务级别时，进行多核同步
    if (level >= LOS_INIT_LEVEL_KMOD_TASK) {
        LOS_AtomicInc(&g_initCount);        // 原子递增初始化计数器
        // 等待所有核心完成当前级别初始化（计数器为核心数的整数倍）
        while ((LOS_AtomicRead(&g_initCount) % LOSCFG_KERNEL_CORE_NUM) != 0) {
        }
    }

#ifdef LOS_INIT_DEBUG
    // 调试模式下打印级别初始化完成信息
    PRINTK("%s initialization at level %u consumes %lluns on cpu %u.\n", name, level, totalTime, ArchCurrCpuid());
#endif
}

/**
 * @brief 内核初始化调用函数
 * @details 内核初始化入口，根据指定级别调用对应初始化流程
 * @param[in] level 要执行的初始化级别
 * @note 当级别大于等于LOS_INIT_LEVEL_FINISH时，初始化已完成，直接返回
 */
VOID OsInitCall(const UINT32 level)
{
    if (level >= LOS_INIT_LEVEL_FINISH) {
        return;                             // 初始化已完成，无需处理
    }

    InitLevelCall("Kernel", level, g_kernInitLevelList);  // 调用内核级别初始化
}