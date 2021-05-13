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

#include "osTest.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/mount.h>
#include <dirent.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include <utime.h>
#include <sys/uio.h>

#define PROC_PATH_NAME                   "/proc/test"
#define PROC_MAIN_DIR                    "/proc"
#define PROC_MOUNT_DIR                   "/proc"
#define MOUNT_DIR_PATH                   "/proc/mounts"
#define UPTIME_DIR_PATH                  "/proc/uptime"
#define VMM_DIR_PATH                     "/proc/vmm"
#define PROCESS_DIR_PATH                 "/proc/process"
#define PROC_DEV_PATH                    NULL
#define PROC_CHINESE_NAME1               "这是一个中文名"
#define PROC_FILESYS_TYPE                "procfs"

#define PROC_NAME_LIMITTED_SIZE          300
#define PROC_NO_ERROR                    0
#define PROC_IS_ERROR                    -1

#if defined(LOSCFG_USER_TEST_SMOKE)
#endif

#if defined(LOSCFG_USER_TEST_FULL)
VOID ItFsProc001(VOID);
VOID ItFsProc002(VOID);
VOID ItFsProc003(VOID);
VOID ItFsProc004(VOID);
VOID ItFsProc005(VOID);
VOID ItFsProc006(VOID);
#endif

#if defined(LOSCFG_USER_TEST_LLT)
#endif

#if defined(LOSCFG_USER_TEST_PRESSURE)
#endif


