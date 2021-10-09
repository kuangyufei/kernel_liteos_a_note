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

#include <unistd.h>
#include <sys/wait.h>
#include <securec.h>

#ifdef LOSCFG_FS_VFS
#include <fcntl.h>
#include <errno.h>
#endif

#include "perf.h"
#include "option.h"
#include "perf_record.h"

#define PERF_FILE_MODE 0644
static PerfConfigAttr g_recordAttr;
static const char *g_savePath = "/storage/data/perf.data";

static inline int GetEvents(const char *argv)
{
    return ParseEvents(argv, &g_recordAttr.eventsCfg, &g_recordAttr.eventsCfg.eventsNr);
}

static inline int GetTids(const char *argv)
{
    return ParseIds(argv, (int *)g_recordAttr.taskIds, &g_recordAttr.taskIdsNr);
}

static inline int GetPids(const char *argv)
{
    return ParseIds(argv, (int *)g_recordAttr.processIds, &g_recordAttr.processIdsNr);
}

static PerfOption g_recordOpts[] = {
    OPTION_CALLBACK("-e", GetEvents),
    OPTION_CALLBACK("-t", GetTids),
    OPTION_CALLBACK("-P", GetPids),
    OPTION_STRING("-o", &g_savePath),
    OPTION_UINT("-p", &g_recordAttr.eventsCfg.events[0].period),
    OPTION_UINT("-s", &g_recordAttr.sampleType),
    OPTION_UINT("-d", &g_recordAttr.eventsCfg.predivided),
};

static int PerfRecordAttrInit(void)
{
    PerfConfigAttr attr = {
        .eventsCfg = {
#ifdef LOSCFG_PERF_HW_PMU
            .type = PERF_EVENT_TYPE_HW,
            .events = {
                [0] = {PERF_COUNT_HW_CPU_CYCLES, 0xFFFF},
            },
#elif defined LOSCFG_PERF_TIMED_PMU
            .type = PERF_EVENT_TYPE_TIMED,
            .events = {
                [0] = {PERF_COUNT_CPU_CLOCK, 100},
            },
#elif defined LOSCFG_PERF_SW_PMU
            .type = PERF_EVENT_TYPE_SW,
            .events = {
                [0] = {PERF_COUNT_SW_TASK_SWITCH, 1},
            },
#endif
            .eventsNr = 1, /* 1 event */
            .predivided = 0,
        },
        .taskIds = {0},
        .taskIdsNr = 0,
        .processIds = {0},
        .processIdsNr = 0,
        .needSample = 1,
        .sampleType = PERF_RECORD_IP | PERF_RECORD_CALLCHAIN,
    };

    return memcpy_s(&g_recordAttr, sizeof(PerfConfigAttr), &attr, sizeof(PerfConfigAttr)) != EOK ? -1 : 0;
}

ssize_t PerfWriteFile(const char *filePath, const char *buf, ssize_t bufSize)
{
#ifdef LOSCFG_FS_VFS
    int fd = -1;
    ssize_t totalToWrite = bufSize;
    ssize_t totalWrite = 0;

    if (filePath == NULL || buf == NULL || bufSize == 0) {
        printf("filePath: %p, buf: %p, bufSize: %u!\n", filePath, buf, bufSize);
        return -1;
    }

    fd = open(filePath, O_CREAT | O_RDWR | O_TRUNC, PERF_FILE_MODE);
    if (fd < 0) {
        printf("create file [%s] failed, fd: %d, %s!\n", filePath, fd, strerror(errno));
        return -1;
    }
    while (totalToWrite > 0) {
        ssize_t writeThisTime = write(fd, buf, totalToWrite);
        if (writeThisTime < 0) {
            printf("failed to write file [%s], %s!\n", filePath, strerror(errno));
            (void)close(fd);
            return -1;
        }
        buf += writeThisTime;
        totalToWrite -= writeThisTime;
        totalWrite += writeThisTime;
    }
    (void)fsync(fd);
    (void)close(fd);

    return (totalWrite == bufSize) ? 0 : -1;
#else
    (void)filePath;
    PerfPrintBuffer(buf, bufSize);
    return 0;
#endif
}

void PerfRecord(int fd, int argc, char **argv)
{
    int ret;
    int child;
    char *buf;
    ssize_t len;
    SubCmd cmd = {0};

    if (argc < 3) { /* perf record argc is at least 3 */
        return;
    }

    ret = PerfRecordAttrInit();
    if (ret != 0) {
        printf("perf record attr init failed\n");
        return;
    }

    ret = ParseOptions(argc - 2, &argv[2], g_recordOpts, &cmd); /* parse option and cmd begin at index 2 */
    if (ret != 0) {
        printf("parse error\n");
        return;
    }

    ret = PerfConfig(fd, &g_recordAttr);
    if (ret != 0) {
        printf("perf config failed\n");
        return;
    }

    PerfStart(fd, 0);
    child = fork();
    if (child < 0) {
        printf("fork error\n");
        PerfStop(fd);
        return;
    } else if (child == 0) {
        (void)execve(cmd.path, cmd.params, NULL);
        exit(0);
    }

    waitpid(child, 0, 0);
    PerfStop(fd);

    buf = (char *)malloc(LOSCFG_PERF_BUFFER_SIZE);
    if (buf == NULL) {
        printf("no memory for read perf 0x%x\n", LOSCFG_PERF_BUFFER_SIZE);
        return;
    }
    len = PerfRead(fd, buf, LOSCFG_PERF_BUFFER_SIZE);
    ret = PerfWriteFile(g_savePath, buf, len);
    if (ret == 0) {
        printf("save perf data success at %s\n", g_savePath);
    } else {
        printf("save perf data failed at %s\n", g_savePath);
    }
    free(buf);
}
