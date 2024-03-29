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
 * @defgroup los_hwi Hardware interrupt
 * @ingroup kernel
 */
#ifndef _LOS_HWI_H
#define _LOS_HWI_H

#include "los_base.h"
#include "los_hw_cpu.h"
#include "hal_hwi.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_hwi
 * Count of interrupts.
 */ 
extern size_t g_intCount[];///< 中断次数,每个CPU都会记录响应中断的次数

/**
 * @ingroup los_hwi
 * An interrupt is active. | 中断处于活动状态
 */
#define OS_INT_ACTIVE ({                    \
    size_t intCount;                        \
    UINT32 intSave_ = LOS_IntLock();        \
    intCount = g_intCount[ArchCurrCpuid()]; \
    LOS_IntRestore(intSave_);               \
    intCount;                               \
})

/**
 * @ingroup los_hwi
 * An interrupt is inactive. | 中断处于非活动状态
 */
#define OS_INT_INACTIVE (!(OS_INT_ACTIVE))

/**
 * @ingroup los_hwi
 * Highest priority of a hardware interrupt. | 硬件中断的最高优先级
 */
#define OS_HWI_PRIO_HIGHEST 0

/**
 * @ingroup los_hwi
 * Lowest priority of a hardware interrupt. | 硬件中断的最低优先级
 */
#define OS_HWI_PRIO_LOWEST 31

/**
 * @ingroup los_hwi
 * Max name length of a hardware interrupt. | 硬件中断的最大名称长度
 */
#define OS_HWI_MAX_NAMELEN 10

/**
 * @ingroup los_hwi
 * Hardware interrupt error code: Invalid interrupt number. | 创建或删除中断时，传入了无效中断号
 *
 * Value: 0x02000900
 *
 * Solution: Ensure that the interrupt number is valid.
 */
#define OS_ERRNO_HWI_NUM_INVALID                LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x00)

/**
 * @ingroup los_hwi
 * Hardware interrupt error code: Null hardware interrupt handling function.
 *
 * Value: 0x02000901
 *
 * Solution: Pass in a valid non-null hardware interrupt handling function.
 */	//创建中断时，传入的中断处理程序指针为空
#define OS_ERRNO_HWI_PROC_FUNC_NULL             LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x01)

/**
 * @ingroup los_hwi
 * Hardware interrupt error code: Insufficient interrupt resources for hardware interrupt creation.
 *
 * Value: 0x02000902
 *
 * Solution: Increase the configured maximum number of supported hardware interrupts.
 */	//无可用中断资源
#define OS_ERRNO_HWI_CB_UNAVAILABLE             LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x02)

/**
 * @ingroup los_hwi
 * Hardware interrupt error code: Insufficient memory for hardware interrupt initialization.
 *
 * Value: 0x02000903
 *
 * Solution: Expand the configured memory.
 */	//创建中断时，出现内存不足的情况
#define OS_ERRNO_HWI_NO_MEMORY                  LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x03)

/**
 * @ingroup los_hwi
 * Hardware interrupt error code: The interrupt has already been created.
 *
 * Value: 0x02000904
 *
 * Solution: Check whether the interrupt specified by the passed-in interrupt number has already been created.
 */	//创建中断时，发现要注册的中断号已经创建
#define OS_ERRNO_HWI_ALREADY_CREATED            LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x04)

/**
 * @ingroup los_hwi
 * Hardware interrupt error code: Invalid interrupt priority.
 *
 * Value: 0x02000905
 *
 * Solution: Ensure that the interrupt priority is valid.
 */	//创建中断时，传入的中断优先级无效
#define OS_ERRNO_HWI_PRIO_INVALID               LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x05)

/**
 * @ingroup los_hwi
 * Hardware interrupt error code: Incorrect interrupt creation mode.
 *
 * Value: 0x02000906
 *
 * Solution: The interrupt creation mode can be only set to OS_HWI_MODE_COMM or OS_HWI_MODE_FAST of
 * which the value can be 0 or 1.
 */	//中断模式无效
#define OS_ERRNO_HWI_MODE_INVALID               LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x06)

/**
 * @ingroup los_hwi
 * Hardware interrupt error code: The interrupt has already been created as a fast interrupt.
 *
 * Value: 0x02000907
 *
 * Solution: Check whether the interrupt specified by the passed-in interrupt number has already been created.
 */	//创建硬中断时，发现要注册的中断号，已经创建为快速中断
#define OS_ERRNO_HWI_FASTMODE_ALREADY_CREATED LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x07)

/**
 * @ingroup los_hwi
 * Hardware interrupt error code: The API is called during an interrupt, which is forbidden.
 *
 * Value: 0x02000908
 *
 * * Solution: Do not call the API during an interrupt.
 */	//接口在中断中调用
#define OS_ERRNO_HWI_INTERR                     LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x08)

/**
 * @ingroup los_hwi
 * Hardware interrupt error code:the hwi support SHARED error.
 *
 * Value: 0x02000909
 *
 * * Solution: Check the input params hwiMode and irqParam of LOS_HwiCreate or
 * LOS_HwiDelete whether adapt the current hwi.
 */ //中断共享出现错误
#define OS_ERRNO_HWI_SHARED_ERROR               LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x09)

/**
 * @ingroup los_hwi
 * Hardware interrupt error code:Invalid interrupt Arg when interrupt mode is IRQF_SHARED.
 *
 * Value: 0x0200090a
 *
 * * Solution: Check the interrupt Arg, Arg should not be NULL and pDevId should not be NULL.
 */ //注册中断入参有误
#define OS_ERRNO_HWI_ARG_INVALID                LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x0a)

/**
 * @ingroup los_hwi
 * Hardware interrupt error code:The interrupt corresponded to the hwi number or devid  has not been created.
 *
 * Value: 0x0200090b
 *
 * * Solution: Check the hwi number or devid, make sure the hwi number or devid need to delete.
 */	//中断共享情况下，删除中断时，中断号对应的链表中，无法匹配到相应的设备ID
#define OS_ERRNO_HWI_HWINUM_UNCREATE            LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x0b)

/**
 * @ingroup los_hwi
 * Define the type of a hardware interrupt number.		//定义硬件中断号的类型
 */
typedef UINT32 HWI_HANDLE_T;

/**
 * @ingroup los_hwi
 * Define the type of a hardware interrupt priority.	//定义硬件中断优先级的类型
 */
typedef UINT16 HWI_PRIOR_T; //定义

/**
 * @ingroup los_hwi
 * Define the type of hardware interrupt mode configurations.	//定义硬件中断配置的类型
 */
typedef UINT16 HWI_MODE_T;

/**
 * @ingroup los_hwi
 * Define the type of the parameter used for the hardware interrupt creation function.
 * The function of this parameter varies among platforms.
 */							//定义用于硬件中断创建功能的参数类型。此参数的功能因平台而异
typedef UINTPTR HWI_ARG_T;

/**
 * @ingroup  los_hwi
 * Define the type of a hardware interrupt handling function. //定义硬件中断处理函数的类型
 */
typedef VOID (*HWI_PROC_FUNC)(VOID);

/*
 * These flags used only by the kernel as part of the
 * irq handling routines.
 *
 * IRQF_SHARED - allow sharing the irq among several devices
 */
#define IRQF_SHARED 0x8000U	//IRQF_SHARED-允许在多个设备之间共享irq

typedef struct tagHwiHandleForm {	
    HWI_PROC_FUNC pfnHook;	///< 中断处理函数
    HWI_ARG_T uwParam;		///< 中断处理函数参数
    struct tagHwiHandleForm *pstNext;	///< 节点，指向下一个中断,用于共享中断的情况
} HwiHandleForm;

typedef struct tagIrqParam {	//中断参数
    int swIrq;				///<	软件中断
    VOID *pDevId;			///<	设备ID
    const CHAR *pName;		///< 名称
} HwiIrqParam;

extern HwiHandleForm g_hwiForm[OS_HWI_MAX_NUM];//中断注册表

/**
 * @ingroup los_hwi
 * @brief Disable all interrupts. | 关闭当前处理器所有中断响应
 *
 * @par Description:
 * <ul>
 * <li>This API is used to disable all IRQ and FIQ interrupts in the CPSR.</li>
 * </ul>
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @param None.
 *
 * @retval #UINT32 CPSR value obtained before all interrupts are disabled.
 * @par Dependency:
 * <ul><li>los_hwi.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_IntRestore
 */ 
STATIC INLINE UINT32 LOS_IntLock(VOID)
{//此API用于禁用CPSR中的所有IRQ和FIQ中断。CPSR:程序状态寄存器(current program status register)
    return ArchIntLock();
}//IRQ(Interrupt Request)：指中断模式。FIQ(Fast Interrupt Request)：指快速中断模式。

/**
 * @ingroup los_hwi
 * @brief Enable all interrupts. | 打开当前处理器所有中断响应
 *
 * @par Description:
 * <ul>
 * <li>This API is used to enable all IRQ and FIQ interrupts in the CPSR.</li>
 * </ul>
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @param None.
 *
 * @retval #UINT32 CPSR value obtained after all interrupts are enabled.
 * @par Dependency:
 * <ul><li>los_hwi.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_IntLock
 */
STATIC INLINE UINT32 LOS_IntUnLock(VOID)
{//此API用于启用CPSR中的所有IRQ和FIQ中断。
    return ArchIntUnlock();
}

/**
 * @ingroup los_hwi
 * @brief Restore interrupts. | 恢复到使用LOS_IntLock关闭所有中断之前的状态
 *
 * @par Description:
 * <ul>
 * <li>This API is used to restore the CPSR value obtained before all interrupts are disabled.</li>
 * </ul>
 * @attention
 * <ul>
 * <li>This API can be called only after all interrupts are disabled, and the input parameter value should be
 * the value returned by LOS_IntLock.</li>
 * </ul>
 *
 * @param intSave [IN] Type #UINT32 : CPSR value obtained before all interrupts are disabled.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_hwi.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_IntLock
 */
STATIC INLINE VOID LOS_IntRestore(UINT32 intSave)
{//只有在禁用所有中断之后才能调用此API，并且输入参数值应为LOS_IntLock返回的值。
    ArchIntRestore(intSave);
}

/**
 * @ingroup los_hwi
 * @brief Gets the maximum number of interrupts supported by the system.
 *
 * @par Description:
 * <ul>
 * <li>This API is used to gets the maximum number of interrupts supported by the system.</li>
 * </ul>
 *
 * @param  None.
 *
 * @retval None.
 * @par Dependency:
 * <ul><li>los_hwi.h: the header file that contains the API declaration.</li></ul>
 */
extern UINT32 LOS_GetSystemHwiMaximum(VOID);

/**
 * @ingroup  los_hwi
 * @brief Create a hardware interrupt.
 *
 * @par Description:
 * This API is used to configure a hardware interrupt and register a hardware interrupt handling function.
 *
 * @attention
 * <ul>
 * <li>The hardware interrupt module is usable only when the configuration item for
 * hardware interrupt tailoring is enabled.</li>
 * <li>Hardware interrupt number value range: [OS_USER_HWI_MIN,OS_USER_HWI_MAX]. </li>
 * <li>OS_HWI_MAX_NUM specifies the maximum number of interrupts that can be created.</li>
 * <li>Before executing an interrupt on a platform, refer to the chip manual of the platform.</li>
 * <li>The parameter handler of this interface is a interrupt handler, it should be correct, otherwise,
 * the system may be abnormal.</li>
 * <li>The input irqParam could be NULL, if not, it should be address which point to a struct HwiIrqParam</li>
 * </ul>
 *
 * @param  hwiNum     [IN] Type #HWI_HANDLE_T: hardware interrupt number.
 *                                             for an ARM926 platform is [0,31].
 * @param  hwiPrio    [IN] Type #HWI_PRIOR_T: hardware interrupt priority. The value range is
 *                                            [0, GIC_MAX_INTERRUPT_PREEMPTION_LEVEL - 1] << PRIORITY_SHIFT.
 * @param  hwiMode    [IN] Type #HWI_MODE_T: hardware interrupt mode. Ignore this parameter temporarily.
 * @param  hwiHandler [IN] Type #HWI_PROC_FUNC: interrupt handler used when a hardware interrupt is triggered.
 * @param  irqParam   [IN] Type #HwiIrqParam: input parameter of the interrupt handler used when
 *                                                a hardware interrupt is triggered.
 *
 * @retval #OS_ERRNO_HWI_PROC_FUNC_NULL              Null hardware interrupt handling function.
 * @retval #OS_ERRNO_HWI_NUM_INVALID                 Invalid interrupt number.
 * @retval #OS_ERRNO_HWI_NO_MEMORY                   Insufficient memory for hardware interrupt creation.
 * @retval #OS_ERRNO_HWI_ALREADY_CREATED             The interrupt handler being created has already been created.
 * @retval #LOS_OK                                   The interrupt is successfully created.
 * @par Dependency:
 * <ul><li>los_hwi.h: the header file that contains the API declaration.</li></ul>
 * @see None.
 */ //中断创建，注册中断号、中断触发模式、中断优先级、中断处理程序。中断被触发时，handleIrq会调用该中断处理程序
extern UINT32 LOS_HwiCreate(HWI_HANDLE_T hwiNum,
                            HWI_PRIOR_T hwiPrio,
                            HWI_MODE_T hwiMode,
                            HWI_PROC_FUNC hwiHandler,
                            HwiIrqParam *irqParam);

/**
 * @ingroup  los_hwi
 * @brief delete a hardware interrupt.
 *
 * @par Description:
 * This API is used to delete a hardware interrupt.
 *
 * @attention
 * <ul>
 * <li>The hardware interrupt module is usable only when the configuration item for
 * hardware interrupt tailoring is enabled.</li>
 * <li>Hardware interrupt number value range: [OS_USER_HWI_MIN,OS_USER_HWI_MAX].</li>
 * <li>OS_HWI_MAX_NUM specifies the maximum number of interrupts that can be created.</li>
 * <li>Before executing an interrupt on a platform, refer to the chip manual of the platform.</li>
 * </ul>
 *
 * @param  hwiNum   [IN] Type #HWI_HANDLE_T: hardware interrupt number.
 * @param  irqParam [IN] Type #HwiIrqParam *: id of hardware interrupt which will base on
 *                                                when delete the hardware interrupt.
 *
 * @retval #OS_ERRNO_HWI_NUM_INVALID         Invalid interrupt number.
 * @retval #OS_ERRNO_HWI_SHARED_ERROR        Invalid interrupt mode.
 * @retval #LOS_OK                           The interrupt is successfully deleted.
 * @retval #LOS_NOK                          The interrupt is failed deleted based on the pDev_ID.

 * @par Dependency:
 * <ul><li>los_hwi.h: the header file that contains the API declaration.</li></ul>
 * @see None.
 */ //删除中断
extern UINT32 LOS_HwiDelete(HWI_HANDLE_T hwiNum, HwiIrqParam *irqParam);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_HWI_H */
