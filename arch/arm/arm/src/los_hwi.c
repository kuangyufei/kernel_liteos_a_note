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

#include "los_hwi.h"
#include "los_memory.h"
#include "los_tickless_pri.h"
#include "los_spinlock.h"
#ifdef LOSCFG_KERNEL_CPUP
#include "los_cpup_pri.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/* spinlock for hwi module, only available on SMP mode */
LITE_OS_SEC_BSS  SPIN_LOCK_INIT(g_hwiSpin); //注意全局变量 g_hwiSpin 是在宏里面定义的
#define HWI_LOCK(state)       LOS_SpinLockSave(&g_hwiSpin, &(state))
#define HWI_UNLOCK(state)     LOS_SpinUnlockRestore(&g_hwiSpin, (state))

size_t g_intCount[LOSCFG_KERNEL_CORE_NUM] = {0};//记录每个CPU core的中断数量 
HwiHandleForm g_hwiForm[OS_HWI_MAX_NUM];		//记录每个硬件中断实体内容            @note_what 表用 form 来表示？有种写 HTML的感觉
STATIC CHAR *g_hwiFormName[OS_HWI_MAX_NUM] = {0};//记录每个硬中断的名称 OS_HWI_MAX_NUM = 128 定义于 hi3516dv300
STATIC UINT32 g_hwiFormCnt[OS_HWI_MAX_NUM] = {0};//记录每个硬中断的总数量

VOID OsIncHwiFormCnt(UINT32 index)	//增加一个中断数,递增的，所以只有++ ,没有--，
{
    g_hwiFormCnt[index]++;
}

UINT32 OsGetHwiFormCnt(UINT32 index)//获取某个中断的中断次数
{
    return g_hwiFormCnt[index];
}

CHAR *OsGetHwiFormName(UINT32 index)//获取某个中断的名称
{
    return g_hwiFormName[index];
}

typedef VOID (*HWI_PROC_FUNC0)(VOID);
typedef VOID (*HWI_PROC_FUNC2)(INT32, VOID *);
VOID OsInterrupt(UINT32 intNum)//中断实际处理函数
{
    HwiHandleForm *hwiForm = NULL;
    UINT32 *intCnt = NULL;

    intCnt = &g_intCount[ArchCurrCpuid()];//当前CPU的中断总数量 ++
    *intCnt = *intCnt + 1;

#ifdef LOSCFG_CPUP_INCLUDE_IRQ //开启查询系统CPU的占用率的中断
    OsCpupIrqStart();
#endif

#ifdef LOSCFG_KERNEL_TICKLESS
    OsTicklessUpdate(intNum);
#endif
    hwiForm = (&g_hwiForm[intNum]);//获取对应中断的实体
#ifndef LOSCFG_NO_SHARED_IRQ	//如果没有定义不共享中断 ，意思就是如果是共享中断
    while (hwiForm->pstNext != NULL) { //一直撸到最后
        hwiForm = hwiForm->pstNext;//下一个继续撸
#endif
        if (hwiForm->uwParam) {//有参数的情况
            HWI_PROC_FUNC2 func = (HWI_PROC_FUNC2)hwiForm->pfnHook;//获取回调函数
            if (func != NULL) {
                UINTPTR *param = (UINTPTR *)(hwiForm->uwParam);
                func((INT32)(*param), (VOID *)(*(param + 1)));//运行带参数的回调函数
            }
        } else {//木有参数的情况
            HWI_PROC_FUNC0 func = (HWI_PROC_FUNC0)hwiForm->pfnHook;//获取回调函数
            if (func != NULL) {
                func();//运行回调函数
            }
        }
#ifndef LOSCFG_NO_SHARED_IRQ
    }
#endif
    ++g_hwiFormCnt[intNum];//中断数量计数器++

    *intCnt = *intCnt - 1;	//@note_why 这里没看明白为什么要 -1 
#ifdef LOSCFG_CPUP_INCLUDE_IRQ	//开启查询系统CPU的占用率的中断
    OsCpupIrqEnd(intNum);
#endif
}
//copy 硬中断参数
STATIC HWI_ARG_T OsHwiCpIrqParam(const HwiIrqParam *irqParam)
{
    HwiIrqParam *paramByAlloc = NULL;

    if (irqParam != NULL) {
        paramByAlloc = (HwiIrqParam *)LOS_MemAlloc(m_aucSysMem0, sizeof(HwiIrqParam));
        if (paramByAlloc == NULL) {
            return LOS_NOK;
        }
        (VOID)memcpy_s(paramByAlloc, sizeof(HwiIrqParam), irqParam, sizeof(HwiIrqParam));
    }
    /* When "irqParam" is NULL, the function return 0(LOS_OK). */
    return (HWI_ARG_T)paramByAlloc;
}

#ifdef LOSCFG_NO_SHARED_IRQ
STATIC UINT32 OsHwiDelNoShared(HWI_HANDLE_T hwiNum)
{
    UINT32 intSave;

    HWI_LOCK(intSave);
    g_hwiForm[hwiNum].pfnHook = NULL;//回调函数直接NULL
    if (g_hwiForm[hwiNum].uwParam) {//如有参数
        (VOID)LOS_MemFree(m_aucSysMem0, (VOID *)g_hwiForm[hwiNum].uwParam);//释放内存
    }
    g_hwiForm[hwiNum].uwParam = 0; //NULL

    HWI_UNLOCK(intSave);
    return LOS_OK;
}
//创建一个不支持共享的中断
STATIC UINT32 OsHwiCreateNoShared(HWI_HANDLE_T hwiNum, HWI_MODE_T hwiMode,
                                  HWI_PROC_FUNC hwiHandler, const HwiIrqParam *irqParam)
{
    HWI_ARG_T retParam;
    UINT32 intSave;

    HWI_LOCK(intSave);
    if (g_hwiForm[hwiNum].pfnHook == NULL) {
        g_hwiForm[hwiNum].pfnHook = hwiHandler;//记录上回调函数

        retParam = OsHwiCpIrqParam(irqParam);//参数copy一份出来记录
        if (retParam == LOS_NOK) {
            HWI_UNLOCK(intSave);
            return OS_ERRNO_HWI_NO_MEMORY;
        }
        g_hwiForm[hwiNum].uwParam = retParam;//作为硬中断处理函数的参数
    } else {
        HWI_UNLOCK(intSave);
        return OS_ERRNO_HWI_ALREADY_CREATED;
    }
    HWI_UNLOCK(intSave);
    return LOS_OK;
}
#else	//删除一个共享中断
STATIC UINT32 OsHwiDelShared(HWI_HANDLE_T hwiNum, const HwiIrqParam *irqParam)
{
    HwiHandleForm *hwiForm = NULL;
    HwiHandleForm *hwiFormtmp = NULL;
    UINT32 hwiValid = FALSE;
    UINT32 intSave;

    HWI_LOCK(intSave);
    hwiForm = &g_hwiForm[hwiNum];
    hwiFormtmp = hwiForm;

    if ((hwiForm->uwParam & IRQF_SHARED) && ((irqParam == NULL) || (irqParam->pDevId == NULL))) {
        HWI_UNLOCK(intSave);
        return OS_ERRNO_HWI_SHARED_ERROR;
    }

    if ((hwiForm->pstNext != NULL) && !(hwiForm->uwParam & IRQF_SHARED)) {
        hwiForm = hwiForm->pstNext;
        if (hwiForm->uwParam) {
            (VOID)LOS_MemFree(m_aucSysMem0, (VOID *)hwiForm->uwParam);
        }
        (VOID)LOS_MemFree(m_aucSysMem0, hwiForm);
        hwiFormtmp->pstNext = NULL;

        g_hwiFormName[hwiNum] = NULL;

        HWI_UNLOCK(intSave);
        return LOS_OK;
    }
    hwiForm = hwiForm->pstNext;
    while (hwiForm != NULL) {
        if (((HwiIrqParam *)(hwiForm->uwParam))->pDevId != irqParam->pDevId) {
            hwiFormtmp = hwiForm;
            hwiForm = hwiForm->pstNext;
        } else {
            hwiFormtmp->pstNext = hwiForm->pstNext;
            (VOID)LOS_MemFree(m_aucSysMem0, (VOID *)hwiForm->uwParam);
            (VOID)LOS_MemFree(m_aucSysMem0, hwiForm);

            hwiValid = TRUE;
            break;
        }
    }

    if (hwiValid != TRUE) {
        HWI_UNLOCK(intSave);
        return OS_ERRNO_HWI_HWINUM_UNCREATE;
    }

    if (g_hwiForm[hwiNum].pstNext == NULL) {
        g_hwiForm[hwiNum].uwParam = 0;
        g_hwiFormName[hwiNum] = NULL;
    }

    HWI_UNLOCK(intSave);
    return LOS_OK;
}
//创建一个共享硬件中断
STATIC UINT32 OsHwiCreateShared(HWI_HANDLE_T hwiNum, HWI_MODE_T hwiMode,
                                HWI_PROC_FUNC hwiHandler, const HwiIrqParam *irqParam)
{
    UINT32 intSave;
    HwiHandleForm *hwiFormNode = NULL;
    HwiHandleForm *hwiForm = NULL;
    HwiIrqParam *hwiParam = NULL;
    HWI_MODE_T modeResult = hwiMode & IRQF_SHARED;

    if (modeResult && ((irqParam == NULL) || (irqParam->pDevId == NULL))) {
        return OS_ERRNO_HWI_SHARED_ERROR;
    }

    HWI_LOCK(intSave);

    hwiForm = &g_hwiForm[hwiNum];
    if ((hwiForm->pstNext != NULL) && ((modeResult == 0) || (!(hwiForm->uwParam & IRQF_SHARED)))) {
        HWI_UNLOCK(intSave);
        return OS_ERRNO_HWI_SHARED_ERROR;
    }

    while (hwiForm->pstNext != NULL) {
        hwiForm = hwiForm->pstNext;
        hwiParam = (HwiIrqParam *)(hwiForm->uwParam);
        if (hwiParam->pDevId == irqParam->pDevId) {
            HWI_UNLOCK(intSave);
            return OS_ERRNO_HWI_ALREADY_CREATED;
        }
    }

    hwiFormNode = (HwiHandleForm *)LOS_MemAlloc(m_aucSysMem0, sizeof(HwiHandleForm));
    if (hwiFormNode == NULL) {
        HWI_UNLOCK(intSave);
        return OS_ERRNO_HWI_NO_MEMORY;
    }

    hwiFormNode->uwParam = OsHwiCpIrqParam(irqParam);
    if (hwiFormNode->uwParam == LOS_NOK) {
        HWI_UNLOCK(intSave);
        (VOID)LOS_MemFree(m_aucSysMem0, hwiFormNode);
        return OS_ERRNO_HWI_NO_MEMORY;
    }

    hwiFormNode->pfnHook = hwiHandler;
    hwiFormNode->pstNext = (struct tagHwiHandleForm *)NULL;
    hwiForm->pstNext = hwiFormNode;

    if ((irqParam != NULL) && (irqParam->pName != NULL)) {
        g_hwiFormName[hwiNum] = (CHAR *)irqParam->pName;
    }

    g_hwiForm[hwiNum].uwParam = modeResult;

    HWI_UNLOCK(intSave);
    return LOS_OK;
}
#endif

/*
 * Description : initialization of the hardware interrupt
 */
LITE_OS_SEC_TEXT_INIT VOID OsHwiInit(VOID)//硬件中断初始化
{
    UINT32 hwiNum;

    for (hwiNum = 0; hwiNum < OS_HWI_MAX_NUM; hwiNum++) {
        g_hwiForm[hwiNum].pfnHook = NULL;
        g_hwiForm[hwiNum].uwParam = 0;
        g_hwiForm[hwiNum].pstNext = NULL;
    }

    (VOID)memset_s(g_hwiFormName, (sizeof(CHAR *) * OS_HWI_MAX_NUM), 0, (sizeof(CHAR *) * OS_HWI_MAX_NUM));

    HalIrqInit();

    return;
}
//创建一个硬中断
LITE_OS_SEC_TEXT_INIT UINT32 LOS_HwiCreate(HWI_HANDLE_T hwiNum,
                                           HWI_PRIOR_T hwiPrio,
                                           HWI_MODE_T hwiMode,
                                           HWI_PROC_FUNC hwiHandler,
                                           HwiIrqParam *irqParam)
{
    UINT32 ret;

    (VOID)hwiPrio;
    if (hwiHandler == NULL) {//中断处理函数不能为NULL
        return OS_ERRNO_HWI_PROC_FUNC_NULL;
    }
    if ((hwiNum > OS_USER_HWI_MAX) || ((INT32)hwiNum < OS_USER_HWI_MIN)) {//中断数区间限制
        return OS_ERRNO_HWI_NUM_INVALID;
    }

#ifdef LOSCFG_NO_SHARED_IRQ	//不支持共享中断
    ret = OsHwiCreateNoShared(hwiNum, hwiMode, hwiHandler, irqParam);
#else
    ret = OsHwiCreateShared(hwiNum, hwiMode, hwiHandler, irqParam);
#endif
    return ret;
}
//删除一个硬中断
LITE_OS_SEC_TEXT_INIT UINT32 LOS_HwiDelete(HWI_HANDLE_T hwiNum, HwiIrqParam *irqParam)
{
    UINT32 ret;

    if ((hwiNum > OS_USER_HWI_MAX) || ((INT32)hwiNum < OS_USER_HWI_MIN)) {
        return OS_ERRNO_HWI_NUM_INVALID;
    }

#ifdef LOSCFG_NO_SHARED_IRQ	//不支持共享中断
    ret = OsHwiDelNoShared(hwiNum);
#else
    ret = OsHwiDelShared(hwiNum, irqParam);
#endif
    return ret;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
