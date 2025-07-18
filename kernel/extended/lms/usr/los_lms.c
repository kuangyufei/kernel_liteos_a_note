/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific prior written
 * permission.
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

#include <sys/mman.h>
#include "los_lms_pri.h"
#include "debug.h"
// 定义空闲节点数组大小  
#define LMS_FREE_NODE_SIZE 16  // 最大支持的内存映射节点数量

/**
 * @brief 内存映射节点结构体
 * @details 用于管理影子内存区域的映射信息
 */
struct MmapNode {
    uintptr_t addr;             // 映射起始地址
    size_t mapSize;             // 映射大小
    struct MmapNode *next;      // 下一个节点指针
};

/**
 * @brief 空闲节点数组
 * @details 存储预分配的MmapNode结构体，最大数量为LMS_FREE_NODE_SIZE
 */
struct MmapNode g_freeNode[LMS_FREE_NODE_SIZE];

/**
 * @brief 内存映射节点链表头
 * @details 指向当前使用中的内存映射节点链表
 */
struct MmapNode *g_mmapNode = NULL;

/**
 * @brief 影子内存起始地址
 * @details 初始化为SHADOW_BASE，用于计算内存地址对应的影子内存位置
 */
uint32_t g_shadowStartAddr = SHADOW_BASE;

/**
 * @brief LMS模块互斥锁
 * @details 用于保护LMS模块中共享数据结构的线程安全
 */
pthread_mutex_t g_lmsMutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief 将内存地址转换为影子内存地址及偏移量
 * @param memAddr 内存地址
 * @param shadowAddr 输出参数，影子内存地址
 * @param shadowOffset 输出参数，影子内存偏移量
 * @note 无返回值，通过指针参数返回结果
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void LmsMem2Shadow(uintptr_t memAddr, uintptr_t *shadowAddr, uint32_t *shadowOffset)
{
    uint32_t memOffset = memAddr - USPACE_MAP_BASE;  // 计算内存地址相对用户空间基址的偏移
    *shadowAddr = g_shadowStartAddr + memOffset / LMS_SHADOW_U8_REFER_BYTES;  // 计算影子内存地址
    // 计算影子内存偏移量
    *shadowOffset = ((memOffset % LMS_SHADOW_U8_REFER_BYTES) / LMS_SHADOW_U8_CELL_NUM) * LMS_SHADOW_BITS_PER_CELL;
}

/**
 * @brief 检查影子内存地址是否已映射
 * @param sdStartAddr 影子内存起始地址
 * @param sdEndAddr 影子内存结束地址
 * @return 成功(LMS_OK)或失败(LMS_NOK)
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS uint32_t LmsIsShadowAddrMapped(uintptr_t sdStartAddr, uintptr_t sdEndAddr)
{
    struct MmapNode *node = g_mmapNode;  // 从链表头开始遍历
    while (node != NULL) {
        // 检查地址范围是否在当前节点的映射范围内
        if ((sdStartAddr >= node->addr) && (sdEndAddr < node->addr + node->mapSize)) {
            return LMS_OK;  // 已映射，返回成功
        }
        node = node->next;  // 遍历下一个节点
    }
    return LMS_NOK;  // 未找到映射，返回失败
}

/**
 * @brief 添加影子内存映射节点
 * @param sdStartAddr 影子内存起始地址
 * @param sdEndAddr 影子内存结束地址
 * @note 无返回值，内部处理内存映射和节点管理
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void LmsAddMapNode(uintptr_t sdStartAddr, uintptr_t sdEndAddr)
{
    static uint32_t freeNodeIdx = 0;  // 静态变量，跟踪空闲节点索引
    struct MmapNode *node = g_mmapNode;  // 从链表头开始遍历
    size_t mapSize;  // 映射大小
    // 计算页面对齐的起始和结束地址
    uintptr_t shadowPageStartAddr = LMS_MEM_ALIGN_DOWN(sdStartAddr, 0x1000);
    uintptr_t shadowPageEndAddr = LMS_MEM_ALIGN_UP(sdEndAddr, 0x1000);

    struct MmapNode *expandNode = NULL;  // 可扩展的节点指针

    // 查找是否有可扩展的现有节点
    while (node != NULL) {
        if ((shadowPageStartAddr >= node->addr) && (shadowPageStartAddr <= node->addr + node->mapSize)) {
            expandNode = node;  // 找到可扩展节点
            break;
        }
        node = node->next;  // 遍历下一个节点
    }

    if (expandNode != NULL) {
        // 调整起始地址并扩展现有节点
        shadowPageStartAddr = expandNode->addr + expandNode->mapSize;
        expandNode->mapSize = shadowPageEndAddr - expandNode->addr;
    }

    mapSize = shadowPageEndAddr - shadowPageStartAddr;  // 计算映射大小
    // 创建匿名内存映射
    void *mapPtr =
        mmap((void *)shadowPageStartAddr, mapSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapPtr == (void *)-1) {
        LMS_OUTPUT_INFO("mmap error! file:%s line:%d\n", __FILE__, __LINE__);  // 映射失败，输出错误信息
        return;
    }
    __real_memset(mapPtr, 0, mapSize);  // 初始化映射内存为0

    if (expandNode != NULL) {
        return;  // 如果是扩展现有节点，无需创建新节点
    }

    if (freeNodeIdx >= LMS_FREE_NODE_SIZE) {
        LMS_OUTPUT_INFO("Add new mmap node error! file:%s line:%d\n", __FILE__, __LINE__);  // 空闲节点不足，输出错误
        return;
    }

    // 使用空闲节点创建新的映射节点
    struct MmapNode *newNode = &g_freeNode[freeNodeIdx];
    freeNodeIdx++;
    newNode->addr = shadowPageStartAddr;  // 设置映射起始地址
    newNode->mapSize = mapSize;  // 设置映射大小
    newNode->next = g_mmapNode;  // 插入到链表头部
    g_mmapNode = newNode;  // 更新链表头指针
}

/**
 * @brief 设置影子内存区域的值
 * @param startAddr 起始地址
 * @param endAddr 结束地址
 * @param value 要设置的值
 * @note 无返回值，用于标记内存区域的可访问性
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void LmsSetShadowValue(uintptr_t startAddr, uintptr_t endAddr, char value)
{
    uintptr_t shadowStart;  // 影子内存起始地址
    uintptr_t shadowEnd;    // 影子内存结束地址
    uint32_t startOffset;   // 起始偏移量
    uint32_t endOffset;     // 结束偏移量

    unsigned char shadowValueMask;  // 影子值掩码
    unsigned char shadowValue;      // 影子值

    // 计算起始和结束地址对应的影子内存地址和偏移量
    LmsMem2Shadow(startAddr, &shadowStart, &startOffset);
    LmsMem2Shadow(endAddr - 1, &shadowEnd, &endOffset);

    if (shadowStart == shadowEnd) {  // 地址范围在同一个字节内
        // 计算掩码，标记需要设置的位区域
        shadowValueMask = LMS_SHADOW_MASK_U8;
        shadowValueMask =
            (shadowValueMask << startOffset) & (~(shadowValueMask << (endOffset + LMS_SHADOW_BITS_PER_CELL)));
        shadowValue = value & shadowValueMask;  // 计算要设置的影子值
        *(char *)shadowStart &= ~shadowValueMask;  // 清除原有位
        *(char *)shadowStart |= shadowValue;  // 设置新值
    } else {
        // 处理起始地址所在的字节
        if (startOffset > 0) {
            shadowValueMask = LMS_SHADOW_MASK_U8;
            shadowValueMask = shadowValueMask << startOffset;  // 计算起始掩码
            shadowValue = value & shadowValueMask;  // 计算起始影子值
            *(char *)shadowStart &= ~shadowValueMask;  // 清除起始位
            *(char *)shadowStart |= shadowValue;  // 设置起始位
            shadowStart += 1;  // 移动到下一个字节
        }

        // 处理结束地址所在的字节
        if (endOffset < (LMS_SHADOW_U8_CELL_NUM - 1) * LMS_SHADOW_BITS_PER_CELL) {
            shadowValueMask = LMS_SHADOW_MASK_U8;
            shadowValueMask &= ~(shadowValueMask << (endOffset + LMS_SHADOW_BITS_PER_CELL));  // 计算结束掩码
            shadowValue = value & shadowValueMask;  // 计算结束影子值
            *(char *)shadowEnd &= ~shadowValueMask;  // 清除结束位
            *(char *)shadowEnd |= shadowValue;  // 设置结束位
            shadowEnd -= 1;  // 移动到上一个字节
        }

        // 批量设置中间完整字节区域
        if (shadowEnd + 1 > shadowStart) {
            (void)__real_memset((void *)shadowStart, value & LMS_SHADOW_MASK_U8, shadowEnd + 1 - shadowStart);
        }
    }
}

/**
 * @brief 获取指定地址的影子内存值
 * @param addr 内存地址
 * @param shadowValue 输出参数，影子内存值
 * @note 无返回值，通过指针参数返回结果
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void LmsGetShadowValue(uintptr_t addr, uint32_t *shadowValue)
{
    uintptr_t shadowAddr;  // 影子内存地址
    uint32_t shadowOffset;  // 影子内存偏移量
    LmsMem2Shadow(addr, &shadowAddr, &shadowOffset);  // 转换为影子内存地址和偏移量
    // 检查影子地址是否已映射
    if (LmsIsShadowAddrMapped(shadowAddr, shadowAddr) != LMS_OK) {
        *shadowValue = LMS_SHADOW_ACCESSIBLE_U8;  // 未映射，视为可访问
        return;
    }

    // 从影子内存中读取值并返回
    *shadowValue = ((*(char *)shadowAddr) >> shadowOffset) & LMS_SHADOW_MASK;
}

/**
 * @brief 标记内存分配区域
 * @param preRzStart 前红区起始地址
 * @param accessMemStart 可访问内存起始地址
 * @param nextRzStart 后红区起始地址
 * @param RzEndAddr 红区结束地址
 * @note 无返回值，用于设置内存区域的影子标记
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void LmsMallocMark(uintptr_t preRzStart, uintptr_t accessMemStart, uintptr_t nextRzStart,
    uintptr_t RzEndAddr)
{
    // 标记前红区
    LmsSetShadowValue(preRzStart, accessMemStart, LMS_SHADOW_REDZONE_U8);
    // 标记可访问区域
    LmsSetShadowValue(accessMemStart, nextRzStart, LMS_SHADOW_ACCESSIBLE_U8);
    // 标记后红区
    LmsSetShadowValue(nextRzStart, RzEndAddr, LMS_SHADOW_REDZONE_U8);
}

/**
 * @brief 标记内存块的影子信息
 * @param ptr 内存块指针
 * @param origSize 请求的内存大小
 * @return 内存块指针
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void *LmsTagMem(void *ptr, size_t origSize)
{
    g_shadowStartAddr = SHADOW_BASE;  // 重置影子内存起始地址
    size_t mSize = malloc_usable_size(ptr);  // 获取实际分配的内存大小
    // 计算影子内存结束地址
    uint32_t memOffset = (uintptr_t)ptr + mSize + OVERHEAD - USPACE_MAP_BASE;
    uintptr_t shadowEndAddr = g_shadowStartAddr + memOffset / LMS_SHADOW_U8_REFER_BYTES;
    LMS_OUTPUT_INFO("SHADOW_BASE:%x g_shadowStartAddr:%x memOffset: %x\n", SHADOW_BASE, g_shadowStartAddr, memOffset);
    // 计算影子内存起始地址
    memOffset = (uintptr_t)ptr - OVERHEAD - USPACE_MAP_BASE;
    uintptr_t shadowStartAddr = g_shadowStartAddr + memOffset / LMS_SHADOW_U8_REFER_BYTES;

    LmsLock(&g_lmsMutex);  // 加锁保护共享资源
    // 检查影子内存是否已映射
    if (LmsIsShadowAddrMapped(shadowStartAddr, shadowEndAddr) != LMS_OK) {
        LmsAddMapNode(shadowStartAddr, shadowEndAddr);  // 添加新的映射
    }

    // 标记内存区域
    LmsMallocMark((uintptr_t)ptr - OVERHEAD, (uintptr_t)ptr, (uintptr_t)ptr + LMS_MEM_ALIGN_UP(origSize, LMS_RZ_SIZE),
        (uintptr_t)((uintptr_t)ptr + mSize + OVERHEAD));
    LmsUnlock(&g_lmsMutex);  // 解锁

    return ptr;  // 返回内存块指针
}

/**
 * @brief 检查地址的影子内存值
 * @param addr 内存地址
 * @return 影子内存值
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS uint32_t LmsCheckAddr(uintptr_t addr)
{
    LmsLock(&g_lmsMutex);  // 加锁保护共享资源
    uint32_t shadowValue = -1;  // 初始化影子值
    LmsGetShadowValue(addr, &shadowValue);  // 获取影子值
    LmsUnlock(&g_lmsMutex);  // 解锁
    return shadowValue;  // 返回影子值
}

/**
 * @brief 检查地址区域的合法性
 * @param addr 起始地址
 * @param size 区域大小
 * @return 合法(LMS_OK)或非法(LMS_NOK)
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS uint32_t LmsCheckAddrRegion(uintptr_t addr, size_t size)
{
    // 检查起始地址和结束地址
    if (LmsCheckAddr(addr) || LmsCheckAddr(addr + size - 1)) {
        return LMS_NOK;  // 存在非法地址
    } else {
        return LMS_OK;  // 地址区域合法
    }
}

/**
 * @brief 打印内存地址周围的信息
 * @param addr 内存地址
 * @note 无返回值，输出内存和影子内存的详细信息
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void LmsPrintMemInfo(uintptr_t addr)
{
#define LMS_DUMP_OFFSET 4  // 转储偏移量
#define LMS_DUMP_RANGE_DOUBLE 2  // 转储范围倍数

    LMS_OUTPUT_INFO("\nDump info around address [0x%8x]:\n", addr);
    uint32_t printY = LMS_DUMP_OFFSET * LMS_DUMP_RANGE_DOUBLE + 1;  // Y轴打印行数
    uint32_t printX = LMS_MEM_BYTES_PER_SHADOW_CELL * LMS_DUMP_RANGE_DOUBLE;  // X轴打印字节数
    uintptr_t dumpAddr = addr - addr % printX - LMS_DUMP_OFFSET * printX;  // 起始转储地址
    uint32_t shadowValue = 0;  // 影子值
    uintptr_t shadowAddr = 0;  // 影子地址
    uint32_t shadowOffset = 0;  // 影子偏移量
    int isCheckAddr;  // 是否为检查地址

    // 遍历打印行
    for (int y = 0; y < printY; y++, dumpAddr += printX) {
        LmsMem2Shadow(dumpAddr, &shadowAddr, &shadowOffset);  // 获取影子地址和偏移量
        // 检查是否在映射区域内
        if (LmsIsShadowAddrMapped(shadowAddr, shadowAddr) != LMS_OK) {
            continue;  // 跳过未映射区域
        }
        uintptr_t maxShadowAddr;  // 最大影子地址
        uint32_t maxShadowOffset;  // 最大影子偏移量
        LmsMem2Shadow(dumpAddr + printX, &maxShadowAddr, &maxShadowOffset);  // 获取结束影子地址
        // 检查是否超出映射区域
        if (LmsIsShadowAddrMapped(maxShadowAddr, maxShadowAddr) != LMS_OK) {
            goto END;  // 跳出循环
        }

        LMS_OUTPUT_INFO("\n\t[0x%x]: ", dumpAddr);  // 打印内存地址
        // 打印内存内容
        for (int x = 0; x < printX; x++) {
            if (dumpAddr + x == addr) {
                LMS_OUTPUT_INFO("[%02x]", *(uint8_t *)(dumpAddr + x));  // 标记检查地址
            } else {
                LMS_OUTPUT_INFO(" %02x ", *(uint8_t *)(dumpAddr + x));  // 打印普通地址
            }
        }

        LMS_OUTPUT_INFO("|\t[0x%x | %2u]: ", shadowAddr, shadowOffset);  // 打印影子地址和偏移量

        // 打印影子内存值
        for (int x = 0; x < printX; x += LMS_MEM_BYTES_PER_SHADOW_CELL) {
            LmsGetShadowValue(dumpAddr + x, &shadowValue);  // 获取影子值
            isCheckAddr = dumpAddr + x - (uintptr_t)addr + LMS_MEM_BYTES_PER_SHADOW_CELL;
            if (isCheckAddr > 0 && isCheckAddr <= LMS_MEM_BYTES_PER_SHADOW_CELL) {
                LMS_OUTPUT_INFO("[%1x]", shadowValue);  // 标记检查地址的影子值
            } else {
                LMS_OUTPUT_INFO(" %1x ", shadowValue);  // 打印普通影子值
            }
        }
    }
END:
    LMS_OUTPUT_INFO("\n");
}
/**
 * @brief 获取内存地址对应的影子内存信息
 * @param memAddr 要查询的内存地址
 * @param info 输出参数，用于存储影子内存信息
 * @note 该函数不会进行地址有效性检查，调用前需确保memAddr有效
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS static inline void LmsGetShadowInfo(uintptr_t memAddr, LmsAddrInfo *info)
{
    uintptr_t shadowAddr;   // 影子内存地址
    uint32_t shadowOffset;  // 影子内存偏移量
    uint32_t shadowValue;   // 影子内存值

    LmsMem2Shadow(memAddr, &shadowAddr, &shadowOffset);  // 将内存地址转换为影子内存地址和偏移量

    shadowValue = ((*(char *)shadowAddr) >> shadowOffset) & LMS_SHADOW_MASK;  // 计算影子内存值
    info->memAddr = memAddr;        // 设置原始内存地址
    info->shadowAddr = shadowAddr;  // 设置影子内存地址
    info->shadowOffset = shadowOffset;  // 设置影子内存偏移量
    info->shadowValue = shadowValue;    // 设置影子内存值
}

/**
 * @brief 获取内存区域的错误信息
 * @param addr 内存区域起始地址
 * @param size 内存区域大小
 * @param info 输出参数，用于存储错误信息
 * @note 优先检查起始地址，若起始地址正常则检查结束地址
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS static void LmsGetErrorInfo(uintptr_t addr, size_t size, LmsAddrInfo *info)
{
    LmsGetShadowInfo(addr, info);  // 获取起始地址的影子信息
    if (info->shadowValue != LMS_SHADOW_ACCESSIBLE_U8) {
        return;  // 起始地址异常，直接返回
    } else {
        LmsGetShadowInfo(addr + size - 1, info);  // 起始地址正常，检查结束地址
    }
}

/**
 * @brief 打印内存错误详细信息
 * @param info 影子内存信息结构体
 * @param errMod 错误模式（FREE_ERRORMODE/LOAD_ERRMODE/STORE_ERRMODE）
 * @note 输出错误类型、地址信息及影子内存状态
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS static void LmsPrintErrInfo(LmsAddrInfo *info, uint32_t errMod)
{
    switch (info->shadowValue) {  // 根据影子值判断错误类型
        case LMS_SHADOW_AFTERFREE:
            LMS_OUTPUT_ERROR("Use after free error detected!\n");
            break;
        case LMS_SHADOW_REDZONE:
            LMS_OUTPUT_ERROR("Heap buffer overflow error detected!\n");
            break;
        case LMS_SHADOW_ACCESSIBLE:
            LMS_OUTPUT_ERROR("No error!\n");
            break;
        default:
            LMS_OUTPUT_ERROR("UnKnown Error detected!\n");
            break;
    }

    switch (errMod) {
        case FREE_ERRORMODE:
            LMS_OUTPUT_ERROR("Illegal Double free address at: [0x%x]\n", info->memAddr);
            break;
        case LOAD_ERRMODE:
            LMS_OUTPUT_ERROR("Illegal READ address at: [0x%x]\n", info->memAddr);
            break;
        case STORE_ERRMODE:
            LMS_OUTPUT_ERROR("Illegal WRITE address at: [0x%x]\n", info->memAddr);
            break;
        default:
            LMS_OUTPUT_ERROR("UnKnown Error mode at: [0x%x]\n", info->memAddr);
            break;
    }

    LMS_OUTPUT_INFO("Shadow memory address: [0x%x : %u]  Shadow memory value: [%u] \n", info->shadowAddr,
        info->shadowOffset, info->shadowValue);

    LMS_OUTPUT_INFO("\n");
    LMS_OUTPUT_INFO("%-25s%d\n", "Accessible heap addr", LMS_SHADOW_ACCESSIBLE);
    LMS_OUTPUT_INFO("%-25s%d\n", "Heap red zone", LMS_SHADOW_REDZONE);
    LMS_OUTPUT_INFO("%-25s%d\n", "Heap freed buffer", LMS_SHADOW_AFTERFREE);
    LMS_OUTPUT_INFO("\n");
}

/**
 * @brief 报告内存错误并输出调试信息
 * @param p 发生错误的内存地址
 * @param size 内存操作大小
 * @param errMod 错误模式（FREE_ERRORMODE/LOAD_ERRMODE/STORE_ERRMODE）
 * @note 根据LMS_CRASH_MODE配置决定是否触发系统崩溃
 */
ATTRIBUTE_NO_SANITIZE_ADDRESS void LmsReportError(uintptr_t p, size_t size, uint32_t errMod)
{
    LmsAddrInfo info;
    (void)__real_memset(&info, 0, sizeof(LmsAddrInfo));  // 初始化错误信息结构体

    int locked = LmsTrylock(&g_lmsMutex);  // 尝试加锁保护错误报告
    LMS_OUTPUT_ERROR("\n*****  Lite Memory Sanitizer Error Detected  *****\n");
    LmsGetErrorInfo(p, size, &info);  // 获取错误地址信息
    LmsPrintErrInfo(&info, errMod);   // 打印错误详细信息
    LmsPrintMemInfo(info.memAddr);    // 打印内存分配信息
    LMS_OUTPUT_ERROR("*****  Lite Memory Sanitizer Error Detected End *****\n");
    if (!locked) {
        LmsUnlock(&g_lmsMutex);  // 若已加锁则释放锁
    }

    if (LMS_CRASH_MODE > 0) {
        LmsCrash();  // 配置为崩溃模式时触发系统崩溃
    } else {
        print_trace();  // 非崩溃模式下打印调用栈
    }
}

/**
 * @brief 检查字符串操作的源地址和目标地址有效性
 * @param dest 目标字符串地址
 * @param src 源字符串地址
 * @note 逐字符检查字符串操作过程中的内存访问合法性
 */
void LmsCheckValid(const char *dest, const char *src)
{
    if (LmsCheckAddr((uintptr_t)dest) != LMS_SHADOW_ACCESSIBLE_U8) {
        LmsReportError((uintptr_t)dest, MEM_REGION_SIZE_1, STORE_ERRMODE);  // 检查目标地址有效性
        return;
    }

    if (LmsCheckAddr((uintptr_t)src) != LMS_SHADOW_ACCESSIBLE_U8) {
        LmsReportError((uintptr_t)src, MEM_REGION_SIZE_1, LOAD_ERRMODE);  // 检查源地址有效性
        return;
    }

    for (uint32_t i = 0; *(src + i) != '\0'; i++) {  // 遍历字符串检查每个字符
        if (LmsCheckAddr((uintptr_t)dest + i + 1) != LMS_SHADOW_ACCESSIBLE_U8) {
            LmsReportError((uintptr_t)dest + i + 1, MEM_REGION_SIZE_1, STORE_ERRMODE);  // 检查目标地址边界
            return;
        }

        if (LmsCheckAddr((uintptr_t)src + i + 1) != LMS_SHADOW_ACCESSIBLE_U8) {
            LmsReportError((uintptr_t)src + i + 1, MEM_REGION_SIZE_1, LOAD_ERRMODE);  // 检查源地址边界
            return;
        }
    }
}

/**
 * @brief 1字节存储操作的内存安全性检查
 * @param p 存储地址
 * @note 不中止程序，仅在检测到错误时报告
 */
void __asan_store1_noabort(uintptr_t p)
{
    if (LmsCheckAddr(p) != LMS_SHADOW_ACCESSIBLE_U8) {
        LmsReportError(p, MEM_REGION_SIZE_1, STORE_ERRMODE);  // 检查1字节存储地址
    }
}

/**
 * @brief 2字节存储操作的内存安全性检查
 * @param p 存储地址
 * @note 不中止程序，仅在检测到错误时报告
 */
void __asan_store2_noabort(uintptr_t p)
{
    if (LmsCheckAddrRegion(p, MEM_REGION_SIZE_2) != LMS_OK) {
        LmsReportError(p, MEM_REGION_SIZE_2, STORE_ERRMODE);  // 检查2字节存储区域
    }
}

/**
 * @brief 4字节存储操作的内存安全性检查
 * @param p 存储地址
 * @note 不中止程序，仅在检测到错误时报告
 */
void __asan_store4_noabort(uintptr_t p)
{
    if (LmsCheckAddrRegion(p, MEM_REGION_SIZE_4) != LMS_OK) {
        LmsReportError(p, MEM_REGION_SIZE_4, STORE_ERRMODE);  // 检查4字节存储区域
    }
}

/**
 * @brief 8字节存储操作的内存安全性检查
 * @param p 存储地址
 * @note 不中止程序，仅在检测到错误时报告
 */
void __asan_store8_noabort(uintptr_t p)
{
    if (LmsCheckAddrRegion(p, MEM_REGION_SIZE_8) != LMS_OK) {
        LmsReportError(p, MEM_REGION_SIZE_8, STORE_ERRMODE);  // 检查8字节存储区域
    }
}

/**
 * @brief 16字节存储操作的内存安全性检查
 * @param p 存储地址
 * @note 不中止程序，仅在检测到错误时报告
 */
void __asan_store16_noabort(uintptr_t p)
{
    if (LmsCheckAddrRegion(p, MEM_REGION_SIZE_16) != LMS_OK) { /* 检查16字节内存区域 */
        LmsReportError(p, MEM_REGION_SIZE_16, STORE_ERRMODE);
    }
}

/**
 * @brief 任意大小存储操作的内存安全性检查
 * @param p 存储地址
 * @param size 存储大小
 * @note 不中止程序，仅在检测到错误时报告
 */
void __asan_storeN_noabort(uintptr_t p, size_t size)
{
    if (LmsCheckAddrRegion(p, size) != LMS_OK) {
        LmsReportError(p, size, STORE_ERRMODE);  // 检查指定大小存储区域
    }
}

/**
 * @brief 1字节加载操作的内存安全性检查
 * @param p 加载地址
 * @note 不中止程序，仅在检测到错误时报告
 */
void __asan_load1_noabort(uintptr_t p)
{
    if (LmsCheckAddr(p) != LMS_SHADOW_ACCESSIBLE_U8) {
        LmsReportError(p, MEM_REGION_SIZE_1, LOAD_ERRMODE);  // 检查1字节加载地址
    }
}

/**
 * @brief 2字节加载操作的内存安全性检查
 * @param p 加载地址
 * @note 不中止程序，仅在检测到错误时报告
 */
void __asan_load2_noabort(uintptr_t p)
{
    if (LmsCheckAddrRegion(p, MEM_REGION_SIZE_2) != LMS_OK) {
        LmsReportError(p, MEM_REGION_SIZE_2, LOAD_ERRMODE);  // 检查2字节加载区域
    }
}

/**
 * @brief 4字节加载操作的内存安全性检查
 * @param p 加载地址
 * @note 不中止程序，仅在检测到错误时报告
 */
void __asan_load4_noabort(uintptr_t p)
{
    if (LmsCheckAddrRegion(p, MEM_REGION_SIZE_4) != LMS_OK) {
        LmsReportError(p, MEM_REGION_SIZE_4, LOAD_ERRMODE);  // 检查4字节加载区域
    }
}

/**
 * @brief 8字节加载操作的内存安全性检查
 * @param p 加载地址
 * @note 不中止程序，仅在检测到错误时报告
 */
void __asan_load8_noabort(uintptr_t p)
{
    if (LmsCheckAddrRegion(p, MEM_REGION_SIZE_8) != LMS_OK) {
        LmsReportError(p, MEM_REGION_SIZE_8, LOAD_ERRMODE);  // 检查8字节加载区域
    }
}

/**
 * @brief 16字节加载操作的内存安全性检查
 * @param p 加载地址
 * @note 不中止程序，仅在检测到错误时报告
 */
void __asan_load16_noabort(uintptr_t p)
{
    if (LmsCheckAddrRegion(p, MEM_REGION_SIZE_16) != LMS_OK) {
        LmsReportError(p, MEM_REGION_SIZE_16, LOAD_ERRMODE);  // 检查16字节加载区域
    }
}

/**
 * @brief 任意大小加载操作的内存安全性检查
 * @param p 加载地址
 * @param size 加载大小
 * @note 不中止程序，仅在检测到错误时报告
 */
void __asan_loadN_noabort(uintptr_t p, size_t size)
{
    if (LmsCheckAddrRegion(p, size) != LMS_OK) {
        LmsReportError(p, size, LOAD_ERRMODE);  // 检查指定大小加载区域
    }
}

/**
 * @brief ASAN无返回处理函数
 * @note 空实现，用于满足ASAN接口要求
 */
void __asan_handle_no_return(void)
{
    return;
}