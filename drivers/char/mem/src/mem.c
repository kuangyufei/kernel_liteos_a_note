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

#include "fcntl.h"
#include "linux/kernel.h"
#include "fs/driver.h"

static int MemOpen(struct file *filep)
{
    return 0;
}

static int MemClose(struct file *filep)
{
    return 0;
}

static ssize_t MemRead(struct file *filep, char *buffer, size_t buflen)
{
    return 0;
}

static ssize_t MemWrite(struct file *filep, const char *buffer, size_t buflen)
{
    return 0;
}
/**
 * @brief   内存映射函数，用于将物理地址映射到虚拟地址空间
 * @param   filep   文件操作指针
 * @param   region  虚拟内存映射区域结构体指针
 * @return  0表示成功，负数表示失败
 */
static ssize_t MemMap(struct file *filep, LosVmMapRegion *region)
{
#ifdef LOSCFG_KERNEL_VM
    size_t size = region->range.size;                     // 映射区域大小
    PADDR_T paddr = region->pgOff << PAGE_SHIFT;         // 物理地址（页偏移转换为字节地址）
    VADDR_T vaddr = region->range.base;                  // 虚拟地址起始
    LosVmSpace *space = LOS_SpaceGet(vaddr);             // 获取虚拟地址空间
    PADDR_T paddrEnd = paddr + size;                     // 物理地址结束位置
    /*
     * 仅以下两种情况有效：
     * [paddr, paddrEnd] < [SYS_MEM_BASE, SYS_MEM_END]（映射区域在系统内存之前）
     * 或
     * [SYS_MEM_BASE, SYS_MEM_END] < [paddr, paddrEnd]（映射区域在系统内存之后）
     * 否则无效
     */
    if (!((paddrEnd > paddr) && ((paddrEnd < SYS_MEM_BASE) || (paddr >= SYS_MEM_END)))) {
        return -EINVAL;                                  // 地址范围无效，返回错误
    }

    /* 外设寄存器内存添加强序属性 */
    region->regionFlags |= VM_MAP_REGION_FLAG_STRONGLY_ORDERED;

    if (space == NULL) {
        return -EAGAIN;                                  // 虚拟地址空间获取失败
    }
    // 执行MMU映射操作
    if (LOS_ArchMmuMap(&space->archMmu, vaddr, paddr, size >> PAGE_SHIFT, region->regionFlags) <= 0) {
        return -EAGAIN;                                  // 映射失败返回错误
    }
#else
    UNUSED(filep);                                       // 未启用内核VM时，标记参数未使用
    UNUSED(region);
#endif
    return 0;                                            // 映射成功
}

/**
 * @brief   内存设备文件操作结构体
 */
static const struct file_operations_vfs g_memDevOps = {
    MemOpen,  /* open：打开文件操作 */
    MemClose, /* close：关闭文件操作 */
    MemRead,  /* read：读取文件操作 */
    MemWrite, /* write：写入文件操作 */
    NULL,      /* seek：未实现的定位操作 */
    NULL,      /* ioctl：未实现的I/O控制操作 */
    MemMap,   /* mmap：内存映射操作 */
#ifndef CONFIG_DISABLE_POLL
    NULL,      /* poll：未实现的轮询操作 */
#endif
    NULL,      /* unlink：未实现的删除操作 */
};

/**
 * @brief   注册内存设备驱动
 * @return  0表示成功，负数表示失败
 */
int DevMemRegister(void)
{
    // 注册字符设备，设备名为/dev/mem，文件权限为0644
    return register_driver("/dev/mem", &g_memDevOps, 0644, 0); /* 0644: 文件权限 */
}