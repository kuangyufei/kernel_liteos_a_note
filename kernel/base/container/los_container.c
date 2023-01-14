/*
 * Copyright (c) 2023-2023 Huawei Device Co., Ltd. All rights reserved.
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
#include "los_container_pri.h"
#include "los_process_pri.h"
#ifdef LOSCFG_KERNEL_CONTAINER

STATIC Container g_rootContainer;

VOID OsContainerInitSystemProcess(LosProcessCB *processCB)
{
    processCB->container = &g_rootContainer;
    LOS_AtomicInc(&processCB->container->rc);
#ifdef LOSCFG_PID_CONTAINER
    (VOID)OsAllocSpecifiedVpidUnsafe(processCB->processID, processCB, NULL);
#endif
    return;
}

VOID OsInitRootContainer(VOID)
{
#ifdef LOSCFG_PID_CONTAINER
    (VOID)OsInitRootPidContainer(&g_rootContainer.pidContainer);
#endif
#ifdef LOSCFG_UTS_CONTAINER
    (VOID)OsInitRootUtsContainer(&g_rootContainer.utsContainer);
#endif
    return;
}

STATIC INLINE Container *CreateContainer(VOID)
{
    Container *container = LOS_MemAlloc(m_aucSysMem1, sizeof(Container));
    if (container == NULL) {
        return NULL;
    }

    (VOID)memset_s(container, sizeof(Container), 0, sizeof(Container));

    LOS_AtomicInc(&container->rc);
    return container;
}

/*
 * called from clone.  This now handles copy for Container and all
 * namespaces therein.
 */
UINT32 OsCopyContainers(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent, UINT32 *processID)
{
    UINT32 intSave;
    UINT32 ret = LOS_OK;

    if (!(flags & (CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWPID | CLONE_NEWNET))) {
        SCHEDULER_LOCK(intSave);
        child->container = parent->container;
        LOS_AtomicInc(&child->container->rc);
        SCHEDULER_UNLOCK(intSave);
    } else {
        child->container = CreateContainer();
        if (child->container == NULL) {
            return ENOMEM;
        }
    }

    /* Pid container initialization must precede other container initialization. */
#ifdef LOSCFG_PID_CONTAINER
    ret = OsCopyPidContainer(flags, child, parent, processID);
    if (ret != LOS_OK) {
        return ret;
    }
#endif
#ifdef LOSCFG_UTS_CONTAINER
    ret = OsCopyUtsContainer(flags, child, parent);
    if (ret != LOS_OK) {
        return ret;
    }
#endif
    return ret;
}

VOID OsContainersDestroy(LosProcessCB *processCB)
{
   /* All processes in the container must be destroyed before the container is destroyed. */
#ifdef LOSCFG_PID_CONTAINER
    if (processCB->processID == 1) {
        OsPidContainersDestroyAllProcess(processCB);
    }
#endif

#ifdef LOSCFG_UTS_CONTAINER
    OsUtsContainersDestroy(processCB);
#endif

#ifndef LOSCFG_PID_CONTAINER
    LOS_AtomicDec(&curr->container->rc);
    if (LOS_AtomicRead(&processCB->container->rc) == 1) {
        (VOID)LOS_MemFree(m_aucSysMem1, processCB->container);
        processCB->container = NULL;
    }
#endif
}
#endif
