/*
 * Copyright (c) 2013-2020, Huawei Technologies Co., Ltd. All rights reserved.
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

#include "los_trace_pri.h"
#include "securec.h"
#include "los_typedef.h"
#include "los_task_pri.h"
#include "ctype.h"

#ifdef LOSCFG_SHELL
#include "shcmd.h"
#include "shell.h"
#include "unistd.h"
#include "stdlib.h"
#endif


#ifndef LOSCFG_KERNEL_TRACE
VOID LOS_TraceInit(VOID)
{
    return;
}

UINT32 LOS_TraceReg(TraceType traceType, WriteHook inHook, const CHAR *typeStr, TraceSwitch onOff)
{
    (VOID)traceType;
    (VOID)inHook;
    (VOID)onOff;
    (VOID)typeStr;
    return LOS_OK;
}

UINT32 LOS_TraceUnreg(TraceType traceType)
{
    (VOID)traceType;
    return LOS_OK;
}

VOID LOS_Trace(TraceType traceType, ...)
{
    (VOID)traceType;
    return;
}

VOID LOS_TraceSwitch(TraceSwitch onOff)
{
    (VOID)onOff;
}

UINT32 LOS_TraceTypeSwitch(TraceType traceType, TraceSwitch onOff)
{
    (VOID)traceType;
    (VOID)onOff;
    return LOS_OK;
}

VOID LOS_TracePrint(VOID)
{
    return;
}

INT32 LOS_Trace2File(const CHAR *filename)
{
    (VOID)filename;
    return 0;
}

UINT8 *LOS_TraceBufDataGet(UINT32 *desLen, UINT32 *relLen)
{
    (VOID)desLen;
    (VOID)relLen;
    return NULL;
}

#else

SPIN_LOCK_INIT(g_traceSpin);
#define TRACE_LOCK(state)       LOS_SpinLockSave(&g_traceSpin, &(state))
#define TRACE_UNLOCK(state)     LOS_SpinUnlockRestore(&g_traceSpin, (state))

#define TMP_DATALEN 128

STATIC UINT8 traceBufArray[LOS_TRACE_BUFFER_SIZE];
STATIC TraceBufferCtl traceBufCtl;
STATIC TraceHook traceFunc[LOS_TRACE_TYPE_MAX + 1];

VOID LOS_TraceInit(VOID)
{
    UINT32 intSave;

    /* Initialize the global variable. */
    (VOID)memset_s((VOID *)traceBufArray, LOS_TRACE_BUFFER_SIZE, 0, LOS_TRACE_BUFFER_SIZE);
    (VOID)memset_s(&traceBufCtl, sizeof(traceBufCtl), 0, sizeof(traceBufCtl));
    (VOID)memset_s((VOID *)traceFunc, sizeof(traceFunc), 0, sizeof(traceFunc));

    TRACE_LOCK(intSave);

    /* Initialize trace contrl. */
    traceBufCtl.bufLen = LOS_TRACE_BUFFER_SIZE;
    traceBufCtl.dataBuf = traceBufArray;
    traceBufCtl.onOff = LOS_TRACE_ENABLE;

    TRACE_UNLOCK(intSave);
}

UINT32 LOS_TraceReg(TraceType traceType, WriteHook inHook, const CHAR *typeStr, TraceSwitch onOff)
{
    UINT32 intSave;
    INT32 i;

    if ((traceType < LOS_TRACE_TYPE_MIN) || (traceType > LOS_TRACE_TYPE_MAX)) {
        return LOS_ERRNO_TRACE_TYPE_INVALID;
    }

    if (inHook == NULL) {
        return LOS_ERRNO_TRACE_FUNCTION_NULL;
    }

    TRACE_LOCK(intSave);
    /* if inputHook is NULL，return failed. */
    if (traceFunc[traceType].inputHook != NULL) {
        PRINT_ERR("Registered Failed!\n");
        for (i = 0; i <= LOS_TRACE_TYPE_MAX; i++) {
            if (traceFunc[i].inputHook == NULL) {
                PRINTK("type:%d ", i);
            }
        }
        PRINTK("could be registered\n");
        TRACE_UNLOCK(intSave);
        return LOS_ERRNO_TRACE_TYPE_EXISTED;
    } else {
        traceFunc[traceType].inputHook = inHook;
        traceFunc[traceType].onOff = onOff;
        traceFunc[traceType].typeStr = typeStr;
    }
    TRACE_UNLOCK(intSave);
    return LOS_OK;
}

UINT32 LOS_TraceUnreg(TraceType traceType)
{
    UINT32 intSave;

    if ((traceType < LOS_TRACE_TYPE_MIN) || (traceType > LOS_TRACE_TYPE_MAX)) {
        return LOS_ERRNO_TRACE_TYPE_INVALID;
    }

    TRACE_LOCK(intSave);
    /* if inputHook is NULL，return failed. */
    if (traceFunc[traceType].inputHook == NULL) {
        PRINT_ERR("Trace not exist!\n");
        TRACE_UNLOCK(intSave);
        return LOS_ERRNO_TRACE_TYPE_NOT_EXISTED;
    } else {
        traceFunc[traceType].inputHook = NULL;
        traceFunc[traceType].onOff = LOS_TRACE_DISABLE;
        traceFunc[traceType].typeStr = NULL;
    }
    TRACE_UNLOCK(intSave);
    return LOS_OK;
}


VOID LOS_TraceSwitch(TraceSwitch onOff)
{
    traceBufCtl.onOff = onOff;
}

UINT32 LOS_TraceTypeSwitch(TraceType traceType, TraceSwitch onOff)
{
    UINT32 intSave;
    if (traceType < LOS_TRACE_TYPE_MIN || traceType > LOS_TRACE_TYPE_MAX) {
        return LOS_ERRNO_TRACE_TYPE_INVALID;
    }
    TRACE_LOCK(intSave);
    if (traceFunc[traceType].inputHook != NULL) {
        traceFunc[traceType].onOff = onOff;
        TRACE_UNLOCK(intSave);
        return LOS_OK;
    }
    TRACE_UNLOCK(intSave);
    return LOS_ERRNO_TRACE_TYPE_NOT_EXISTED;
}

STATIC UINT32 OsFindReadFrameHead(UINT32 readIndex, UINT32 dataSize)
{
    UINT32 historySize = 0;
    UINT32 index = readIndex;
    while (historySize < dataSize) {
        historySize += ((FrameHead *)&(traceBufCtl.dataBuf[index]))->frameSize;
        index = readIndex + historySize;
        if (index >= traceBufCtl.bufLen) {
            index = index - traceBufCtl.bufLen;
        }
    }
    return index;
}

STATIC VOID OsAddData2Buf(UINT8 *buf, UINT32 dataSize)
{
    UINT32 intSave;
    UINT32 ret;

    TRACE_LOCK(intSave);

    UINT32 desLen = traceBufCtl.bufLen;
    UINT32 writeIndex = traceBufCtl.writeIndex;
    UINT32 readIndex = traceBufCtl.readIndex;
    UINT32 writeRange = writeIndex + dataSize;
    UINT8  *des = traceBufCtl.dataBuf + writeIndex;

    /* update readIndex */
    if ((readIndex > writeIndex) && (writeRange > readIndex)) {
        traceBufCtl.readIndex = OsFindReadFrameHead(readIndex, writeRange - readIndex);
    } else if ((readIndex <= writeIndex) && (writeRange > desLen + readIndex)) {
        traceBufCtl.readIndex = OsFindReadFrameHead(readIndex, writeRange - readIndex - desLen);
    }

    /* copy the data and update writeIndex */
    UINT32 tmpLen = desLen - writeIndex;
    if (tmpLen >= dataSize) {
        ret = (UINT32)memcpy_s(des, tmpLen, buf, dataSize);
        if (ret != 0) {
            goto EXIT;
        }
        traceBufCtl.writeIndex = writeIndex + dataSize;
    } else {
        ret = (UINT32)memcpy_s(des, tmpLen, buf, tmpLen);  /* tmpLen: The length of ringbuf that can be written */
        if (ret != 0) {
            goto EXIT;
        }
        ret = (UINT32)memcpy_s(traceBufCtl.dataBuf, desLen, buf + tmpLen, dataSize - tmpLen);
        if (ret != 0) {
            goto EXIT;
        }
        traceBufCtl.writeIndex = dataSize - tmpLen;
    }

EXIT:
    TRACE_UNLOCK(intSave);
}

VOID LOS_Trace(TraceType traceType, ...)
{
    va_list ap;
    if ((traceType > LOS_TRACE_TYPE_MAX) || (traceType < LOS_TRACE_TYPE_MIN) ||
        (traceFunc[traceType].inputHook == NULL)) {
        return;
    }
    if ((traceBufCtl.onOff == LOS_TRACE_DISABLE) || (traceFunc[traceType].onOff == LOS_TRACE_DISABLE)) {
        return;
    }
    /* Set the trace frame head */
    UINT8 buf[TMP_DATALEN];
    FrameHead *frameHead = (FrameHead *)buf;
    frameHead->type = traceType;
    frameHead->cpuID = ArchCurrCpuid();
    frameHead->taskID = LOS_CurTaskIDGet();
    frameHead->timestamp = HalClockGetCycles();

#ifdef LOSCFG_TRACE_LR
    /* Get the linkreg from stack fp and storage to frameHead */
    LOS_RecordLR(frameHead->linkReg, LOSCFG_TRACE_LR_RECORD, LOSCFG_TRACE_LR_RECORD, LOSCFG_TRACE_LR_IGNOR);
#endif

    /* Get the trace message */
    va_start(ap, traceType);
    INT32 dataSize = (traceFunc[traceType].inputHook)(buf + sizeof(FrameHead), TMP_DATALEN - sizeof(FrameHead), ap);
    va_end(ap);
    if (dataSize <= 0) {
        return;
    }
    frameHead->frameSize = sizeof(FrameHead) + dataSize;
    OsAddData2Buf(buf, frameHead->frameSize);
}

UINT8 *LOS_TraceBufDataGet(UINT32 *desLen, UINT32 *relLen)
{
    UINT32 traceSwitch = traceBufCtl.onOff;

    if (desLen == NULL || relLen == NULL) {
        return NULL;
    }

    if (traceSwitch != LOS_TRACE_DISABLE) {
        LOS_TraceSwitch(LOS_TRACE_DISABLE);
    }

    UINT32 writeIndex = traceBufCtl.writeIndex;
    UINT32 readIndex = traceBufCtl.readIndex;
    UINT32 srcLen = traceBufCtl.bufLen;
    UINT8 *des = (UINT8 *)malloc(srcLen * sizeof(UINT8));
    if (des == NULL) {
        *desLen = 0;
        *relLen = 0;
        if (traceSwitch != LOS_TRACE_DISABLE) {
            LOS_TraceSwitch(LOS_TRACE_DISABLE);
        }
        return NULL;
    }
    *desLen = LOS_TRACE_BUFFER_SIZE;
    if (EOK != memset_s(des, srcLen * sizeof(UINT8), 0, LOS_TRACE_BUFFER_SIZE)) {
        *desLen = 0;
        *relLen = 0;
        free(des);
        return NULL;
    }
    if (writeIndex > readIndex) {
        *relLen = readIndex - writeIndex;
        (VOID)memcpy_s(des, *desLen, &(traceBufArray[readIndex]), *relLen);
    } else {
        UINT32 sumLen = srcLen - readIndex;
        (VOID)memcpy_s(des, *desLen, &(traceBufArray[readIndex]), sumLen);
        (VOID)memcpy_s(&(des[sumLen]), *desLen - sumLen, traceBufArray, writeIndex);
        *relLen = sumLen + writeIndex;
    }

    if (traceSwitch != LOS_TRACE_DISABLE) {
        LOS_TraceSwitch(LOS_TRACE_ENABLE);
    }

    return des;
}

#ifdef LOSCFG_FS_VFS
INT32 LOS_Trace2File(const CHAR *filename)
{
    INT32 ret;
    CHAR *fullpath = NULL;
    CHAR *shellWorkingDirectory = OsShellGetWorkingDirtectory();
    UINT32 traceSwitch = traceBufCtl.onOff;

    ret = vfs_normalize_path(shellWorkingDirectory, filename, &fullpath);
    if (ret != 0) {
        return -1;
    }

    if (traceSwitch != LOS_TRACE_DISABLE) {
        LOS_TraceSwitch(LOS_TRACE_DISABLE);
    }

    INT32 fd = open(fullpath, O_CREAT | O_RDWR | O_APPEND, 0644); /* 0644:file right */
    if (fd < 0) {
        return -1;
    }

    UINT32 writeIndex = traceBufCtl.writeIndex;
    UINT32 readIndex = traceBufCtl.readIndex;

    if (writeIndex > readIndex) {
        ret = write(fd, &(traceBufArray[readIndex]), writeIndex - readIndex);
    } else {
        ret = write(fd, &(traceBufArray[readIndex]), traceBufCtl.bufLen - readIndex);
        ret += write(fd, traceBufArray, writeIndex);
    }

    (VOID)close(fd);

    free(fullpath);

    if (traceSwitch != LOS_TRACE_DISABLE) {
        LOS_TraceSwitch(LOS_TRACE_ENABLE);
    }
    return ret;
}
#endif

#ifdef LOSCFG_SHELL
UINT32 OsShellCmdTraceNumSwitch(TraceType traceType, const CHAR *onOff)
{
    UINT32 ret = LOS_NOK;

    if (strcmp("on", onOff) == 0) {
        ret = LOS_TraceTypeSwitch(traceType, LOS_TRACE_ENABLE);
        if (ret == LOS_OK) {
            PRINTK("trace %s on\n", traceFunc[traceType].typeStr);
        } else {
            PRINTK("trace %d is unregistered\n", traceType);
        }
    } else if (strcmp("off", onOff) == 0) {
        ret = LOS_TraceTypeSwitch(traceType, LOS_TRACE_DISABLE);
        if (ret == LOS_OK) {
            PRINTK("trace %s off\n", traceFunc[traceType].typeStr);
        } else {
            PRINTK("trace %d is unregistered\n", traceType);
        }
    } else {
        PRINTK("Unknown option: %s\n", onOff);
    }

    return ret;
}

UINT32 OsShellCmdTraceStrSwitch(const CHAR *typeStr, const CHAR *onOff)
{
    UINT32 ret = LOS_NOK;
    UINT32 i;
    for (i = 0; i <= LOS_TRACE_TYPE_MAX; i++) {
        if (traceFunc[i].typeStr != NULL && !strcmp(typeStr, traceFunc[i].typeStr)) {
            ret = OsShellCmdTraceNumSwitch(i, onOff);
            if (ret != LOS_OK) {
                PRINTK("Unknown option: %s\n", onOff);
            }
            return ret;
        }
    }
    PRINTK("Unknown option: %s\n", typeStr);
    return ret;
}

UINT32 OsShellCmdTraceSwitch(INT32 argc, const CHAR **argv)
{
    UINT32 ret;
    if (argc == 1) {
        if (strcmp("on", argv[0]) == 0) {
            LOS_TraceSwitch(LOS_TRACE_ENABLE);
            PRINTK("trace on\n");
        } else if (strcmp("off", argv[0]) == 0) {
            LOS_TraceSwitch(LOS_TRACE_DISABLE);
            PRINTK("trace off\n");
        } else {
            PRINTK("Unknown option: %s\n", argv[0]);
            goto TRACE_HELP;
        }
    } else if (argc == 2) { /* 2:argc number limited */
        if (isdigit(argv[0][0]) != 0) {
            CHAR *endPtr = NULL;
            UINT32 traceType = strtoul(argv[0], &endPtr, 0);
            if ((endPtr != NULL) || (*endPtr != 0)) {
                PRINTK("Unknown option: %s\n", argv[0]);
                goto TRACE_HELP;
            }
            ret = OsShellCmdTraceNumSwitch(traceType, argv[1]);
            if (ret != LOS_OK) {
                goto TRACE_HELP;
            }
        } else {
            ret = OsShellCmdTraceStrSwitch(argv[0], argv[1]);
            if (ret != LOS_OK) {
                goto TRACE_HELP;
            }
        }
    } else {
        PRINTK("Argc is Incorrect!\n");
        goto TRACE_HELP;
    }
    return LOS_OK;
TRACE_HELP:
    PRINTK("Usage:trace [typeNum/typeName] on/off\n");
    PRINTK("      typeNum range: [%d,%d]\n", LOS_TRACE_TYPE_MIN, LOS_TRACE_TYPE_MAX);
    return LOS_NOK;
}

#ifdef LOSCFG_FS_VFS
UINT32 OsShellCmdTrace2File(INT32 argc, const CHAR **argv)
{
    INT32 ret;
    if (argc == 1) {
        ret = LOS_Trace2File(argv[0]);
        if (ret == -1) {
            PRINTK("Trace to file failed: %s\n", argv[0]);
        } else {
            PRINTK("Trace to file successed: %s\n", argv[0]);
        }
    } else {
        PRINTK("Trace to file:wrong argc\n");
        goto TRACE_HELP;
    }
    return LOS_OK;

TRACE_HELP:
    PRINTK("usage:trace2file filename\n");
    return LOS_NOK;
}

SHELLCMD_ENTRY(trace2file_shellcmd, CMD_TYPE_EX, "trace2file", 1, (CmdCallBackFunc)OsShellCmdTrace2File);
#endif

SHELLCMD_ENTRY(trace_shellcmd, CMD_TYPE_EX, "trace", 1, (CmdCallBackFunc)OsShellCmdTraceSwitch);
#endif

#endif

