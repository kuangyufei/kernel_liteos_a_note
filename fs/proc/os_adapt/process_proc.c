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

#include <sys/statfs.h>
#include <sys/mount.h>
#include "proc_fs.h"
#include "internal.h"
#include "los_process_pri.h"
#include "user_copy.h"
#include "los_memory.h"

#ifdef LOSCFG_PROC_PROCESS_DIR
#include "los_vm_dump.h"
// 进程数据类型枚举，定义不同的proc文件类型
typedef enum {
    PROC_PID,               // 进程ID节点
    PROC_PID_MEM,           // 进程内存信息节点
#ifdef LOSCFG_KERNEL_CPUP
    PROC_PID_CPUP,          // 进程CPU使用情况节点（仅启用CPUP时）
#endif
#ifdef LOSCFG_USER_CONTAINER
    PROC_UID_MAP,           // UID映射节点（仅启用用户容器时）
    PROC_GID_MAP,           // GID映射节点（仅启用用户容器时）
#endif
    PROC_P_TYPE_MAX,        // 进程数据类型最大值
} ProcessDataType;

// Proc文件系统进程相关结构体，定义proc节点属性
struct ProcProcess {
    char         *name;     // 节点名称
    mode_t       mode;      // 文件权限模式
    int          type;      // 节点类型（对应ProcessDataType）
    const struct ProcFileOperations *fileOps;  // 文件操作函数集
};

// 进程数据结构体，存储进程相关信息
struct ProcessData {
    uintptr_t process;      // 进程指针（0表示当前进程）
    unsigned int type;      // 数据类型（ProcessDataType）
};

/**
 * @brief 获取进程控制块(PCB)
 * @param data 进程数据结构体指针
 * @return 成功返回进程PCB指针，失败返回NULL
 */
static LosProcessCB *ProcGetProcessCB(struct ProcessData *data)
{
    if (data->process != 0) {                  // 如果指定了进程指针
        return (LosProcessCB *)data->process;  // 返回指定进程的PCB
    }
    return OsCurrProcessGet();                 // 返回当前进程的PCB
}

#define PROC_PID_PRIVILEGE 7                   // PID节点权限
#define PROC_PID_DIR_LEN 100                   // PID目录长度

#ifdef LOSCFG_KERNEL_CONTAINER                 // 如果启用内核容器功能
/**
 * @brief 生成容器链接字符串
 * @param containerID 容器ID
 * @param type 容器类型
 * @param buffer 输出缓冲区
 * @param bufLen 缓冲区长度
 * @return 成功返回字符串长度，失败返回错误码
 */
static ssize_t ProcessContainerLink(unsigned int containerID, ContainerType type, char *buffer, size_t bufLen)
{
    ssize_t count = -1;                        // 字符串长度计数
    if ((type == PID_CONTAINER) || (type == PID_CHILD_CONTAINER)) {  // PID容器类型
        count = snprintf_s(buffer, bufLen, bufLen - 1, "'pid:[%u]'", containerID);
    } else if (type == UTS_CONTAINER) {        // UTS容器类型
        count = snprintf_s(buffer, bufLen, bufLen - 1, "'uts:[%u]'", containerID);
    } else if (type == MNT_CONTAINER) {        // 挂载容器类型
        count = snprintf_s(buffer, bufLen, bufLen - 1, "'mnt:[%u]'", containerID);
    } else if (type == IPC_CONTAINER) {        // IPC容器类型
        count = snprintf_s(buffer, bufLen, bufLen - 1, "'ipc:[%u]'", containerID);
    } else if ((type == TIME_CONTAINER) || (type == TIME_CHILD_CONTAINER)) {  // 时间容器类型
        count = snprintf_s(buffer, bufLen, bufLen - 1, "'time:[%u]'", containerID);
    } else if (type == USER_CONTAINER) {       // 用户容器类型
        count = snprintf_s(buffer, bufLen, bufLen - 1, "'user:[%u]'", containerID);
    }  else if (type == NET_CONTAINER) {       // 网络容器类型
        count = snprintf_s(buffer, bufLen, bufLen - 1, "'net:[%u]'", containerID);
    }

    if (count < 0) {                           // 字符串生成失败
        return -EBADF;                         // 返回文件描述符错误
    }
    return count;                              // 返回字符串长度
}

/**
 * @brief 读取容器链接信息
 * @param entry Proc目录项指针
 * @param buffer 输出缓冲区
 * @param bufLen 缓冲区长度
 * @return 成功返回读取长度，失败返回错误码
 */
static ssize_t ProcessContainerReadLink(struct ProcDirEntry *entry, char *buffer, size_t bufLen)
{
    char *freeBuf = NULL;                      // 临时缓冲区（需要释放）
    char *buf = buffer;                        // 工作缓冲区
    ssize_t count;                             // 读取长度
    unsigned int intSave;                      // 中断标志
    if (entry == NULL) {                       // 目录项无效
        return -EINVAL;                        // 返回无效参数错误
    }
    struct ProcessData *data = (struct ProcessData *)entry->data;  // 获取进程数据
    if (data == NULL) {                        // 进程数据无效
        return -EINVAL;                        // 返回无效参数错误
    }

    if (LOS_IsUserAddressRange((VADDR_T)(UINTPTR)buffer, bufLen)) {  // 如果缓冲区在用户空间
        buf = LOS_MemAlloc(m_aucSysMem1, bufLen);                    // 分配内核缓冲区
        if (buf == NULL) {                                           // 分配失败
            return -ENOMEM;                                          // 返回内存不足错误
        }
        (void)memset_s(buf, bufLen, 0, bufLen);                      // 清空缓冲区
        freeBuf = buf;                                               // 记录需要释放的缓冲区
    }

    LosProcessCB *processCB = ProcGetProcessCB(data);  // 获取进程PCB
    SCHEDULER_LOCK(intSave);                           // 关调度器
    UINT32 containerID = OsGetContainerID(processCB, (ContainerType)data->type);  // 获取容器ID
    SCHEDULER_UNLOCK(intSave);                         // 开调度器
    if (containerID != OS_INVALID_VALUE) {             // 容器ID有效
        count = ProcessContainerLink(containerID, (ContainerType)data->type, buf, bufLen);  // 生成链接字符串
    } else {                                           // 容器ID无效
        count = strlen("(unknown)");                  // 未知容器
        if (memcpy_s(buf, bufLen, "(unknown)", count + 1) != EOK) {  // 复制字符串
            (void)LOS_MemFree(m_aucSysMem1, freeBuf);  // 释放缓冲区
            return -EBADF;                             // 返回文件错误
        }
    }
    if (count < 0) {                                   // 处理失败
        (void)LOS_MemFree(m_aucSysMem1, freeBuf);      // 释放缓冲区
        return count;                                  // 返回错误码
    }

    if (LOS_IsUserAddressRange((VADDR_T)(UINTPTR)buffer, bufLen)) {  // 如果缓冲区在用户空间
        if (LOS_ArchCopyToUser(buffer, buf, bufLen) != 0) {          // 复制到用户空间
            (void)LOS_MemFree(m_aucSysMem1, freeBuf);                // 释放缓冲区
            return -EFAULT;                                          // 返回内存访问错误
        }
    }
    (void)LOS_MemFree(m_aucSysMem1, freeBuf);          // 释放临时缓冲区
    return count;                                      // 返回读取长度
}

// 容器链接文件操作结构体
static const struct ProcFileOperations PID_CONTAINER_FOPS = {
    .readLink = ProcessContainerReadLink,              // 读取链接操作
};

/**
 * @brief 通过文件描述符获取容器信息
 * @param fd 文件描述符
 * @param containerType 输出参数，容器类型
 * @return 成功返回进程PCB指针，失败返回NULL
 */
void *ProcfsContainerGet(int fd, unsigned int *containerType)
{
    if ((fd <= 0) || (containerType == NULL)) {        // 参数无效
        return NULL;                                   // 返回NULL
    }

    VnodeHold();                                       // 持有Vnode锁
    struct Vnode *vnode = VnodeFind(fd);               // 通过fd查找Vnode
    if (vnode == NULL) {                               // Vnode不存在
        VnodeDrop();                                   // 释放Vnode锁
        return NULL;                                   // 返回NULL
    }

    struct ProcDirEntry *entry = VnodeToEntry(vnode);  // Vnode转换为ProcDirEntry
    if (entry == NULL) {                               // 转换失败
        VnodeDrop();                                   // 释放Vnode锁
        return NULL;                                   // 返回NULL
    }

    struct ProcessData *data = (struct ProcessData *)entry->data;  // 获取进程数据
    if (data == NULL) {                                // 进程数据无效
        VnodeDrop();                                   // 释放Vnode锁
        return NULL;                                   // 返回NULL
    }

    void *processCB = (void *)ProcGetProcessCB(data);  // 获取进程PCB
    *containerType = data->type;                       // 设置容器类型
    VnodeDrop();                                       // 释放Vnode锁
    return processCB;                                  // 返回进程PCB
}

#endif /* LOSCFG_KERNEL_CONTAINER */

/**
 * @brief 读取进程内存信息
 * @param seqBuf 序列缓冲区指针
 * @param pcb 进程控制块指针
 * @return 成功返回0，失败返回错误码
 */
static int ProcessMemInfoRead(struct SeqBuf *seqBuf, LosProcessCB *pcb)
{
    unsigned int intSave;                              // 中断标志
    unsigned int size = sizeof(LosVmSpace) + sizeof(LosVmMapRegion);  // 内存大小
    LosVmSpace *vmSpace = (LosVmSpace *)LOS_MemAlloc(m_aucSysMem1, size);  // 分配内存
    if (vmSpace == NULL) {                             // 分配失败
        return -ENOMEM;                                // 返回内存不足错误
    }
    (void)memset_s(vmSpace, size, 0, size);            // 清空内存
    LosVmMapRegion *heap = (LosVmMapRegion *)((char *)vmSpace + sizeof(LosVmSpace));  // 堆区域指针

    SCHEDULER_LOCK(intSave);                           // 关调度器
    if (OsProcessIsInactive(pcb)) {                    // 进程未激活
        SCHEDULER_UNLOCK(intSave);                     // 开调度器
        (void)LOS_MemFree(m_aucSysMem1, vmSpace);      // 释放内存
        return -EINVAL;                                // 返回无效参数错误
    }
    (void)memcpy_s(vmSpace, sizeof(LosVmSpace), pcb->vmSpace, sizeof(LosVmSpace));  // 复制虚拟内存空间信息
    (void)memcpy_s(heap, sizeof(LosVmMapRegion), pcb->vmSpace->heap, sizeof(LosVmMapRegion));  // 复制堆区域信息
    SCHEDULER_UNLOCK(intSave);                         // 开调度器

    // 输出内存信息到序列缓冲区
    (void)LosBufPrintf(seqBuf, "\nVMSpaceSize:      %u byte\n", vmSpace->size);
    (void)LosBufPrintf(seqBuf, "VMSpaceMapSize:   %u byte\n", vmSpace->mapSize);
    (void)LosBufPrintf(seqBuf, "VM TLB Asid:      %u\n", vmSpace->archMmu.asid);
    (void)LosBufPrintf(seqBuf, "VMHeapSize:       %u byte\n", heap->range.size);
    (void)LosBufPrintf(seqBuf, "VMHeapRegionName: %s\n", OsGetRegionNameOrFilePath(heap));
    (void)LosBufPrintf(seqBuf, "VMHeapRegionType: 0x%x\n", heap->regionType);
    (void)LOS_MemFree(m_aucSysMem1, vmSpace);          // 释放内存
    return 0;                                          // 返回成功
}

#ifdef LOSCFG_KERNEL_CPUP                              // 如果启用CPU性能统计
#define TIME_CYCLE_TO_US(time) ((((UINT64)time) * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US)  // 周期转微秒

/**
 * @brief 读取进程CPU使用情况
 * @param seqBuf 序列缓冲区指针
 * @param pcb 进程控制块指针
 * @return 成功返回0，失败返回错误码
 */
static int ProcessCpupRead(struct SeqBuf *seqBuf, LosProcessCB *pcb)
{
    unsigned int intSave;                              // 中断标志
    OsCpupBase *processCpup = (OsCpupBase *)LOS_MemAlloc(m_aucSysMem1, sizeof(OsCpupBase));  // 分配CPU使用数据结构
    if (processCpup == NULL) {                         // 分配失败
        return -ENOMEM;                                // 返回内存不足错误
    }
    (void)memset_s(processCpup, sizeof(OsCpupBase), 0, sizeof(OsCpupBase));  // 清空数据

    SCHEDULER_LOCK(intSave);                           // 关调度器
    if (OsProcessIsInactive(pcb)) {                    // 进程未激活
        SCHEDULER_UNLOCK(intSave);                     // 开调度器
        (VOID)LOS_MemFree(m_aucSysMem1, processCpup);  // 释放内存
        return -EINVAL;                                // 返回无效参数错误
    }
    (void)memcpy_s(processCpup, sizeof(OsCpupBase), pcb->processCpup, sizeof(OsCpupBase));  // 复制CPU使用数据
    SCHEDULER_UNLOCK(intSave);                         // 开调度器

    // 输出CPU使用信息到序列缓冲区
    (void)LosBufPrintf(seqBuf, "\nTotalRunningTime: %lu us\n", TIME_CYCLE_TO_US(processCpup->allTime));
    (void)LosBufPrintf(seqBuf, "HistoricalRunningTime:(us) ");
    for (UINT32 i = 0; i < OS_CPUP_HISTORY_RECORD_NUM + 1; i++) {  // 输出历史记录
        (void)LosBufPrintf(seqBuf, "%lu  ", TIME_CYCLE_TO_US(processCpup->historyTime[i]));
    }
    (void)LosBufPrintf(seqBuf, "\n");
    (void)LOS_MemFree(m_aucSysMem1, processCpup);      // 释放内存
    return 0;                                          // 返回成功
}
#endif

#ifdef LOSCFG_TIME_CONTAINER                           // 如果启用时间容器
static const CHAR *g_monotonic = "monotonic";         // 单调时钟名称
#define DECIMAL_BASE 10                               // 十进制基数

/**
 * @brief 读取时间容器信息
 * @param m 序列缓冲区指针
 * @param v 进程数据指针
 * @return 成功返回0，失败返回错误码
 */
static int ProcTimeContainerRead(struct SeqBuf *m, void *v)
{
    int ret;                                          // 返回值
    unsigned int intSave;                              // 中断标志
    struct timespec64 offsets = {0};                   // 时间偏移

    if ((m == NULL) || (v == NULL)) {                  // 参数无效
        return -EINVAL;                                // 返回无效参数错误
    }

    struct ProcessData *data = (struct ProcessData *)v;  // 获取进程数据
    SCHEDULER_LOCK(intSave);                           // 关调度器
    LosProcessCB *processCB = ProcGetProcessCB(data);  // 获取进程PCB
    ret = OsGetTimeContainerMonotonic(processCB, &offsets);  // 获取单调时钟偏移
    SCHEDULER_UNLOCK(intSave);                         // 开调度器
    if (ret != LOS_OK) {                               // 获取失败
        return -ret;                                   // 返回错误码
    }

    LosBufPrintf(m, "monotonic %lld %ld\n", offsets.tv_sec, offsets.tv_nsec);  // 输出时间信息
    return 0;                                          // 返回成功
}

/**
 * @brief 设置时间容器偏移
 * @param buf 输入缓冲区
 * @param processCB 进程控制块指针
 * @return 成功返回0，失败返回错误码
 */
static int ProcSetTimensOffset(const char *buf, LosProcessCB *processCB)
{
    unsigned int intSave;                              // 中断标志
    struct timespec64 offsets;                         // 时间偏移
    char *endptr = NULL;                               // 字符串结束指针

    offsets.tv_sec = strtoll(buf, &endptr, DECIMAL_BASE);  // 解析秒数
    offsets.tv_nsec = strtoll(endptr, NULL, DECIMAL_BASE); // 解析纳秒数
    if (offsets.tv_nsec >= OS_SYS_NS_PER_SECOND) {     // 纳秒数无效
        return -EACCES;                                // 返回权限错误
    }

    SCHEDULER_LOCK(intSave);                           // 关调度器
    unsigned int ret = OsSetTimeContainerMonotonic(processCB, &offsets);  // 设置时间偏移
    SCHEDULER_UNLOCK(intSave);                         // 开调度器
    if (ret != LOS_OK) {                               // 设置失败
        return -ret;                                   // 返回错误码
    }
    return 0;                                          // 返回成功
}

/**
 * @brief 写入时间容器信息
 * @param pf Proc文件结构体指针
 * @param buf 输入缓冲区
 * @param count 写入长度
 * @param ppos 文件位置指针
 * @return 成功返回写入长度，失败返回错误码
 */
static int ProcTimeContainerWrite(struct ProcFile *pf, const char *buf, size_t count, loff_t *ppos)
{
    (void)ppos;                                        // 未使用参数
    char *kbuf = NULL;                                 // 内核缓冲区
    int ret;                                           // 返回值

    if ((pf == NULL) || (count <= 0)) {                // 参数无效
        return -EINVAL;                                // 返回无效参数错误
    }

    struct ProcDirEntry *entry = pf->pPDE;             // 获取Proc目录项
    if (entry == NULL) {                               // 目录项无效
        return -EINVAL;                                // 返回无效参数错误
    }

    struct ProcessData *data = (struct ProcessData *)entry->data;  // 获取进程数据
    if (data == NULL) {                                // 进程数据无效
        return -EINVAL;                                // 返回无效参数错误
    }

    if (LOS_IsUserAddressRange((VADDR_T)(UINTPTR)buf, count)) {  // 如果缓冲区在用户空间
        kbuf = LOS_MemAlloc(m_aucSysMem1, count + 1);             // 分配内核缓冲区
        if (kbuf == NULL) {                                       // 分配失败
            return -ENOMEM;                                      // 返回内存不足错误
        }

        if (LOS_ArchCopyFromUser(kbuf, buf, count) != 0) {         // 复制用户空间数据
            (VOID)LOS_MemFree(m_aucSysMem1, kbuf);                // 释放缓冲区
            return -EFAULT;                                       // 返回内存访问错误
        }
        kbuf[count] = '\0';                                       // 添加字符串结束符
        buf = kbuf;                                               // 指向内核缓冲区
    }

    ret = strncmp(buf, g_monotonic, strlen(g_monotonic));  // 比较时钟名称
    if (ret != 0) {                                        // 名称不匹配
        (VOID)LOS_MemFree(m_aucSysMem1, kbuf);             // 释放缓冲区
        return -EINVAL;                                    // 返回无效参数错误
    }

    buf += strlen(g_monotonic);                            // 跳过时钟名称
    ret = ProcSetTimensOffset(buf, ProcGetProcessCB(data));  // 设置时间偏移
    if (ret < 0) {                                         // 设置失败
        (VOID)LOS_MemFree(m_aucSysMem1, kbuf);             // 释放缓冲区
        return ret;                                        // 返回错误码
    }
    (VOID)LOS_MemFree(m_aucSysMem1, kbuf);                  // 释放缓冲区
    return count;                                          // 返回写入长度
}

// 时间容器文件操作结构体
static const struct ProcFileOperations TIME_CONTAINER_FOPS = {
    .read = ProcTimeContainerRead,                         // 读取操作
    .write = ProcTimeContainerWrite                        // 写入操作
};
#endif

#ifdef LOSCFG_USER_CONTAINER                             // 如果启用用户容器

/**
 * @brief 从用户空间复制字符串并添加结束符
 * @param src 用户空间源地址
 * @param len 长度
 * @return 成功返回内核空间字符串指针，失败返回NULL
 */
static void *MemdupUserNul(const void *src, size_t len)
{
    char *des = NULL;                                    // 目标缓冲区
    if (len <= 0) {                                      // 长度无效
        return NULL;                                     // 返回NULL
    }
    des = LOS_MemAlloc(OS_SYS_MEM_ADDR, len + 1);         // 分配内核内存
    if (des == NULL) {                                   // 分配失败
        return NULL;                                     // 返回NULL
    }

    if (LOS_ArchCopyFromUser(des, src, len) != 0) {       // 从用户空间复制
        (VOID)LOS_MemFree(OS_SYS_MEM_ADDR, des);         // 释放内存
        return NULL;                                     // 返回NULL
    }

    des[len] = '\0';                                     // 添加结束符
    return des;                                          // 返回内核字符串
}

/**
 * @brief UID/GID映射写入前检查
 * @param pf Proc文件结构体指针
 * @param buf 输入缓冲区
 * @param size 大小
 * @param kbuf 输出参数，内核缓冲区
 * @param type 输出参数，数据类型
 * @return 成功返回进程PCB指针，失败返回NULL
 */
static LosProcessCB *ProcUidGidMapWriteCheck(struct ProcFile *pf, const char *buf, size_t size, char **kbuf, ProcessDataType *type)
{
    if ((pf == NULL) || (size <= 0) || (size >= PAGE_SIZE)) {  // 参数无效
        return NULL;                                           // 返回NULL
    }

    struct ProcDirEntry *entry = pf->pPDE;             // 获取Proc目录项
    if (entry == NULL) {                               // 目录项无效
        return NULL;                                   // 返回NULL
    }

    struct ProcessData *data = (struct ProcessData *)entry->data;  // 获取进程数据
    if (data == NULL) {                                // 进程数据无效
        return NULL;                                   // 返回NULL
    }

    *kbuf = MemdupUserNul(buf, size);                  // 复制用户空间数据
    if (*kbuf == NULL) {                               // 复制失败
        return NULL;                                   // 返回NULL
    }
    *type = (ProcessDataType)data->type;               // 设置数据类型
    return ProcGetProcessCB(data);                     // 返回进程PCB
}

/**
 * @brief UID/GID映射写入操作
 * @param file Proc文件结构体指针
 * @param buf 输入缓冲区
 * @param size 写入大小
 * @param ppos 文件位置指针
 * @return 成功返回写入长度，失败返回错误码
 */
static ssize_t ProcIDMapWrite(struct ProcFile *file, const char *buf, size_t size, loff_t *ppos)
{
    (void)ppos;                                        // 未使用参数
    char *kbuf = NULL;                                 // 内核缓冲区
    int ret;                                           // 返回值
    unsigned int intSave;                              // 中断标志
    ProcessDataType type = PROC_P_TYPE_MAX;            // 数据类型
    LosProcessCB *processCB = ProcUidGidMapWriteCheck(file, buf, size, &kbuf, &type);  // 写入前检查
    if (processCB == NULL) {                           // 检查失败
        return -EINVAL;                                // 返回无效参数错误
    }

    SCHEDULER_LOCK(intSave);                           // 关调度器
    if ((processCB->credentials == NULL) || (processCB->credentials->userContainer == NULL)) {  // 凭据或用户容器无效
        SCHEDULER_UNLOCK(intSave);                     // 开调度器
        (void)LOS_MemFree(m_aucSysMem1, kbuf);         // 释放缓冲区
        return -EINVAL;                                // 返回无效参数错误
    }
    UserContainer *userContainer = processCB->credentials->userContainer;  // 获取用户容器
    if (userContainer->parent == NULL) {               // 无父容器
        SCHEDULER_UNLOCK(intSave);                     // 开调度器
        (void)LOS_MemFree(m_aucSysMem1, kbuf);         // 释放缓冲区
        return -EPERM;                                 // 返回权限错误
    }
    if (type == PROC_UID_MAP) {                        // UID映射
        ret = OsUserContainerMapWrite(file, kbuf, size, CAP_SETUID, &userContainer->uidMap, &userContainer->parent->uidMap);
    } else {                                           // GID映射
        ret = OsUserContainerMapWrite(file, kbuf, size, CAP_SETGID, &userContainer->gidMap, &userContainer->parent->gidMap);
    }
    SCHEDULER_UNLOCK(intSave);                         // 开调度器
    (void)LOS_MemFree(m_aucSysMem1, kbuf);             // 释放缓冲区
    return ret;                                        // 返回结果
}

/**
 * @brief UID/GID映射读取操作
 * @param seqBuf 序列缓冲区指针
 * @param v 进程数据指针
 * @return 成功返回0，失败返回错误码
 */
static int ProcIDMapRead(struct SeqBuf *seqBuf, void *v)
{
    unsigned int intSave;                              // 中断标志
    if ((seqBuf == NULL) || (v == NULL)) {             // 参数无效
        return -EINVAL;                                // 返回无效参数错误
    }
    struct ProcessData *data = (struct ProcessData *)v;  // 获取进程数据
    LosProcessCB *processCB = ProcGetProcessCB(data);   // 获取进程PCB

    SCHEDULER_LOCK(intSave);                            // 关调度器
    if ((processCB->credentials == NULL) || (processCB->credentials->userContainer == NULL)) {  // 凭据或用户容器无效
        SCHEDULER_UNLOCK(intSave);                      // 开调度器
        return -EINVAL;                                 // 返回无效参数错误
    }
    UserContainer *userContainer = processCB->credentials->userContainer;  // 获取用户容器
    if ((userContainer != NULL) && (userContainer->parent == NULL)) {  // 无父容器
        UidGidExtent uidGidExtent = userContainer->uidMap.extent[0];   // 获取UID映射范围
        SCHEDULER_UNLOCK(intSave);                      // 开调度器
        (void)LosBufPrintf(seqBuf, "%d %d %u\n", uidGidExtent.first, uidGidExtent.lowerFirst, uidGidExtent.count);  // 输出映射信息
        return 0;                                       // 返回成功
    }
    SCHEDULER_LOCK(intSave);                            // 关调度器（此处应为解锁，代码可能存在错误）
    return 0;                                           // 返回成功
}

// UID/GID映射文件操作结构体
static const struct ProcFileOperations UID_GID_MAP_FOPS = {
    .read = ProcIDMapRead,                             // 读取操作
    .write = ProcIDMapWrite                            // 写入操作
};
#endif

/**
 * @brief 进程信息读取调度函数
 * @param m 序列缓冲区指针
 * @param v 进程数据指针
 * @return 成功返回0，失败返回错误码
 */
static int ProcProcessRead(struct SeqBuf *m, void *v)
{
    if ((m == NULL) || (v == NULL)) {                  // 参数无效
        return -EINVAL;                                // 返回无效参数错误
    }
    struct ProcessData *data = (struct ProcessData *)v;  // 获取进程数据
    switch (data->type) {                               // 根据类型调用对应读取函数
        case PROC_PID_MEM:                              // 内存信息
            return ProcessMemInfoRead(m, ProcGetProcessCB(data));
#ifdef LOSCFG_KERNEL_CPUP
        case PROC_PID_CPUP:                             // CPU使用情况
            return ProcessCpupRead(m, ProcGetProcessCB(data));
#endif
        default:                                        // 未知类型
            break;
    }
    return -EINVAL;                                    // 返回无效参数错误
}

/**
 * @brief PID相关文件操作结构体
 * @details 定义proc文件系统中PID目录下文件的读操作接口
 */
static const struct ProcFileOperations PID_FOPS = {
    .read = ProcProcessRead,  // 读取PID相关信息的回调函数
};

/**
 * @brief 进程proc节点配置数组
 * @details 定义/proc下进程相关节点的名称、权限、类型和文件操作结构体
 */
static struct ProcProcess g_procProcess[] = {
    {
        .name = NULL,  // 节点名称为NULL，表示PID目录本身
        .mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IROTH,  // 目录权限：所有者读写执行，组读写执行，其他读执行
        .type = PROC_PID,  // 节点类型：PID目录
        .fileOps = &PID_FOPS  // 关联PID文件操作结构体

    },
    {
        .name = "meminfo",  // 节点名称：内存信息文件
        .mode = 0,  // 默认权限
        .type = PROC_PID_MEM,  // 节点类型：进程内存信息
        .fileOps = &PID_FOPS  // 关联PID文件操作结构体
    },
#ifdef LOSCFG_KERNEL_CPUP
    {
        .name = "cpup",  // 节点名称：CPU使用信息文件
        .mode = 0,  // 默认权限
        .type = PROC_PID_CPUP,  // 节点类型：进程CPU使用信息
        .fileOps = &PID_FOPS  // 关联PID文件操作结构体

    },
#endif
#ifdef LOSCFG_KERNEL_CONTAINER
    {
        .name = "container",  // 节点名称：容器目录
        .mode = S_IFDIR | S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH,  // 目录权限：所有用户读执行
        .type = CONTAINER,  // 节点类型：容器目录
        .fileOps = &PID_CONTAINER_FOPS  // 关联容器文件操作结构体

    },
#ifdef LOSCFG_PID_CONTAINER
    {
        .name = "container/pid",  // 节点名称：PID容器链接
        .mode = S_IFLNK,  // 文件类型：符号链接
        .type = PID_CONTAINER,  // 节点类型：PID容器
        .fileOps = &PID_CONTAINER_FOPS  // 关联容器文件操作结构体
    },
    {
        .name = "container/pid_for_children",  // 节点名称：子进程PID容器链接
        .mode = S_IFLNK,  // 文件类型：符号链接
        .type = PID_CHILD_CONTAINER,  // 节点类型：子进程PID容器
        .fileOps = &PID_CONTAINER_FOPS  // 关联容器文件操作结构体
    },
#endif
// ... 其他容器相关节点定义（UTS、MNT、IPC等） ...
};

/**
 * @brief 释放进程proc目录
 * @details 从proc文件系统中移除指定的进程目录项
 * @param processDir 要释放的进程目录项指针
 */
void ProcFreeProcessDir(struct ProcDirEntry *processDir)
{
    if (processDir == NULL) {  // 检查目录项指针是否为空
        return;  // 空指针直接返回，避免空引用
    }
    RemoveProcEntry(processDir->name, NULL);  // 移除proc目录项
}

/**
 * @brief 创建进程相关的proc节点
 * @details 根据进程ID和节点配置，在proc文件系统中创建对应的文件或目录节点
 * @param pid 进程ID，OS_INVALID_VALUE表示当前进程
 * @param porcess 进程节点配置结构体指针
 * @param processCB 进程控制块指针
 * @return 成功返回创建的目录项指针，失败返回NULL
 */
static struct ProcDirEntry *ProcCreatePorcess(UINT32 pid, struct ProcProcess *porcess, uintptr_t processCB)
{
    int ret;  // 函数返回值
    struct ProcDataParm dataParm;  // proc数据参数结构体
    char pidName[PROC_PID_DIR_LEN] = {0};  // 节点路径名称缓冲区
    struct ProcessData *data = (struct ProcessData *)malloc(sizeof(struct ProcessData));  // 分配进程数据内存
    if (data == NULL) {  // 检查内存分配是否成功
        return NULL;  // 分配失败返回NULL
    }
    if (pid != OS_INVALID_VALUE) {  // 判断是否为有效进程ID
        if (porcess->name != NULL) {  // 节点名称非空（文件节点）
            ret = snprintf_s(pidName, PROC_PID_DIR_LEN, PROC_PID_DIR_LEN - 1, "%u/%s", pid, porcess->name);  // 格式化路径：PID/节点名
        } else {  // 节点名称为空（目录节点）
            ret = snprintf_s(pidName, PROC_PID_DIR_LEN, PROC_PID_DIR_LEN - 1, "%u", pid);  // 格式化路径：PID
        }
    } else {  // 无效进程ID，表示当前进程
        if (porcess->name != NULL) {  // 节点名称非空（文件节点）
            ret = snprintf_s(pidName, PROC_PID_DIR_LEN, PROC_PID_DIR_LEN - 1, "%s/%s", "self", porcess->name);  // 格式化路径：self/节点名
        } else {  // 节点名称为空（目录节点）
            ret = snprintf_s(pidName, PROC_PID_DIR_LEN, PROC_PID_DIR_LEN - 1, "%s", "self");  // 格式化路径：self
        }
    }
    if (ret < 0) {  // 检查路径格式化是否成功
        free(data);  // 释放已分配的内存
        return NULL;  // 格式化失败返回NULL
    }

    data->process = processCB;  // 保存进程控制块指针
    data->type = porcess->type;  // 保存节点类型
    dataParm.data = data;  // 设置数据参数
    dataParm.dataType = PROC_DATA_FREE;  // 设置数据释放类型
    struct ProcDirEntry *container = ProcCreateData(pidName, porcess->mode, NULL, porcess->fileOps, &dataParm);  // 创建proc节点
    if (container == NULL) {  // 检查节点创建是否成功
        free(data);  // 释放已分配的内存
        PRINT_ERR("create /proc/%s error!\n", pidName);  // 打印错误信息
        return NULL;  // 创建失败返回NULL
    }
    return container;  // 返回创建的目录项指针
}

/**
 * @brief 创建进程proc目录及子节点
 * @details 根据配置数组批量创建进程相关的所有proc节点，并关联到进程控制块
 * @param pid 进程ID
 * @param process 进程控制块指针
 * @return 成功返回0，失败返回-1
 */
int ProcCreateProcessDir(UINT32 pid, uintptr_t process)
{
    unsigned int intSave;  // 中断状态保存变量
    struct ProcDirEntry *pidDir = NULL;  // PID目录项指针
    // 遍历所有进程节点配置
    for (int index = 0; index < (sizeof(g_procProcess) / sizeof(struct ProcProcess)); index++) {
        struct ProcProcess *procProcess = &g_procProcess[index];  // 当前节点配置
        struct ProcDirEntry *dir = ProcCreatePorcess(pid, procProcess, process);  // 创建节点
        if (dir == NULL) {  // 检查节点创建是否成功
            PRINT_ERR("create /proc/%s error!\n", procProcess->name);  // 打印错误信息
            goto CREATE_ERROR;  // 跳转到错误处理
        }
        if (index == 0) {  // 第一个节点为PID目录本身
            pidDir = dir;  // 保存PID目录项指针
        }
    }

    if (process != 0) {  // 进程控制块非空
        SCHEDULER_LOCK(intSave);  // 关闭调度器
        ((LosProcessCB *)process)->procDir = pidDir;  // 将PID目录项关联到进程控制块
        SCHEDULER_UNLOCK(intSave);  // 开启调度器
    }

    return 0;  // 创建成功返回0

CREATE_ERROR:  // 错误处理标签
    if (pidDir != NULL) {  // 如果PID目录已创建
        RemoveProcEntry(pidDir->name, NULL);  // 移除PID目录
    }
    return -1;  // 创建失败返回-1
}
#endif /* LOSCFG_PROC_PROCESS_DIR */

/**
 * @brief 进程信息填充函数
 * @details 读取并填充系统中所有进程的信息到序列缓冲区
 * @param m 序列缓冲区指针
 * @param v 未使用的参数
 * @return 始终返回0
 */
static int ProcessProcFill(struct SeqBuf *m, void *v)
{
    (void)v;  // 未使用参数，避免编译警告
    (void)OsShellCmdTskInfoGet(OS_ALL_TASK_MASK, m, OS_PROCESS_INFO_ALL);  // 获取所有进程信息并填充到缓冲区
    return 0;  // 返回成功
}

/**
 * @brief 进程信息文件操作结构体
 * @details 定义/proc/process文件的读操作接口
 */
static const struct ProcFileOperations PROCESS_PROC_FOPS = {
    .read       = ProcessProcFill,  // 读取进程信息的回调函数
};

/**
 * @brief 进程proc节点初始化
 * @details 创建/proc/process节点，并初始化进程相关的proc目录（如self和根进程目录）
 */
void ProcProcessInit(void)
{
    struct ProcDirEntry *pde = CreateProcEntry("process", 0, NULL);  // 创建/proc/process节点
    if (pde == NULL) {  // 检查节点创建是否成功
        PRINT_ERR("create /proc/process error!\n");  // 打印错误信息
        return;  // 创建失败返回
    }
    pde->procFileOps = &PROCESS_PROC_FOPS;  // 关联进程信息文件操作结构体

#ifdef LOSCFG_PROC_PROCESS_DIR
    int ret = ProcCreateProcessDir(OS_INVALID_VALUE, 0);  // 创建self进程目录
    if (ret < 0) {
        PRINT_ERR("Create proc process self dir failed!\n");  // 打印错误信息
    }

    ret = ProcCreateProcessDir(OS_USER_ROOT_PROCESS_ID, (uintptr_t)OsGetUserInitProcess());  // 创建根进程目录
    if (ret < 0) {
        PRINT_ERR("Create proc process %d dir failed!\n", OS_USER_ROOT_PROCESS_ID);  // 打印错误信息
    }
#endif
    return;  // 初始化完成
}
