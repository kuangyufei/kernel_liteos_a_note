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

#include "proc_fs.h"
#include "time.h"
#include "errno.h"
#include "los_cpup.h"

#define MSEC_TO_NSEC        1000000
#define SEC_TO_MSEC         1000
#define DECIMAL_TO_PERCENT  100

static int UptimeProcFill(struct SeqBuf *seqBuf, void *v)
{
    struct timespec curtime = {0, 0};
    int ret;
#ifdef LOSCFG_KERNEL_CPUP
    float idleRate;
    float idleMSec;
    float usage;
#endif
    ret = clock_gettime(CLOCK_MONOTONIC, &curtime);
    if (ret < 0) {
        PRINT_ERR("clock_gettime error!\n");
        return -get_errno();
    }

#ifdef LOSCFG_KERNEL_CPUP
    usage = (float)LOS_HistorySysCpuUsage(CPUP_ALL_TIME);
    idleRate = LOSCFG_KERNEL_CORE_NUM - usage / LOS_CPUP_PRECISION_MULT / DECIMAL_TO_PERCENT;
    idleMSec = ((float)curtime.tv_sec * SEC_TO_MSEC + curtime.tv_nsec / MSEC_TO_NSEC) * idleRate;

    (void)LosBufPrintf(seqBuf, "%llu.%03llu %llu.%03llu\n", (unsigned long long)curtime.tv_sec,
                       (unsigned long long)(curtime.tv_nsec / MSEC_TO_NSEC),
                       (unsigned long long)(idleMSec / SEC_TO_MSEC),
                       (unsigned long long)((unsigned long long)idleMSec % SEC_TO_MSEC));
#else
    (void)LosBufPrintf(seqBuf, "%llu.%03llu\n", (unsigned long long)curtime.tv_sec,
                       (unsigned long long)(curtime.tv_nsec / MSEC_TO_NSEC));

#endif
    return 0;
}

static const struct ProcFileOperations UPTIME_PROC_FOPS = {
    .read       = UptimeProcFill,
};

void ProcUptimeInit(void)
{
    struct ProcDirEntry *pde = CreateProcEntry("uptime", 0, NULL);
    if (pde == NULL) {
        PRINT_ERR("creat /proc/uptime error!\n");
        return;
    }

    pde->procFileOps = &UPTIME_PROC_FOPS;
}

