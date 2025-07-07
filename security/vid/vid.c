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

#include "vid_type.h"
#include "vid_api.h"
#include "los_memory.h"
/**
 * @brief 初始化进程的定时器ID映射列表
 * @param processCB 进程控制块指针
 * @return LOS_OK 初始化成功; LOS_NOK 初始化失败
 */
UINT32 VidMapListInit(LosProcessCB *processCB)
{
    (void)memset_s(&processCB->timerIdMap, sizeof(TimerIdMap), 0, sizeof(TimerIdMap));  // 清零定时器ID映射结构
    LOS_ListInit(&processCB->timerIdMap.head);  // 初始化映射列表头节点
    processCB->timerIdMap.bitMap = (UINT32*)LOS_MemAlloc(m_aucSysMem0, sizeof(UINT32));  // 分配位图内存
    if (processCB->timerIdMap.bitMap == NULL) {  // 检查内存分配结果
        PRINT_ERR("%s %d, alloc memory failed\n", __FUNCTION__, __LINE__);
        return LOS_NOK;
    }

    processCB->timerIdMap.mapCount = 1;  // 初始化位图计数为1
    (void)memset_s(processCB->timerIdMap.bitMap, sizeof(UINT32), 0, sizeof(UINT32));  // 清零位图内存
    if (LOS_MuxInit(&processCB->timerIdMap.vidMapLock, NULL) != LOS_OK) {  // 初始化互斥锁
        PRINT_ERR("%s %d, Create mutex for vmm failed\n", __FUNCTION__, __LINE__);
        LOS_MemFree(m_aucSysMem0, processCB->timerIdMap.bitMap);  // 释放已分配的位图内存
        processCB->timerIdMap.bitMap = NULL;
        return LOS_NOK;
    }
    return LOS_OK;
}

/**
 * @brief 销毁进程的定时器ID映射列表
 * @param processCB 进程控制块指针
 */
void VidMapDestroy(LosProcessCB *processCB)
{
    TimerIdMapNode *idNode = NULL;
    TimerIdMapNode *idNodeNext = NULL;

    LOS_MuxLock(&processCB->timerIdMap.vidMapLock, LOS_WAIT_FOREVER);  // 加锁保护映射列表操作
    // 遍历并删除所有映射节点
    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(idNode, idNodeNext, &processCB->timerIdMap.head, TimerIdMapNode, node) {
        LOS_ListDelete(&idNode->node);  // 从链表中删除节点
        LOS_MemFree(m_aucSysMem0, idNode);  // 释放节点内存
    }

    LOS_MuxUnlock(&processCB->timerIdMap.vidMapLock);  // 解锁
    LOS_MemFree(m_aucSysMem0, processCB->timerIdMap.bitMap);  // 释放位图内存
    LOS_MuxDestroy(&processCB->timerIdMap.vidMapLock);  // 销毁互斥锁
}

/**
 * @brief 根据VID查找映射节点
 * @param vid 虚拟ID
 * @return 成功返回节点指针; 失败返回NULL
 */
static TimerIdMapNode *FindListNodeByVid(UINT16 vid)
{
    TimerIdMapNode *idNode = NULL;
    LosProcessCB *processCB = OsCurrProcessGet();  // 获取当前进程控制块

    LOS_MuxLock(&processCB->timerIdMap.vidMapLock, LOS_WAIT_FOREVER);  // 加锁保护
    // 遍历链表查找VID匹配的节点
    LOS_DL_LIST_FOR_EACH_ENTRY(idNode, &processCB->timerIdMap.head, TimerIdMapNode, node) {
        if (vid == idNode->vid) {  // 找到匹配节点
            LOS_MuxUnlock(&processCB->timerIdMap.vidMapLock);  // 提前解锁
            return idNode;
        }
    }
    LOS_MuxUnlock(&processCB->timerIdMap.vidMapLock);  // 解锁

    return NULL;
}

/**
 * @brief 根据RID查找映射节点
 * @param rid 真实ID
 * @return 成功返回节点指针; 失败返回NULL
 */
static TimerIdMapNode *FindListNodeByRid(UINT32 rid)
{
    TimerIdMapNode *idNode = NULL;
    LosProcessCB *processCB = OsCurrProcessGet();  // 获取当前进程控制块

    LOS_MuxLock(&processCB->timerIdMap.vidMapLock, LOS_WAIT_FOREVER);  // 加锁保护
    // 遍历链表查找RID匹配的节点
    LOS_DL_LIST_FOR_EACH_ENTRY(idNode, &processCB->timerIdMap.head, TimerIdMapNode, node) {
        if (rid == idNode->rid) {  // 找到匹配节点
            LOS_MuxUnlock(&processCB->timerIdMap.vidMapLock);  // 提前解锁
            return idNode;
        }
    }
    LOS_MuxUnlock(&processCB->timerIdMap.vidMapLock);  // 解锁

    return NULL;
}

/**
 * @brief 获取空闲的虚拟ID
 * @return 成功返回虚拟ID; 失败返回MAX_INVALID_TIMER_VID
 */
static UINT16 GetFreeVid(VOID)
{
    UINT16 i, j;
    UINT32 num;
    UINT32 *tmp = NULL;
    LosProcessCB *processCB = OsCurrProcessGet();  // 获取当前进程控制块
    TimerIdMap *idMap = &processCB->timerIdMap;
    UINT16 mapMaxNum = idMap->mapCount;

    // 遍历位图查找空闲位
    for (i = 0; i < mapMaxNum; i++) {
        num = idMap->bitMap[i];
        for (j = 0; j < INT_BIT_COUNT; j++) {
            if ((num & (1U << j)) == 0) {  // 找到空闲位
                num |= 1U << j;  // 标记为已使用
                idMap->bitMap[i] = num;
                return (INT_BIT_COUNT * i + j);  // 返回计算得到的VID
            }
        }
    }

    /* 扩展位图 */
    mapMaxNum++;
    if (mapMaxNum > VID_MAP_MAX_NUM) {  // 检查是否超过最大允许值
        PRINT_ERR("%s %d, timer vid run out\n", __FUNCTION__, __LINE__);
        return MAX_INVALID_TIMER_VID;
    }

    tmp = (UINT32*)LOS_MemAlloc(m_aucSysMem0, mapMaxNum * sizeof(UINT32));  // 分配新位图内存
    if (tmp == NULL) {
        PRINT_ERR("%s %d, alloc memory failed\n", __FUNCTION__, __LINE__);
        return MAX_INVALID_TIMER_VID;
    }

    (void)memcpy_s(tmp, mapMaxNum * sizeof(UINT32), idMap->bitMap, (mapMaxNum - 1) * sizeof(UINT32));  // 复制旧位图数据
    LOS_MemFree(m_aucSysMem0, idMap->bitMap);  // 释放旧位图内存
    idMap->bitMap = tmp;
    idMap->mapCount = mapMaxNum;
    idMap->bitMap[i] = 1;  // 标记新位图的第一个位为已使用
    return (INT_BIT_COUNT * i);
}

/**
 * @brief 释放虚拟ID
 * @param vid 需要释放的虚拟ID
 */
static VOID ReleaseVid(UINT16 vid)
{
    UINT16 a, b;
    UINT32 *tmpMap = NULL;
    LosProcessCB *processCB = OsCurrProcessGet();  // 获取当前进程控制块
    TimerIdMap *idMap = &processCB->timerIdMap;
    UINT16 mapMaxNum = idMap->mapCount;

    if (vid >= (VID_MAP_MAX_NUM * INT_BIT_COUNT)) {  // 检查VID有效性
        return;
    }

    a = vid >> INT_BIT_SHIFT;  // 计算位图索引
    b = vid & (INT_BIT_COUNT - 1);  // 计算位索引

    idMap->bitMap[a] &= ~(1U << b);  // 清除位标记

    /* 收缩位图 */
    if (mapMaxNum > 1) {
        if (idMap->bitMap[mapMaxNum - 1] == 0) {  // 最后一个位图为空
            mapMaxNum--;
            // 重新分配内存缩小位图
            tmpMap = LOS_MemRealloc(m_aucSysMem0, idMap->bitMap, mapMaxNum * sizeof(UINT32));
            if (tmpMap) {
                idMap->bitMap = tmpMap;
                idMap->mapCount = mapMaxNum;
            }
        }
    }
}

/**
 * @brief 通过RID添加映射节点
 * @param rid 真实ID
 * @return 成功返回虚拟ID; 失败返回MAX_INVALID_TIMER_VID
 */
UINT16 AddNodeByRid(UINT16 rid)
{
    TimerIdMapNode *tmp = NULL;
    LosProcessCB *processCB = OsCurrProcessGet();  // 获取当前进程控制块
    UINT16 vid;

    tmp = FindListNodeByRid(rid);  // 检查RID是否已存在映射
    if (tmp) {
        return tmp->vid;  // 返回已存在的VID
    }

    LOS_MuxLock(&processCB->timerIdMap.vidMapLock, LOS_WAIT_FOREVER);  // 加锁保护
    vid = GetFreeVid();  // 获取空闲VID
    if (vid == MAX_INVALID_TIMER_VID) {
        LOS_MuxUnlock(&processCB->timerIdMap.vidMapLock);  // 解锁
        errno = ENOMEM;
        return MAX_INVALID_TIMER_VID;
    }
    tmp = (TimerIdMapNode *)LOS_MemAlloc(m_aucSysMem0, sizeof(TimerIdMapNode));  // 分配节点内存
    if (tmp == NULL) {
        PRINT_ERR("%s %d, alloc memory failed\n", __FUNCTION__, __LINE__);
        LOS_MuxUnlock(&processCB->timerIdMap.vidMapLock);  // 解锁
        errno = ENOMEM;
        return MAX_INVALID_TIMER_VID;
    }
    tmp->rid = rid;  // 设置真实ID
    tmp->vid = vid;  // 设置虚拟ID

    LOS_ListTailInsert(&processCB->timerIdMap.head, &tmp->node);  // 将节点插入链表尾部
    LOS_MuxUnlock(&processCB->timerIdMap.vidMapLock);  // 解锁

    return vid;
}

/**
 * @brief 通过VID获取RID
 * @param vid 虚拟ID
 * @return 成功返回真实ID; 失败返回0xffff
 */
UINT16 GetRidByVid(UINT16 vid)
{
    TimerIdMapNode *tmp = FindListNodeByVid(vid);  // 查找VID对应的节点
    if (tmp) {
        return tmp->rid;  // 返回RID
    }
    return 0xffff;  // 返回无效值
}

/**
 * @brief 通过VID删除映射节点
 * @param vid 需要删除的虚拟ID
 */
void RemoveNodeByVid(UINT16 vid)
{
    TimerIdMapNode *tmp = NULL;
    LosProcessCB *processCB = OsCurrProcessGet();  // 获取当前进程控制块

    tmp = FindListNodeByVid(vid);  // 查找VID对应的节点
    if (tmp == NULL) {
        return;
    }

    LOS_MuxLock(&processCB->timerIdMap.vidMapLock, LOS_WAIT_FOREVER);  // 加锁保护
    LOS_ListDelete(&tmp->node);  // 从链表中删除节点
    ReleaseVid(tmp->vid);  // 释放VID
    LOS_MuxUnlock(&processCB->timerIdMap.vidMapLock);  // 解锁
    LOS_MemFree(m_aucSysMem0, tmp);  // 释放节点内存

    return;
}