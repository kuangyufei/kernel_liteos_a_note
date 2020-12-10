/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
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
/************************************************
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
************************************************/
#define MTD_NORFLASH        3//存储空间一般比较小，但它可以不用初始化，可以在其内部运行程序，一般在其存储一些初始化内存的固件代码；
#define MTD_NANDFLASH       4
#define MTD_DATAFLASH       6
#define MTD_MLCNANDFLASH    8
/*********************************************
扇区是对硬盘而言，而块是对文件系统而言
文件系统不是一个扇区一个扇区的来读数据，一个扇区512个字节,太慢了，所以有了block（块）的概念，
文件系统是一个块一个块的读取的，block才是文件存取的最小单位。
*********************************************/
struct MtdNorDev {//一个block是4K，即:文件系统中1个块是由连续的8个扇区组成
    unsigned long blockSize;	//块大小,不用猜也知道,是4K,和内存的页等同,如此方便置换
    unsigned long blockStart;	//开始块索引
    unsigned long blockEnd;		//结束块索引
};

struct MtdDev {//flash描述符
    VOID *priv;
    UINT32 type;

    UINT64 size;
    UINT32 eraseSize;//4K, 跟PAGE_CACHE_SIZE对应

    int (*erase)(struct MtdDev *mtd, UINT64 start, UINT64 len, UINT64 *failAddr);//擦除flash操作
    int (*read)(struct MtdDev *mtd, UINT64 start, UINT64 len, const char *buf);	 //读flash操作
    int (*write)(struct MtdDev *mtd, UINT64 start, UINT64 len, const char *buf); //写flash操作	
};

#endif /* __MTD_DEV_H__ */
