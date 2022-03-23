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

#include "los_random.h"
#include "fcntl.h"
#include "hisoc/random.h"
#include "linux/kernel.h"
#include "fs/driver.h"

/*****************************************************************
随机数设备硬驱动.硬件随机数发生器，
	/dev/urandom在类UNIX系统中是一个特殊的设备文件，可以用作随机数发生器或伪随机数发生器。

*****************************************************************/
static RandomOperations g_randomOp;
void RandomOperationsInit(const RandomOperations *r)//随机数操作集初始化
{
    if (r != NULL) {
        (void)memcpy_s(&g_randomOp, sizeof(RandomOperations), r, sizeof(RandomOperations));
    } else {
        PRINT_ERR("%s %d param is invalid\n", __FUNCTION__, __LINE__);
    }
    return;
}
static int RandomHwOpen(struct file *filep)
{
    if (g_randomOp.init != NULL) {
        g_randomOp.init();
        return ENOERR;
    }
    return -1;
}

static int RandomHwClose(struct file *filep)
{
    if (g_randomOp.deinit != NULL) {
        g_randomOp.deinit();
        return ENOERR;
    }
    return -1;
}

static int RandomHwIoctl(struct file *filep, int cmd, unsigned long arg)
{
    int ret = -1;

    switch (cmd) {
        default:
            PRINT_ERR("!!!bad command!!!\n");
            return -EINVAL;
    }
    return ret;
}

static ssize_t RandomHwRead(struct file *filep, char *buffer, size_t buflen)
{
    int ret = -1;

    if (g_randomOp.read != NULL) {
        ret = g_randomOp.read(buffer, buflen);
        if (ret == ENOERR) {
            ret = buflen;
        }
    } else {
        ret = -1;
    }
    return ret;
}

static ssize_t RandomMap(struct file *filep, LosVmMapRegion *region)
{
    PRINTK("%s %d, mmap is not support\n", __FUNCTION__, __LINE__);
    return 0;
}

static const struct file_operations_vfs g_randomHwDevOps = {
    RandomHwOpen,  /* open */
    RandomHwClose, /* close */
    RandomHwRead,  /* read */
    NULL,            /* write */
    NULL,            /* seek */
    RandomHwIoctl, /* ioctl */
    RandomMap,      /* mmap */
#ifndef CONFIG_DISABLE_POLL
    NULL,            /* poll */
#endif
    NULL,            /* unlink */
};

int DevUrandomRegister(void)
{
    if (g_randomOp.support != NULL) {
        int ret = g_randomOp.support();
        if (ret) {
            return register_driver("/dev/urandom", &g_randomHwDevOps, 0666, 0); /* 0666: file mode */
        }
    }
    return -EPERM;
}

