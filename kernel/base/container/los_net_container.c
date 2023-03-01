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

#ifdef LOSCFG_NET_CONTAINER
#include <sched.h>
#include "los_net_container_pri.h"
#include "los_config.h"
#include "los_memory.h"
#include "los_process_pri.h"

STATIC UINT32 g_currentNetContainerNum = 0;
STATIC NetContainer *g_rootNetContainer = NULL;

STATIC struct netif *NetifAlloc(VOID)
{
    UINT32 size = sizeof(struct netif);
    struct netif *netif = LOS_MemAlloc(m_aucSysMem1, size);
    if (netif == NULL) {
        return NULL;
    }
    (VOID)memset_s(netif, size, 0, size);

    return netif;
}

STATIC VOID FreeNetif(struct netif *netif)
{
    if (netif != NULL) {
        if (netif->peer != NULL) {
            netif_remove(netif->peer);
            (VOID)LOS_MemFree(m_aucSysMem1, netif->peer);
        }
        netif_remove(netif);
        (VOID)LOS_MemFree(m_aucSysMem1, netif);
    }
}

STATIC UINT32 CreateVethNetif(NetContainer *netContainer, struct netif **veth)
{
    struct netif *netif = NetifAlloc();
    if (netif == NULL) {
        return ENOMEM;
    }

    veth_init(netif, netContainer->group);

    *veth = netif;
    return LOS_OK;
}

STATIC UINT32 InitVethPair(NetContainer *netContainer)
{
    struct netif *vethOfNetContainer = NULL;
    struct netif *vethOfRootNetContainer = NULL;

    UINT32 ret = CreateVethNetif(netContainer, &vethOfNetContainer);
    if (ret != LOS_OK) {
        return ret;
    }

    ret = CreateVethNetif(g_rootNetContainer, &vethOfRootNetContainer);
    if (ret != LOS_OK) {
        FreeNetif(vethOfNetContainer);
        return ret;
    }

    vethOfNetContainer->peer = vethOfRootNetContainer;
    vethOfRootNetContainer->peer = vethOfNetContainer;
    return LOS_OK;
}

STATIC UINT32 InitLoopNetif(NetContainer *netContainer)
{
    struct netif *loop_netif = NetifAlloc();
    if (loop_netif == NULL) {
        return ENOMEM;
    }

    netContainer->group->loop_netif = loop_netif;
    netif_init(netContainer->group);
    return LOS_OK;
}

STATIC UINT32 InitContainerNetifs(NetContainer *netContainer)
{
    UINT32 ret = InitLoopNetif(netContainer);
    if (ret != LOS_OK) {
        return ret;
    }
    return InitVethPair(netContainer);
}

STATIC UINT32 InitNetGroup(NetContainer *netContainer)
{
    UINT32 size = sizeof(struct net_group);
    struct net_group *group = LOS_MemAlloc(m_aucSysMem1, size);
    if (group == NULL) {
        return ENOMEM;
    }
    (VOID)memset_s(group, size, 0, size);

    netContainer->group = group;
    return LOS_OK;
}

STATIC VOID FreeNetContainerGroup(struct net_group *group)
{
    if (group == NULL) {
        return;
    }

    struct netif *freeNetif = group->netif_list;
    struct netif *nextNetif = NULL;

    while (freeNetif != NULL) {
        nextNetif = freeNetif->next;
        FreeNetif(freeNetif);
        freeNetif = nextNetif;
    }
}

VOID OsNetContainerDestroy(Container *container)
{
    UINT32 intSave;
    if (container == NULL) {
        return;
    }

    SCHEDULER_LOCK(intSave);
    NetContainer *netContainer = container->netContainer;
    if (netContainer == NULL) {
        SCHEDULER_UNLOCK(intSave);
        return;
    }

    LOS_AtomicDec(&netContainer->rc);
    if (LOS_AtomicRead(&netContainer->rc) > 0) {
        SCHEDULER_UNLOCK(intSave);
        return;
    }

    g_currentNetContainerNum--;
    SCHEDULER_UNLOCK(intSave);
    FreeNetContainerGroup(netContainer->group);
    container->netContainer = NULL;
    (VOID)LOS_MemFree(m_aucSysMem1, netContainer->group);
    (VOID)LOS_MemFree(m_aucSysMem1, netContainer);
    return;
}

STATIC NetContainer *CreateNewNetContainer(NetContainer *parent)
{
    NetContainer *netContainer = LOS_MemAlloc(m_aucSysMem1, sizeof(NetContainer));
    if (netContainer == NULL) {
        return NULL;
    }
    (VOID)memset_s(netContainer, sizeof(NetContainer), 0, sizeof(NetContainer));

    netContainer->containerID = OsAllocContainerID();

    if (parent != NULL) {
        LOS_AtomicSet(&netContainer->rc, 1);
    } else {
        LOS_AtomicSet(&netContainer->rc, 3); /* 3: Three system processes */
    }
    return netContainer;
}

STATIC UINT32 CreateNetContainer(Container *container, NetContainer *parentContainer)
{
    UINT32 intSave, ret;
    NetContainer *newNetContainer = CreateNewNetContainer(parentContainer);
    if (newNetContainer == NULL) {
        return ENOMEM;
    }

    ret = InitNetGroup(newNetContainer);
    if (ret != LOS_OK) {
        (VOID)LOS_MemFree(m_aucSysMem1, newNetContainer);
        return ret;
    }

    ret = InitContainerNetifs(newNetContainer);
    if (ret != LOS_OK) {
        (VOID)LOS_MemFree(m_aucSysMem1, newNetContainer->group);
        (VOID)LOS_MemFree(m_aucSysMem1, newNetContainer);
        return ret;
    }

    SCHEDULER_LOCK(intSave);
    g_currentNetContainerNum++;
    container->netContainer = newNetContainer;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

UINT32 OsCopyNetContainer(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent)
{
    UINT32 intSave;
    NetContainer *currNetContainer = parent->container->netContainer;

    if (!(flags & CLONE_NEWNET)) {
        SCHEDULER_LOCK(intSave);
        LOS_AtomicInc(&currNetContainer->rc);
        child->container->netContainer = currNetContainer;
        SCHEDULER_UNLOCK(intSave);
        return LOS_OK;
    }

    if (OsContainerLimitCheck(NET_CONTAINER, &g_currentNetContainerNum) != LOS_OK) {
        return EPERM;
    }

    return CreateNetContainer(child->container, currNetContainer);
}

UINT32 OsUnshareNetContainer(UINTPTR flags, LosProcessCB *curr, Container *newContainer)
{
    UINT32 intSave;
    NetContainer *parentContainer = curr->container->netContainer;

    if (!(flags & CLONE_NEWNET)) {
        SCHEDULER_LOCK(intSave);
        newContainer->netContainer = parentContainer;
        LOS_AtomicInc(&parentContainer->rc);
        SCHEDULER_UNLOCK(intSave);
        return LOS_OK;
    }

    if (OsContainerLimitCheck(NET_CONTAINER, &g_currentNetContainerNum) != LOS_OK) {
        return EPERM;
    }

    return CreateNetContainer(newContainer, parentContainer);
}

UINT32 OsSetNsNetContainer(UINT32 flags, Container *container, Container *newContainer)
{
    if (flags & CLONE_NEWNET) {
        newContainer->netContainer = container->netContainer;
        LOS_AtomicInc(&container->netContainer->rc);
        return LOS_OK;
    }

    newContainer->netContainer = OsCurrProcessGet()->container->netContainer;
    LOS_AtomicInc(&newContainer->netContainer->rc);
    return LOS_OK;
}

UINT32 OsGetNetContainerID(NetContainer *netContainer)
{
    if (netContainer == NULL) {
        return OS_INVALID_VALUE;
    }

    return netContainer->containerID;
}

STATIC struct net_group *DoGetNetGroupFromCurrProcess(VOID)
{
    LosProcessCB *processCB = OsCurrProcessGet();
    NetContainer *netContainer = processCB->container->netContainer;
    return netContainer->group;
}

STATIC VOID DoSetNetifNetGroup(struct netif *netif, struct net_group *group)
{
    netif->group = group;
}

STATIC struct net_group *DoGetNetGroupFromNetif(struct netif *netif)
{
    return netif != NULL ? netif->group : NULL;
}

STATIC VOID DoSetIppcbNetGroup(struct ip_pcb *pcb, struct net_group *group)
{
    pcb->group = group;
}

STATIC struct net_group *DoGetNetGroupFromIppcb(struct ip_pcb *pcb)
{
    return pcb != NULL ? pcb->group : NULL;
}

struct net_group_ops netGroupOps = {
    .get_curr_process_net_group = DoGetNetGroupFromCurrProcess,
    .set_netif_net_group = DoSetNetifNetGroup,
    .get_net_group_from_netif = DoGetNetGroupFromNetif,
    .set_ippcb_net_group = DoSetIppcbNetGroup,
    .get_net_group_from_ippcb = DoGetNetGroupFromIppcb,
};

UINT32 OsInitRootNetContainer(NetContainer **netContainer)
{
    UINT32 intSave;
    NetContainer *newNetContainer = CreateNewNetContainer(NULL);
    if (newNetContainer == NULL) {
        return ENOMEM;
    }

    newNetContainer->group = get_root_net_group();
    set_default_net_group_ops(&netGroupOps);

    SCHEDULER_LOCK(intSave);
    g_currentNetContainerNum++;
    *netContainer = newNetContainer;
    g_rootNetContainer = newNetContainer;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

UINT32 OsGetNetContainerCount(VOID)
{
    return g_currentNetContainerNum;
}
#endif
