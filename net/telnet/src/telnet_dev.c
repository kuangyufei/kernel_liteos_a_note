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

#define TELNET_EVENT_MORE_CMD   0x01  // 表示有更多命令的事件标志
#define TELNET_DEV_DRV_MODE     0666  // telnet设备驱动模式，权限为0666

STATIC TELNET_DEV_S g_telnetDev;       // telnet设备全局实例
STATIC EVENT_CB_S *g_event;            // 事件回调指针
STATIC struct Vnode *g_currentVnode;   // 当前Vnode指针

/**
 * @brief 通过文件指针获取telnet设备实例
 * @param file 文件指针
 * @param isOpenOp 是否为打开操作
 * @return 成功返回TELNET_DEV_S指针，失败返回NULL
 */
STATIC INLINE TELNET_DEV_S *GetTelnetDevByFile(const struct file *file, BOOL isOpenOp)
{
    struct Vnode *telnetInode = NULL;
    TELNET_DEV_S *telnetDev = NULL;

    if (file == NULL) {  // 文件指针为空检查
        return NULL;
    }
    telnetInode = file->f_vnode;  // 获取文件对应的Vnode
    if (telnetInode == NULL) {  // Vnode为空检查
        return NULL;
    }
    /*
     * 非打开操作时检查f_vnode是否有效（打开操作应获取无效f_vnode）：
     * 当telnet断开连接时，可能仍有'TelentShellTask'任务尝试写入文件，
     * 但文件因被其他进程使用而具有非法f_vnode
     */
    if (!isOpenOp) {
        if (telnetInode != g_currentVnode) {  // 检查Vnode是否为当前有效Vnode
            return NULL;
        }
    }
    telnetDev = (TELNET_DEV_S *)((struct drv_data*)telnetInode->data)->priv;  // 从Vnode数据中获取telnet设备实例
    return telnetDev;
}

/**
 * @brief 接收用户输入命令，将命令复制到telnet设备的FIFO，然后通知命令解析任务（独立的shell任务）取出并运行命令
 * @param buf 命令缓冲区
 * @param bufLen 缓冲区长度
 * @return 失败返回-1，成功返回写入的命令长度
 */
INT32 TelnetTx(const CHAR *buf, UINT32 bufLen)
{
    UINT32 i;
    TELNET_DEV_S *telnetDev = NULL;

    TelnetLock();  // 加锁保护临界区

    telnetDev = &g_telnetDev;
    if ((buf == NULL) || (telnetDev->cmdFifo == NULL)) {  // 缓冲区或FIFO为空检查
        TelnetUnlock();  // 解锁
        return -1;
    }

    /* 大小限制 */
    if (bufLen > telnetDev->cmdFifo->fifoNum) {  // 如果请求长度超过FIFO可用空间，截断为可用空间
        bufLen = telnetDev->cmdFifo->fifoNum;
    }

    if (bufLen == 0) {  // 空数据检查
        TelnetUnlock();  // 解锁
        return 0;
    }

    /* 将命令复制到telnet设备的FIFO */
    for (i = 0; i < bufLen; i++) {
        telnetDev->cmdFifo->rxBuf[telnetDev->cmdFifo->rxIndex] = *buf;  // 复制数据到FIFO缓冲区
        telnetDev->cmdFifo->rxIndex++;  // 递增接收索引
        telnetDev->cmdFifo->rxIndex %= FIFO_MAX;  // 循环索引
        buf++;
    }
    telnetDev->cmdFifo->fifoNum -= bufLen;  // 更新FIFO可用空间

    if (telnetDev->eventPend) {
        /* 发送有工作要做的信号 */
        (VOID)LOS_EventWrite(&telnetDev->eventTelnet, TELNET_EVENT_MORE_CMD);  // 写入事件
    }
    /* 通知命令解析任务 */
    notify_poll(&telnetDev->wait);  // 通知轮询等待队列
    TelnetUnlock();  // 解锁

    return (INT32)bufLen;
}

/**
 * @brief 打开telnet设备时，初始化FIFO、等待队列等
 * @param file 文件指针
 * @return 成功返回0，失败返回-1
 */
STATIC INT32 TelnetOpen(struct file *file)
{
    struct wait_queue_head *wait = NULL;
    TELNET_DEV_S *telnetDev = NULL;

    TelnetLock();  // 加锁保护临界区

    telnetDev = GetTelnetDevByFile(file, TRUE);  // 获取telnet设备实例
    if (telnetDev == NULL) {
        TelnetUnlock();  // 解锁
        return -1;
    }

    if (telnetDev->cmdFifo == NULL) {  // 如果FIFO未初始化
        wait = &telnetDev->wait;
        (VOID)LOS_EventInit(&telnetDev->eventTelnet);  // 初始化事件
        g_event = &telnetDev->eventTelnet;  // 设置全局事件指针
        telnetDev->cmdFifo = (TELNTE_FIFO_S *)malloc(sizeof(TELNTE_FIFO_S));  // 分配FIFO内存
        if (telnetDev->cmdFifo == NULL) {  // 内存分配失败检查
            TelnetUnlock();  // 解锁
            return -1;
        }
        (VOID)memset_s(telnetDev->cmdFifo, sizeof(TELNTE_FIFO_S), 0, sizeof(TELNTE_FIFO_S));  // 初始化FIFO内存
        telnetDev->cmdFifo->fifoNum = FIFO_MAX;  // 设置FIFO最大容量
        LOS_ListInit(&wait->poll_queue);  // 初始化轮询等待队列
    }
    g_currentVnode = file->f_vnode;  // 更新当前Vnode
    TelnetUnlock();  // 解锁
    return 0;
}

/**
 * @brief 关闭telnet设备时，释放FIFO、等待队列等资源
 * @param file 文件指针
 * @return 成功返回0
 */
STATIC INT32 TelnetClose(struct file *file)
{
    struct wait_queue_head *wait = NULL;
    TELNET_DEV_S *telnetDev = NULL;

    TelnetLock();  // 加锁保护临界区

    telnetDev = GetTelnetDevByFile(file, FALSE);  // 获取telnet设备实例
    if (telnetDev != NULL) {
        wait = &telnetDev->wait;
        LOS_ListDelete(&wait->poll_queue);  // 删除轮询等待队列
        free(telnetDev->cmdFifo);  // 释放FIFO内存
        telnetDev->cmdFifo = NULL;  // 置空FIFO指针
        (VOID)LOS_EventDestroy(&telnetDev->eventTelnet);  // 销毁事件
        g_event = NULL;  // 置空全局事件指针
    }
    g_currentVnode = NULL;  // 重置当前Vnode
    TelnetUnlock();  // 解锁
    return 0;
}

/**
 * @brief 当命令解析任务尝试读取telnet设备时调用，从FIFO中取出用户命令运行
 * @param file 文件指针
 * @param buf 接收缓冲区
 * @param bufLen 缓冲区长度
 * @return 失败返回-1，成功返回从FIFO取出的命令长度
 */
STATIC ssize_t TelnetRead(struct file *file, CHAR *buf, size_t bufLen)
{
    UINT32 i;
    TELNET_DEV_S *telnetDev = NULL;

    TelnetLock();  // 加锁保护临界区

    telnetDev = GetTelnetDevByFile(file, FALSE);  // 获取telnet设备实例
    if ((buf == NULL) || (telnetDev == NULL) || (telnetDev->cmdFifo == NULL)) {  // 参数有效性检查
        TelnetUnlock();  // 解锁
        return -1;
    }

    if (telnetDev->eventPend) {
        TelnetUnlock();  // 解锁
        (VOID)LOS_EventRead(g_event, TELNET_EVENT_MORE_CMD, LOS_WAITMODE_OR, LOS_WAIT_FOREVER);  // 等待事件
        TelnetLock();  // 重新加锁
    }

    if (bufLen > (FIFO_MAX - telnetDev->cmdFifo->fifoNum)) {  // 如果请求长度超过FIFO数据量，截断为实际数据量
        bufLen = FIFO_MAX - telnetDev->cmdFifo->fifoNum;
    }

    for (i = 0; i < bufLen; i++) {
        *buf++ = telnetDev->cmdFifo->rxBuf[telnetDev->cmdFifo->rxOutIndex++];  // 从FIFO读取数据到缓冲区
        if (telnetDev->cmdFifo->rxOutIndex >= FIFO_MAX) {  // 循环索引
            telnetDev->cmdFifo->rxOutIndex = 0;
        }
    }
    telnetDev->cmdFifo->fifoNum += bufLen;  // 更新FIFO可用空间
    /* 检查是否没有更多命令需要运行 */
    if (telnetDev->cmdFifo->fifoNum == FIFO_MAX) {  // FIFO为空
        (VOID)LOS_EventClear(&telnetDev->eventTelnet, ~TELNET_EVENT_MORE_CMD);  // 清除事件
    }

    TelnetUnlock();  // 解锁
    return (ssize_t)bufLen;
}

/**
 * @brief 当命令解析任务尝试将命令结果写入telnet设备时，使用lwIP send函数发送结果
 * @param file 文件指针
 * @param buf 发送缓冲区
 * @param bufLen 缓冲区长度
 * @return 缓冲区为空返回-1，成功返回写入的数据长度（可能为0）
 */
STATIC ssize_t TelnetWrite(struct file *file, const CHAR *buf, const size_t bufLen)
{
    INT32 ret = 0;
    TELNET_DEV_S *telnetDev = NULL;

    TelnetLock();  // 加锁保护临界区

    telnetDev = GetTelnetDevByFile(file, FALSE);  // 获取telnet设备实例
    if ((buf == NULL) || (telnetDev == NULL) || (telnetDev->cmdFifo == NULL)) {  // 参数有效性检查
        TelnetUnlock();  // 解锁
        return -1;
    }

    if (OS_INT_ACTIVE) {  // 中断上下文检查
        TelnetUnlock();  // 解锁
        return ret;
    }

    if (!OsPreemptable()) {  // 不可抢占检查
        TelnetUnlock();  // 解锁
        return ret;
    }

    if (telnetDev->clientFd != 0) {  // 客户端文件描述符有效
#ifdef LOSCFG_BASE_CORE_SWTMR_ENABLE
         /* 不要在软件定时器任务中调用阻塞API */
        if (OsIsSwtmrTask(OsCurrTaskGet())) {  // 检查是否为软件定时器任务
            TelnetUnlock();  // 解锁
            return ret;
        }
#endif
        ret = send(telnetDev->clientFd, buf, bufLen, 0);  // 发送数据
    }
    TelnetUnlock();  // 解锁
    return ret;
}

/**
 * @brief telnet设备IO控制函数
 * @param file 文件指针
 * @param cmd 控制命令
 * @param arg 命令参数
 * @return 成功返回0，失败返回-1
 */
STATIC INT32 TelnetIoctl(struct file *file, const INT32 cmd, unsigned long arg)
{
    TELNET_DEV_S *telnetDev = NULL;

    TelnetLock();  // 加锁保护临界区

    telnetDev = GetTelnetDevByFile(file, FALSE);  // 获取telnet设备实例
    if (telnetDev == NULL) {
        TelnetUnlock();  // 解锁
        return -1;
    }

    if (cmd == CFG_TELNET_EVENT_PEND) {  // 设置事件等待标志
        if (arg == 0) {
            telnetDev->eventPend = FALSE;
            (VOID)LOS_EventWrite(&(telnetDev->eventTelnet), TELNET_EVENT_MORE_CMD);  // 写入事件
            (VOID)LOS_EventClear(&(telnetDev->eventTelnet), ~TELNET_EVENT_MORE_CMD);  // 清除事件
        } else {
            telnetDev->eventPend = TRUE;
        }
    } else if (cmd == CFG_TELNET_SET_FD) {  // 设置客户端文件描述符
        if (arg >= (FD_SETSIZE - 1)) {  // 文件描述符有效性检查
            TelnetUnlock();  // 解锁
            return -1;
        }
        telnetDev->clientFd = (INT32)arg;  // 设置客户端文件描述符
    }
    TelnetUnlock();  // 解锁
    return 0;
}

/**
 * @brief telnet设备轮询函数
 * @param file 文件指针
 * @param table 轮询表
 * @return 有命令时返回POLLIN | POLLRDNORM，失败返回-1，无命令返回0
 */
STATIC INT32 TelnetPoll(struct file *file, poll_table *table)
{
    TELNET_DEV_S *telnetDev = NULL;

    TelnetLock();  // 加锁保护临界区

    telnetDev = GetTelnetDevByFile(file, FALSE);  // 获取telnet设备实例
    if ((telnetDev == NULL) || (telnetDev->cmdFifo == NULL)) {  // 参数有效性检查
        TelnetUnlock();  // 解锁
        return -1;
    }

    poll_wait(file, &telnetDev->wait, table);  // 将等待队列添加到轮询表

    /* 检查是否有命令需要运行 */
    if (telnetDev->cmdFifo->fifoNum != FIFO_MAX) {  // FIFO非空
        TelnetUnlock();  // 解锁
        return POLLIN | POLLRDNORM;  // 返回可读事件
    }
    TelnetUnlock();  // 解锁
    return 0;
}

// 文件操作结构体，定义telnet设备的各种操作接口
STATIC const struct file_operations_vfs g_telnetOps = {
    TelnetOpen,    // 打开操作
    TelnetClose,   // 关闭操作
    TelnetRead,    // 读取操作
    TelnetWrite,   // 写入操作
    NULL,          // 定位操作
    TelnetIoctl,   // IO控制操作
    NULL,          // 刷新操作
#ifndef CONFIG_DISABLE_POLL
    TelnetPoll,    // 轮询操作
#endif
    NULL,          // 读目录操作
};

/**
 * @brief telnet服务器停止时，移除telnet设备文件
 * @return 成功返回0
 */
INT32 TelnetedUnregister(VOID)
{
    free(g_telnetDev.cmdFifo);  // 释放FIFO内存
    g_telnetDev.cmdFifo = NULL;  // 置空FIFO指针
    (VOID)unregister_driver(TELNET);  // 注销驱动

    return 0;
}

/**
 * @brief telnet服务器启动时，注册telnet设备文件
 * @return 成功返回0，失败返回非0
 */
INT32 TelnetedRegister(VOID)
{
    INT32 ret;

    g_telnetDev.id = 0;  // 设备ID初始化
    g_telnetDev.cmdFifo = NULL;  // FIFO指针初始化
    g_telnetDev.eventPend = TRUE;  // 事件等待标志初始化

    ret = register_driver(TELNET, &g_telnetOps, TELNET_DEV_DRV_MODE, &g_telnetDev);  // 注册驱动
    if (ret != 0) {
        PRINT_ERR("Telnet register driver error.\n");  // 注册失败打印错误信息
    }
    return ret;
}

/**
 * @brief telnet客户端连接建立时，更新任务的输出控制台
 * @param clientFd 客户端文件描述符
 * @return 成功返回0，失败返回-1
 */
INT32 TelnetDevInit(INT32 clientFd)
{
    INT32 ret;

    if (clientFd < 0) {  // 文件描述符有效性检查
        PRINT_ERR("Invalid telnet clientFd.\n");  // 打印错误信息
        return -1;
    }
    ret = system_console_init(TELNET);  // 初始化系统控制台
    if (ret != 0) {
        PRINT_ERR("Telnet console init error.\n");  // 初始化失败打印错误信息
        return ret;
    }
    ret = ioctl(STDIN_FILENO, CFG_TELNET_SET_FD, clientFd);  // 设置文件描述符
    if (ret != 0) {
        PRINT_ERR("Telnet device ioctl error.\n");  // IO控制失败打印错误信息
        (VOID)system_console_deinit(TELNET);  // 反初始化系统控制台
    }
    return ret;
}

/**
 * @brief 关闭telnet客户端连接时，重置任务的输出控制台
 * @return 成功返回0，失败返回非0
 */
INT32 TelnetDevDeinit(VOID)
{
    INT32 ret;

    ret = system_console_deinit(TELNET);  // 反初始化系统控制台
    if (ret != 0) {
        PRINT_ERR("Telnet console deinit error.\n");  // 反初始化失败打印错误信息
    }
    return ret;
}

#endif

