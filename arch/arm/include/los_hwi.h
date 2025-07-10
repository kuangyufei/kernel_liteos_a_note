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
 * 中断计数器数组，记录每个CPU核的中断嵌套次数
 * 数组索引对应CPU核ID，值为当前中断嵌套深度
 */
extern size_t g_intCount[];

/**
 * @ingroup los_hwi
 * 判断当前是否处于中断活跃状态
 *
 * @return size_t 中断嵌套次数，0表示无中断活跃，>0表示中断活跃
 * @note 通过读取当前CPU核的中断计数器实现，操作过程关中断保证原子性
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
 * 判断当前是否处于中断非活跃状态
 *
 * @return bool true表示无中断活跃，false表示中断活跃
 * @note 本质是对OS_INT_ACTIVE取反操作
 */
#define OS_INT_INACTIVE (!(OS_INT_ACTIVE))

/**
 * @ingroup los_hwi
 * 硬件中断最高优先级
 *
 * 值为0（十进制），表示系统中可设置的最高中断优先级
 */
#define OS_HWI_PRIO_HIGHEST 0

/**
 * @ingroup los_hwi
 * 硬件中断最低优先级
 *
 * 值为31（十进制），表示系统中可设置的最低中断优先级
 * @note 优先级数值越大，实际优先级越低
 */
#define OS_HWI_PRIO_LOWEST 31

/**
 * @ingroup los_hwi
 * 硬件中断名称最大长度
 *
 * 值为10（十进制），表示中断名称字符串的最大字符数（含终止符）
 */
#define OS_HWI_MAX_NAMELEN 10

/**
 * @ingroup los_hwi
 * 硬件中断错误码：无效的中断号
 *
 * 值：0x02000900（十进制536873216）
 *
 * 解决方案：确保传入的中断号在系统支持的有效范围内
 */
#define OS_ERRNO_HWI_NUM_INVALID                LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x00)

/**
 * @ingroup los_hwi
 * 硬件中断错误码：中断处理函数为空
 *
 * 值：0x02000901（十进制536873217）
 *
 * 解决方案：传入有效的非空硬件中断处理函数
 */
#define OS_ERRNO_HWI_PROC_FUNC_NULL             LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x01)

/**
 * @ingroup los_hwi
 * 硬件中断错误码：创建中断时资源不足
 *
 * 值：0x02000902（十进制536873218）
 *
 * 解决方案：增加配置的最大支持硬件中断数量
 */
#define OS_ERRNO_HWI_CB_UNAVAILABLE             LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x02)

/**
 * @ingroup los_hwi
 * 硬件中断错误码：初始化中断时内存不足
 *
 * 值：0x02000903（十进制536873219）
 *
 * 解决方案：扩展配置的系统内存大小
 */
#define OS_ERRNO_HWI_NO_MEMORY                  LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x03)

/**
 * @ingroup los_hwi
 * 硬件中断错误码：中断已创建
 *
 * 值：0x02000904（十进制536873220）
 *
 * 解决方案：检查传入的中断号对应的中断是否已创建
 */
#define OS_ERRNO_HWI_ALREADY_CREATED            LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x04)

/**
 * @ingroup los_hwi
 * 硬件中断错误码：无效的中断优先级
 *
 * 值：0x02000905（十进制536873221）
 *
 * 解决方案：确保中断优先级在有效范围内（0-31）
 */
#define OS_ERRNO_HWI_PRIO_INVALID               LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x05)
/**
 * @ingroup los_hwi
 * 硬件中断错误码：中断创建模式不正确
 *
 * 值：0x02000906（十进制536873222）
 *
 * 解决方案：中断创建模式只能设置为OS_HWI_MODE_COMM（0）或OS_HWI_MODE_FAST（1）
 */
#define OS_ERRNO_HWI_MODE_INVALID               LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x06)

/**
 * @ingroup los_hwi
 * 硬件中断错误码：中断已创建为快速中断
 *
 * 值：0x02000907（十进制536873223）
 *
 * 解决方案：检查传入的中断号对应的中断是否已创建为快速中断
 */
#define OS_ERRNO_HWI_FASTMODE_ALREADY_CREATED LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x07)

/**
 * @ingroup los_hwi
 * 硬件中断错误码：在中断中调用禁止的API
 *
 * 值：0x02000908（十进制536873224）
 *
 * 解决方案：不要在中断处理过程中调用此API
 */
#define OS_ERRNO_HWI_INTERR                     LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x08)

/**
 * @ingroup los_hwi
 * 硬件中断错误码：不支持共享中断模式
 *
 * 值：0x02000909（十进制536873225）
 *
 * 解决方案：检查LOS_HwiCreate或LOS_HwiDelete的hwiMode和irqParam参数是否适配当前中断
 */
#define OS_ERRNO_HWI_SHARED_ERROR               LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x09)

/**
 * @ingroup los_hwi
 * 硬件中断错误码：共享中断模式下参数无效
 *
 * 值：0x0200090a（十进制536873226）
 *
 * 解决方案：检查中断参数Arg和pDevId，两者都不应为NULL
 */
#define OS_ERRNO_HWI_ARG_INVALID                LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x0a)

/**
 * @ingroup los_hwi
 * 硬件中断错误码：中断号或设备ID对应的中断未创建
 *
 * 值：0x0200090b（十进制536873227）
 *
 * 解决方案：检查中断号或设备ID，确保要删除的中断已创建
 */
#define OS_ERRNO_HWI_HWINUM_UNCREATE            LOS_ERRNO_OS_ERROR(LOS_MOD_HWI, 0x0b)

/**
 * @ingroup los_hwi
 * 硬件中断号类型定义
 *
 * 用于标识一个硬件中断的唯一编号，不同平台支持的范围不同
 */
typedef UINT32 HWI_HANDLE_T;

/**
 * @ingroup los_hwi
 * 硬件中断优先级类型定义
 *
 * 用于表示中断的优先级级别，数值越小优先级越高
 */
typedef UINT16 HWI_PRIOR_T;

/**
 * @ingroup los_hwi
 * 硬件中断模式配置类型定义
 *
 * 用于指定中断的工作模式（如快速中断/普通中断、共享/独占等）
 */
typedef UINT16 HWI_MODE_T;

/**
 * @ingroup los_hwi
 * 硬件中断创建函数参数类型定义
 *
 * 该参数的具体功能因平台而异，通常用于传递中断特定的配置信息
 */
typedef UINTPTR HWI_ARG_T;

/**
 * @ingroup  los_hwi
 * 硬件中断处理函数类型定义
 *
 * 中断触发时执行的回调函数原型，无返回值且无参数
 */
typedef VOID (*HWI_PROC_FUNC)(VOID);

/*
 * 内核中断处理例程专用标志
 *
 * IRQF_SHARED - 允许多个设备共享同一个中断号
 */
#define IRQF_SHARED 0x8000U  /* 共享中断标志（十六进制0x8000，十进制32768） */

/**
 * @ingroup los_hwi
 * 硬件中断句柄表单结构
 *
 * 用于管理中断处理函数链表，支持共享中断功能
 */
typedef struct tagHwiHandleForm {
    HWI_PROC_FUNC pfnHook;       /* 中断处理函数指针 */
    HWI_ARG_T uwParam;           /* 中断处理函数参数 */
    struct tagHwiHandleForm *pstNext; /* 指向下一个中断句柄的链表指针，用于共享中断 */
} HwiHandleForm;

/**
 * @ingroup los_hwi
 * 中断参数结构
 *
 * 用于传递中断相关的软件信息，如软件中断号、设备ID和名称
 */
typedef struct tagIrqParam {
    int swIrq;                   /* 软件中断号 */
    VOID *pDevId;                /* 设备ID，用于共享中断时标识特定设备 */
    const CHAR *pName;           /* 中断名称字符串 */
} HwiIrqParam;

extern HwiHandleForm g_hwiForm[OS_HWI_MAX_NUM];  /* 中断句柄表单数组，大小为OS_HWI_MAX_NUM */

/**
 * @ingroup los_hwi
 * @brief 关闭所有中断
 *
 * @par 描述
 * <ul>
 * <li>此API用于关闭CPSR中的所有IRQ和FIQ中断</li>
 * </ul>
 * @attention
 * <ul>
 * <li>关闭中断后应尽快恢复，避免影响系统实时性</li>
 * </ul>
 *
 * @param 无
 *
 * @retval #UINT32 关闭所有中断前获取的CPSR值，用于后续恢复
 * @par 依赖
 * <ul><li>los_hwi.h: 包含API声明的头文件</li></ul>
 * @see LOS_IntRestore
 */
STATIC INLINE UINT32 LOS_IntLock(VOID)
{
    return ArchIntLock();
}

/**
 * @ingroup los_hwi
 * @brief 开启所有中断
 *
 * @par 描述
 * <ul>
 * <li>此API用于开启CPSR中的所有IRQ和FIQ中断</li>
 * </ul>
 * @attention
 * <ul>
 * <li>开启中断前应确保系统处于安全状态</li>
 * </ul>
 *
 * @param 无
 *
 * @retval #UINT32 开启所有中断后获取的CPSR值
 * @par 依赖
 * <ul><li>los_hwi.h: 包含API声明的头文件</li></ul>
 * @see LOS_IntLock
 */
STATIC INLINE UINT32 LOS_IntUnLock(VOID)
{
    return ArchIntUnlock();
}

/**
 * @ingroup los_hwi
 * @brief 恢复中断状态
 *
 * @par 描述
 * <ul>
 * <li>此API用于恢复调用LOS_IntLock前的CPSR值</li>
 * </ul>
 * @attention
 * <ul>
 * <li>只能在关闭中断后调用此API，输入参数必须是LOS_IntLock返回的值</li>
 * </ul>
 *
 * @param intSave [IN] Type #UINT32 : 关闭所有中断前获取的CPSR值
 *
 * @retval 无
 * @par 依赖
 * <ul><li>los_hwi.h: 包含API声明的头文件</li></ul>
 * @see LOS_IntLock
 */
STATIC INLINE VOID LOS_IntRestore(UINT32 intSave)
{
    ArchIntRestore(intSave);
}

/**
 * @ingroup los_hwi
 * @brief 获取系统支持的最大中断数
 *
 * @par 描述
 * <ul>
 * <li>此API用于获取系统配置支持的最大中断数量</li>
 * </ul>
 *
 * @param 无
 *
 * @retval UINT32 系统支持的最大中断数
 * @par 依赖
 * <ul><li>los_hwi.h: 包含API声明的头文件</li></ul>
 */
extern UINT32 LOS_GetSystemHwiMaximum(VOID);

/**
 * @ingroup  los_hwi
 * @brief 创建硬件中断
 *
 * @par 描述
 * 此API用于配置硬件中断并注册硬件中断处理函数
 *
 * @attention
 * <ul>
 * <li>只有当中断裁剪配置项启用时，硬件中断模块才可用</li>
 * <li>硬件中断号取值范围: [OS_USER_HWI_MIN, OS_USER_HWI_MAX]</li>
 * <li>OS_HWI_MAX_NUM指定可创建的最大中断数</li>
 * <li>在平台上执行中断前，请参考该平台的芯片手册</li>
 * <li>此接口的handler参数是中断处理函数，必须正确设置，否则系统可能异常</li>
 * <li>输入参数irqParam可以为NULL，若不为NULL，则必须指向HwiIrqParam结构体</li>
 * </ul>
 *
 * @param  hwiNum     [IN] Type #HWI_HANDLE_T: 硬件中断号，ARM926平台范围是[0,31]
 * @param  hwiPrio    [IN] Type #HWI_PRIOR_T: 硬件中断优先级，取值范围为[0, GIC_MAX_INTERRUPT_PREEMPTION_LEVEL - 1] << PRIORITY_SHIFT
 * @param  hwiMode    [IN] Type #HWI_MODE_T: 硬件中断模式，暂时忽略此参数
 * @param  hwiHandler [IN] Type #HWI_PROC_FUNC: 硬件中断触发时使用的中断处理函数
 * @param  irqParam   [IN] Type #HwiIrqParam*: 硬件中断触发时使用的中断处理函数的输入参数
 *
 * @retval #OS_ERRNO_HWI_PROC_FUNC_NULL      硬件中断处理函数为空
 * @retval #OS_ERRNO_HWI_NUM_INVALID         无效的中断号
 * @retval #OS_ERRNO_HWI_NO_MEMORY           创建硬件中断内存不足
 * @retval #OS_ERRNO_HWI_ALREADY_CREATED     要创建的中断处理函数已存在
 * @retval #LOS_OK                           中断创建成功
 * @par 依赖
 * <ul><li>los_hwi.h: 包含API声明的头文件</li></ul>
 * @see LOS_HwiDelete
 */
extern UINT32 LOS_HwiCreate(HWI_HANDLE_T hwiNum,
                            HWI_PRIOR_T hwiPrio,
                            HWI_MODE_T hwiMode,
                            HWI_PROC_FUNC hwiHandler,
                            HwiIrqParam *irqParam);

/**
 * @ingroup  los_hwi
 * @brief 删除硬件中断
 *
 * @par 描述
 * 此API用于删除已创建的硬件中断
 *
 * @attention
 * <ul>
 * <li>只有当中断裁剪配置项启用时，硬件中断模块才可用</li>
 * <li>硬件中断号取值范围: [OS_USER_HWI_MIN, OS_USER_HWI_MAX]</li>
 * <li>OS_HWI_MAX_NUM指定可创建的最大中断数</li>
 * <li>在平台上执行中断前，请参考该平台的芯片手册</li>
 * </ul>
 *
 * @param  hwiNum   [IN] Type #HWI_HANDLE_T: 硬件中断号
 * @param  irqParam [IN] Type #HwiIrqParam*: 删除硬件中断时使用的设备ID，用于共享中断场景
 *
 * @retval #OS_ERRNO_HWI_NUM_INVALID         无效的中断号
 * @retval #OS_ERRNO_HWI_SHARED_ERROR        无效的中断模式
 * @retval #LOS_OK                           中断删除成功
 * @retval #LOS_NOK                          根据pDev_ID删除中断失败
 * @par 依赖
 * <ul><li>los_hwi.h: 包含API声明的头文件</li></ul>
 * @see LOS_HwiCreate
 */
extern UINT32 LOS_HwiDelete(HWI_HANDLE_T hwiNum, HwiIrqParam *irqParam);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_HWI_H */
