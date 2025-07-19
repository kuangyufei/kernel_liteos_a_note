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

#include "virtual_serial.h"
#include "fcntl.h"
#ifdef LOSCFG_FILE_MODE
#include "stdarg.h"
#endif
#ifdef LOSCFG_FS_VFS
#include "console.h"
#include "fs/driver.h"
#endif
/**
 * UART 简介
 * UART（Universal Asynchronous Receiver/Transmitter）通用异步收发传输器，UART 作为异步串口通信协议的一种，
 * 工作原理是将传输数据的每个字符一位接一位地传输。是在应用程序开发过程中使用频率最高的数据总线。
 * UART 串口的特点是将数据一位一位地顺序传送，只要 2 根传输线就可以实现双向通信，一根线发送数据的同时用另一根线接收数据。
 * UART 串口通信有几个重要的参数，分别是波特率、起始位、数据位、停止位和奇偶检验位，对于两个使用 UART 串口通信的端口，
 * 这些参数必须匹配，否则通信将无法正常完成。
 * -----------------------------------------------------------------
 * +起始位	+ 数据位(D0-D7) + 奇偶校验位 + 停止位 +	
 * -----------------------------------------------------------------
 * 起始位：表示数据传输的开始，电平逻辑为 “0” 。
 * 数据位：可能值有 5、6、7、8、9，表示传输这几个 bit 位数据。一般取值为 8，因为一个 ASCII 字符值为 8 位。
 * 奇偶校验位：用于接收方对接收到的数据进行校验，校验 “1” 的位数为偶数(偶校验)或奇数(奇校验)，以此来校验数据传送的正确性，使用时不需要此位也可以。
 * 停止位： 表示一帧数据的结束。电平逻辑为 “1”。
 * 波特率：串口通信时的速率，它用单位时间内传输的二进制代码的有效位(bit)数来表示，其单位为每秒比特数 bit/s(bps)。常见的波特率值有 4800、9600、14400、38400、115200等，数值越大数据传输的越快，波特率为 115200 表示每秒钟传输 115200 位数据。
 * /

/**
 * @var g_serialType
 * @brief 串口设备类型全局变量
 * @details 存储当前虚拟串口的硬件类型，支持UART和USB TTY两种模式
 * @note 使用volatile关键字确保多线程环境下的可见性
 * @attention 仅通过SerialTypeSet()函数修改，通过SerialTypeGet()函数读取
 * @ingroup driver_virtual_serial
 */
STATIC volatile UINT32 g_serialType = 0;  // 0:未初始化, 1:SERIAL_TYPE_UART_DEV, 2:SERIAL_TYPE_USBTTY_DEV

/**
 * @var g_serialFilep
 * @brief 串口文件对象
 * @details 虚拟串口对应的VFS文件结构体实例，用于与底层设备交互
 * @ingroup driver_virtual_serial
 */
STATIC struct file g_serialFilep;

/**
 * @brief 获取当前串口设备类型
 * @return UINT32 串口类型枚举值
 * @retval 0 未初始化
 * @retval SERIAL_TYPE_UART_DEV UART模式
 * @retval SERIAL_TYPE_USBTTY_DEV USB TTY模式
 * @note 线程安全，可在中断上下文调用
 * @ingroup driver_virtual_serial
 */
UINT32 SerialTypeGet(VOID)
{
    return g_serialType;  // 返回当前串口类型
}

/**
 * @brief 设置串口设备类型
 * @param[in] deviceName 设备名称字符串，如"/dev/ttyS0"或"/dev/ttyGS0"
 * @details 根据设备名自动识别并设置串口类型，支持UART和USB TTY两种设备
 * @note 仅在虚拟串口初始化时调用一次
 * @ingroup driver_virtual_serial
 * @internal
 */
STATIC VOID SerialTypeSet(const CHAR *deviceName)
{
    // 比较设备名前缀，确定UART类型
    if (!strncmp(deviceName, SERIAL_UARTDEV, strlen(SERIAL_UARTDEV))) {
        g_serialType = SERIAL_TYPE_UART_DEV;  // 设置为UART设备
    } else if (!strncmp(deviceName, SERIAL_TTYGS0, strlen(SERIAL_TTYGS0))) {
        g_serialType = SERIAL_TYPE_USBTTY_DEV;  // 设置为USB TTY设备
    }
}

/**
 * @brief 虚拟串口打开操作
 * @param[in] filep VFS文件结构体指针
 * @return INT32 操作结果
 * @retval ENOERR 成功
 * @retval EINVAL 参数无效
 * @retval EPERM 底层设备打开失败
 * @details 实现虚拟串口的打开逻辑，获取底层设备操作接口并打开实际设备
 * @note 对于UART设备会解除中断屏蔽
 * @ingroup driver_virtual_serial
 * @internal
 */
STATIC INT32 SerialOpen(struct file *filep)
{
    INT32 ret;                       // 函数返回值
    struct file *privFilep = NULL;   // 底层设备文件指针
    const struct file_operations_vfs *fileOps = NULL;  // 底层设备操作集

    // 获取底层设备的文件指针和操作接口
    ret = GetFilepOps(filep, &privFilep, &fileOps);
    if (ret != ENOERR) {  // 参数验证失败
        ret = EINVAL;
        goto ERROUT;  // 跳转到错误处理
    }

    // 调用底层设备的打开接口
    ret = FilepOpen(privFilep, fileOps);
    if (ret < 0) {  // 底层设备打开失败
        ret = EPERM;
        goto ERROUT;
    }

    // 如果是UART设备，解除UART中断屏蔽
    if (g_serialType == SERIAL_TYPE_UART_DEV) {
        HalIrqUnmask(NUM_HAL_INTERRUPT_UART);  // 使能UART中断
    }
    return ENOERR;  // 成功返回

ERROUT:  // 错误处理标签
    set_errno(ret);  // 设置错误码
    return VFS_ERROR;  // 返回VFS错误
}

/**
 * @brief 虚拟串口关闭操作
 * @param[in] filep VFS文件结构体指针（未使用）
 * @return INT32 操作结果，始终返回ENOERR
 * @details 实现虚拟串口的关闭逻辑，根据设备类型执行不同的清理操作
 * @note UART设备会屏蔽中断，USB TTY设备会重置掩码
 * @ingroup driver_virtual_serial
 * @internal
 */
STATIC INT32 SerialClose(struct file *filep)
{
    (VOID)filep;  // 未使用参数，避免编译警告

    // 根据设备类型执行不同关闭操作
    if (g_serialType == SERIAL_TYPE_UART_DEV) {
        HalIrqMask(NUM_HAL_INTERRUPT_UART);  // 屏蔽UART中断
    }
#if defined(LOSCFG_DRIVERS_USB_SERIAL_GADGET) || defined(LOSCFG_DRIVERS_USB_ETH_SER_GADGET)
    else if (g_serialType == SERIAL_TYPE_USBTTY_DEV) {
        userial_mask_set(0);  // 重置USB TTY中断掩码
    }
#endif

    return ENOERR;  // 始终成功返回
}

/**
 * @brief 虚拟串口读操作
 * @param[in] filep VFS文件结构体指针
 * @param[out] buffer 接收数据缓冲区
 * @param[in] bufLen 缓冲区长度，单位字节
 * @return ssize_t 实际读取字节数，失败返回VFS_ERROR
 * @details 将读请求转发到底层设备，实现数据接收功能
 * @note 错误码通过set_errno设置
 * @ingroup driver_virtual_serial
 * @internal
 */
STATIC ssize_t SerialRead(struct file *filep, CHAR *buffer, size_t bufLen)
{
    INT32 ret;                       // 函数返回值
    struct file *privFilep = NULL;   // 底层设备文件指针
    const struct file_operations_vfs *fileOps = NULL;  // 底层设备操作集

    // 获取底层设备的文件指针和操作接口
    ret = GetFilepOps(filep, &privFilep, &fileOps);
    if (ret != ENOERR) {  // 参数验证失败
        ret = -EINVAL;
        goto ERROUT;
    }

    // 调用底层设备的读接口
    ret = FilepRead(privFilep, fileOps, buffer, bufLen);
    if (ret < 0) {  // 读操作失败
        goto ERROUT;
    }
    return ret;  // 返回实际读取字节数

ERROUT:
    set_errno(-ret);  // 设置错误码（转为正数）
    return VFS_ERROR;  // 返回VFS错误
}

/**
 * @brief 虚拟串口写操作
 * @param[in] filep VFS文件结构体指针
 * @param[in] buffer 发送数据缓冲区
 * @param[in] bufLen 发送数据长度，单位字节
 * @return ssize_t 实际写入字节数，失败返回VFS_ERROR
 * @details 将写请求转发到底层设备，实现数据发送功能
 * @note 模块内禁止使用打印函数，避免递归调用死锁
 * @attention 确保buffer指向的内存区域有效且可访问
 * @ingroup driver_virtual_serial
 * @internal
 */
/* Note: do not add print function in this module! */
STATIC ssize_t SerialWrite(struct file *filep,  const CHAR *buffer, size_t bufLen)
{
    INT32 ret;                       // 函数返回值
    struct file *privFilep = NULL;   // 底层设备文件指针
    const struct file_operations_vfs *fileOps = NULL;  // 底层设备操作集

    // 获取底层设备的文件指针和操作接口
    ret = GetFilepOps(filep, &privFilep, &fileOps);
    if (ret != ENOERR) {  // 参数验证失败
        ret = -EINVAL;
        goto ERROUT;
    }

    // 调用底层设备的写接口
    ret = FilepWrite(privFilep, fileOps, buffer, bufLen);
    if (ret < 0) {  // 写操作失败
        goto ERROUT;
    }
    return ret;  // 返回实际写入字节数

ERROUT:
    set_errno(-ret);  // 设置错误码（转为正数）
    return VFS_ERROR;  // 返回VFS错误
}

/**
 * @brief 虚拟串口IO控制操作
 * @param[in] filep VFS文件结构体指针
 * @param[in] cmd 控制命令字
 * @param[in] arg 命令参数
 * @return INT32 操作结果，0表示成功，负数表示失败
 * @details 将IO控制请求转发到底层设备，支持设备特定控制命令
 * @note 命令格式和参数需与底层设备驱动匹配
 * @ingroup driver_virtual_serial
 * @internal
 */
STATIC INT32 SerialIoctl(struct file *filep, INT32 cmd, unsigned long arg)
{
    INT32 ret;                       // 函数返回值
    struct file *privFilep = NULL;   // 底层设备文件指针
    const struct file_operations_vfs *fileOps = NULL;  // 底层设备操作集

    // 获取底层设备的文件指针和操作接口
    ret = GetFilepOps(filep, &privFilep, &fileOps);
    if (ret != ENOERR) {  // 参数验证失败
        ret = -EINVAL;
        goto ERROUT;
    }

    // 调用底层设备的IO控制接口
    ret = FilepIoctl(privFilep, fileOps, cmd, arg);
    if (ret < 0) {  // IO控制失败
        goto ERROUT;
    }
    return ret;  // 返回操作结果

ERROUT:
    set_errno(-ret);  // 设置错误码（转为正数）
    return VFS_ERROR;  // 返回VFS错误
}

/**
 * @brief 虚拟串口轮询操作
 * @param[in] filep VFS文件结构体指针
 * @param[in] fds 轮询文件描述符集
 * @return INT32 可读写事件掩码，失败返回VFS_ERROR
 * @details 实现虚拟串口的轮询机制，支持非阻塞IO操作
 * @note 仅当未定义CONFIG_DISABLE_POLL时编译此函数
 * @ingroup driver_virtual_serial
 * @internal
 */
STATIC INT32 SerialPoll(struct file *filep, poll_table *fds)
{
    INT32 ret;                       // 函数返回值
    struct file *privFilep = NULL;   // 底层设备文件指针
    const struct file_operations_vfs *fileOps = NULL;  // 底层设备操作集

    // 获取底层设备的文件指针和操作接口
    ret = GetFilepOps(filep, &privFilep, &fileOps);
    if (ret != ENOERR) {  // 参数验证失败
        ret = -EINVAL;
        goto ERROUT;
    }
    // 调用底层设备的轮询接口
    ret = FilepPoll(privFilep, fileOps, fds);
    if (ret < 0) {  // 轮询操作失败
        goto ERROUT;
    }
    return ret;  // 返回事件掩码

ERROUT:
    set_errno(-ret);  // 设置错误码（转为正数）
    return VFS_ERROR;  // 返回VFS错误
}

/**
 * @var g_serialDevOps
 * @brief 虚拟串口设备操作集
 * @details 实现VFS标准文件操作接口，将请求转发到底层硬件设备
 * @ingroup driver_virtual_serial
 */
STATIC const struct file_operations_vfs g_serialDevOps = {
    SerialOpen,  /* open: 打开虚拟串口设备 */
    SerialClose, /* close: 关闭虚拟串口设备 */
    SerialRead,  /* read: 从串口读取数据 */
    SerialWrite, /* write: 向串口写入数据 */
    NULL,        /* pread: 未实现的预读操作 */
    SerialIoctl, /* ioctl: IO控制操作 */
    NULL,        /* mmap: 未实现的内存映射操作 */
#ifndef CONFIG_DISABLE_POLL
    SerialPoll,  /* poll: 轮询操作 */
#endif
    NULL,        /* fsync: 未实现的同步操作 */
};

/**
 * @brief 虚拟串口驱动初始化
 * @param[in] deviceName 底层物理设备名称，如"/dev/ttyS0"
 * @return INT32 初始化结果
 * @retval ENOERR 成功
 * @retval EINVAL 参数无效
 * @retval EACCES 设备查找失败
 * @retval EFAULT 设备操作接口无效
 * @details 完成虚拟串口的初始化工作，包括设备类型识别、Vnode查找和驱动注册
 * @note 必须在系统启动阶段调用
 * @ingroup driver_virtual_serial
 */
INT32 virtual_serial_init(const CHAR *deviceName)
{
    INT32 ret;                       // 函数返回值
    struct Vnode *vnode = NULL;      // VFS节点指针

    if (deviceName == NULL) {  // 参数合法性检查
        ret = EINVAL;
        goto ERROUT;
    }

    SerialTypeSet(deviceName);  // 设置串口设备类型

    VnodeHold();  // 获取Vnode操作锁
    // 查找底层物理设备的Vnode
    ret = VnodeLookup(deviceName, &vnode, V_DUMMY);
    if (ret != LOS_OK) {  // Vnode查找失败
        ret = EACCES;
        goto ERROUT;
    }

    // 初始化文件结构体
    (VOID)memset_s(&g_serialFilep, sizeof(struct file), 0, sizeof(struct file));
    g_serialFilep.f_oflags = O_RDWR;  // 设置为读写模式
    g_serialFilep.f_vnode = vnode;    // 关联Vnode
    // 从Vnode获取设备操作接口
    g_serialFilep.ops = ((struct drv_data *)vnode->data)->ops;

    // 调用底层设备的打开接口
    if (g_serialFilep.ops->open != NULL) {
        (VOID)g_serialFilep.ops->open(&g_serialFilep);
    } else {
        ret = EFAULT;  // 设备操作接口无效
        PRINTK("virtual_serial_init %s open is NULL\n", deviceName);
        goto ERROUT;
    }
    // 注册虚拟串口驱动到VFS
    (VOID)register_driver(SERIAL, &g_serialDevOps, DEFFILEMODE, &g_serialFilep);

    VnodeDrop();  // 释放Vnode操作锁
    return ENOERR;  // 初始化成功

ERROUT:
    VnodeDrop();  // 释放Vnode操作锁
    set_errno(ret);  // 设置错误码
    return VFS_ERROR;  // 初始化失败
}

/**
 * @brief 虚拟串口驱动去初始化
 * @return INT32 去初始化结果
 * @retval 0 成功
 * @retval 负数 失败
 * @details 从VFS注销虚拟串口驱动，释放相关资源
 * @note 通常在系统关闭阶段调用
 * @ingroup driver_virtual_serial
 */
INT32 virtual_serial_deinit(VOID)
{
    return unregister_driver(SERIAL);  // 注销驱动
}
