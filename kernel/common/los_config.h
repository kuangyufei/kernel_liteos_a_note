/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2022 Huawei Device Co., Ltd. All rights reserved.
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
 * @defgroup los_config System configuration items
 */

#ifndef _LOS_CONFIG_H
#define _LOS_CONFIG_H

#include "los_tick.h"
#include "los_vm_zone.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

// ==============================================
// 内核内存布局符号声明（链接器定义的全局符号）
// ==============================================
/**
 * @ingroup los_config
 * @brief 内核中断栈起始地址
 * @details 由链接器脚本定义，标记中断处理栈的起始位置
 */
extern CHAR __int_stack_start;
/** @brief 只读数据段起始地址 */
extern CHAR __rodata_start;
/** @brief 只读数据段结束地址 */
extern CHAR __rodata_end;
/** @brief BSS段起始地址（未初始化全局变量） */
extern CHAR __bss_start;
/** @brief BSS段结束地址 */
extern CHAR __bss_end;
/** @brief 代码段起始地址 */
extern CHAR __text_start;
/** @brief 代码段结束地址 */
extern CHAR __text_end;
/** @brief 内存数据段起始地址（初始化全局变量） */
extern CHAR __ram_data_start;
/** @brief 内存数据段结束地址 */
extern CHAR __ram_data_end;
/** @brief 堆内存起始地址 */
extern UINT32 __heap_start;
/** @brief 堆内存结束地址 */
extern UINT32 __heap_end;

// ==============================================
// 系统时钟模块配置（内核时序基础参数）
// ==============================================
/**
 * @ingroup los_config
 * @brief 系统主时钟频率（单位：Hz）
 * @details 默认为总线时钟频率，可通过get_bus_clk()获取实际硬件时钟
 * @note 该值决定内核时间管理的基础频率，影响定时器精度和任务调度粒度
 */
#ifndef OS_SYS_CLOCK
#define OS_SYS_CLOCK (get_bus_clk())  /* 系统主时钟，由硬件层提供 */
#endif

/**
 * @ingroup los_config
 * @brief 定时器时钟频率（单位：Hz）
 * @details 默认为系统主时钟，可单独配置为不同频率源
 */
#ifndef OS_TIME_TIMER_CLOCK
#define OS_TIME_TIMER_CLOCK OS_SYS_CLOCK  /* 定时器时钟源，默认与系统时钟一致 */
#endif

/**
 * @ingroup los_config
 * @brief 函数地址搜索范围起始值
 * @details 用于符号解析时限定函数地址查找范围，防止越界访问
 */
#ifndef OS_SYS_FUNC_ADDR_START
#define OS_SYS_FUNC_ADDR_START ((UINTPTR)&__int_stack_start)  /* 从中断栈起始地址开始搜索 */
#endif

/**
 * @ingroup los_config
 * @brief 函数地址搜索范围结束值
 * @details 基于内核虚拟内存基地址和默认内存大小计算得出
 */
#ifndef OS_SYS_FUNC_ADDR_END
#define OS_SYS_FUNC_ADDR_END (KERNEL_VMM_BASE + SYS_MEM_SIZE_DEFAULT)  /* 内核内存空间结束地址 */
#endif

/**
 * @ingroup los_config
 * @brief 系统每秒Tick数
 * @details 定义系统时钟中断的频率，决定任务调度的时间精度
 * @note 1000表示1ms产生一次时钟中断，值越大调度精度越高但系统开销也越大
 */
#ifndef LOSCFG_BASE_CORE_TICK_PER_SECOND
#define LOSCFG_BASE_CORE_TICK_PER_SECOND  1000  /* 1ms per tick，系统默认1ms调度粒度 */
#endif

/**
 * @ingroup los_config
 * @brief 时钟中断最小响应误差（单位：Hz）
 * @details 定义Tick中断的最小精度误差范围
 */
#ifndef LOSCFG_BASE_CORE_TICK_PER_SECOND_MINI
#define LOSCFG_BASE_CORE_TICK_PER_SECOND_MINI  1000UL  /* 1ms，最小Tick精度 */
#endif

/* 时钟参数合法性检查：确保实际Tick频率不超过最小误差精度 */
#if (LOSCFG_BASE_CORE_TICK_PER_SECOND > LOSCFG_BASE_CORE_TICK_PER_SECOND_MINI)
    #error "LOSCFG_BASE_CORE_TICK_PER_SECOND_MINI must be greater than LOSCFG_BASE_CORE_TICK_PER_SECOND"
#endif

/* 时钟参数合法性检查：确保最小误差精度不超过1000Hz */
#if (LOSCFG_BASE_CORE_TICK_PER_SECOND_MINI > 1000UL)
    #error "LOSCFG_BASE_CORE_TICK_PER_SECOND_MINI must be less than or equal to 1000"
#endif

/**
 * @ingroup los_config
 * @brief 每秒时间调整微秒数
 * @details 用于系统时间微调，影响adjtime系统调用的调整幅度
 */
#ifndef LOSCFG_BASE_CORE_ADJ_PER_SECOND
#define LOSCFG_BASE_CORE_ADJ_PER_SECOND 500  /* 每秒最多调整500微秒 */
#endif

/**
 * @ingroup los_config
 * @brief 调度时钟间隔（单位：Tick）
 * @details 定义调度器检查时间的间隔，默认为每秒一次
 */
#define SCHED_CLOCK_INTETRVAL_TICKS  LOSCFG_BASE_CORE_TICK_PER_SECOND  /* 调度时钟间隔 = 1秒 */

/**
 * @ingroup los_config
 * @brief 定时器裁剪外部配置项
 * @details 当配置为0时取消硬件定时器功能
 */
#if defined(LOSCFG_BASE_CORE_TICK_HW_TIME) && (LOSCFG_BASE_CORE_TICK_HW_TIME == 0)
#undef LOSCFG_BASE_CORE_TICK_HW_TIME  /* 禁用硬件定时器 */
#endif

// ==============================================
// 硬件中断模块配置（中断控制器参数）
// ==============================================
/**
 * @ingroup los_config
 * @brief 硬件中断模块裁剪配置
 * @details 定义此宏以启用硬件中断功能，注释则禁用
 */
#ifndef LOSCFG_PLATFORM_HWI
#define LOSCFG_PLATFORM_HWI  /* 启用硬件中断模块 */
#endif

/**
 * @ingroup los_config
 * @brief 最大硬件中断数量
 * @details 包括Tick定时器中断在内的所有硬件中断总数上限
 * @note 实际支持的中断数量需与硬件中断控制器匹配
 */
#ifndef LOSCFG_PLATFORM_HWI_LIMIT
#define LOSCFG_PLATFORM_HWI_LIMIT 96  /* 支持最多96个硬件中断 */
#endif

/**
 * @ingroup los_config
 * @brief 中断抢占级别二进制点值
 * @details 决定最大中断抢占级别，取值范围[0,6]，对应抢占级别[128,64,32,16,8,4,2]
 * @note 仅在启用LOSCFG_ARCH_INTERRUPT_PREEMPTION时生效
 */
#ifdef LOSCFG_ARCH_INTERRUPT_PREEMPTION
#ifndef MAX_BINARY_POINT_VALUE
#define MAX_BINARY_POINT_VALUE  4  /* 二进制点值4对应抢占级别8 */
#endif
#endif

// ==============================================
// 任务模块配置（任务管理核心参数）
// ==============================================
/**
 * @ingroup los_config
 * @brief 最小任务栈大小
 * @details 不同架构下有不同的对齐要求：64位8字节对齐，32位4字节对齐
 * @note 该值为任务栈的最小安全容量，实际任务栈不应小于此值
 */
#ifndef LOS_TASK_MIN_STACK_SIZE
#ifdef __LP64__
#define LOS_TASK_MIN_STACK_SIZE (ALIGN(0x800, 8))  /* 64位系统：2048字节，8字节对齐 */
#else
#define LOS_TASK_MIN_STACK_SIZE (ALIGN(0x800, 4))  /* 32位系统：2048字节，4字节对齐 */
#endif
#endif

/**
 * @ingroup los_config
 * @brief 默认任务优先级
 * @details 未显式指定优先级时的任务默认优先级
 * @note 优先级数值越小表示优先级越高（0为最高优先级）
 */
#ifndef LOSCFG_BASE_CORE_TSK_DEFAULT_PRIO
#define LOSCFG_BASE_CORE_TSK_DEFAULT_PRIO 10  /* 默认优先级10（中等优先级） */
#endif

/**
 * @ingroup los_config
 * @brief 最大任务数量限制
 * @details 系统支持的最大任务数（不包括空闲任务）
 * @note 实际可用任务数可能受系统内存限制
 */
#ifndef LOSCFG_BASE_CORE_TSK_LIMIT
#define LOSCFG_BASE_CORE_TSK_LIMIT 128  /* 最多支持128个任务 */
#endif

/**
 * @ingroup los_config
 * @brief 最大进程数量限制
 * @details 系统支持的最大进程数
 * @attention 任务数必须大于等于进程数，否则会触发编译错误
 */
#ifndef LOSCFG_BASE_CORE_PROCESS_LIMIT
#define LOSCFG_BASE_CORE_PROCESS_LIMIT 64  /* 最多支持64个进程 */
#endif

/* 任务数与进程数关系检查：任务数必须大于等于进程数 */
#if (LOSCFG_BASE_CORE_TSK_LIMIT < LOSCFG_BASE_CORE_PROCESS_LIMIT)
#error "The number of tasks must be greater than or equal to the number of processes!"
#endif

/**
 * @ingroup los_config
 * @brief 空闲任务栈大小
 * @details 系统空闲任务的栈空间大小
 * @note 空闲任务在系统无其他可运行任务时执行
 */
#ifndef LOSCFG_BASE_CORE_TSK_IDLE_STACK_SIZE
#define LOSCFG_BASE_CORE_TSK_IDLE_STACK_SIZE SIZE(0x800)  /* 空闲任务栈大小2048字节 */
#endif

/**
 * @ingroup los_config
 * @brief 默认任务栈大小
 * @details 未显式指定栈大小时的任务默认栈空间
 */
#ifndef LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE
#define LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE SIZE(0x4000)  /* 默认任务栈大小16KB */
#endif

/**
 * @ingroup los_config
 * @brief 同优先级任务最长执行时间
 * @details 时间片轮转调度中，同优先级任务的最大连续运行时间
 * @note 单位为微秒，20000表示20ms
 */
#ifndef LOSCFG_BASE_CORE_TIMESLICE_TIMEOUT
#define LOSCFG_BASE_CORE_TIMESLICE_TIMEOUT  20000 /* 20ms，时间片长度 */
#endif

/**
 * @ingroup los_config
 * @brief 任务（栈）监控模块裁剪配置
 * @details 定义此宏以启用任务栈监控功能，用于检测栈溢出等异常
 */
#ifndef LOSCFG_BASE_CORE_TSK_MONITOR
#define LOSCFG_BASE_CORE_TSK_MONITOR  /* 启用任务栈监控 */
#endif

// ==============================================
// 信号量模块配置（IPC机制之一）
// ==============================================
/**
 * @ingroup los_config
 * @brief 信号量模块裁剪配置
 * @details 定义此宏以启用信号量功能，注释则禁用
 */
#ifndef LOSCFG_BASE_IPC_SEM
#define LOSCFG_BASE_IPC_SEM  /* 启用信号量模块 */
#endif

/**
 * @ingroup los_config
 * @brief 最大信号量数量
 * @details 系统支持的信号量总数上限
 */
#ifndef LOSCFG_BASE_IPC_SEM_LIMIT
#define LOSCFG_BASE_IPC_SEM_LIMIT 1024  /* 最多支持1024个信号量 */
#endif

/**
 * @ingroup los_config
 * @brief 信号量最大计数值
 * @details 二进制信号量通常为1，计数信号量可设置更大值
 * @note 最大值为0xFFFE（65534），保留0xFFFF作为特殊标记
 */
#ifndef OS_SEM_COUNT_MAX
#define OS_SEM_COUNT_MAX 0xFFFE  /* 信号量最大计数值 */
#endif

// ==============================================
// 互斥锁模块配置（IPC机制之二）
// ==============================================
/**
 * @ingroup los_config
 * @brief 互斥锁模块裁剪配置
 * @details 定义此宏以启用互斥锁功能，注释则禁用
 */
#ifndef LOSCFG_BASE_IPC_MUX
#define LOSCFG_BASE_IPC_MUX  /* 启用互斥锁模块 */
#endif

// ==============================================
// 读写锁模块配置（IPC机制之三）
// ==============================================
/**
 * @ingroup los_config
 * @brief 读写锁模块裁剪配置
 * @details 定义此宏以启用读写锁功能，注释则禁用
 */
#ifndef LOSCFG_BASE_IPC_RWLOCK
#define LOSCFG_BASE_IPC_RWLOCK  /* 启用读写锁模块 */
#endif

// ==============================================
// 队列模块配置（IPC机制之四）
// ==============================================
/**
 * @ingroup los_config
 * @brief 队列模块裁剪配置
 * @details 定义此宏以启用队列功能，注释则禁用
 * @note 队列是软件定时器的依赖模块，禁用队列将导致定时器不可用
 */
#ifndef LOSCFG_BASE_IPC_QUEUE
#define LOSCFG_BASE_IPC_QUEUE  /* 启用队列模块 */
#endif

/**
 * @ingroup los_config
 * @brief 最大队列数量
 * @details 系统支持的消息队列总数上限
 */
#ifndef LOSCFG_BASE_IPC_QUEUE_LIMIT
#define LOSCFG_BASE_IPC_QUEUE_LIMIT 1024  /* 最多支持1024个队列 */
#endif

// ==============================================
// 软件定时器模块配置（依赖队列模块）
// ==============================================
#ifdef LOSCFG_BASE_IPC_QUEUE  /* 仅当队列模块启用时才编译定时器代码 */

/**
 * @ingroup los_config
 * @brief 软件定时器模块裁剪配置
 * @details 定义此宏以启用软件定时器功能
 */
#ifndef LOSCFG_BASE_CORE_SWTMR_ENABLE
#define LOSCFG_BASE_CORE_SWTMR_ENABLE  /* 启用软件定时器 */
#endif

/**
 * @ingroup los_config
 * @brief 软件定时器使能标志
 * @details 1表示启用，0表示禁用
 */
#ifdef LOSCFG_BASE_CORE_SWTMR_ENABLE
#define LOSCFG_BASE_CORE_SWTMR 1  /* 软件定时器已启用 */
#else
#define LOSCFG_BASE_CORE_SWTMR 0  /* 软件定时器已禁用 */
#endif

/**
 * @ingroup los_config
 * @brief 最大软件定时器数量
 * @details 系统支持的软件定时器总数上限
 */
#ifndef LOSCFG_BASE_CORE_SWTMR_LIMIT
#define LOSCFG_BASE_CORE_SWTMR_LIMIT 1024  /* 最多支持1024个软件定时器 */
#endif

/**
 * @ingroup los_config
 * @brief 软件定时器ID最大值
 * @details 基于最大定时器数量计算得出的ID上限
 * @note 确保ID范围均匀分布以避免冲突
 */
#ifndef OS_SWTMR_MAX_TIMERID
#define OS_SWTMR_MAX_TIMERID ((0xFFFF / LOSCFG_BASE_CORE_SWTMR_LIMIT) * LOSCFG_BASE_CORE_SWTMR_LIMIT)
#endif

/**
 * @ingroup los_config
 * @brief 软件定时器队列最大长度
 * @details 存储定时器句柄的队列大小，应不小于定时器数量
 */
#ifndef OS_SWTMR_HANDLE_QUEUE_SIZE
#define OS_SWTMR_HANDLE_QUEUE_SIZE LOSCFG_BASE_CORE_SWTMR_LIMIT  /* 队列大小等于定时器数量 */
#endif

#endif  /* LOSCFG_BASE_IPC_QUEUE */

// ==============================================
// 内存模块配置（系统内存管理）
// ==============================================
/**
 * @ingroup los_config
 * @brief 系统内存起始地址
 * @details 默认为系统内存数组的起始地址
 */
#ifndef OS_SYS_MEM_ADDR
#define OS_SYS_MEM_ADDR                        (&m_aucSysMem1[0])  /* 系统内存基地址 */
#endif

/**
 * @ingroup los_config
 * @brief 系统内存大小
 * @details 通过计算函数地址结束值与BSS段结束地址的差值获得
 * @note 64字节对齐确保内存分配效率
 */
#ifndef OS_SYS_MEM_SIZE
#define OS_SYS_MEM_SIZE \
    ((OS_SYS_FUNC_ADDR_END) - (((UINTPTR)&__bss_end + (64 - 1)) & ~(64 - 1)))  /* 系统内存大小 */
#endif

// ==============================================
// SMP模块配置（多核处理支持）
// ==============================================
#ifdef LOSCFG_KERNEL_SMP  /* 对称多处理配置 */
#define LOSCFG_KERNEL_CORE_NUM                          LOSCFG_KERNEL_SMP_CORE_NUM  /* 多核数量由SMP配置决定 */
#else
#define LOSCFG_KERNEL_CORE_NUM                          1  /* 默认单核心配置 */
#endif

/**
 * @ingroup los_config
 * @brief CPU核心掩码
 * @details 基于核心数量生成的位掩码，用于表示CPU亲和性
 * @example 4核系统掩码为0b1111（0x0F）
 */
#define LOSCFG_KERNEL_CPU_MASK                          ((1 << LOSCFG_KERNEL_CORE_NUM) - 1)  /* CPU亲和性掩码 */
// ==============================================
// 内核版本信息模块（系统标识与版本管理）
// ==============================================
/**
 * @ingroup los_config
 * @brief 字符串化宏定义
 * @details 将输入参数转换为字符串字面量，用于版本信息拼接
 */
#define _T(x)                                   x

/** @brief 内核名称定义 */
#define KERNEL_NAME                            "Huawei LiteOS"

/** @brief 内核节点名称 */
#define KERNEL_NODE_NAME                       "hisilicon"

/** @brief 字符串分隔符 */
#define KERNEL_SEP                             " "

/**
 * @brief 内核版本字符串拼接宏
 * @details 组合内核名称与版本号形成完整版本字符串
 * @param v 版本号字符串
 * @return 格式化的版本字符串（如"Huawei LiteOS V2.0.0.37"）
 */
#define _V(v)                                  _T(KERNEL_NAME)_T(KERNEL_SEP)_T(v)

/**
 * @ingroup los_config
 * @brief 公共版本号定义
 * @details 遵循语义化版本规范(MAJOR.MINOR.PATCH.ITERATION)
 */
#define KERNEL_MAJOR                     2       /* 主版本号：重大功能更新 */
#define KERNEL_MINOR                     0       /* 次版本号：功能增强 */
#define KERNEL_PATCH                     0       /* 修订号：问题修复 */
#define KERNEL_ITRE                      37      /* 迭代号：开发迭代次数 */

/**
 * @brief 版本号编码宏
 * @details 将四个版本数字编码为32位整数，便于版本比较
 * @param a 主版本号
 * @param b 次版本号
 * @param c 修订号
 * @param d 迭代号
 * @return 32位版本编码（格式：0xAABBCCDD）
 */
#define VERSION_NUM(a, b, c, d) (((a) << 24) | ((b) << 16) | (c) << 8 | (d))

/** @brief 内核开放版本号（编码后） */
#define KERNEL_OPEN_VERSION_NUM VERSION_NUM(KERNEL_MAJOR, KERNEL_MINOR, KERNEL_PATCH, KERNEL_ITRE)

// ==============================================
// 容器模块配置（资源隔离与限制）
// ==============================================
/**
 * @ingroup los_config
 * @brief 默认容器数量限制
 * @details 系统支持的最大容器实例数量
 * @note 容器用于实现资源隔离，每个容器拥有独立的进程空间
 */
#ifndef LOSCFG_KERNEL_CONTAINER_DEFAULT_LIMIT
#define LOSCFG_KERNEL_CONTAINER_DEFAULT_LIMIT  10  /* 默认支持10个容器 */
#endif

// ==============================================
// 异常信息配置（系统故障诊断）
// ==============================================
#ifdef LOSCFG_SAVE_EXCINFO  /* 条件编译：启用异常信息保存功能 */

/**
 * @ingroup los_config
 * @brief 异常信息记录缓冲区大小
 * @details 用于存储系统崩溃时的异常上下文信息
 * @note 16KB缓冲区可存储约4次完整异常信息（取决于架构）
 */
#define EXCINFO_RECORD_BUF_SIZE (16 * 1024)  /* 异常信息缓冲区大小：16KB */

/**
 * @ingroup los_config
 * @brief 异常信息记录地址
 * @attention
 * <ul>
 * <li>地址必须指向有效Flash区域，且不与其他存储区域重叠</li>
 * <li>默认值0xffffffff表示未配置，需用户根据硬件实际情况设置</li>
 * </ul>
 */
#define EXCINFO_RECORD_ADDR (0xffffffff)  /* 异常信息存储地址（未配置） */

/**
 * @ingroup los_config
 * @brief 异常信息读写函数类型定义
 * @details 定义用于读写异常信息的函数指针类型
 * @param startAddr [IN] 存储起始地址（必须为预留的异常信息空间）
 * @param space     [IN] 存储空间大小（必须与buf大小一致）
 * @param rwFlag    [IN] 读写标志：0=写入，1=读取，其他值=无操作
 * @param buf       [IN] 数据缓冲区（由用户代码分配和释放）
 * @retval 无返回值
 * @par 依赖
 * <ul><li>los_config.h: 包含此类型定义的头文件</li></ul>
 */
typedef VOID (*log_read_write_fn)(UINT32 startAddr, UINT32 space, UINT32 rwFlag, CHAR *buf);

/**
 * @ingroup los_config
 * @brief 注册异常信息记录函数
 * @details 用于设置异常信息的存储位置、大小及读写函数
 * @attention
 * <ul>
 * <li>startAddr必须指向预留的异常信息存储空间</li>
 * <li>buf大小必须等于space，且由用户代码负责内存管理</li>
 * </ul>
 * @param startAddr [IN] 存储起始地址（必须为有效Flash地址）
 * @param space     [IN] 存储空间大小（单位：字节）
 * @param buf       [IN] 异常信息缓冲区（用户管理的内存）
 * @param hook      [IN] 异常信息读写函数指针
 * @retval 无返回值
 * @par 依赖
 * <ul><li>los_config.h: 包含此API声明的头文件</li></ul>
 */
VOID LOS_ExcInfoRegHook(UINT32 startAddr, UINT32 space, CHAR *buf, log_read_write_fn hook);

#endif  /* LOSCFG_SAVE_EXCINFO */

// ==============================================
// 系统核心函数声明（启动与重启）
// ==============================================
/**
 * @brief 内核主入口函数声明
 * @details 系统启动后执行的第一个内核函数，负责初始化核心服务
 * @return UINT32 初始化结果
 *         - LOS_OK: 初始化成功
 *         - 其他值: 初始化失败
 */
extern UINT32 OsMain(VOID);

/**
 * @brief 系统重启钩子函数类型
 * @details 定义重启钩子函数的函数指针类型，无参数无返回值
 * @note 用于注册自定义重启前处理函数
 */
typedef VOID (*SystemRebootFunc)(VOID);

/**
 * @brief 设置系统重启钩子函数
 * @details 注册自定义重启处理函数，系统重启前会调用此函数
 * @param func [IN] 重启钩子函数指针，NULL表示取消注册
 * @retval 无返回值
 * @see SystemRebootFunc
 */
VOID OsSetRebootHook(SystemRebootFunc func);

/**
 * @brief 获取当前注册的重启钩子函数
 * @details 返回当前设置的重启钩子函数指针
 * @return SystemRebootFunc 当前注册的钩子函数，NULL表示未注册
 * @see SystemRebootFunc
 */
SystemRebootFunc OsGetRebootHook(VOID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_CONFIG_H */
