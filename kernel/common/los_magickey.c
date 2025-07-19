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

#include "los_magickey.h"
#include "console.h"
#include "los_task_pri.h"
#include "los_process_pri.h"
/**
 * @file los_magickey.c
 * @brief 内核魔术键（Magic Key）功能实现，提供调试和系统控制的快捷键机制
 */

/**
 * @brief 魔术键功能总开关
 * @details 当定义LOSCFG_ENABLE_MAGICKEY时启用魔术键功能，提供系统调试快捷键
 * @note 该宏通常在Kconfig中配置，通过菜单选项控制是否启用
 */
#ifdef LOSCFG_ENABLE_MAGICKEY

/**
 * @def MAGIC_KEY_NUM
 * @brief 魔术键最大数量定义
 * @details 指定系统支持的魔术键操作最大数量，包括一个终止NULL指针
 * @value 5 支持4个有效魔术键操作+1个NULL终止符
 */
#define MAGIC_KEY_NUM 5

/**
 * @brief 魔术键帮助信息显示函数声明
 * @details 显示所有支持的魔术键操作及其帮助信息
 * @retval None
 */
STATIC VOID OsMagicHelp(VOID);

/**
 * @brief 任务信息显示函数声明
 * @details 通过魔术键触发，显示系统当前所有任务的详细信息
 * @retval None
 */
STATIC VOID OsMagicTaskShow(VOID);

/**
 * @brief 系统恐慌触发函数声明
 * @details 通过魔术键触发系统恐慌（Panic），用于调试致命错误
 * @attention 此操作会导致系统立即停止并输出调试信息，谨慎使用
 * @retval None
 */
STATIC VOID OsMagicPanic(VOID);

/**
 * @brief 内存完整性检查函数声明
 * @details 通过魔术键触发系统内存完整性检查，验证内存一致性
 * @retval None
 */
STATIC VOID OsMagicMemCheck(VOID);

/**
 * @var g_magicMemCheckOp
 * @brief 内存检查魔术键操作结构体
 * @details 绑定Ctrl+E快捷键到内存检查功能
 * @note 静态全局变量，仅在魔术键功能启用时定义
 */
STATIC MagicKeyOp g_magicMemCheckOp = {
    .opHandler = OsMagicMemCheck,          /**< 内存检查处理函数 */
    .helpMsg = "Check system memory(ctrl+e) ",  /**< 帮助信息字符串 */
    .magicKey = 0x05 /* ctrl + e */         /**< 魔术键值：Ctrl+E (ASCII 0x05) */
};

/**
 * @var g_magicPanicOp
 * @brief 系统恐慌魔术键操作结构体
 * @details 绑定Ctrl+P快捷键到系统恐慌功能
 * @note 静态全局变量，仅在魔术键功能启用时定义
 */
STATIC MagicKeyOp g_magicPanicOp = {
    .opHandler = OsMagicPanic,             /**< 系统恐慌处理函数 */
    .helpMsg = "System panic(ctrl+p) ",     /**< 帮助信息字符串 */
    .magicKey = 0x10 /* ctrl + p */         /**< 魔术键值：Ctrl+P (ASCII 0x10) */
};

/**
 * @var g_magicTaskShowOp
 * @brief 任务显示魔术键操作结构体
 * @details 绑定Ctrl+T快捷键到任务信息显示功能
 * @note 静态全局变量，仅在魔术键功能启用时定义
 */
STATIC MagicKeyOp g_magicTaskShowOp = {
    .opHandler = OsMagicTaskShow,          /**< 任务显示处理函数 */
    .helpMsg = "Show task information(ctrl+t) ",  /**< 帮助信息字符串 */
    .magicKey = 0x14 /* ctrl + t */         /**< 魔术键值：Ctrl+T (ASCII 0x14) */
};

/**
 * @var g_magicHelpOp
 * @brief 帮助信息魔术键操作结构体
 * @details 绑定Ctrl+Z快捷键到帮助信息显示功能
 * @note 静态全局变量，仅在魔术键功能启用时定义
 */
STATIC MagicKeyOp g_magicHelpOp = {
    .opHandler = OsMagicHelp,              /**< 帮助信息处理函数 */
    .helpMsg = "Show all magic op key(ctrl+z) ",  /**< 帮助信息字符串 */
    .magicKey = 0x1a /* ctrl + z */         /**< 魔术键值：Ctrl+Z (ASCII 0x1a) */
};

/**
 * @brief 魔术键操作表说明
 * @note 以下控制键不建议用作魔术键：
 * - ctrl+h/退格键=0x8：可能与终端退格功能冲突
 * - ctrl+i/Tab=0x9：可能与终端制表符功能冲突
 * - ctrl+m/回车=0xd：可能与终端回车功能冲突
 * - ctrl+n/shift out=0xe：可能与终端字符集切换冲突
 * - ctrl+o/shift in=0xf：可能与终端字符集切换冲突
 * - ctrl+[/ESC=0x1b：可能与终端退出功能冲突
 * - ctrl+]=0x1d：用于telnet命令模式
 */
/**
 * @var g_magicOpTable
 * @brief 魔术键操作表
 * @details 存储所有魔术键操作结构体的指针，形成魔术键功能列表
 * @note 以NULL指针结束，便于遍历；顺序定义了快捷键的优先级
 */
STATIC MagicKeyOp *g_magicOpTable[MAGIC_KEY_NUM] = {
    &g_magicMemCheckOp, /* ctrl + e：内存检查 */
    &g_magicPanicOp,    /* ctrl + p：系统恐慌 */
    &g_magicTaskShowOp, /* ctrl + t：任务显示 */
    &g_magicHelpOp,     /* ctrl + z：帮助信息 */
    NULL                /* 操作表结束标志 */
};

/**
 * @brief 魔术键帮助信息显示函数实现
 * @details 遍历魔术键操作表，打印所有支持的魔术键及其帮助信息
 * @retval None
 * @par 输出示例
 * @code
 * HELP: Check system memory(ctrl+e) System panic(ctrl+p) Show task information(ctrl+t) Show all magic op key(ctrl+z) 
 * @endcode
 */
STATIC VOID OsMagicHelp(VOID)
{
    INT32 i;                         /* 循环索引变量 */
    PRINTK("HELP: ");                /* 打印帮助信息前缀 */
    /* 遍历魔术键操作表，打印每个魔术键的帮助信息 */
    for (i = 0; g_magicOpTable[i] != NULL; ++i) {
        PRINTK("%s ", g_magicOpTable[i]->helpMsg);
    }
    PRINTK("\n");                   /* 打印换行符结束帮助信息 */
    return;
}

/**
 * @brief 任务信息显示函数实现
 * @details 通过调用Shell命令处理函数，显示系统所有任务的详细信息
 * @retval None
 * @note 等效于在Shell中执行任务信息查询命令
 */
STATIC VOID OsMagicTaskShow(VOID)
{
    /* 调用任务信息获取函数，显示所有任务的完整信息 */
    (VOID)OsShellCmdTskInfoGet(OS_ALL_TASK_MASK, NULL, OS_PROCESS_INFO_ALL);
    return;
}

/**
 * @brief 系统恐慌触发函数实现
 * @details 调用LOS_Panic函数触发系统恐慌，输出调试信息并停止系统
 * @attention 此函数会导致系统立即终止，仅用于调试致命错误
 * @retval None
 */
STATIC VOID OsMagicPanic(VOID)
{
    LOS_Panic("Magic key :\n");     /* 触发系统恐慌，输出魔术键触发信息 */
    return;
}

/**
 * @brief 内存完整性检查函数实现
 * @details 调用内存完整性检查函数，验证系统内存的一致性
 * @retval None
 * @note 检查结果通过PrintExcInfo输出，成功时显示"all passed"
 */
STATIC VOID OsMagicMemCheck(VOID)
{
    /* 检查系统内存完整性，若成功则输出通过信息 */
    if (LOS_MemIntegrityCheck(m_aucSysMem1) == LOS_OK) {
        PrintExcInfo("system memcheck over, all passed!\n");
    }
    return;
}
#endif /* LOSCFG_ENABLE_MAGICKEY */

/**
 * @brief 魔术键检查与处理函数
 * @details 根据输入的按键值和魔术键开关状态，执行相应的魔术键操作
 * @param[in] key 输入的按键ASCII值
 * @param[in] consoleId 控制台ID，用于确定需要操作的进程组
 * @retval INT32 处理结果
 * @retval 0 普通按键，已处理（如Ctrl+C）
 * @retval 1 魔术键，已处理
 * @retval 其他值 未处理或错误
 * @note 当魔术键开关关闭时，所有魔术键操作均不响应
 */
INT32 CheckMagicKey(CHAR key, UINT16 consoleId)
{
#ifdef LOSCFG_ENABLE_MAGICKEY
    INT32 i;                         /* 循环索引变量 */
    STATIC UINT32 magicKeySwitch = 0; /* 魔术键开关状态：0-关闭，非0-开启；静态变量保持状态 */

    /* 处理Ctrl+C（0x03）：终止当前控制台进程组 */
    if (key == 0x03) { /* ctrl + c */
        KillPgrp(consoleId);          /* 终止指定控制台的进程组 */
        return 0;                     /* 返回0表示已处理普通控制键 */
    /* 处理Ctrl+R（0x12）：切换魔术键开关状态 */
    } else if (key == 0x12) { /* ctrl + r */
        magicKeySwitch = ~magicKeySwitch; /* 翻转开关状态 */
        if (magicKeySwitch != 0) {
            PrintExcInfo("Magic key on\n"); /* 输出魔术键开启信息 */
        } else {
            PrintExcInfo("Magic key off\n"); /* 输出魔术键关闭信息 */
        }
        return 1;                     /* 返回1表示已处理魔术键开关 */
    }

    /* 当魔术键开关开启时，处理已注册的魔术键 */
    if (magicKeySwitch != 0) {
        /* 遍历魔术键操作表，查找匹配的按键 */
        for (i = 0; i < MAGIC_KEY_NUM; i++) {
            /* 跳过空指针，检查按键是否匹配 */
            if (g_magicOpTable[i] != NULL && key == g_magicOpTable[i]->magicKey) {
                (g_magicOpTable[i])->opHandler(); /* 调用对应的处理函数 */
                return 1;                         /* 返回1表示已处理魔术键 */
            }
        }
    }
#endif /* LOSCFG_ENABLE_MAGICKEY */
    return 0; /* 未处理的按键或魔术键功能未启用 */
}