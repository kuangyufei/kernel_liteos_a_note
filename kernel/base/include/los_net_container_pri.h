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

#ifndef _LOS_NET_CONTAINER_PRI_H
#define _LOS_NET_CONTAINER_PRI_H
#include <lwip/net_group.h>
#include <lwip/netif.h>
#include <lwip/ip.h>
#include "los_atomic.h"

#ifdef LOSCFG_NET_CONTAINER
typedef struct ProcessCB LosProcessCB;
struct Container;

/**
 * @brief 网络容器结构体，用于管理网络资源的容器化对象
 * @details 维护网络组关联关系和容器标识，提供线程安全的引用计数机制
 */
typedef struct NetContainer {
    Atomic rc;                  /* 原子操作的引用计数器，用于管理结构体实例的生命周期 */
    struct net_group *group;    /* 指向网络组的指针，关联该容器所属的网络组 */
    UINT32 containerID;         /* 容器唯一标识符，用于区分不同的网络容器实例 */
} NetContainer;

UINT32 OsInitRootNetContainer(NetContainer **ipcContainer);

UINT32 OsCopyNetContainer(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent);

UINT32 OsUnshareNetContainer(UINTPTR flags, LosProcessCB *curr, struct Container *newContainer);

UINT32 OsSetNsNetContainer(UINT32 flags, struct Container *container, struct Container *newContainer);

VOID OsNetContainerDestroy(struct Container *container);

UINT32 OsGetNetContainerID(NetContainer *ipcContainer);

UINT32 OsGetNetContainerCount(VOID);
#endif
#endif /* _LOS_NET_CONTAINER_PRI_H */
