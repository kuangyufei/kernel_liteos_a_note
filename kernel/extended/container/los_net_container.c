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
// 当前网络容器数量  -- 全局变量，用于跟踪系统中网络容器的总数
STATIC UINT32 g_currentNetContainerNum = 0;                                 
// 根网络容器指针    -- 全局变量，指向系统根网络容器实例
STATIC NetContainer *g_rootNetContainer = NULL;                             
                                                                             
/**                                                                          
 * @brief  分配并初始化网络接口结构                                           
 * @return 成功返回分配的netif指针，失败返回NULL                              
 */                                                                          
STATIC struct netif *NetifAlloc(VOID)                                         
{                                                                             
    UINT32 size = sizeof(struct netif);                                      
    // 从系统内存区域1分配netif结构内存                                      
    struct netif *netif = LOS_MemAlloc(m_aucSysMem1, size);                  
    if (netif == NULL) {                                                     
        return NULL;                                                        
    }                                                                        
    // 将分配的内存区域初始化为0                                             
    (VOID)memset_s(netif, size, 0, size);                                    
                                                                             
    return netif;                                                            
}                                                                             
                                                                             
/**                                                                          
 * @brief  释放网络接口结构及其关联资源                                       
 * @param  netif: 待释放的网络接口指针                                       
 */                                                                          
STATIC VOID FreeNetif(struct netif *netif)                                    
{                                                                             
    if (netif != NULL) {                                                     
        // 如果存在对等网络接口，先释放对等接口                               
        if (netif->peer != NULL) {                                           
            netif_remove(netif->peer);  // 从系统中移除对等网络接口          
            (VOID)LOS_MemFree(m_aucSysMem1, netif->peer);  // 释放对等接口内存
        }                                                                    
        netif_remove(netif);  // 从系统中移除当前网络接口                    
        (VOID)LOS_MemFree(m_aucSysMem1, netif);  // 释放当前接口内存         
    }                                                                        
}                                                                             
                                                                             
/**                                                                          
 * @brief  创建虚拟以太网接口                                                 
 * @param  netContainer: 网络容器指针                                         
 * @param  veth: 输出参数，用于返回创建的虚拟以太网接口指针                   
 * @return 成功返回LOS_OK，失败返回错误码                                    
 */                                                                          
STATIC UINT32 CreateVethNetif(NetContainer *netContainer, struct netif **veth)
{                                                                             
    struct netif *netif = NetifAlloc();  // 分配网络接口结构                  
    if (netif == NULL) {                                                     
        return ENOMEM;  // 内存分配失败，返回内存不足错误                    
    }                                                                        
                                                                             
    // 初始化虚拟以太网接口，关联到网络容器组                               
    veth_init(netif, netContainer->group);                                   
                                                                             
    *veth = netif;  // 通过输出参数返回创建的接口                            
    return LOS_OK;                                                           
}                                                                             
                                                                             
/**                                                                          
 * @brief  初始化虚拟以太网接口对                                             
 * @param  netContainer: 网络容器指针                                         
 * @return 成功返回LOS_OK，失败返回错误码                                    
 */                                                                          
STATIC UINT32 InitVethPair(NetContainer *netContainer)                        
{                                                                             
    struct netif *vethOfNetContainer = NULL;  // 容器侧veth接口               
    struct netif *vethOfRootNetContainer = NULL;  // 根容器侧veth接口         
                                                                             
    // 创建容器侧虚拟以太网接口                                              
    UINT32 ret = CreateVethNetif(netContainer, &vethOfNetContainer);         
    if (ret != LOS_OK) {                                                     
        return ret;  // 创建失败，返回错误码                                 
    }                                                                        
                                                                             
    // 创建根容器侧虚拟以太网接口                                            
    ret = CreateVethNetif(g_rootNetContainer, &vethOfRootNetContainer);      
    if (ret != LOS_OK) {                                                     
        FreeNetif(vethOfNetContainer);  // 创建失败，释放已分配的容器侧接口   
        return ret;                                                         
    }                                                                        
                                                                             
    // 建立两个veth接口之间的对等关系                                        
    vethOfNetContainer->peer = vethOfRootNetContainer;                       
    vethOfRootNetContainer->peer = vethOfNetContainer;                       
    return LOS_OK;                                                           
}                                                                             
                                                                             
/**                                                                          
 * @brief  初始化回环网络接口                                                 
 * @param  netContainer: 网络容器指针                                         
 * @return 成功返回LOS_OK，失败返回错误码                                    
 */                                                                          
STATIC UINT32 InitLoopNetif(NetContainer *netContainer)                       
{                                                                             
    // 分配回环网络接口结构                                                  
    struct netif *loop_netif = NetifAlloc();                                 
    if (loop_netif == NULL) {                                                
        return ENOMEM;  // 内存分配失败                                      
    }                                                                        
                                                                             
    // 将回环接口关联到网络容器组                                           
    netContainer->group->loop_netif = loop_netif;                            
    // 初始化网络接口                                                       
    netif_init(netContainer->group);                                         
    return LOS_OK;                                                           
}                                                                             
                                                                             
/**                                                                          
 * @brief  初始化容器网络接口（回环接口+虚拟以太网接口对）                    
 * @param  netContainer: 网络容器指针                                         
 * @return 成功返回LOS_OK，失败返回错误码                                    
 */                                                                          
STATIC UINT32 InitContainerNetifs(NetContainer *netContainer)                 
{                                                                             
    // 先初始化回环接口                                                      
    UINT32 ret = InitLoopNetif(netContainer);                                
    if (ret != LOS_OK) {                                                     
        return ret;  // 回环接口初始化失败，返回错误                         
    }                                                                        
    // 再初始化veth接口对                                                    
    return InitVethPair(netContainer);                                       
}                                                                             
                                                                             
/**                                                                          
 * @brief  初始化网络容器组                                                   
 * @param  netContainer: 网络容器指针                                         
 * @return 成功返回LOS_OK，失败返回错误码                                    
 */                                                                          
STATIC UINT32 InitNetGroup(NetContainer *netContainer)                        
{                                                                             
    UINT32 size = sizeof(struct net_group);                                  
    // 分配网络组结构内存                                                    
    struct net_group *group = LOS_MemAlloc(m_aucSysMem1, size);              
    if (group == NULL) {                                                     
        return ENOMEM;  // 内存分配失败                                      
    }                                                                        
    // 初始化网络组结构                                                     
    (VOID)memset_s(group, size, 0, size);                                    
                                                                             
    netContainer->group = group;  // 将网络组关联到网络容器                   
    return LOS_OK;                                                           
}                                                                             
                                                                             
/**                                                                          
 * @brief  释放网络容器组资源                                                 
 * @param  group: 网络组指针                                                 
 */                                                                          
STATIC VOID FreeNetContainerGroup(struct net_group *group)                    
{                                                                             
    if (group == NULL) {                                                     
        return;  // 空指针检查                                              
    }                                                                        
                                                                             
    struct netif *freeNetif = group->netif_list;  // 从网络接口列表头开始     
    struct netif *nextNetif = NULL;  // 用于保存下一个接口指针                
                                                                             
    // 遍历释放所有网络接口                                                  
    while (freeNetif != NULL) {                                              
        nextNetif = freeNetif->next;  // 先保存下一个接口指针                
        FreeNetif(freeNetif);  // 释放当前接口                               
        freeNetif = nextNetif;  // 移动到下一个接口                          
    }                                                                        
}                                                                             
                                                                             
/**                                                                          
 * @brief  销毁网络容器                                                       
 * @param  container: 容器指针                                               
 */                                                                          
VOID OsNetContainerDestroy(Container *container)                              
{                                                                             
    UINT32 intSave;                                                          
    if (container == NULL) {                                                 
        return;  // 空指针检查                                              
    }                                                                        
                                                                             
    SCHEDULER_LOCK(intSave);  // 关闭调度器，防止并发访问                     
    NetContainer *netContainer = container->netContainer;                    
    if (netContainer == NULL) {                                              
        SCHEDULER_UNLOCK(intSave);  // 无网络容器，解锁调度器                 
        return;                                                             
    }                                                                        
                                                                             
    // 减少引用计数                                                          
    LOS_AtomicDec(&netContainer->rc);                                        
    // 引用计数大于0，仍有使用者，不销毁                                    
    if (LOS_AtomicRead(&netContainer->rc) > 0) {                             
        SCHEDULER_UNLOCK(intSave);  // 解锁调度器                            
        return;                                                             
    }                                                                        
                                                                             
    g_currentNetContainerNum--;  // 网络容器总数减1                          
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器                                
    FreeNetContainerGroup(netContainer->group);  // 释放网络组资源           
    container->netContainer = NULL;  // 清除容器的网络容器指针                
    (VOID)LOS_MemFree(m_aucSysMem1, netContainer->group);  // 释放网络组内存 
    (VOID)LOS_MemFree(m_aucSysMem1, netContainer);  // 释放网络容器内存      
    return;                                                                  
}                                                                             
                                                                             
/**                                                                          
 * @brief  创建新的网络容器                                                   
 * @param  parent: 父网络容器指针，NULL表示创建根容器                         
 * @return 成功返回网络容器指针，失败返回NULL                                 
 */                                                                          
STATIC NetContainer *CreateNewNetContainer(NetContainer *parent)              
{                                                                             
    // 分配网络容器结构内存                                                  
    NetContainer *netContainer = LOS_MemAlloc(m_aucSysMem1, sizeof(NetContainer));
    if (netContainer == NULL) {                                              
        return NULL;  // 内存分配失败                                        
    }                                                                        
    // 初始化网络容器结构                                                   
    (VOID)memset_s(netContainer, sizeof(NetContainer), 0, sizeof(NetContainer));
                                                                             
    // 分配容器ID                                                           
    netContainer->containerID = OsAllocContainerID();                        
                                                                             
    if (parent != NULL) {                                                    
        // 非根容器，初始引用计数为1                                         
        LOS_AtomicSet(&netContainer->rc, 1);                                 
    } else {                                                                 
        // 根容器，初始引用计数为3（对应3个系统进程）                        
        LOS_AtomicSet(&netContainer->rc, 3); /* 3: Three system processes */ 
    }                                                                        
    return netContainer;                                                     
}                                                                             
                                                                             
/**                                                                          
 * @brief  创建并初始化网络容器                                               
 * @param  container: 容器指针                                               
 * @param  parentContainer: 父网络容器指针                                   
 * @return 成功返回LOS_OK，失败返回错误码                                    
 */                                                                          
STATIC UINT32 CreateNetContainer(Container *container, NetContainer *parentContainer)
{                                                                             
    UINT32 intSave, ret;                                                     
    // 创建新网络容器实例                                                   
    NetContainer *newNetContainer = CreateNewNetContainer(parentContainer);  
    if (newNetContainer == NULL) {                                           
        return ENOMEM;  // 创建失败，返回内存不足错误                        
    }                                                                        
                                                                             
    // 初始化网络组                                                         
    ret = InitNetGroup(newNetContainer);                                     
    if (ret != LOS_OK) {                                                     
        (VOID)LOS_MemFree(m_aucSysMem1, newNetContainer);  // 初始化失败，释放容器
        return ret;                                                         
    }                                                                        
                                                                             
    // 初始化容器网络接口                                                   
    ret = InitContainerNetifs(newNetContainer);                              
    if (ret != LOS_OK) {                                                     
        // 接口初始化失败，释放已分配资源                                    
        (VOID)LOS_MemFree(m_aucSysMem1, newNetContainer->group);             
        (VOID)LOS_MemFree(m_aucSysMem1, newNetContainer);                    
        return ret;                                                         
    }                                                                        
                                                                             
    SCHEDULER_LOCK(intSave);  // 关闭调度器                                  
    g_currentNetContainerNum++;  // 网络容器总数加1                          
    container->netContainer = newNetContainer;  // 关联网络容器到容器        
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器                                
    return LOS_OK;                                                           
}                                                                             
                                                                             
/**                                                                          
 * @brief  复制网络容器                                                       
 * @param  flags: 克隆标志位                                                 
 * @param  child: 子进程控制块                                               
 * @param  parent: 父进程控制块                                               
 * @return 成功返回LOS_OK，失败返回错误码                                    
 */                                                                          
UINT32 OsCopyNetContainer(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent)
{                                                                             
    UINT32 intSave;                                                          
    // 获取父进程的网络容器                                                  
    NetContainer *currNetContainer = parent->container->netContainer;        
                                                                             
    // 检查是否需要创建新的网络命名空间                                      
    if (!(flags & CLONE_NEWNET)) {                                           
        SCHEDULER_LOCK(intSave);  // 关闭调度器                              
        LOS_AtomicInc(&currNetContainer->rc);  // 增加引用计数                
        // 子进程共享父进程的网络容器                                        
        child->container->netContainer = currNetContainer;                   
        SCHEDULER_UNLOCK(intSave);  // 解锁调度器                            
        return LOS_OK;                                                       
    }                                                                        
                                                                             
    // 检查网络容器数量限制                                                  
    if (OsContainerLimitCheck(NET_CONTAINER, &g_currentNetContainerNum) != LOS_OK) {
        return EPERM;  // 超过限制，返回权限错误                             
    }                                                                        
                                                                             
    // 创建新的网络容器                                                      
    return CreateNetContainer(child->container, currNetContainer);           
}                                                                             
                                                                             
/**                                                                          
 * @brief  取消共享网络容器                                                   
 * @param  flags: 标志位                                                     
 * @param  curr: 当前进程控制块                                               
 * @param  newContainer: 新容器指针                                           
 * @return 成功返回LOS_OK，失败返回错误码                                    
 */                                                                          
UINT32 OsUnshareNetContainer(UINTPTR flags, LosProcessCB *curr, Container *newContainer)
{                                                                             
    UINT32 intSave;                                                          
    // 获取当前进程的网络容器                                                
    NetContainer *parentContainer = curr->container->netContainer;           
                                                                             
    // 检查是否需要创建新的网络命名空间                                      
    if (!(flags & CLONE_NEWNET)) {                                           
        SCHEDULER_LOCK(intSave);  // 关闭调度器                              
        newContainer->netContainer = parentContainer;  // 共享原网络容器     
        LOS_AtomicInc(&parentContainer->rc);  // 增加引用计数                
        SCHEDULER_UNLOCK(intSave);  // 解锁调度器                            
        return LOS_OK;                                                       
    }                                                                        
                                                                             
    // 检查网络容器数量限制                                                  
    if (OsContainerLimitCheck(NET_CONTAINER, &g_currentNetContainerNum) != LOS_OK) {
        return EPERM;  // 超过限制，返回权限错误                             
    }                                                                        
                                                                             
    // 创建新的网络容器                                                      
    return CreateNetContainer(newContainer, parentContainer);                
}                                                                             
                                                                             
/**                                                                          
 * @brief  设置网络命名空间                                                   
 * @param  flags: 标志位                                                     
 * @param  container: 目标容器指针                                           
 * @param  newContainer: 新容器指针                                           
 * @return 成功返回LOS_OK，失败返回错误码                                    
 */                                                                          
UINT32 OsSetNsNetContainer(UINT32 flags, Container *container, Container *newContainer)
{                                                                             
    // 检查是否需要设置新的网络命名空间                                      
    if (flags & CLONE_NEWNET) {                                              
        // 关联目标容器的网络容器                                            
        newContainer->netContainer = container->netContainer;                
        // 增加网络容器引用计数                                              
        LOS_AtomicInc(&container->netContainer->rc);                         
        return LOS_OK;                                                       
    }                                                                        
                                                                             
    // 不设置新网络命名空间，使用当前进程的网络容器                          
    newContainer->netContainer = OsCurrProcessGet()->container->netContainer;
    LOS_AtomicInc(&newContainer->netContainer->rc);  // 增加引用计数         
    return LOS_OK;                                                           
}                                                                             
                                                                             
/**                                                                          
 * @brief  获取网络容器ID                                                     
 * @param  netContainer: 网络容器指针                                         
 * @return 成功返回容器ID，失败返回OS_INVALID_VALUE                          
 */                                                                          
UINT32 OsGetNetContainerID(NetContainer *netContainer)                        
{                                                                             
    if (netContainer == NULL) {                                              
        return OS_INVALID_VALUE;  // 空指针检查，返回无效值                  
    }                                                                        
                                                                             
    return netContainer->containerID;  // 返回容器ID                          
}                                                                             
                                                                             
/**                                                                          
 * @brief  从当前进程获取网络组                                               
 * @return 返回当前进程的网络组指针                                          
 */                                                                          
STATIC struct net_group *DoGetNetGroupFromCurrProcess(VOID)                   
{                                                                             
    LosProcessCB *processCB = OsCurrProcessGet();  // 获取当前进程控制块      
    NetContainer *netContainer = processCB->container->netContainer;  // 获取网络容器
    return netContainer->group;  // 返回网络组                               
}                                                                             
                                                                             
/**                                                                          
 * @brief  设置网络接口的网络组                                               
 * @param  netif: 网络接口指针                                               
 * @param  group: 网络组指针                                                 
 */                                                                          
STATIC VOID DoSetNetifNetGroup(struct netif *netif, struct net_group *group)  
{                                                                             
    netif->group = group;  // 关联网络接口到网络组                           
}                                                                             
                                                                             
/**                                                                          
 * @brief  从网络接口获取网络组                                               
 * @param  netif: 网络接口指针                                               
 * @return 返回网络接口所属的网络组指针，若netif为NULL则返回NULL              
 */                                                                          
STATIC struct net_group *DoGetNetGroupFromNetif(struct netif *netif)          
{                                                                             
    return netif != NULL ? netif->group : NULL;  // 安全获取网络组指针        
}                                                                             
                                                                             
/**                                                                          
 * @brief  设置IP控制块的网络组                                               
 * @param  pcb: IP控制块指针                                                 
 * @param  group: 网络组指针                                                 
 */                                                                          
STATIC VOID DoSetIppcbNetGroup(struct ip_pcb *pcb, struct net_group *group)   
{                                                                             
    pcb->group = group;  // 关联IP控制块到网络组                             
}                                                                             
                                                                             
/**                                                                          
 * @brief  从IP控制块获取网络组                                               
 * @param  pcb: IP控制块指针                                                 
 * @return 返回IP控制块所属的网络组指针，若pcb为NULL则返回NULL                
 */                                                                          
STATIC struct net_group *DoGetNetGroupFromIppcb(struct ip_pcb *pcb)           
{                                                                             
    return pcb != NULL ? pcb->group : NULL;  // 安全获取网络组指针            
}                                                                             
                                                                             
// 网络组操作函数集 -- 定义网络组相关的操作接口                              
struct net_group_ops netGroupOps = {                                          
    .get_curr_process_net_group = DoGetNetGroupFromCurrProcess,  // 获取当前进程网络组
    .set_netif_net_group = DoSetNetifNetGroup,  // 设置网络接口网络组         
    .get_net_group_from_netif = DoGetNetGroupFromNetif,  // 从网络接口获取网络组
    .set_ippcb_net_group = DoSetIppcbNetGroup,  // 设置IP控制块网络组         
    .get_net_group_from_ippcb = DoGetNetGroupFromIppcb,  // 从IP控制块获取网络组
};                                                                            
                                                                             
/**                                                                          
 * @brief  初始化根网络容器                                                   
 * @param  netContainer: 输出参数，用于返回根网络容器指针                     
 * @return 成功返回LOS_OK，失败返回错误码                                    
 */                                                                          
UINT32 OsInitRootNetContainer(NetContainer **netContainer)                    
{                                                                             
    UINT32 intSave;                                                          
    // 创建新的网络容器（父容器为NULL，表示根容器）                          
    NetContainer *newNetContainer = CreateNewNetContainer(NULL);             
    if (newNetContainer == NULL) {                                           
        return ENOMEM;  // 创建失败，返回内存不足错误                        
    }                                                                        
                                                                             
    // 设置根网络组                                                         
    newNetContainer->group = get_root_net_group();                           
    // 设置默认网络组操作函数集                                             
    set_default_net_group_ops(&netGroupOps);                                 
                                                                             
    SCHEDULER_LOCK(intSave);  // 关闭调度器                                  
    g_currentNetContainerNum++;  // 网络容器总数加1                          
    *netContainer = newNetContainer;  // 通过输出参数返回根网络容器           
    g_rootNetContainer = newNetContainer;  // 设置全局根网络容器指针          
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器                                
    return LOS_OK;                                                           
}                                                                             
                                                                             
/**                                                                          
 * @brief  获取网络容器数量                                                   
 * @return 返回当前网络容器总数                                              
 */                                                                          
UINT32 OsGetNetContainerCount(VOID)                                           
{                                                                             
    return g_currentNetContainerNum;  // 返回全局网络容器计数变量             
}                                                                             
#endif
