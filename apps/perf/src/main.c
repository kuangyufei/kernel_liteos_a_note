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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "perf.h"
#include "perf_list.h"
#include "perf_stat.h"
#include "perf_record.h"

int main(int argc, char **argv)
{
#define TWO_ARGS    2
#define THREE_ARGS  3
    int fd = open("/dev/perf", O_RDWR);
    if (fd == -1) {
        printf("Perf open failed.\n");
        exit(EXIT_FAILURE);
    }

    if (argc == 1) {
        PerfUsage();
    } else if ((argc == TWO_ARGS) && strcmp(argv[1], "start") == 0) {
        PerfStart(fd, 0);
    } else if ((argc == THREE_ARGS) && strcmp(argv[1], "start") == 0) {
        size_t id = strtoul(argv[THREE_ARGS - 1], NULL, 0);
        PerfStart(fd, id);
    } else if ((argc == TWO_ARGS) && strcmp(argv[1], "stop") == 0) {
        PerfStop(fd);
    } else if ((argc == THREE_ARGS) && strcmp(argv[1], "read") == 0) {
        size_t size = strtoul(argv[THREE_ARGS - 1], NULL, 0);
        if (size <= 0) {
            goto EXIT:
        }

        char *buf = (char *)malloc(size);
        if (buf != NULL) {
            int len = PerfRead(fd, buf, size);
            PerfPrintBuffer(buf, len);
            free(buf);
            buf = NULL;
        }
    } else if ((argc == TWO_ARGS) && strcmp(argv[1], "list") == 0) {
        PerfList();
    } else if ((argc >= THREE_ARGS) && strcmp(argv[1], "stat") == 0) {
        PerfStat(fd, argc, argv);
    } else if ((argc >= THREE_ARGS) && strcmp(argv[1], "record") == 0) {
        PerfRecord(fd, argc, argv);
    } else {
        printf("Unsupported perf command.\n");
        PerfUsage();
    }

EXIT:
    close(fd);
    return 0;
}
