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

#include "asm/page.h"
#include "time.h"
#include "sys/time.h"
#include "los_typedef.h"
#include "los_vdso_datapage.h"
/**
 * @brief 获取粗粒度实时时钟时间
 * @details 从VDSO数据页读取实时时钟（墙上时间），采用忙等待机制确保数据一致性
 * @param ts [输出] 指向timespec结构体的指针，用于存储获取的时间值
 * @param usrVdsoDataPage [输入] 指向VDSO数据页的常量指针，包含系统时间信息
 * @return INT32 成功返回0；失败返回非0值
 * @note 实时时钟可被系统管理员调整，可能存在不连续跳转
 */
STATIC INT32 VdsoGetRealtimeCoarse(struct timespec *ts, const VdsoDataPage *usrVdsoDataPage)
{
    do {  // 忙等待循环，直到数据页解锁
        if (!usrVdsoDataPage->lockCount) {  // 检查数据页锁状态，0表示未锁定
            ts->tv_sec = usrVdsoDataPage->realTimeSec;      // 秒数（自Epoch以来的秒数）
            ts->tv_nsec = usrVdsoDataPage->realTimeNsec;    // 纳秒数（0-999,999,999）
            return 0;                                       // 成功返回
        }
    } while (1);  // 锁被占用时持续等待
}

/**
 * @brief 获取粗粒度单调时钟时间
 * @details 从VDSO数据页读取单调时钟时间，该时间自系统启动后单调递增
 * @param ts [输出] 指向timespec结构体的指针，用于存储获取的时间值
 * @param usrVdsoDataPage [输入] 指向VDSO数据页的常量指针，包含系统时间信息
 * @return INT32 成功返回0；失败返回非0值
 * @note 单调时钟不受系统时间调整影响，适用于测量时间间隔
 */
STATIC INT32 VdsoGetMonotimeCoarse(struct timespec *ts, const VdsoDataPage *usrVdsoDataPage)
{
    do {  // 忙等待循环，确保读取有效数据
        if (!usrVdsoDataPage->lockCount) {  // 检查数据页锁状态
            ts->tv_sec = usrVdsoDataPage->monoTimeSec;      // 秒数（自系统启动以来的秒数）
            ts->tv_nsec = usrVdsoDataPage->monoTimeNsec;    // 纳秒数（0-999,999,999）
            return 0;                                       // 成功返回
        }
    } while (1);  // 持续等待直到锁释放
}

/**
 * @brief 定位VDSO（虚拟动态共享对象）的起始地址
 * @details 通过匹配ELF文件头特征在内存中查找VDSO区域的起始位置
 * @param vdsoStart [输入] VDSO起始地址的初始猜测值（通常基于当前PC值）
 * @param elfHead [输入] ELF文件头特征数据（用于匹配定位）
 * @param len [输入] ELF文件头特征数据长度（字节数）
 * @return size_t 成功返回VDSO实际起始地址；失败返回0
 * @note 最大搜索范围为MAX_PAGES个内存页，每个页大小为PAGE_SIZE
 */
STATIC size_t LocVdsoStart(size_t vdsoStart, const CHAR *elfHead, const size_t len)
{
    CHAR *head = NULL;    // 内存页数据指针
    INT32 i;              // 页计数器
    INT32 loop;           // 字节匹配计数器

    // 在最大MAX_PAGES（默认32）个内存页范围内查找VDSO起始地址
    for (i = 1; i <= MAX_PAGES; i++) {
        head = (CHAR *)((UINTPTR)vdsoStart);  // 将地址转换为字符指针以逐字节访问
        // 从后向前匹配ELF头特征（提高定位准确性）
        for (loop = len; loop >= 1; loop--) {
            if (*head++ != *(elfHead + len - loop)) {  // 比较内存数据与特征值
                break;  // 特征不匹配，跳出内层循环
            }
        }
        if (loop) {  // loop>0表示未完全匹配
            vdsoStart -= PAGE_SIZE;  // 向前移动一个页（4KB）继续查找
        } else {
            break;  // 找到匹配的ELF头，跳出外层循环
        }
    }

    if (i > MAX_PAGES) {  // 超过最大搜索页数，查找失败
        return 0;        // 返回0表示定位失败
    }
    return (vdsoStart - PAGE_SIZE);  // 返回VDSO实际起始地址
}

/**
 * @brief VDSO时钟获取系统调用实现
 * @details 用户空间直接访问VDSO获取时钟时间，避免系统调用开销
 * @param clk [输入] 时钟类型，支持CLOCK_REALTIME_COARSE和CLOCK_MONOTONIC_COARSE
 * @param ts [输出] 指向timespec结构体的指针，用于存储获取的时间值
 * @return INT32 成功返回0；失败返回-1
 * @note 不支持的时钟类型或VDSO定位失败时返回-1
 */
INT32 VdsoClockGettime(clockid_t clk, struct timespec *ts)
{
    INT32 ret;           // 返回值
    size_t vdsoStart;    // VDSO起始地址

    __asm__ __volatile__("mov %0, pc" : "=r"(vdsoStart));  // 内联汇编获取当前PC寄存器值
    vdsoStart = vdsoStart - (vdsoStart & (PAGE_SIZE - 1));  // 将地址对齐到页边界（4KB对齐）
    vdsoStart = LocVdsoStart(vdsoStart, ELF_HEAD, ELF_HEAD_LEN);  // 定位VDSO起始地址
    if (vdsoStart == 0) {  // VDSO定位失败
        return -1;         // 返回-1表示错误
    }

    // 将VDSO起始地址转换为数据页指针
    VdsoDataPage *usrVdsoDataPage = (VdsoDataPage *)(UINTPTR)vdsoStart;

    // 根据时钟类型调用相应的获取函数
    switch (clk) {
        case CLOCK_REALTIME_COARSE:  // 粗粒度实时时钟
            ret = VdsoGetRealtimeCoarse(ts, usrVdsoDataPage);
            break;
        case CLOCK_MONOTONIC_COARSE:  // 粗粒度单调时钟
            ret = VdsoGetMonotimeCoarse(ts, usrVdsoDataPage);
            break;
        default:  // 不支持的时钟类型
            ret = -1;
            break;
    }

    return ret;
}