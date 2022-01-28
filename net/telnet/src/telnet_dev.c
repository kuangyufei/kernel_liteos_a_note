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

#include "telnet_dev.h"
#ifdef LOSCFG_NET_TELNET
#include "unistd.h"
#include "stdlib.h"
#include "sys/ioctl.h"
#include "sys/types.h"
#include "pthread.h"

#include "los_printf.h"
#ifdef LOSCFG_BASE_CORE_SWTMR_ENABLE
#include "los_swtmr_pri.h"
#endif
#include "los_sched_pri.h"
#include "console.h"
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "telnet_pri.h"

#include "fs/driver.h"

/* event: there are more commands left in the FIFO to run */
#define TELNET_EVENT_MORE_CMD   0x01 ///< 还有很多命令在FIFO中等待执行的事件
#define TELNET_DEV_DRV_MODE     0666 ///< 文件权限 chmod = 666 

STATIC TELNET_DEV_S g_telnetDev;	///< 远程登录设备
STATIC EVENT_CB_S *g_event;
STATIC struct Vnode *g_currentVnode;

STATIC INLINE TELNET_DEV_S *GetTelnetDevByFile(const struct file *file, BOOL isOpenOp)
{
    struct Vnode *telnetInode = NULL;
    TELNET_DEV_S *telnetDev = NULL;

    if (file == NULL) {
        return NULL;
    }
    telnetInode = file->f_vnode;
    if (telnetInode == NULL) {
        return NULL;
    }
    /*
     * Check if the f_vnode is valid here for non-open ops (open is supposed to get invalid f_vnode):
     * when telnet is disconnected, there still may be 'TelentShellTask' tasks trying to write
     * to the file, but the file has illegal f_vnode because the file is used by others.
     */
    if (!isOpenOp) {
        if (telnetInode != g_currentVnode) {
            return NULL;
        }
    }
    telnetDev = (TELNET_DEV_S *)((struct drv_data*)telnetInode->data)->priv;
    return telnetDev;
}

/*
 * Description : When receive user's input commands, first copy commands to the FIFO of the telnet device.
 *               Then, notify a command resolver task (an individual shell task) to take out and run commands.
	 当接收到用户输入的命令时，首先将命令复制到telnet设备的FIFO中，然后通知一个命令解析器任务
	 （一个单独的shell任务）取出并运行命令。
 * Return      : -1                   --- On failure
 *               Non-negative integer --- length of written commands
 */
INT32 TelnetTx(const CHAR *buf, UINT32 bufLen)
{
    UINT32 i;
    TELNET_DEV_S *telnetDev = NULL;

    TelnetLock();

    telnetDev = &g_telnetDev;
    if ((buf == NULL) || (telnetDev->cmdFifo == NULL)) {
        TelnetUnlock();
        return -1;
    }

    /* size limited */
    if (bufLen > telnetDev->cmdFifo->fifoNum) {//一次拿不完数据的情况
        bufLen = telnetDev->cmdFifo->fifoNum;//只能装满
    }

    if (bufLen == 0) { //参数要先判断 @note_thinking
        TelnetUnlock();
        return 0;
    }

    /* copy commands to the fifo of the telnet device | 复制命令到telnet设备的fifo*/
    for (i = 0; i < bufLen; i++) {
        telnetDev->cmdFifo->rxBuf[telnetDev->cmdFifo->rxIndex] = *buf;
        telnetDev->cmdFifo->rxIndex++;
        telnetDev->cmdFifo->rxIndex %= FIFO_MAX;
        buf++;
    }
    telnetDev->cmdFifo->fifoNum -= bufLen;

    if (telnetDev->eventPend) {
        /* signal that there are some works to do */
        (VOID)LOS_EventWrite(&telnetDev->eventTelnet, TELNET_EVENT_MORE_CMD);
    }
    /* notify the command resolver task */
    notify_poll(&telnetDev->wait);
    TelnetUnlock();

    return (INT32)bufLen;
}

/*
 * Description : When open the telnet device, init the FIFO, wait queue etc.
 */
STATIC INT32 TelnetOpen(struct file *file)
{
    struct wait_queue_head *wait = NULL;
    TELNET_DEV_S *telnetDev = NULL;

    TelnetLock();

    telnetDev = GetTelnetDevByFile(file, TRUE);//获取标准file私有数据
    if (telnetDev == NULL) {
        TelnetUnlock();
        return -1;
    }

    if (telnetDev->cmdFifo == NULL) {
        wait = &telnetDev->wait;
        (VOID)LOS_EventInit(&telnetDev->eventTelnet);//初始化事件
        g_event = &telnetDev->eventTelnet;
        telnetDev->cmdFifo = (TELNTE_FIFO_S *)malloc(sizeof(TELNTE_FIFO_S));
        if (telnetDev->cmdFifo == NULL) {
            TelnetUnlock();
            return -1;
        }
        (VOID)memset_s(telnetDev->cmdFifo, sizeof(TELNTE_FIFO_S), 0, sizeof(TELNTE_FIFO_S));
        telnetDev->cmdFifo->fifoNum = FIFO_MAX;
        LOS_ListInit(&wait->poll_queue);
    }
    g_currentVnode = file->f_vnode;
    TelnetUnlock();
    return 0;
}

/*
 * Description : When close the telnet device, free the FIFO, wait queue etc.
 */
STATIC INT32 TelnetClose(struct file *file)
{
    struct wait_queue_head *wait = NULL;
    TELNET_DEV_S *telnetDev = NULL;

    TelnetLock();

    telnetDev = GetTelnetDevByFile(file, FALSE);
    if (telnetDev != NULL) {
        wait = &telnetDev->wait;
        LOS_ListDelete(&wait->poll_queue);
        free(telnetDev->cmdFifo);
        telnetDev->cmdFifo = NULL;
        (VOID)LOS_EventDestroy(&telnetDev->eventTelnet);
        g_event = NULL;
    }
    g_currentVnode = NULL;
    TelnetUnlock();
    return 0;
}

/*
 * Description : When a command resolver task trys to read the telnet device,
 *               this method is called, and it will take out user's commands from the FIFO to run.
 * 当命令解析器任务尝试读取 telnet 设备时，调用这个方法，它会从FIFO中取出用户的命令来运行。
 * 读取远程终端输入的命令,比如: # task 
 * Return      : -1                   --- On failure
 *               Non-negative integer --- length of commands taken out from the FIFO of the telnet device.
 */
STATIC ssize_t TelnetRead(struct file *file, CHAR *buf, size_t bufLen)
{
    UINT32 i;
    TELNET_DEV_S *telnetDev = NULL;

    TelnetLock();

    telnetDev = GetTelnetDevByFile(file, FALSE);//通过文件获取远程登录实体
    if ((buf == NULL) || (telnetDev == NULL) || (telnetDev->cmdFifo == NULL)) {
        TelnetUnlock();
        return -1;
    }

    if (telnetDev->eventPend) {//挂起时,说明没有数据可读,等待事件发生
        TelnetUnlock();
        (VOID)LOS_EventRead(g_event, TELNET_EVENT_MORE_CMD, LOS_WAITMODE_OR, LOS_WAIT_FOREVER);//等待读取 TELNET_EVENT_MORE_CMD 事件
        TelnetLock();
    }

    if (bufLen > (FIFO_MAX - telnetDev->cmdFifo->fifoNum)) {
        bufLen = FIFO_MAX - telnetDev->cmdFifo->fifoNum;
    }
	//把远程终端过来的数据接走, 一般由 Shell Entry 任务中的 read(fd,&buf)读走数据
    for (i = 0; i < bufLen; i++) {
        *buf++ = telnetDev->cmdFifo->rxBuf[telnetDev->cmdFifo->rxOutIndex++];
        if (telnetDev->cmdFifo->rxOutIndex >= FIFO_MAX) {
            telnetDev->cmdFifo->rxOutIndex = 0;
        }
    }
    telnetDev->cmdFifo->fifoNum += bufLen;
    /* check if no more commands left to run | 检查是否没有更多命令可以运行 */
    if (telnetDev->cmdFifo->fifoNum == FIFO_MAX) {
        (VOID)LOS_EventClear(&telnetDev->eventTelnet, ~TELNET_EVENT_MORE_CMD);//清除读取内容事件
    }

    TelnetUnlock();
    return (ssize_t)bufLen;
}

/*
 * Description : When a command resolver task trys to write command results to the telnet device,
 *               just use lwIP send function to send out results.
 * Return      : -1                   --- buffer is NULL
 *               Non-negative integer --- length of written data, maybe 0.
 */
STATIC ssize_t TelnetWrite(struct file *file, const CHAR *buf, const size_t bufLen)
{
    INT32 ret = 0;
    TELNET_DEV_S *telnetDev = NULL;

    TelnetLock();

    telnetDev = GetTelnetDevByFile(file, FALSE);
    if ((buf == NULL) || (telnetDev == NULL) || (telnetDev->cmdFifo == NULL)) {
        TelnetUnlock();
        return -1;
    }

    if (OS_INT_ACTIVE) {
        TelnetUnlock();
        return ret;
    }

    if (!OsPreemptable()) {//@note_thinking 这里为何要有这个判断?
        TelnetUnlock();
        return ret;
    }

    if (telnetDev->clientFd != 0) {
#ifdef LOSCFG_BASE_CORE_SWTMR_ENABLE
         /* DO NOT call blocking API in software timer task | 不要在软件定时器任务中调用阻塞 API */
        if (OsIsSwtmrTask(OsCurrTaskGet())) {
            TelnetUnlock();
            return ret;
        }
#endif
        ret = send(telnetDev->clientFd, buf, bufLen, 0);//向 socket 发送
    }
    TelnetUnlock();
    return ret;
}
/// 远程登录控制操作
STATIC INT32 TelnetIoctl(struct file *file, const INT32 cmd, unsigned long arg)
{
    TELNET_DEV_S *telnetDev = NULL;

    TelnetLock();

    telnetDev = GetTelnetDevByFile(file, FALSE);
    if (telnetDev == NULL) {
        TelnetUnlock();
        return -1;
    }

    if (cmd == CFG_TELNET_EVENT_PEND) {
        if (arg == 0) {
            telnetDev->eventPend = FALSE;
            (VOID)LOS_EventWrite(&(telnetDev->eventTelnet), TELNET_EVENT_MORE_CMD);
            (VOID)LOS_EventClear(&(telnetDev->eventTelnet), ~TELNET_EVENT_MORE_CMD);
        } else {
            telnetDev->eventPend = TRUE;
        }
    } else if (cmd == CFG_TELNET_SET_FD) {
        if (arg >= (FD_SETSIZE - 1)) {
            TelnetUnlock();
            return -1;
        }
        telnetDev->clientFd = (INT32)arg;
    }
    TelnetUnlock();
    return 0;
}

STATIC INT32 TelnetPoll(struct file *file, poll_table *table)
{
    TELNET_DEV_S *telnetDev = NULL;

    TelnetLock();

    telnetDev = GetTelnetDevByFile(file, FALSE);
    if ((telnetDev == NULL) || (telnetDev->cmdFifo == NULL)) {
        TelnetUnlock();
        return -1;
    }

    poll_wait(file, &telnetDev->wait, table);

    /* check if there are some commands to run */
    if (telnetDev->cmdFifo->fifoNum != FIFO_MAX) {
        TelnetUnlock();
        return POLLIN | POLLRDNORM;
    }
    TelnetUnlock();
    return 0;
}
//远程登录操作命令
STATIC const struct file_operations_vfs g_telnetOps = {
    TelnetOpen,
    TelnetClose,
    TelnetRead,
    TelnetWrite,
    NULL,
    TelnetIoctl,
    NULL,
#ifndef CONFIG_DISABLE_POLL
    TelnetPoll,
#endif
    NULL,
};

/* Once the telnet server stopped, remove the telnet device file. */
INT32 TelnetedUnregister(VOID)
{
    free(g_telnetDev.cmdFifo);
    g_telnetDev.cmdFifo = NULL;
    (VOID)unregister_driver(TELNET);//注销字符设备驱动

    return 0;
}

/* Once the telnet server started, setup the telnet device file. 
| telnet 服务器启动后，设置 telnet 设备文件*/
INT32 TelnetedRegister(VOID)
{
    INT32 ret;

    g_telnetDev.id = 0;
    g_telnetDev.cmdFifo = NULL;
    g_telnetDev.eventPend = TRUE;
	//注册 telnet 驱动, g_telnetDev为私有数据
    ret = register_driver(TELNET, &g_telnetOps, TELNET_DEV_DRV_MODE, &g_telnetDev);//翻译过来是当读TELNET时,真正要去操作的是 g_telnetDev
    if (ret != 0) {
        PRINT_ERR("Telnet register driver error.\n");
    }
    return ret;
}

/* When a telnet client connection established, update the output console for tasks. 
| 建立 telnet 客户端连接后，更新任务的输出控制台 */
INT32 TelnetDevInit(INT32 clientFd)
{
    INT32 ret;

    if (clientFd < 0) {
        PRINT_ERR("Invalid telnet clientFd.\n");
        return -1;
    }
    ret = system_console_init(TELNET);//创建一个带远程登录功能的控制台
    if (ret != 0) {
        PRINT_ERR("Telnet console init error.\n");
        return ret;
    }
    ret = ioctl(STDIN_FILENO, CFG_TELNET_SET_FD, clientFd);//绑定FD,相当于shell和控制台绑定
    if (ret != 0) {
        PRINT_ERR("Telnet device ioctl error.\n");
        (VOID)system_console_deinit(TELNET);
    }
    return ret;
}

/* When closing the telnet client connection, reset the output console for tasks. | 
关闭 telnet 客户端连接时，重置任务的控制台信息。*/
INT32 TelnetDevDeinit(VOID)
{
    INT32 ret;

    ret = system_console_deinit(TELNET);
    if (ret != 0) {
        PRINT_ERR("Telnet console deinit error.\n");
    }
    return ret;
}
#endif

