/*
 * Copyright (c) 2022-2022 Huawei Device Co., Ltd. All rights reserved.
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

#ifndef _LOS_USER_CONTAINER_PRI_H
#define _LOS_USER_CONTAINER_PRI_H

#include "los_atomic.h"
#include "los_credentials_pri.h"
/**
 * @ingroup los_user_container
 * @brief UID/GID映射表最大范围数量
 * @details 定义用户ID/组ID映射表支持的最大连续范围数量
 * @value 5 - 表示最多可同时映射5个不连续的ID范围
 */
#define UID_GID_MAP_MAX_EXTENTS 5  /* UID/GID映射表支持的最大范围数量 */

#ifdef LOSCFG_USER_CONTAINER  /* 仅当启用用户容器配置时编译以下代码 */

struct ProcFile;  /* 前向声明：进程文件结构体 */

/**
 * @ingroup los_user_container
 * @brief UID/GID映射范围结构体
 * @details 描述一组连续的用户ID或组ID映射关系
 * @note 用于实现容器内ID到宿主ID的转换
 */
typedef struct UidGidExtent {
    UINT32 first;       /* 容器内起始ID - 容器中进程可见的起始ID */
    UINT32 lowerFirst;  /* 宿主起始ID - 映射到宿主系统的起始ID */
    UINT32 count;       /* ID数量 - 连续映射的ID总数 */
} UidGidExtent;  /* UID/GID单个映射范围定义 */

/**
 * @ingroup los_user_container
 * @brief UID/GID映射表结构体
 * @details 管理多个连续ID范围的映射关系
 * @note 采用联合体结构节省内存，最多支持UID_GID_MAP_MAX_EXTENTS个范围
 */
typedef struct UidGidMap {
    UINT32 extentCount; /* 范围数量 - 当前有效的映射范围总数 */
    union {
        UidGidExtent extent[UID_GID_MAP_MAX_EXTENTS];  /* 映射范围数组 - 存储具体映射关系 */
    };
} UidGidMap;  /* UID/GID完整映射表定义 */

/**
 * @ingroup los_user_container
 * @brief 用户容器结构体
 * @details 描述一个用户命名空间容器，包含ID映射、层级关系和所有权信息
 * @note 用于实现容器化环境下的用户隔离与权限控制
 */
typedef struct UserContainer {
    Atomic rc;                  /* 引用计数 - 原子操作确保线程安全 */
    INT32 level;                /* 容器层级 - 0表示顶层容器，数值越大层级越深 */
    UINT32 owner;               /* 所有者UID - 创建该容器的用户ID */
    UINT32 group;               /* 所属组GID - 创建该容器的组ID */
    struct UserContainer *parent; /* 父容器指针 - 指向父容器，形成容器层级结构 */
    UidGidMap uidMap;           /* UID映射表 - 容器内UID到宿主UID的映射关系 */
    UidGidMap gidMap;           /* GID映射表 - 容器内GID到宿主GID的映射关系 */
    UINT32 containerID;         /* 容器ID - 全局唯一的容器标识符 */
} UserContainer;  /* 用户容器核心结构体 */

/**
 * @ingroup los_user_container
 * @brief 创建用户容器
 * @param[in] newCredentials 新凭证指针 - 容器的安全凭证信息
 * @param[in] parentUserContainer 父容器指针 - 新容器的父容器，NULL表示创建顶层容器
 * @retval LOS_OK 成功
 * @retval 其他值 失败，具体错误码见错误码定义
 * @details 初始化容器结构体，设置层级关系，复制父容器映射表
 * @note 新容器的引用计数初始化为1
 */
UINT32 OsCreateUserContainer(Credentials *newCredentials, UserContainer *parentUserContainer);

/**
 * @ingroup los_user_container
 * @brief 释放用户容器
 * @param[in] userContainer 容器指针 - 待释放的用户容器
 * @details 原子递减引用计数，当计数为0时释放容器内存及关联资源
 * @note 线程安全的容器销毁函数
 */
VOID FreeUserContainer(UserContainer *userContainer);

/**
 * @ingroup los_user_container
 * @brief 将内核UID转换为容器内UID
 * @param[in] userContainer 容器指针 - 目标用户容器
 * @param[in] kuid 内核UID - 宿主系统中的UID
 * @return 容器内UID - 转换后的ID， UINT32_MAX表示转换失败
 * @details 根据容器的UID映射表进行反向查找
 */
UINT32 OsFromKuidMunged(UserContainer *userContainer, UINT32 kuid);

/**
 * @ingroup los_user_container
 * @brief 将内核GID转换为容器内GID
 * @param[in] userContainer 容器指针 - 目标用户容器
 * @param[in] kgid 内核GID - 宿主系统中的GID
 * @return 容器内GID - 转换后的ID， UINT32_MAX表示转换失败
 * @details 根据容器的GID映射表进行反向查找
 */
UINT32 OsFromKgidMunged(UserContainer *userContainer, UINT32 kgid);

/**
 * @ingroup los_user_container
 * @brief 将容器内UID转换为内核UID
 * @param[in] userContainer 容器指针 - 目标用户容器
 * @param[in] uid 容器内UID - 容器中进程使用的UID
 * @return 内核UID - 转换后的宿主系统UID
 * @details 根据容器的UID映射表进行正向转换
 */
UINT32 OsMakeKuid(UserContainer *userContainer, UINT32 uid);

/**
 * @ingroup los_user_container
 * @brief 将容器内GID转换为内核GID
 * @param[in] userContainer 容器指针 - 目标用户容器
 * @param[in] gid 容器内GID - 容器中进程使用的GID
 * @return 内核GID - 转换后的宿主系统GID
 * @details 根据容器的GID映射表进行正向转换
 */
UINT32 OsMakeKgid(UserContainer *userContainer, UINT32 gid);

/**
 * @ingroup los_user_container
 * @brief 写入UID/GID映射表
 * @param[in] fp 进程文件指针 - 映射表对应的proc文件
 * @param[in] buf 缓冲区指针 - 包含映射规则的输入数据
 * @param[in] count 数据长度 - 输入数据的字节数
 * @param[in] capSetid 权限标志 - 是否拥有CAP_SETUID/CAP_SETGID权限
 * @param[in,out] map 映射表指针 - 待更新的UID/GID映射表
 * @param[in] parentMap 父容器映射表 - 父容器的UID/GID映射表
 * @retval 实际写入的字节数 成功
 * @retval -1 失败
 * @details 解析输入缓冲区的映射规则，更新映射表，支持多范围映射
 * @attention 只有拥有CAP_SETUID/CAP_SETGID权限的进程才能修改映射表
 */
INT32 OsUserContainerMapWrite(struct ProcFile *fp, CHAR *buf, size_t count,
                              INT32 capSetid, UidGidMap *map, UidGidMap *parentMap);

/**
 * @ingroup los_user_container
 * @brief 获取用户容器总数
 * @return UINT32 容器总数 - 当前系统中活跃的用户容器数量
 * @details 遍历容器链表统计活跃容器数量
 * @note 用于系统状态监控和资源管理
 */
UINT32 OsGetUserContainerCount(VOID);
#endif /* LOSCFG_USER_CONTAINER */  /* 用户容器配置代码结束 */
#endif
