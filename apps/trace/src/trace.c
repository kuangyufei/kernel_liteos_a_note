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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>

#define TRACE_IOC_MAGIC     'T'
#define TRACE_START         _IO(TRACE_IOC_MAGIC, 1)
#define TRACE_STOP          _IO(TRACE_IOC_MAGIC, 2)
#define TRACE_RESET         _IO(TRACE_IOC_MAGIC, 3)
#define TRACE_DUMP          _IO(TRACE_IOC_MAGIC, 4)
#define TRACE_SET_MASK      _IO(TRACE_IOC_MAGIC, 5)

#define TRACE_USR_MAX_PARAMS 3
typedef struct {
    unsigned int eventType;
    uintptr_t identity;
    uintptr_t params[TRACE_USR_MAX_PARAMS];
} UsrEventInfo;

static void TraceUsage(void)
{
    printf("\nUsage: ./trace [start] Start to trace events.\n");
    printf("\nUsage: ./trace [stop] Stop trace.\n");
    printf("\nUsage: ./trace [reset] Clear the trace record buffer.\n");
    printf("\nUsage: ./trace [dump 0/1] Format printf trace data,"
                "0/1 stands for whether to send data to studio for analysis.\n");
    printf("\nUsage: ./trace [mask num] Set trace filter event mask.\n");
    printf("\nUsage: ./trace [read nBytes] Read nBytes raw data from trace buffer.\n");
    printf("\nUsage: ./trace [write type id params..] Write a user event, no more than 3 parameters.\n");
}

static void TraceRead(int fd, size_t size)
{
    ssize_t i;
    ssize_t len;
    char *buffer = (char *)malloc(size);
    if (buffer == NULL) {
        printf("Read buffer malloc failed.\n");
        return;
    }

    len = read(fd, buffer, size);
    for (i = 0; i < len; i++) {
        printf("%02x ", buffer[i] & 0xFF);
    }
    printf("\n");
    free(buffer);
}

static void TraceWrite(int fd, int argc, char **argv)
{
    int i;
    UsrEventInfo info = {0};
    info.eventType = strtoul(argv[2], NULL, 0);
    info.identity = strtoul(argv[3], NULL, 0);
    int paramNum = (argc - 4) > TRACE_USR_MAX_PARAMS ? TRACE_USR_MAX_PARAMS : (argc - 4);
    
    for (i = 0; i < paramNum; i++) {
        info.params[i] = strtoul(argv[4 + i], NULL, 0);
    }
    (void)write(fd, &info, sizeof(UsrEventInfo));
}

int main(int argc, char **argv)
{
    int fd = open("/dev/trace", O_RDWR);
    if (fd == -1) {
        printf("Trace open failed.\n");
        exit(EXIT_FAILURE);
    }

    if (argc == 1) {
        TraceUsage();
    } else if (argc == 2 && strcmp(argv[1], "start") == 0) {
        ioctl(fd, TRACE_START, NULL);
    } else if (argc == 2 && strcmp(argv[1], "stop") == 0) {
        ioctl(fd, TRACE_STOP, NULL);
    } else if (argc == 2 && strcmp(argv[1], "reset") == 0) {
        ioctl(fd, TRACE_RESET, NULL);
    } else if (argc == 3 && strcmp(argv[1], "mask") == 0) {
        size_t mask = strtoul(argv[2], NULL, 0);
        ioctl(fd, TRACE_SET_MASK, mask);
    } else if (argc == 3 && strcmp(argv[1], "dump") == 0) {
        size_t flag = strtoul(argv[2], NULL, 0);
        ioctl(fd, TRACE_DUMP, flag);
    } else if (argc == 3 && strcmp(argv[1], "read") == 0) {
        size_t size = strtoul(argv[2], NULL, 0);
        TraceRead(fd, size);
    } else if (argc >= 4 && strcmp(argv[1], "write") == 0) {
        TraceWrite(fd, argc, argv);
    } else {
        printf("Unsupported trace command.\n");
        TraceUsage();
    }

    close(fd);
    return 0;
}
