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

#include "los_mux.h"
#include "los_queue.h"
#include "los_sem.h"
#include "los_task.h"

/**
 * @brief   将内核错误码映射为POSIX标准错误码
 * @param   err 内核错误码(LOS_ERRNO系列)
 * @return  对应的POSIX标准错误码
 * @note    映射结果同时会设置全局errno变量
 */
int map_errno(UINT32 err)
{
    // 如果内核错误码为成功，则返回ENOERR(0)
    if (err == LOS_OK) {
        return ENOERR;
    }
    
    // 根据不同的内核错误码进行映射
    switch (err) {
        // 队列相关无效参数错误 -> EINVAL(无效参数)
        case LOS_ERRNO_QUEUE_INVALID:
        case LOS_ERRNO_QUEUE_WRITE_PTR_NULL:
        case LOS_ERRNO_QUEUE_WRITESIZE_ISZERO:
        case LOS_ERRNO_QUEUE_SIZE_TOO_BIG:
        case LOS_ERRNO_QUEUE_CREAT_PTR_NULL:
        case LOS_ERRNO_QUEUE_PARA_ISZERO:
        case LOS_ERRNO_QUEUE_WRITE_SIZE_TOO_BIG:
            errno = EINVAL;  // 设置POSIX错误码为无效参数
            break;
        
        // 队列满/空错误 -> EAGAIN(资源暂时不可用)
        case LOS_ERRNO_QUEUE_ISFULL:
        case LOS_ERRNO_QUEUE_ISEMPTY:
            errno = EAGAIN;  // 设置POSIX错误码为资源暂时不可用
            break;
        
        // 队列创建内存不足 -> ENOSPC(设备上没有空间)
        case LOS_ERRNO_QUEUE_CREATE_NO_MEMORY:
            errno = ENOSPC;  // 设置POSIX错误码为设备空间不足
            break;
        
        // 队列操作超时 -> ETIMEDOUT(连接超时)
        case LOS_ERRNO_QUEUE_TIMEOUT:
            errno = ETIMEDOUT;  // 设置POSIX错误码为超时
            break;
        
        // 队列控制块不可用 -> ENFILE(已达到打开文件数上限)
        case LOS_ERRNO_QUEUE_CB_UNAVAILABLE:
            errno = ENFILE;  // 设置POSIX错误码为打开文件过多
            break;
        
        // 中断中读写队列 -> EINTR(函数调用被中断)
        case LOS_ERRNO_QUEUE_READ_IN_INTERRUPT:
        case LOS_ERRNO_QUEUE_WRITE_IN_INTERRUPT:
            errno = EINTR;  // 设置POSIX错误码为中断
            break;
        
        // 任务/信号量相关无效参数错误 -> EINVAL(无效参数)
        case LOS_ERRNO_TSK_ID_INVALID:
        case LOS_ERRNO_TSK_PTR_NULL:
        case LOS_ERRNO_TSK_NAME_EMPTY:
        case LOS_ERRNO_TSK_ENTRY_NULL:
        case LOS_ERRNO_TSK_PRIOR_ERROR:
        case LOS_ERRNO_TSK_STKSZ_TOO_LARGE:
        case LOS_ERRNO_TSK_STKSZ_TOO_SMALL:
        case LOS_ERRNO_TSK_NOT_CREATED:
        case LOS_ERRNO_TSK_CPU_AFFINITY_MASK_ERR:
        case OS_ERROR:
        case LOS_ERRNO_SEM_INVALID:
        case LOS_ERRNO_SEM_UNAVAILABLE:
            errno = EINVAL;  // 设置POSIX错误码为无效参数
            break;
        
        // 任务/信号量资源不可用 -> ENOSPC(设备上没有空间)
        case LOS_ERRNO_TSK_TCB_UNAVAILABLE:
        case LOS_ERRNO_TSK_MP_SYNC_RESOURCE:
        case LOS_ERRNO_SEM_ALL_BUSY:
            errno = ENOSPC;  // 设置POSIX错误码为设备空间不足
            break;
        
        // 内存分配失败 -> ENOMEM(内存不足)
        case LOS_ERRNO_TSK_NO_MEMORY:
        case LOS_ERRNO_SEM_OVERFLOW:
            errno = ENOMEM;  // 设置POSIX错误码为内存不足
            break;
        
        // 信号量/事件忙 -> EBUSY(设备或资源忙)
        case LOS_ERRNO_SEM_PENDED:
        case LOS_ERRNO_EVENT_SHOULD_NOT_DESTROY:
            errno = EBUSY;  // 设置POSIX错误码为资源忙
            break;
        
        // 信号量在锁中等待 -> EPERM(操作不允许)
        case LOS_ERRNO_SEM_PEND_IN_LOCK:
            errno = EPERM;  // 设置POSIX错误码为权限不足
            break;
        
        // 信号量等待被中断 -> EINTR(函数调用被中断)
        case LOS_ERRNO_SEM_PEND_INTERR:
            errno = EINTR;  // 设置POSIX错误码为中断
            break;
        
        // 信号量等待超时 -> ETIMEDOUT(连接超时)
        case LOS_ERRNO_SEM_TIMEOUT:
            errno = ETIMEDOUT;  // 设置POSIX错误码为超时
            break;
        
        // 默认情况 -> EINVAL(无效参数)
        default:
            errno = EINVAL;  // 设置POSIX错误码为无效参数
            break;
    }
    return errno;  // 返回映射后的POSIX错误码
}

