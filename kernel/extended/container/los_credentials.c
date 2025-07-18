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

#include <errno.h>
#include "los_credentials_pri.h"
#include "los_user_container_pri.h"
#include "los_config.h"
#include "los_memory.h"
#include "los_process_pri.h"
#ifdef LOSCFG_USER_CONTAINER  // 如果启用用户容器配置
STATIC Credentials *CreateNewCredential(LosProcessCB *parent)
{
    UINT32 size = sizeof(Credentials);  // 计算凭证结构体大小
    Credentials *newCredentials = LOS_MemAlloc(m_aucSysMem1, size);  // 分配新凭证结构体内存
    if (newCredentials == NULL) {  // 检查内存分配是否成功
        return NULL;  // 分配失败，返回空指针
    }

    if (parent != NULL) {  // 如果存在父进程
        const Credentials *oldCredentials = parent->credentials;  // 获取父进程凭证
        (VOID)memcpy_s(newCredentials, sizeof(Credentials), oldCredentials, sizeof(Credentials));  // 复制父进程凭证信息
        LOS_AtomicSet(&newCredentials->rc, 1);  // 设置引用计数为1
    } else {  // 如果是根进程
        (VOID)memset_s(newCredentials, sizeof(Credentials), 0, sizeof(Credentials));  // 初始化凭证结构体
        LOS_AtomicSet(&newCredentials->rc, 3);  // 3: 三个系统进程
    }
    newCredentials->userContainer = NULL;  // 初始化用户容器指针为空
    return newCredentials;  // 返回新创建的凭证
}

Credentials *PrepareCredential(LosProcessCB *runProcessCB)
{
    Credentials *newCredentials = CreateNewCredential(runProcessCB);  // 创建新凭证
    if (newCredentials == NULL) {  // 检查凭证创建是否成功
        return NULL;  // 创建失败，返回空指针
    }

    newCredentials->userContainer = runProcessCB->credentials->userContainer;  // 复制用户容器指针
    LOS_AtomicInc(&newCredentials->userContainer->rc);  // 增加用户容器引用计数
    return newCredentials;  // 返回准备好的凭证
}

VOID FreeCredential(Credentials *credentials)
{
    if (credentials == NULL) {  // 检查凭证指针是否为空
        return;  // 空指针，直接返回
    }

    if (credentials->userContainer != NULL) {  // 如果用户容器指针不为空
        LOS_AtomicDec(&credentials->userContainer->rc);  // 减少用户容器引用计数
        if (LOS_AtomicRead(&credentials->userContainer->rc) <= 0) {  // 检查引用计数是否为0
            FreeUserContainer(credentials->userContainer);  // 释放用户容器
            credentials->userContainer = NULL;  // 置空用户容器指针
        }
    }

    LOS_AtomicDec(&credentials->rc);  // 减少凭证引用计数
    if (LOS_AtomicRead(&credentials->rc) <= 0) {  // 检查引用计数是否为0
        (VOID)LOS_MemFree(m_aucSysMem1, credentials);  // 释放凭证内存
    }
}

VOID OsUserContainerDestroy(LosProcessCB *curr)
{
    UINT32 intSave;  // 中断保存标志
    SCHEDULER_LOCK(intSave);  // 关闭调度器，保护临界区
    FreeCredential(curr->credentials);  // 释放当前进程凭证
    curr->credentials = NULL;  // 置空凭证指针
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    return;  // 返回
}

STATIC Credentials *CreateCredentials(unsigned long flags, LosProcessCB *parent)
{
    UINT32 ret;  // 返回值
    Credentials *newCredentials = CreateNewCredential(parent);  // 创建新凭证
    if (newCredentials == NULL) {  // 检查凭证创建是否成功
        return NULL;  // 创建失败，返回空指针
    }

    if (!(flags & CLONE_NEWUSER)) {  // 如果不需要创建新用户命名空间
        newCredentials->userContainer = parent->credentials->userContainer;  // 复制父进程用户容器
        LOS_AtomicInc(&newCredentials->userContainer->rc);  // 增加用户容器引用计数
        return newCredentials;  // 返回新凭证
    }

    if (parent != NULL) {  // 如果存在父进程
        ret = OsCreateUserContainer(newCredentials, parent->credentials->userContainer);  // 从父进程创建用户容器
    } else {  // 如果是根进程
        ret = OsCreateUserContainer(newCredentials, NULL);  // 创建新的用户容器
    }
    if (ret != LOS_OK) {  // 检查用户容器创建是否成功
        FreeCredential(newCredentials);  // 创建失败，释放凭证
        return NULL;  // 返回空指针
    }
    return newCredentials;  // 返回新凭证
}

UINT32 OsCopyCredentials(unsigned long flags,  LosProcessCB *child, LosProcessCB *parent)
{
    UINT32 intSave;  // 中断保存标志

    SCHEDULER_LOCK(intSave);  // 关闭调度器，保护临界区
    child->credentials = CreateCredentials(flags, parent);  // 为子进程创建凭证
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    if (child->credentials == NULL) {  // 检查凭证是否创建成功
        return ENOMEM;  // 创建失败，返回内存不足错误
    }
    return LOS_OK;  // 成功返回
}

UINT32 OsInitRootUserCredentials(Credentials **credentials)
{
    *credentials = CreateCredentials(CLONE_NEWUSER, NULL);  // 创建根用户凭证
    if (*credentials == NULL) {  // 检查凭证创建是否成功
        return ENOMEM;  // 创建失败，返回内存不足错误
    }
    return LOS_OK;  // 成功返回
}

UINT32 OsUnshareUserCredentials(UINTPTR flags, LosProcessCB *curr)
{
    UINT32 intSave;  // 中断保存标志
    if (!(flags & CLONE_NEWUSER)) {  // 如果不需要创建新用户命名空间
        return LOS_OK;  // 直接返回成功
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器，保护临界区
    UINT32 ret = OsCreateUserContainer(curr->credentials, curr->credentials->userContainer);  // 创建用户容器
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    return ret;  // 返回创建结果
}

UINT32 OsSetNsUserContainer(struct UserContainer *targetContainer, LosProcessCB *runProcess)
{
    Credentials *oldCredentials = runProcess->credentials;  // 保存旧凭证
    Credentials *newCredentials = CreateNewCredential(runProcess);  // 创建新凭证
    if (newCredentials == NULL) {  // 检查凭证创建是否成功
        return ENOMEM;  // 创建失败，返回内存不足错误
    }

    runProcess->credentials = newCredentials;  // 更新进程凭证为新凭证
    newCredentials->userContainer = targetContainer;  // 设置新凭证的用户容器
    LOS_AtomicInc(&targetContainer->rc);  // 增加目标容器引用计数
    FreeCredential(oldCredentials);  // 释放旧凭证
    return LOS_OK;  // 成功返回
}

UINT32 OsGetUserContainerID(Credentials *credentials)
{
    if ((credentials == NULL) || (credentials->userContainer == NULL)) {  // 检查凭证和用户容器是否有效
        return OS_INVALID_VALUE;  // 无效，返回无效值
    }

    return credentials->userContainer->containerID;  // 返回用户容器ID
}

INT32 CommitCredentials(Credentials *newCredentials)
{
    Credentials *oldCredentials = OsCurrProcessGet()->credentials;  // 获取当前进程旧凭证

    if (LOS_AtomicRead(&newCredentials->rc) < 1) {  // 检查新凭证引用计数是否有效
        return -EINVAL;  // 无效，返回参数错误
    }

    OsCurrProcessGet()->credentials = newCredentials;  // 更新当前进程凭证
    FreeCredential(oldCredentials);  // 释放旧凭证
    return 0;  // 成功返回
}

Credentials *CurrentCredentials(VOID)
{
    return OsCurrProcessGet()->credentials;  // 返回当前进程凭证
}

UserContainer *OsCurrentUserContainer(VOID)
{
    UserContainer *userContainer = OsCurrProcessGet()->credentials->userContainer;  // 获取当前用户容器
    return userContainer;  // 返回用户容器
}
#endif
