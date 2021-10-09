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

#include "perf_output_pri.h"

STATIC PERF_BUF_NOTIFY_HOOK g_perfBufNotifyHook = NULL;
STATIC PERF_BUF_FLUSH_HOOK g_perfBufFlushHook = NULL;
STATIC PerfOutputCB g_perfOutputCb;

STATIC VOID OsPerfDefaultNotify(VOID)
{
    PRINT_INFO("perf buf waterline notify!\n");
}

UINT32 OsPerfOutputInit(VOID *buf, UINT32 size)
{
    UINT32 ret;
    BOOL releaseFlag = FALSE;
    if (buf == NULL) {
        buf = LOS_MemAlloc(m_aucSysMem1, size);
        if (buf == NULL) {
            return LOS_NOK;
        } else {
            releaseFlag = TRUE;
        }
    }
    ret = LOS_CirBufInit(&g_perfOutputCb.ringbuf, buf, size);
    if (ret != LOS_OK) {
        goto RELEASE;
    }
    g_perfOutputCb.waterMark = size / PERF_BUFFER_WATERMARK_ONE_N;
    g_perfBufNotifyHook = OsPerfDefaultNotify;
    return ret;
RELEASE:
    if (releaseFlag) {
        (VOID)LOS_MemFree(m_aucSysMem1, buf);
    }
    return ret;
}

VOID OsPerfOutputFlush(VOID)
{
    if (g_perfBufFlushHook != NULL) {
        g_perfBufFlushHook(g_perfOutputCb.ringbuf.fifo, g_perfOutputCb.ringbuf.size);
    }
}

UINT32 OsPerfOutputRead(CHAR *dest, UINT32 size)
{
    OsPerfOutputFlush();
    return LOS_CirBufRead(&g_perfOutputCb.ringbuf, dest, size);
}

STATIC BOOL OsPerfOutputBegin(UINT32 size)
{
    if (g_perfOutputCb.ringbuf.remain < size) {
        PRINT_INFO("perf buf has no enough space for 0x%x\n", size);
        return FALSE;
    }
    return TRUE;
}

STATIC VOID OsPerfOutputEnd(VOID)
{
    OsPerfOutputFlush();
    if (LOS_CirBufUsedSize(&g_perfOutputCb.ringbuf) >= g_perfOutputCb.waterMark) {
        if (g_perfBufNotifyHook != NULL) {
            g_perfBufNotifyHook();
        }
    }
}

UINT32 OsPerfOutputWrite(CHAR *data, UINT32 size)
{
    if (!OsPerfOutputBegin(size)) {
        return LOS_NOK;
    }

    LOS_CirBufWrite(&g_perfOutputCb.ringbuf, data, size);

    OsPerfOutputEnd();
    return LOS_OK;
}

VOID OsPerfOutputInfo(VOID)
{
    PRINT_EMG("dump perf data, addr: %p length: %#x\n", g_perfOutputCb.ringbuf.fifo, g_perfOutputCb.ringbuf.size);
}

VOID OsPerfNotifyHookReg(const PERF_BUF_NOTIFY_HOOK func)
{
    g_perfBufNotifyHook = func;
}

VOID OsPerfFlushHookReg(const PERF_BUF_FLUSH_HOOK func)
{
    g_perfBufFlushHook = func;
}
