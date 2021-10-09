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

#include "los_perf_pri.h"
#include "perf_pmu_pri.h"
#include "perf_output_pri.h"
#include "los_init.h"
#include "los_process.h"
#include "los_tick.h"
#include "los_sys.h"
#include "los_spinlock.h"

STATIC Pmu *g_pmu = NULL;
STATIC PerfCB g_perfCb = {0};

LITE_OS_SEC_BSS SPIN_LOCK_INIT(g_perfSpin);
#define PERF_LOCK(state)       LOS_SpinLockSave(&g_perfSpin, &(state))
#define PERF_UNLOCK(state)     LOS_SpinUnlockRestore(&g_perfSpin, (state))

#define MIN(x, y)             ((x) < (y) ? (x) : (y))

STATIC INLINE UINT64 OsPerfGetCurrTime(VOID)
{
#ifdef LOSCFG_PERF_CALC_TIME_BY_TICK
    return LOS_TickCountGet();
#else
    return HalClockGetCycles();
#endif
}

STATIC UINT32 OsPmuInit(VOID)
{
#ifdef LOSCFG_PERF_HW_PMU
    if (OsHwPmuInit() != LOS_OK) {
        return LOS_ERRNO_PERF_HW_INIT_ERROR;
    }
#endif

#ifdef LOSCFG_PERF_TIMED_PMU
    if (OsTimedPmuInit() != LOS_OK) {
        return LOS_ERRNO_PERF_TIMED_INIT_ERROR;
    }
#endif

#ifdef LOSCFG_PERF_SW_PMU
    if (OsSwPmuInit() != LOS_OK) {
        return LOS_ERRNO_PERF_SW_INIT_ERROR;
    }
#endif
    return LOS_OK;
}

STATIC UINT32 OsPerfConfig(PerfEventConfig *eventsCfg)
{
    UINT32 i;
    UINT32 ret;

    g_pmu = OsPerfPmuGet(eventsCfg->type);
    if (g_pmu == NULL) {
        PRINT_ERR("perf config type error %u!\n", eventsCfg->type);
        return LOS_ERRNO_PERF_INVALID_PMU;
    }

    UINT32 eventNum = MIN(eventsCfg->eventsNr, PERF_MAX_EVENT);

    (VOID)memset_s(&g_pmu->events, sizeof(PerfEvent), 0, sizeof(PerfEvent));

    for (i = 0; i < eventNum; i++) {
        g_pmu->events.per[i].eventId = eventsCfg->events[i].eventId;
        g_pmu->events.per[i].period = eventsCfg->events[i].period;
    }
    g_pmu->events.nr = i;
    g_pmu->events.cntDivided = eventsCfg->predivided;
    g_pmu->type = eventsCfg->type;

    ret = g_pmu->config();
    if (ret != LOS_OK) {
        PRINT_ERR("perf config failed!\n");
        (VOID)memset_s(&g_pmu->events, sizeof(PerfEvent), 0, sizeof(PerfEvent));
        return LOS_ERRNO_PERF_PMU_CONFIG_ERROR;
    }
    return LOS_OK;
}

STATIC VOID OsPerfPrintCount(VOID)
{
    UINT32 index;
    UINT32 intSave;
    UINT32 cpuid = ArchCurrCpuid();

    PerfEvent *events = &g_pmu->events;
    UINT32 eventNum = events->nr;

    PERF_LOCK(intSave);
    for (index = 0; index < eventNum; index++) {
        Event *event = &(events->per[index]);

        /* filter out event counter with no event binded. */
        if (event->period == 0) {
            continue;
        }
        PRINT_EMG("[%s] eventType: 0x%x [core %u]: %llu\n", g_pmu->getName(event), event->eventId, cpuid,
            event->count[cpuid]);
    }
    PERF_UNLOCK(intSave);
}

STATIC INLINE VOID OsPerfPrintTs(VOID)
{
#ifdef LOSCFG_PERF_CALC_TIME_BY_TICK
    DOUBLE time = (g_perfCb.endTime - g_perfCb.startTime) * 1.0 / LOSCFG_BASE_CORE_TICK_PER_SECOND;
#else
    DOUBLE time = (g_perfCb.endTime - g_perfCb.startTime) * 1.0 / OS_SYS_CLOCK;
#endif
    PRINT_EMG("time used: %.6f(s)\n", time);
}

STATIC VOID OsPerfStart(VOID)
{
    UINT32 cpuid = ArchCurrCpuid();

    if (g_pmu == NULL) {
        PRINT_ERR("pmu not registered!\n");
        return;
    }

    if (g_perfCb.pmuStatusPerCpu[cpuid] != PERF_PMU_STARTED) {
        UINT32 ret = g_pmu->start();
        if (ret != LOS_OK) {
            PRINT_ERR("perf start on core:%u failed, ret = 0x%x\n", cpuid, ret);
            return;
        }

        g_perfCb.pmuStatusPerCpu[cpuid] = PERF_PMU_STARTED;
    } else {
        PRINT_ERR("percpu status err %d\n", g_perfCb.pmuStatusPerCpu[cpuid]);
    }
}

STATIC VOID OsPerfStop(VOID)
{
    UINT32 cpuid = ArchCurrCpuid();

    if (g_pmu == NULL) {
        PRINT_ERR("pmu not registered!\n");
        return;
    }

    if (g_perfCb.pmuStatusPerCpu[cpuid] != PERF_PMU_STOPED) {
        UINT32 ret = g_pmu->stop();
        if (ret != LOS_OK) {
            PRINT_ERR("perf stop on core:%u failed, ret = 0x%x\n", cpuid, ret);
            return;
        }

        if (!g_perfCb.needSample) {
            OsPerfPrintCount();
        }

        g_perfCb.pmuStatusPerCpu[cpuid] = PERF_PMU_STOPED;
    } else {
        PRINT_ERR("percpu status err %d\n", g_perfCb.pmuStatusPerCpu[cpuid]);
    }
}

STATIC INLINE UINT32 OsPerfSaveIpInfo(CHAR *buf, IpInfo *info)
{
    UINT32 size = 0;
#ifdef LOSCFG_KERNEL_VM
    UINT32 len = ALIGN(info->len, sizeof(size_t));

    *(UINTPTR *)buf = info->ip; /* save ip */
    size += sizeof(UINTPTR);

    *(UINT32 *)(buf + size) = len; /* save f_path length */
    size += sizeof(UINT32);

    if (strncpy_s(buf + size, REGION_PATH_MAX, info->f_path, info->len) != EOK) { /* save f_path */
        PRINT_ERR("copy f_path failed, %s\n", info->f_path);
    }
    size += len;
#else
    *(UINTPTR *)buf = info->ip; /* save ip */
    size += sizeof(UINTPTR);
#endif
    return size;
}

STATIC UINT32 OsPerfBackTrace(PerfBackTrace *callChain, UINT32 maxDepth, PerfRegs *regs)
{
    UINT32 count = BackTraceGet(regs->fp, (IpInfo *)(callChain->ip), maxDepth);
    PRINT_DEBUG("backtrace depth = %u, fp = 0x%x\n", count, regs->fp);
    return count;
}

STATIC INLINE UINT32 OsPerfSaveBackTrace(CHAR *buf, PerfBackTrace *callChain, UINT32 count)
{
    UINT32 i;
    *(UINT32 *)buf = count;
    UINT32 size = sizeof(UINT32);
    for (i = 0; i < count; i++) {
        size += OsPerfSaveIpInfo(buf + size, &(callChain->ip[i]));
    }
    return size;
}

STATIC UINT32 OsPerfCollectData(Event *event, PerfSampleData *data, PerfRegs *regs)
{
    UINT32 size = 0;
    UINT32 depth;
    IpInfo pc = {0};
    PerfBackTrace callChain = {0};
    UINT32 sampleType = g_perfCb.sampleType;
    CHAR *p = (CHAR *)data;

    if (sampleType & PERF_RECORD_CPU) {
        *(UINT32 *)(p + size) = ArchCurrCpuid();
        size += sizeof(data->cpuid);
    }

    if (sampleType & PERF_RECORD_TID) {
        *(UINT32 *)(p + size) = LOS_CurTaskIDGet();
        size += sizeof(data->taskId);
    }

    if (sampleType & PERF_RECORD_PID) {
        *(UINT32 *)(p + size) = LOS_GetCurrProcessID();
        size += sizeof(data->processId);
    }

    if (sampleType & PERF_RECORD_TYPE) {
        *(UINT32 *)(p + size) = event->eventId;
        size += sizeof(data->eventId);
    }

    if (sampleType & PERF_RECORD_PERIOD) {
        *(UINT32 *)(p + size) = event->period;
        size += sizeof(data->period);
    }

    if (sampleType & PERF_RECORD_TIMESTAMP) {
        *(UINT64 *)(p + size) = OsPerfGetCurrTime();
        size += sizeof(data->time);
    }

    if (sampleType & PERF_RECORD_IP) {
        OsGetUsrIpInfo(regs->pc, &pc);
        size += OsPerfSaveIpInfo(p + size, &pc);
    }

    if (sampleType & PERF_RECORD_CALLCHAIN) {
        depth = OsPerfBackTrace(&callChain, PERF_MAX_CALLCHAIN_DEPTH, regs);
        size += OsPerfSaveBackTrace(p + size, &callChain, depth);
    }

    return size;
}

/*
 * return TRUE if the taskId in the task filter list, return FALSE otherwise;
 * return TRUE if user haven't specified any taskId(which is supposed
 * to instrument the whole system)
 */
STATIC INLINE BOOL OsFilterId(UINT32 id, UINT32 *ids, UINT8 idsNr)
{
    UINT32 i;
    if (!idsNr) {
        return TRUE;
    }

    for (i = 0; i < idsNr; i++) {
        if (ids[i] == id) {
            return TRUE;
        }
    }
    return FALSE;
}

STATIC INLINE BOOL OsPerfFilter(UINT32 taskId, UINT32 processId)
{
    return OsFilterId(taskId, g_perfCb.taskIds, g_perfCb.taskIdsNr) &&
            OsFilterId(processId, g_perfCb.processIds, g_perfCb.processIdsNr);
}

STATIC INLINE UINT32 OsPerfParamValid(VOID)
{
    UINT32 index;
    UINT32 res = 0;

    if (g_pmu == NULL) {
        return 0;
    }
    PerfEvent *events = &g_pmu->events;
    UINT32 eventNum = events->nr;

    for (index = 0; index < eventNum; index++) {
        res |= events->per[index].period;
    }
    return res;
}

STATIC UINT32 OsPerfHdrInit(UINT32 id)
{
    PerfDataHdr head = {
        .magic      = PERF_DATA_MAGIC_WORD,
        .sampleType = g_perfCb.sampleType,
        .sectionId  = id,
        .eventType  = g_pmu->type,
        .len        = sizeof(PerfDataHdr),
    };
    return OsPerfOutputWrite((CHAR *)&head, head.len);
}

VOID OsPerfUpdateEventCount(Event *event, UINT32 value)
{
    if (event == NULL) {
        return;
    }
    event->count[ArchCurrCpuid()] += (value & 0xFFFFFFFF); /* event->count is UINT64 */
}

VOID OsPerfHandleOverFlow(Event *event, PerfRegs *regs)
{
    PerfSampleData data;
    UINT32 len;

    (VOID)memset_s(&data, sizeof(PerfSampleData), 0, sizeof(PerfSampleData));
    if ((g_perfCb.needSample) && OsPerfFilter(LOS_CurTaskIDGet(), LOS_GetCurrProcessID())) {
        len = OsPerfCollectData(event, &data, regs);
        OsPerfOutputWrite((CHAR *)&data, len);
    }
}

STATIC UINT32 OsPerfInit(VOID)
{
    UINT32 ret;
    if (g_perfCb.status != PERF_UNINIT) {
        ret = LOS_ERRNO_PERF_STATUS_INVALID;
        goto PERF_INIT_ERROR;
    }

    ret = OsPmuInit();
    if (ret != LOS_OK) {
        goto PERF_INIT_ERROR;
    }

    ret = OsPerfOutputInit(NULL, LOSCFG_PERF_BUFFER_SIZE);
    if (ret != LOS_OK) {
        ret = LOS_ERRNO_PERF_BUF_ERROR;
        goto PERF_INIT_ERROR;
    }
    g_perfCb.status = PERF_STOPPED;
PERF_INIT_ERROR:
    return ret;
}

STATIC VOID PerfInfoDump(VOID)
{
    UINT32 i;
    if (g_pmu != NULL) {
        PRINTK("type: %d\n", g_pmu->type);
        for (i = 0; i < g_pmu->events.nr; i++) {
            PRINTK("events[%d]: %d, 0x%x\n", i, g_pmu->events.per[i].eventId, g_pmu->events.per[i].period);
        }
        PRINTK("predivided: %d\n", g_pmu->events.cntDivided);
    } else {
        PRINTK("pmu is NULL\n");
    }

    PRINTK("sampleType: 0x%x\n", g_perfCb.sampleType);
    for (i = 0; i < g_perfCb.taskIdsNr; i++) {
        PRINTK("filter taskIds[%d]: %d\n", i, g_perfCb.taskIds[i]);
    }
    for (i = 0; i < g_perfCb.processIdsNr; i++) {
        PRINTK("filter processIds[%d]: %d\n", i, g_perfCb.processIds[i]);
    }
    PRINTK("needSample: %d\n", g_perfCb.needSample);
}

STATIC INLINE VOID OsPerfSetFilterIds(UINT32 *dstIds, UINT8 *dstIdsNr, UINT32 *ids, UINT32 idsNr)
{
    errno_t ret;
    if (idsNr) {
        ret = memcpy_s(dstIds, PERF_MAX_FILTER_TSKS * sizeof(UINT32), ids, idsNr * sizeof(UINT32));
        if (ret != EOK) {
            PRINT_ERR("In %s At line:%d execute memcpy_s error\n", __FUNCTION__, __LINE__);
            *dstIdsNr = 0;
            return;
        }
        *dstIdsNr = MIN(idsNr, PERF_MAX_FILTER_TSKS);
    } else {
        *dstIdsNr = 0;
    }
}

UINT32 LOS_PerfConfig(PerfConfigAttr *attr)
{
    UINT32 ret;
    UINT32 intSave;

    if (attr == NULL) {
        return LOS_ERRNO_PERF_CONFIG_NULL;
    }

    PERF_LOCK(intSave);
    if (g_perfCb.status != PERF_STOPPED) {
        ret = LOS_ERRNO_PERF_STATUS_INVALID;
        PRINT_ERR("perf config status error : 0x%x\n", g_perfCb.status);
        goto PERF_CONFIG_ERROR;
    }

    g_pmu = NULL;

    g_perfCb.needSample = attr->needSample;
    g_perfCb.sampleType = attr->sampleType;

    OsPerfSetFilterIds(g_perfCb.taskIds, &g_perfCb.taskIdsNr, attr->taskIds, attr->taskIdsNr);
    OsPerfSetFilterIds(g_perfCb.processIds, &g_perfCb.processIdsNr, attr->processIds, attr->processIdsNr);

    ret = OsPerfConfig(&attr->eventsCfg);
    PerfInfoDump();
PERF_CONFIG_ERROR:
    PERF_UNLOCK(intSave);
    return ret;
}

VOID LOS_PerfStart(UINT32 sectionId)
{
    UINT32 intSave;
    UINT32 ret;

    PERF_LOCK(intSave);
    if (g_perfCb.status != PERF_STOPPED) {
        PRINT_ERR("perf start status error : 0x%x\n", g_perfCb.status);
        goto PERF_START_ERROR;
    }

    if (!OsPerfParamValid()) {
        PRINT_ERR("forgot call `LOS_PerfConfig(...)` before perf start?\n");
        goto PERF_START_ERROR;
    }

    if (g_perfCb.needSample) {
        ret = OsPerfHdrInit(sectionId); /* section header init */
        if (ret != LOS_OK) {
            PRINT_ERR("perf hdr init error 0x%x\n", ret);
            goto PERF_START_ERROR;
        }
    }

    SMP_CALL_PERF_FUNC(OsPerfStart); /* send to all cpu to start pmu */
    g_perfCb.status = PERF_STARTED;
    g_perfCb.startTime = OsPerfGetCurrTime();
PERF_START_ERROR:
    PERF_UNLOCK(intSave);
    return;
}

VOID LOS_PerfStop(VOID)
{
    UINT32 intSave;

    PERF_LOCK(intSave);
    if (g_perfCb.status != PERF_STARTED) {
        PRINT_ERR("perf stop status error : 0x%x\n", g_perfCb.status);
        goto PERF_STOP_ERROR;
    }

    SMP_CALL_PERF_FUNC(OsPerfStop); /* send to all cpu to stop pmu */

    OsPerfOutputFlush();

    if (g_perfCb.needSample) {
        OsPerfOutputInfo();
    }

    g_perfCb.status = PERF_STOPPED;
    g_perfCb.endTime = OsPerfGetCurrTime();

    OsPerfPrintTs();
PERF_STOP_ERROR:
    PERF_UNLOCK(intSave);
    return;
}

UINT32 LOS_PerfDataRead(CHAR *dest, UINT32 size)
{
    return OsPerfOutputRead(dest, size);
}

VOID LOS_PerfNotifyHookReg(const PERF_BUF_NOTIFY_HOOK func)
{
    UINT32 intSave;

    PERF_LOCK(intSave);
    OsPerfNotifyHookReg(func);
    PERF_UNLOCK(intSave);
}

VOID LOS_PerfFlushHookReg(const PERF_BUF_FLUSH_HOOK func)
{
    UINT32 intSave;

    PERF_LOCK(intSave);
    OsPerfFlushHookReg(func);
    PERF_UNLOCK(intSave);
}

VOID OsPerfSetIrqRegs(UINTPTR pc, UINTPTR fp)
{
    LosTaskCB *runTask = (LosTaskCB *)ArchCurrTaskGet();
    runTask->pc = pc;
    runTask->fp = fp;
}

LOS_MODULE_INIT(OsPerfInit, LOS_INIT_LEVEL_KMOD_EXTENDED);
