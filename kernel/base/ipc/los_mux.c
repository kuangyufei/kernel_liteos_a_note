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

#include "los_mux_pri.h"
#include "los_bitmap.h"
#include "los_spinlock.h"
#include "los_mp.h"
#include "los_task_pri.h"
#include "los_exc.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

#if (LOSCFG_BASE_IPC_MUX == YES)
#define MUTEXATTR_TYPE_MASK 0x0FU
//互斥属性初始化
LITE_OS_SEC_TEXT UINT32 LOS_MuxAttrInit(LosMuxAttr *attr)
{
    if (attr == NULL) {
        return LOS_EINVAL;
    }

    attr->protocol    = LOS_MUX_PRIO_INHERIT;	//协议默认用继承方式, A(4)task等B(19)释放锁时,B的调度优先级直接升到(4)
    attr->prioceiling = OS_TASK_PRIORITY_LOWEST;//最低优先级
    attr->type        = LOS_MUX_DEFAULT;		//默认 LOS_MUX_RECURSIVE
    return LOS_OK;
}
// ????? 销毁互斥属 ,这里啥也没干呀 
LITE_OS_SEC_TEXT UINT32 LOS_MuxAttrDestroy(LosMuxAttr *attr)
{
    if (attr == NULL) {
        return LOS_EINVAL;
    }

    return LOS_OK;
}
//获取互斥锁的类型属性,由outType接走,不送! 
LITE_OS_SEC_TEXT UINT32 LOS_MuxAttrGetType(const LosMuxAttr *attr, INT32 *outType)
{
    INT32 type;

    if ((attr == NULL) || (outType == NULL)) {
        return LOS_EINVAL;
    }

    type = (INT32)(attr->type & MUTEXATTR_TYPE_MASK);
    if ((type < LOS_MUX_NORMAL) || (type > LOS_MUX_ERRORCHECK)) {
        return LOS_EINVAL;
    }

    *outType = type;

    return LOS_OK;
}
//设置互斥锁的类型属性  
LITE_OS_SEC_TEXT UINT32 LOS_MuxAttrSetType(LosMuxAttr *attr, INT32 type)
{
    if ((attr == NULL) || (type < LOS_MUX_NORMAL) || (type > LOS_MUX_ERRORCHECK)) {
        return LOS_EINVAL;
    }

    attr->type = (UINT8)((attr->type & ~MUTEXATTR_TYPE_MASK) | (UINT32)type);
    return LOS_OK;
}
//获取互斥锁的类型属性 
LITE_OS_SEC_TEXT UINT32 LOS_MuxAttrGetProtocol(const LosMuxAttr *attr, INT32 *protocol)
{
    if ((attr != NULL) && (protocol != NULL)) {
        *protocol = attr->protocol;
    } else {
        return LOS_EINVAL;
    }

    return LOS_OK;
}
//设置互斥锁属性的协议 
LITE_OS_SEC_TEXT UINT32 LOS_MuxAttrSetProtocol(LosMuxAttr *attr, INT32 protocol)
{
    if (attr == NULL) {
        return LOS_EINVAL;
    }

    switch (protocol) {
        case LOS_MUX_PRIO_NONE:
        case LOS_MUX_PRIO_INHERIT:
        case LOS_MUX_PRIO_PROTECT:
            attr->protocol = (UINT8)protocol;
            return LOS_OK;
        default:
            return LOS_EINVAL;
    }
}
//获取互斥锁属性优先级
LITE_OS_SEC_TEXT UINT32 LOS_MuxAttrGetPrioceiling(const LosMuxAttr *attr, INT32 *prioceiling)
{
    if (attr == NULL) {
        return LOS_EINVAL;
    }

    if (prioceiling != NULL) {
        *prioceiling = attr->prioceiling;
    }

    return LOS_OK;
}
//设置互斥锁属性的优先级的上限 
LITE_OS_SEC_TEXT UINT32 LOS_MuxAttrSetPrioceiling(LosMuxAttr *attr, INT32 prioceiling)
{
    if ((attr == NULL) ||
        (prioceiling < OS_TASK_PRIORITY_HIGHEST) ||
        (prioceiling > OS_TASK_PRIORITY_LOWEST)) {
        return LOS_EINVAL;
    }

    attr->prioceiling = (UINT8)prioceiling;

    return LOS_OK;
}
//设置互斥锁的优先级的上限,老优先级由oldPrioceiling带走
LITE_OS_SEC_TEXT UINT32 LOS_MuxSetPrioceiling(LosMux *mutex, INT32 prioceiling, INT32 *oldPrioceiling)
{
    INT32 ret;
    INT32 retLock;
    if ((mutex == NULL) ||
        (prioceiling < OS_TASK_PRIORITY_HIGHEST) ||
        (prioceiling > OS_TASK_PRIORITY_LOWEST)) {
        return LOS_EINVAL;
    }

    retLock = LOS_MuxLock(mutex, LOS_WAIT_FOREVER);
    if (retLock != LOS_OK) {
        return retLock;
    }

    if (oldPrioceiling != NULL) {
        *oldPrioceiling = mutex->attr.prioceiling;
    }

    ret = LOS_MuxAttrSetPrioceiling(&mutex->attr, prioceiling);

    retLock = LOS_MuxUnlock(mutex);
    if ((ret == LOS_OK) && (retLock != LOS_OK)) {
        return retLock;
    }

    return ret;
}
//获取互斥锁的优先级的上限
LITE_OS_SEC_TEXT UINT32 LOS_MuxGetPrioceiling(const LosMux *mutex, INT32 *prioceiling)
{
    if ((mutex != NULL) && (prioceiling != NULL) && (mutex->magic == OS_MUX_MAGIC)) {
        *prioceiling = mutex->attr.prioceiling;
        return LOS_OK;
    }

    return LOS_EINVAL;
}
//互斥锁是否有效
LITE_OS_SEC_TEXT BOOL LOS_MuxIsValid(const LosMux *mutex)
{
    if ((mutex != NULL) && (mutex->magic == OS_MUX_MAGIC)) {
        return TRUE;
    }

    return FALSE;
}
//检查互斥锁属性是否OK,否则 no ok :|)
STATIC UINT32 OsCheckMutexAttr(const LosMuxAttr *attr)
{
    if (((INT8)(attr->type) < LOS_MUX_NORMAL) || (attr->type > LOS_MUX_ERRORCHECK)) {
        return LOS_NOK;
    }
    if (((INT8)(attr->prioceiling) < OS_TASK_PRIORITY_HIGHEST) || (attr->prioceiling > OS_TASK_PRIORITY_LOWEST)) {
        return LOS_NOK;
    }
    if (((INT8)(attr->protocol) < LOS_MUX_PRIO_NONE) || (attr->protocol > LOS_MUX_PRIO_PROTECT)) {
        return LOS_NOK;
    }
    return LOS_OK;
}
//初始化互斥
LITE_OS_SEC_TEXT UINT32 LOS_MuxInit(LosMux *mutex, const LosMuxAttr *attr)
{
    UINT32 intSave;

    if (mutex == NULL) {
        return LOS_EINVAL;
    }

    if (attr == NULL) {
        (VOID)LOS_MuxAttrInit(&mutex->attr);//属性初始化
    } else {
        (VOID)memcpy_s(&mutex->attr, sizeof(LosMuxAttr), attr, sizeof(LosMuxAttr));//把attr 拷贝到 mutex->attr
    }

    if (OsCheckMutexAttr(&mutex->attr) != LOS_OK) {//检查属性
        return LOS_EINVAL;
    }

    SCHEDULER_LOCK(intSave);		//保存调度自旋锁
    mutex->muxCount = 0;			//锁定互斥量的次数
    mutex->owner = NULL;			//谁持有该锁
    LOS_ListInit(&mutex->muxList);	//互斥量双循环链表
    mutex->magic = OS_MUX_MAGIC;	//固定标识,相当于什么文件固定开头的几个字节
    SCHEDULER_UNLOCK(intSave);		//释放调度自旋锁
    return LOS_OK;
}
//销毁互斥锁
LITE_OS_SEC_TEXT UINT32 LOS_MuxDestroy(LosMux *mutex)
{
    UINT32 intSave;

    if (mutex == NULL) {
        return LOS_EINVAL;
    }

    SCHEDULER_LOCK(intSave);	//保存调度自旋锁
    if (mutex->magic != OS_MUX_MAGIC) {
        SCHEDULER_UNLOCK(intSave);//释放调度自旋锁
        return LOS_EBADF;
    }

    if (mutex->muxCount != 0) {
        SCHEDULER_UNLOCK(intSave);//释放调度自旋锁
        return LOS_EBUSY;
    }

    (VOID)memset_s(mutex, sizeof(LosMux), 0, sizeof(LosMux));//很简单,全部清0处理.
    SCHEDULER_UNLOCK(intSave);	//释放调度自旋锁
    return LOS_OK;
}
//设置互斥锁位图
STATIC VOID OsMuxBitmapSet(const LosMux *mutex, const LosTaskCB *runTask, LosTaskCB *owner)
{	//当前任务优先级高于锁持有task时的处理
    if ((owner->priority > runTask->priority) && (mutex->attr.protocol == LOS_MUX_PRIO_INHERIT)) {//协议用继承方式是怎么的呢?
        LOS_BitmapSet(&(owner->priBitMap), owner->priority);//1.priBitMap是记录任务优先级变化的位图，这里把持有锁任务优先级记录在自己的priBitMap中
        OsTaskPriModify(owner, runTask->priority);//2.把高优先级的当前任务优先级设为持有锁任务的优先级.
    }//注意任务优先级有32个, 是0最高,31最低!!!这里等于提高了持有锁任务的优先级,目的是让其在下次调度中提高选中的概率,从而快速的释放锁.您明白了吗? :|)
}
//恢复互斥锁位图
VOID OsMuxBitmapRestore(const LosMux *mutex, const LosTaskCB *taskCB, LosTaskCB *owner)
{
    UINT16 bitMapPri;

    if (mutex->attr.protocol != LOS_MUX_PRIO_INHERIT) {
        return;
    }

    if (owner->priority >= taskCB->priority) {
        bitMapPri = LOS_LowBitGet(owner->priBitMap);
        if (bitMapPri != LOS_INVALID_BIT_INDEX) {
            LOS_BitmapClr(&(owner->priBitMap), bitMapPri);
            OsTaskPriModify(owner, bitMapPri);
        }
    } else {
        if (LOS_HighBitGet(owner->priBitMap) != taskCB->priority) {
            LOS_BitmapClr(&(owner->priBitMap), taskCB->priority);
        }
    }
}

STATIC LOS_DL_LIST *OsMuxPendFindPosSub(const LosTaskCB *runTask, const LosMux *mutex)
{
    LosTaskCB *pendedTask = NULL;
    LOS_DL_LIST *node = NULL;

    LOS_DL_LIST_FOR_EACH_ENTRY(pendedTask, &(mutex->muxList), LosTaskCB, pendList) {
        if (pendedTask->priority < runTask->priority) {
            continue;
        } else if (pendedTask->priority > runTask->priority) {
            node = &pendedTask->pendList;
            break;
        } else {
            node = pendedTask->pendList.pstNext;
            break;
        }
    }

    return node;
}
//在等锁链表中找到一个优先级比当前任务更低的任务
STATIC LOS_DL_LIST *OsMuxPendFindPos(const LosTaskCB *runTask, LosMux *mutex)
{
    LosTaskCB *pendedTask1 = NULL;
    LosTaskCB *pendedTask2 = NULL;
    LOS_DL_LIST *node = NULL;

    if (LOS_ListEmpty(&mutex->muxList)) {//任务列表为空
        node = &mutex->muxList;//直接赋给node 返回
    } else {//muxList上挂的都是task的pendlist节点
        pendedTask1 = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&mutex->muxList));//找到第一个等锁的任务, 
        pendedTask2 = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_LAST(&mutex->muxList));//找到最后一个等锁的任务
        if ((pendedTask1 != NULL) && (pendedTask1->priority > runTask->priority)) {//如果第一个任务优先级低于当前任务 要怎么搞?
            node = mutex->muxList.pstNext;//找到了,就是第一个
        } else if ((pendedTask2 != NULL) && (pendedTask2->priority <= runTask->priority)) {//如果最后一个任务的优先级高获等于当前任务优先级
            node = &mutex->muxList;//返回自己,这里node就是当前任务的pendlist节点
        } else {//当前任务比第一个低,比最后一个高的情况
            node = OsMuxPendFindPosSub(runTask, mutex);//找下一级
        }
    }

    return node;
}
//互斥锁的主体函数,由OsMuxlockUnsafe调用,互斥锁模块最重要的几个函数之一
//最坏情况就是拿锁失败,让出CPU,变成阻塞任务,等别的任务释放锁后排到自己了接着执行. 
STATIC UINT32 OsMuxPendOp(LosTaskCB *runTask, LosMux *mutex, UINT32 timeout)
{
    UINT32 ret;
    LOS_DL_LIST *node = NULL;
    LosTaskCB *owner = NULL;

    if ((mutex->muxList.pstPrev == NULL) || (mutex->muxList.pstNext == NULL)) {//列表为空时的处理
        /* This is for mutex macro initialization. */
        mutex->muxCount = 0;//锁计数器清0
        mutex->owner = NULL;//锁没有归属任务
        LOS_ListInit(&mutex->muxList);//初始化mux链表[]
    }

    if (mutex->muxCount == 0) {//无task用锁时,肯定能拿到锁了.在里面返回
        mutex->muxCount++;				//互斥锁计数器加1
        mutex->owner = (VOID *)runTask;	//当前任务拿到锁
        LOS_ListTailInsert(&runTask->lockList, &mutex->holdList);//持有锁的任务改变了,节点挂到当前task的锁链表
        if ((runTask->priority > mutex->attr.prioceiling) && (mutex->attr.protocol == LOS_MUX_PRIO_PROTECT)) {//看保护协议的做法是怎样的?
            LOS_BitmapSet(&runTask->priBitMap, runTask->priority);//1.priBitMap是记录任务优先级变化的位图，这里把任务当前的优先级记录在priBitMap
            OsTaskPriModify(runTask, mutex->attr.prioceiling);//2.把高优先级的mutex->attr.prioceiling设为当前任务的优先级.
        }//注意任务优先级有32个, 是0最高,31最低!!!这里等于提高了任务的优先级,目的是让其在下次调度中继续提高被选中的概率,从而快速的释放锁.您明白了吗? :|)
        return LOS_OK;
    }
	//递归锁muxCount>0 如果是递归锁就要处理两种情况 1.runtask持有锁 2.锁被别的任务拿走了
    if (((LosTaskCB *)mutex->owner == runTask) && (mutex->attr.type == LOS_MUX_RECURSIVE)) {//第一种情况 runtask是锁持有方
        mutex->muxCount++;	//递归锁计数器加1,递归锁的目的是防止死锁,鸿蒙默认用的就是递归锁(LOS_MUX_DEFAULT = LOS_MUX_RECURSIVE)
        return LOS_OK;		//成功退出
    }
	//到了这里说明锁在别的任务那里,当前任务只能被阻塞了.
    if (!timeout) {//参数timeout表示等待多久再来拿锁
        return LOS_EINVAL;//timeout = 0表示不等了,没拿到锁就返回不纠结,返回错误.见于LOS_MuxTrylock 
    }
	//自己要被阻塞,只能申请调度,让出CPU core 让别的任务上
    if (!OsPreemptableInSched()) {//不能申请调度 (不能调度的原因是因为没有持有调度任务自旋锁)
        return LOS_EDEADLK;//返回错误,自旋锁被别的CPU core 持有
    }

    OsMuxBitmapSet(mutex, runTask, (LosTaskCB *)mutex->owner);//设置bitmap,尽可能的提高锁持有任务的优先级

    owner = (LosTaskCB *)mutex->owner;	//记录持有锁的任务
    runTask->taskMux = (VOID *)mutex;	//记下当前任务在等待这把锁
    node = OsMuxPendFindPos(runTask, mutex);//在都等锁阻塞链表上找一个适当的位置,在OsTaskWait中把自己从这个入口挂上去
    ret = OsTaskWait(node, timeout, TRUE);//task陷入等待状态 TRUE代表需要调度
    if (ret == LOS_ERRNO_TSK_TIMEOUT) {//这行代码虽和OsTaskWait挨在一起,但要过很久才会执行到,因为在OsTaskWait中CPU切换了任务上下文
        runTask->taskMux = NULL;// 所以重新回到这里时可能已经超时了
        ret = LOS_ETIMEDOUT;//返回超时
    }

    if (timeout != LOS_WAIT_FOREVER) {//不是永远等待的情况
        OsMuxBitmapRestore(mutex, runTask, owner);//恢复bitmap
    }

    return ret;
}

UINT32 OsMuxLockUnsafe(LosMux *mutex, UINT32 timeout)
{
    LosTaskCB *runTask = OsCurrTaskGet();//获取当前任务

    if (mutex->magic != OS_MUX_MAGIC) {
        return LOS_EBADF;
    }

    if (OsCheckMutexAttr(&mutex->attr) != LOS_OK) {
        return LOS_EINVAL;
    }
	//LOS_MUX_ERRORCHECK 时 muxCount是要等于0 ,当前任务持有锁就不能再lock了. 鸿蒙默认用的是递归锁LOS_MUX_RECURSIVE
    if ((mutex->attr.type == LOS_MUX_ERRORCHECK) && (mutex->muxCount != 0) && (mutex->owner == (VOID *)runTask)) {
        return LOS_EDEADLK;
    }

    return OsMuxPendOp(runTask, mutex, timeout);
}
//尝试加锁,
UINT32 OsMuxTrylockUnsafe(LosMux *mutex, UINT32 timeout)
{
    LosTaskCB *runTask = OsCurrTaskGet();//获取当前任务

    if (mutex->magic != OS_MUX_MAGIC) {//检查MAGIC有没有被改变
        return LOS_EBADF;
    }

    if (OsCheckMutexAttr(&mutex->attr) != LOS_OK) {//检查互斥锁属性
        return LOS_EINVAL;
    }

    if ((mutex->owner != NULL) && ((LosTaskCB *)mutex->owner != runTask)) {//已经名锁有主,可惜不是当前任务
        return LOS_EBUSY;//返回busy
    }
    if ((mutex->attr.type != LOS_MUX_RECURSIVE) && (mutex->muxCount != 0)) {//非LOS_MUX_RECURSIVE时 muxCount只能是[0,1]两个值
        return LOS_EBUSY;//这里也表示名锁有主了
    }

    return OsMuxPendOp(runTask, mutex, timeout);//当前任务去拿锁,拿不到就等timeout
}
//拿互斥锁,
LITE_OS_SEC_TEXT UINT32 LOS_MuxLock(LosMux *mutex, UINT32 timeout)
{
    LosTaskCB *runTask = NULL;
    UINT32 intSave;
    UINT32 ret;

    if (mutex == NULL) {
        return LOS_EINVAL;
    }

    if (OS_INT_ACTIVE) {
        return LOS_EINTR;
    }

    runTask = (LosTaskCB *)OsCurrTaskGet();//获取当前任务
    /* DO NOT Call blocking API in system tasks */
    if (runTask->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {//不要在内核任务里用mux锁
        PRINTK("Warning: DO NOT call %s in system tasks.\n", __FUNCTION__);
        OsBackTrace();//打印task信息
    }

    SCHEDULER_LOCK(intSave);//调度自旋锁
    ret = OsMuxLockUnsafe(mutex, timeout);//如果任务没拿到锁,将进入阻塞队列一直等待,直到timeout或者持锁任务释放锁时唤醒它 
    SCHEDULER_UNLOCK(intSave);
    return ret;
}
//尝试锁不纠结,没拿到不会等待,返回
LITE_OS_SEC_TEXT UINT32 LOS_MuxTrylock(LosMux *mutex)
{
    LosTaskCB *runTask = NULL;
    UINT32 intSave;
    UINT32 ret;

    if (mutex == NULL) {
        return LOS_EINVAL;
    }

    if (OS_INT_ACTIVE) {
        return LOS_EINTR;
    }

    runTask = (LosTaskCB *)OsCurrTaskGet();//获取当前执行的任务
    /* DO NOT Call blocking API in system tasks */
    if (runTask->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {//系统任务不能
        PRINTK("Warning: DO NOT call %s in system tasks.\n", __FUNCTION__);
        OsBackTrace();
    }

    SCHEDULER_LOCK(intSave);
    ret = OsMuxTrylockUnsafe(mutex, 0);//timeout = 0,不等待,没拿到锁就算了
    SCHEDULER_UNLOCK(intSave);
    return ret;
}
//降低参数任务的优先级上限
STATIC VOID OsMuxPostOpSub(LosTaskCB *taskCB, LosMux *mutex)
{
    LosTaskCB *pendedTask = NULL;
    UINT16 bitMapPri;
	//理解OsMuxPostOpSub之前要先理解一个task在运行过程中优先级会变化,这些变化都记录在taskCB->priBitMap中
    if (!LOS_ListEmpty(&mutex->muxList)) {//还有任务在等锁
        bitMapPri = LOS_HighBitGet(taskCB->priBitMap);//获取参数任务bitmap中优先级下限(意思是最低优先级记录)
        LOS_DL_LIST_FOR_EACH_ENTRY(pendedTask, (&mutex->muxList), LosTaskCB, pendList) {//循环拿等锁任务
            if (bitMapPri != pendedTask->priority) {//等锁任务优先级不等于参数任务优先级下限的情况
                LOS_BitmapClr(&taskCB->priBitMap, pendedTask->priority);//在bitmap的pendedTask->priority位上置0
            }
        }
    }
    bitMapPri = LOS_LowBitGet(taskCB->priBitMap);//拿低位优先级,也就是最高优先级的记录
    LOS_BitmapClr(&taskCB->priBitMap, bitMapPri);//把taskCB已知的最高优先级记录位清0,实际等于降低了taskCB优先级的上限
    OsTaskPriModify((LosTaskCB *)mutex->owner, bitMapPri);//修改持有任务的优先级
}
//是否有其他任务持有互斥锁而处于阻塞状,如果是就要唤醒它,注意唤醒一个任务的操作是由别的任务完成的
//OsMuxPostOp只由OsMuxUnlockUnsafe,参数任务归还锁了,自然就会遇到锁要给谁用的问题, 因为很多任务在申请锁,由OsMuxPostOp来回答这个问题
STATIC UINT32 OsMuxPostOp(LosTaskCB *taskCB, LosMux *mutex, BOOL *needSched)
{
    LosTaskCB *resumedTask = NULL;

    if (LOS_ListEmpty(&mutex->muxList)) {//如果互斥锁列表为空
        LOS_ListDelete(&mutex->holdList);//把持有互斥锁的节点摘掉
        mutex->owner = NULL;
        return LOS_OK;
    }

    resumedTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(mutex->muxList)));//拿到等待互斥锁链表的第一个任务实体,接下来要唤醒任务
    if (mutex->attr.protocol == LOS_MUX_PRIO_INHERIT) {//互斥锁属性协议是继承会怎么操作?
        if (resumedTask->priority > taskCB->priority) {//拿到锁的任务优先级低于参数任务优先级
            if (LOS_HighBitGet(taskCB->priBitMap) != resumedTask->priority) {//参数任务bitmap中最低的优先级不等于等待锁的任务优先级
                LOS_BitmapClr(&taskCB->priBitMap, resumedTask->priority);//把等待任务锁的任务的优先级记录在参数任务的bitmap中
            }
        } else if (taskCB->priBitMap != 0) {//如果bitmap不等于0说明参数任务至少有任务调度的优先级
            OsMuxPostOpSub(taskCB, mutex);//
        }
    }
    mutex->muxCount = 1;//互斥锁数量为1
    mutex->owner = (VOID *)resumedTask;//互斥锁的持有人换了
    resumedTask->taskMux = NULL;//resumedTask不再等锁了
    LOS_ListDelete(&mutex->holdList);//自然要从等锁链表中把自己摘出去
    LOS_ListTailInsert(&resumedTask->lockList, &mutex->holdList);//把锁挂到恢复任务的锁链表上,lockList是任务持有的所有锁记录
    OsTaskWake(resumedTask);//resumedTask有了锁就唤醒它,因为当初在没有拿到锁时处于了pend状态
    if (needSched != NULL) {//如果不为空
        *needSched = TRUE;//就走起再次调度流程
    }

    return LOS_OK;
}

UINT32 OsMuxUnlockUnsafe(LosTaskCB *taskCB, LosMux *mutex, BOOL *needSched)
{
    UINT16 bitMapPri;

    if (mutex->magic != OS_MUX_MAGIC) {
        return LOS_EBADF;
    }

    if (OsCheckMutexAttr(&mutex->attr) != LOS_OK) {
        return LOS_EINVAL;
    }

    if (mutex->muxCount == 0) {
        return LOS_EPERM;
    }

    if ((LosTaskCB *)mutex->owner != taskCB) {
        return LOS_EPERM;
    }
	//注意 --mutex->muxCount 先执行了-- 操作.
    if ((--mutex->muxCount != 0) && (mutex->attr.type == LOS_MUX_RECURSIVE)) {//属性类型为LOS_MUX_RECURSIVE时,muxCount是可以不为0的
        return LOS_OK;
    }

    if (mutex->attr.protocol == LOS_MUX_PRIO_PROTECT) {//属性协议为保护时
        bitMapPri = LOS_HighBitGet(taskCB->priBitMap);//找priBitMap记录中最高的那个优先级
        if (bitMapPri != LOS_INVALID_BIT_INDEX) {
            LOS_BitmapClr(&taskCB->priBitMap, bitMapPri);//找priBitMap记录中最高的那个优先级
            OsTaskPriModify(taskCB, bitMapPri);//改回task原来自己的优先级
        }
    }

    /* Whether a task block the mutex lock. *///任务是否阻塞互斥锁
    return OsMuxPostOp(taskCB, mutex, needSched);//一个任务去唤醒另一个在等锁的任务
}
//释放锁
LITE_OS_SEC_TEXT UINT32 LOS_MuxUnlock(LosMux *mutex)
{
    LosTaskCB *runTask = NULL;
    BOOL needSched = FALSE;
    UINT32 intSave;
    UINT32 ret;

    if (mutex == NULL) {
        return LOS_EINVAL;
    }

    if (OS_INT_ACTIVE) {
        return LOS_EINTR;
    }

    runTask = (LosTaskCB *)OsCurrTaskGet();//获取当前任务
    /* DO NOT Call blocking API in system tasks */
    if (runTask->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {//不能在系统任务里调用,因为很容易让系统任务发生死锁
        PRINTK("Warning: DO NOT call %s in system tasks.\n", __FUNCTION__);
        OsBackTrace();
    }

    SCHEDULER_LOCK(intSave);
    ret = OsMuxUnlockUnsafe(runTask, mutex, &needSched);
    SCHEDULER_UNLOCK(intSave);
    if (needSched == TRUE) {//需要调度的情况
        LOS_MpSchedule(OS_MP_CPU_ALL);//向所有CPU发送调度指令
        LOS_Schedule();//发起调度
    }
    return ret;
}

#endif /* (LOSCFG_BASE_IPC_MUX == YES) */

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
