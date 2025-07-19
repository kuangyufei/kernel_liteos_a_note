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
 * @ingroup los_vm
 * @brief MMU初始映射标志位定义 - 临时映射标志
 * @details 用于标识该映射为临时映射，系统启动后可能会被释放或修改
 * @note 位掩码值：0x1 (bit 0)
 */
#define MMU_INITIAL_MAPPING_TEMPORARY       (0x1)  /* 临时映射标志位 */

/**
 * @ingroup los_vm
 * @brief MMU初始映射标志位定义 - 非缓存映射标志
 * @details 用于标识该映射区域不启用缓存
 * @note 位掩码值：0x2 (bit 1)
 * @attention 适用于需要直接访问物理内存的设备寄存器区域
 */
#define MMU_INITIAL_MAPPING_FLAG_UNCACHED   (0x2)  /* 非缓存映射标志位 */

/**
 * @ingroup los_vm
 * @brief MMU初始映射标志位定义 - 设备映射标志
 * @details 用于标识该映射区域为设备内存
 * @note 位掩码值：0x4 (bit 2)
 * @attention 通常与非缓存标志配合使用，用于外设寄存器映射
 */
#define MMU_INITIAL_MAPPING_FLAG_DEVICE     (0x4)  /* 设备内存映射标志位 */

/**
 * @ingroup los_vm
 * @brief MMU初始映射标志位定义 - 动态映射标志
 * @details 用于标识该映射为动态映射，可在运行时调整
 * @note 位掩码值：0x8 (bit 3)
 */
#define MMU_INITIAL_MAPPING_FLAG_DYNAMIC    (0x8)  /* 动态映射标志位 */

#ifndef ASSEMBLY  /* 如果不是汇编代码，则包含以下C语言定义 */

#ifndef __LOS_VM_BOOT_H__  /* 防止头文件重复包含 */
#define __LOS_VM_BOOT_H__

#include "los_typedef.h"  /* 包含基础类型定义 */

#ifdef __cplusplus        /* C++兼容性处理 */
#if __cplusplus
extern "C" {            /* 确保C++编译器按C语言规则编译 */
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_vm
 * @brief 内核堆块大小定义
 * @details 内核堆初始化时的块大小
 * @value 512 * 1024UL = 524288字节 (512KB)
 */
#define OS_KHEAP_BLOCK_SIZE                 (512 * 1024UL)  /* 内核堆块大小：512KB */

/**
 * @ingroup los_vm
 * @brief MMU初始映射表结构体
 * @details 定义系统启动阶段MMU初始化所需的映射关系
 * @note 用于内核启动时建立物理地址到虚拟地址的初始映射
 */
typedef struct ArchMmuInitMapping {
    PADDR_T phys;          /* 物理地址起始值 */
    VADDR_T virt;          /* 虚拟地址起始值 */
    size_t  size;          /* 映射区域大小(字节) */
    unsigned int flags;    /* 映射标志位(见MMU_INITIAL_MAPPING_xxx宏定义) */
    const char *name;      /* 映射区域名称(调试用) */
} LosArchMmuInitMapping;

/**
 * @ingroup los_vm
 * @brief 全局MMU初始映射表
 * @details 存储系统所有初始映射项的数组
 * @note 在具体架构的实现文件中初始化
 */
extern LosArchMmuInitMapping g_archMmuInitMapping[];

/**
 * @ingroup los_vm
 * @brief 引导阶段内存基地址
 * @details 内核启动阶段可用内存的起始地址
 * @note 由启动代码初始化，用于早期内存分配
 */
extern UINTPTR g_vmBootMemBase;

/**
 * @ingroup los_vm
 * @brief 内核堆初始化标志
 * @details 指示内核堆是否已初始化完成
 * @value TRUE - 已初始化；FALSE - 未初始化
 */
extern BOOL g_kHeapInited;

/**
 * @ingroup los_vm
 * @brief 虚拟地址有效性检查函数
 * @param[in] tempAddr 待检查的虚拟地址
 * @param[in] length   地址范围长度
 * @retval LOS_OK      地址有效
 * @retval LOS_NOK     地址无效
 * @details 检查指定虚拟地址范围是否在有效的内核地址空间内
 */
UINT32 OsVmAddrCheck(size_t tempAddr, size_t length);

/**
 * @ingroup los_vm
 * @brief 引导阶段内存分配函数
 * @param[in] len 申请的内存大小(字节)
 * @return 成功：分配的内存起始地址；失败：NULL
 * @details 从引导阶段可用内存中分配连续物理内存
 * @note 用于内核堆初始化前的早期内存分配
 */
VOID *OsVmBootMemAlloc(size_t len);

/**
 * @ingroup los_vm
 * @brief 系统内存初始化函数
 * @retval LOS_OK      初始化成功
 * @retval LOS_NOK     初始化失败
 * @details 完成系统内存管理模块的初始化，包括内存区域划分和内核堆创建
 */
UINT32 OsSysMemInit(VOID);

/**
 * @ingroup los_vm
 * @brief 启动阶段映射初始化函数
 * @details 建立系统启动所需的初始内存映射表
 * @note 在MMU使能前调用，完成关键区域的地址映射
 */
VOID OsInitMappingStartUp(VOID);

#ifdef __cplusplus        /* C++兼容性处理 */
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* __LOS_VM_BOOT_H__ */  /* 头文件结束 */
#endif /* ASSEMBLY */          /* 汇编代码条件编译结束 */
