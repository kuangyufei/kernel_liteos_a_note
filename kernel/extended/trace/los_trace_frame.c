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
#include "los_trace_frame.h"


#ifdef LOSCFG_KERNEL_TRACE

INT32 OsTaskTrace(UINT8 *inputBuffer, UINT32 bufLen, va_list ap)
{
    va_list ap2;
    va_copy(ap2, ap);

    TaskTraceFrame *x = (TaskTraceFrame *)inputBuffer;
    if (sizeof(TaskTraceFrame) > bufLen) {
        va_end(ap2);
        return 0;
    }

    SETPARAM(ap2, x, taskEntry, UINTPTR);
    SETPARAM(ap2, x, status, UINT16);
    SETPARAM(ap2, x, mask, UINT16);
    SETPARAM(ap2, x, ipcId, UINTPTR);

    va_end(ap2);
    return sizeof(TaskTraceFrame);
}

INT32 OsIpcTrace(UINT8 *inputBuffer, UINT32 bufLen, va_list ap)
{
    va_list ap2;
    va_copy(ap2, ap);

    IpcTraceFrame *x = (IpcTraceFrame *)inputBuffer;
    if (sizeof(IpcTraceFrame) > bufLen) {
        va_end(ap2);
        return 0;
    }

    SETPARAM(ap2, x, srcTid, UINT32);
    SETPARAM(ap2, x, srcPid, UINT32);
    SETPARAM(ap2, x, dstTid, UINT32);
    SETPARAM(ap2, x, dstPid, UINT32);
    SETPARAM(ap2, x, msgType, UINT8);
    SETPARAM(ap2, x, code, UINT8);
    SETPARAM(ap2, x, operation, UINT8);
    SETPARAM(ap2, x, ipcStatus, UINT8);

    va_end(ap2);
    return sizeof(IpcTraceFrame);
}

INT32 OsMemTimeTrace(UINT8 *inputBuffer, UINT32 bufLen, va_list ap)
{
    va_list ap2;
    va_copy(ap2, ap);

    MemTimeTraceFrame *x = (MemTimeTraceFrame *)inputBuffer;
    if (sizeof(MemTimeTraceFrame) > bufLen) {
        va_end(ap2);
        return 0;
    }

    SETPARAM(ap2, x, poolAddr, UINT16);
    SETPARAM(ap2, x, type, UINT16);
    SETPARAM(ap2, x, timeUsed, UINT32);

    va_end(ap2);
    return sizeof(MemTimeTraceFrame);
}

INT32 OsMemInfoTrace(UINT8 *inputBuffer, UINT32 bufLen, va_list ap)
{
    va_list ap2;
    va_copy(ap2, ap);

    MemInfoTraceFrame *x = (MemInfoTraceFrame *)inputBuffer;
    if (sizeof(MemInfoTraceFrame) > bufLen) {
        va_end(ap2);
        return 0;
    }

    SETPARAM(ap2, x, poolAddr, UINT16);
    SETPARAM(ap2, x, fragment, UINT8);
    SETPARAM(ap2, x, usage, UINT8);
    SETPARAM(ap2, x, freeTotalSize, UINT32);
    SETPARAM(ap2, x, maxFreeSize, UINT32);
    SETPARAM(ap2, x, usedNodeNum, UINT16);
    SETPARAM(ap2, x, freeNodeNum, UINT16);

    va_end(ap2);
    return sizeof(MemInfoTraceFrame);
}
#endif

