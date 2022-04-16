/*!
 * @file    los_mux.c
 * @brief
 * @link kernel-mini-basic-ipc-mutex-guide http://weharmonyos.com/openharmony/zh-cn/device-dev/kernel/kernel-mini-basic-ipc-mutex-guide.html @endlink
   @verbatim
   基本概念
		互斥锁又称互斥型信号量，是一种特殊的二值性信号量，用于实现对共享资源的独占式处理。
		任意时刻互斥锁的状态只有两种，开锁或闭锁。当有任务持有时，互斥锁处于闭锁状态，这个任务获得该互斥锁的所有权。
		当该任务释放它时，该互斥锁被开锁，任务失去该互斥锁的所有权。当一个任务持有互斥锁时，其他任务将不能再对该互斥锁进行开锁或持有。
		多任务环境下往往存在多个任务竞争同一共享资源的应用场景，互斥锁可被用于对共享资源的保护从而实现独占式访问。
		另外互斥锁可以解决信号量存在的优先级翻转问题。
   
   运作机制
	   多任务环境下会存在多个任务访问同一公共资源的场景，而有些公共资源是非共享的临界资源，
	   只能被独占使用。互斥锁怎样来避免这种冲突呢？
	   用互斥锁处理临界资源的同步访问时，如果有任务访问该资源，则互斥锁为加锁状态。此时其他任务
	   如果想访问这个临界资源则会被阻塞，直到互斥锁被持有该锁的任务释放后，其他任务才能重新访问
	   该公共资源，此时互斥锁再次上锁，如此确保同一时刻只有一个任务正在访问这个临界资源，保证了
	   临界资源操作的完整性。
   
   使用场景
	   多任务环境下往往存在多个任务竞争同一临界资源的应用场景，互斥锁可以提供任务间的互斥机制，
	   防止两个任务在同一时刻访问相同的临界资源，从而实现独占式访问。
   
   申请互斥锁有三种模式：无阻塞模式、永久阻塞模式、定时阻塞模式。
	   无阻塞模式：任务需要申请互斥锁，若该互斥锁当前没有任务持有，或者持有该互斥锁的任务和申请
	   		该互斥锁的任务为同一个任务，则申请成功。
	   永久阻塞模式：任务需要申请互斥锁，若该互斥锁当前没有被占用，则申请成功。否则，该任务进入阻塞态，
	   		系统切换到就绪任务中优先级高者继续执行。任务进入阻塞态后，直到有其他任务释放该互斥锁，阻塞任务才会重新得以执行。
	   定时阻塞模式：任务需要申请互斥锁，若该互斥锁当前没有被占用，则申请成功。否则该任务进入阻塞态，
	   		系统切换到就绪任务中优先级高者继续执行。任务进入阻塞态后，指定时间超时前有其他任务释放该互斥锁，
	   		或者用户指定时间超时后，阻塞任务才会重新得以执行。 
	   释放互斥锁：
			如果有任务阻塞于该互斥锁，则唤醒被阻塞任务中优先级最高的，该任务进入就绪态，并进行任务调度。
			如果没有任务阻塞于该互斥锁，则互斥锁释放成功。
   
   互斥锁典型场景的开发流程：
	   通过make menuconfig配置互斥锁模块。
	   创建互斥锁LOS_MuxCreate。
	   申请互斥锁LOS_MuxPend。
	   释放互斥锁LOS_MuxPost。
	   删除互斥锁LOS_MuxDelete。

   @endverbatim
 * @image html https://gitee.com/weharmonyos/resources/raw/master/27/mux.png
 * @attention 两个任务不能对同一把互斥锁加锁。如果某任务对已被持有的互斥锁加锁，则该任务会被挂起，直到持有该锁的任务对互斥锁解锁，才能执行对这把互斥锁的加锁操作。
			  \n 互斥锁不能在中断服务程序中使用。
			  \n LiteOS-M内核作为实时操作系统需要保证任务调度的实时性，尽量避免任务的长时间阻塞，因此在获得互斥锁之后，应该尽快释放互斥锁。
			  \n 持有互斥锁的过程中，不得再调用LOS_TaskPriSet等接口更改持有互斥锁任务的优先级。  
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-11-18
 */
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

#include "los_mux_pri.h"
#include "los_bitmap.h"
#include "los_spinlock.h"
#include "los_mp.h"
#include "los_task_pri.h"
#include "los_exc.h"
#include "los_sched_pri.h"


#ifdef LOSCFG_BASE_IPC_MUX
#define MUTEXATTR_TYPE_MASK 0x0FU
///互斥属性初始化
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
/// ????? 销毁互斥属 ,这里啥也没干呀 
LITE_OS_SEC_TEXT UINT32 LOS_MuxAttrDestroy(LosMuxAttr *attr)
{
    if (attr == NULL) {
        return LOS_EINVAL;
    }

    return LOS_OK;
}
///获取互斥锁的类型属性,由outType接走,不送! 
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
///设置互斥锁的类型属性  
LITE_OS_SEC_TEXT UINT32 LOS_MuxAttrSetType(LosMuxAttr *attr, INT32 type)
{
    if ((attr == NULL) || (type < LOS_MUX_NORMAL) || (type > LOS_MUX_ERRORCHECK)) {
        return LOS_EINVAL;
    }

    attr->type = (UINT8)((attr->type & ~MUTEXATTR_TYPE_MASK) | (UINT32)type);
    return LOS_OK;
}
///获取互斥锁的类型属性 
LITE_OS_SEC_TEXT UINT32 LOS_MuxAttrGetProtocol(const LosMuxAttr *attr, INT32 *protocol)
{
    if ((attr != NULL) && (protocol != NULL)) {
        *protocol = attr->protocol;
    } else {
        return LOS_EINVAL;
    }

    return LOS_OK;
}
///设置互斥锁属性的协议 
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
///获取互斥锁属性优先级
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
///设置互斥锁属性的优先级的上限 
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
///设置互斥锁的优先级的上限,老优先级由oldPrioceiling带走
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
///获取互斥锁的优先级的上限
LITE_OS_SEC_TEXT UINT32 LOS_MuxGetPrioceiling(const LosMux *mutex, INT32 *prioceiling)
{
    if ((mutex != NULL) && (prioceiling != NULL) && (mutex->magic == OS_MUX_MAGIC)) {
        *prioceiling = mutex->attr.prioceiling;
        return LOS_OK;
    }

    return LOS_EINVAL;
}
///互斥锁是否有效
LITE_OS_SEC_TEXT BOOL LOS_MuxIsValid(const LosMux *mutex)
{
    if ((mutex != NULL) && (mutex->magic == OS_MUX_MAGIC)) {
        return TRUE;
    }

    return FALSE;
}
///检查互斥锁属性是否OK,否则 no ok :|)
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
/// 初始化互斥锁
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

    SCHEDULER_LOCK(intSave);		//拿到调度自旋锁
    mutex->muxCount = 0;			//锁定互斥量的次数
    mutex->owner = NULL;			//谁持有该锁
    LOS_ListInit(&mutex->muxList);	//互斥量双循环链表
    mutex->magic = OS_MUX_MAGIC;	//固定标识,互斥锁的魔法数字
    SCHEDULER_UNLOCK(intSave);		//释放调度自旋锁
    return LOS_OK;
}
///销毁互斥锁
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
///设置互斥锁位图
STATIC VOID OsMuxBitmapSet(const LosMux *mutex, const LosTaskCB *runTask)
{
    if (mutex->attr.protocol != LOS_MUX_PRIO_INHERIT) {
        return;
    }

    SchedParam param = { 0 };
    LosTaskCB *owner = (LosTaskCB *)mutex->owner;
    INT32 ret = OsSchedParamCompare(owner, runTask);
    if (ret > 0) {
        runTask->ops->schedParamGet(runTask, &param);
        owner->ops->priorityInheritance(owner, &param);
    }
}
///恢复互斥锁位图
VOID OsMuxBitmapRestore(const LosMux *mutex, const LOS_DL_LIST *list, const LosTaskCB *runTask)
{
    if (mutex->attr.protocol != LOS_MUX_PRIO_INHERIT) {
        return;
    }

    SchedParam param = { 0 };
    LosTaskCB *owner = (LosTaskCB *)mutex->owner;
    runTask->ops->schedParamGet(runTask, &param);
    owner->ops->priorityRestore(owner, list, &param);
}

/// 最坏情况就是拿锁失败,让出CPU,变成阻塞任务,等别的任务释放锁后排到自己了接着执行. 
STATIC UINT32 OsMuxPendOp(LosTaskCB *runTask, LosMux *mutex, UINT32 timeout)
{
    UINT32 ret;

    if ((mutex->muxList.pstPrev == NULL) || (mutex->muxList.pstNext == NULL)) {//列表为空时的处理
        /* This is for mutex macro initialization. */
        mutex->muxCount = 0;//锁计数器清0
        mutex->owner = NULL;//锁没有归属任务
        LOS_ListInit(&mutex->muxList);//初始化锁的任务链表,后续申请这把锁任务都会挂上去
    }

    if (mutex->muxCount == 0) {//无task用锁时,肯定能拿到锁了.在里面返回
        mutex->muxCount++;				//互斥锁计数器加1
        mutex->owner = (VOID *)runTask;	//当前任务拿到锁
        LOS_ListTailInsert(&runTask->lockList, &mutex->holdList);
        if (mutex->attr.protocol == LOS_MUX_PRIO_PROTECT) {
            SchedParam param = { 0 };
            runTask->ops->schedParamGet(runTask, &param);
            param.priority = mutex->attr.prioceiling;
            runTask->ops->priorityInheritance(runTask, &param);
        }
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

    OsMuxBitmapSet(mutex, runTask);//设置锁位图,尽可能的提高锁持有任务的优先级

    runTask->taskMux = (VOID *)mutex;	//记下当前任务在等待这把锁
    LOS_DL_LIST *node = OsSchedLockPendFindPos(runTask, &mutex->muxList);
    if (node == NULL) {
        ret = LOS_NOK;
        return ret;
    }

    OsTaskWaitSetPendMask(OS_TASK_WAIT_MUTEX, (UINTPTR)mutex, timeout);
    ret = runTask->ops->wait(runTask, node, timeout);
    if (ret == LOS_ERRNO_TSK_TIMEOUT) {//这行代码虽和OsTaskWait挨在一起,但要过很久才会执行到,因为在OsTaskWait中CPU切换了任务上下文
        OsMuxBitmapRestore(mutex, NULL, runTask);
        runTask->taskMux = NULL;// 所以重新回到这里时可能已经超时了
        ret = LOS_ETIMEDOUT;//返回超时
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
    if ((mutex->attr.type == LOS_MUX_ERRORCHECK) && (mutex->owner == (VOID *)runTask)) {
        return LOS_EDEADLK;
    }

    return OsMuxPendOp(runTask, mutex, timeout);
}
/// 尝试加锁,
UINT32 OsMuxTrylockUnsafe(LosMux *mutex, UINT32 timeout)
{
    LosTaskCB *runTask = OsCurrTaskGet();//获取当前任务

    if (mutex->magic != OS_MUX_MAGIC) {//检查MAGIC有没有被改变
        return LOS_EBADF;
    }

    if (OsCheckMutexAttr(&mutex->attr) != LOS_OK) {//检查互斥锁属性
        return LOS_EINVAL;
    }

    if ((mutex->owner != NULL) &&
        (((LosTaskCB *)mutex->owner != runTask) || (mutex->attr.type != LOS_MUX_RECURSIVE))) {
        return LOS_EBUSY;
    }

    return OsMuxPendOp(runTask, mutex, timeout);//当前任务去拿锁,拿不到就等timeout
}
/// 拿互斥锁,
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
///尝试要锁,没拿到也不等,直接返回,不纠结
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

/*!
 * @brief OsMuxPostOp	
 * 是否有其他任务持有互斥锁而处于阻塞状,如果是就要唤醒它,注意唤醒一个任务的操作是由别的任务完成的
 * OsMuxPostOp只由OsMuxUnlockUnsafe,参数任务归还锁了,自然就会遇到锁要给谁用的问题, 因为很多任务在申请锁,由OsMuxPostOp来回答这个问题
 * @param mutex	
 * @param needSched	
 * @param taskCB	
 * @return	
 *
 * @see
 */
STATIC UINT32 OsMuxPostOp(LosTaskCB *taskCB, LosMux *mutex, BOOL *needSched)
{
    if (LOS_ListEmpty(&mutex->muxList)) {//如果互斥锁列表为空
        LOS_ListDelete(&mutex->holdList);//把持有互斥锁的节点摘掉
        mutex->owner = NULL;
        return LOS_OK;
    }

    LosTaskCB *resumedTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(mutex->muxList)));//拿到等待互斥锁链表的第一个任务实体,接下来要唤醒任务
    OsMuxBitmapRestore(mutex, &mutex->muxList, resumedTask);

    mutex->muxCount = 1;//互斥锁数量为1
    mutex->owner = (VOID *)resumedTask;//互斥锁的持有人换了
    LOS_ListDelete(&mutex->holdList);//自然要从等锁链表中把自己摘出去
    LOS_ListTailInsert(&resumedTask->lockList, &mutex->holdList);//把锁挂到恢复任务的锁链表上,lockList是任务持有的所有锁记录
    OsTaskWakeClearPendMask(resumedTask);
    resumedTask->ops->wake(resumedTask);
    resumedTask->taskMux = NULL;
    if (needSched != NULL) {//如果不为空
        *needSched = TRUE;//就走起再次调度流程
    }

    return LOS_OK;
}

UINT32 OsMuxUnlockUnsafe(LosTaskCB *taskCB, LosMux *mutex, BOOL *needSched)
{
    if (mutex->magic != OS_MUX_MAGIC) {
        return LOS_EBADF;
    }

    if (OsCheckMutexAttr(&mutex->attr) != LOS_OK) {
        return LOS_EINVAL;
    }

    if ((LosTaskCB *)mutex->owner != taskCB) {
        return LOS_EPERM;
    }

    if (mutex->muxCount == 0) {
        return LOS_EPERM;
    }
	//注意 --mutex->muxCount 先执行了-- 操作.
    if ((--mutex->muxCount != 0) && (mutex->attr.type == LOS_MUX_RECURSIVE)) {//属性类型为LOS_MUX_RECURSIVE时,muxCount是可以不为0的
        return LOS_OK;
    }

    if (mutex->attr.protocol == LOS_MUX_PRIO_PROTECT) {//属性协议为保护时
        SchedParam param = { 0 };
        taskCB->ops->schedParamGet(taskCB, &param);
        taskCB->ops->priorityRestore(taskCB, NULL, &param);
    }

    /* Whether a task block the mutex lock. *///任务是否阻塞互斥锁
    return OsMuxPostOp(taskCB, mutex, needSched);//一个任务去唤醒另一个在等锁的任务
}
///释放锁
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

