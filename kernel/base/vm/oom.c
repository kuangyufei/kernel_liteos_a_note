/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
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

#include "sys_config.h"
#include "los_oom.h"
#include "los_vm_dump.h"
#include "los_vm_lock.h"
#include "los_vm_phys.h"
#include "los_vm_filemap.h"
#include "los_process_pri.h"
#if (LOSCFG_BASE_CORE_SWTMR == YES)
#include "los_swtmr_pri.h"
#endif

#ifdef LOSCFG_FS_VFS
#include "console.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

LITE_OS_SEC_BSS OomCB *g_oomCB = NULL; //全局内存溢出控制块
static SPIN_LOCK_INIT(g_oomSpinLock);//内存溢出自旋锁

LITE_OS_SEC_TEXT_MINOR STATIC UINT32 OomScoreProcess(LosProcessCB *candidateProcess)
{
    UINT32 actualPm;

#if (LOSCFG_KERNEL_SMP != YES)
    (VOID)LOS_MuxAcquire(&candidateProcess->vmSpace->regionMux);
#endif
    /* we only consider actual physical memory here. */ //只考虑实际的物理内存
    OsUProcessPmUsage(candidateProcess->vmSpace, NULL, &actualPm);
#if (LOSCFG_KERNEL_SMP != YES)
    (VOID)LOS_MuxRelease(&candidateProcess->vmSpace->regionMux);
#endif
    return actualPm;
}
//内存不足时,进程
LITE_OS_SEC_TEXT_MINOR STATIC UINT32 OomKillProcess(UINTPTR param)
{
    /* we will not kill process, and do nothing here */
    return LOS_OK;
}

LITE_OS_SEC_TEXT_MINOR STATIC UINT32 OomForceShrinkMemory(VOID)
{
    UINT32 i;
    UINT32 reclaimMemPages = 0;

    /*
     * TryShrinkMemory maybe reclaim 0 pages in the first time from active list
     * to inactive list, and in the second time reclaim memory from inactive list.
     */
    for (i = 0; i < MAX_SHRINK_PAGECACHE_TRY; i++) {
        reclaimMemPages += OsTryShrinkMemory(0);
    }

    return reclaimMemPages;
}

LITE_OS_SEC_TEXT_MINOR STATIC BOOL OomReclaimPageCache(VOID)
{
    UINT32 totalPm = 0;
    UINT32 usedPm = 0;
    BOOL isReclaimMemory = FALSE;
    UINT32 reclaimMemPages;
    UINT32 i;

    for (i = 0; i < MAX_SHRINK_PAGECACHE_TRY; i++) {
        OsVmPhysUsedInfoGet(&usedPm, &totalPm);
        isReclaimMemory = ((totalPm - usedPm) << PAGE_SHIFT) < g_oomCB->reclaimMemThreshold;
        if (isReclaimMemory) {
            /*
             * we do force memory reclaim from page cache here.
             * if we get memory, we will reclaim pagecache memory again.
             * if there is no memory to reclaim, we will return.
             */
            reclaimMemPages = OomForceShrinkMemory();
            if (reclaimMemPages > 0) {
                continue;
            }
        }
        break;
    }

    return isReclaimMemory;
}

/*
 * check is low memory or not, if low memory, try to kill process. //检查内存是否不足，如果内存不足，请尝试终止进程。
 * return is kill process or not. //返回是否kill进程
 */
LITE_OS_SEC_TEXT_MINOR BOOL OomCheckProcess(VOID)
{
    UINT32 totalPm;
    UINT32 usedPm;
    BOOL isLowMemory = FALSE;

    /*
     * spinlock the current core schedule, make sure oom process atomic //旋转锁定当前核心计划，确保oom进程原子化
     * spinlock other place entering OomCheckProcess, make sure oom process mutex //旋转锁定其他进入OomCheckProcess的地方，确保oom进程互斥
     */
    LOS_SpinLock(&g_oomSpinLock);

    /* first we will check if we need to reclaim pagecache memory */
    if (OomReclaimPageCache() == FALSE) {
        goto NO_VICTIM_PROCESS;
    }

    /* get free bytes */
    OsVmPhysUsedInfoGet(&usedPm, &totalPm);
    isLowMemory = ((totalPm - usedPm) << PAGE_SHIFT) < g_oomCB->lowMemThreshold;
    if (isLowMemory) {
        PRINTK("[oom] OS is in low memory state\n"
               "total physical memory: %#x(byte), used: %#x(byte),"
               "free: %#x(byte), low memory threshold: %#x(byte)\n",
               totalPm << PAGE_SHIFT, usedPm << PAGE_SHIFT,
               (totalPm - usedPm) << PAGE_SHIFT, g_oomCB->lowMemThreshold);
    }

NO_VICTIM_PROCESS:
    LOS_SpinUnlock(&g_oomSpinLock);
    return isLowMemory;
}

#ifdef LOSCFG_ENABLE_OOM_LOOP_TASK	//内存溢出监测任务开关
STATIC VOID OomWriteEvent(VOID) // OomTaskInit中创建的定时器回调
{
    OsWriteResourceEvent(OS_RESOURCE_EVENT_OOM);//写内存溢出事件
}
#endif

LITE_OS_SEC_TEXT_MINOR VOID OomInfodump(VOID) //打印内存溢出信息
{
    PRINTK("[oom] oom loop task status: %s\n"
           "      oom low memory threshold: %#x(byte)\n"
           "      oom reclaim memory threshold: %#x(byte)\n"
           "      oom check interval: %d(microsecond)\n",
           g_oomCB->enabled ? "enabled" : "disabled",
           g_oomCB->lowMemThreshold, g_oomCB->reclaimMemThreshold,
           g_oomCB->checkInterval);
}

LITE_OS_SEC_TEXT_MINOR VOID OomSetLowMemThreashold(UINT32 lowMemThreshold)
{
    if ((lowMemThreshold > OOM_DEFAULT_LOW_MEM_THRESHOLD_MAX)) {
        PRINTK("[oom] low memory threshold %#x(byte) invalid,"
               "should be in [%#x, %#x](byte)\n",
               lowMemThreshold, OOM_DEFAULT_LOW_MEM_THRESHOLD_MIN,
               OOM_DEFAULT_LOW_MEM_THRESHOLD_MAX);
    } else {
        g_oomCB->lowMemThreshold = lowMemThreshold;
        PRINTK("[oom] set oom low memory threshold %#x(byte) successful\n",
               g_oomCB->lowMemThreshold);
    }
}

LITE_OS_SEC_TEXT_MINOR VOID OomSetReclaimMemThreashold(UINT32 reclaimMemThreshold)
{
    UINT32 totalPm = 0;
    UINT32 usedPm = 0;

    OsVmPhysUsedInfoGet(&usedPm, &totalPm);
    if ((reclaimMemThreshold >= (totalPm << PAGE_SHIFT)) ||
        (reclaimMemThreshold < g_oomCB->lowMemThreshold)) {
        PRINTK("[oom] reclaim memory threshold %#x(byte) invalid,"
               "should be in [%#x, %#x)(byte)\n",
               reclaimMemThreshold, g_oomCB->lowMemThreshold, (totalPm << PAGE_SHIFT));
    } else {
        g_oomCB->reclaimMemThreshold = reclaimMemThreshold;
        PRINTK("[oom] set oom reclaim memory threshold %#x(byte) successful\n",
               g_oomCB->reclaimMemThreshold);
    }
}

LITE_OS_SEC_TEXT_MINOR VOID OomSetCheckInterval(UINT32 checkInterval)
{
    if ((checkInterval >= OOM_CHECK_MIN) && (checkInterval <= OOM_CHECK_MAX)) {
        g_oomCB->checkInterval = checkInterval;
        PRINTK("[oom] set oom check interval (%d)ms successful\n",
               g_oomCB->checkInterval);
    } else {
        PRINTK("[oom] set oom check interval (%d)ms failed, should be in [%d, %d]\n",
               g_oomCB->checkInterval, OOM_CHECK_MIN, OOM_CHECK_MAX);
    }
}
//内存溢出任务初始化
LITE_OS_SEC_TEXT_MINOR UINT32 OomTaskInit(VOID)
{
    g_oomCB = (OomCB *)LOS_MemAlloc(m_aucSysMem0, sizeof(OomCB));
    if (g_oomCB == NULL) {
        VM_ERR("oom task init failed, malloc OomCB failed.");
        return LOS_NOK;
    }

    g_oomCB->lowMemThreshold     = OOM_DEFAULT_LOW_MEM_THRESHOLD;
    g_oomCB->reclaimMemThreshold = OOM_DEFAULT_RECLAIM_MEM_THRESHOLD;
    g_oomCB->checkInterval       = OOM_DEFAULT_CHECK_INTERVAL;
    g_oomCB->processVictimCB     = (OomFn)OomKillProcess;
    g_oomCB->scoreCB             = (OomFn)OomScoreProcess;
    g_oomCB->enabled             = FALSE;

#ifdef LOSCFG_ENABLE_OOM_LOOP_TASK //内存溢出检测开关
    g_oomCB->enabled         = TRUE;
    UINT32 ret = LOS_SwtmrCreate(g_oomCB->checkInterval, LOS_SWTMR_MODE_PERIOD, (SWTMR_PROC_FUNC)OomWriteEvent,
                                 &g_oomCB->swtmrID, (UINTPTR)g_oomCB);//创建检测定时器
    if (ret != LOS_OK) {
        return ret;
    }

    return LOS_SwtmrStart(g_oomCB->swtmrID);
#else
    return LOS_OK;
#endif
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
