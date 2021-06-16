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
#endif

#include "fs/path_cache.h"


STATIC volatile UINT32 g_serialType = 0;
STATIC struct file g_serialFilep;


UINT32 SerialTypeGet(VOID)
{
    return g_serialType;
}

STATIC VOID SerialTypeSet(const CHAR *deviceName)
{
    if (!strncmp(deviceName, SERIAL_UARTDEV, strlen(SERIAL_UARTDEV))) {
        g_serialType = SERIAL_TYPE_UART_DEV;
    } else if (!strncmp(deviceName, SERIAL_TTYGS0, strlen(SERIAL_TTYGS0))) {
        g_serialType = SERIAL_TYPE_USBTTY_DEV;
    }
}

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

STATIC ssize_t SerialRead(struct file *filep, CHAR *buffer, size_t bufLen)
{
    INT32 ret;
    struct file *privFilep = NULL;
    const struct file_operations_vfs *fileOps = NULL;

    ret = GetFilepOps(filep, &privFilep, &fileOps);
    if (ret != ENOERR) {
        ret = -EINVAL;
        goto ERROUT;
    }

    ret = FilepRead(privFilep, fileOps, buffer, bufLen);
    if (ret < 0) {
        goto ERROUT;
    }
    return ret;

ERROUT:
    set_errno(-ret);
    return VFS_ERROR;
}

/* Note: do not add print function in this module! */
STATIC ssize_t SerialWrite(struct file *filep,  const CHAR *buffer, size_t bufLen)
{
    INT32 ret;
    struct file *privFilep = NULL;
    const struct file_operations_vfs *fileOps = NULL;

    ret = GetFilepOps(filep, &privFilep, &fileOps);
    if (ret != ENOERR) {
        ret = -EINVAL;
        goto ERROUT;
    }

    ret = FilepWrite(privFilep, fileOps, buffer, bufLen);
    if (ret < 0) {
        goto ERROUT;
    }
    return ret;

ERROUT:
    set_errno(-ret);
    return VFS_ERROR;
}

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
//虚拟串口初始化
INT32 virtual_serial_init(const CHAR *deviceName)
{
    INT32 ret;
    struct Vnode *vnode = NULL;

    if (deviceName == NULL) {
        ret = EINVAL;
        goto ERROUT;
    }

    SerialTypeSet(deviceName);

    VnodeHold();
    ret = VnodeLookup(deviceName, &vnode, V_DUMMY);
    if (ret != LOS_OK) {
        ret = EACCES;
        goto ERROUT;
    }

    (VOID)memset_s(&g_serialFilep, sizeof(struct file), 0, sizeof(struct file));
    g_serialFilep.f_oflags = O_RDWR;
    g_serialFilep.f_vnode = vnode;
    g_serialFilep.ops = ((struct drv_data *)vnode->data)->ops;

    if (g_serialFilep.ops->open != NULL) {
        (VOID)g_serialFilep.ops->open(&g_serialFilep);
    } else {
        ret = EFAULT;
        PRINTK("virtual_serial_init %s open is NULL\n", deviceName);
        goto ERROUT;
    }
    (VOID)register_driver(SERIAL, &g_serialDevOps, DEFFILEMODE, &g_serialFilep);

    VnodeDrop();
    return ENOERR;

ERROUT:
    VnodeDrop();
    set_errno(ret);
    return VFS_ERROR;
}

INT32 virtual_serial_deinit(VOID)
{
    return unregister_driver(SERIAL);
}

