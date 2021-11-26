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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include "perf.h"

#define PERF_IOC_MAGIC     'T'
#define PERF_START         _IO(PERF_IOC_MAGIC, 1)
#define PERF_STOP          _IO(PERF_IOC_MAGIC, 2)

void PerfUsage(void)
{
    printf("\nUsage: ./perf start [id]. Start perf.\n");
    printf("\nUsage: ./perf stop. Stop perf.\n");
    printf("\nUsage: ./perf read <nBytes>. Read nBytes raw data from perf buffer and print out.\n");
    printf("\nUsage: ./perf list. List events to be used in -e.\n");
    printf("\nUsage: ./perf stat/record [option] <command>. \n"
                    "-e, event selector. use './perf list' to list available events.\n"
                    "-p, event period.\n"
                    "-o, perf data output filename.\n"
                    "-t, taskId filter(allowlist), if not set perf will sample all tasks.\n"
                    "-s, type of data to sample defined in PerfSampleType los_perf.h.\n"
                    "-P, processId filter(allowlist), if not set perf will sample all processes.\n"
                    "-d, whether to prescaler (once every 64 counts),"
                    "which only take effect on cpu cycle hardware event.\n"
    );
}

static void PerfSetPeriod(PerfConfigAttr *attr)
{
    int i;
    for (i = 1; i < attr->eventsCfg.eventsNr; i++) {
        attr->eventsCfg.events[i].period = attr->eventsCfg.events[0].period;
    }
}

void PerfPrintBuffer(const char *buf, ssize_t num)
{
#define BYTES_PER_LINE  4
    ssize_t i;
    for (i = 0; i < num; i++) {
        printf(" %02x", (unsigned char)buf[i]);
        if (((i + 1) % BYTES_PER_LINE) == 0) {
            printf("\n");
        }
    }
    printf("\n");
}

void PerfDumpAttr(PerfConfigAttr *attr)
{
    int i;
    printf_debug("attr->type: %d\n", attr->eventsCfg.type);
    for (i = 0; i < attr->eventsCfg.eventsNr; i++) {
        printf_debug("attr->events[%d]: %d, 0x%x\n", i, attr->eventsCfg.events[i].eventId,
            attr->eventsCfg.events[i].period);
    }
    printf_debug("attr->predivided: %d\n", attr->eventsCfg.predivided);
    printf_debug("attr->sampleType: 0x%x\n", attr->sampleType);

    for (i = 0; i < attr->taskIdsNr; i++) {
        printf_debug("attr->taskIds[%d]: %d\n", i, attr->taskIds[i]);
    }

    for (i = 0; i < attr->processIdsNr; i++) {
        printf_debug("attr->processIds[%d]: %d\n", i, attr->processIds[i]);
    }

    printf_debug("attr->needSample: %d\n", attr->needSample);
}


void PerfStart(int fd, size_t sectionId)
{
    (void)ioctl(fd, PERF_START, sectionId);
}

void PerfStop(int fd)
{
    (void)ioctl(fd, PERF_STOP, NULL);
}

int PerfConfig(int fd, PerfConfigAttr *attr)
{
    if (attr == NULL) {
        return -1;
    }
    PerfSetPeriod(attr);
    PerfDumpAttr(attr);
    return write(fd, attr, sizeof(PerfConfigAttr));
}

ssize_t PerfRead(int fd, char *buf, size_t size)
{
    ssize_t len;
    if (buf == NULL) {
        printf("Read buffer is null.\n");
        return 0;
    }

    len = read(fd, buf, size);
    return len;
}