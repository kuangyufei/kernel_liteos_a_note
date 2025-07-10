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
 * @defgroup los_typedef Type define
 * @ingroup kernel
 */

#ifndef _LOS_TYPEDEF_H
#define _LOS_TYPEDEF_H
#include "stddef.h"
#include "stdbool.h"
#include "stdint.h"
#include "los_builddef.h"
#include "los_toolchain.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_typedef
 * @brief 将宏参数转换为字符串字面量
 * @param x 待转换的宏参数
 * @return 转换后的字符串字面量
 * @par 示例
 * @code
 * OS_STRING(123) → "123"
 * @endcode
 */
#define OS_STRING(x)  #x

/**
 * @ingroup los_typedef
 * @brief 双重字符串化宏，用于处理宏参数的间接展开
 * @param x 待转换的宏参数（可包含其他宏）
 * @return 完全展开后的字符串字面量
 * @note 需配合OS_STRING使用，解决宏参数直接字符串化无法展开的问题
 * @par 示例
 * @code
 * #define VERSION 100
 * X_STRING(VERSION) → "100"  // 而非"VERSION"
 * @endcode
 */
#define X_STRING(x) OS_STRING(x)

/* 基础数据类型定义 */
typedef unsigned char      UINT8;   /**< 无符号8位整数 (0-255) */
typedef unsigned short     UINT16;  /**< 无符号16位整数 (0-65535) */
typedef unsigned int       UINT32;  /**< 无符号32位整数 (0-4294967295) */
typedef signed char        INT8;    /**< 有符号8位整数 (-128-127) */
typedef signed short       INT16;   /**< 有符号16位整数 (-32768-32767) */
typedef signed int         INT32;   /**< 有符号32位整数 (-2147483648-2147483647) */
typedef float              FLOAT;   /**< 单精度浮点数 (32位) */
typedef double             DOUBLE;  /**< 双精度浮点数 (64位) */
typedef char               CHAR;    /**< 字符类型 (8位) */

/**
 * @brief 64位系统特定类型定义
 * @note 当编译器定义__LP64__时启用，通常对应64位架构
 */
#ifdef __LP64__
typedef long unsigned int  UINT64;   /**< 无符号64位整数 (0-18446744073709551615) */
typedef long signed int    INT64;    /**< 有符号64位整数 (-9223372036854775808-9223372036854775807) */
typedef unsigned long      UINTPTR;  /**< 无符号指针类型 (64位) */
typedef signed long        INTPTR;   /**< 有符号指针类型 (64位) */
#else
/**
 * @brief 32位系统特定类型定义
 * @note 当编译器未定义__LP64__时启用，通常对应32位架构
 */
typedef unsigned long long UINT64;   /**< 无符号64位整数 (0-18446744073709551615) */
typedef signed long long   INT64;    /**< 有符号64位整数 (-9223372036854775808-9223372036854775807) */
typedef unsigned int       UINTPTR;  /**< 无符号指针类型 (32位) */
typedef signed int         INTPTR;   /**< 有符号指针类型 (32位) */
#endif

#ifdef __LP64__
typedef __uint128_t        UINT128;  /**< 无符号128位整数类型 */
typedef INT64              ssize_t;  /**< 带符号大小类型 (64位) */
typedef UINT64             size_t;   /**< 无符号大小类型 (64位) */
#define LOSCFG_AARCH64               /**< 定义AArch64架构标识 */
#else
typedef INT32              ssize_t;  /**< 带符号大小类型 (32位) */
typedef UINT32             size_t;   /**< 无符号大小类型 (32位) */
#endif

typedef UINTPTR            AARCHPTR;  /**< 架构相关指针类型，根据系统位数自动适配 */
typedef size_t             BOOL;      /**< 布尔类型，非0表示真，0表示假 */

#define VOID               void       /**< 无类型关键字重定义 */
#define STATIC             static     /**< 静态存储类关键字重定义 */

#ifndef FALSE
#define FALSE              0U         /**< 布尔假值 (无符号0) */
#endif

#ifndef TRUE
#define TRUE               1U         /**< 布尔真值 (无符号1) */
#endif

#ifndef NULL
#define NULL               ((VOID *)0) /**< 空指针常量 */
#endif

#define OS_NULL_BYTE       ((UINT8)0xFF)    /**< 8位无符号空值 (十进制255) */
#define OS_NULL_SHORT      ((UINT16)0xFFFF) /**< 16位无符号空值 (十进制65535) */
#define OS_NULL_INT        ((UINT32)0xFFFFFFFF) /**< 32位无符号空值 (十进制4294967295) */

#ifndef USER
#define USER                            /**< 用户模式标识宏（预留） */
#endif

/** @name 通用状态码 */
/** @{ */
#ifndef LOS_OK
#define LOS_OK             0           /**< 操作成功 */
#endif

#ifndef LOS_NOK
#define LOS_NOK            1           /**< 操作失败（通用） */
#endif
/** @} */

/** @name POSIX兼容错误码 */
/** @{ */
#ifndef LOS_EPERM
#define LOS_EPERM          1           /**< 操作不允许 (Permission denied) */
#endif

#ifndef LOS_ESRCH
#define LOS_ESRCH          3           /**< 未找到进程/线程 (No such process) */
#endif

#ifndef LOS_EINTR
#define LOS_EINTR          4           /**< 系统调用被中断 (Interrupted system call) */
#endif

#ifndef LOS_EBADF
#define LOS_EBADF          9           /**< 无效文件描述符 (Bad file descriptor) */
#endif

#ifndef LOS_ECHILD
#define LOS_ECHILD         10          /**< 无子进程 (No child processes) */
#endif

#ifndef LOS_EAGAIN
#define LOS_EAGAIN         11          /**< 资源暂时不可用 (Try again) */
#endif

#ifndef LOS_ENOMEM
#define LOS_ENOMEM         12          /**< 内存不足 (Out of memory) */
#endif

#ifndef LOS_EACCES
#define LOS_EACCES         13          /**< 权限被拒绝 (Permission denied) */
#endif

#ifndef LOS_EFAULT
#define LOS_EFAULT         14          /**< 地址错误 (Bad address) */
#endif

#ifndef LOS_EBUSY
#define LOS_EBUSY          16          /**< 资源忙 (Device or resource busy) */
#endif

#ifndef LOS_EINVAL
#define LOS_EINVAL         22          /**< 参数无效 (Invalid argument) */
#endif

#ifndef LOS_EDEADLK
#define LOS_EDEADLK        35          /**< 可能导致死锁 (Resource deadlock would occur) */
#endif

#ifndef LOS_EOPNOTSUPP
#define LOS_EOPNOTSUPP     95          /**< 操作不支持 (Operation not supported) */
#endif

#ifndef LOS_ETIMEDOUT
#define LOS_ETIMEDOUT      110         /**< 操作超时 (Connection timed out) */
#endif
/** @} */
/** @name 通用状态码与错误值 */
/** @{ */
#define OS_FAIL            1           /**< 操作失败（通用状态码） */
#define OS_ERROR           (UINT32)(-1) /**< 错误状态值 (十进制4294967295) */
#define OS_INVALID         (UINT32)(-1) /**< 无效值标识 (与OS_ERROR等价) */
#define OS_INVALID_VALUE   ((UINT32)0xFFFFFFFF) /**< 32位无效值 (十六进制0xFFFFFFFF，十进制4294967295) */
#define OS_64BIT_MAX       0xFFFFFFFFFFFFFFFFULL /**< 64位无符号最大值 (十进制18446744073709551615) */
/** @} */

/** @name 编译器关键字适配 */
/** @{ */
#define asm __asm                         /**< 汇编关键字重定义，确保跨编译器兼容 */
#ifdef typeof
#undef typeof                            /**< 移除可能存在的typeof宏定义 */
#endif
#define typeof __typeof__                 /**< 类型查询关键字，兼容GCC的__typeof__ */
/** @} */

/**
 * @ingroup los_typedef
 * @brief 标签定义宏
 * @param label 标签名称
 * @return 未修改的标签名称
 * @note 用于统一标签定义风格，便于后续扩展或条件编译处理
 */
#ifndef LOS_LABEL_DEFN
#define LOS_LABEL_DEFN(label) label
#endif

/** @name 内存对齐配置 */
/** @{ */
#ifndef LOSARC_ALIGNMENT
#define LOSARC_ALIGNMENT 8                /**< 默认内存对齐字节数 (8字节对齐) */
#endif

/* 对齐字节数对应的2的幂次 */
#ifndef LOSARC_P2ALIGNMENT
#ifdef LOSCFG_AARCH64
#define LOSARC_P2ALIGNMENT 3              /**< 64位架构：8字节对齐 = 2^3 */
#else
#define LOSARC_P2ALIGNMENT 2              /**< 32位架构：4字节对齐 = 2^2 */
#endif
#endif
/** @} */

/** @name 系统架构相关类型 */
/** @{ */
typedef int status_t;                     /**< 状态码类型 */
typedef unsigned long vaddr_t;            /**< 虚拟地址类型 */
typedef unsigned long PADDR_T;            /**< 物理地址类型 (大写命名风格) */
typedef unsigned long VADDR_T;            /**< 虚拟地址类型 (大写命名风格) */
typedef unsigned long paddr_t;            /**< 物理地址类型 (小写命名风格) */
typedef unsigned long DMA_ADDR_T;         /**< DMA地址类型 */
typedef unsigned long ADDR_T;             /**< 通用地址类型 */
typedef unsigned long VM_OFFSET_T;        /**< 虚拟内存偏移量类型 */
typedef unsigned long PTE_T;              /**< 页表项类型 */
typedef unsigned int  ULONG_T;            /**< 无符号长整数类型 (32位) */
typedef int STATUS_T;                     /**< 状态码类型 (大写命名风格) */
/** @} */

/** @name 编译器属性宏 */
/** @{ */
/**
 * @brief 为类型或变量指定显式内存对齐属性
 * @param __align__ 对齐字节数，必须是2的幂次
 * @note 编译时生效，用于优化内存访问效率
 * @par 示例
 * @code
 * LOSBLD_ATTRIB_ALIGN(16) int buffer[4]; // 强制16字节对齐
 * @endcode
 */
#if !defined(LOSBLD_ATTRIB_ALIGN)
#define LOSBLD_ATTRIB_ALIGN(__align__) __attribute__((aligned(__align__)))
#endif

/**
 * @brief 将变量或函数分配到指定的链接段
 * @param __sect__ 段名称字符串
 * @note 用于控制链接脚本中的内存布局，如将关键数据放入特定RAM区域
 * @par 示例
 * @code
 * LOSBLD_ATTRIB_SECTION(".rodata.special") const char g_version[] = "V1.0";
 * @endcode
 */
#if !defined(LOSBLD_ATTRIB_SECTION)
#define LOSBLD_ATTRIB_SECTION(__sect__) __attribute__((section(__sect__)))
#endif

/**
 * @brief 告知编译器保留未使用的变量或函数
 * @note 防止编译器优化删除看似未使用但实际需要的符号（如中断处理函数）
 * @attention 仅GCC 3.3.2及以上版本支持，低版本编译器可能忽略此属性
 */
#define LOSBLD_ATTRIB_USED __attribute__((used))
/** @} */

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_TYPEDEF_H */
