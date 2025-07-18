/*
 * Copyright (c) 2021-2021 Huawei Device Co., Ltd. All rights reserved.
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

#ifndef LOS_BLACKBOX_H
#define LOS_BLACKBOX_H

#include "stdarg.h"
#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/* 路径相关宏定义 */
#define PATH_MAX_LEN          256  /* 文件路径最大长度 */
#define EVENT_MAX_LEN         32   /* 事件名称最大长度 */
#define MODULE_MAX_LEN        32   /* 模块名称最大长度 */
#define ERROR_DESC_MAX_LEN    512  /* 错误描述最大长度 */
#define KERNEL_FAULT_LOG_PATH LOSCFG_BLACKBOX_LOG_ROOT_PATH "/kernel_fault.log"  /* 内核故障日志路径 */
#define USER_FAULT_LOG_PATH   LOSCFG_BLACKBOX_LOG_ROOT_PATH "/user_fault.log"    /* 用户故障日志路径 */

/* 模块与事件类型宏定义 */
#define MODULE_SYSTEM         "SYSTEM"        /* 系统模块名称 */
#define EVENT_SYSREBOOT       "SYSREBOOT"     /* 系统重启事件 */
#define EVENT_LONGPRESS       "LONGPRESS"     /* 长按事件 */
#define EVENT_COMBINATIONKEY  "COMBINATIONKEY"/* 组合键事件 */
#define EVENT_SUBSYSREBOOT    "SUBSYSREBOOT"  /* 子系统重启事件 */
#define EVENT_POWEROFF        "POWEROFF"      /* 关机事件 */
#define EVENT_PANIC           "PANIC"         /* 系统崩溃事件 */
#define EVENT_SYS_WATCHDOG    "SYSWATCHDOG"   /* 系统看门狗事件 */
#define EVENT_HUNGTASK        "HUNGTASK"      /* 任务挂起事件 */
#define EVENT_BOOTFAIL        "BOOTFAIL"      /* 启动失败事件 */

/**
 * @brief 错误信息结构体
 * @details 用于存储错误事件的详细信息，包括事件类型、模块名称和错误描述
 */
struct ErrorInfo {
    char event[EVENT_MAX_LEN];        /* 事件名称，如EVENT_PANIC */
    char module[MODULE_MAX_LEN];      /* 模块名称，如MODULE_SYSTEM */
    char errorDesc[ERROR_DESC_MAX_LEN];/* 错误描述信息 */
};

/**
 * @brief 模块操作函数结构体
 * @details 定义黑盒模块的标准操作接口，包括日志转储、重置、获取和保存等功能
 */
struct ModuleOps {
    char module[MODULE_MAX_LEN];                      /* 模块名称 */
    void (*Dump)(const char *logDir, struct ErrorInfo *info); /* 日志转储函数：将错误信息写入指定目录 */
    void (*Reset)(struct ErrorInfo *info);             /* 重置函数：清除错误信息 */
    int (*GetLastLogInfo)(struct ErrorInfo *info);     /* 获取最后日志信息函数 */
    int (*SaveLastLog)(const char *logDir, struct ErrorInfo *info); /* 保存最后日志函数 */
};

/**
 * @brief 注册模块操作函数
 * @param[in] ops 模块操作函数结构体指针
 * @return 成功返回0，失败返回错误码
 * @note 各模块通过此函数向黑盒系统注册自己的操作接口
 */
int BBoxRegisterModuleOps(struct ModuleOps *ops);

/**
 * @brief 通知错误事件
 * @param[in] event 事件名称（如EVENT_PANIC）
 * @param[in] module 模块名称（如MODULE_SYSTEM）
 * @param[in] errorDesc 错误描述信息
 * @param[in] needSysReset 是否需要系统重置（1：需要，0：不需要）
 * @return 成功返回0，失败返回错误码
 * @note 系统发生错误时调用此函数记录错误信息
 */
int BBoxNotifyError(const char event[EVENT_MAX_LEN],
    const char module[MODULE_MAX_LEN],
    const char errorDesc[ERROR_DESC_MAX_LEN],
    int needSysReset);

/**
 * @brief 黑盒驱动初始化
 * @return 成功返回0，失败返回错误码
 * @note 系统启动时初始化黑盒驱动，包括日志目录创建等
 */
int OsBBoxDriverInit(void);


#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif