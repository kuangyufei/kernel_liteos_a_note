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

/**
 * @defgroup los_pte_ops page table entry operations
 * @ingroup kernel
 */

#ifndef __LOS_PTE_OPS_H__
#define __LOS_PTE_OPS_H__

#include "los_typedef.h"
#include "arm.h"
#include "los_mmu_descriptor_v6.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @brief 保存一级页表项(PTE1)并添加内存屏障
 * @param pte1Ptr 一级页表项指针
 * @param pte1 要保存的一级页表项值
 * @note 使用DMB(数据内存屏障)确保存储操作完成前的内存访问已完成，DSB(数据同步屏障)确保页表更新对后续操作可见
 */
STATIC INLINE VOID OsSavePte1(PTE_T *pte1Ptr, PTE_T pte1)
{
    DMB;                                  // 数据内存屏障，确保之前的内存访问完成
    *pte1Ptr = pte1;                      // 写入一级页表项值
    DSB;                                  // 数据同步屏障，确保页表更新对所有处理器可见
}

/**
 * @brief 截断地址到一级页表(section)对齐地址
 * @param addr 输入地址
 * @return 按一级页表大小对齐后的地址
 * @note 使用MMU_DESCRIPTOR_L1_SECTION_ADDR宏进行地址对齐，通常为1MB边界
 */
STATIC INLINE ADDR_T OsTruncPte1(ADDR_T addr)
{
    return MMU_DESCRIPTOR_L1_SECTION_ADDR(addr);
}

/**
 * @brief 获取虚拟地址在一级页表中的索引
 * @param va 虚拟地址
 * @return 一级页表索引值
 * @note 计算公式: va >> MMU_DESCRIPTOR_L1_SMALL_SHIFT，其中L1_SHIFT通常为20(对应1MB页表项)
 */
STATIC INLINE UINT32 OsGetPte1Index(vaddr_t va)
{
    return va >> MMU_DESCRIPTOR_L1_SMALL_SHIFT;  // 右移20位计算一级页表索引
}

/**
 * @brief 清除一级页表项(设置为无效)
 * @param pte1Ptr 一级页表项指针
 * @note 调用OsSavePte1设置页表项为0，0表示无效页表项
 */
STATIC INLINE VOID OsClearPte1(PTE_T *pte1Ptr)
{
    OsSavePte1(pte1Ptr, 0);               // 保存0值到页表项，使其变为无效
}

/**
 * @brief 计算一级页表项的物理地址
 * @param PhysTtb 页表基址(物理地址)
 * @param va 虚拟地址
 * @return 一级页表项的物理地址
 * @note 页表项大小为sizeof(PADDR_T)，通常为4字节(32位系统)或8字节(64位系统)
 */
STATIC INLINE PADDR_T OsGetPte1Paddr(PADDR_T PhysTtb, vaddr_t va)
{
    return (PhysTtb + (OsGetPte1Index(va) * sizeof(PADDR_T)));
}

/**
 * @brief 获取一级页表项指针
 * @param pte1BasePtr 一级页表基址指针
 * @param va 虚拟地址
 * @return 对应虚拟地址的一级页表项指针
 */
STATIC INLINE PTE_T *OsGetPte1Ptr(PTE_T *pte1BasePtr, vaddr_t va)
{
    return (pte1BasePtr + OsGetPte1Index(va));  // 基址+索引计算页表项指针
}

/**
 * @brief 读取一级页表项值
 * @param pte1BasePtr 一级页表基址指针
 * @param va 虚拟地址
 * @return 一级页表项值
 */
STATIC INLINE PTE_T OsGetPte1(PTE_T *pte1BasePtr, vaddr_t va)
{
    return *OsGetPte1Ptr(pte1BasePtr, va);  // 间接调用OsGetPte1Ptr获取并返回页表项值
}

/**
 * @brief 判断一级页表项是否为页表类型(指向二级页表)
 * @param pte1 一级页表项值
 * @return TRUE: 页表类型; FALSE: 其他类型
 * @note 通过与MMU_DESCRIPTOR_L1_TYPE_MASK掩码按位与，判断是否等于MMU_DESCRIPTOR_L1_TYPE_PAGE_TABLE
 */
STATIC INLINE BOOL OsIsPte1PageTable(PTE_T pte1)
{
    return (pte1 & MMU_DESCRIPTOR_L1_TYPE_MASK) == MMU_DESCRIPTOR_L1_TYPE_PAGE_TABLE;
}

/**
 * @brief 判断一级页表项是否为无效类型
 * @param pte1 一级页表项值
 * @return TRUE: 无效; FALSE: 有效
 */
STATIC INLINE BOOL OsIsPte1Invalid(PTE_T pte1)
{
    return (pte1 & MMU_DESCRIPTOR_L1_TYPE_MASK) == MMU_DESCRIPTOR_L1_TYPE_INVALID;
}

/**
 * @brief 判断一级页表项是否为段(section)类型
 * @param pte1 一级页表项值
 * @return TRUE: 段类型; FALSE: 其他类型
 * @note 段类型通常对应1MB大小的内存块映射
 */
STATIC INLINE BOOL OsIsPte1Section(PTE_T pte1)
{
    return (pte1 & MMU_DESCRIPTOR_L1_TYPE_MASK) == MMU_DESCRIPTOR_L1_TYPE_SECTION;
}

/**
 * @brief 获取虚拟地址在二级页表中的索引
 * @param va 虚拟地址
 * @return 二级页表索引值
 * @note 计算公式: (va % L1_SIZE) >> L2_SHIFT，其中L1_SIZE通常为1MB，L2_SHIFT通常为12(对应4KB页)
 */
STATIC INLINE UINT32 OsGetPte2Index(vaddr_t va)
{
    return (va % MMU_DESCRIPTOR_L1_SMALL_SIZE) >> MMU_DESCRIPTOR_L2_SMALL_SHIFT;
}

/**
 * @brief 获取二级页表项指针
 * @param pte2BasePtr 二级页表基址指针
 * @param va 虚拟地址
 * @return 对应虚拟地址的二级页表项指针
 */
STATIC INLINE PTE_T *OsGetPte2Ptr(PTE_T *pte2BasePtr, vaddr_t va)
{
    return (pte2BasePtr + OsGetPte2Index(va));  // 基址+索引计算二级页表项指针
}

/**
 * @brief 读取二级页表项值
 * @param pte2BasePtr 二级页表基址指针
 * @param va 虚拟地址
 * @return 二级页表项值
 */
STATIC INLINE PTE_T OsGetPte2(PTE_T *pte2BasePtr, vaddr_t va)
{
    return *(pte2BasePtr + OsGetPte2Index(va));  // 通过索引计算地址并读取值
}

/**
 * @brief 保存二级页表项(PTE2)并添加内存屏障
 * @param pte2Ptr 二级页表项指针
 * @param pte2 要保存的二级页表项值
 * @note 同OsSavePte1，确保页表更新的内存一致性
 */
STATIC INLINE VOID OsSavePte2(PTE_T *pte2Ptr, PTE_T pte2)
{
    DMB;                                  // 数据内存屏障
    *pte2Ptr = pte2;                      // 写入二级页表项值
    DSB;                                  // 数据同步屏障
}

/**
 * @brief 连续保存多个二级页表项
 * @param pte2BasePtr 二级页表基址指针
 * @param index 起始索引
 * @param pte2 起始页表项值
 * @param count 要保存的数量
 * @return 实际保存的数量
 * @note 1. 循环写入连续页表项，每次pte2增加一个页面大小(通常4KB)
 *       2. 当count为0或index达到MMU_DESCRIPTOR_L2_NUMBERS_PER_L1(一级页表项对应二级页表项总数)时停止
 */
STATIC INLINE UINT32 OsSavePte2Continuous(PTE_T *pte2BasePtr, UINT32 index, PTE_T pte2, UINT32 count)
{
    UINT32 saveCounts = 0;                // 实际保存计数
    if (count == 0) {                     // 边界条件检查: 计数为0直接返回
        return 0;
    }

    DMB;                                  // 数据内存屏障
    do {
        pte2BasePtr[index++] = pte2;      // 保存当前页表项
        count--;                          // 剩余计数递减
        pte2 += MMU_DESCRIPTOR_L2_SMALL_SIZE;  // 页表项值增加一个页面大小(4KB)
        saveCounts++;                     // 实际保存计数递增
    } while ((count != 0) && (index != MMU_DESCRIPTOR_L2_NUMBERS_PER_L1));  // 循环条件: 计数未用完且索引未越界
    DSB;                                  // 数据同步屏障

    return saveCounts;                    // 返回实际保存数量
}

/**
 * @brief 连续清除多个二级页表项(设置为无效)
 * @param pte2Ptr 二级页表起始指针
 * @param count 要清除的数量
 * @note 将指定数量的页表项设置为0(无效)，并添加内存屏障确保操作生效
 */
STATIC INLINE VOID OsClearPte2Continuous(PTE_T *pte2Ptr, UINT32 count)
{
    UINT32 index = 0;                     // 循环索引

    DMB;                                  // 数据内存屏障
    while (count > 0) {                   // 循环清除count个页表项
        pte2Ptr[index++] = 0;             // 设置页表项为0(无效)
        count--;                          // 剩余计数递减
    }
    DSB;                                  // 数据同步屏障
}

/**
 * @brief 判断二级页表项是否为小页面类型
 * @param pte2 二级页表项值
 * @return TRUE: 小页面; FALSE: 其他类型
 * @note 小页面通常为4KB(ARMv7架构)
 */
STATIC INLINE BOOL OsIsPte2SmallPage(PTE_T pte2)
{
    return (pte2 & MMU_DESCRIPTOR_L2_TYPE_MASK) == MMU_DESCRIPTOR_L2_TYPE_SMALL_PAGE;
}

/**
 * @brief 判断二级页表项是否为不可执行(XN)小页面类型
 * @param pte2 二级页表项值
 * @return TRUE: 不可执行小页面; FALSE: 其他类型
 * @note XN(eXecute Never)位用于设置页面不可执行，增强安全性
 */
STATIC INLINE BOOL OsIsPte2SmallPageXN(PTE_T pte2)
{
    return (pte2 & MMU_DESCRIPTOR_L2_TYPE_MASK) == MMU_DESCRIPTOR_L2_TYPE_SMALL_PAGE_XN;
}

/**
 * @brief 判断二级页表项是否为大页面类型
 * @param pte2 二级页表项值
 * @return TRUE: 大页面; FALSE: 其他类型
 * @note 大页面通常为64KB(ARMv7架构)
 */
STATIC INLINE BOOL OsIsPte2LargePage(PTE_T pte2)
{
    return (pte2 & MMU_DESCRIPTOR_L2_TYPE_MASK) == MMU_DESCRIPTOR_L2_TYPE_LARGE_PAGE;
}

/**
 * @brief 判断二级页表项是否为无效类型
 * @param pte2 二级页表项值
 * @return TRUE: 无效; FALSE: 有效
 */
STATIC INLINE BOOL OsIsPte2Invalid(PTE_T pte2)
{
    return (pte2 & MMU_DESCRIPTOR_L2_TYPE_MASK) == MMU_DESCRIPTOR_L2_TYPE_INVALID;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* __LOS_PTE_OPS_H__ */
