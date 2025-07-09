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
 * @defgroup los_mmu_descriptor_v6 MMU Descriptor v6
 * @ingroup kernel
 */

#ifndef __LOS_MMU_DESCRIPTOR_V6_H__
#define __LOS_MMU_DESCRIPTOR_V6_H__

#include "los_vm_common.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/*
 * MMU描述符定义头文件(ARMv6架构)
 * 包含页表项格式、缓存策略、访问权限等MMU配置相关宏定义
 */

/* I/O内存访问修饰符，用于标识内存映射的I/O区域 */
#define __iomem

/*
 * 地址对齐检查宏
 * 判断地址a是否按b字节对齐
 * 实现原理：如果a是b的整数倍，则a & (b-1)结果为0
 *
 * @param a 待检查的地址
 * @param b 对齐字节数(必须是2的幂)
 * @return 1表示对齐，0表示未对齐
 */
#ifndef IS_ALIGNED
#define IS_ALIGNED(a, b)                                        (!(((UINTPTR)(a)) & (((UINTPTR)(b))-1)))
#endif

/*
 * TEX(Type Extension)字段定义
 * TEX与CB(Cache/Buffer)位组合用于指定内存区域类型和属性
 */
#define MMU_DESCRIPTOR_TEX_0                                    0  /* TEX字段值0 */
#define MMU_DESCRIPTOR_TEX_1                                    1  /* TEX字段值1 */
#define MMU_DESCRIPTOR_TEX_2                                    2  /* TEX字段值2 */
#define MMU_DESCRIPTOR_TEX_MASK                                 7  /* TEX字段掩码(3位) */

/*
 * 缓存策略相关宏定义
 * CB(Cache/Buffer)位配置，用于控制内存区域的缓存行为
 */
#define MMU_DESCRIPTOR_CACHE_BUFFER_SHIFT                       2  /* CB位在描述符中的偏移量 */
#define MMU_DESCRIPTOR_CACHE_BUFFER(x)                          ((x) << MMU_DESCRIPTOR_CACHE_BUFFER_SHIFT)  /* CB位字段构造 */
#define MMU_DESCRIPTOR_NON_CACHEABLE                            MMU_DESCRIPTOR_CACHE_BUFFER(0)  /* 非缓存(non-cacheable) */
#define MMU_DESCRIPTOR_WRITE_BACK_ALLOCATE                      MMU_DESCRIPTOR_CACHE_BUFFER(1)  /* 回写分配(write-back, allocate) */
#define MMU_DESCRIPTOR_WRITE_THROUGH_NO_ALLOCATE                MMU_DESCRIPTOR_CACHE_BUFFER(2)  /* 直写不分配(write-through, no allocate) */
#define MMU_DESCRIPTOR_WRITE_BACK_NO_ALLOCATE                   MMU_DESCRIPTOR_CACHE_BUFFER(3)  /* 回写不分配(write-back, no allocate) */

/* 用户空间MMU访问权限定义开始 */
#define MMU_DESCRIPTOR_DOMAIN_MANAGER                           0  /* 域管理权限(可配置域访问控制) */
#define MMU_DESCRIPTOR_DOMAIN_CLIENT                            1  /* 域客户端权限(受域访问控制限制) */
#define MMU_DESCRIPTOR_DOMAIN_NA                                2  /* 未使用的域权限 */

/*
 * L1页表描述符类型定义
 * L1描述符位于一级页表，用于指示内存区域的映射方式
 */
#define MMU_DESCRIPTOR_L1_TYPE_INVALID                          (0x0 << 0)  /* 无效描述符 */
#define MMU_DESCRIPTOR_L1_TYPE_PAGE_TABLE                       (0x1 << 0)  /* 页表描述符(指向L2页表) */
#define MMU_DESCRIPTOR_L1_TYPE_SECTION                          (0x2 << 0)  /* 段描述符(1MB区域映射) */
#define MMU_DESCRIPTOR_L1_TYPE_MASK                             (0x3 << 0)  /* L1描述符类型掩码(bit0-1) */

/*
 * L2页表描述符类型定义
 * L2描述符位于二级页表，用于指示小页或大页映射
 */
#define MMU_DESCRIPTOR_L2_TYPE_INVALID                          (0x0 << 0)  /* 无效描述符 */
#define MMU_DESCRIPTOR_L2_TYPE_LARGE_PAGE                       (0x1 << 0)  /* 大页描述符(64KB) */
#define MMU_DESCRIPTOR_L2_TYPE_SMALL_PAGE                       (0x2 << 0)  /* 小页描述符(4KB) */
#define MMU_DESCRIPTOR_L2_TYPE_SMALL_PAGE_XN                    (0x3 << 0)  /* 带执行禁止的小页描述符 */
#define MMU_DESCRIPTOR_L2_TYPE_MASK                             (0x3 << 0)  /* L2描述符类型掩码(bit0-1) */

/*
 * L1段映射相关宏定义
 * 段映射直接将虚拟地址映射到物理地址，粒度为1MB
 */
#define MMU_DESCRIPTOR_IS_L1_SIZE_ALIGNED(x)                    IS_ALIGNED(x, MMU_DESCRIPTOR_L1_SMALL_SIZE)  /* 检查地址是否按L1段大小对齐 */
#define MMU_DESCRIPTOR_L1_SMALL_SIZE                            0x100000     /* L1段大小：1MB(1024*1024字节) */
#define MMU_DESCRIPTOR_L1_SMALL_MASK                            (MMU_DESCRIPTOR_L1_SMALL_SIZE - 1)  /* L1段内偏移掩码 */
#define MMU_DESCRIPTOR_L1_SMALL_FRAME                           (~MMU_DESCRIPTOR_L1_SMALL_MASK)  /* L1段地址掩码(取1MB对齐地址) */
#define MMU_DESCRIPTOR_L1_SMALL_SHIFT                           20  /* L1段索引移位量(20位虚拟地址对应1MB) */
#define MMU_DESCRIPTOR_L1_SECTION_ADDR(x)                       ((x) & MMU_DESCRIPTOR_L1_SMALL_FRAME)  /* 获取段对齐地址 */
#define MMU_DESCRIPTOR_L1_PAGE_TABLE_ADDR(x)                    ((x) & ~((1 << 10)-1))  /* L2页表地址对齐(1KB对齐) */
#define MMU_DESCRIPTOR_L1_SMALL_L2_TABLES_PER_PAGE              4  /* 每个L1页表项对应4个L2页表 */
#define MMU_DESCRIPTOR_L1_SMALL_ENTRY_NUMBERS                   0x4000U  /* L1页表项数量：16384(4GB/1MB=4096，实际为二级页表时更大) */
#define MMU_DESCRIPTOR_L1_SMALL_DOMAIN_MASK                     (~(0x0f << 5))  /* L1段域字段掩码(bit5-8) */
#define MMU_DESCRIPTOR_L1_SMALL_DOMAIN_CLIENT                   (MMU_DESCRIPTOR_DOMAIN_CLIENT << 5)  /* L1段域客户端权限(bit5-8设为0x1) */

/* L1描述符额外属性位定义 */
#define MMU_DESCRIPTOR_L1_PAGETABLE_NON_SECURE                  (1 << 3)   /* 非安全状态位(用于TrustZone) */
#define MMU_DESCRIPTOR_L1_SECTION_NON_SECURE                    (1 << 19)  /* 段非安全状态位 */
#define MMU_DESCRIPTOR_L1_SECTION_SHAREABLE                     (1 << 16)  /* 段共享位(多核共享内存区域) */
#define MMU_DESCRIPTOR_L1_SECTION_NON_GLOBAL                    (1 << 17)  /* 段非全局位(进程私有映射) */
#define MMU_DESCRIPTOR_L1_SECTION_XN                            (1 << 4)   /* 执行禁止位(不可执行区域) */

/*
 * L1描述符TEX字段组合宏
 * TEX与CB位组合定义不同内存类型
 */
#define MMU_DESCRIPTOR_L1_TEX_SHIFT                             12  /* L1描述符TEX字段偏移量(bit12-14) */
#define MMU_DESCRIPTOR_L1_TEX(x)                                \
    ((x) << MMU_DESCRIPTOR_L1_TEX_SHIFT)  /* 构造L1 TEX字段值 */
#define MMU_DESCRIPTOR_L1_TYPE_STRONGLY_ORDERED                 \
    (MMU_DESCRIPTOR_L1_TEX(MMU_DESCRIPTOR_TEX_0) | MMU_DESCRIPTOR_NON_CACHEABLE)  /* 强序型内存(无缓存) */
#define MMU_DESCRIPTOR_L1_TYPE_NORMAL_NOCACHE                   \
    (MMU_DESCRIPTOR_L1_TEX(MMU_DESCRIPTOR_TEX_1) | MMU_DESCRIPTOR_NON_CACHEABLE)  /* 普通非缓存内存 */
#define MMU_DESCRIPTOR_L1_TYPE_DEVICE_SHARED                    \
    (MMU_DESCRIPTOR_L1_TEX(MMU_DESCRIPTOR_TEX_0) | MMU_DESCRIPTOR_WRITE_BACK_ALLOCATE)  /* 共享设备内存 */
#define MMU_DESCRIPTOR_L1_TYPE_DEVICE_NON_SHARED                \
    (MMU_DESCRIPTOR_L1_TEX(MMU_DESCRIPTOR_TEX_2) | MMU_DESCRIPTOR_NON_CACHEABLE)  /* 非共享设备内存 */
#define MMU_DESCRIPTOR_L1_TYPE_NORMAL_WRITE_BACK_ALLOCATE       \
    (MMU_DESCRIPTOR_L1_TEX(MMU_DESCRIPTOR_TEX_1) | MMU_DESCRIPTOR_WRITE_BACK_NO_ALLOCATE)  /* 普通回写分配内存 */
#define MMU_DESCRIPTOR_L1_TEX_TYPE_MASK                         \
    (MMU_DESCRIPTOR_L1_TEX(MMU_DESCRIPTOR_TEX_MASK) | MMU_DESCRIPTOR_WRITE_BACK_NO_ALLOCATE)  /* L1内存类型字段掩码 */

/*
 * L1描述符访问权限(AP)位定义
 * AP位控制不同特权级(PL1/PL0)对内存的访问权限
 */
#define MMU_DESCRIPTOR_L1_AP2_SHIFT                             15  /* AP2位偏移量(bit15) */
#define MMU_DESCRIPTOR_L1_AP2(x)                                ((x) << MMU_DESCRIPTOR_L1_AP2_SHIFT)  /* 构造AP2字段 */
#define MMU_DESCRIPTOR_L1_AP2_0                                 (MMU_DESCRIPTOR_L1_AP2(0))  /* AP2=0 */
#define MMU_DESCRIPTOR_L1_AP2_1                                 (MMU_DESCRIPTOR_L1_AP2(1))  /* AP2=1 */
#define MMU_DESCRIPTOR_L1_AP01_SHIFT                            10  /* AP0-1位偏移量(bit10-11) */
#define MMU_DESCRIPTOR_L1_AP01(x)                               ((x) << MMU_DESCRIPTOR_L1_AP01_SHIFT)  /* 构造AP0-1字段 */
#define MMU_DESCRIPTOR_L1_AP01_0                                (MMU_DESCRIPTOR_L1_AP01(0))  /* AP0-1=00 */
#define MMU_DESCRIPTOR_L1_AP01_1                                (MMU_DESCRIPTOR_L1_AP01(1))  /* AP0-1=01 */
#define MMU_DESCRIPTOR_L1_AP01_3                                (MMU_DESCRIPTOR_L1_AP01(3))  /* AP0-1=11 */
#define MMU_DESCRIPTOR_L1_AP_P_NA_U_NA                          (MMU_DESCRIPTOR_L1_AP2_0 | MMU_DESCRIPTOR_L1_AP01_0)  /* 特权级不可访问，用户级不可访问 */
#define MMU_DESCRIPTOR_L1_AP_P_RW_U_RW                          (MMU_DESCRIPTOR_L1_AP2_0 | MMU_DESCRIPTOR_L1_AP01_3)  /* 特权级读写，用户级读写 */
#define MMU_DESCRIPTOR_L1_AP_P_RW_U_NA                          (MMU_DESCRIPTOR_L1_AP2_0 | MMU_DESCRIPTOR_L1_AP01_1)  /* 特权级读写，用户级不可访问 */
#define MMU_DESCRIPTOR_L1_AP_P_RO_U_RO                          (MMU_DESCRIPTOR_L1_AP2_1 | MMU_DESCRIPTOR_L1_AP01_3)  /* 特权级只读，用户级只读 */
#define MMU_DESCRIPTOR_L1_AP_P_RO_U_NA                          (MMU_DESCRIPTOR_L1_AP2_1 | MMU_DESCRIPTOR_L1_AP01_1)  /* 特权级只读，用户级不可访问 */
#define MMU_DESCRIPTOR_L1_AP_MASK                               (MMU_DESCRIPTOR_L1_AP2_1 | MMU_DESCRIPTOR_L1_AP01_3)  /* AP位字段掩码 */

/*
 * L2小页映射相关宏定义
 * 小页映射粒度为4KB，用于精细内存管理
 */
#define MMU_DESCRIPTOR_L2_SMALL_SIZE                            0x1000     /* L2小页大小：4KB(4096字节) */
#define MMU_DESCRIPTOR_L2_SMALL_MASK                            (MMU_DESCRIPTOR_L2_SMALL_SIZE - 1)  /* 小页内偏移掩码 */
#define MMU_DESCRIPTOR_L2_SMALL_FRAME                           (~MMU_DESCRIPTOR_L2_SMALL_MASK)  /* 小页地址掩码(4KB对齐) */
#define MMU_DESCRIPTOR_L2_SMALL_SHIFT                           12  /* 小页索引移位量(12位对应4KB) */
#define MMU_DESCRIPTOR_L2_NUMBERS_PER_L1                        \
    (MMU_DESCRIPTOR_L1_SMALL_SIZE >> MMU_DESCRIPTOR_L2_SMALL_SHIFT)  /* 每个L1段包含的L2小页数：256(1MB/4KB) */
#define MMU_DESCRIPTOR_IS_L2_SIZE_ALIGNED(x)                    IS_ALIGNED(x, MMU_DESCRIPTOR_L2_SMALL_SIZE)  /* 检查地址是否按L2小页大小对齐 */

/*
 * L2描述符TEX字段组合宏
 * 与L1类似，但TEX字段位置不同
 */
#define MMU_DESCRIPTOR_L2_TEX_SHIFT                             6  /* L2描述符TEX字段偏移量(bit6-8) */
#define MMU_DESCRIPTOR_L2_TEX(x)                                \
    ((x) << MMU_DESCRIPTOR_L2_TEX_SHIFT)  /* 构造L2 TEX字段值 */
#define MMU_DESCRIPTOR_L2_TYPE_STRONGLY_ORDERED                 \
    (MMU_DESCRIPTOR_L2_TEX(MMU_DESCRIPTOR_TEX_0) | MMU_DESCRIPTOR_NON_CACHEABLE)  /* 强序型内存 */
#define MMU_DESCRIPTOR_L2_TYPE_NORMAL_NOCACHE                   \
    (MMU_DESCRIPTOR_L2_TEX(MMU_DESCRIPTOR_TEX_1) | MMU_DESCRIPTOR_NON_CACHEABLE)  /* 普通非缓存内存 */
#define MMU_DESCRIPTOR_L2_TYPE_DEVICE_SHARED                    \
    (MMU_DESCRIPTOR_L2_TEX(MMU_DESCRIPTOR_TEX_0) | MMU_DESCRIPTOR_WRITE_BACK_ALLOCATE)  /* 共享设备内存 */
#define MMU_DESCRIPTOR_L2_TYPE_DEVICE_NON_SHARED                \
    (MMU_DESCRIPTOR_L2_TEX(MMU_DESCRIPTOR_TEX_2) | MMU_DESCRIPTOR_NON_CACHEABLE)  /* 非共享设备内存 */
#define MMU_DESCRIPTOR_L2_TYPE_NORMAL_WRITE_BACK_ALLOCATE       \
    (MMU_DESCRIPTOR_L2_TEX(MMU_DESCRIPTOR_TEX_1) | MMU_DESCRIPTOR_WRITE_BACK_NO_ALLOCATE)  /* 普通回写分配内存 */
#define MMU_DESCRIPTOR_L2_TEX_TYPE_MASK                         \
    (MMU_DESCRIPTOR_L2_TEX(MMU_DESCRIPTOR_TEX_MASK) | MMU_DESCRIPTOR_WRITE_BACK_NO_ALLOCATE)  /* L2内存类型字段掩码 */

/*
 * L2描述符访问权限(AP)位定义
 * 与L1类似，但AP位位置不同
 */
#define MMU_DESCRIPTOR_L2_AP2_SHIFT                             9  /* AP2位偏移量(bit9) */
#define MMU_DESCRIPTOR_L2_AP2(x)                                ((x) << MMU_DESCRIPTOR_L2_AP2_SHIFT)  /* 构造AP2字段 */
#define MMU_DESCRIPTOR_L2_AP2_0                                 (MMU_DESCRIPTOR_L2_AP2(0))  /* AP2=0 */
#define MMU_DESCRIPTOR_L2_AP2_1                                 (MMU_DESCRIPTOR_L2_AP2(1))  /* AP2=1 */
#define MMU_DESCRIPTOR_L2_AP01_SHIFT                            4  /* AP0-1位偏移量(bit4-5) */
#define MMU_DESCRIPTOR_L2_AP01(x)                               ((x) << MMU_DESCRIPTOR_L2_AP01_SHIFT)  /* 构造AP0-1字段 */
#define MMU_DESCRIPTOR_L2_AP01_0                                (MMU_DESCRIPTOR_L2_AP01(0))  /* AP0-1=00 */
#define MMU_DESCRIPTOR_L2_AP01_1                                (MMU_DESCRIPTOR_L2_AP01(1))  /* AP0-1=01 */
#define MMU_DESCRIPTOR_L2_AP01_3                                (MMU_DESCRIPTOR_L2_AP01(3))  /* AP0-1=11 */
#define MMU_DESCRIPTOR_L2_AP_P_NA_U_NA                          (MMU_DESCRIPTOR_L2_AP2_0 | MMU_DESCRIPTOR_L2_AP01_0)  /* 特权级不可访问，用户级不可访问 */
#define MMU_DESCRIPTOR_L2_AP_P_RW_U_RW                          (MMU_DESCRIPTOR_L2_AP2_0 | MMU_DESCRIPTOR_L2_AP01_3)  /* 特权级读写，用户级读写 */
#define MMU_DESCRIPTOR_L2_AP_P_RW_U_NA                          (MMU_DESCRIPTOR_L2_AP2_0 | MMU_DESCRIPTOR_L2_AP01_1)  /* 特权级读写，用户级不可访问 */
#define MMU_DESCRIPTOR_L2_AP_P_RO_U_RO                          (MMU_DESCRIPTOR_L2_AP2_1 | MMU_DESCRIPTOR_L2_AP01_3)  /* 特权级只读，用户级只读 */
#define MMU_DESCRIPTOR_L2_AP_P_RO_U_NA                          (MMU_DESCRIPTOR_L2_AP2_1 | MMU_DESCRIPTOR_L2_AP01_1)  /* 特权级只读，用户级不可访问 */
#define MMU_DESCRIPTOR_L2_AP_MASK                               (MMU_DESCRIPTOR_L2_AP2_1 | MMU_DESCRIPTOR_L2_AP01_3)  /* AP位字段掩码 */

/* L2描述符其他属性位 */
#define MMU_DESCRIPTOR_L2_SHAREABLE                             (1 << 10)  /* 共享位 */
#define MMU_DESCRIPTOR_L2_NON_GLOBAL                            (1 << 11)  /* 非全局位 */
#define MMU_DESCRIPTOR_L2_SMALL_PAGE_ADDR(x)                    ((x) & MMU_DESCRIPTOR_L2_SMALL_FRAME)  /* 获取小页对齐地址 */

/*
 * TTBR(Translation Table Base Register)相关宏定义
 * 用于配置页表基地址寄存器的属性
 */
#define MMU_DESCRIPTOR_TTBCR_PD0                                (1 << 4)  /* TTBCR寄存器PD0位(禁用TTBR0) */
#define MMU_DESCRIPTOR_TTBR_WRITE_BACK_ALLOCATE                 1  /* TTBR回写分配策略 */
#define MMU_DESCRIPTOR_TTBR_RGN(x)                              (((x) & 0x3) << 3)  /* TTBR内存区域属性字段 */
#define MMU_DESCRIPTOR_TTBR_IRGN(x)                             ((((x) & 0x1) << 6) | ((((x) >> 1) & 0x1) << 0))  /* TTBR内部内存区域属性 */
#define MMU_DESCRIPTOR_TTBR_S                                   (1 << 1)  /* TTBR共享位 */
#define MMU_DESCRIPTOR_TTBR_NOS                                 (1 << 5)  /* TTBR非共享覆盖位 */

/* 根据SMP配置定义TTBR共享标志 */
#ifdef LOSCFG_KERNEL_SMP
#define MMU_TTBRx_SHARABLE_FLAGS (MMU_DESCRIPTOR_TTBR_S | MMU_DESCRIPTOR_TTBR_NOS)  /* SMP模式下启用共享标志 */
#else
#define MMU_TTBRx_SHARABLE_FLAGS 0  /* 非SMP模式下禁用共享标志 */
#endif

/* TTBR寄存器组合标志 */
#define MMU_TTBRx_FLAGS                                                     \
    (MMU_DESCRIPTOR_TTBR_RGN(MMU_DESCRIPTOR_TTBR_WRITE_BACK_ALLOCATE) |     \
    MMU_DESCRIPTOR_TTBR_IRGN(MMU_DESCRIPTOR_TTBR_WRITE_BACK_ALLOCATE) |     \
    MMU_TTBRx_SHARABLE_FLAGS)  /* TTBR回写分配+共享标志组合 */

/*
 * 内核L1页表项标志
 * 根据SMP配置是否添加共享属性
 */
#ifdef LOSCFG_KERNEL_SMP
#define MMU_DESCRIPTOR_KERNEL_L1_PTE_FLAGS                                  \
    (MMU_DESCRIPTOR_L1_TYPE_SECTION |                                       \
    MMU_DESCRIPTOR_L1_TYPE_NORMAL_WRITE_BACK_ALLOCATE |                     \
    MMU_DESCRIPTOR_L1_AP_P_RW_U_NA |                                        \
    MMU_DESCRIPTOR_L1_SMALL_DOMAIN_CLIENT |                                 \
    MMU_DESCRIPTOR_L1_SECTION_SHAREABLE)  /* SMP模式：段+回写+特权读写+共享 */
#else
#define MMU_DESCRIPTOR_KERNEL_L1_PTE_FLAGS                                  \
    (MMU_DESCRIPTOR_L1_TYPE_SECTION |                                       \
    MMU_DESCRIPTOR_L1_TYPE_NORMAL_WRITE_BACK_ALLOCATE |                     \
    MMU_DESCRIPTOR_L1_SMALL_DOMAIN_CLIENT |                                 \
    MMU_DESCRIPTOR_L1_AP_P_RW_U_NA)  /* 非SMP模式：段+回写+特权读写 */
#endif

/*
 * 初始内存映射类型定义
 * 用于内核启动阶段的内存区域映射配置
 */
#define MMU_INITIAL_MAP_STRONGLY_ORDERED                                    \
    (MMU_DESCRIPTOR_L1_TYPE_SECTION |                                       \
    MMU_DESCRIPTOR_L1_TYPE_STRONGLY_ORDERED |                               \
    MMU_DESCRIPTOR_L1_SMALL_DOMAIN_CLIENT |                                 \
    MMU_DESCRIPTOR_L1_AP_P_RW_U_NA)  /* 强序型映射(通常用于设备寄存器) */

#define MMU_INITIAL_MAP_NORMAL_NOCACHE                                      \
    (MMU_DESCRIPTOR_L1_TYPE_SECTION |                                       \
    MMU_DESCRIPTOR_L1_TYPE_NORMAL_NOCACHE |                                 \
    MMU_DESCRIPTOR_L1_SMALL_DOMAIN_CLIENT |                                 \
    MMU_DESCRIPTOR_L1_AP_P_RW_U_NA)  /* 普通非缓存映射 */

#define MMU_INITIAL_MAP_DEVICE                                              \
    (MMU_DESCRIPTOR_L1_TYPE_SECTION |                                       \
    MMU_DESCRIPTOR_L1_TYPE_DEVICE_SHARED |                                  \
    MMU_DESCRIPTOR_L1_SMALL_DOMAIN_CLIENT |                                 \
    MMU_DESCRIPTOR_L1_AP_P_RW_U_NA)  /* 设备内存映射 */
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* __LOS_MMU_DESCRIPTOR_V6_H__ */

