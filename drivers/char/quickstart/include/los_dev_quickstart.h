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

#ifndef __LOS_DEV_QUICKSTART_H__
#define __LOS_DEV_QUICKSTART_H__

#include "los_typedef.h"
#include "sys/ioctl.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

typedef enum {
    QS_STAGE1 = 1,   /* 1: start from stage1, 0 is already called in kernel process */
    QS_STAGE2,       /* system init stage No 2 */
    QS_STAGE3,       /* system init stage No 3 */
    QS_STAGE_LIMIT
} QuickstartStage;

typedef enum {
    QS_UNREGISTER = QS_STAGE_LIMIT,  /* quickstart dev unregister */
    QS_NOTIFY,          /* quickstart notify */
    QS_LISTEN,          /* quickstart listen */
    QS_CTL_LIMIT
} QuickstartConctrl;

typedef struct {
    unsigned int pid;
    unsigned int events;
} QuickstartMask;

#define QUICKSTART_IOC_MAGIC    'T'
#define QUICKSTART_UNREGISTER   _IO(QUICKSTART_IOC_MAGIC, QS_UNREGISTER)
#define QUICKSTART_NOTIFY       _IO(QUICKSTART_IOC_MAGIC, QS_NOTIFY)
#define QUICKSTART_LISTEN       _IOR(QUICKSTART_IOC_MAGIC, QS_LISTEN, QuickstartMask)
#define QUICKSTART_STAGE(x)     _IO(QUICKSTART_IOC_MAGIC, (x))

#define QUICKSTART_NODE         "/dev/quickstart"

#define QS_STAGE_CNT            (QS_STAGE_LIMIT - QS_STAGE1)

typedef void (*SysteminitHook)(void);

typedef struct {
    SysteminitHook func[QS_STAGE_CNT];
} LosSysteminitHook;

extern void QuickstartHookRegister(LosSysteminitHook hooks);

extern int QuickstartDevRegister(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
