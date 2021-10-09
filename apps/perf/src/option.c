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
#include "option.h"
#include "perf_list.h"

static int ParseOption(char **argv, int *index, PerfOption *opts)
{
    int ret = 0;
    const char *str = NULL;

    while ((opts->name != NULL) && (*opts->name != 0)) {
        if (strcmp(argv[*index], opts->name) == 0) {
            switch (opts->type) {
                case OPTION_TYPE_UINT:
                    *opts->value = strtoul(argv[++(*index)], NULL, 0);
                    break;
                case OPTION_TYPE_STRING:
                    *opts->str = argv[++(*index)];
                    break;
                case OPTION_TYPE_CALLBACK:
                    str = argv[++(*index)];
                    if ((*opts->cb)(str) != 0) {
                        printf("parse error\n");
                        ret = -1;
                    }
                    break;
                default:
                    printf("invalid option\n");
                    ret = -1;
                    break;
            }
            return ret;
        }
        opts++;
    }

    return -1;
}

int ParseOptions(int argc, char **argv, PerfOption *opts, SubCmd *cmd)
{
    int i;
    int index = 0;

    while ((index < argc) && (argv[index] != NULL) && (*argv[index] == '-')) {
        if (ParseOption(argv, &index, opts) != 0) {
            return -1;
        }
        index++;
    }

    if ((index < argc) && (argv[index] != NULL)) {
        cmd->path = argv[index];
        cmd->params[0] = argv[index];
        index++;
    } else {
        printf("no subcmd to execute\n");
        return -1;
    }

    for (i = 1; (index < argc) && (i < CMD_MAX_PARAMS); index++, i++) {
        cmd->params[i] = argv[index];
    }
    printf_debug("subcmd = %s\n", cmd->path);
    for (int j = 0; j < i; j++) {
        printf_debug("paras[%d]:%s\n", j, cmd->params[j]);
    }
    return 0;
}

int ParseIds(const char *argv, int *arr, unsigned int *len)
{
    int res, ret;
    unsigned int index  = 0;
    char *sp = NULL;
    char *this = NULL;
    char *list = strdup(argv);

    if (list == NULL) {
        printf("no memory for ParseIds\n");
        return -1;
    }

    sp = strtok_r(list, ",", &this);
    while (sp) {
        res = strtoul(sp, NULL, 0);
        if (res < 0) {
            ret = -1;
            goto EXIT;
        }
        arr[index++] = res;
        sp = strtok_r(NULL, ",", &this);
    }
    *len = index;
    ret = 0;
EXIT:
    free(list);
    return ret;
}

static inline const PerfEvent *StrToEvent(const char *str)
{
    const PerfEvent *evt = &g_events[0];

    for (; evt->event != -1; evt++) {
        if (strcmp(str, evt->name) == 0) {
            return evt;
        }
    }
    return NULL;
}

int ParseEvents(const char *argv, PerfEventConfig *eventsCfg, unsigned int *len)
{
    int ret;
    unsigned int index  = 0;
    const PerfEvent *event = NULL;
    char *sp = NULL;
    char *this = NULL;
    char *list = strdup(argv);

    if (list == NULL) {
        printf("no memory for ParseEvents\n");
        return -1;
    }

    sp = strtok_r(list, ",", &this);
    while (sp) {
        event = StrToEvent(sp);
        if (event == NULL) {
            ret = -1;
            goto EXIT;
        }

        if (index == 0) {
            eventsCfg->type = event->type;
        } else if (eventsCfg->type != event->type) {
            printf("events type must be same\n");
            ret = -1;
            goto EXIT;
        }
        eventsCfg->events[index].eventId = event->event;
        sp = strtok_r(NULL, ",", &this);
        index++;
    }
    *len = index;
    ret = 0;
EXIT:
    free(list);
    return ret;
}