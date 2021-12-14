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

/*
UART 简介
UART（Universal Asynchronous Receiver/Transmitter）通用异步收发传输器，UART 作为异步串口通信协议的一种，
工作原理是将传输数据的每个字符一位接一位地传输。是在应用程序开发过程中使用频率最高的数据总线。
UART 串口的特点是将数据一位一位地顺序传送，只要 2 根传输线就可以实现双向通信，一根线发送数据的同时用另一根线接收数据。
UART 串口通信有几个重要的参数，分别是波特率、起始位、数据位、停止位和奇偶检验位，对于两个使用 UART 串口通信的端口，
这些参数必须匹配，否则通信将无法正常完成。

-----------------------------------------------------------------
+起始位	+ 数据位(D0-D7) + 奇偶校验位 + 停止位 +	
-----------------------------------------------------------------
起始位：表示数据传输的开始，电平逻辑为 “0” 。
数据位：可能值有 5、6、7、8、9，表示传输这几个 bit 位数据。一般取值为 8，因为一个 ASCII 字符值为 8 位。
奇偶校验位：用于接收方对接收到的数据进行校验，校验 “1” 的位数为偶数(偶校验)或奇数(奇校验)，以此来校验数据传送的正确性，使用时不需要此位也可以。
停止位： 表示一帧数据的结束。电平逻辑为 “1”。
波特率：串口通信时的速率，它用单位时间内传输的二进制代码的有效位(bit)数来表示，其单位为每秒比特数 bit/s(bps)。常见的波特率值有 4800、9600、14400、38400、115200等，数值越大数据传输的越快，波特率为 115200 表示每秒钟传输 115200 位数据。
*/

STATIC volatile UINT32 g_serialType = 0;
STATIC struct file g_serialFilep;// COM0 /dev/uartdev-0 在内核层的表述 属于 .bbs段

//获取串口类型
UINT32 SerialTypeGet(VOID)
{
    return g_serialType;
}
///设置串口类型
STATIC VOID SerialTypeSet(const CHAR *deviceName)
{///dev/uartdev-0
    if (!strncmp(deviceName, SERIAL_UARTDEV, strlen(SERIAL_UARTDEV))) {
        g_serialType = SERIAL_TYPE_UART_DEV;
    } else if (!strncmp(deviceName, SERIAL_TTYGS0, strlen(SERIAL_TTYGS0))) {
        g_serialType = SERIAL_TYPE_USBTTY_DEV;
    }
}
///打开串口设备
STATIC INT32 SerialOpen(struct file *filep)
{
    INT32 ret;
    struct file *privFilep = NULL;
    const struct file_operations_vfs *fileOps = NULL;

    ret = GetFilepOps(filep, &privFilep, &fileOps);
    if (ret != ENOERR) {
        ret = EINVAL;
        goto ERROUT;
    }

    ret = FilepOpen(privFilep, fileOps);
    if (ret < 0) {
        ret = EPERM;
        goto ERROUT;
    }

    if (g_serialType == SERIAL_TYPE_UART_DEV) {
        HalIrqUnmask(NUM_HAL_INTERRUPT_UART);
    }
    return ENOERR;

ERROUT:
    set_errno(ret);
    return VFS_ERROR;
}
///关闭串口设备
STATIC INT32 SerialClose(struct file *filep)
{
    (VOID)filep;

    if (g_serialType == SERIAL_TYPE_UART_DEV) {
        HalIrqMask(NUM_HAL_INTERRUPT_UART);
    }
#if defined(LOSCFG_DRIVERS_USB_SERIAL_GADGET) || defined(LOSCFG_DRIVERS_USB_ETH_SER_GADGET)
    else if (g_serialType == SERIAL_TYPE_USBTTY_DEV) {
        userial_mask_set(0);
    }
#endif

    return ENOERR;
}
///读取串口数据
STATIC ssize_t SerialRead(struct file *filep, CHAR *buffer, size_t bufLen)
{
    INT32 ret;
    struct file *privFilep = NULL;
    const struct file_operations_vfs *fileOps = NULL;

    ret = GetFilepOps(filep, &privFilep, &fileOps);//获取COM口在内核的file实例
    /*以 register_driver(SERIAL, &g_serialDevOps, DEFFILEMODE, &g_serialFilep);为例
		privFilep = g_serialFilep
		fileOps = g_serialDevOps
    */
    
    if (ret != ENOERR) {
        ret = -EINVAL;
        goto ERROUT;
    }

    ret = FilepRead(privFilep, fileOps, buffer, bufLen);//从USB或者UART 读buf
    if (ret < 0) {
        goto ERROUT;
    }
    return ret;

ERROUT:
    set_errno(-ret);
    return VFS_ERROR;
}
///写入串口数据
/* Note: do not add print function in this module! */ //注意：请勿在本模块中添加打印功能！
STATIC ssize_t SerialWrite(struct file *filep,  const CHAR *buffer, size_t bufLen)
{
    INT32 ret;
    struct file *privFilep = NULL;
    const struct file_operations_vfs *fileOps = NULL;

    ret = GetFilepOps(filep, &privFilep, &fileOps);//获取COM口在内核的file实例
    if (ret != ENOERR) {
        ret = -EINVAL;
        goto ERROUT;
    }

    ret = FilepWrite(privFilep, fileOps, buffer, bufLen);//向控制台文件写入数据
    if (ret < 0) {
        goto ERROUT;
    }
    return ret;

ERROUT:
    set_errno(-ret);
    return VFS_ERROR;
}
///控制串口设备
STATIC INT32 SerialIoctl(struct file *filep, INT32 cmd, unsigned long arg)
{
    INT32 ret;
    struct file *privFilep = NULL;
    const struct file_operations_vfs *fileOps = NULL;

    ret = GetFilepOps(filep, &privFilep, &fileOps);
    if (ret != ENOERR) {
        ret = -EINVAL;
        goto ERROUT;
    }

    ret = FilepIoctl(privFilep, fileOps, cmd, arg);
    if (ret < 0) {
        goto ERROUT;
    }
    return ret;

ERROUT:
    set_errno(-ret);
    return VFS_ERROR;
}

STATIC INT32 SerialPoll(struct file *filep, poll_table *fds)
{
    INT32 ret;
    struct file *privFilep = NULL;
    const struct file_operations_vfs *fileOps = NULL;

    ret = GetFilepOps(filep, &privFilep, &fileOps);
    if (ret != ENOERR) {
        ret = -EINVAL;
        goto ERROUT;
    }
    ret = FilepPoll(privFilep, fileOps, fds);
    if (ret < 0) {
        goto ERROUT;
    }
    return ret;

ERROUT:
    set_errno(-ret);
    return VFS_ERROR;
}
///串口实现VFS接口,以便支持按文件方式操作串口
STATIC const struct file_operations_vfs g_serialDevOps = {
    SerialOpen,  /* open */
    SerialClose, /* close */
    SerialRead,  /* read */
    SerialWrite,
    NULL,
    SerialIoctl,
    NULL,
#ifndef CONFIG_DISABLE_POLL
    SerialPoll,
#endif
    NULL,
};
//虚拟串口初始化,注册驱动程序 ,例如 : deviceName = "/dev/uartdev-0"
INT32 virtual_serial_init(const CHAR *deviceName)
{
    INT32 ret;
    struct Vnode *vnode = NULL;

    if (deviceName == NULL) {
        ret = EINVAL;
        goto ERROUT;
    }

    SerialTypeSet(deviceName);//例如: /dev/uartdev-0 为 UART串口

    VnodeHold();
    ret = VnodeLookup(deviceName, &vnode, V_DUMMY);//由deviceName查询vnode节点
    if (ret != LOS_OK) {
        ret = EACCES;
        goto ERROUT;
    }
	//接着是 vnode < -- > file 的绑定操作
    (VOID)memset_s(&g_serialFilep, sizeof(struct file), 0, sizeof(struct file));
    g_serialFilep.f_oflags = O_RDWR;//可读可写
    g_serialFilep.f_vnode = vnode;	//
    g_serialFilep.ops = ((struct drv_data *)vnode->data)->ops;//这里代表 访问 /dev/serial 意味着是访问 /dev/uartdev-0
	
    if (g_serialFilep.ops->open != NULL) {//用于检测是否有默认的驱动程序
        (VOID)g_serialFilep.ops->open(&g_serialFilep);
    } else {
        ret = EFAULT;
        PRINTK("virtual_serial_init %s open is NULL\n", deviceName);
        goto ERROUT;
    }
    (VOID)register_driver(SERIAL, &g_serialDevOps, DEFFILEMODE, &g_serialFilep);//注册虚拟串口驱动程序
	//g_serialFilep作为私有数据给了 (drv_data)data->priv = g_serialFilep
	//
    VnodeDrop();
    return ENOERR;

ERROUT:
    VnodeDrop();
    set_errno(ret);
    return VFS_ERROR;
}
///串口设备去初始化,其实就是注销驱动程序
INT32 virtual_serial_deinit(VOID)
{
    return unregister_driver(SERIAL);//注销驱动程序
}

