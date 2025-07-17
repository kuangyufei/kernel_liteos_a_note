/*
 * Copyright (c) 2023-2023 Huawei Device Co., Ltd. All rights reserved.
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

#ifndef _LOS_DEVICELIMIT_H
#define _LOS_DEVICELIMIT_H

#include "los_typedef.h"
#include "los_list.h"
#include "vfs_config.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
// 设备访问权限定义：创建节点权限（1）  
#define DEVLIMIT_ACC_MKNOD 1
// 设备访问权限定义：读权限（2）  
#define DEVLIMIT_ACC_READ  2
// 设备访问权限定义：写权限（4）  
#define DEVLIMIT_ACC_WRITE 4
// 设备访问权限掩码：包含创建节点、读、写权限（1|2|4=7）  
#define DEVLIMIT_ACC_MASK (DEVLIMIT_ACC_MKNOD | DEVLIMIT_ACC_READ | DEVLIMIT_ACC_WRITE)

// 设备类型定义：块设备（1）  
#define DEVLIMIT_DEV_BLOCK 1
// 设备类型定义：字符设备（2）  
#define DEVLIMIT_DEV_CHAR  2
// 设备类型定义：所有设备（4）  
#define DEVLIMIT_DEV_ALL   4 /* all devices */

// 设备限制策略：允许访问（1）  
#define DEVLIMIT_ALLOW 1
// 设备限制策略：拒绝访问（2）  
#define DEVLIMIT_DENY 2

// 访问控制列表长度（4）  
#define ACCLEN 4

struct SeqBuf;
typedef struct TagPLimiterSet ProcLimitSet;

/**
 * @brief 设备限制默认行为枚举
 * @note 定义当未匹配具体规则时的默认处理策略
 */
enum DevLimitBehavior {
    DEVLIMIT_DEFAULT_NONE,    // 无默认行为（需显式配置规则）  
    DEVLIMIT_DEFAULT_ALLOW,   // 默认允许所有设备访问  
    DEVLIMIT_DEFAULT_DENY,    // 默认拒绝所有设备访问  
};

/**
 * @brief 设备访问项结构体
 * @note 用于描述单个设备的访问控制规则
 */
typedef struct DevAccessItem {
    INT16 type;               // 设备类型（DEVLIMIT_DEV_BLOCK/CHAR/ALL）  
    INT16 access;             // 访问权限（DEVLIMIT_ACC_MKNOD/READ/WRITE组合）  
    LOS_DL_LIST list;         // 双向链表节点，用于接入访问控制列表  
    CHAR name[PATH_MAX];      // 设备名称（文件路径）  
} DevAccessItem;

/**
 * @brief 进程设备限制结构体
 * @note 描述进程的设备访问控制策略及规则集合
 */
typedef struct ProcDevLimit {
    struct ProcDevLimit *parent; // 父限制器指针（用于继承关系）  
    UINT8 allowFile;        // 允许访问的设备规则文件标志  
    UINT8 denyFile;         // 拒绝访问的设备规则文件标志  
    LOS_DL_LIST accessList; // 设备访问控制规则链表（DevAccessItem节点）  
    enum DevLimitBehavior behavior; // 默认行为策略  
} ProcDevLimit;

VOID OsDevLimitInit(UINTPTR limit);
VOID *OsDevLimitAlloc(VOID);
VOID OsDevLimitFree(UINTPTR limit);
VOID OsDevLimitCopy(UINTPTR dest, UINTPTR src);
UINT32 OsDevLimitWriteAllow(ProcLimitSet *plimit, const CHAR *buf, UINT32 size);
UINT32 OsDevLimitWriteDeny(ProcLimitSet *plimit, const CHAR *buf, UINT32 size);
UINT32 OsDevLimitShow(ProcDevLimit *devLimit, struct SeqBuf *seqBuf);
UINT32 OsDevLimitCheckPermission(INT32 vnodeType, const CHAR *pathName, INT32 flags);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
#endif /* _LOS_DEVICELIMIT_H */
