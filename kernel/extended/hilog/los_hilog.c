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

#include "los_hilog.h"
#include "los_init.h"
#include "los_mp.h"
#include "los_mux.h"
#include "los_process_pri.h"
#include "los_task_pri.h"
#include "fs/file.h"
#include "fs/driver.h"
#include "los_vm_map.h"
#include "los_vm_lock.h"
#include "user_copy.h"
#define HILOG_BUFFER LOSCFG_HILOG_BUFFER_SIZE  // 日志缓冲区大小，由配置项定义
#define DRIVER_MODE 0666                       // 驱动文件访问权限：读写权限
#define HILOG_DRIVER "/dev/hilog"              // 日志驱动设备路径

/**
 * @brief 日志条目结构体定义
 * @details 用于存储单条日志的完整信息，包括头部和消息内容
 */
struct HiLogEntry {
    unsigned int len;           // 日志总长度（包括头部和消息）
    unsigned int hdrSize;       // 头部大小
    unsigned int pid : 16;      // 进程ID（16位）
    unsigned int taskId : 16;   // 任务ID（16位）
    unsigned int sec;           // 秒级时间戳
    unsigned int nsec;          // 纳秒级时间戳
    unsigned int reserved;      // 保留字段
    char msg[0];                // 日志消息内容（柔性数组）
};

// 文件操作函数声明
ssize_t HilogRead(struct file *filep, char __user *buf, size_t count);
ssize_t HilogWrite(struct file *filep, const char __user *buf, size_t count);
int HiLogOpen(struct file *filep);
int HiLogClose(struct file *filep);

// 静态函数声明
static ssize_t HiLogWrite(struct file *filep, const char *buffer, size_t bufLen);
static ssize_t HiLogRead(struct file *filep, char *buffer, size_t bufLen);

/**
 * @brief 日志设备文件操作结构体
 * @details 实现日志设备的打开、关闭、读、写等文件操作接口
 */
STATIC struct file_operations_vfs g_hilogFops = {
    HiLogOpen,  /* open    - 打开日志设备 */
    HiLogClose, /* close   - 关闭日志设备 */
    HiLogRead,  /* read    - 读取日志数据 */
    HiLogWrite, /* write   - 写入日志数据 */
    NULL,       /* seek    - 不支持文件定位 */
    NULL,       /* ioctl   - 不支持IO控制 */
    NULL,       /* mmap    - 不支持内存映射 */
#ifndef CONFIG_DISABLE_POLL
    NULL, /* poll    - 不支持轮询操作 */
#endif
    NULL, /* unlink  - 不支持删除操作 */
};

/**
 * @brief 日志字符设备结构体
 * @details 管理日志设备的状态、缓冲区和同步机制
 */
struct HiLogCharDevice {
    int flag;                     // 设备标志
    LosMux mtx;                   // 互斥锁，保护设备操作
    unsigned char *buffer;        // 环形缓冲区指针
    wait_queue_head_t wq;         // 等待队列，用于阻塞读写
    size_t writeOffset;           // 写指针偏移量
    size_t headOffset;            // 读指针偏移量
    size_t size;                  // 当前缓冲区数据大小
    size_t count;                 // 日志条目计数
} g_hiLogDev;                     // 日志设备全局实例

/**
 * @brief 获取缓冲区头部指针
 * @return 缓冲区头部（读指针位置）的地址
 */
static inline unsigned char *HiLogBufferHead(void)
{
    return g_hiLogDev.buffer + g_hiLogDev.headOffset;  // 计算并返回读指针位置
}

/**
 * @brief 打开日志设备
 * @param filep 文件指针
 * @return 0 - 成功；其他值 - 失败
 */
int HiLogOpen(struct file *filep)
{
    (void)filep;  // 未使用的参数
    return 0;     // 始终返回成功
}

/**
 * @brief 关闭日志设备
 * @param filep 文件指针
 * @return 0 - 成功；其他值 - 失败
 */
int HiLogClose(struct file *filep)
{
    (void)filep;  // 未使用的参数
    return 0;     // 始终返回成功
}

/**
 * @brief 增加缓冲区数据大小
 * @param sz 新增数据大小
 * @note 仅当缓冲区有足够空间时才更新状态
 */
static void HiLogBufferInc(size_t sz)
{
    if (g_hiLogDev.size + sz <= HILOG_BUFFER) {  // 检查缓冲区是否有足够空间
        g_hiLogDev.size += sz;                   // 更新数据大小
        g_hiLogDev.writeOffset += sz;            // 更新写指针
        g_hiLogDev.writeOffset %= HILOG_BUFFER;  // 环形缓冲区写指针回绕
        g_hiLogDev.count++;                      // 增加日志条目计数
    }
}

/**
 * @brief 减少缓冲区数据大小
 * @param sz 减少的数据大小
 * @note 仅当缓冲区数据大小大于等于sz时才更新状态
 */
static void HiLogBufferDec(size_t sz)
{
    if (g_hiLogDev.size >= sz) {                 // 检查缓冲区数据是否足够
        g_hiLogDev.size -= sz;                   // 更新数据大小
        g_hiLogDev.headOffset += sz;             // 更新读指针
        g_hiLogDev.headOffset %= HILOG_BUFFER;   // 环形缓冲区读指针回绕
        g_hiLogDev.count--;                      // 减少日志条目计数
    }
}

/**
 * @brief 日志缓冲区数据拷贝
 * @param dst 目标缓冲区
 * @param dstLen 目标缓冲区长度
 * @param src 源缓冲区
 * @param srcLen 源数据长度
 * @return 0 - 成功；-1 - 失败
 * @note 自动处理用户空间与内核空间之间的数据拷贝
 */
static int HiLogBufferCopy(unsigned char *dst, unsigned dstLen, const unsigned char *src, size_t srcLen)
{
    int retval = -1;                                 // 默认返回失败
    size_t minLen = (dstLen > srcLen) ? srcLen : dstLen;  // 取目标和源长度的较小值

    // 检查是否同时为用户空间地址（不允许直接拷贝）
    if (LOS_IsUserAddressRange((VADDR_T)(UINTPTR)dst, minLen) &&
        LOS_IsUserAddressRange((VADDR_T)(UINTPTR)src, minLen)) {
        return retval;  // 同时为用户空间地址，返回失败
    }

    if (LOS_IsUserAddressRange((VADDR_T)(UINTPTR)dst, minLen)) {
        retval = LOS_ArchCopyToUser(dst, src, minLen);  // 内核空间到用户空间拷贝
    } else if (LOS_IsUserAddressRange((VADDR_T)(UINTPTR)src, minLen)) {
        retval = LOS_ArchCopyFromUser(dst, src, minLen);  // 用户空间到内核空间拷贝
    } else {
        retval = memcpy_s(dst, dstLen, src, srcLen);  // 内核空间内部拷贝
    }
    return retval;  // 返回拷贝结果
}

/**
 * @brief 从环形缓冲区读取数据
 * @param buffer 目标缓冲区
 * @param bufLen 要读取的长度
 * @return 0 - 成功；其他值 - 失败
 * @note 处理环形缓冲区的边界回绕情况
 */
static int HiLogReadRingBuffer(unsigned char *buffer, size_t bufLen)
{
    int retval;
    size_t bufLeft = HILOG_BUFFER - g_hiLogDev.headOffset;  // 计算当前读指针到缓冲区末尾的空间
    if (bufLeft > bufLen) {
        // 无需回绕，直接从读指针位置拷贝
        retval = HiLogBufferCopy(buffer, bufLen, HiLogBufferHead(), bufLen);
    } else {
        // 先拷贝从读指针到缓冲区末尾的数据
        retval = HiLogBufferCopy(buffer, bufLen, HiLogBufferHead(), bufLeft);
        if (retval < 0) {
            return retval;  // 拷贝失败，直接返回
        }

        // 再拷贝从缓冲区开头到剩余长度的数据（处理回绕）
        retval = HiLogBufferCopy(buffer + bufLeft, bufLen - bufLeft, g_hiLogDev.buffer, bufLen - bufLeft);
    }
    return retval;  // 返回读取结果
}
/**
 * @brief 从日志设备读取数据
 * @param filep 文件指针
 * @param buffer 接收数据的缓冲区
 * @param bufLen 缓冲区长度
 * @return 成功时返回读取的字节数；失败时返回负数错误码
 * @note 该函数会阻塞等待直到有数据可读
 */
static ssize_t HiLogRead(struct file *filep, char *buffer, size_t bufLen)
{
    int retval;                     // 函数返回值
    struct HiLogEntry header;       // 日志条目头部

    (void)filep;                    // 未使用的参数
    // 阻塞等待直到缓冲区有数据（可被信号中断）
    wait_event_interruptible(g_hiLogDev.wq, (g_hiLogDev.size > 0));

    (VOID)LOS_MuxAcquire(&g_hiLogDev.mtx);  // 获取互斥锁，保护缓冲区操作
    // 从环形缓冲区读取日志头部
    retval = HiLogReadRingBuffer((unsigned char *)&header, sizeof(header));
    if (retval < 0) {
        retval = -EINVAL;           // 读取失败，设置参数无效错误
        goto out;                   // 跳转到释放资源处
    }

    // 检查缓冲区是否足够容纳头部+消息体
    if (bufLen < header.len + sizeof(header)) {
        PRINTK("buffer too small,bufLen=%d, header.len=%d,%d\n", bufLen, header.len, header.hdrSize);
        retval = -ENOMEM;           // 设置内存不足错误
        goto out;                   // 跳转到释放资源处
    }

    HiLogBufferDec(sizeof(header));  // 减少缓冲区已用大小（头部）

    // 将日志头部拷贝到用户缓冲区
    retval = HiLogBufferCopy((unsigned char *)buffer, bufLen, (unsigned char *)&header, sizeof(header));
    if (retval < 0) {
        retval = -EINVAL;           // 拷贝失败，设置参数无效错误
        goto out;                   // 跳转到释放资源处
    }

    // 从环形缓冲区读取日志消息体
    retval = HiLogReadRingBuffer((unsigned char *)(buffer + sizeof(header)), header.len);
    if (retval < 0) {
        retval = -EINVAL;           // 读取失败，设置参数无效错误
        goto out;                   // 跳转到释放资源处
    }

    HiLogBufferDec(header.len);     // 减少缓冲区已用大小（消息体）
    retval = header.len + sizeof(header);  // 返回总读取字节数
out:
    if (retval == -ENOMEM) {        // 如果是内存不足错误
        // 清空环形缓冲区
        g_hiLogDev.writeOffset = 0;
        g_hiLogDev.headOffset = 0;
        g_hiLogDev.size = 0;
        g_hiLogDev.count = 0;
    }
    (VOID)LOS_MuxRelease(&g_hiLogDev.mtx);  // 释放互斥锁
    return (ssize_t)retval;         // 返回结果
}

/**
 * @brief 向环形缓冲区写入数据
 * @param buffer 待写入的数据缓冲区
 * @param bufLen 待写入数据长度
 * @return 0 - 成功；-1 - 失败
 * @note 自动处理环形缓冲区的边界回绕
 */
static int HiLogWriteRingBuffer(unsigned char *buffer, size_t bufLen)
{
    int retval;                     // 函数返回值
    // 计算当前写指针到缓冲区末尾的剩余空间
    size_t bufLeft = HILOG_BUFFER - g_hiLogDev.writeOffset;
    if (bufLen > bufLeft) {         // 如果剩余空间不足，需要分两次写入
        // 先写满当前剩余空间
        retval = HiLogBufferCopy(g_hiLogDev.buffer + g_hiLogDev.writeOffset, bufLeft, buffer, bufLeft);
        if (retval) {
            return -1;              // 拷贝失败，返回错误
        }
        // 再从缓冲区开头写入剩余数据
        retval = HiLogBufferCopy(g_hiLogDev.buffer, HILOG_BUFFER, buffer + bufLeft, bufLen - bufLeft);
    } else {
        // 空间足够，直接写入
        retval = HiLogBufferCopy(g_hiLogDev.buffer + g_hiLogDev.writeOffset, bufLeft, buffer, bufLen);
    }
    if (retval < 0) {
        return -1;                  // 拷贝失败，返回错误
    }
    return 0;                       // 成功返回
}

/**
 * @brief 初始化日志条目头部
 * @param header 日志头部指针
 * @param len 日志消息体长度
 * @note 自动填充进程ID、任务ID和时间戳信息
 */
static void HiLogHeadInit(struct HiLogEntry *header, size_t len)
{
    struct timespec now = {0};      // 时间戳结构体
    (void)clock_gettime(CLOCK_REALTIME, &now);  // 获取当前系统时间

    header->len = len;              // 设置消息体长度
    header->pid = LOS_GetCurrProcessID();  // 设置当前进程ID
    header->taskId = LOS_CurTaskIDGet();   // 设置当前任务ID
    header->sec = now.tv_sec;       // 设置秒级时间戳
    header->nsec = now.tv_nsec;     // 设置纳秒级时间戳
    header->hdrSize = sizeof(struct HiLogEntry);  // 设置头部大小
}

/**
 * @brief 当日志缓冲区满时覆盖旧日志
 * @param bufLen 新日志数据长度
 * @note 当缓冲区空间不足时，删除最旧的日志条目直到有足够空间
 */
static void HiLogCoverOldLog(size_t bufLen)
{
    int retval;                     // 函数返回值
    struct HiLogEntry header;       // 日志条目头部
    // 计算新日志所需总空间（头部+消息体）
    size_t totalSize = bufLen + sizeof(struct HiLogEntry);
    static int dropLogLines = 0;    // 丢弃日志计数（静态变量）
    static int isLastTimeFull = 0;  // 上次是否缓冲区满标志
    int isThisTimeFull = 0;         // 本次是否缓冲区满标志

    // 循环删除旧日志直到有足够空间
    while (totalSize + g_hiLogDev.size > HILOG_BUFFER) {
        // 读取最旧日志的头部
        retval = HiLogReadRingBuffer((unsigned char *)&header, sizeof(header));
        if (retval < 0) {
            break;                  // 读取失败，退出循环
        }

        dropLogLines++;              // 增加丢弃计数
        isThisTimeFull = 1;         // 设置本次缓冲区满标志
        isLastTimeFull = 1;         // 设置上次缓冲区满标志
        HiLogBufferDec(sizeof(header));  // 减少缓冲区已用大小（头部）
        HiLogBufferDec(header.len);     // 减少缓冲区已用大小（消息体）
    }
    // 如果上次满但本次不满（缓冲区刚刚腾出空间）
    if (isLastTimeFull == 1 && isThisTimeFull == 0) {
        /* 短时间内缓冲区满只打印一次丢弃日志信息 */
        if (dropLogLines > 0) {
            PRINTK("hilog ringbuffer full, drop %d line(s) log\n", dropLogLines);
        }
        isLastTimeFull = 0;         // 重置上次满标志
        dropLogLines = 0;           // 重置丢弃计数
    }
}

/**
 * @brief 日志写入内部实现函数
 * @param buffer 日志数据缓冲区
 * @param bufLen 日志数据长度
 * @return 成功时返回写入的字节数；失败时返回负数错误码
 * @note 处理日志写入的核心逻辑，包括缓冲区检查、加锁和唤醒等待进程
 */
int HiLogWriteInternal(const char *buffer, size_t bufLen)
{
    struct HiLogEntry header;       // 日志条目头部
    int retval;                     // 函数返回值
    LosTaskCB *runTask =  (LosTaskCB *)OsCurrTaskGet();  // 获取当前任务控制块

    // 如果缓冲区未初始化、处于中断上下文或系统任务，则直接打印
    if ((g_hiLogDev.buffer == NULL) || (OS_INT_ACTIVE) || (runTask->taskStatus & OS_TASK_FLAG_SYSTEM_TASK)) {
        PRINTK("%s\n", buffer);   // 直接使用PRINTK输出
        return -EAGAIN;             // 返回重试错误
    }

    (VOID)LOS_MuxAcquire(&g_hiLogDev.mtx);  // 获取互斥锁，保护缓冲区操作
    HiLogCoverOldLog(bufLen);       // 确保有足够空间（必要时覆盖旧日志）
    HiLogHeadInit(&header, bufLen); // 初始化日志头部

    // 写入日志头部到环形缓冲区
    retval = HiLogWriteRingBuffer((unsigned char *)&header, sizeof(header));
    if (retval) {
        retval = -ENODATA;          // 写入失败，设置无数据错误
        goto out;                   // 跳转到释放资源处
    }
    HiLogBufferInc(sizeof(header));  // 增加缓冲区已用大小（头部）

    // 写入日志消息体到环形缓冲区
    retval = HiLogWriteRingBuffer((unsigned char *)(buffer), header.len);
    if (retval) {
        retval = -ENODATA;          // 写入失败，设置无数据错误
        goto out;                   // 跳转到释放资源处
    }

    HiLogBufferInc(header.len);     // 增加缓冲区已用大小（消息体）
    retval = header.len;            // 设置返回值为消息体长度

out:
    (VOID)LOS_MuxRelease(&g_hiLogDev.mtx);  // 释放互斥锁
    if (retval > 0) {
        wake_up_interruptible(&g_hiLogDev.wq);  // 唤醒等待读的进程
    }
    if (retval < 0) {
        PRINTK("write fail retval=%d\n", retval);  // 打印写入失败信息
    }
    return retval;                  // 返回结果
}

/**
 * @brief 日志设备写入函数
 * @param filep 文件指针
 * @param buffer 待写入的数据缓冲区
 * @param bufLen 待写入数据长度
 * @return 成功时返回写入的字节数；失败时返回负数错误码
 * @note 对输入参数进行验证后调用内部写入函数
 */
static ssize_t HiLogWrite(struct file *filep, const char *buffer, size_t bufLen)
{
    (void)filep;                    // 未使用的参数
    // 计算总所需空间（头部+消息体）
    size_t totalBufLen = bufLen + sizeof(struct HiLogEntry);
    // 检查是否溢出或超出缓冲区总大小
    if ((totalBufLen < bufLen) || (totalBufLen > HILOG_BUFFER)) {
        PRINTK("input bufLen %lld too large\n", bufLen);  // 打印缓冲区过大信息
        return -ENOMEM;             // 返回内存不足错误
    }

    return HiLogWriteInternal(buffer, bufLen);  // 调用内部写入函数
}

/**
 * @brief 日志设备初始化函数
 * @note 完成缓冲区分配、等待队列初始化和互斥锁初始化
 */
static void HiLogDeviceInit(void)
{
    // 从系统内存分配日志缓冲区
    g_hiLogDev.buffer = LOS_MemAlloc((VOID *)OS_SYS_MEM_ADDR, HILOG_BUFFER);
    if (g_hiLogDev.buffer == NULL) {
        PRINTK("In %s line %d,LOS_MemAlloc fail\n", __FUNCTION__, __LINE__);  // 打印分配失败信息
    }

    init_waitqueue_head(&g_hiLogDev.wq);  // 初始化等待队列
    LOS_MuxInit(&g_hiLogDev.mtx, NULL);   // 初始化互斥锁

    // 初始化缓冲区指针和状态
    g_hiLogDev.writeOffset = 0;
    g_hiLogDev.headOffset = 0;
    g_hiLogDev.size = 0;
    g_hiLogDev.count = 0;
}

/**
 * @brief 日志驱动初始化函数
 * @return 0 - 成功；其他值 - 失败
 * @note 作为驱动入口点，完成设备初始化并注册字符设备
 */
int OsHiLogDriverInit(VOID)
{
    HiLogDeviceInit();              // 初始化日志设备
    // 注册字符设备驱动
    return register_driver(HILOG_DRIVER, &g_hilogFops, DRIVER_MODE, NULL);
}

// 将日志驱动初始化函数注册到内核扩展模块初始化阶段
LOS_MODULE_INIT(OsHiLogDriverInit, LOS_INIT_LEVEL_KMOD_EXTENDED);
