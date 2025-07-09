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
 * @file    los_hwi.c 
 * @brief 硬中断主文件
 * @link
 * @verbatim
     基本概念 
        中断是指出现需要时，CPU暂停执行当前程序，转而执行新程序的过程。即在程序运行过程中，
        出现了一个必须由CPU立即处理的事务。此时，CPU暂时中止当前程序的执行转而处理这个事务，
        这个过程就叫做中断。通过中断机制，可以使CPU避免把大量时间耗费在等待、查询外设状态的操作上，
        大大提高系统实时性以及执行效率。

        异常处理是操作系统对运行期间发生的异常情况（芯片硬件异常）进行处理的一系列动作，
        例如虚拟内存缺页异常、打印异常发生时函数的调用栈信息、CPU现场信息、任务的堆栈情况等。
        
        外设可以在没有CPU介入的情况下完成一定的工作，但某些情况下也需要CPU为其执行一定的工作。
        通过中断机制，在外设不需要CPU介入时，CPU可以执行其它任务，而当外设需要CPU时，产生一个中断信号，
        该信号连接至中断控制器。中断控制器是一方面接收其它外设中断引脚的输入，另一方面它会发出中断信号给CPU。
        可以通过对中断控制器编程来打开和关闭中断源、设置中断源的优先级和触发方式。
        常用的中断控制器有VIC（Vector Interrupt Controller）和GIC（General Interrupt Controller）。
        在ARM Cortex-A7中使用的中断控制器是GIC。CPU收到中断控制器发送的中断信号后，中断当前任务来响应中断请求。

        异常处理就是可以打断CPU正常运行流程的一些事情，如未定义指令异常、试图修改只读的数据异常、不对齐的地址访问异常等。
        当异常发生时，CPU暂停当前的程序，先处理异常事件，然后再继续执行被异常打断的程序。

        中断特性： 
            中断共享，且可配置。 
            中断嵌套，即高优先级的中断可抢占低优先级的中断，且可配置。 
            使用独立中断栈，可配置。 
            可配置支持的中断优先级个数。 
            可配置支持的中断数。 

    中断相关的硬件介绍 
        与中断相关的硬件可以划分为三类：设备、中断控制器、CPU本身。 
        设备 
            发起中断的源，当设备需要请求CPU时，产生一个中断信号，该信号连接至中断控制器。 
        中断控制器 
            中断控制器是CPU众多外设中的一个，它一方面接收其它外设中断引脚的输入，另一方面， 
            它会发出中断信号给CPU。可以通过对中断控制器编程来打开和关闭中断源、设置中断源 
            的优先级和触发方式。常用的中断控制器有VIC（Vector Interrupt Controller）和 
            GIC（General Interrupt Controller）。在ARM Cortex-M系列中使用的中断控制器是 
            NVIC（Nested Vector Interrupt Controller）。在ARM Cortex-A7中使用的中断控制器是GIC。 
        CPU 
            CPU会响应中断源的请求，中断当前正在执行的任务，转而执行中断处理程序。 
    中断相关概念 
        中断号 
            每个中断请求信号都会有特定的标志，使得计算机能够判断是哪个设备提出的中断请求，这个标志就是中断号。 
        中断请求 
            “紧急事件”需向CPU提出申请（发一个电脉冲信号），要求中断，及要求CPU暂停当前执行的任务， 
            转而处理该“紧急事件”，这一申请过程称为中断请求。 
        中断优先级 
            为使系统能够及时响应并处理所有中断，系统根据中断时间的重要性和紧迫程度，将中断源分为若干个级别， 
            称作中断优先级。 
        中断处理程序 
            当外设产生中断请求后，CPU暂停当前的任务，转而响应中断申请，即执行中断处理程序。产生中断的每个设备 
            都有相应的中断处理程序。 
        中断嵌套 
            中断嵌套也称为中断抢占，指的是正在执行一个中断处理程序时，如果有另一个优先级更高的中断源提出中断请求， 
            这时会暂时终止当前正在执行的优先级较低的中断源的中断处理程序，转而去处理更高优先级的中断请求，待处理完毕， 
            再返回到之前被中断的处理程序中继续执行。 
        中断触发 
            中断源向中断控制器发送中断信号，中断控制器对中断进行仲裁，确定优先级，将中断信号送给CPU。 
            中断源产生中断信号的时候，会将中断触发器置“1”，表明该中断源产生了中断，要求CPU去响应该中断。 
        中断触发类型 
            外部中断申请通过一个物理信号发送到NVIC/GIC，可以是电平触发或边沿触发。 
        中断向量 
            中断服务程序的入口地址。 
        中断向量表 
            存储中断向量的存储区，中断向量与中断号对应，中断向量在中断向量表中按照中断号顺序存储。 
        中断共享 
            当外设较少时，可以实现一个外设对应一个中断号，但为了支持更多的硬件设备，可以让多个设备共享 
            一个中断号，共享同一个中断号的中断处理程序形成一个链表。当外部设备产生中断申请时，系统会 
            遍历执行中断号对应的中断处理程序链表直到找到对应设备的中断处理程序。在遍历执行过程中， 
            各中断处理程序可以通过检测设备ID，判断是否是这个中断处理程序对应的设备产生的中断。 
        核间中断 
            对于多核系统，中断控制器允许一个CPU的硬件线程去中断其他CPU的硬件线程，这种方式被称为核间中断。 
            核间中断的实现基础是多CPU内存共享，采用核间中断可以减少某个CPU负荷过大，有效提升系统效率。 
            目前只有GIC中断控制器支持。 
    使用场景 
        当有中断请求产生时，CPU暂停当前的任务，转而去响应外设请求。根据需要，用户通过 
        中断申请，注册中断处理程序，可以指定CPU响应中断请求时所执行的具体操作。 
    开发流程 
        调用中断创建接口LOS_HwiCreate创建中断。  
        如果是SMP模式，调用LOS_HwiSetAffinity设置中断的亲和性，否则直接进入步骤4。 
        调用LOS_HwiEnable接口使能指定中断。 
        调用LOS_HwiTrigger接口触发指定中断（该接口通过写中断控制器的相关寄存器模拟外部中断，一般的外设设备，不需要执行这一步）。 
        调用LOS_HwiDisable接口屏蔽指定中断，此接口根据实际情况使用，判断是否需要屏蔽中断。 
        调用LOS_HwiDelete接口删除指定中断，此接口根据实际情况使用，判断是否需要删除中断。 
    注意事项 
        根据具体硬件，配置支持的最大中断数及可设置的中断优先级个数。 
        中断共享机制，支持不同的设备使用相同的中断号注册同一中断处理程序，但中断处理程序的入参pDevId（设备号） 
        必须唯一，代表不同的设备。即同一中断号，同一dev只能挂载一次；但同一中断号，同一中断处理程序，dev不同则可以重复挂载。 
        中断处理程序耗时不能过长，否则会影响CPU对中断的及时响应。 
        中断响应过程中不能执行引起调度的函数。 
        中断恢复LOS_IntRestore()的入参必须是与之对应的LOS_IntLock()的返回值（即关中断之前的CPSR值）。 
        Cortex-M系列处理器中0-15中断为内部使用，Cortex-A7中0-31中断为内部使用，因此不建议用户去申请和创建。
        
        以ARMv7-a架构为例，中断和异常处理的入口为中断向量表，中断向量表包含各个中断和异常处理的入口函数。
 * @endverbatim
 * @image html https://gitee.com/weharmonyos/resources/raw/master/44/vector.png
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-11-16
 *
 * @history
 *
 */
#include "los_hwi.h"
#include "los_memory.h"
#include "los_spinlock.h"
#ifdef LOSCFG_KERNEL_CPUP
#include "los_cpup_pri.h"
#endif
#include "los_sched_pri.h"
#include "los_hook.h"

/* spinlock for hwi module, only available on SMP mode */
LITE_OS_SEC_BSS  SPIN_LOCK_INIT(g_hwiSpin);  // 硬件中断模块自旋锁，仅SMP模式可用
#define HWI_LOCK(state)       LOS_SpinLockSave(&g_hwiSpin, &(state))  // 获取硬件中断自旋锁并保存中断状态
#define HWI_UNLOCK(state)     LOS_SpinUnlockRestore(&g_hwiSpin, (state))  // 恢复中断状态并释放硬件中断自旋锁

size_t g_intCount[LOSCFG_KERNEL_CORE_NUM] = {0};  // 每个CPU核心的中断计数器，初始化为0 (LOSCFG_KERNEL_CORE_NUM为CPU核心数)
HwiHandleForm g_hwiForm[OS_HWI_MAX_NUM];  // 硬件中断句柄数组，大小为最大中断数 (OS_HWI_MAX_NUM通常为256)
STATIC CHAR *g_hwiFormName[OS_HWI_MAX_NUM] = {0};  // 中断名称数组，与中断号对应
STATIC UINT32 g_hwiFormCnt[LOSCFG_KERNEL_CORE_NUM][OS_HWI_MAX_NUM] = {0};  // 每个CPU核心的中断触发计数矩阵

/**
 * @brief 获取指定CPU核心的中断触发次数
 * @param cpuid CPU核心ID
 * @param index 中断号
 * @return 中断触发次数
 */
UINT32 OsGetHwiFormCnt(UINT16 cpuid, UINT32 index)
{
    return g_hwiFormCnt[cpuid][index];
}

/**
 * @brief 获取指定中断号的名称
 * @param index 中断号
 * @return 中断名称字符串
 */
CHAR *OsGetHwiFormName(UINT32 index)
{
    return g_hwiFormName[index];
}

/**
 * @brief 获取系统支持的最大硬件中断数
 * @return 最大中断数 OS_HWI_MAX_NUM
 */
UINT32 LOS_GetSystemHwiMaximum(VOID)
{
    return OS_HWI_MAX_NUM;  // 返回系统定义的最大中断数，十进制值通常为256
}

typedef VOID (*HWI_PROC_FUNC0)(VOID);  // 无参数的中断处理函数指针类型
typedef VOID (*HWI_PROC_FUNC2)(INT32, VOID *);  // 带两个参数的中断处理函数指针类型

/**
 * @brief 中断处理入口函数
 * @param intNum 中断号
 * @details 负责中断分发、计数更新和钩子函数调用
 *          必须保持接口开头和结尾的中断计数操作
 */
VOID OsInterrupt(UINT32 intNum)
{
    HwiHandleForm *hwiForm = NULL;  // 中断句柄指针
    UINT32 *intCnt = NULL;  // 中断计数器指针
    UINT16 cpuid = ArchCurrCpuid();  // 获取当前CPU核心ID

    /* Must keep the operation at the beginning of the interface */
    intCnt = &g_intCount[cpuid];
    *intCnt = *intCnt + 1;  // 递增当前CPU的中断计数器

#ifdef LOSCFG_CPUP_INCLUDE_IRQ
    OsCpupIrqStart(cpuid);  // 如果启用CPU性能统计，记录中断开始时间
#endif

    OsSchedIrqStartTime();  // 记录调度器中断开始时间
    OsHookCall(LOS_HOOK_TYPE_ISR_ENTER, intNum);  // 调用中断进入钩子函数
    hwiForm = (&g_hwiForm[intNum]);  // 获取中断号对应的句柄
#ifndef LOSCFG_NO_SHARED_IRQ
    while (hwiForm->pstNext != NULL) {  // 遍历共享中断链表
        hwiForm = hwiForm->pstNext;
#endif
        if (hwiForm->uwParam) {  // 检查是否有参数
            HWI_PROC_FUNC2 func = (HWI_PROC_FUNC2)hwiForm->pfnHook;  // 转换为带参数的函数指针
            if (func != NULL) {
                UINTPTR *param = (UINTPTR *)(hwiForm->uwParam);  // 获取参数指针
                func((INT32)(*param), (VOID *)(*(param + 1)));  // 调用带参数的中断处理函数
            }
        } else {
            HWI_PROC_FUNC0 func = (HWI_PROC_FUNC0)hwiForm->pfnHook;  // 转换为无参数的函数指针
            if (func != NULL) {
                func();  // 调用无参数的中断处理函数
            }
        }
#ifndef LOSCFG_NO_SHARED_IRQ
    }
#endif
    ++g_hwiFormCnt[cpuid][intNum];  // 递增当前CPU上该中断的触发计数

    OsHookCall(LOS_HOOK_TYPE_ISR_EXIT, intNum);  // 调用中断退出钩子函数
    OsSchedIrqUsedTimeUpdate();  // 更新调度器中断使用时间

#ifdef LOSCFG_CPUP_INCLUDE_IRQ
    OsCpupIrqEnd(cpuid, intNum);  // 如果启用CPU性能统计，记录中断结束时间
#endif
    /* Must keep the operation at the end of the interface */
    *intCnt = *intCnt - 1;  // 递减当前CPU的中断计数器
}

/**
 * @brief 复制中断参数
 * @param irqParam 原始中断参数指针
 * @return 复制后的参数指针，内存分配失败返回LOS_NOK
 */
STATIC HWI_ARG_T OsHwiCpIrqParam(const HwiIrqParam *irqParam)
{
    HwiIrqParam *paramByAlloc = NULL;  // 分配的参数副本指针

    if (irqParam != NULL) {
        paramByAlloc = (HwiIrqParam *)LOS_MemAlloc(m_aucSysMem0, sizeof(HwiIrqParam));  // 分配内存
        if (paramByAlloc == NULL) {
            return LOS_NOK;  // 内存分配失败
        }
        (VOID)memcpy_s(paramByAlloc, sizeof(HwiIrqParam), irqParam, sizeof(HwiIrqParam));  // 复制参数内容
    }
    /* When "irqParam" is NULL, the function return 0(LOS_OK). */
    return (HWI_ARG_T)paramByAlloc;  // 返回参数副本指针或NULL
}

#ifdef LOSCFG_NO_SHARED_IRQ  // 如果禁用共享中断功能
/**
 * @brief 删除非共享中断
 * @param hwiNum 中断号
 * @return 操作结果，LOS_OK表示成功
 */
STATIC UINT32 OsHwiDelNoShared(HWI_HANDLE_T hwiNum)
{
    UINT32 intSave;  // 中断状态保存变量

    HWI_LOCK(intSave);  // 获取中断锁
    g_hwiForm[hwiNum].pfnHook = NULL;  // 清除中断处理函数
    if (g_hwiForm[hwiNum].uwParam) {
        (VOID)LOS_MemFree(m_aucSysMem0, (VOID *)g_hwiForm[hwiNum].uwParam);  // 释放参数内存
    }
    g_hwiForm[hwiNum].uwParam = 0;  // 清除参数

    HWI_UNLOCK(intSave);  // 释放中断锁
    return LOS_OK;
}

/**
 * @brief 创建非共享中断
 * @param hwiNum 中断号
 * @param hwiMode 中断模式
 * @param hwiHandler 中断处理函数
 * @param irqParam 中断参数
 * @return 操作结果，成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 OsHwiCreateNoShared(HWI_HANDLE_T hwiNum, HWI_MODE_T hwiMode,
                                  HWI_PROC_FUNC hwiHandler, const HwiIrqParam *irqParam)
{
    HWI_ARG_T retParam;  // 参数复制结果
    UINT32 intSave;  // 中断状态保存变量

    HWI_LOCK(intSave);  // 获取中断锁
    if (g_hwiForm[hwiNum].pfnHook == NULL) {  // 检查中断是否未被创建
        g_hwiForm[hwiNum].pfnHook = hwiHandler;  // 设置中断处理函数

        retParam = OsHwiCpIrqParam(irqParam);  // 复制中断参数
        if (retParam == LOS_NOK) {  // 参数复制失败
            HWI_UNLOCK(intSave);  // 释放中断锁
            return OS_ERRNO_HWI_NO_MEMORY;  // 返回内存不足错误
        }
        g_hwiForm[hwiNum].uwParam = retParam;  // 设置中断参数
    } else {
        HWI_UNLOCK(intSave);  // 释放中断锁
        return OS_ERRNO_HWI_ALREADY_CREATED;  // 返回中断已存在错误
    }
    HWI_UNLOCK(intSave);  // 释放中断锁
    return LOS_OK;
}
#else  // 如果启用共享中断功能
/**
 * @brief 删除共享中断
 * @param hwiNum 中断号
 * @param irqParam 中断参数，包含设备ID
 * @return 操作结果，成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 OsHwiDelShared(HWI_HANDLE_T hwiNum, const HwiIrqParam *irqParam)
{
    HwiHandleForm *hwiForm = NULL;  // 中断句柄指针
    HwiHandleForm *hwiFormtmp = NULL;  // 临时中断句柄指针
    UINT32 hwiValid = FALSE;  // 中断是否找到标志
    UINT32 intSave;  // 中断状态保存变量

    HWI_LOCK(intSave);  // 获取中断锁
    hwiForm = &g_hwiForm[hwiNum];
    hwiFormtmp = hwiForm;

    if ((hwiForm->uwParam & IRQF_SHARED) && ((irqParam == NULL) || (irqParam->pDevId == NULL))) {  // 共享中断必须提供设备ID
        HWI_UNLOCK(intSave);
        return OS_ERRNO_HWI_SHARED_ERROR;  // 返回共享中断错误
    }

    if ((hwiForm->pstNext != NULL) && !(hwiForm->uwParam & IRQF_SHARED)) {  // 非共享中断处理
        hwiForm = hwiForm->pstNext;
        if (hwiForm->uwParam) {
            (VOID)LOS_MemFree(m_aucSysMem0, (VOID *)hwiForm->uwParam);  // 释放参数内存
        }
        (VOID)LOS_MemFree(m_aucSysMem0, hwiForm);  // 释放句柄内存
        hwiFormtmp->pstNext = NULL;  // 断开链表

        g_hwiFormName[hwiNum] = NULL;  // 清除中断名称

        HWI_UNLOCK(intSave);
        return LOS_OK;
    }
    hwiForm = hwiForm->pstNext;
    while (hwiForm != NULL) {  // 遍历共享中断链表
        if (((HwiIrqParam *)(hwiForm->uwParam))->pDevId != irqParam->pDevId) {  // 匹配设备ID
            hwiFormtmp = hwiForm;
            hwiForm = hwiForm->pstNext;
        } else {
            hwiFormtmp->pstNext = hwiForm->pstNext;  // 移除当前节点
            (VOID)LOS_MemFree(m_aucSysMem0, (VOID *)hwiForm->uwParam);  // 释放参数内存
            (VOID)LOS_MemFree(m_aucSysMem0, hwiForm);  // 释放句柄内存

            hwiValid = TRUE;  // 标记找到中断
            break;
        }
    }

    if (hwiValid != TRUE) {  // 未找到指定中断
        HWI_UNLOCK(intSave);
        return OS_ERRNO_HWI_HWINUM_UNCREATE;  // 返回中断未创建错误
    }

    if (g_hwiForm[hwiNum].pstNext == NULL) {  // 如果链表为空
        g_hwiForm[hwiNum].uwParam = 0;  // 清除参数
        g_hwiFormName[hwiNum] = NULL;  // 清除名称
    }

    HWI_UNLOCK(intSave);  // 释放中断锁
    return LOS_OK;
}

/**
 * @brief 创建共享中断
 * @param hwiNum 中断号
 * @param hwiMode 中断模式
 * @param hwiHandler 中断处理函数
 * @param irqParam 中断参数，包含设备ID
 * @return 操作结果，成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 OsHwiCreateShared(HWI_HANDLE_T hwiNum, HWI_MODE_T hwiMode,
                                HWI_PROC_FUNC hwiHandler, const HwiIrqParam *irqParam)
{
    UINT32 intSave;  // 中断状态保存变量
    HwiHandleForm *hwiFormNode = NULL;  // 新中断节点
    HwiHandleForm *hwiForm = NULL;  // 中断句柄指针
    HwiIrqParam *hwiParam = NULL;  // 中断参数指针
    HWI_MODE_T modeResult = hwiMode & IRQF_SHARED;  // 提取共享模式标志

    if (modeResult && ((irqParam == NULL) || (irqParam->pDevId == NULL))) {  // 共享中断必须提供设备ID
        return OS_ERRNO_HWI_SHARED_ERROR;  // 返回共享中断错误
    }

    HWI_LOCK(intSave);  // 获取中断锁

    hwiForm = &g_hwiForm[hwiNum];
    if ((hwiForm->pstNext != NULL) && ((modeResult == 0) || (!(hwiForm->uwParam & IRQF_SHARED)))) {  // 检查模式兼容性
        HWI_UNLOCK(intSave);
        return OS_ERRNO_HWI_SHARED_ERROR;  // 返回共享中断错误
    }

    while (hwiForm->pstNext != NULL) {  // 遍历链表检查设备ID是否已存在
        hwiForm = hwiForm->pstNext;
        hwiParam = (HwiIrqParam *)(hwiForm->uwParam);
        if (hwiParam->pDevId == irqParam->pDevId) {  // 设备ID已存在
            HWI_UNLOCK(intSave);
            return OS_ERRNO_HWI_ALREADY_CREATED;  // 返回中断已创建错误
        }
    }

    hwiFormNode = (HwiHandleForm *)LOS_MemAlloc(m_aucSysMem0, sizeof(HwiHandleForm));  // 分配新节点内存
    if (hwiFormNode == NULL) {  // 内存分配失败
        HWI_UNLOCK(intSave);
        return OS_ERRNO_HWI_NO_MEMORY;  // 返回内存不足错误
    }

    hwiFormNode->uwParam = OsHwiCpIrqParam(irqParam);  // 复制中断参数
    if (hwiFormNode->uwParam == LOS_NOK) {  // 参数复制失败
        HWI_UNLOCK(intSave);
        (VOID)LOS_MemFree(m_aucSysMem0, hwiFormNode);  // 释放节点内存
        return OS_ERRNO_HWI_NO_MEMORY;  // 返回内存不足错误
    }

    hwiFormNode->pfnHook = hwiHandler;  // 设置中断处理函数
    hwiFormNode->pstNext = (struct tagHwiHandleForm *)NULL;  // 初始化链表指针
    hwiForm->pstNext = hwiFormNode;  // 添加到链表尾部

    if ((irqParam != NULL) && (irqParam->pName != NULL)) {
        g_hwiFormName[hwiNum] = (CHAR *)irqParam->pName;  // 设置中断名称
    }

    g_hwiForm[hwiNum].uwParam = modeResult;  // 设置中断模式

    HWI_UNLOCK(intSave);  // 释放中断锁
    return LOS_OK;
}
#endif

/**
 * @brief 硬件中断初始化
 * @details 初始化中断句柄数组、名称数组，并调用硬件层中断初始化
 */
LITE_OS_SEC_TEXT_INIT VOID OsHwiInit(VOID)
{
    UINT32 hwiNum;  // 中断号循环变量

    for (hwiNum = 0; hwiNum < OS_HWI_MAX_NUM; hwiNum++) {  // 初始化所有中断句柄
        g_hwiForm[hwiNum].pfnHook = NULL;  // 清除处理函数
        g_hwiForm[hwiNum].uwParam = 0;  // 清除参数
        g_hwiForm[hwiNum].pstNext = NULL;  // 清除链表指针
    }

    (VOID)memset_s(g_hwiFormName, (sizeof(CHAR *) * OS_HWI_MAX_NUM), 0, (sizeof(CHAR *) * OS_HWI_MAX_NUM));  // 初始化名称数组

    HalIrqInit();  // 调用硬件抽象层中断初始化

    return;
}

/**
 * @brief 创建硬件中断
 * @param hwiNum 中断号
 * @param hwiPrio 中断优先级 (当前未使用)
 * @param hwiMode 中断模式
 * @param hwiHandler 中断处理函数
 * @param irqParam 中断参数
 * @return 操作结果，成功返回LOS_OK，失败返回错误码
 */
LITE_OS_SEC_TEXT_INIT UINT32 LOS_HwiCreate(HWI_HANDLE_T hwiNum,
                                           HWI_PRIOR_T hwiPrio,
                                           HWI_MODE_T hwiMode,
                                           HWI_PROC_FUNC hwiHandler,
                                           HwiIrqParam *irqParam)
{
    UINT32 ret;  // 返回结果

    (VOID)hwiPrio;  // 未使用的参数
    if (hwiHandler == NULL) {  // 检查处理函数是否为空
        return OS_ERRNO_HWI_PROC_FUNC_NULL;  // 返回处理函数为空错误
    }
    if ((hwiNum > OS_USER_HWI_MAX) || ((INT32)hwiNum < OS_USER_HWI_MIN)) {  // 检查中断号范围
        return OS_ERRNO_HWI_NUM_INVALID;  // 返回中断号无效错误
    }

#ifdef LOSCFG_NO_SHARED_IRQ
    ret = OsHwiCreateNoShared(hwiNum, hwiMode, hwiHandler, irqParam);  // 创建非共享中断
#else
    ret = OsHwiCreateShared(hwiNum, hwiMode, hwiHandler, irqParam);  // 创建共享中断
#endif
    return ret;
}

/**
 * @brief 删除硬件中断
 * @param hwiNum 中断号
 * @param irqParam 中断参数，共享中断需要提供设备ID
 * @return 操作结果，成功返回LOS_OK，失败返回错误码
 */
LITE_OS_SEC_TEXT_INIT UINT32 LOS_HwiDelete(HWI_HANDLE_T hwiNum, HwiIrqParam *irqParam)
{
    UINT32 ret;  // 返回结果

    if ((hwiNum > OS_USER_HWI_MAX) || ((INT32)hwiNum < OS_USER_HWI_MIN)) {  // 检查中断号范围
        return OS_ERRNO_HWI_NUM_INVALID;  // 返回中断号无效错误
    }

#ifdef LOSCFG_NO_SHARED_IRQ
    ret = OsHwiDelNoShared(hwiNum);  // 删除非共享中断
#else
    ret = OsHwiDelShared(hwiNum, irqParam);  // 删除共享中断
#endif
    return ret;
}