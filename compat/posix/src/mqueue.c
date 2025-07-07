/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2023 Huawei Device Co., Ltd. All rights reserved.
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

#include "mqueue.h"
#ifdef LOSCFG_FS_VFS
#include "fcntl.h"
#include "pthread.h"
#include "map_error.h"
#include "time_posix.h"
#include "los_memory.h"
#include "los_vm_map.h"
#include "los_process_pri.h"
#include "fs/file.h"
#include "user_copy.h"

// 将非阻塞标志定义为O_NONBLOCK，用于消息队列操作
#define FNONBLOCK   O_NONBLOCK

#ifndef LOSCFG_IPC_CONTAINER  // 非容器环境下的消息队列实现
/* 全局变量定义 */
STATIC fd_set g_queueFdSet;  // 消息队列文件描述符集合
STATIC struct mqarray g_queueTable[LOSCFG_BASE_IPC_QUEUE_LIMIT];  // 消息队列控制块表
STATIC pthread_mutex_t g_mqueueMutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;  // 消息队列互斥锁（支持递归加锁）
STATIC struct mqpersonal *g_mqPrivBuf[MAX_MQ_FD];  // 消息队列私有数据缓冲区

// 宏定义：简化全局变量访问
#define IPC_QUEUE_FD_SET g_queueFdSet          // 文件描述符集合宏
#define IPC_QUEUE_TABLE  g_queueTable           // 队列控制块表宏
#define IPC_QUEUE_MUTEX  g_mqueueMutex          // 互斥锁宏
#define IPC_QUEUE_MQ_PRIV_BUF g_mqPrivBuf       // 私有数据缓冲区宏
#endif

/* 本地函数声明 */
/**
 * @brief 消息队列名称合法性检查
 * @param[in] mqName 消息队列名称
 * @return 成功返回0，失败返回-1并设置errno
 * @note 检查名称是否为空、长度是否合法
 */
STATIC INLINE INT32 MqNameCheck(const CHAR *mqName)
{
    if (mqName == NULL) {  // 检查名称指针是否为空
        errno = EINVAL;  // 设置参数无效错误
        return -1;  // 返回错误
    }

    if (strlen(mqName) == 0) {  // 检查名称长度是否为0
        errno = EINVAL;  // 设置参数无效错误
        return -1;  // 返回错误
    }

    if (strlen(mqName) > (PATH_MAX - 1)) {  // 检查名称长度是否超过系统限制
        errno = ENAMETOOLONG;  // 设置名称过长错误
        return -1;  // 返回错误
    }
    return 0;  // 名称合法，返回成功
}

/**
 * @brief 通过队列ID获取消息队列控制块
 * @param[in] queueID 队列ID
 * @param[out] queueCB 输出参数，指向获取到的队列控制块
 * @return 成功返回LOS_OK，失败返回错误码
 * @note 验证队列ID有效性并返回对应的控制块
 */
STATIC INLINE UINT32 GetMqueueCBByID(UINT32 queueID, LosQueueCB **queueCB)
{
    LosQueueCB *tmpQueueCB = NULL;  // 临时队列控制块指针
    if (queueCB == NULL) {  // 检查输出参数是否为空
        errno = EINVAL;  // 设置参数无效错误
        return LOS_ERRNO_QUEUE_READ_PTR_NULL;  // 返回指针为空错误码
    }
    tmpQueueCB = GET_QUEUE_HANDLE(queueID);  // 通过ID获取队列句柄
    // 检查队列索引是否越界或队列ID是否匹配
    if ((GET_QUEUE_INDEX(queueID) >= LOSCFG_BASE_IPC_QUEUE_LIMIT) || (tmpQueueCB->queueID != queueID)) {
        return LOS_ERRNO_QUEUE_INVALID;  // 返回队列无效错误码
    }
    *queueCB = tmpQueueCB;  // 设置输出参数

    return LOS_OK;  // 返回成功
}

/**
 * @brief 通过名称查找消息队列控制块
 * @param[in] name 消息队列名称
 * @return 成功返回控制块指针，失败返回NULL
 * @note 遍历队列表，通过名称精确匹配查找
 */
STATIC INLINE struct mqarray *GetMqueueCBByName(const CHAR *name)
{
    UINT32 index;  // 循环索引
    UINT32 mylen = strlen(name);  // 目标名称长度

    for (index = 0; index < LOSCFG_BASE_IPC_QUEUE_LIMIT; index++) {  // 遍历队列控制块表
        // 跳过未初始化或名称长度不匹配的条目
        if ((IPC_QUEUE_TABLE[index].mq_name == NULL) || (strlen(IPC_QUEUE_TABLE[index].mq_name) != mylen)) {
            continue;  // 继续下一个条目
        }

        // 比较名称（精确匹配）
        if (strncmp(name, (const CHAR *)(IPC_QUEUE_TABLE[index].mq_name), mylen) == 0) {
            return &(IPC_QUEUE_TABLE[index]);  // 返回找到的控制块
        }
    }
    return NULL;  // 未找到，返回NULL
}

/**
 * @brief 执行消息队列删除操作
 * @param[in] mqueueCB 指向要删除的队列控制块
 * @return 成功返回0，失败返回-1并设置errno
 * @note 释放资源并删除内核队列
 */
STATIC INT32 DoMqueueDelete(struct mqarray *mqueueCB)
{
    UINT32 ret;  // 操作返回值
#ifdef LOSCFG_KERNEL_IPC_PLIMIT  // 如果启用了IPC资源限制
    OsIPCLimitMqFree();  // 释放消息队列资源限制计数
#endif
    if (mqueueCB->mq_name != NULL) {  // 如果名称不为空
        LOS_MemFree(OS_SYS_MEM_ADDR, mqueueCB->mq_name);  // 释放名称内存
        mqueueCB->mq_name = NULL;  // 重置名称指针
    }

    mqueueCB->mqcb = NULL;  // 重置队列控制块指针
    /* 当需要释放消息队列列表头节点时，重置mode_data */
    mqueueCB->mode_data.data = 0;  // 重置模式数据
    mqueueCB->euid = -1;  // 重置有效用户ID
    mqueueCB->egid = -1;  // 重置有效组ID
    mqueueCB->mq_notify.pid = 0;  // 重置通知PID

    ret = LOS_QueueDelete(mqueueCB->mq_id);  // 调用内核API删除队列
    switch (ret) {  // 根据返回值处理
        case LOS_OK:  // 删除成功
            return 0;  // 返回成功
        case LOS_ERRNO_QUEUE_NOT_FOUND:  // 队列未找到
        case LOS_ERRNO_QUEUE_NOT_CREATE:  // 队列未创建
        case LOS_ERRNO_QUEUE_IN_TSKUSE:  // 队列正在被任务使用
        case LOS_ERRNO_QUEUE_IN_TSKWRITE:  // 队列正在被任务写入
            errno = EAGAIN;  // 设置重试错误
            return -1;  // 返回错误
        default:  // 其他错误
            errno = EINVAL;  // 设置参数无效错误
            return -1;  // 返回错误
    }
}

/**
 * @brief 保存消息队列名称到控制块
 * @param[in] mqName 消息队列名称
 * @param[in,out] mqueueCB 队列控制块
 * @return 成功返回LOS_OK，失败返回LOS_NOK并设置errno
 * @note 分配内存并复制名称字符串
 */
STATIC int SaveMqueueName(const CHAR *mqName, struct mqarray *mqueueCB)
{
    size_t nameLen;  // 名称长度

    nameLen = strlen(mqName);  // 获取名称长度（sys_mq_open已检查名称和长度）
    // 分配内存存储名称（系统内存池）
    mqueueCB->mq_name = (char *)LOS_MemAlloc(OS_SYS_MEM_ADDR, nameLen + 1);
    if (mqueueCB->mq_name == NULL) {  // 内存分配失败
        errno = ENOMEM;  // 设置内存不足错误
        return LOS_NOK;  // 返回失败
    }

    // 复制名称字符串（带长度检查）
    if (strncpy_s(mqueueCB->mq_name, (nameLen + 1), mqName, nameLen) != EOK) {
        LOS_MemFree(OS_SYS_MEM_ADDR, mqueueCB->mq_name);  // 释放已分配内存
        mqueueCB->mq_name = NULL;  // 重置名称指针
        errno = EINVAL;  // 设置参数无效错误
        return LOS_NOK;  // 返回失败
    }
    mqueueCB->mq_name[nameLen] = '\0';  // 确保字符串以null结尾
    return LOS_OK;  // 返回成功
}

/**
 * @brief 初始化消息队列控制块
 * @param[in,out] mqueueCB 队列控制块
 * @param[in] attr 队列属性
 * @param[in] openFlag 打开标志
 * @param[in] mode 访问模式
 * @note 初始化控制块中的各种标志和计数器
 */
STATIC VOID MqueueCBInit(struct mqarray *mqueueCB, const struct mq_attr *attr, INT32 openFlag, UINT32 mode)
{
    mqueueCB->unlinkflag = FALSE;  // 未链接标志
    mqueueCB->unlink_ref = 0;  // 链接引用计数
    mqueueCB->mq_personal->mq_status = MQ_USE_MAGIC;  // 设置使用中标记
    mqueueCB->mq_personal->mq_next = NULL;  // 下一个私有数据指针
    mqueueCB->mq_personal->mq_posixdes = mqueueCB;  // 指向队列控制块
    // 设置标志（打开标志 | 属性中的非阻塞标志）
    mqueueCB->mq_personal->mq_flags = (INT32)((UINT32)openFlag | ((UINT32)attr->mq_flags & (UINT32)FNONBLOCK));
    mqueueCB->mq_personal->mq_mode = mode;  // 访问模式
    mqueueCB->mq_personal->mq_refcount = 0;  // 引用计数
    mqueueCB->mq_notify.pid = 0;  // 通知PID
}

/**
 * @brief 创建新的消息队列
 * @param[in] attr 队列属性（包含最大消息数和消息大小）
 * @param[in] mqName 队列名称
 * @param[in] openFlag 打开标志
 * @param[in] mode 访问模式
 * @return 成功返回私有数据指针，失败返回(struct mqpersonal *)-1并设置errno
 * @note 调用内核API创建队列并初始化控制块
 */
STATIC struct mqpersonal *DoMqueueCreate(const struct mq_attr *attr, const CHAR *mqName, INT32 openFlag, UINT32 mode)
{
    struct mqarray *mqueueCB = NULL;  // 队列控制块指针
    UINT32 mqueueID;  // 队列ID

#ifdef LOSCFG_KERNEL_IPC_PLIMIT  // 如果启用了IPC资源限制
    if (OsIPCLimitMqAlloc() != LOS_OK) {  // 分配资源限制失败
        return (struct mqpersonal *)-1;  // 返回错误
    }
#endif
    // 创建内核消息队列（无名称，指定最大消息数和消息大小）
    UINT32 err = LOS_QueueCreate(NULL, attr->mq_maxmsg, &mqueueID, 0, attr->mq_msgsize);
    if (map_errno(err) != ENOERR) {  // 检查错误码
        goto ERROUT;  // 跳转到错误处理
    }

    // 查找空闲的队列控制块
    if (IPC_QUEUE_TABLE[GET_QUEUE_INDEX(mqueueID)].mqcb == NULL) {
        mqueueCB = &(IPC_QUEUE_TABLE[GET_QUEUE_INDEX(mqueueID)]);  // 获取控制块
        mqueueCB->mq_id = mqueueID;  // 设置队列ID
    }

    if (mqueueCB == NULL) {  // 未找到空闲控制块
        errno = EINVAL;  // 设置参数无效错误
        goto ERROUT;  // 跳转到错误处理
    }

    if (SaveMqueueName(mqName, mqueueCB) != LOS_OK) {  // 保存队列名称失败
        goto ERROUT;  // 跳转到错误处理
    }

    // 通过ID获取队列控制块
    if (GetMqueueCBByID(mqueueCB->mq_id, &(mqueueCB->mqcb)) != LOS_OK) {
        errno = ENOSPC;  // 设置空间不足错误
        goto ERROUT;  // 跳转到错误处理
    }

    // 分配私有数据内存
    mqueueCB->mq_personal = (struct mqpersonal *)LOS_MemAlloc(OS_SYS_MEM_ADDR, sizeof(struct mqpersonal));
    if (mqueueCB->mq_personal == NULL) {  // 内存分配失败
        (VOID)LOS_QueueDelete(mqueueCB->mq_id);  // 删除已创建的内核队列
        mqueueCB->mqcb->queueHandle = NULL;  // 重置队列句柄
        mqueueCB->mqcb = NULL;  // 重置控制块指针
        errno = ENOSPC;  // 设置空间不足错误
        goto ERROUT;  // 跳转到错误处理
    }

    MqueueCBInit(mqueueCB, attr, openFlag, mode);  // 初始化队列控制块

    return mqueueCB->mq_personal;  // 返回私有数据指针
ERROUT:  // 错误处理标签

    if ((mqueueCB != NULL) && (mqueueCB->mq_name != NULL)) {  // 如果已分配名称内存
        LOS_MemFree(OS_SYS_MEM_ADDR, mqueueCB->mq_name);  // 释放名称内存
        mqueueCB->mq_name = NULL;  // 重置名称指针
    }
#ifdef LOSCFG_KERNEL_IPC_PLIMIT  // 如果启用了IPC资源限制
    OsIPCLimitMqFree();  // 释放资源限制计数
#endif
    return (struct mqpersonal *)-1;  // 返回错误
}

/**
 * @brief 打开已存在的消息队列
 * @param[in] mqueueCB 队列控制块
 * @param[in] openFlag 打开标志
 * @return 成功返回私有数据指针，失败返回(struct mqpersonal *)-1并设置errno
 * @note 为现有队列创建新的私有数据结构
 */
STATIC struct mqpersonal *DoMqueueOpen(struct mqarray *mqueueCB, INT32 openFlag)
{
    struct mqpersonal *privateMqPersonal = NULL;  // 私有数据指针

    /* 已存在同名队列于全局表中 */
    if (mqueueCB->unlinkflag == TRUE) {  // 如果队列已标记为删除
        errno = EINVAL;  // 设置参数无效错误
        goto ERROUT;  // 跳转到错误处理
    }
    // 分配私有数据内存
    privateMqPersonal = (struct mqpersonal *)LOS_MemAlloc(OS_SYS_MEM_ADDR, sizeof(struct mqpersonal));
    if (privateMqPersonal == NULL) {  // 内存分配失败
        errno = ENOSPC;  // 设置空间不足错误
        goto ERROUT;  // 跳转到错误处理
    }

    privateMqPersonal->mq_next = mqueueCB->mq_personal;  // 插入到私有数据链表头部
    mqueueCB->mq_personal = privateMqPersonal;  // 更新链表头

    privateMqPersonal->mq_posixdes = mqueueCB;  // 指向队列控制块
    privateMqPersonal->mq_flags = openFlag;  // 设置打开标志
    privateMqPersonal->mq_status = MQ_USE_MAGIC;  // 设置使用中标记
    privateMqPersonal->mq_refcount = 0;  // 初始化引用计数

    return privateMqPersonal;  // 返回私有数据指针

ERROUT:  // 错误处理标签
    return (struct mqpersonal *)-1;  // 返回错误
}

/**
 * @brief 关闭消息队列
 * @param[in] privateMqPersonal 私有数据指针
 * @return 成功返回LOS_OK，失败返回LOS_NOK并设置errno
 * @note 从链表中移除私有数据并在必要时删除队列
 */
STATIC INT32 DoMqueueClose(struct mqpersonal *privateMqPersonal)
{
    struct mqarray *mqueueCB = NULL;  // 队列控制块指针
    struct mqpersonal *tmp = NULL;  // 临时指针，用于遍历链表
    INT32 ret;  // 操作返回值

    mqueueCB = privateMqPersonal->mq_posixdes;  // 获取队列控制块
    if (mqueueCB == NULL || mqueueCB->mq_personal == NULL) {  // 检查控制块有效性
        errno = EBADF;  // 设置文件描述符错误
        return LOS_NOK;  // 返回失败
    }

    // 如果队列已标记为删除且是最后一个引用
    if ((mqueueCB->unlinkflag == TRUE) && (privateMqPersonal->mq_next == NULL)) {
        ret = DoMqueueDelete(mqueueCB);  // 执行队列删除
        if (ret < 0) {  // 删除失败
            return ret;  // 返回错误
        }
    }
    /* 查找并移除私有数据 */
    if (mqueueCB->mq_personal == privateMqPersonal) {  // 如果是链表头节点
        mqueueCB->mq_personal = privateMqPersonal->mq_next;  // 更新头指针
    } else {  // 非头节点，遍历查找
        for (tmp = mqueueCB->mq_personal; tmp->mq_next != NULL; tmp = tmp->mq_next) {
            if (tmp->mq_next == privateMqPersonal) {  // 找到目标节点
                break;  // 跳出循环
            }
        }
        if (tmp->mq_next == NULL) {  // 未找到节点
            errno = EBADF;  // 设置文件描述符错误
            return LOS_NOK;  // 返回失败
        }
        tmp->mq_next = privateMqPersonal->mq_next;  // 移除节点
    }
    /* 标记为未使用 */
    privateMqPersonal->mq_status = 0;  // 清除使用中标记

    /* 释放私有数据 */
    (VOID)LOS_MemFree(OS_SYS_MEM_ADDR, privateMqPersonal);  // 释放内存

    return LOS_OK;  // 返回成功
}

/* 将系统文件描述符转换为私有数据 */
/**
 * @brief 通过文件描述符获取私有数据
 * @param[in] personal 文件描述符
 * @return 成功返回私有数据指针，失败返回NULL并设置errno
 * @note 验证描述符有效性并返回对应的私有数据
 */
STATIC struct mqpersonal *MqGetPrivDataBuff(mqd_t personal)
{
    INT32 sysFd = (INT32)personal;  // 系统文件描述符
    INT32 id = sysFd - MQUEUE_FD_OFFSET;  // 计算缓冲区索引

    /* 过滤非法索引 */
    if ((id < 0) || (id >= MAX_MQ_FD)) {  // 检查索引范围
        errno = EBADF;  // 设置文件描述符错误
        return NULL;  // 返回NULL
    }
    return IPC_QUEUE_MQ_PRIV_BUF[id];  // 返回私有数据指针
}

/**
 * @brief 分配消息队列系统文件描述符
 * @param[in] maxfdp 最大允许的文件描述符数
 * @param[in] privateMqPersonal 私有数据指针
 * @return 成功返回新分配的文件描述符，失败返回-1
 * @note 在文件描述符集合中查找空闲项并关联私有数据
 */
STATIC INT32 MqAllocSysFd(int maxfdp, struct mqpersonal *privateMqPersonal)
{
    INT32 i;  // 循环索引
    fd_set *fdset = &IPC_QUEUE_FD_SET;  // 文件描述符集合
    for (i = 0; i < maxfdp; i++) {  // 遍历可能的描述符
        /* 检查描述符是否未使用 */
        if (fdset && !(FD_ISSET(i + MQUEUE_FD_OFFSET, fdset))) {
            FD_SET(i + MQUEUE_FD_OFFSET, fdset);  // 标记为已使用
            if (!IPC_QUEUE_MQ_PRIV_BUF[i]) {  // 如果私有数据缓冲区空闲
                IPC_QUEUE_MQ_PRIV_BUF[i] = privateMqPersonal;  // 关联私有数据
                return i + MQUEUE_FD_OFFSET;  // 返回新分配的描述符
            }
        }
    }
    return -1;  // 无可用描述符，返回错误
}

/**
 * @brief 释放消息队列系统文件描述符
 * @param[in] personal 文件描述符
 * @note 清除描述符集合中的标记并解除私有数据关联
 */
STATIC VOID MqFreeSysFd(mqd_t personal)
{
    INT32 sysFd = (INT32)personal;  // 系统文件描述符
    fd_set *fdset = &IPC_QUEUE_FD_SET;  // 文件描述符集合
    if (fdset && FD_ISSET(sysFd, fdset)) {  // 如果描述符已使用
        FD_CLR(sysFd, fdset);  // 清除使用标记
        IPC_QUEUE_MQ_PRIV_BUF[sysFd - MQUEUE_FD_OFFSET] = NULL;  // 解除私有数据关联
    }
}

/* 消息队列文件描述符引用计数 */
/**
 * @brief 增加消息队列文件描述符引用计数
 * @param[in] sysFd 系统文件描述符
 * @note 线程安全地增加对应私有数据的引用计数
 */
void MqueueRefer(int sysFd)
{
    struct mqarray *mqueueCB = NULL;  // 队列控制块指针
    struct mqpersonal *privateMqPersonal = NULL;  // 私有数据指针

    (VOID)pthread_mutex_lock(&IPC_QUEUE_MUTEX);  // 加锁保护
    /* 获取私有数据并重置描述符 */
    privateMqPersonal = MqGetPrivDataBuff((mqd_t)sysFd);  // 获取私有数据
    if (privateMqPersonal == NULL) {  // 私有数据不存在
        goto OUT_UNLOCK;  // 跳转到解锁
    }
    mqueueCB = privateMqPersonal->mq_posixdes;  // 获取队列控制块
    if (mqueueCB == NULL) {  // 控制块不存在
        goto OUT_UNLOCK;  // 跳转到解锁
    }

    privateMqPersonal->mq_refcount++;  // 增加引用计数
OUT_UNLOCK:  // 解锁标签
    (VOID)pthread_mutex_unlock(&IPC_QUEUE_MUTEX);  // 解锁
    return;  // 返回
}

/**
 * @brief 尝试关闭消息队列（引用计数减1）
 * @param[in] privateMqPersonal 私有数据指针
 * @return 引用计数为0返回TRUE，否则返回FALSE并设置errno
 * @note 当引用计数减为0时表示可以关闭队列
 */
STATIC INT32 MqTryClose(struct mqpersonal *privateMqPersonal)
{
    struct mqarray *mqueueCB = NULL;  // 队列控制块指针
    mqueueCB = privateMqPersonal->mq_posixdes;  // 获取队列控制块
    if (mqueueCB == NULL) {  // 控制块不存在
        errno = ENFILE;  // 设置文件数量超出错误
        return false;  // 返回FALSE
    }

    if (privateMqPersonal->mq_refcount == 0) {  // 引用计数为0
        return TRUE;  // 返回TRUE，表示可以关闭
    }
    privateMqPersonal->mq_refcount--;  // 引用计数减1
    return FALSE;  // 返回FALSE
}

/* Set the mode data bit,for consumer's mode comparing. */
/**
 * @brief 分析并设置消息队列的访问权限模式
 * @param privateMqPersonal 指向mqpersonal结构体的指针，包含消息队列的私有信息
 * @return 成功返回0，失败返回-1
 */
STATIC INT32 MqueueModeAnalysisSet(struct mqpersonal *privateMqPersonal)
{
    UINT32 mode;                     // 消息队列的访问权限模式
    UINT32 intSave;                  // 用于保存中断状态
    User *user = NULL;               // 当前用户结构体指针
    struct mqarray *mqueueCB = NULL; // 消息队列控制块指针

    if ((INT32)(UINTPTR)privateMqPersonal < 0) {  // 检查私有信息指针是否有效
        return -1;                               // 无效则返回错误
    }
    /* 获取首次创建消息队列时的控制块 */
    mqueueCB = privateMqPersonal->mq_posixdes;    // 从私有信息中获取消息队列控制块
    if (mqueueCB == NULL) {                       // 检查控制块是否为空
        errno = ENFILE;                           // 设置错误号为文件表溢出
        return -1;                                // 返回错误
    }

    mode = mqueueCB->mq_personal->mq_mode;        // 获取消息队列的访问模式
    /* 设置消息队列的gid和uid */
    SCHEDULER_LOCK(intSave);                      // 关闭调度器，保存中断状态
    user = OsCurrUserGet();                       // 获取当前用户
    mqueueCB->euid = user->effUserID;             // 设置有效用户ID
    mqueueCB->egid = user->effGid;                // 设置有效组ID
    SCHEDULER_UNLOCK(intSave);                    // 恢复调度器

    /* 设置模式数据位 */
    if (mode & S_IRUSR) {                         // 如果用户有读权限
        mqueueCB->mode_data.usr |= S_IRUSR;       // 设置用户读权限位
    }
    if (mode & S_IWUSR) {                         // 如果用户有写权限
        mqueueCB->mode_data.usr |= S_IWUSR;       // 设置用户写权限位
    }
    if (mode & S_IRGRP) {                         // 如果组有读权限
        mqueueCB->mode_data.grp |= S_IRGRP;       // 设置组读权限位
    }
    if (mode & S_IWGRP) {                         // 如果组有写权限
        mqueueCB->mode_data.grp |= S_IWGRP;       // 设置组写权限位
    }
    if (mode & S_IROTH) {                         // 如果其他用户有读权限
        mqueueCB->mode_data.oth |= S_IROTH;       // 设置其他用户读权限位
    }
    if (mode & S_IWOTH) {                         // 如果其他用户有写权限
        mqueueCB->mode_data.oth |= S_IWOTH;       // 设置其他用户写权限位
    }
    return 0;                                     // 成功返回0
}

/**
 * @brief 检查访问者对消息队列的权限
 * @param mqueueCB 指向mqarray结构体的指针，消息队列控制块
 * @return 有权限返回ENOERR，无权限返回-EPERM
 */
STATIC INT32 GetPermissionOfVisitor(struct mqarray *mqueueCB)
{
    uid_t euid;               // 有效用户ID
    gid_t egid;               // 有效组ID
    UINT32 intSave;           // 用于保存中断状态
    User *user = NULL;        // 当前用户结构体指针

    if (mqueueCB == NULL) {   // 检查消息队列控制块是否为空
        errno = ENOENT;       // 设置错误号为文件不存在
        return -EPERM;        // 返回权限错误
    }

    /* 获取访问进程的euid和egid */
    SCHEDULER_LOCK(intSave);  // 关闭调度器，保存中断状态
    user = OsCurrUserGet();   // 获取当前用户
    euid = user->effUserID;   // 获取有效用户ID
    egid = user->effGid;      // 获取有效组ID
    SCHEDULER_UNLOCK(intSave); // 恢复调度器

    /* root用户 */
    if (euid == 0) {          // 如果是root用户
        return ENOERR;        // 直接返回成功
    }
    if (euid == mqueueCB->euid) { /* 检查用户是否为消息队列所有者 */
        if (!((mqueueCB->mode_data.usr & S_IRUSR) || (mqueueCB->mode_data.usr & S_IWUSR))) { // 检查是否有读写权限
            errno = EACCES;  // 设置错误号为权限被拒绝
            goto ERR_OUT;    // 跳转到错误处理
        }
    } else if (egid == mqueueCB->egid) { /* 检查用户组是否匹配 */
        if (!((mqueueCB->mode_data.grp & S_IRGRP) || (mqueueCB->mode_data.grp & S_IWGRP))) { // 检查组是否有读写权限
            errno = EACCES;  // 设置错误号为权限被拒绝
            goto ERR_OUT;    // 跳转到错误处理
        }
    } else { /* 其他用户 */
        if (!((mqueueCB->mode_data.oth & S_IROTH) || (mqueueCB->mode_data.oth & S_IWOTH))) { // 检查其他用户是否有读写权限
            errno = EACCES;  // 设置错误号为权限被拒绝
            goto ERR_OUT;    // 跳转到错误处理
        }
    }
    return ENOERR;            // 有权限，返回成功

ERR_OUT:
    return -EPERM;            // 返回权限错误
}

/**
 * @brief 获取并验证消息队列属性
 * @param defaultAttr 默认属性结构体指针
 * @param attr 用户提供的属性结构体指针
 * @return 成功返回0，失败返回-1
 */
STATIC INT32 GetMqueueAttr(struct mq_attr *defaultAttr, struct mq_attr *attr)
{
    if (attr != NULL) {       // 如果用户提供了属性
        if (LOS_ArchCopyFromUser(defaultAttr, attr, sizeof(struct mq_attr))) { // 从用户空间复制属性
            errno = EFAULT;  // 设置错误号为地址错误
            return -1;       // 返回错误
        }
        // 检查消息队列属性是否合法
        if ((defaultAttr->mq_maxmsg < 0) || (defaultAttr->mq_maxmsg > (long int)USHRT_MAX) ||
            (defaultAttr->mq_msgsize < 0) || (defaultAttr->mq_msgsize > (long int)(USHRT_MAX - sizeof(UINT32)))) {
            errno = EINVAL;  // 设置错误号为无效参数
            return -1;       // 返回错误
        }
    }
    return 0;                 // 成功返回0
}

/**
 * @brief 打开或创建一个消息队列
 * @param mqName 消息队列名称
 * @param openFlag 打开标志
 * @param ... 可变参数，包含mode和attr
 * @return 成功返回消息队列描述符，失败返回(mqd_t)-1
 */
mqd_t mq_open(const char *mqName, int openFlag, ...)
{
    struct mqarray *mqueueCB = NULL;               // 消息队列控制块指针
    struct mqpersonal *privateMqPersonal = (struct mqpersonal *)-1; // 私有信息指针
    struct mq_attr *attr = NULL;                   // 属性结构体指针
    struct mq_attr defaultAttr = { 0, MQ_MAX_MSG_NUM, MQ_MAX_MSG_LEN, 0 }; // 默认属性
    va_list ap;                                    // 可变参数列表
    int sysFd;                                     // 系统文件描述符
    mqd_t mqFd = -1;                               // 消息队列描述符
    unsigned int mode = 0;                         // 访问权限模式

    if (MqNameCheck(mqName) == -1) {               // 检查消息队列名称是否合法
        return (mqd_t)-1;                          // 不合法返回错误
    }

    (VOID)pthread_mutex_lock(&IPC_QUEUE_MUTEX);    // 加互斥锁
    mqueueCB = GetMqueueCBByName(mqName);          // 根据名称获取消息队列控制块
    if ((UINT32)openFlag & (UINT32)O_CREAT) {      // 如果指定了创建标志
        if (mqueueCB != NULL) {                    // 消息队列已存在
            if (((UINT32)openFlag & (UINT32)O_EXCL)) { // 如果同时指定了O_EXCL
                errno = EEXIST;                    // 设置错误号为文件已存在
                goto OUT;                          // 跳转到释放锁
            }
            privateMqPersonal = DoMqueueOpen(mqueueCB, openFlag); // 打开已存在的消息队列
        } else {                                   // 消息队列不存在
            va_start(ap, openFlag);                // 初始化可变参数
            mode = va_arg(ap, unsigned int);       // 获取权限模式
            attr = va_arg(ap, struct mq_attr *);   // 获取属性指针
            va_end(ap);                            // 结束可变参数处理

            if (GetMqueueAttr(&defaultAttr, attr)) { // 获取并验证属性
                goto OUT;                          // 失败跳转到释放锁
            }
            privateMqPersonal = DoMqueueCreate(&defaultAttr, mqName, openFlag, mode); // 创建消息队列
        }
        /* 设置模式数据位，仅对第一个节点 */
        if (MqueueModeAnalysisSet(privateMqPersonal)) { // 分析并设置权限模式
            if ((INT32)(UINTPTR)privateMqPersonal > 0) { // 如果私有信息有效
                (VOID)DoMqueueClose(privateMqPersonal); // 关闭消息队列
            }
            goto OUT;                              // 跳转到释放锁
        }
    } else {                                       // 未指定创建标志
        if (GetPermissionOfVisitor(mqueueCB)) {    // 检查访问权限
            goto OUT;                              // 无权限跳转到释放锁
        }
        privateMqPersonal = DoMqueueOpen(mqueueCB, openFlag); // 打开消息队列
    }

    if ((INT32)(UINTPTR)privateMqPersonal > 0) {   // 如果私有信息有效
        /* 分配系统文件描述符 */
        sysFd = MqAllocSysFd(MAX_MQ_FD, privateMqPersonal); // 分配文件描述符
        if (sysFd == -1) {                         // 分配失败
            /* 没有可用的消息队列文件描述符，关闭私有信息 */
            (VOID)DoMqueueClose(privateMqPersonal); // 关闭消息队列
            errno = ENFILE;                        // 设置错误号为文件表溢出
        }
        mqFd = (mqd_t)sysFd;                       // 设置消息队列描述符
    }
OUT:
    (VOID)pthread_mutex_unlock(&IPC_QUEUE_MUTEX);  // 释放互斥锁
    return mqFd;                                   // 返回消息队列描述符
}

/**
 * @brief 关闭消息队列描述符
 * @param personal 消息队列描述符
 * @return 成功返回0，失败返回-1
 */
int mq_close(mqd_t personal)
{
    INT32 ret = -1;                                // 返回值
    struct mqpersonal *privateMqPersonal = NULL;   // 私有信息指针

    (VOID)pthread_mutex_lock(&IPC_QUEUE_MUTEX);    // 加互斥锁

    /* 获取个人系统文件描述符并重置为-1 */
    privateMqPersonal = MqGetPrivDataBuff(personal); // 获取私有信息
    if (privateMqPersonal == NULL) {               // 私有信息为空
        goto OUT_UNLOCK;                           // 跳转到释放锁
    }

    if (privateMqPersonal->mq_status != MQ_USE_MAGIC) { // 检查消息队列状态
        errno = EBADF;                             // 设置错误号为无效文件描述符
        goto OUT_UNLOCK;                           // 跳转到释放锁
    }

    if (!MqTryClose(privateMqPersonal)) {          // 尝试关闭消息队列
        ret = 0;                                   // 成功关闭
        goto OUT_UNLOCK;                           // 跳转到释放锁
    }

    ret = DoMqueueClose(privateMqPersonal);        // 执行关闭操作
    if (ret < 0) {                                 // 关闭失败
        goto OUT_UNLOCK;                           // 跳转到释放锁
    }
    MqFreeSysFd(personal);                         // 释放文件描述符

OUT_UNLOCK:
    (VOID)pthread_mutex_unlock(&IPC_QUEUE_MUTEX);  // 释放互斥锁
    return ret;                                    // 返回结果
}

/**
 * @brief 获取消息队列属性
 * @param personal 消息队列描述符
 * @param mqAttr 属性结构体指针，用于存储获取到的属性
 * @return 成功返回0，失败返回-1
 */
int OsMqGetAttr(mqd_t personal, struct mq_attr *mqAttr)
{
    struct mqarray *mqueueCB = NULL;               // 消息队列控制块指针
    struct mqpersonal *privateMqPersonal = NULL;   // 私有信息指针

    (VOID)pthread_mutex_lock(&IPC_QUEUE_MUTEX);    // 加互斥锁
    privateMqPersonal = MqGetPrivDataBuff(personal); // 获取私有信息
    if (privateMqPersonal == NULL) {               // 私有信息为空
        (VOID)pthread_mutex_unlock(&IPC_QUEUE_MUTEX); // 释放互斥锁
        return -1;                                 // 返回错误
    }

    if (mqAttr == NULL) {                          // 属性指针为空
        errno = EINVAL;                            // 设置错误号为无效参数
        (VOID)pthread_mutex_unlock(&IPC_QUEUE_MUTEX); // 释放互斥锁
        return -1;                                 // 返回错误
    }

    if (privateMqPersonal->mq_status != MQ_USE_MAGIC) { // 检查消息队列状态
        errno = EBADF;                             // 设置错误号为无效文件描述符
        (VOID)pthread_mutex_unlock(&IPC_QUEUE_MUTEX); // 释放互斥锁
        return -1;                                 // 返回错误
    }

    mqueueCB = privateMqPersonal->mq_posixdes;     // 获取消息队列控制块
    mqAttr->mq_maxmsg = mqueueCB->mqcb->queueLen;  // 设置最大消息数
    mqAttr->mq_msgsize = mqueueCB->mqcb->queueSize - sizeof(UINT32); // 设置消息大小
    mqAttr->mq_curmsgs = mqueueCB->mqcb->readWriteableCnt[OS_QUEUE_READ]; // 设置当前消息数
    mqAttr->mq_flags = privateMqPersonal->mq_flags; // 设置标志
    (VOID)pthread_mutex_unlock(&IPC_QUEUE_MUTEX);  // 释放互斥锁
    return 0;                                      // 成功返回0
}

/**
 * @brief 设置消息队列属性
 * @param personal 消息队列描述符
 * @param mqSetAttr 要设置的属性结构体指针
 * @param mqOldAttr 用于存储旧属性的结构体指针
 * @return 成功返回0，失败返回-1
 */
int OsMqSetAttr(mqd_t personal, const struct mq_attr *mqSetAttr, struct mq_attr *mqOldAttr)
{
    struct mqpersonal *privateMqPersonal = NULL;   // 私有信息指针

    (VOID)pthread_mutex_lock(&IPC_QUEUE_MUTEX);    // 加互斥锁
    privateMqPersonal = MqGetPrivDataBuff(personal); // 获取私有信息
    if (privateMqPersonal == NULL) {               // 私有信息为空
        (VOID)pthread_mutex_unlock(&IPC_QUEUE_MUTEX); // 释放互斥锁
        return -1;                                 // 返回错误
    }

    if (mqSetAttr == NULL) {                       // 要设置的属性为空
        errno = EINVAL;                            // 设置错误号为无效参数
        (VOID)pthread_mutex_unlock(&IPC_QUEUE_MUTEX); // 释放互斥锁
        return -1;                                 // 返回错误
    }

    if (privateMqPersonal->mq_status != MQ_USE_MAGIC) { // 检查消息队列状态
        errno = EBADF;                             // 设置错误号为无效文件描述符
        (VOID)pthread_mutex_unlock(&IPC_QUEUE_MUTEX); // 释放互斥锁
        return -1;                                 // 返回错误
    }

    if (mqOldAttr != NULL) {                       // 如果需要获取旧属性
        (VOID)OsMqGetAttr(personal, mqOldAttr);    // 获取旧属性
    }

    // 清除FNONBLOCK标志
    privateMqPersonal->mq_flags = (INT32)((UINT32)privateMqPersonal->mq_flags & (UINT32)(~FNONBLOCK)); 
    if (((UINT32)mqSetAttr->mq_flags & (UINT32)FNONBLOCK) == (UINT32)FNONBLOCK) { // 如果设置了非阻塞标志
        privateMqPersonal->mq_flags = (INT32)((UINT32)privateMqPersonal->mq_flags | (UINT32)FNONBLOCK); // 设置非阻塞标志
    }
    (VOID)pthread_mutex_unlock(&IPC_QUEUE_MUTEX);  // 释放互斥锁
    return 0;                                      // 成功返回0
}

/**
 * @brief 获取或设置消息队列属性
 * @param mqd 消息队列描述符
 * @param new 要设置的新属性，为NULL则仅获取属性
 * @param old 用于存储旧属性的指针
 * @return 成功返回0，失败返回-1
 */
int mq_getsetattr(mqd_t mqd, const struct mq_attr *new, struct mq_attr *old)
{
    if (new == NULL) {                             // 如果new为NULL
        return OsMqGetAttr(mqd, old);              // 仅获取属性
    }
    return OsMqSetAttr(mqd, new, old);             // 设置并获取属性
}

/**
 * @brief 删除消息队列名称
 * @param mqName 要删除的消息队列名称
 * @return 成功返回0，失败返回-1
 */
int mq_unlink(const char *mqName)
{
    INT32 ret = 0;                                 // 返回值
    struct mqarray *mqueueCB = NULL;               // 消息队列控制块指针

    if (MqNameCheck(mqName) == -1) {               // 检查名称是否合法
        return -1;                                 // 不合法返回错误
    }

    (VOID)pthread_mutex_lock(&IPC_QUEUE_MUTEX);    // 加互斥锁
    mqueueCB = GetMqueueCBByName(mqName);          // 根据名称获取控制块
    if (mqueueCB == NULL) {                        // 控制块不存在
        errno = ENOENT;                            // 设置错误号为文件不存在
        goto ERROUT_UNLOCK;                        // 跳转到释放锁
    }

    if (mqueueCB->mq_personal != NULL) {           // 如果有进程打开该消息队列
        mqueueCB->unlinkflag = TRUE;               // 设置删除标志
    } else if (mqueueCB->unlink_ref == 0) {        // 如果没有引用
        ret = DoMqueueDelete(mqueueCB);            // 执行删除操作
    }

    (VOID)pthread_mutex_unlock(&IPC_QUEUE_MUTEX);  // 释放互斥锁
    return ret;                                    // 返回结果

ERROUT_UNLOCK:
    (VOID)pthread_mutex_unlock(&IPC_QUEUE_MUTEX);  // 释放互斥锁
    return -1;                                     // 返回错误
}

/**
 * @brief 将超时时间转换为系统滴答数
 * @param flags 打开标志
 * @param absTimeout 绝对超时时间
 * @param ticks 用于存储转换后的滴答数
 * @return 成功返回0，失败返回-1
 */
STATIC INT32 ConvertTimeout(long flags, const struct timespec *absTimeout, UINT64 *ticks)
{
    if ((UINT32)flags & (UINT32)FNONBLOCK) {       // 如果是非阻塞模式
        *ticks = LOS_NO_WAIT;                      // 设置为不等待
        return 0;                                  // 返回成功
    }

    if (absTimeout == NULL) {                      // 如果超时时间为NULL
        *ticks = LOS_WAIT_FOREVER;                 // 设置为永久等待
        return 0;                                  // 返回成功
    }

    if (!ValidTimeSpec(absTimeout)) {              // 检查时间格式是否有效
        errno = EINVAL;                            // 设置错误号为无效参数
        return -1;                                 // 返回错误
    }

    *ticks = OsTimeSpec2Tick(absTimeout);          // 将timespec转换为滴答数
    return 0;                                      // 返回成功
}

/**
 * @brief 检查消息发送参数的有效性
 * @param personal 消息队列描述符
 * @param msg 消息内容指针
 * @param msgLen 消息长度
 * @return 有效返回TRUE，无效返回FALSE
 */
STATIC INLINE BOOL MqParamCheck(mqd_t personal, const char *msg, size_t msgLen)
{
    if (personal < 0) {                            // 描述符无效
        return FALSE;                              // 返回无效
    }

    if ((msg == NULL) || (msgLen == 0)) {          // 消息为空或长度为0
        errno = EINVAL;                            // 设置错误号为无效参数
        return FALSE;                              // 返回无效
    }
    return TRUE;                                   // 参数有效
}

/*
 * 向已通过mq_notify注册的进程发送实时信号
 */
static void MqSendNotify(struct mqarray *mqueueCB)
{
    struct mqnotify *mqnotify = &mqueueCB->mq_notify; // 通知结构体指针

    // 如果已注册进程ID且消息队列为空
    if ((mqnotify->pid) && (mqueueCB->mqcb->readWriteableCnt[OS_QUEUE_READ] == 0)) {
        siginfo_t info;                            // 信号信息结构体

        switch (mqnotify->notify.sigev_notify) {   // 根据通知类型处理
            case SIGEV_SIGNAL:                     // 信号通知
                /* 发送信号 */
                /* 创建siginfo结构体 */
                info.si_signo = mqnotify->notify.sigev_signo; // 设置信号号
                info.si_code = SI_MESGQ;           // 设置信号代码
                info.si_value = mqnotify->notify.sigev_value; // 设置信号值
                OsDispatch(mqnotify->pid, &info, OS_USER_KILL_PERMISSION); // 发送信号
                break;
            case SIGEV_NONE:                       // 无通知
            default:                               // 默认情况
                break;
        }
        /* 通知后注销进程 */
        mqnotify->pid = 0;                         // 重置进程ID
    }
}

#define OS_MQ_GOTO_ERROUT_UNLOCK_IF(expr, errcode) \
    if (expr) {                        \
        errno = errcode;                 \
        goto ERROUT_UNLOCK;                     \
    }
#define OS_MQ_GOTO_ERROUT_IF(expr, errcode) \
    if (expr) {                        \
        errno = errcode;                 \
        goto ERROUT;                     \
    }
/**
 * @brief 在指定超时时间内发送消息到消息队列
 * @param personal 消息队列描述符
 * @param msg 消息内容指针
 * @param msgLen 消息长度
 * @param msgPrio 消息优先级
 * @param absTimeout 绝对超时时间
 * @return 成功返回0，失败返回-1
 */
int mq_timedsend(mqd_t personal, const char *msg, size_t msgLen, unsigned int msgPrio,
                 const struct timespec *absTimeout)
{
    UINT32 mqueueID, err;                          // 消息队列ID和错误码
    UINT64 absTicks;                               // 超时滴答数
    struct mqarray *mqueueCB = NULL;               // 消息队列控制块指针
    struct mqpersonal *privateMqPersonal = NULL;   // 私有信息指针

    OS_MQ_GOTO_ERROUT_IF(!MqParamCheck(personal, msg, msgLen), errno); // 检查参数
    OS_MQ_GOTO_ERROUT_IF(msgPrio > (MQ_PRIO_MAX - 1), EINVAL); // 检查优先级

    (VOID)pthread_mutex_lock(&IPC_QUEUE_MUTEX);    // 加互斥锁
    privateMqPersonal = MqGetPrivDataBuff(personal); // 获取私有信息

    // 检查私有信息和状态
    OS_MQ_GOTO_ERROUT_UNLOCK_IF(privateMqPersonal == NULL || privateMqPersonal->mq_status != MQ_USE_MAGIC, EBADF);

    mqueueCB = privateMqPersonal->mq_posixdes;     // 获取消息队列控制块
    // 检查消息长度是否超过限制
    OS_MQ_GOTO_ERROUT_UNLOCK_IF(msgLen > (size_t)(mqueueCB->mqcb->queueSize - sizeof(UINT32)), EMSGSIZE);

    // 检查是否有写权限
    OS_MQ_GOTO_ERROUT_UNLOCK_IF((((UINT32)privateMqPersonal->mq_flags & (UINT32)O_WRONLY) != (UINT32)O_WRONLY) &&
                                (((UINT32)privateMqPersonal->mq_flags & (UINT32)O_RDWR) != (UINT32)O_RDWR),
                                EBADF);

    // 转换超时时间
    OS_MQ_GOTO_ERROUT_UNLOCK_IF(ConvertTimeout(privateMqPersonal->mq_flags, absTimeout, &absTicks) == -1, errno);
    mqueueID = mqueueCB->mq_id;                    // 获取消息队列ID
    (VOID)pthread_mutex_unlock(&IPC_QUEUE_MUTEX);  // 释放互斥锁

    if (LOS_ListEmpty(&mqueueCB->mqcb->readWriteList[OS_QUEUE_READ])) { // 如果读列表为空
        MqSendNotify(mqueueCB);                    // 发送通知
    }
    err = LOS_QueueWriteCopy(mqueueID, (VOID *)msg, (UINT32)msgLen, (UINT32)absTicks);  // 写入消息到队列
    if (map_errno(err) != ENOERR) {  // 检查是否写入成功
        goto ERROUT;                 // 失败则跳转到错误处理
    }
    return 0;                        // 成功返回0
ERROUT_UNLOCK:
    (VOID)pthread_mutex_unlock(&IPC_QUEUE_MUTEX);  // 释放互斥锁
ERROUT:
    return -1;                       // 返回错误
}

/**
 * @brief 在指定超时时间内从消息队列接收消息
 * @param personal 消息队列描述符
 * @param msg 用于存储接收消息的缓冲区
 * @param msgLen 缓冲区长度
 * @param msgPrio 用于存储消息优先级的指针
 * @param absTimeout 绝对超时时间
 * @return 成功返回接收的消息长度，失败返回-1
 */
ssize_t mq_timedreceive(mqd_t personal, char *msg, size_t msgLen, unsigned int *msgPrio,
                        const struct timespec *absTimeout)
{
    UINT32 mqueueID, err;                     // 消息队列ID和错误码
    UINT32 receiveLen;                        // 接收消息的长度
    UINT64 absTicks;                          // 超时滴答数
    struct mqarray *mqueueCB = NULL;          // 消息队列控制块指针
    struct mqpersonal *privateMqPersonal = NULL; // 私有信息指针

    if (!MqParamCheck(personal, msg, msgLen)) { // 检查参数有效性
        goto ERROUT;                          // 参数无效跳转到错误处理
    }

    if (msgPrio != NULL) {                    // 如果优先级指针有效
        *msgPrio = 0;                         // 初始化优先级为0
    }

    (VOID)pthread_mutex_lock(&IPC_QUEUE_MUTEX); // 加互斥锁
    privateMqPersonal = MqGetPrivDataBuff(personal); // 获取私有信息
    if (privateMqPersonal == NULL || privateMqPersonal->mq_status != MQ_USE_MAGIC) { // 检查私有信息和状态
        errno = EBADF;                        // 设置错误号为无效文件描述符
        goto ERROUT_UNLOCK;                   // 跳转到释放锁
    }

    mqueueCB = privateMqPersonal->mq_posixdes; // 获取消息队列控制块
    if (msgLen < (size_t)(mqueueCB->mqcb->queueSize - sizeof(UINT32))) { // 检查缓冲区大小
        errno = EMSGSIZE;                     // 设置错误号为消息大小不合适
        goto ERROUT_UNLOCK;                   // 跳转到释放锁
    }

    if (((UINT32)privateMqPersonal->mq_flags & (UINT32)O_WRONLY) == (UINT32)O_WRONLY) { // 检查是否有读权限
        errno = EBADF;                        // 设置错误号为无效文件描述符
        goto ERROUT_UNLOCK;                   // 跳转到释放锁
    }

    if (ConvertTimeout(privateMqPersonal->mq_flags, absTimeout, &absTicks) == -1) { // 转换超时时间
        goto ERROUT_UNLOCK;                   // 转换失败跳转到释放锁
    }

    receiveLen = msgLen;                      // 设置接收长度
    mqueueID = mqueueCB->mq_id;               // 获取消息队列ID
    (VOID)pthread_mutex_unlock(&IPC_QUEUE_MUTEX); // 释放互斥锁

    err = LOS_QueueReadCopy(mqueueID, (VOID *)msg, &receiveLen, (UINT32)absTicks); // 从队列读取消息
    if (map_errno(err) == ENOERR) {           // 检查读取是否成功
        return (ssize_t)receiveLen;           // 成功返回接收长度
    } else {
        goto ERROUT;                          // 失败跳转到错误处理
    }

ERROUT_UNLOCK:
    (VOID)pthread_mutex_unlock(&IPC_QUEUE_MUTEX); // 释放互斥锁
ERROUT:
    return -1;                                // 返回错误
}

/* 不支持优先级 */
/**
 * @brief 发送消息到消息队列（不支持超时）
 * @param personal 消息队列描述符
 * @param msg_ptr 消息内容指针
 * @param msg_len 消息长度
 * @param msg_prio 消息优先级（未使用）
 * @return 成功返回0，失败返回-1
 */
int mq_send(mqd_t personal, const char *msg_ptr, size_t msg_len, unsigned int msg_prio)
{
    return mq_timedsend(personal, msg_ptr, msg_len, msg_prio, NULL); // 调用带超时的发送函数，超时设为NULL
}

/**
 * @brief 从消息队列接收消息（不支持超时）
 * @param personal 消息队列描述符
 * @param msg_ptr 用于存储消息的缓冲区指针
 * @param msg_len 缓冲区长度
 * @param msg_prio 用于存储消息优先级的指针
 * @return 成功返回接收的消息长度，失败返回-1
 */
ssize_t mq_receive(mqd_t personal, char *msg_ptr, size_t msg_len, unsigned int *msg_prio)
{
    return mq_timedreceive(personal, msg_ptr, msg_len, msg_prio, NULL); // 调用带超时的接收函数，超时设为NULL
}

/**
 * @brief 检查消息队列通知参数的有效性
 * @param personal 消息队列描述符
 * @param sigev 信号事件结构体指针
 * @return 有效返回TRUE，无效返回FALSE
 */
STATIC INLINE BOOL MqNotifyParamCheck(mqd_t personal, const struct sigevent *sigev)
{
    if (personal < 0) {                       // 描述符无效
        errno = EBADF;                        // 设置错误号为无效文件描述符
        goto ERROUT;                          // 跳转到错误处理
    }

    if (sigev != NULL) {                      // 如果信号事件指针有效
        // 检查通知类型是否支持
        if (sigev->sigev_notify != SIGEV_NONE && sigev->sigev_notify != SIGEV_SIGNAL) {
            errno = EINVAL;                   // 设置错误号为无效参数
            goto ERROUT;                      // 跳转到错误处理
        }
        // 检查信号号是否有效
        if (sigev->sigev_notify == SIGEV_SIGNAL && !GOOD_SIGNO(sigev->sigev_signo)) {
            errno = EINVAL;                   // 设置错误号为无效参数
            goto ERROUT;                      // 跳转到错误处理
        }
    }

    return TRUE;                              // 参数有效
ERROUT:
    return FALSE;                             // 参数无效
}

/**
 * @brief 设置或移除消息队列通知
 * @param personal 消息队列描述符
 * @param sigev 信号事件结构体指针，为NULL则移除通知
 * @return 成功返回0，失败返回-1
 */
int OsMqNotify(mqd_t personal, const struct sigevent *sigev)
{
    struct mqarray *mqueueCB = NULL;          // 消息队列控制块指针
    struct mqnotify *mqnotify = NULL;         // 通知结构体指针
    struct mqpersonal *privateMqPersonal = NULL; // 私有信息指针

    if (!MqNotifyParamCheck(personal, sigev)) { // 检查参数有效性
        goto ERROUT;                          // 参数无效跳转到错误处理
    }

    (VOID)pthread_mutex_lock(&IPC_QUEUE_MUTEX); // 加互斥锁
    privateMqPersonal = MqGetPrivDataBuff(personal); // 获取私有信息
    if (privateMqPersonal == NULL) {          // 私有信息为空
        goto OUT_UNLOCK;                      // 跳转到释放锁
    }

    if (privateMqPersonal->mq_status != MQ_USE_MAGIC) { // 检查消息队列状态
        errno = EBADF;                        // 设置错误号为无效文件描述符
        goto OUT_UNLOCK;                      // 跳转到释放锁
    }

    mqueueCB = privateMqPersonal->mq_posixdes; // 获取消息队列控制块
    mqnotify = &mqueueCB->mq_notify;          // 获取通知结构体

    if (sigev == NULL) {                      // 如果sigev为NULL，表示移除通知
        if (mqnotify->pid == LOS_GetCurrProcessID()) { // 如果是当前进程注册的通知
            mqnotify->pid = 0;                // 重置进程ID
        }
    } else if (mqnotify->pid != 0) {          // 如果已有进程注册了通知
        errno = EBUSY;                        // 设置错误号为资源忙
        goto OUT_UNLOCK;                      // 跳转到释放锁
    } else {                                  // 设置新的通知
        switch (sigev->sigev_notify) {        // 根据通知类型处理
            case SIGEV_NONE:                  // 无通知
                mqnotify->notify.sigev_notify = SIGEV_NONE;
                break;
            case SIGEV_SIGNAL:                // 信号通知
                mqnotify->notify.sigev_signo = sigev->sigev_signo; // 设置信号号
                mqnotify->notify.sigev_value = sigev->sigev_value; // 设置信号值
                mqnotify->notify.sigev_notify = SIGEV_SIGNAL;      // 设置通知类型
                break;
            default:
                break;
        }

        mqnotify->pid = LOS_GetCurrProcessID(); // 设置当前进程ID为通知进程
    }

    (VOID)pthread_mutex_unlock(&IPC_QUEUE_MUTEX); // 释放互斥锁
    return 0;                                 // 成功返回0
OUT_UNLOCK:
    (VOID)pthread_mutex_unlock(&IPC_QUEUE_MUTEX); // 释放互斥锁
ERROUT:
    return -1;                                // 返回错误
}

/**
 * @brief 销毁消息队列控制块表
 * @param queueTable 消息队列控制块表指针
 */
VOID OsMqueueCBDestroy(struct mqarray *queueTable)
{
    if (queueTable == NULL) {                 // 控制块表为空
        return;                               // 直接返回
    }

    // 遍历所有消息队列控制块
    for (UINT32 index = 0; index < LOSCFG_BASE_IPC_QUEUE_LIMIT; index++) {
        struct mqarray *mqueueCB = &(queueTable[index]); // 获取控制块
        if (mqueueCB->mq_name == NULL) {      // 如果名称为空，表示未使用
            continue;                         // 跳过
        }
        (VOID)DoMqueueClose(mqueueCB->mq_personal); // 关闭消息队列
    }
}
#endif
