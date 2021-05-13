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


EVENT_CB_S g_qsEvent;
static SysteminitHook g_systemInitFunc[QS_STAGE_CNT] = {0};
static char g_callOnce[QS_STAGE_CNT] = {0};

static int QuickstartOpen(struct file *filep)
{
    return 0;
}

static int QuickstartClose(struct file *filep)
{
    return 0;
}

static int QuickstartNotify(unsigned int events)
{
    unsigned int pid = LOS_GetCurrProcessID();
    /* 16:low 16 bits for eventMask, high 16 bits for pid */
    unsigned int notifyEvent = (pid << 16) | events;
    int ret = LOS_EventWrite((PEVENT_CB_S)&g_qsEvent, notifyEvent);
    if (ret != 0) {
        PRINT_ERR("%s,%d:0x%x\n", __FUNCTION__, __LINE__, ret);
        ret = -EINVAL;
    }
    return ret;
}

static int QuickstartListen(unsigned long arg)
{
    QuickstartMask listenMask;
    if (copy_from_user(&listenMask, (struct QuickstartMask __user *)arg, sizeof(QuickstartMask)) != LOS_OK) {
        PRINT_ERR("%s,%d\n", __FUNCTION__, __LINE__);
        return -EINVAL;
    }
    /* 16:low 16 bits for eventMask, high 16 bits for pid */
    unsigned int mask = (listenMask.pid << 16) | listenMask.events;
    int ret = LOS_EventRead((PEVENT_CB_S)&g_qsEvent, mask, LOS_WAITMODE_AND | LOS_WAITMODE_CLR, LOS_WAIT_FOREVER);
    if (ret != mask) {
        PRINT_ERR("%s,%d:0x%x\n", __FUNCTION__, __LINE__, ret);
        ret = -EINVAL;
    }
    return ret;
}

void QuickstartHookRegister(LosSysteminitHook hooks)
{
    for (int i = 0; i < QS_STAGE_CNT; i++) {
        g_systemInitFunc[i] = hooks.func[i];
    }
}

static int QuickstartStageWorking(unsigned int level)
{
    if ((level < QS_STAGE_CNT) && (g_callOnce[level] == 0) && (g_systemInitFunc[level] != NULL)) {
        g_callOnce[level] = 1;    /* 1: Already called */
        g_systemInitFunc[level]();
    } else {
        PRINT_WARN("Trigger quickstart,but doing nothing!!\n");
    }
    return 0;
}

static int QuickstartDevUnregister(void)
{
    return unregister_driver(QUICKSTART_NODE);
}

static ssize_t QuickstartIoctl(struct file *filep, int cmd, unsigned long arg)
{
    ssize_t ret;
    if (cmd == QUICKSTART_NOTIFY) {
        return QuickstartNotify(arg);
    }

    if (OsGetUserInitProcessID() != LOS_GetCurrProcessID()) {
        PRINT_ERR("Permission denios!\n");
        return -EACCES;
    }
    switch (cmd) {
        case QUICKSTART_UNREGISTER:
            ret = QuickstartDevUnregister();
            break;
        case QUICKSTART_LISTEN:
            ret = QuickstartListen(arg);
            break;
        default:
            ret = QuickstartStageWorking(cmd - QUICKSTART_STAGE(QS_STAGE1));  /* ioctl cmd converted to stage level */
            break;
    }
    return ret;
}

static const struct file_operations_vfs g_quickstartDevOps = {
    QuickstartOpen,  /* open */
    QuickstartClose, /* close */
    NULL,  /* read */
    NULL, /* write */
    NULL,      /* seek */
    QuickstartIoctl,      /* ioctl */
    NULL,   /* mmap */
#ifndef CONFIG_DISABLE_POLL
    NULL,      /* poll */
#endif
    NULL,      /* unlink */
};

int QuickstartDevRegister(void)
{
    LOS_EventInit(&g_qsEvent);
    return register_driver(QUICKSTART_NODE, &g_quickstartDevOps, 0644, 0); /* 0644: file mode */
}

