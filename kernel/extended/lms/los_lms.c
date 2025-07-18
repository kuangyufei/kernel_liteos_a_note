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
 
/*!
 * @file  los_lms.c
 * @brief 检测内存主文件
 * @link
   @verbatim
    基本概念
		LMS全称为Lite Memory Sanitizer，是一种实时检测内存操作合法性的调测工具。LMS能够实时检测缓冲区溢出（buffer overflow），
		释放后使用（use after free) 和重复释放（double Free), 在异常发生的第一时间通知操作系统，结合backtrace等定位手段，
		能准确定位到产生内存问题的代码行，极大提升内存问题定位效率。
		OpenHarmony LiteOS-M内核的LMS模块提供下面几种功能：
		支持多内存池检测。
		支持LOS_MemAlloc、LOS_MemAllocAlign、LOS_MemRealloc申请出的内存检测。
		支持安全函数的访问检测（默认开启）。
		支持libc 高频函数的访问检测，包括：memset、memcpy、memmove、strcat、strcpy、strncat、strncpy。
	运行机制
		LMS使用影子内存映射标记系统内存的状态，一共可标记为三个状态：可读写，不可读写，已释放。影子内存存放在内存池的尾部。
		内存从堆上申请后，会将数据区的影子内存设置为“可读写”状态，并将头结点区的影子内存设置为“不可读写”状态。
		内存在堆上被释放时，会将被释放内存的影子内存设置为“已释放”状态。
		编译代码时，会在代码中的读写指令前插入检测函数，对地址的合法性进行检验。主要是检测访问内存的影子内存的状态值，
		若检测到影子内存为不可读写，则会报溢出错误；若检测到影子内存为已释放，则会报释放后使用错误。
		在内存释放时，会检测被释放地址的影子内存状态值，若检测到影子内存非可读写，则会报重复释放错误。
   @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2025-07-18
 */

#include "los_lms_pri.h"
#include "los_spinlock.h"
#include "los_exc.h"
#include "los_sched_pri.h"
#include "los_atomic.h"
#include "los_init.h"

/**
 * @brief LMS检查池数组，用于管理多个内存检查池
 * @note 静态数组，大小由LOSCFG_LMS_MAX_RECORD_POOL_NUM配置
 */
LITE_OS_SEC_BSS STATIC LmsMemListNode g_lmsCheckPoolArray[LOSCFG_LMS_MAX_RECORD_POOL_NUM];

/**
 * @brief LMS检查池链表头，用于维护活跃的内存检查池
 */
LITE_OS_SEC_BSS STATIC LOS_DL_LIST g_lmsCheckPoolList;

/**
 * @brief 检查深度计数器，用于跟踪内存检查的嵌套层级
 */
STATIC Atomic g_checkDepth = 0;

/**
 * @brief LMS钩子函数指针，指向内存检查相关的回调函数集合
 */
LmsHook *g_lms = NULL;

/**
 * @brief LMS模块自旋锁，用于保护共享数据结构的并发访问
 */
LITE_OS_SEC_BSS SPIN_LOCK_INIT(g_lmsSpin);

/**
 * @brief 获取LMS自旋锁并保存中断状态
 * @param state 用于保存中断状态的变量
 */
#define LMS_LOCK(state)                 LOS_SpinLockSave(&g_lmsSpin, &(state))

/**
 * @brief 释放LMS自旋锁并恢复中断状态
 * @param state 之前保存的中断状态
 */
#define LMS_UNLOCK(state)               LOS_SpinUnlockRestore(&g_lmsSpin, (state))

/**
 * @brief 将数值按指定对齐大小向下对齐
 * @param value 待对齐的数值
 * @param align 对齐大小
 * @return 对齐后的数值
 */
#define OS_MEM_ALIGN_BACK(value, align) (((UINT32)(value)) & ~((UINT32)((align) - 1)))

/**
 * @brief 内存对齐大小，等于指针类型的大小
 */
#define OS_MEM_ALIGN_SIZE               sizeof(UINTPTR)

/**
 * @brief 内存池地址对齐大小，64字节
 */
#define POOL_ADDR_ALIGNSIZE             64

/**
 * @brief LMS检查池未使用状态
 */
#define LMS_POOL_UNUSED                 0

/**
 * @brief LMS检查池已使用状态
 */
#define LMS_POOL_USED                   1

/**
 * @brief 无效的影子值
 */
#define INVALID_SHADOW_VALUE            0xFFFFFFFF

/**
 * @brief 调整内存池大小，按POOL_ADDR_ALIGNSIZE对齐
 * @param size 原始内存池大小
 * @return 调整后的内存池大小
 */
STATIC UINT32 OsLmsPoolResize(UINT32 size)
{
    return OS_MEM_ALIGN_BACK(LMS_POOL_RESIZE(size), POOL_ADDR_ALIGNSIZE);  // 按64字节对齐调整大小
}

/**
 * @brief 根据内存池地址查找对应的LmsMemListNode节点
 * @param pool 内存池起始地址
 * @return 找到的节点指针，未找到返回NULL
 */
STATIC LmsMemListNode *OsLmsGetPoolNode(const VOID *pool)
{
    UINTPTR poolAddr = (UINTPTR)pool;                                      // 将指针转换为无符号整数地址
    LmsMemListNode *current = NULL;                                        // 当前遍历节点指针
    LOS_DL_LIST *listHead = &g_lmsCheckPoolList;                           // 链表头指针

    if (LOS_ListEmpty(&g_lmsCheckPoolList)) {                              // 检查链表是否为空
        goto EXIT;                                                         // 链表为空，跳转到EXIT
    }

    LOS_DL_LIST_FOR_EACH_ENTRY(current, listHead, LmsMemListNode, node) {  // 遍历链表中的所有节点
        if (current->poolAddr == poolAddr) {                               // 比较节点的内存池地址与目标地址
            return current;                                                // 找到匹配节点，返回该节点
        }
    }

EXIT:
    return NULL;                                                           // 未找到匹配节点，返回NULL
}

/**
 * @brief 根据内存地址查找包含该地址的内存池节点
 * @param addr 待查找的内存地址
 * @return 包含该地址的内存池节点，未找到返回NULL
 */
STATIC LmsMemListNode *OsLmsGetPoolNodeFromAddr(UINTPTR addr)
{
    LmsMemListNode *current = NULL;                                        // 当前遍历节点指针
    LmsMemListNode *previous = NULL;                                       // 上一个符合条件的节点指针
    LOS_DL_LIST *listHead = &g_lmsCheckPoolList;                           // 链表头指针

    if (LOS_ListEmpty(&g_lmsCheckPoolList)) {                              // 检查链表是否为空
        return NULL;                                                       // 链表为空，返回NULL
    }

    LOS_DL_LIST_FOR_EACH_ENTRY(current, listHead, LmsMemListNode, node) {  // 遍历链表中的所有节点
        if ((addr < current->poolAddr) || (addr >= (current->poolAddr + current->poolSize))) {  // 检查地址是否在当前节点的内存池范围内
            continue;                                                     // 不在范围内，继续下一个节点
        }
        if ((previous == NULL) ||                                          // 如果previous为空，或当前节点是更精确的包含关系
            ((previous->poolAddr <= current->poolAddr) &&
            ((current->poolAddr + current->poolSize) <= (previous->poolAddr + previous->poolSize)))) {
            previous = current;                                            // 更新previous为当前节点
        }
    }

    return previous;                                                       // 返回找到的最精确节点
}

/**
 * @brief 创建一个新的LMS检查池节点
 * @return 新创建的节点指针，创建失败返回NULL
 */
STATIC LmsMemListNode *OsLmsCheckPoolCreate(VOID)
{
    UINT32 i;                                                              // 循环计数器
    LmsMemListNode *current = NULL;                                        // 当前节点指针
    for (i = 0; i < LOSCFG_LMS_MAX_RECORD_POOL_NUM; i++) {                 // 遍历检查池数组
        current = &g_lmsCheckPoolArray[i];                                 // 获取数组中的当前节点
        if (current->used == LMS_POOL_UNUSED) {                            // 检查节点是否未使用
            current->used = LMS_POOL_USED;                                 // 标记节点为已使用
            return current;                                                // 返回新创建的节点
        }
    }
    return NULL;                                                           // 所有节点都已使用，返回NULL
}

/**
 * @brief 向LMS检查列表添加一个内存池
 * @param pool 内存池起始地址
 * @param size 内存池大小
 * @return 调整后的内存池大小，失败返回0
 */
UINT32 LOS_LmsCheckPoolAdd(const VOID *pool, UINT32 size)
{
    UINT32 intSave;                                                        // 用于保存中断状态
    UINTPTR poolAddr = (UINTPTR)pool;                                      // 内存池地址
    UINT32 realSize;                                                       // 调整后的内存池大小
    LmsMemListNode *lmsPoolNode = NULL;                                    // LMS检查池节点指针

    if (pool == NULL) {                                                    // 检查内存池地址是否为空
        return 0;                                                          // 地址为空，返回0
    }

    LMS_LOCK(intSave);                                                     // 获取LMS自旋锁

    lmsPoolNode = OsLmsGetPoolNode(pool);                                  // 查找内存池是否已在检查列表中
    if (lmsPoolNode != NULL) { /* 如果内存池已在检查列表中 */
        /* 重新初始化同一个内存池，可能大小不同 */
        /* 删除旧节点，然后添加新节点 */
        lmsPoolNode->used = LMS_POOL_UNUSED;                               // 标记旧节点为未使用
        LOS_ListDelete(&(lmsPoolNode->node));                              // 从链表中删除旧节点
    }

    lmsPoolNode = OsLmsCheckPoolCreate();                                  // 创建新的检查池节点
    if (lmsPoolNode == NULL) {                                             // 检查节点创建是否成功
        PRINT_DEBUG("[LMS]the num of lms check pool is max already !\n");  // 打印调试信息：检查池数量已达上限
        LMS_UNLOCK(intSave);                                               // 释放LMS自旋锁
        return 0;                                                          // 返回0表示失败
    }
    realSize = OsLmsPoolResize(size);                                      // 调整内存池大小

    lmsPoolNode->poolAddr = poolAddr;                                      // 设置节点的内存池地址
    lmsPoolNode->poolSize = realSize;                                      // 设置节点的内存池大小
    lmsPoolNode->shadowStart = (UINTPTR)poolAddr + realSize;               // 计算影子内存起始地址
    lmsPoolNode->shadowSize = poolAddr + size - lmsPoolNode->shadowStart;  // 计算影子内存大小
    /* 初始化影子值 */
    (VOID)memset_s((VOID *)lmsPoolNode->shadowStart, lmsPoolNode->shadowSize,  // 使用memset_s初始化影子内存
                   LMS_SHADOW_AFTERFREE_U8, lmsPoolNode->shadowSize);

    LOS_ListAdd(&g_lmsCheckPoolList, &(lmsPoolNode->node));                 // 将新节点添加到检查池链表

    LMS_UNLOCK(intSave);                                                   // 释放LMS自旋锁
    return realSize;                                                       // 返回调整后的内存池大小
}

/**
 * @brief 从LMS检查列表中删除一个内存池
 * @param pool 要删除的内存池起始地址
 */
VOID LOS_LmsCheckPoolDel(const VOID *pool)
{
    UINT32 intSave;                                                        // 用于保存中断状态
    if (pool == NULL) {                                                    // 检查内存池地址是否为空
        return;                                                           // 地址为空，直接返回
    }

    LMS_LOCK(intSave);                                                     // 获取LMS自旋锁
    LmsMemListNode *delNode = OsLmsGetPoolNode(pool);                      // 查找要删除的节点
    if (delNode == NULL) {                                                 // 检查节点是否存在
        PRINT_ERR("[LMS]pool %p is not on lms checklist !\n", pool);      // 打印错误信息：内存池不在检查列表中
        goto Release;                                                     // 跳转到释放锁的标签
    }
    delNode->used = LMS_POOL_UNUSED;                                       // 标记节点为未使用
    LOS_ListDelete(&(delNode->node));                                      // 从链表中删除节点
Release:
    LMS_UNLOCK(intSave);                                                   // 释放LMS自旋锁
}

/**
 * @brief 初始化LMS模块
 * @return 初始化结果，LOS_OK表示成功
 */
STATIC UINT32 OsLmsInit(VOID)
{
    (VOID)memset_s(g_lmsCheckPoolArray, sizeof(g_lmsCheckPoolArray), 0, sizeof(g_lmsCheckPoolArray));  // 初始化检查池数组
    LOS_ListInit(&g_lmsCheckPoolList);                                     // 初始化检查池链表
    static LmsHook hook = {                                                // 定义静态LMS钩子结构体
        .init = LOS_LmsCheckPoolAdd,                                       // 初始化回调函数
        .deInit = LOS_LmsCheckPoolDel,                                     // 去初始化回调函数
        .mallocMark = OsLmsLosMallocMark,                                  // 内存分配标记回调函数
        .freeMark = OsLmsLosFreeMark,                                      // 内存释放标记回调函数
        .simpleMark = OsLmsSimpleMark,                                     // 简单标记回调函数
        .check = OsLmsCheckValid,                                          // 内存检查回调函数
    };
    g_lms = &hook;                                                         // 设置全局LMS钩子指针
    return LOS_OK;                                                         // 返回初始化成功
}

/**
 * @brief 将内存地址转换为影子内存地址和偏移量
 * @param node LMS内存池节点
 * @param memAddr 内存地址
 * @param shadowAddr 输出参数，影子内存地址
 * @param shadowOffset 输出参数，影子内存偏移量
 * @return 转换结果，LOS_OK表示成功，LOS_NOK表示失败
 */
STATIC INLINE UINT32 OsLmsMem2Shadow(LmsMemListNode *node, UINTPTR memAddr, UINTPTR *shadowAddr, UINT32 *shadowOffset)
{
    if ((memAddr < node->poolAddr) || (memAddr >= node->poolAddr + node->poolSize)) { /* 检查指针是否有效 */
        PRINT_ERR("[LMS]memAddr %p is not in pool region [%p, %p)\n", memAddr, node->poolAddr,  // 打印错误信息：内存地址不在池范围内
            node->poolAddr + node->poolSize);
        return LOS_NOK;                                                    // 返回失败
    }

    UINT32 memOffset = memAddr - node->poolAddr;                           // 计算内存地址在池中的偏移量
    *shadowAddr = node->shadowStart + memOffset / LMS_SHADOW_U8_REFER_BYTES;  // 计算影子内存地址
    *shadowOffset = ((memOffset % LMS_SHADOW_U8_REFER_BYTES) / LMS_SHADOW_U8_CELL_NUM) *  // 计算影子内存偏移量
        LMS_SHADOW_BITS_PER_CELL; /* (memOffset % 16) / 4 */
    return LOS_OK;                                                         // 返回成功
}

/**
 * @brief 获取内存地址对应的影子信息
 * @param node LMS内存池节点
 * @param memAddr 内存地址
 * @param info 输出参数，影子信息结构体
 */
STATIC INLINE VOID OsLmsGetShadowInfo(LmsMemListNode *node, UINTPTR memAddr, LmsAddrInfo *info)
{
    UINTPTR shadowAddr;                                                    // 影子内存地址
    UINT32 shadowOffset;                                                   // 影子内存偏移量
    UINT32 shadowValue;                                                    // 影子值

    if (OsLmsMem2Shadow(node, memAddr, &shadowAddr, &shadowOffset) != LOS_OK) {  // 转换内存地址为影子地址
        return;                                                           // 转换失败，直接返回
    }

    shadowValue = ((*(UINT8 *)shadowAddr) >> shadowOffset) & LMS_SHADOW_MASK;  // 计算影子值
    info->memAddr = memAddr;                                               // 设置内存地址
    info->shadowAddr = shadowAddr;                                         // 设置影子内存地址
    info->shadowOffset = shadowOffset;                                     // 设置影子内存偏移量
    info->shadowValue = shadowValue;                                       // 设置影子值
}

/**
 * @brief 设置影子内存的值
 * @param node LMS内存池节点
 * @param startAddr 起始内存地址
 * @param endAddr 结束内存地址（不包含）
 * @param value 要设置的影子值
 */
VOID OsLmsSetShadowValue(LmsMemListNode *node, UINTPTR startAddr, UINTPTR endAddr, UINT8 value)
{
    UINTPTR shadowStart;                                                   // 影子内存起始地址
    UINTPTR shadowEnd;                                                     // 影子内存结束地址
    UINT32 startOffset;                                                    // 起始偏移量
    UINT32 endOffset;                                                      // 结束偏移量

    UINT8 shadowValueMask;                                                 // 影子值掩码
    UINT8 shadowValue;                                                     // 影子值

    /* endAddr -1，然后我们将[startAddr, endAddr)标记为value */
    if (OsLmsMem2Shadow(node, startAddr, &shadowStart, &startOffset) ||    // 转换起始地址
        OsLmsMem2Shadow(node, endAddr - 1, &shadowEnd, &endOffset)) {      // 转换结束地址-1
        return;                                                           // 转换失败，直接返回
    }

    if (shadowStart == shadowEnd) { /* 在同一个字节内 */
        /* 因为endAddr - 1，结束偏移量落在前一个单元，所以需要计算endOffset + 2 */
        shadowValueMask = LMS_SHADOW_MASK_U8;                              // 初始化影子值掩码
        shadowValueMask =                                                  // 计算掩码，仅覆盖[startOffset, endOffset]范围
            (shadowValueMask << startOffset) & (~(shadowValueMask << (endOffset + LMS_SHADOW_BITS_PER_CELL)));
        shadowValue = value & shadowValueMask;                             // 计算要设置的影子值
        *(UINT8 *)shadowStart &= ~shadowValueMask;                         // 清除原有影子值
        *(UINT8 *)shadowStart |= shadowValue;                              // 设置新的影子值
    } else {
        /* 调整startAddr向左，直到到达一个字节的开始 */
        if (startOffset > 0) {
            shadowValueMask = LMS_SHADOW_MASK_U8;                          // 初始化影子值掩码
            shadowValueMask = shadowValueMask << startOffset;              // 左移到起始偏移量
            shadowValue = value & shadowValueMask;                         // 计算要设置的影子值
            *(UINT8 *)shadowStart &= ~shadowValueMask;                     // 清除原有影子值
            *(UINT8 *)shadowStart |= shadowValue;                          // 设置新的影子值
            shadowStart += 1;                                              // 影子起始地址加1
        }

        /* 调整endAddr向右，直到到达一个字节的结束 */
        if (endOffset < (LMS_SHADOW_U8_CELL_NUM - 1) * LMS_SHADOW_BITS_PER_CELL) {
            shadowValueMask = LMS_SHADOW_MASK_U8;                          // 初始化影子值掩码
            shadowValueMask &= ~(shadowValueMask << (endOffset + LMS_SHADOW_BITS_PER_CELL));  // 计算掩码
            shadowValue = value & shadowValueMask;                         // 计算要设置的影子值
            *(UINT8 *)shadowEnd &= ~shadowValueMask;                       // 清除原有影子值
            *(UINT8 *)shadowEnd |= shadowValue;                            // 设置新的影子值
            shadowEnd -= 1;                                                // 影子结束地址减1
        }

        if (shadowEnd + 1 > shadowStart) {                                 // 如果还有完整字节需要设置
            (VOID)memset((VOID *)shadowStart, value & LMS_SHADOW_MASK_U8, shadowEnd + 1 - shadowStart);  // 使用memset设置连续字节
        }
    }
}

/**
 * @brief 获取内存地址对应的影子值
 * @param node LMS内存池节点
 * @param addr 内存地址
 * @param shadowValue 输出参数，影子值
 */
VOID OsLmsGetShadowValue(LmsMemListNode *node, UINTPTR addr, UINT32 *shadowValue)
{
    UINTPTR shadowAddr;                                                    // 影子内存地址
    UINT32 shadowOffset;                                                   // 影子内存偏移量
    if (OsLmsMem2Shadow(node, addr, &shadowAddr, &shadowOffset) != LOS_OK) {  // 转换内存地址为影子地址
        return;                                                           // 转换失败，直接返回
    }

    *shadowValue = ((*(UINT8 *)shadowAddr) >> shadowOffset) & LMS_SHADOW_MASK;  // 计算并设置影子值
}

/**
 * @brief 简单标记内存区域的影子值
 * @param startAddr 起始内存地址
 * @param endAddr 结束内存地址（不包含）
 * @param value 要设置的影子值
 */
VOID OsLmsSimpleMark(UINTPTR startAddr, UINTPTR endAddr, UINT32 value)
{
    UINT32 intSave;                                                        // 用于保存中断状态
    if (endAddr <= startAddr) {                                            // 检查结束地址是否小于等于起始地址
        PRINT_DEBUG("[LMS]mark 0x%x, 0x%x, 0x%x\n", startAddr, endAddr, (UINTPTR)__builtin_return_address(0));  // 打印调试信息
        return;                                                           // 直接返回
    }

    if (!IS_ALIGNED(startAddr, OS_MEM_ALIGN_SIZE) || !IS_ALIGNED(endAddr, OS_MEM_ALIGN_SIZE)) {  // 检查地址是否对齐
        PRINT_ERR("[LMS]mark addr is not aligned! 0x%x, 0x%x\n", startAddr, endAddr);  // 打印错误信息：地址未对齐
        return;  // 直接返回
    }

    LMS_LOCK(intSave);  // 获取LMS自旋锁

    LmsMemListNode *node = OsLmsGetPoolNodeFromAddr(startAddr);  // 查找包含起始地址的内存池节点
    if (node == NULL) {  // 检查节点是否存在
        LMS_UNLOCK(intSave);  // 释放LMS自旋锁
        return;  // 直接返回
    }

    OsLmsSetShadowValue(node, startAddr, endAddr, value);  // 设置影子值
    LMS_UNLOCK(intSave);  // 释放LMS自旋锁
}

/**
 * @brief 标记内存分配节点的影子内存状态
 * @param curNodeStart 当前内存节点起始地址
 * @param nextNodeStart 下一个内存节点起始地址
 * @param nodeHeadSize 内存节点头部大小
 */
VOID OsLmsLosMallocMark(const VOID *curNodeStart, const VOID *nextNodeStart, UINT32 nodeHeadSize)
{
    UINT32 intSave;                                                         // 中断状态保存变量
    UINTPTR curNodeStartAddr = (UINTPTR)curNodeStart;                       // 当前节点起始地址（无符号整数）
    UINTPTR nextNodeStartAddr = (UINTPTR)nextNodeStart;                     // 下一个节点起始地址（无符号整数）

    LMS_LOCK(intSave);                                                      // 获取LMS自旋锁
    LmsMemListNode *node = OsLmsGetPoolNodeFromAddr((UINTPTR)curNodeStart); // 获取当前节点所在的内存池
    if (node == NULL) {                                                     // 检查内存池节点是否存在
        LMS_UNLOCK(intSave);                                                // 释放LMS自旋锁
        return;                                                            // 返回
    }

    // 设置当前节点头部为红色区域（不可访问）
    OsLmsSetShadowValue(node, curNodeStartAddr, curNodeStartAddr + nodeHeadSize, LMS_SHADOW_REDZONE_U8);
    // 设置当前节点数据区域为可访问区域
    OsLmsSetShadowValue(node, curNodeStartAddr + nodeHeadSize, nextNodeStartAddr, LMS_SHADOW_ACCESSIBLE_U8);
    // 设置下一个节点头部为红色区域（不可访问）
    OsLmsSetShadowValue(node, nextNodeStartAddr, nextNodeStartAddr + nodeHeadSize, LMS_SHADOW_REDZONE_U8);
    LMS_UNLOCK(intSave);                                                    // 释放LMS自旋锁
}

/**
 * @brief 检查内存地址的有效性
 * @param checkAddr 待检查的内存地址
 * @param isFreeCheck 是否为释放检查
 */
VOID OsLmsCheckValid(UINTPTR checkAddr, BOOL isFreeCheck)
{
    UINT32 intSave;                                                         // 中断状态保存变量
    UINT32 shadowValue = INVALID_SHADOW_VALUE;                              // 影子值初始化为无效值
    LMS_LOCK(intSave);                                                      // 获取LMS自旋锁
    LmsMemListNode *node = OsLmsGetPoolNodeFromAddr(checkAddr);             // 获取地址所在的内存池节点
    if (node == NULL) {                                                     // 检查内存池节点是否存在
        LMS_UNLOCK(intSave);                                                // 释放LMS自旋锁
        return;                                                            // 返回
    }

    OsLmsGetShadowValue(node, checkAddr, &shadowValue);                     // 获取地址对应的影子值
    LMS_UNLOCK(intSave);                                                    // 释放LMS自旋锁
    // 检查影子值是否为可访问状态或释放检查时的已标记状态
    if ((shadowValue == LMS_SHADOW_ACCESSIBLE) || ((isFreeCheck) && (shadowValue == LMS_SHADOW_PAINT))) {
        return;                                                            // 地址有效，返回
    }

    // 报告内存错误，根据是否为释放检查选择错误模式
    OsLmsReportError(checkAddr, MEM_REGION_SIZE_1, isFreeCheck ? FREE_ERRORMODE : COMMON_ERRMODE);
}

/**
 * @brief 标记内存释放节点的影子内存状态
 * @param curNodeStart 当前内存节点起始地址
 * @param nextNodeStart 下一个内存节点起始地址
 * @param nodeHeadSize 内存节点头部大小
 */
VOID OsLmsLosFreeMark(const VOID *curNodeStart, const VOID *nextNodeStart, UINT32 nodeHeadSize)
{
    UINT32 intSave;                                                         // 中断状态保存变量
    UINT32 shadowValue = INVALID_SHADOW_VALUE;                              // 影子值初始化为无效值

    LMS_LOCK(intSave);                                                      // 获取LMS自旋锁
    LmsMemListNode *node = OsLmsGetPoolNodeFromAddr((UINTPTR)curNodeStart); // 获取当前节点所在的内存池
    if (node == NULL) {                                                     // 检查内存池节点是否存在
        LMS_UNLOCK(intSave);                                                // 释放LMS自旋锁
        return;                                                            // 返回
    }

    UINTPTR curNodeStartAddr = (UINTPTR)curNodeStart;                       // 当前节点起始地址（无符号整数）
    UINTPTR nextNodeStartAddr = (UINTPTR)nextNodeStart;                     // 下一个节点起始地址（无符号整数）

    // 获取当前节点数据区域的影子值
    OsLmsGetShadowValue(node, curNodeStartAddr + nodeHeadSize, &shadowValue);
    // 检查影子值是否为可访问状态或已标记状态
    if ((shadowValue != LMS_SHADOW_ACCESSIBLE) && (shadowValue != LMS_SHADOW_PAINT)) {
        LMS_UNLOCK(intSave);                                                // 释放LMS自旋锁
        // 报告释放错误
        OsLmsReportError(curNodeStartAddr + nodeHeadSize, MEM_REGION_SIZE_1, FREE_ERRORMODE);
        return;                                                            // 返回
    }

    if (*((UINT8 *)curNodeStart) == 0) { /* 如果合并的节点已用memset清零 */
        // 设置当前节点头部为释放后状态
        OsLmsSetShadowValue(node, curNodeStartAddr, curNodeStartAddr + nodeHeadSize, LMS_SHADOW_AFTERFREE_U8);
    }
    // 设置当前节点数据区域为释放后状态
    OsLmsSetShadowValue(node, curNodeStartAddr + nodeHeadSize, nextNodeStartAddr, LMS_SHADOW_AFTERFREE_U8);

    if (*((UINT8 *)nextNodeStart) == 0) { /* 如果合并的节点已用memset清零 */
        // 设置下一个节点头部为释放后状态
        OsLmsSetShadowValue(node, nextNodeStartAddr, nextNodeStartAddr + nodeHeadSize, LMS_SHADOW_AFTERFREE_U8);
    }

    LMS_UNLOCK(intSave);                                                    // 释放LMS自旋锁
}

/**
 * @brief 保护指定内存区域（标记为红色区域）
 * @param addrStart 内存区域起始地址
 * @param addrEnd 内存区域结束地址
 */
VOID LOS_LmsAddrProtect(UINTPTR addrStart, UINTPTR addrEnd)
{
    UINT32 intSave;                                                         // 中断状态保存变量
    if (addrEnd <= addrStart) {                                             // 检查结束地址是否小于等于起始地址
        return;                                                            // 地址无效，返回
    }
    LMS_LOCK(intSave);                                                      // 获取LMS自旋锁
    LmsMemListNode *node = OsLmsGetPoolNodeFromAddr(addrStart);             // 获取起始地址所在的内存池节点
    if (node != NULL) {                                                     // 检查内存池节点是否存在
        // 设置内存区域为红色区域（不可访问）
        OsLmsSetShadowValue(node, addrStart, addrEnd, LMS_SHADOW_REDZONE_U8);
    }
    LMS_UNLOCK(intSave);                                                    // 释放LMS自旋锁
}

/**
 * @brief 取消保护指定内存区域（标记为可访问区域）
 * @param addrStart 内存区域起始地址
 * @param addrEnd 内存区域结束地址
 */
VOID LOS_LmsAddrDisableProtect(UINTPTR addrStart, UINTPTR addrEnd)
{
    UINT32 intSave;                                                         // 中断状态保存变量
    if (addrEnd <= addrStart) {                                             // 检查结束地址是否小于等于起始地址
        return;                                                            // 地址无效，返回
    }
    LMS_LOCK(intSave);                                                      // 获取LMS自旋锁
    LmsMemListNode *node = OsLmsGetPoolNodeFromAddr(addrStart);             // 获取起始地址所在的内存池节点
    if (node != NULL) {                                                     // 检查内存池节点是否存在
        // 设置内存区域为可访问区域
        OsLmsSetShadowValue(node, addrStart, addrEnd, LMS_SHADOW_ACCESSIBLE_U8);
    }
    LMS_UNLOCK(intSave);                                                    // 释放LMS自旋锁
}

/**
 * @brief 检查单个内存地址的影子值
 * @param addr 待检查的内存地址
 * @return 影子值，LMS_SHADOW_ACCESSIBLE_U8表示可访问
 */
STATIC UINT32 OsLmsCheckAddr(UINTPTR addr)
{
    UINT32 intSave;                                                         // 中断状态保存变量
    UINT32 shadowValue = INVALID_SHADOW_VALUE;                              // 影子值初始化为无效值
    /* 不检查嵌套调用或所有CPU启动前的状态 */
    if ((LOS_AtomicRead(&g_checkDepth)) || (!OS_SCHEDULER_ALL_ACTIVE)) {
        return 0;                                                          // 返回0
    }

    LMS_LOCK(intSave);                                                      // 获取LMS自旋锁
    LmsMemListNode *node = OsLmsGetPoolNodeFromAddr(addr);                  // 获取地址所在的内存池节点
    if (node == NULL) {                                                     // 检查内存池节点是否存在
        LMS_UNLOCK(intSave);                                                // 释放LMS自旋锁
        return LMS_SHADOW_ACCESSIBLE_U8;                                    // 返回可访问状态
    }

    OsLmsGetShadowValue(node, addr, &shadowValue);                          // 获取地址对应的影子值
    LMS_UNLOCK(intSave);                                                    // 释放LMS自旋锁
    return shadowValue;                                                     // 返回影子值
}

#ifdef LOSCFG_LMS_CHECK_STRICT
/**
 * @brief 严格模式下检查内存区域的有效性（逐字节检查）
 * @param addr 内存区域起始地址
 * @param size 内存区域大小
 * @return LOS_OK表示有效，LOS_NOK表示无效
 */
STATIC INLINE UINT32 OsLmsCheckAddrRegion(UINTPTR addr, UINT32 size)
{
    UINT32 i;                                                               // 循环计数器
    for (i = 0; i < size; i++) {                                            // 遍历内存区域的每个字节
        if (OsLmsCheckAddr(addr + i)) {                                     // 检查每个字节的影子值
            return LOS_NOK;                                                 // 发现无效地址，返回LOS_NOK
        }
    }
    return LOS_OK;                                                          // 所有字节有效，返回LOS_OK
}

#else
/**
 * @brief 非严格模式下检查内存区域的有效性（仅检查边界）
 * @param addr 内存区域起始地址
 * @param size 内存区域大小
 * @return LOS_OK表示有效，LOS_NOK表示无效
 */
STATIC INLINE UINT32 OsLmsCheckAddrRegion(UINTPTR addr, UINT32 size)
{
    // 仅检查起始地址和结束地址的影子值
    if (OsLmsCheckAddr(addr) || OsLmsCheckAddr(addr + size - 1)) {
        return LOS_NOK;                                                     // 边界地址无效，返回LOS_NOK
    } else {
        return LOS_OK;                                                      // 边界地址有效，返回LOS_OK
    }
}
#endif

/**
 * @brief 打印所有内存池的信息
 */
VOID OsLmsPrintPoolListInfo(VOID)
{
    UINT32 count = 0;                                                       // 内存池计数器
    UINT32 intSave;                                                         // 中断状态保存变量
    LmsMemListNode *current = NULL;                                         // 当前内存池节点指针
    LOS_DL_LIST *listHead = &g_lmsCheckPoolList;                            // 内存池链表头

    LMS_LOCK(intSave);                                                      // 获取LMS自旋锁
    LOS_DL_LIST_FOR_EACH_ENTRY(current, listHead, LmsMemListNode, node)     // 遍历内存池链表
    {
        count++;                                                            // 计数器递增
        PRINT_DEBUG(                                                        // 打印内存池详细信息
            "[LMS]memory pool[%1u]: totalsize 0x%-8x  memstart 0x%-8x memstop 0x%-8x memsize 0x%-8x shadowstart 0x%-8x "
            "shadowSize 0x%-8x\n",
            count, current->poolSize + current->shadowSize, current->poolAddr, current->poolAddr + current->poolSize,
            current->poolSize, current->shadowStart, current->shadowSize);
    }

    LMS_UNLOCK(intSave);                                                    // 释放LMS自旋锁
}

/**
 * @brief 打印指定地址周围的内存和影子内存信息
 * @param addr 要打印的内存地址
 */
VOID OsLmsPrintMemInfo(UINTPTR addr)
{
#define LMS_DUMP_OFFSET 16                                                  // 内存转储偏移量
#define LMS_DUMP_RANGE_DOUBLE 2                                             // 内存转储范围倍数

    PRINTK("\n[LMS] Dump info around address [0x%8x]:\n", addr);            // 打印转储标题
    const UINT32 printY = LMS_DUMP_OFFSET * LMS_DUMP_RANGE_DOUBLE + 1;       // Y轴打印范围
    const UINT32 printX = LMS_MEM_BYTES_PER_SHADOW_CELL * LMS_DUMP_RANGE_DOUBLE; // X轴打印范围
    UINTPTR dumpAddr = addr - addr % printX - LMS_DUMP_OFFSET * printX;     // 转储起始地址
    UINT32 shadowValue = 0;                                                 // 影子值
    UINTPTR shadowAddr = 0;                                                 // 影子内存地址
    UINT32 shadowOffset = 0;                                                // 影子内存偏移量
    LmsMemListNode *nodeInfo = NULL;                                        // 内存池节点信息
    INT32 isCheckAddr, x, y;                                                // 临时变量

    nodeInfo = OsLmsGetPoolNodeFromAddr(addr);                              // 获取地址所在的内存池节点
    if (nodeInfo == NULL) {                                                 // 检查内存池节点是否存在
        PRINT_ERR("[LMS]addr is not in checkpool\n");                       // 打印错误信息
        return;                                                            // 返回
    }

    for (y = 0; y < printY; y++, dumpAddr += printX) {                      // 遍历Y轴范围
        if (dumpAddr < nodeInfo->poolAddr) { /* 找到转储地址在内存池范围内 */
            continue;                                                       // 继续下一次循环
        }

        if ((dumpAddr + printX) >=
            nodeInfo->poolAddr + nodeInfo->poolSize) { /* finish if dumpAddr exceeds pool's upper region */
            goto END;
        }

        PRINTK("\n\t[0x%x]: ", dumpAddr);                                   // 打印内存地址
        for (x = 0; x < printX; x++) {                                      // 遍历X轴范围
            if ((dumpAddr + x) == addr) {                                   // 如果是目标地址
                PRINTK("[%02x]", *(UINT8 *)(dumpAddr + x));                 // 用方括号标记目标地址
            } else {
                PRINTK(" %02x ", *(UINT8 *)(dumpAddr + x));                 // 打印内存字节值
            }
        }

        if (OsLmsMem2Shadow(nodeInfo, dumpAddr, &shadowAddr, &shadowOffset) != LOS_OK) { // 转换内存地址到影子地址
            goto END;                                                       // 转换失败，跳转到结束标签
        }

        PRINTK("|\t[0x%x | %2u]: ", shadowAddr, shadowOffset);              // 打印影子内存地址和偏移量

        for (x = 0; x < printX; x += LMS_MEM_BYTES_PER_SHADOW_CELL) {        // 遍历影子内存单元
            OsLmsGetShadowValue(nodeInfo, dumpAddr + x, &shadowValue);      // 获取影子值
            isCheckAddr = dumpAddr + x - (UINTPTR)addr + LMS_MEM_BYTES_PER_SHADOW_CELL; // 计算检查地址
            if ((isCheckAddr > 0) && (isCheckAddr <= LMS_MEM_BYTES_PER_SHADOW_CELL)) {
                PRINTK("[%1x]", shadowValue);                              // 用方括号标记目标影子值
            } else {
                PRINTK(" %1x ", shadowValue);                              // 打印影子值
            }
        }
    }
END:
    PRINTK("\n");                                                          // 打印换行
}

/**
 * @brief 获取内存错误信息
 * @param addr 错误内存地址
 * @param size 内存区域大小
 * @param info 输出参数，错误信息结构体
 */
STATIC VOID OsLmsGetErrorInfo(UINTPTR addr, UINT32 size, LmsAddrInfo *info)
{
    LmsMemListNode *node = OsLmsGetPoolNodeFromAddr(addr);                  // 获取地址所在的内存池节点
    OsLmsGetShadowInfo(node, addr, info);                                   // 获取地址的影子信息
    if (info->shadowValue != LMS_SHADOW_ACCESSIBLE_U8) {                    // 如果影子值不是可访问状态
        return;                                                            // 返回
    } else {
        OsLmsGetShadowInfo(node, addr + size - 1, info);                    // 获取结束地址的影子信息
    }
}

/**
 * @brief 打印内存错误信息
 * @param info 错误信息结构体
 * @param errMod 错误模式
 */
STATIC VOID OsLmsPrintErrInfo(LmsAddrInfo *info, UINT32 errMod)
{
    switch (info->shadowValue) {                                            // 根据影子值判断错误类型
        case LMS_SHADOW_AFTERFREE:
            PRINT_ERR("Use after free error detected\n");                   // 使用已释放内存错误
            break;
        case LMS_SHADOW_REDZONE:
            PRINT_ERR("Heap buffer overflow error detected\n");             // 堆缓冲区溢出错误
            break;
        case LMS_SHADOW_ACCESSIBLE:
            PRINT_ERR("No error\n");                                       // 无错误
            break;
        default:
            PRINT_ERR("UnKnown Error detected\n");                          // 未知错误
            break;
    }

    switch (errMod) {                                                       // 根据错误模式打印信息
        case FREE_ERRORMODE:
            PRINT_ERR("Illegal Double free address at: [0x%lx]\n", info->memAddr); // 非法双重释放
            break;
        case LOAD_ERRMODE:
            PRINT_ERR("Illegal READ address at: [0x%lx]\n", info->memAddr); // 非法读地址
            break;
        case STORE_ERRMODE:
            PRINT_ERR("Illegal WRITE address at: [0x%lx]\n", info->memAddr); // 非法写地址
            break;
        case COMMON_ERRMODE:
            PRINT_ERR("Common Error at: [0x%lx]\n", info->memAddr);        // 通用错误
            break;
        default:
            PRINT_ERR("UnKnown Error mode at: [0x%lx]\n", info->memAddr);   // 未知错误模式
            break;
    }

    PRINT_ERR("Shadow memory address: [0x%lx : %1u]  Shadow memory value: [%u] \n", info->shadowAddr,  // 打印影子内存信息
        info->shadowOffset, info->shadowValue);
}

/**
 * @brief 报告内存错误
 * @param p 错误内存地址
 * @param size 内存区域大小
 * @param errMod 错误模式
 */
VOID OsLmsReportError(UINTPTR p, UINT32 size, UINT32 errMod)
{
    UINT32 intSave;                                                         // 中断状态保存变量
    LmsAddrInfo info;                                                       // 错误信息结构体

    (VOID)LOS_AtomicAdd(&g_checkDepth, 1);                                  // 增加检查深度计数器
    LMS_LOCK(intSave);                                                      // 获取LMS自旋锁
    (VOID)memset_s(&info, sizeof(LmsAddrInfo), 0, sizeof(LmsAddrInfo));     // 初始化错误信息结构体

    PRINT_ERR("*****  Kernel Address Sanitizer Error Detected Start *****\n"); // 打印错误报告开始标记

    OsLmsGetErrorInfo(p, size, &info);                                      // 获取错误信息

    OsLmsPrintErrInfo(&info, errMod);                                       // 打印错误信息

    OsBackTrace();                                                          // 打印函数调用栈

    OsLmsPrintMemInfo(info.memAddr);                                        // 打印内存信息
    LMS_UNLOCK(intSave);                                                    // 释放LMS自旋锁
    PRINT_ERR("*****  Kernel Address Sanitizer Error Detected End *****\n");   // 打印错误报告结束标记
    (VOID)LOS_AtomicSub(&g_checkDepth, 1);                                  // 减少检查深度计数器
}

#ifdef LOSCFG_LMS_STORE_CHECK
/**
 * @brief 1字节存储操作检查（不终止）
 * @param p 存储地址
 */
VOID __asan_store1_noabort(UINTPTR p)
{
    if (OsLmsCheckAddr(p) != LMS_SHADOW_ACCESSIBLE_U8) {                     // 检查地址是否可访问
        OsLmsReportError(p, MEM_REGION_SIZE_1, STORE_ERRMODE);              // 报告存储错误
    }
}

/**
 * @brief 2字节存储操作检查（不终止）
 * @param p 存储地址
 */
VOID __asan_store2_noabort(UINTPTR p)
{
    if (OsLmsCheckAddrRegion(p, MEM_REGION_SIZE_2) != LOS_OK) {              // 检查地址区域是否可访问
        OsLmsReportError(p, MEM_REGION_SIZE_2, STORE_ERRMODE);              // 报告存储错误
    }
}

/**
 * @brief 4字节存储操作检查（不终止）
 * @param p 存储地址
 */
VOID __asan_store4_noabort(UINTPTR p)
{
    if (OsLmsCheckAddrRegion(p, MEM_REGION_SIZE_4) != LOS_OK) {              // 检查地址区域是否可访问
        OsLmsReportError(p, MEM_REGION_SIZE_4, STORE_ERRMODE);              // 报告存储错误
    }
}

/**
 * @brief 8字节存储操作检查（不终止）
 * @param p 存储地址
 */
VOID __asan_store8_noabort(UINTPTR p)
{
    if (OsLmsCheckAddrRegion(p, MEM_REGION_SIZE_8) != LOS_OK) {              // 检查地址区域是否可访问
        OsLmsReportError(p, MEM_REGION_SIZE_8, STORE_ERRMODE);              // 报告存储错误
    }
}

/**
 * @brief 16字节存储操作检查（不终止）
 * @param p 存储地址
 */
VOID __asan_store16_noabort(UINTPTR p)
{
    if (OsLmsCheckAddrRegion(p, MEM_REGION_SIZE_16) != LOS_OK) {             // 检查地址区域是否可访问
        OsLmsReportError(p, MEM_REGION_SIZE_16, STORE_ERRMODE);             // 报告存储错误
    }
}

/**
 * @brief N字节存储操作检查（不终止）
 * @param p 存储地址
 * @param size 存储大小
 */
VOID __asan_storeN_noabort(UINTPTR p, UINT32 size)
{
    if (OsLmsCheckAddrRegion(p, size) != LOS_OK) {                           // 检查地址区域是否可访问
        OsLmsReportError(p, size, STORE_ERRMODE);                           // 报告存储错误
    }
}
#else
/**
 * @brief 1字节存储操作检查（不终止，空实现）
 * @param p 存储地址
 */
VOID __asan_store1_noabort(UINTPTR p)
{
    (VOID)p;                                                                // 未使用参数
}

/**
 * @brief 2字节存储操作检查（不终止，空实现）
 * @param p 存储地址
 */
VOID __asan_store2_noabort(UINTPTR p)
{
    (VOID)p;                                                                // 未使用参数
}

/**
 * @brief 4字节存储操作检查（不终止，空实现）
 * @param p 存储地址
 */
VOID __asan_store4_noabort(UINTPTR p)
{
    (VOID)p;                                                                // 未使用参数
}

/**
 * @brief 8字节存储操作检查（不终止，空实现）
 * @param p 存储地址
 */
VOID __asan_store8_noabort(UINTPTR p)
{
    (VOID)p;                                                                // 未使用参数
}

/**
 * @brief 16字节存储操作检查（不终止，空实现）
 * @param p 存储地址
 */
VOID __asan_store16_noabort(UINTPTR p)
{
    (VOID)p;                                                                // 未使用参数
}

/**
 * @brief N字节存储操作检查（不终止，空实现）
 * @param p 存储地址
 * @param size 存储大小
 */
VOID __asan_storeN_noabort(UINTPTR p, UINT32 size)
{
    (VOID)p;                                                                // 未使用参数
    (VOID)size;                                                             // 未使用参数
}

#endif

#ifdef LOSCFG_LMS_LOAD_CHECK
/**
 * @brief 1字节加载操作检查（不终止）
 * @param p 加载地址
 */
VOID __asan_load1_noabort(UINTPTR p)
{
    if (OsLmsCheckAddr(p) != LMS_SHADOW_ACCESSIBLE_U8) {                     // 检查地址是否可访问
        OsLmsReportError(p, MEM_REGION_SIZE_1, LOAD_ERRMODE);               // 报告加载错误
    }
}

/**
 * @brief 2字节加载操作检查（不终止）
 * @param p 加载地址
 */
VOID __asan_load2_noabort(UINTPTR p)
{
    if (OsLmsCheckAddrRegion(p, MEM_REGION_SIZE_2) != LOS_OK) {              // 检查地址区域是否可访问
        OsLmsReportError(p, MEM_REGION_SIZE_2, LOAD_ERRMODE);               // 报告加载错误
    }
}

/**
 * @brief 4字节加载操作检查（不终止）
 * @param p 加载地址
 */
VOID __asan_load4_noabort(UINTPTR p)
{
    if (OsLmsCheckAddrRegion(p, MEM_REGION_SIZE_4) != LOS_OK) {              // 检查地址区域是否可访问
        OsLmsReportError(p, MEM_REGION_SIZE_4, LOAD_ERRMODE);               // 报告加载错误
    }
}

/**
 * @brief 8字节加载操作检查（不终止）
 * @param p 加载地址
 */
VOID __asan_load8_noabort(UINTPTR p)
{
    if (OsLmsCheckAddrRegion(p, MEM_REGION_SIZE_8) != LOS_OK) {              // 检查地址区域是否可访问
        OsLmsReportError(p, MEM_REGION_SIZE_8, LOAD_ERRMODE);               // 报告加载错误
    }
}

/**
 * @brief 16字节加载操作检查（不终止）
 * @param p 加载地址
 */
VOID __asan_load16_noabort(UINTPTR p)
{
    if (OsLmsCheckAddrRegion(p, MEM_REGION_SIZE_16) != LOS_OK) {             // 检查地址区域是否可访问
        OsLmsReportError(p, MEM_REGION_SIZE_16, LOAD_ERRMODE);              // 报告加载错误
    }
}

/**
 * @brief N字节加载操作检查（不终止）
 * @param p 加载地址
 * @param size 加载大小
 */
VOID __asan_loadN_noabort(UINTPTR p, UINT32 size)
{
    if (OsLmsCheckAddrRegion(p, size) != LOS_OK) {                           // 检查地址区域是否可访问
        OsLmsReportError(p, size, LOAD_ERRMODE);                            // 报告加载错误
    }
}
#else
/**
 * @brief 1字节加载操作检查（不终止，空实现）
 * @param p 加载地址
 */
VOID __asan_load1_noabort(UINTPTR p)
{
    (VOID)p;  // 未使用参数
}

/**
 * @brief 2字节加载操作检查（不终止，空实现）
 * @param p 加载地址
 */
VOID __asan_load2_noabort(UINTPTR p)
{
    (VOID)p;  // 未使用参数
}

/**
 * @brief 4字节加载操作检查（不终止，空实现）
 * @param p 加载地址
 */
VOID __asan_load4_noabort(UINTPTR p)
{
    (VOID)p;  // 未使用参数
}

/**
 * @brief 8字节加载操作检查（不终止，空实现）
 * @param p 加载地址
 */
VOID __asan_load8_noabort(UINTPTR p)
{
    (VOID)p;  // 未使用参数
}

/**
 * @brief 16字节加载操作检查（不终止，空实现）
 * @param p 加载地址
 */
VOID __asan_load16_noabort(UINTPTR p)
{
    (VOID)p;  // 未使用参数
}

/**
 * @brief N字节加载操作检查（不终止，空实现）
 * @param p 加载地址
 * @param size 加载大小
 */
VOID __asan_loadN_noabort(UINTPTR p, UINT32 size)
{
    (VOID)p;  // 未使用参数
    (VOID)size;  // 未使用参数
}
#endif
/**
 * @brief 处理无返回函数（空实现）
 */
VOID __asan_handle_no_return(VOID)
{
    return;  // 返回
}

LOS_MODULE_INIT(OsLmsInit, LOS_INIT_LEVEL_KMOD_PREVM);  // 注册LMS模块初始化函数
