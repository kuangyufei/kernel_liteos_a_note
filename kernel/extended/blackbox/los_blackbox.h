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

#define PATH_MAX_LEN          256
#define EVENT_MAX_LEN         32
#define MODULE_MAX_LEN        32
#define ERROR_DESC_MAX_LEN    512
#define KERNEL_FAULT_LOG_PATH LOSCFG_BLACKBOX_LOG_ROOT_PATH "/kernel_fault.log"
#define USER_FAULT_LOG_PATH   LOSCFG_BLACKBOX_LOG_ROOT_PATH "/user_fault.log"

#define MODULE_SYSTEM         "SYSTEM"
#define EVENT_SYSREBOOT       "SYSREBOOT"
#define EVENT_LONGPRESS       "LONGPRESS"
#define EVENT_COMBINATIONKEY  "COMBINATIONKEY"
#define EVENT_SUBSYSREBOOT    "SUBSYSREBOOT"
#define EVENT_POWEROFF        "POWEROFF"
#define EVENT_PANIC           "PANIC"
#define EVENT_SYS_WATCHDOG    "SYSWATCHDOG"
#define EVENT_HUNGTASK        "HUNGTASK"
#define EVENT_BOOTFAIL        "BOOTFAIL"

struct ErrorInfo {
    char event[EVENT_MAX_LEN];
    char module[MODULE_MAX_LEN];
    char errorDesc[ERROR_DESC_MAX_LEN];
};

struct ModuleOps {
    char module[MODULE_MAX_LEN];
    void (*Dump)(const char *logDir, struct ErrorInfo *info);
    void (*Reset)(struct ErrorInfo *info);
    int (*GetLastLogInfo)(struct ErrorInfo *info);
    int (*SaveLastLog)(const char *logDir, struct ErrorInfo *info);
};

int BBoxRegisterModuleOps(struct ModuleOps *ops);
int BBoxNotifyError(const char event[EVENT_MAX_LEN],
    const char module[MODULE_MAX_LEN],
    const char errorDesc[ERROR_DESC_MAX_LEN],
    int needSysReset);
int OsBBoxDriverInit(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif