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

#include "fcntl.h"
#include "user_copy.h"
#include "sys/ioctl.h"
#include "fs/driver.h"
#include "los_dev_perf.h"
#include "los_perf.h"
#include "los_init.h"

#define PERF_DRIVER "/dev/perf"
#define PERF_DRIVER_MODE 0666

/* perf ioctl */
#define PERF_IOC_MAGIC     'T'
#define PERF_START         _IO(PERF_IOC_MAGIC, 1)
#define PERF_STOP          _IO(PERF_IOC_MAGIC, 2)

static int PerfOpen(struct file *filep)
{
    (void)filep;
    return 0;
}

static int PerfClose(struct file *filep)
{
    (void)filep;
    return 0;
}

static ssize_t PerfRead(struct file *filep, char *buffer, size_t buflen)
{
    /* perf record buffer read */
    (void)filep;
    int ret;
    int realLen;

    char *records = LOS_MemAlloc(m_aucSysMem0, buflen);
    if (records == NULL) {
        return -ENOMEM;
    }

    realLen = LOS_PerfDataRead(records, buflen); /* get sample data */
    if (realLen == 0) {
        PRINT_ERR("Perf read failed, check whether perf is configured to sample mode.\n");
        ret = -EINVAL;
        goto EXIT;
    }

    ret = LOS_CopyFromKernel((void *)buffer, buflen, (void *)records, realLen);
    if (ret != 0) {
        ret = -EINVAL;
        goto EXIT;
    }

    ret = realLen;
EXIT:
    LOS_MemFree(m_aucSysMem0, records);
    return ret;
}

static ssize_t PerfConfig(struct file *filep, const char *buffer, size_t buflen)
{
    (void)filep;
    int ret;
    PerfConfigAttr attr = {0};
    int attrlen = sizeof(PerfConfigAttr);

    if (buflen != attrlen) {
        PRINT_ERR("PerfConfigAttr is %d bytes not %d\n", attrlen, buflen);
        return -EINVAL;
    }

    ret = LOS_CopyToKernel(&attr, attrlen, buffer, buflen);
    if (ret != 0) {
        return -EINVAL;
    }

    ret = LOS_PerfConfig(&attr);
    if (ret != LOS_OK) {
        PRINT_ERR("perf config error %u\n", ret);
        return -EINVAL;
    }

    return 0;
}

static int PerfIoctl(struct file *filep, int cmd, unsigned long arg)
{
    (void)filep;
    switch (cmd) {
        case PERF_START:
            LOS_PerfStart((UINT32)arg);
            break;
        case PERF_STOP:
            LOS_PerfStop();
            break;
        default:
            PRINT_ERR("Unknown perf ioctl cmd:%d\n", cmd);
            return -EINVAL;
    }
    return 0;
}

static const struct file_operations_vfs g_perfDevOps = {
    PerfOpen,        /* open */
    PerfClose,       /* close */
    PerfRead,        /* read */
    PerfConfig,      /* write */
    NULL,            /* seek */
    PerfIoctl,       /* ioctl */
    NULL,            /* mmap */
#ifndef CONFIG_DISABLE_POLL
    NULL,            /* poll */
#endif
    NULL,            /* unlink */
};

int DevPerfRegister(void)
{
    return register_driver(PERF_DRIVER, &g_perfDevOps, PERF_DRIVER_MODE, 0); /* 0666: file mode */
}

LOS_MODULE_INIT(DevPerfRegister, LOS_INIT_LEVEL_KMOD_EXTENDED);
