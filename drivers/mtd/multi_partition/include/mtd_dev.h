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
#ifndef __MTD_DEV_H__
#define __MTD_DEV_H__

#include "los_typedef.h"

/**
 * @file mtd_dev.h
 * @verbatim
    MTD，Memory Technology Device即内存技术设备
    Linux系统中采用MTD来管理不同类型的Flash芯片，包括NandFlash和NorFlash
    NAND型和NOR型Flash在进行写入和擦除时都需要MTD（Memory Technology Drivers，MTD已集成在Flash芯片内部，
    它是对Flash进行操作的接口），这是它们的共同特点；但在NOR型Flash上运行代码不需要任何的软件支持，
    而在NAND型Flash上进行同样操作时，通常需要驱动程序，即内存技术驱动程序MTD。

    NOR型Flash采用的SRAM接口，提供足够的地址引脚来寻址，可以很容易的存取其片内的每一个字节；
    NAND型Flash使用复杂的I/O口来串行的存取数据，各个产品或厂商的方法可能各不相同，通常是采用8个I/O引脚
    来传送控制、地址、数据信息。

    NAND型Flash具有较高的单元密度，容量可以做得比较大，加之其生产过程更为简单，价格较低；NOR型Flash占据了容量
    为1～16MB闪存市场的大部分，而NAND型Flash只是用在8～128MB的产品中，这也说明NOR主要用在代码存储介质中，
    NAND适合数据存储。 
 * @endverbatim 
 * @brief 
 */

/* MTD设备类型定义 */
#define MTD_NORFLASH        3       /* NOR闪存类型标识 */
#define MTD_NANDFLASH       4       /* NAND闪存类型标识 */
#define MTD_DATAFLASH       6       /* 数据闪存类型标识 */
#define MTD_MLCNANDFLASH    8       /* 多层单元NAND闪存类型标识 */

/**
 * @brief NOR闪存设备结构
 * @details 定义NOR闪存的块大小和地址范围信息
 */
struct MtdNorDev {
    unsigned long blockSize;        /* 块大小 (字节) */
    unsigned long blockStart;       /* 起始块地址 (物理地址) */
    unsigned long blockEnd;         /* 结束块地址 (物理地址) */
};

/**
 * @brief MTD设备抽象结构
 * @details 定义各类MTD设备的通用接口和属性，包含设备类型、大小信息及操作函数指针
 */
struct MtdDev {
    VOID *priv;                     /* 私有数据指针，指向具体设备的扩展信息 */
    UINT32 type;                    /* 设备类型，取值为MTD_xxx系列宏定义 */

    UINT64 size;                    /* 设备总大小 (字节) */
    UINT32 eraseSize;               /* 擦除块大小 (字节) */

    /**
     * @brief 擦除设备指定区域
     * @param mtd [in] MTD设备指针
     * @param start [in] 起始地址 (字节)
     * @param len [in] 擦除长度 (字节)
     * @param failAddr [out] 擦除失败地址，NULL表示不返回
     * @return 0表示成功，负数表示失败
     */
    int (*erase)(struct MtdDev *mtd, UINT64 start, UINT64 len, UINT64 *failAddr);
    /**
     * @brief 从设备读取数据
     * @param mtd [in] MTD设备指针
     * @param start [in] 起始地址 (字节)
     * @param len [in] 读取长度 (字节)
     * @param buf [out] 接收数据的缓冲区
     * @return 0表示成功，负数表示失败
     */
    int (*read)(struct MtdDev *mtd, UINT64 start, UINT64 len, const char *buf);
    /**
     * @brief 向设备写入数据
     * @param mtd [in] MTD设备指针
     * @param start [in] 起始地址 (字节)
     * @param len [in] 写入长度 (字节)
     * @param buf [in] 待写入数据的缓冲区
     * @return 0表示成功，负数表示失败
     */
    int (*write)(struct MtdDev *mtd, UINT64 start, UINT64 len, const char *buf);
};


#endif /* __MTD_DEV_H__ */
