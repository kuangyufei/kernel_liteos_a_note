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

#include "los_init_pri.h"
#include "los_atomic.h"
#include "los_config.h"
#include "los_hw.h"
#include "los_printf.h"
#include "los_spinlock.h"
#include "los_typedef.h"

#ifdef LOS_INIT_DEBUG
#include "los_sys_pri.h"
#include "los_tick.h"
#endif

/**
 * Register kernel init level labels.
 */
OS_INIT_LEVEL_REG(kernel, 10, g_kernInitLevelList);

STATIC volatile UINT32 g_initCurrentLevel = OS_INVALID_VALUE;
STATIC volatile struct ModuleInitInfo *g_initCurrentModule = 0;
STATIC Atomic g_initCount = 0;
STATIC SPIN_LOCK_INIT(g_initLock);

/**
 * It is recommended that each startup framework encapsulate a layer of its own calling interface.
 */
STATIC VOID InitLevelCall(const CHAR *name, const UINT32 level, struct ModuleInitInfo *initLevelList[])
{
    struct ModuleInitInfo *module = NULL;
#ifdef LOS_INIT_DEBUG
    UINT64 startNsec, endNsec;
    UINT64 totalTime = 0;
    UINT64 singleTime = 0;
    UINT32 ret;
#endif

    if (ArchCurrCpuid() == 0) {
#ifdef LOS_INIT_DEBUG
        PRINTK("-------- %s Module Init... level = %u --------\n", name, level);
#endif
        g_initCurrentLevel = level;
        g_initCurrentModule = initLevelList[level];
   } else {
        while (g_initCurrentLevel != level) {
        }
    }

    do {
        LOS_SpinLock(&g_initLock);
        if (g_initCurrentModule >= initLevelList[level + 1]) {
            LOS_SpinUnlock(&g_initLock);
            break;
        }
        module = (struct ModuleInitInfo *)g_initCurrentModule;
        g_initCurrentModule++;
        LOS_SpinUnlock(&g_initLock);
        if (module->hook != NULL) {
#ifdef LOS_INIT_DEBUG
            ret = LOS_OK;
            startNsec = LOS_CurrNanosec();
            ret = (UINT32)module->hook();
            endNsec = LOS_CurrNanosec();
            singleTime = endNsec - startNsec;
            totalTime += singleTime;
            PRINTK("Starting %s module consumes %llu ns. Run on cpu %u\n", module->name, singleTime, ArchCurrCpuid());
#else
            module->hook();
#endif
        }
#ifdef LOS_INIT_DEBUG
        if (ret != LOS_OK) {
            PRINT_ERR("%s initialization failed at module %s, function addr at 0x%x, ret code is %u\n",
                      name, module->name, module->hook, ret);
        }
#endif
    } while (1);

    if (level >= LOS_INIT_LEVEL_VM_COMPLETE) {
        LOS_AtomicInc(&g_initCount);
        while ((LOS_AtomicRead(&g_initCount) % LOSCFG_KERNEL_CORE_NUM) != 0) {
        }
    }

#ifdef LOS_INIT_DEBUG
    PRINTK("%s initialization at level %u consumes %lluns on cpu %u.\n", name, level, totalTime, ArchCurrCpuid());
#endif
}

VOID OsInitCall(const UINT32 level)
{
    if (level >= LOS_INIT_LEVEL_FINISH) {
        return;
    }

    InitLevelCall("Kernel", level, g_kernInitLevelList);
}
