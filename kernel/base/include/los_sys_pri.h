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

#ifndef _LOS_SYS_PRI_H
#define _LOS_SYS_PRI_H

#include "los_sys.h"
#include "los_base_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_sys
 * Number of milliseconds in one second.
 */
#define OS_SYS_MS_PER_SECOND   1000

/**
 * @ingroup los_sys
 * Number of microseconds in one second.
 */
#define OS_SYS_US_PER_SECOND   1000000

/**
 * @ingroup los_sys
 * Number of nanoseconds in one second.
 */
#define OS_SYS_NS_PER_SECOND   1000000000

/**
 * @ingroup los_sys
 * Number of microseconds in one milliseconds.
 */
#define OS_SYS_US_PER_MS        1000

/**
 * @ingroup los_sys
 * Number of nanoseconds in one milliseconds.
 */
#define OS_SYS_NS_PER_MS        1000000

/**
 * @ingroup los_sys
 * Number of nanoseconds in one microsecond.
 */
#define OS_SYS_NS_PER_US        1000

/**
 * @ingroup los_sys
 * Number of cycle in one tick.
 */
#define OS_CYCLE_PER_TICK       (OS_SYS_CLOCK / LOSCFG_BASE_CORE_TICK_PER_SECOND)

/**
 * @ingroup los_sys
 * Number of nanoseconds in one cycle.
 */
#define OS_NS_PER_CYCLE         (OS_SYS_NS_PER_SECOND / OS_SYS_CLOCK)

/**
 * @ingroup los_sys
 * Number of microseconds in one tick.
 */
#define OS_US_PER_TICK          (OS_SYS_US_PER_SECOND / LOSCFG_BASE_CORE_TICK_PER_SECOND)

/**
 * @ingroup los_sys
 * Number of nanoseconds in one tick.
 */
#define OS_NS_PER_TICK          (OS_SYS_NS_PER_SECOND / LOSCFG_BASE_CORE_TICK_PER_SECOND)

#define OS_US_TO_CYCLE(time, freq)  ((((time) / OS_SYS_US_PER_SECOND) * (freq)) + \
    (((time) % OS_SYS_US_PER_SECOND) * (freq) / OS_SYS_US_PER_SECOND))

#define OS_SYS_US_TO_CYCLE(time) OS_US_TO_CYCLE((time), OS_SYS_CLOCK)

#define OS_CYCLE_TO_US(cycle, freq) ((((cycle) / (freq)) * OS_SYS_US_PER_SECOND) + \
    ((cycle) % (freq) * OS_SYS_US_PER_SECOND / (freq)))

#define OS_SYS_CYCLE_TO_US(cycle) OS_CYCLE_TO_US((cycle), OS_SYS_CLOCK)

/**
 * @ingroup los_sys
 * The maximum length of name.
 */
#define OS_SYS_APPVER_NAME_MAX 64

/**
 * @ingroup los_sys
 * The magic word.
 */
#define OS_SYS_MAGIC_WORD      0xAAAAAAAA

/**
 * @ingroup los_sys
 * The initialization value of stack space.
 */
#define OS_SYS_EMPTY_STACK     0xCACACACA

/**
 * @ingroup los_sys
 * Convert nanoseconds to Ticks.
 */
extern UINT32 OsNS2Tick(UINT64 nanoseconds);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_SYS_PRI_H */
