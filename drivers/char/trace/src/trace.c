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
#include "linux/kernel.h"
#include "sys/ioctl.h"
#include "fs/driver.h"
#include "los_dev_trace.h"
#include "los_trace.h"
#include "los_hook.h"

#define TRACE_DRIVER "/dev/trace"
#define TRACE_DRIVER_MODE 0666

/* trace ioctl */
#define TRACE_IOC_MAGIC     'T'
#define TRACE_START         _IO(TRACE_IOC_MAGIC, 1)
#define TRACE_STOP          _IO(TRACE_IOC_MAGIC, 2)
#define TRACE_RESET         _IO(TRACE_IOC_MAGIC, 3)
#define TRACE_DUMP          _IO(TRACE_IOC_MAGIC, 4)
#define TRACE_SET_MASK      _IO(TRACE_IOC_MAGIC, 5)

static int TraceOpen(struct file *filep)
{
    return 0;
}

static int TraceClose(struct file *filep)
{
    return 0;
}

static ssize_t TraceRead(struct file *filep, char *buffer, size_t buflen)
{
    /* trace record buffer read */
    ssize_t len = buflen;
    OfflineHead *records;
    int ret;
    int realLen;

    if (len % sizeof(unsigned int)) {
        PRINT_ERR("Buffer size not aligned by 4 bytes\n");
        return -EINVAL;
    }

    records = LOS_TraceRecordGet();
    if (records == NULL) {
        PRINT_ERR("Trace read failed, check whether trace mode is set to offline\n");
        return -EINVAL;
    }

    realLen = buflen < records->totalLen ? buflen : records->totalLen;
    ret = LOS_CopyFromKernel((void *)buffer, buflen, (void *)records, realLen);
    if (ret != 0) {
        return -EINVAL;
    }

    return realLen;
}

static ssize_t TraceWrite(struct file *filep, const char *buffer, size_t buflen)
{
    /* trace usr event here */
    int ret;
    UsrEventInfo *info = NULL;
    int infoLen = sizeof(UsrEventInfo);

    if (buflen != infoLen) {
        PRINT_ERR("Buffer size not %d bytes\n", infoLen);
        return -EINVAL;
    }

    info = LOS_MemAlloc(m_aucSysMem0, infoLen);
    if (info == NULL) {
        return -ENOMEM;
    }
    memset_s(info, infoLen, 0, infoLen);

    ret = LOS_CopyToKernel(info, infoLen, buffer, buflen);
    if (ret != 0) {
        LOS_MemFree(m_aucSysMem0, info);
        return -EINVAL;
    }
    OsHookCall(LOS_HOOK_TYPE_USR_EVENT, info, infoLen);
    return 0;
}

static int TraceIoctl(struct file *filep, int cmd, unsigned long arg)
{
    switch (cmd) {
        case TRACE_START:
            return LOS_TraceStart();
        case TRACE_STOP:
            LOS_TraceStop();
            break;
        case TRACE_RESET:
            LOS_TraceReset();
            break;
        case TRACE_DUMP:
            LOS_TraceRecordDump((BOOL)arg);
            break;
        case TRACE_SET_MASK:
            LOS_TraceEventMaskSet((UINT32)arg);
            break;
        default:
            PRINT_ERR("Unknown trace ioctl cmd:%d\n", cmd);
            return -EINVAL;
    }
    return 0;
}

static const struct file_operations_vfs g_traceDevOps = {
    TraceOpen,       /* open */
    TraceClose,      /* close */
    TraceRead,       /* read */
    TraceWrite,      /* write */
    NULL,            /* seek */
    TraceIoctl,      /* ioctl */
    NULL,            /* mmap */
#ifndef CONFIG_DISABLE_POLL
    NULL,            /* poll */
#endif
    NULL,            /* unlink */
};

int DevTraceRegister(void)
{
    return register_driver(TRACE_DRIVER, &g_traceDevOps, TRACE_DRIVER_MODE, 0); /* 0666: file mode */
}
