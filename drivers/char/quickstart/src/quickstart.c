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

#include "los_dev_quickstart.h"
#include "fcntl.h"
#include "linux/kernel.h"
#include "los_process_pri.h"
#include "fs/file.h"
#include "fs/driver.h"
/** 快速启动事件控制块 */
EVENT_CB_S g_qsEvent;
/** 系统初始化钩子函数数组，存储各阶段初始化函数 */
static SysteminitHook g_systemInitFunc[QS_STAGE_CNT] = {0};
/** 阶段执行标志数组，确保每个阶段只执行一次 (1表示已执行) */
static char g_callOnce[QS_STAGE_CNT] = {0};

/**
 * @brief   快速启动设备打开操作
 * @param   filep   文件操作指针
 * @return  0表示成功
 */
static int QuickstartOpen(struct file *filep)
{
    return 0;  // 始终返回成功
}

/**
 * @brief   快速启动设备关闭操作
 * @param   filep   文件操作指针
 * @return  0表示成功
 */
static int QuickstartClose(struct file *filep)
{
    return 0;  // 始终返回成功
}

/**
 * @brief   快速启动事件通知
 * @param   events  事件掩码
 * @return  0表示成功，负数表示失败
 */
static int QuickstartNotify(unsigned int events)
{
    // 写入事件到事件控制块
    int ret = LOS_EventWrite((PEVENT_CB_S)&g_qsEvent, events);
    if (ret != 0) {
        PRINT_ERR("%s,%d:0x%x\n", __FUNCTION__, __LINE__, ret);
        ret = -EINVAL;  // 事件写入失败
    }
    return ret;
}

/** 最大等待时间限制 (5分钟 = 5*60*1000*1ms/ tick) */
#define WAITLIMIT    300000 /* 5min = 5*60*1000*1tick(1ms) */

/**
 * @brief   监听快速启动事件
 * @param   arg     用户空间传入的参数指针
 * @return  0表示成功，负数表示失败
 */
static int QuickstartListen(unsigned long arg)
{
    QuickstartListenArgs args;  // 监听参数结构体
    // 从用户空间复制参数
    if (copy_from_user(&args, (QuickstartListenArgs __user *)arg, sizeof(QuickstartListenArgs)) != LOS_OK) {
        PRINT_ERR("%s,%d,failed!\n", __FUNCTION__, __LINE__);
        return -EINVAL;  // 参数复制失败
    }
    // 检查等待时间是否超过限制
    if (args.wait > WAITLIMIT) {
        args.wait = WAITLIMIT;
        PRINT_ERR("%s wait arg is too longer, set to WAITLIMIT!\n", __FUNCTION__);
    }
    // 读取事件 (与模式，清除已读位，带超时)
    int ret = LOS_EventRead((PEVENT_CB_S)&g_qsEvent, args.events, LOS_WAITMODE_AND | LOS_WAITMODE_CLR, args.wait);
    if (ret != args.events && ret != 0) {  /* 0: 非阻塞模式正常返回 */
        PRINT_ERR("%s,%d:0x%x\n", __FUNCTION__, __LINE__, ret);
        ret = -EINVAL;  // 事件读取失败
    }
    return ret;
}

/**
 * @brief   注册快速启动阶段钩子函数
 * @param   hooks   包含各阶段钩子函数的结构体
 */
void QuickstartHookRegister(LosSysteminitHook hooks)
{
    // 将钩子函数注册到全局数组
    for (int i = 0; i < QS_STAGE_CNT; i++) {
        g_systemInitFunc[i] = hooks.func[i];
    }
}

/**
 * @brief   执行指定阶段的初始化工作
 * @param   level   阶段级别
 * @return  0表示成功
 */
static int QuickstartStageWorking(unsigned int level)
{
    // 检查阶段有效性、是否已执行及钩子函数是否存在
    if ((level < QS_STAGE_CNT) && (g_callOnce[level] == 0) && (g_systemInitFunc[level] != NULL)) {
        g_callOnce[level] = 1;    /* 1: 标记为已执行 */
        g_systemInitFunc[level]();  // 执行该阶段初始化函数
    } else {
        PRINT_WARN("Trigger quickstart,but doing nothing!!\n");
    }
    return 0;
}

/**
 * @brief   快速启动设备卸载操作
 * @param   node    VFS节点指针
 * @return  0表示成功，负数表示失败
 */
static int QuickstartDevUnlink(struct Vnode *node)
{
    (void)node;  // 未使用的参数
    return unregister_driver(QUICKSTART_NODE);  // 注销驱动
}

/**
 * @brief   快速启动设备IO控制操作
 * @param   filep   文件操作指针
 * @param   cmd     IO控制命令
 * @param   arg     命令参数
 * @return  0表示成功，负数表示失败
 */
static ssize_t QuickstartIoctl(struct file *filep, int cmd, unsigned long arg)
{
    ssize_t ret;
    // 事件通知命令
    if (cmd == QUICKSTART_NOTIFY) {
        return QuickstartNotify(arg);
    }

    // 权限检查：仅允许根进程执行
    if (LOS_GetCurrProcessID() != OS_USER_ROOT_PROCESS_ID) {
        PRINT_ERR("Permission denios!\n");
        return -EACCES;  // 权限被拒绝
    }
    switch (cmd) {
        case QUICKSTART_LISTEN:
            ret = QuickstartListen(arg);  // 监听事件
            break;
        default:
            // 将IOCTL命令转换为阶段级别并执行
            ret = QuickstartStageWorking(cmd - QUICKSTART_STAGE(QS_STAGE1));  /* ioctl cmd converted to stage level */
            break;
    }
    return ret;
}

/**
 * @brief   快速启动设备文件操作结构体
 */
static const struct file_operations_vfs g_quickstartDevOps = {
    .open = QuickstartOpen,    /* open：打开设备 */
    .close = QuickstartClose,  /* close：关闭设备 */
    .ioctl = QuickstartIoctl,  /* ioctl：控制设备 */
    .unlink = QuickstartDevUnlink, /* unlink：卸载设备 */
};

/**
 * @brief   注册快速启动设备驱动
 * @return  0表示成功，负数表示失败
 */
int QuickstartDevRegister(void)
{
    LOS_EventInit(&g_qsEvent);  // 初始化事件控制块
    // 注册字符设备，文件权限为0644
    return register_driver(QUICKSTART_NODE, &g_quickstartDevOps, 0644, 0); /* 0644: 文件权限 */
}
