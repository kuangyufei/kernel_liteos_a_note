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

#include "errno.h"
#include "fatfs.h"
#include "unistd.h"
#include "stdio.h"
#include "string.h"
#include "errcode_fat.h"
#include "integer.h"
#ifdef LOSCFG_FS_FAT
/**
 * @brief 存储文件系统卷标
 * @note 长度由LABEL_LEN宏定义，用于格式化时设置卷标信息
 */
char FatLabel[LABEL_LEN];

/**
 * @brief 设备名称前缀长度定义
 * @note 用于判断输入设备路径是否以"/dev"开头
 */
#define DEV_NAME_SIZE 4

/**
 * @brief 文件系统格式化主函数
 * @param dev 设备节点路径（必须以"/dev"开头）
 * @param sectors 分配单元大小（字节或扇区为单位，0表示使用默认大小）
 * @param option 文件系统类型选项（1:FAT, 2:FAT32, 7:自动选择, 8:擦除设备）
 * @return 成功返回0，失败返回-1并设置errno
 * @note 函数会调用fatfs_mkfs执行实际格式化操作，并处理虚拟分区错误
 */
int format(const char *dev, int sectors, int option)
{
    struct Vnode *device = NULL;  // 设备节点指针
    INT err;                      // 错误码变量
    if (dev == NULL) {            // 检查设备路径是否为空
        set_errno(EINVAL);        // 设置参数无效错误
        return -1;                // 返回错误
    }

    // 检查设备路径是否以"/dev"开头
    if (strncmp(dev, "/dev", DEV_NAME_SIZE) != 0) {
        PRINTK("Usage  :\n");  // 打印使用说明
        PRINTK("        format <dev_vnodename> <sectors> <option> <label>\n");
        PRINTK("        dev_vnodename : the name of dev\n");
        PRINTK("        sectors       : Size of allocation unit in unit of byte or sector, ");
        PRINTK("0 instead of default size\n");
        PRINTK("        options       : Index of filesystem. 1 for FAT filesystem, 2 for FAT32 filesystem, ");
        PRINTK("7 for any, 8 for erase\n");
        PRINTK("        label         : The volume of device. It will be emptyed when this parameter is null\n");
        PRINTK("Example:\n");       // 打印示例命令
        PRINTK("        format /dev/mmcblk0 128 2\n");

        set_errno(EINVAL);        // 设置参数无效错误
        return -1;                // 返回错误
    }
    VnodeHold();                  // 获取Vnode锁
    // 查找设备节点
    err = VnodeLookup(dev, &device, 0);
    // 处理设备不存在或不支持的情况
    if (err == -ENOENT || err == -ENOSYS) {
        VnodeDrop();              // 释放Vnode锁
        set_errno(ENODEV);        // 设置设备不存在错误
        return -1;                // 返回错误
    } else if (err < 0) {         // 处理其他查找错误
        VnodeDrop();              // 释放Vnode锁
        set_errno(-err);          // 设置错误码
        return -1;                // 返回错误
    }
    // 调用fatfs_mkfs执行格式化
    err = fatfs_mkfs(device, sectors, option);
    if (err < 0) {                // 处理格式化错误
        VnodeDrop();              // 释放Vnode锁
        set_errno(-err);          // 设置错误码
        return -1;                // 返回错误
    }
#ifdef LOSCFG_FS_FAT_VIRTUAL_PARTITION
    // 处理虚拟分区错误
    else if (err >= VIRERR_BASE) {
        set_errno(err);           // 设置虚拟分区错误码
    }
#endif
    VnodeDrop();                  // 释放Vnode锁
    return 0;                     // 返回成功
}

/**
 * @brief 设置文件系统卷标
 * @param name 卷标名称字符串（可为空）
 * @note 卷标长度限制为LABEL_LEN-1，超出部分会被截断
 */
void set_label(const char *name)
{
    INT len;                      // 字符串长度变量
    INT err;                      // 错误码变量

    // 初始化卷标缓冲区
    (void)memset_s(FatLabel, LABEL_LEN, 0, LABEL_LEN);

    if (name == NULL || *name == '\0') {  // 检查卷标是否为空
        return;                     // 直接返回
    }

    len = strlen(name);            // 获取卷标长度
    if (len >= LABEL_LEN) {        // 检查长度是否超出限制
        len = LABEL_LEN - 1;       // 截断到最大允许长度
    }

    // 复制卷标到缓冲区
    err = strncpy_s(FatLabel, LABEL_LEN, name, len);
    if (err != EOK) {              // 检查复制是否成功
        PRINT_ERR("Fat set_label error");  // 打印错误信息
    }
}
#endif    /* #ifdef CONFIG_FS_FAT */