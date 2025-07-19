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

#ifndef _LOS_INIT_INFO_H
#define _LOS_INIT_INFO_H

#include "stdalign.h"
#include "los_toolchain.h"
#include "los_typedef.h"
/**
 * @file los_init_info.h
 * @brief 内核初始化框架核心头文件，定义初始化级别、钩子注册和模块信息结构
 */

/**
 * @ingroup los_init_info
 * @def LOS_INIT_DEBUG
 * @brief 初始化框架调试宏开关
 * @details 定义此宏将启用初始化框架的调试功能，包括模块初始化耗时统计和详细日志输出
 * @note 需在此处定义，与LOS_INIT_STATISTICS互斥
 * @par 示例
 * @code
 * #define LOS_INIT_DEBUG  // 启用初始化调试功能
 * @endcode
 */
/**
 * @ingroup los_init_info
 * Macro LOS_INIT_DEBUG needs to be defined here, used to debug the init framework.
 * #define LOS_INIT_DEBUG
 */

/**
 * @ingroup los_init_info
 * @def LOS_INIT_STATISTICS
 * @brief 内核启动时间统计宏开关
 * @details 定义此宏将启用内核启动时间统计功能，精确测量各阶段初始化耗时
 * @attention
 * <ul>
 * <li>使用此宏时必须确保LOS_INIT_DEBUG未定义，二者不可同时启用</li>
 * <li>统计结果可用于系统启动性能优化和瓶颈分析</li>
 * </ul>
 * @note 需在此处定义，与LOS_INIT_DEBUG互斥
 * @par 示例
 * @code
 * #define LOS_INIT_STATISTICS  // 启用启动时间统计功能
 * @endcode
 */
/**
 * @ingroup los_init_info
 * Macro LOS_INIT_STATISTICS needs to be defined here, used to count the kernel startup time.
 * @attention
 * <ul>
 * <li> if uses, the macro LOS_INIT_DEBUG must be undefined. </li>
 * </ul>
 * #define LOS_INIT_STATISTICS
 */

/**
 * @brief 初始化调试与统计宏互斥检查
 * @details 确保LOS_INIT_STATISTICS和LOS_INIT_DEBUG不会同时定义，避免功能冲突
 * @note 编译时断言，若两个宏同时定义将产生编译错误
 */
#if defined(LOS_INIT_STATISTICS) && defined(LOS_INIT_DEBUG)
#error "LOS_INIT_STATISTICS needs LOS_INIT_DEBUG to be undefined"
#endif

/**
 * @ingroup los_init_info
 * @def INIT_SECTION(_type, _level, _hook)
 * @brief 将初始化函数放入指定的链接段
 * @details 使用GCC属性将初始化钩子函数放置到特定的只读数据段，实现按类型和级别组织初始化代码
 * @param[in] _type 启动框架类型名称，用于区分不同的初始化框架
 * @param[in] _level 初始化级别，决定执行顺序，数值越小越早执行
 * @param[in] _hook 初始化钩子函数名，作为段名的一部分确保唯一性
 * @note 链接器脚本(.ld文件)需包含对应段定义才能正常工作
 * @par 示例
 * @code
 * INIT_SECTION(kernel, 0, OsKernelInit)  // 将OsKernelInit放入内核初始化0级段
 * @endcode
 */
#define INIT_SECTION(_type, _level, _hook)          __attribute__((section(".rodata.init."#_type"."#_level"."#_hook)))

/**
 * @ingroup los_init_info
 * @def INIT_ALIGN
 * @brief 初始化结构对齐宏
 * @details 确保ModuleInitInfo结构体按其自身大小对齐，提高访问效率
 * @note 使用alignof运算符动态获取对齐要求，保证跨平台兼容性
 */
#define INIT_ALIGN                                  __attribute__((aligned(alignof(struct ModuleInitInfo))))

/**
 * @ingroup los_init_info
 * @typedef OsInitHook
 * @brief 初始化钩子函数指针类型
 * @details 定义所有初始化钩子函数的统一接口类型
 * @return UINT32 初始化结果
 * @retval LOS_OK 初始化成功
 * @retval 其他值 初始化失败，具体错误码见错误码定义
 */
typedef UINT32 (*OsInitHook)(VOID);

/**
 * @ingroup los_init_info
 * @struct ModuleInitInfo
 * @brief 模块初始化信息结构体
 * @details 存储模块初始化钩子函数及调试名称，是初始化框架的核心数据结构
 * @note 此结构体在只读数据段分配，确保初始化过程中不会被意外修改
 */
struct ModuleInitInfo {
    OsInitHook hook;          /**< 初始化钩子函数指针 */
#ifdef LOS_INIT_DEBUG
    const CHAR *name;         /**< 模块名称，仅调试模式可见 */
#endif
};

/**
 * @ingroup los_init_info
 * @def SET_MODULE_NAME(_hook)
 * @brief 条件设置模块名称
 * @details 根据是否启用调试模式，条件性地设置模块名称字段
 * @param[in] _hook 初始化钩子函数名，将作为模块名称字符串
 * @note 非调试模式下此宏为空操作，节省存储空间
 */
#ifdef LOS_INIT_DEBUG
#define SET_MODULE_NAME(_hook)                      .name = #_hook,
#else
#define SET_MODULE_NAME(_hook)
#endif

/**
* @ingroup  los_init_info
* @brief 向启动框架的指定级别添加注册模块
* @par 描述
* 此API用于向启动框架的指定级别添加注册模块，实现模块化初始化
* @attention
 * <ul>
 * <li>不建议直接调用，推荐每个启动框架在los_init.h中封装一层接口</li>
 * <li>注册的钩子函数将按级别顺序执行，同一级别内的执行顺序取决于链接顺序</li>
 * </ul>
* @param  _type     [IN] 启动框架类型名称，用于区分不同框架
* @param  _hook     [IN] 注册函数，符合OsInitHook函数指针类型
* @param  _level    [IN] 注册级别，范围[0, LOS_INIT_LEVEL_MAX]，决定执行顺序
* @retval None
* @par 依赖
* <ul><li>los_init_info.h: 包含此API声明的头文件</li></ul>
* @par 示例
* @code
* OS_INIT_HOOK_REG(kernel, OsTaskInit, 1)  // 在内核框架1级注册任务初始化钩子
* @endcode
*/
#define OS_INIT_HOOK_REG(_type, _hook, _level)                              \
    STATIC const struct ModuleInitInfo ModuleInitInfo_##_hook               \
    USED INIT_SECTION(_type, _level, _hook) INIT_ALIGN = {                  \
        .hook = (UINT32 (*)(VOID))&_hook,                                   \
        SET_MODULE_NAME(_hook)                                              \
    };

/**
 * @ingroup los_init_info
 * @def EXTERN_LABEL(_type, _level)
 * @brief 声明指定类型和级别的初始化标签
 * @details 外部声明特定类型和级别的初始化段标签，用于链接器定位
 * @param[in] _type 启动框架类型名称
 * @param[in] _level 初始化级别
 * @note 生成形如__kernel_init_level_0的外部标签声明
 */
#define EXTERN_LABEL(_type, _level)                 extern struct ModuleInitInfo __##_type##_init_level_##_level;

/**
 * @ingroup los_init_info
 * @def GET_LABEL(_type, _level)
 * @brief 获取指定类型和级别的初始化标签地址
 * @details 返回特定类型和级别的初始化段标签地址，用于构建级别列表
 * @param[in] _type 启动框架类型名称
 * @param[in] _level 初始化级别
 * @return struct ModuleInitInfo* 指向该级别初始化段的指针
 * @note 生成形如&__kernel_init_level_0的地址表达式
 */
#define GET_LABEL(_type, _level)                    &__##_type##_init_level_##_level,

/**
 * @ingroup los_init_info
 * @def INIT_LABEL_REG_0(_op, _type)
 * @brief 注册0级初始化标签
 * @details 对0级初始化标签执行指定操作(_op)
 * @param[in] _op 要执行的操作宏(如EXTERN_LABEL或GET_LABEL)
 * @param[in] _type 启动框架类型名称
 */
#define INIT_LABEL_REG_0(_op, _type)                \
    _op(_type, 0)

/**
 * @ingroup los_init_info
 * @def INIT_LABEL_REG_1(_op, _type)
 * @brief 注册0-1级初始化标签
 * @details 递归调用INIT_LABEL_REG_0并添加1级初始化标签操作
 * @param[in] _op 要执行的操作宏
 * @param[in] _type 启动框架类型名称
 */
#define INIT_LABEL_REG_1(_op, _type)                \
    INIT_LABEL_REG_0(_op, _type)                    \
    _op(_type, 1)

/**
 * @ingroup los_init_info
 * @def INIT_LABEL_REG_2(_op, _type)
 * @brief 注册0-2级初始化标签
 * @details 递归调用INIT_LABEL_REG_1并添加2级初始化标签操作
 * @param[in] _op 要执行的操作宏
 * @param[in] _type 启动框架类型名称
 */
#define INIT_LABEL_REG_2(_op, _type)                \
    INIT_LABEL_REG_1(_op, _type)                    \
    _op(_type, 2)

/**
 * @ingroup los_init_info
 * @def INIT_LABEL_REG_3(_op, _type)
 * @brief 注册0-3级初始化标签
 * @details 递归调用INIT_LABEL_REG_2并添加3级初始化标签操作
 * @param[in] _op 要执行的操作宏
 * @param[in] _type 启动框架类型名称
 */
#define INIT_LABEL_REG_3(_op, _type)                \
    INIT_LABEL_REG_2(_op, _type)                    \
    _op(_type, 3)

/**
 * @ingroup los_init_info
 * @def INIT_LABEL_REG_4(_op, _type)
 * @brief 注册0-4级初始化标签
 * @details 递归调用INIT_LABEL_REG_3并添加4级初始化标签操作
 * @param[in] _op 要执行的操作宏
 * @param[in] _type 启动框架类型名称
 */
#define INIT_LABEL_REG_4(_op, _type)                \
    INIT_LABEL_REG_3(_op, _type)                    \
    _op(_type, 4)

/**
 * @ingroup los_init_info
 * @def INIT_LABEL_REG_5(_op, _type)
 * @brief 注册0-5级初始化标签
 * @details 递归调用INIT_LABEL_REG_4并添加5级初始化标签操作
 * @param[in] _op 要执行的操作宏
 * @param[in] _type 启动框架类型名称
 */
#define INIT_LABEL_REG_5(_op, _type)                \
    INIT_LABEL_REG_4(_op, _type)                    \
    _op(_type, 5)

/**
 * @ingroup los_init_info
 * @def INIT_LABEL_REG_6(_op, _type)
 * @brief 注册0-6级初始化标签
 * @details 递归调用INIT_LABEL_REG_5并添加6级初始化标签操作
 * @param[in] _op 要执行的操作宏
 * @param[in] _type 启动框架类型名称
 */
#define INIT_LABEL_REG_6(_op, _type)                \
    INIT_LABEL_REG_5(_op, _type)                    \
    _op(_type, 6)

/**
 * @ingroup los_init_info
 * @def INIT_LABEL_REG_7(_op, _type)
 * @brief 注册0-7级初始化标签
 * @details 递归调用INIT_LABEL_REG_6并添加7级初始化标签操作
 * @param[in] _op 要执行的操作宏
 * @param[in] _type 启动框架类型名称
 */
#define INIT_LABEL_REG_7(_op, _type)                \
    INIT_LABEL_REG_6(_op, _type)                    \
    _op(_type, 7)

/**
 * @ingroup los_init_info
 * @def INIT_LABEL_REG_8(_op, _type)
 * @brief 注册0-8级初始化标签
 * @details 递归调用INIT_LABEL_REG_7并添加8级初始化标签操作
 * @param[in] _op 要执行的操作宏
 * @param[in] _type 启动框架类型名称
 */
#define INIT_LABEL_REG_8(_op, _type)                \
    INIT_LABEL_REG_7(_op, _type)                    \
    _op(_type, 8)

/**
 * @ingroup los_init_info
 * @def INIT_LABEL_REG_9(_op, _type)
 * @brief 注册0-9级初始化标签
 * @details 递归调用INIT_LABEL_REG_8并添加9级初始化标签操作
 * @param[in] _op 要执行的操作宏
 * @param[in] _type 启动框架类型名称
 */
#define INIT_LABEL_REG_9(_op, _type)                \
    INIT_LABEL_REG_8(_op, _type)                    \
    _op(_type, 9)

/**
 * @ingroup los_init_info
 * @def INIT_LABEL_REG_10(_op, _type)
 * @brief 注册0-10级初始化标签
 * @details 递归调用INIT_LABEL_REG_9并添加10级初始化标签操作
 * @param[in] _op 要执行的操作宏
 * @param[in] _type 启动框架类型名称
 */
#define INIT_LABEL_REG_10(_op, _type)               \
    INIT_LABEL_REG_9(_op, _type)                    \
    _op(_type, 10)

/**
* @ingroup  los_init_info
* @brief 定义一组级别并初始化每个级别的标签
* @par 描述
* 此API用于定义一组初始化级别并初始化每个级别的标签，构建完整的初始化级别管理体系
* @attention
 * <ul>
 * <li>此接口用于添加新的启动框架</li>
 * <li>使用此接口需在链接器脚本(liteos.ld和liteos_llvm.ld)中添加对应段描述</li>
 * <li>级别范围为[0, _num]，共_num+1个级别</li>
 * </ul>
* @param  _type [IN] 启动框架类型名称，如"kernel"、"fs"等
* @param  _num  [IN] 启动框架的最大有效级别，级别范围是[0, _num]
* @param  _list [IN] 静态全局数组，用于管理所有级别的标签
* @retval None
* @par 依赖
* <ul><li>los_init_info.h: 包含此API声明的头文件</li></ul>
* @par 示例
* @code
* OS_INIT_LEVEL_REG(kernel, 10, g_kernInitLevelList)  // 定义内核启动框架的0-10级
* @endcode
*/
#define OS_INIT_LEVEL_REG(_type, _num, _list)       \
    INIT_LABEL_REG_##_num(EXTERN_LABEL, _type)      \
    STATIC struct ModuleInitInfo *_list[] = {       \
        INIT_LABEL_REG_##_num(GET_LABEL, _type)     \
    }

#endif /* _LOS_INIT_INFO_H */
