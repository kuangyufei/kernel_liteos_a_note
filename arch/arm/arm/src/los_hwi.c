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

#include "los_hwi.h"
#include "los_memory.h"
#include "los_spinlock.h"
#ifdef LOSCFG_KERNEL_CPUP
#include "los_cpup_pri.h"
#endif
#include "los_sched_pri.h"
#include "los_hook.h"


/* spinlock for hwi module, only available on SMP mode */
LITE_OS_SEC_BSS  SPIN_LOCK_INIT(g_hwiSpin); ///< 注意全局变量 g_hwiSpin 是在宏里面定义的
#define HWI_LOCK(state)       LOS_SpinLockSave(&g_hwiSpin, &(state))
#define HWI_UNLOCK(state)     LOS_SpinUnlockRestore(&g_hwiSpin, (state))

size_t g_intCount[LOSCFG_KERNEL_CORE_NUM] = {0};///< 记录每个CPUcore的中断数量 
HwiHandleForm g_hwiForm[OS_HWI_MAX_NUM];		///< 中断注册表        @note_why 用 form 来表示？有种写 HTML的感觉
STATIC CHAR *g_hwiFormName[OS_HWI_MAX_NUM] = {0};///< 记录每个硬中断的名称
STATIC UINT32 g_hwiFormCnt[LOSCFG_KERNEL_CORE_NUM][OS_HWI_MAX_NUM] = {0};

/**
 * @brief 获取某个中断的中断次数
 * 
 * @param index 
 * @return UINT32 
 */
UINT32 OsGetHwiFormCnt(UINT16 cpuid, UINT32 index)
{
    return g_hwiFormCnt[cpuid][index];
}

CHAR *OsGetHwiFormName(UINT32 index)//获取某个中断的名称
{
    return g_hwiFormName[index];
}
/// 获取系统支持的最大中断数
UINT32 LOS_GetSystemHwiMaximum(VOID)
{
    return OS_HWI_MAX_NUM;
}
typedef VOID (*HWI_PROC_FUNC0)(VOID);
typedef VOID (*HWI_PROC_FUNC2)(INT32, VOID *);
VOID OsInterrupt(UINT32 intNum)//中断实际处理函数
{
    HwiHandleForm *hwiForm = NULL;
    UINT32 *intCnt = NULL;
    UINT16 cpuid = ArchCurrCpuid();

    /* Must keep the operation at the beginning of the interface */
    intCnt = &g_intCount[cpuid];//当前CPU的中断总数量 ++
    *intCnt = *intCnt + 1;//@note_why 这里没看明白为什么要 +1

#ifdef LOSCFG_CPUP_INCLUDE_IRQ //开启查询系统CPU的占用率的中断
    OsCpupIrqStart(cpuid);
#endif
    OsSchedIrqStartTime();
    OsHookCall(LOS_HOOK_TYPE_ISR_ENTER, intNum);
    hwiForm = (&g_hwiForm[intNum]);//获取对应中断的实体
#ifndef LOSCFG_NO_SHARED_IRQ	//如果没有定义不共享中断 ，意思就是如果是共享中断
    while (hwiForm->pstNext != NULL) { //一直撸到最后
        hwiForm = hwiForm->pstNext;//下一个继续撸
#endif
        if (hwiForm->uwParam) {//有参数的情况
            HWI_PROC_FUNC2 func = (HWI_PROC_FUNC2)hwiForm->pfnHook;//获取回调函数
            if (func != NULL) {
                UINTPTR *param = (UINTPTR *)(hwiForm->uwParam);
                func((INT32)(*param), (VOID *)(*(param + 1)));//运行带参数的回调函数
            }
        } else {//没有参数的情况
            HWI_PROC_FUNC0 func = (HWI_PROC_FUNC0)hwiForm->pfnHook;//获取回调函数
            if (func != NULL) {
                func();//运行回调函数
            }
        }
#ifndef LOSCFG_NO_SHARED_IRQ
    }
#endif
    ++g_hwiFormCnt[cpuid][intNum];

    OsHookCall(LOS_HOOK_TYPE_ISR_EXIT, intNum);
    OsSchedIrqUsedTimeUpdate();

#ifdef LOSCFG_CPUP_INCLUDE_IRQ
    OsCpupIrqEnd(cpuid, intNum);
#endif
    /* Must keep the operation at the end of the interface */
    *intCnt = *intCnt - 1;
}
/**
 * @brief 申请内核空间并拷贝硬中断参数;
 * 
 * 该函数用于为传入的硬中断参数结构体分配内核空间，并将参数内容复制到新分配的空间中。如果传入的参数指针为 NULL，则直接返回 0。;
 * 
 * @param irqParam 指向原始硬中断参数结构体的常量指针。;
 * @return HWI_ARG_T 若内存分配成功，返回新分配内存的地址；若内存分配失败，返回 LOS_NOK；若 irqParam 为 NULL，返回 0。;
 */
STATIC HWI_ARG_T OsHwiCpIrqParam(const HwiIrqParam *irqParam)
{
    HwiIrqParam *paramByAlloc = NULL; // 用于存储新分配内存的指针，指向新的硬中断参数结构体;

    if (irqParam != NULL) { // 检查传入的硬中断参数指针是否不为 NULL;
        paramByAlloc = (HwiIrqParam *)LOS_MemAlloc(m_aucSysMem0, sizeof(HwiIrqParam)); // 调用内存分配函数，从 m_aucSysMem0 内存池分配 sizeof(HwiIrqParam) 大小的内存;
        if (paramByAlloc == NULL) { // 检查内存分配是否失败;
            return LOS_NOK; // 内存分配失败，返回错误码 LOS_NOK;
        }
        (VOID)memcpy_s(paramByAlloc, sizeof(HwiIrqParam), irqParam, sizeof(HwiIrqParam)); // 内存分配成功，将原始参数内容复制到新分配的内存中;
    }
    /* 当 "irqParam" 为 NULL 时，函数返回 0(LOS_OK)。 */
    return (HWI_ARG_T)paramByAlloc; // 将新分配内存的地址（或 0）转换为 HWI_ARG_T 类型返回;
}

#ifdef LOSCFG_NO_SHARED_IRQ
STATIC UINT32 OsHwiDelNoShared(HWI_HANDLE_T hwiNum)
{
    UINT32 intSave;

    HWI_LOCK(intSave);//申请硬中断自旋锁
    g_hwiForm[hwiNum].pfnHook = NULL;//回调函数直接NULL
    if (g_hwiForm[hwiNum].uwParam) {//如有参数
        (VOID)LOS_MemFree(m_aucSysMem0, (VOID *)g_hwiForm[hwiNum].uwParam);//释放内存
    }
    g_hwiForm[hwiNum].uwParam = 0; //NULL

    HWI_UNLOCK(intSave);//释放硬中断自旋锁
    return LOS_OK;
}
/**
 * @brief 删除一个共享中断
 * 
 * 该函数用于删除指定中断号的共享中断。会遍历共享中断链表，
 * 找到与传入设备 ID 匹配的中断处理节点并删除。
 * 
 * @param hwiNum 硬中断句柄编号
 * @param irqParam 硬中断参数，包含设备 ID 等信息
 * @return UINT32 操作结果，成功返回 LOS_OK，失败返回相应错误码
 */
STATIC UINT32 OsHwiDelShared(HWI_HANDLE_T hwiNum, const HwiIrqParam *irqParam)
{
    HwiHandleForm *hwiForm = NULL; // 指向当前遍历到的中断处理节点
    HwiHandleForm *hwiFormtmp = NULL; // 指向当前遍历节点的前一个节点
    UINT32 hwiValid = FALSE; // 标记是否找到要删除的中断节点
    UINT32 intSave; // 用于保存中断状态

    HWI_LOCK(intSave); // 加自旋锁，保护共享中断数据结构
    hwiForm = &g_hwiForm[hwiNum]; // 从全局注册的中断向量表中获取中断项
    hwiFormtmp = hwiForm;

    // 检查是否为共享中断且传入参数是否有效
    if ((hwiForm->uwParam & IRQF_SHARED) && ((irqParam == NULL) || (irqParam->pDevId == NULL))) {
        HWI_UNLOCK(intSave); // 解锁自旋锁
        return OS_ERRNO_HWI_SHARED_ERROR; // 返回共享中断参数错误码
    }

    // 处理非共享中断但有后续节点的情况
    if ((hwiForm->pstNext != NULL) && !(hwiForm->uwParam & IRQF_SHARED)) {
        hwiForm = hwiForm->pstNext; // 指向下一个中断处理节点
        if (hwiForm->uwParam) { // 如果节点有参数
            (VOID)LOS_MemFree(m_aucSysMem0, (VOID *)hwiForm->uwParam); // 释放参数占用的内存
        }
        (VOID)LOS_MemFree(m_aucSysMem0, hwiForm); // 释放当前中断处理节点的内存
        hwiFormtmp->pstNext = NULL; // 前一个节点的下一个节点指针置空

        g_hwiFormName[hwiNum] = NULL; // 清除该中断的名称

        HWI_UNLOCK(intSave); // 解锁自旋锁
        return LOS_OK; // 返回操作成功
    }

    hwiForm = hwiForm->pstNext; // 指向下一个中断处理节点
    // 遍历共享中断链表
    while (hwiForm != NULL) {
        // 检查当前节点的设备 ID 是否与传入的设备 ID 不匹配
        if (((HwiIrqParam *)(hwiForm->uwParam))->pDevId != irqParam->pDevId) {
            hwiFormtmp = hwiForm; // 更新前一个节点指针
            hwiForm = hwiForm->pstNext; // 指向下一个节点
        } else {
            hwiFormtmp->pstNext = hwiForm->pstNext; // 跳过当前节点
            (VOID)LOS_MemFree(m_aucSysMem0, (VOID *)hwiForm->uwParam); // 释放参数占用的内存
            (VOID)LOS_MemFree(m_aucSysMem0, hwiForm); // 释放当前中断处理节点的内存

            hwiValid = TRUE; // 标记找到要删除的节点
            break; // 跳出循环
        }
    }

    // 如果未找到要删除的节点
    if (hwiValid != TRUE) {
        HWI_UNLOCK(intSave); // 解锁自旋锁
        return OS_ERRNO_HWI_HWINUM_UNCREATE; // 返回中断号未创建错误码
    }

    // 如果该中断号的共享中断链表为空
    if (g_hwiForm[hwiNum].pstNext == NULL) {
        g_hwiForm[hwiNum].uwParam = 0; // 清除中断参数
        g_hwiFormName[hwiNum] = NULL; // 清除该中断的名称
    }

    HWI_UNLOCK(intSave); // 解锁自旋锁
    return LOS_OK; // 返回操作成功
}
///创建一个共享硬件中断,共享中断就是一个中断能触发多个响应函数
STATIC UINT32 OsHwiCreateShared(HWI_HANDLE_T hwiNum, HWI_MODE_T hwiMode,
                                HWI_PROC_FUNC hwiHandler, const HwiIrqParam *irqParam)
{
    UINT32 intSave;
    HwiHandleForm *hwiFormNode = NULL;
    HwiHandleForm *hwiForm = NULL;
    HwiIrqParam *hwiParam = NULL;
    HWI_MODE_T modeResult = hwiMode & IRQF_SHARED;

    if (modeResult && ((irqParam == NULL) || (irqParam->pDevId == NULL))) {
        return OS_ERRNO_HWI_SHARED_ERROR;
    }

    HWI_LOCK(intSave);//中断自旋锁

    hwiForm = &g_hwiForm[hwiNum];//获取中断处理项
    if ((hwiForm->pstNext != NULL) && ((modeResult == 0) || (!(hwiForm->uwParam & IRQF_SHARED)))) {
        HWI_UNLOCK(intSave);
        return OS_ERRNO_HWI_SHARED_ERROR;
    }

    while (hwiForm->pstNext != NULL) {//pstNext指向 共享中断的各处理函数节点,此处一直撸到最后一个
        hwiForm = hwiForm->pstNext;//找下一个中断
        hwiParam = (HwiIrqParam *)(hwiForm->uwParam);//获取中断参数,用于检测该设备ID是否已经有中断处理函数
        if (hwiParam->pDevId == irqParam->pDevId) {//设备ID一致时,说明设备对应的中断处理函数已经存在了.
            HWI_UNLOCK(intSave);
            return OS_ERRNO_HWI_ALREADY_CREATED;
        }
    }

    hwiFormNode = (HwiHandleForm *)LOS_MemAlloc(m_aucSysMem0, sizeof(HwiHandleForm));//创建一个中断处理节点
    if (hwiFormNode == NULL) {
        HWI_UNLOCK(intSave);
        return OS_ERRNO_HWI_NO_MEMORY;
    }

    hwiFormNode->uwParam = OsHwiCpIrqParam(irqParam);//获取中断处理函数的参数
    if (hwiFormNode->uwParam == LOS_NOK) {
        HWI_UNLOCK(intSave);
        (VOID)LOS_MemFree(m_aucSysMem0, hwiFormNode);
        return OS_ERRNO_HWI_NO_MEMORY;
    }

    hwiFormNode->pfnHook = hwiHandler;//绑定中断处理函数
    hwiFormNode->pstNext = (struct tagHwiHandleForm *)NULL;//指定下一个中断为NULL,用于后续遍历找到最后一个中断项(见于以上 while (hwiForm->pstNext != NULL)处)
    hwiForm->pstNext = hwiFormNode;//共享中断

    if ((irqParam != NULL) && (irqParam->pName != NULL)) {
        g_hwiFormName[hwiNum] = (CHAR *)irqParam->pName;
    }

    g_hwiForm[hwiNum].uwParam = modeResult;

    HWI_UNLOCK(intSave);
    return LOS_OK;
}
#endif

/*
 * Description : initialization of the hardware interrupt
 */
LITE_OS_SEC_TEXT_INIT VOID OsHwiInit(VOID)//硬件中断初始化
{
    UINT32 hwiNum;

    for (hwiNum = 0; hwiNum < OS_HWI_MAX_NUM; hwiNum++) {//初始化中断向量表,默认128个中断
        g_hwiForm[hwiNum].pfnHook = NULL;
        g_hwiForm[hwiNum].uwParam = 0;
        g_hwiForm[hwiNum].pstNext = NULL;
    }

    (VOID)memset_s(g_hwiFormName, (sizeof(CHAR *) * OS_HWI_MAX_NUM), 0, (sizeof(CHAR *) * OS_HWI_MAX_NUM));

    HalIrqInit();

    return;
}

/**
 * @brief  创建一个硬中断
    \n 中断创建，注册中断号、中断触发模式、中断优先级、中断处理程序。中断被触发时，
    \n handleIrq会调用该中断处理程序
 * @param hwiNum 硬中断句柄编号 默认范围[0-127]
 * @param hwiPrio 硬中断优先级	
 * @param hwiMode 硬中断模式 共享和非共享
 * @param hwiHandler 硬中断处理函数
 * @param irqParam 硬中断处理函数参数
 * @return LITE_OS_SEC_TEXT_INIT 
 */
LITE_OS_SEC_TEXT_INIT UINT32 LOS_HwiCreate(HWI_HANDLE_T hwiNum,
                                           HWI_PRIOR_T hwiPrio,
                                           HWI_MODE_T hwiMode,
                                           HWI_PROC_FUNC hwiHandler,
                                           HwiIrqParam *irqParam)
{
    UINT32 ret;

    (VOID)hwiPrio;
    if (hwiHandler == NULL) {//中断处理函数不能为NULL
        return OS_ERRNO_HWI_PROC_FUNC_NULL;
    }
    if ((hwiNum > OS_USER_HWI_MAX) || ((INT32)hwiNum < OS_USER_HWI_MIN)) {//中断数区间限制 [32,96]
        return OS_ERRNO_HWI_NUM_INVALID;
    }

#ifdef LOSCFG_NO_SHARED_IRQ	//不支持共享中断
    ret = OsHwiCreateNoShared(hwiNum, hwiMode, hwiHandler, irqParam);
#else
    ret = OsHwiCreateShared(hwiNum, hwiMode, hwiHandler, irqParam);
#endif
    return ret;
}
///删除一个硬中断
LITE_OS_SEC_TEXT_INIT UINT32 LOS_HwiDelete(HWI_HANDLE_T hwiNum, HwiIrqParam *irqParam)
{
    UINT32 ret;

    if ((hwiNum > OS_USER_HWI_MAX) || ((INT32)hwiNum < OS_USER_HWI_MIN)) {
        return OS_ERRNO_HWI_NUM_INVALID;
    }

#ifdef LOSCFG_NO_SHARED_IRQ	//不支持共享中断
    ret = OsHwiDelNoShared(hwiNum);
#else
    ret = OsHwiDelShared(hwiNum, irqParam);
#endif
    return ret;
}

