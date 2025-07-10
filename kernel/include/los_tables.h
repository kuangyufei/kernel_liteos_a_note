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

#ifndef _LOS_TABLES_H
#define _LOS_TABLES_H

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/*
HAL层主要功能是实现轻OpenHarmony与芯片的解耦，
*/

/**
 * @ingroup los_hal
 * @brief HAL表起始标记定义宏
 *
 * 该宏用于定义HAL(硬件抽象层)表的起始位置，通过内联汇编创建特定的数据段来存储表结构
 *
 * @param label 表的标签名称，用于标识表的起始位置
 * @param name 表的名称，用于生成唯一的段名
 *
 * @note 此宏必须与LOS_HAL_TABLE_END宏配合使用，以定义完整的表结构
 * @par 汇编指令说明
 * - .section：创建名为".liteos.table.XXX.begin"的专用段，"aw"表示可分配可写
 * - .globl：声明标签为全局可见，以便链接器识别
 * - .type：指定标签类型为对象
 * - .p2align：按LOSARC_P2ALIGNMENT指定的2的幂次对齐(通常为4字节或8字节)
 * - .previous：返回到之前的段，避免影响后续代码编译
    如: LOS_HAL_TABLE_BEGIN(g_shellcmd, shellcmd);
    扩展后如下:
    .section ".liteos.table.shellcmd.begin","aw"
            .globl g_shellcmd
            .type  g_shellcmd, object
            .p2align 3
    g_shellcmd :  
        .previous 
 */
#ifndef LOS_HAL_TABLE_BEGIN
#define LOS_HAL_TABLE_BEGIN(label, name)                                     \
    __asm__(".section \".liteos.table." X_STRING(name) \".begin\",\"aw\"\n"   \
            ".globl " X_STRING(LOS_LABEL_DEFN(label)) "\n"                   \
            ".type    " X_STRING(LOS_LABEL_DEFN(label)) ",object\n"          \
            ".p2align " X_STRING(LOSARC_P2ALIGNMENT) "\n"                    \
            X_STRING(LOS_LABEL_DEFN(label)) ":\n"                            \
            ".previous\n"                                                    \
            )
#endif

/**
 * @ingroup los_hal
 * @brief HAL表结束标记定义宏
 *
 * 该宏用于定义HAL(硬件抽象层)表的结束位置，与LOS_HAL_TABLE_BEGIN宏配合使用，形成完整的表结构
 * 通过内联汇编创建特定的数据段来存储表的结束标记
 *
 * @param label 表的标签名称，用于标识表的结束位置
 * @param name 表的名称，用于生成唯一的段名，需与BEGIN宏的name参数保持一致
 *
 * @note 此宏必须与LOS_HAL_TABLE_BEGIN宏配对使用，且两个宏的name参数必须相同
 * @par 汇编指令说明
 * - .section：创建名为".liteos.table.XXX.finish"的专用段，"aw"表示可分配可写
 * - .globl：声明标签为全局可见，以便链接器识别
 * - .type：指定标签类型为对象
 * - .p2align：按LOSARC_P2ALIGNMENT指定的2的幂次对齐(通常为4字节或8字节)
 * - .previous：返回到之前的段，避免影响后续代码编译
    如: LOS_HAL_TABLE_END(g_shellcmdEnd, shellcmd);
    扩展后如下:
    .section ".liteos.table.shellcmd.finish","aw"
            .globl g_shellcmdEnd
            .type  g_shellcmdEnd, object
            .p2align 3
    g_shellcmdEnd :  
        .previous  
 * @par 与BEGIN宏的关系
 * 两者共同构成一个完整的表结构，BEGIN宏定义表的起始地址，END宏定义表的结束地址，
 * 系统可通过这两个标签计算表的大小和遍历表中的元素
 */
#ifndef LOS_HAL_TABLE_END
#define LOS_HAL_TABLE_END(label, name)                                       \
    __asm__(".section \".liteos.table." X_STRING(name) \".finish\",\"aw\"\n"  \
            ".globl " X_STRING(LOS_LABEL_DEFN(label)) "\n"                   \
            ".type    " X_STRING(LOS_LABEL_DEFN(label)) ",object\n"          \
            ".p2align " X_STRING(LOSARC_P2ALIGNMENT) "\n"                    \
            X_STRING(LOS_LABEL_DEFN(label)) ":\n"                            \
            ".previous\n"                                                    \
            )
#endif

/**
 * @brief HAL表类型及条目定义宏集合
 *
 * 以下宏用于定义HAL(硬件抽象层)表的类型、额外数据段和表条目，确保表数据在内存中正确布局
 * 和对齐，以便系统启动时能够正确识别和遍历这些表结构
 */

/**
 * @ingroup los_hal
 * @brief HAL表类型定义宏
 *
 * 该宏必须应用于所有要放入表中的对象类型定义，确保表中元素按系统要求对齐
 *
 * @param LOSBLD_ATTRIB_ALIGN 编译器属性宏，用于指定对齐方式
 * @param LOSARC_ALIGNMENT 系统架构定义的对齐字节数(通常为4或8字节)
 * @note 此宏应添加到结构体或联合体定义前，例如：
 *       typedef LOS_HAL_TABLE_TYPE struct {
 *           int member1;
 *           void (*func)(void);
 *       } MyTableType;
 */
#ifndef LOS_HAL_TABLE_TYPE
#define LOS_HAL_TABLE_TYPE LOSBLD_ATTRIB_ALIGN(LOSARC_ALIGNMENT)
#endif

/**
 * @ingroup los_hal
 * @brief HAL表额外数据段定义宏
 *
 * 将数据放入表的额外数据段，用于存储与主表关联但不需要在主表中遍历的数据
 *
 * @param name 表名称，需与主表名称保持一致以建立关联
 * @note 生成的段名为".liteos.table.name.extra"
 * @par 使用示例
 * @code
 * LOS_HAL_TABLE_EXTRA(my_table) int g_myExtraData = 0x12345678;
 * @endcode
 */
#ifndef LOS_HAL_TABLE_EXTRA
#define LOS_HAL_TABLE_EXTRA(name) \
    LOSBLD_ATTRIB_SECTION(".liteos.table." X_STRING(name) ".extra")
#endif

/**
 * @ingroup los_hal
 * @brief HAL表条目定义宏
 *
 * 将结构体实例标记为表条目，放入指定的数据段并防止编译器优化移除
 *
 * @param name 表名称，用于指定条目所属的表
 * @note 1. 生成的段名为".liteos.table.name.data"
 *       2. LOSBLD_ATTRIB_USED属性确保即使没有显式引用，条目也不会被优化
    如: LOS_HAL_TABLE_ENTRY(shellcmd)
    扩展后如下:
    __attribute__((section(".liteos.table.shellcmd.data")))
    __attribute__((used))
 * @par 使用示例
 * @code
 * LOS_HAL_TABLE_ENTRY(my_table) MyTableType g_myEntry = {
 *     .member1 = 1,
 *     .func = myFunc,
 * };
 * @endcode
 */
#ifndef LOS_HAL_TABLE_ENTRY
#define LOS_HAL_TABLE_ENTRY(name)                                  \
    LOSBLD_ATTRIB_SECTION(".liteos.table." X_STRING(name) ".data") \
    LOSBLD_ATTRIB_USED
#endif

/**
 * @ingroup los_hal_table
 * @brief 定义带限定符的HAL表项，并指定其存储段
 *
 * @param name 表名称，用于构建段路径
 * @param _qual 表项限定符，用于区分同表中的不同类型表项
 * @note 段路径格式为".liteos.table.<name>.data.<_qual>"
 * @par 示例
 * @code
 * LOS_HAL_TABLE_QUALIFIED_ENTRY(uart, controller) struct UartController g_uart0;
 * // 将创建段：.liteos.table.uart.data.controller
 * @endcode
 * @see LOS_HAL_TABLE_BEGIN, LOS_HAL_TABLE_ENTRY
 */
#ifndef LOS_HAL_TABLE_QUALIFIED_ENTRY
#define LOS_HAL_TABLE_QUALIFIED_ENTRY(name, _qual)                                  \
    LOSBLD_ATTRIB_SECTION(".liteos.table." X_STRING(name) ".data." X_STRING(_qual)) \
    LOSBLD_ATTRIB_USED
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_TABLES_H */
