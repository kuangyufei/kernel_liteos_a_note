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

#include "los_base.h"
#include "los_hwi.h"
#ifdef LOSCFG_SHELL
#include "shcmd.h"
#endif
#ifdef LOSCFG_FS_VFS
#include "fs/fs.h"
#include "fs/fs_operation.h"
#endif

/*
 * 条件编译块：当启用异常信息保存功能(LOSCFG_SAVE_EXCINFO)时编译以下代码
 * 功能：实现异常信息的采集、存储、读取和Shell命令查询机制
 */
#ifdef LOSCFG_SAVE_EXCINFO
/* 异常信息读写钩子函数指针，用于自定义异常信息的读写实现 */
STATIC log_read_write_fn g_excInfoRW = NULL; /* the hook of read-writing exception information */
/* 异常信息缓冲区指针，用于存储格式化后的异常信息字符串 */
STATIC CHAR *g_excInfoBuf = NULL;            /* pointer to the buffer for storing the exception information */
/* 异常信息缓冲区当前写入索引，记录下次写入位置 */
STATIC UINT32 g_excInfoIndex = 0xFFFFFFFF;   /* the index of the buffer for storing the exception information */
/* 异常信息存储起始地址，用于定位异常信息在系统中的存储位置 */
STATIC UINT32 g_recordAddr = 0;              /* the address of storing the exception information */
/* 异常信息存储空间大小，定义缓冲区的最大容量 */
STATIC UINT32 g_recordSpace = 0;             /* the size of storing the exception information */

/*
 * 功能：设置异常信息读写钩子函数
 * 参数：
 *   func - 日志读写函数指针，类型为log_read_write_fn
 * 返回值：无
 * 说明：钩子函数用于自定义异常信息的读写逻辑，可由用户实现具体存储介质操作
 */
VOID SetExcInfoRW(log_read_write_fn func)
{
    g_excInfoRW = func;
}

/*
 * 功能：获取当前设置的异常信息读写钩子函数
 * 参数：无
 * 返回值：log_read_write_fn类型，当前钩子函数指针，未设置则返回NULL
 * 说明：用于Shell命令读取异常信息时调用注册的钩子函数
 */
log_read_write_fn GetExcInfoRW(VOID)
{
    return g_excInfoRW;
}

/*
 * 功能：设置异常信息缓冲区指针
 * 参数：
 *   buf - 字符型指针，指向预分配的异常信息存储缓冲区
 * 返回值：无
 * 说明：缓冲区需在调用LOS_ExcInfoRegHook前完成分配，大小应不小于recordSpace
 */
VOID SetExcInfoBuf(CHAR *buf)
{
    g_excInfoBuf = buf;
}

/*
 * 功能：获取当前异常信息缓冲区指针
 * 参数：无
 * 返回值：CHAR*类型，当前缓冲区指针，未设置则返回NULL
 */
CHAR *GetExcInfoBuf(VOID)
{
    return g_excInfoBuf;
}

/*
 * 功能：设置异常信息缓冲区写入索引
 * 参数：
 *   index - 新的写入位置索引，初始值通常为0
 * 返回值：无
 * 说明：索引值应小于缓冲区大小(recordSpace)，否则写入会失败
 */
VOID SetExcInfoIndex(UINT32 index)
{
    g_excInfoIndex = index;
}

/*
 * 功能：获取当前异常信息缓冲区写入索引
 * 参数：无
 * 返回值：UINT32类型，当前写入位置索引
 */
UINT32 GetExcInfoIndex(VOID)
{
    return g_excInfoIndex;
}

/*
 * 功能：设置异常信息存储起始地址
 * 参数：
 *   addr - 存储区域起始地址（物理地址或虚拟地址，取决于钩子函数实现）
 * 返回值：无
 */
VOID SetRecordAddr(UINT32 addr)
{
    g_recordAddr = addr;
}

/*
 * 功能：获取异常信息存储起始地址
 * 参数：无
 * 返回值：UINT32类型，当前设置的存储起始地址
 */
UINT32 GetRecordAddr(VOID)
{
    return g_recordAddr;
}

/*
 * 功能：设置异常信息存储空间大小
 * 参数：
 *   space - 存储区域大小，单位为字节
 * 返回值：无
 * 说明：应与缓冲区大小保持一致，用于边界检查
 */
VOID SetRecordSpace(UINT32 space)
{
    g_recordSpace = space;
}

/*
 * 功能：获取异常信息存储空间大小
 * 参数：无
 * 返回值：UINT32类型，当前设置的存储空间大小
 */
UINT32 GetRecordSpace(VOID)
{
    return g_recordSpace;
}

/*
 * 功能：使用可变参数列表向异常缓冲区写入格式化信息
 * 参数：
 *   format - 格式化字符串
 *   arglist - 可变参数列表
 * 返回值：无
 * 说明：内部使用vsnprintf_s进行安全格式化，自动更新写入索引，缓冲区不足时打印错误
 */
VOID WriteExcBufVa(const CHAR *format, va_list arglist)
{
    errno_t ret;                               /* vsnprintf_s返回值，0表示成功，-1表示失败 */

    /* 检查缓冲区是否有剩余空间 */
    if (g_recordSpace > g_excInfoIndex) {
        /* 安全格式化输出到缓冲区，保留1字节用于终止符 */
        ret = vsnprintf_s((g_excInfoBuf + g_excInfoIndex), (g_recordSpace - g_excInfoIndex),
                          (g_recordSpace - g_excInfoIndex - 1), format, arglist);
        if (ret == -1) {                       /* 格式化失败（缓冲区不足或格式错误） */
            PRINT_ERR("exc info buffer is not enough or vsnprintf_s is error.\n");
            return;
        }
        g_excInfoIndex += ret;                 /* 更新写入索引，指向下一个空闲位置 */
    }
}

/*
 * 功能：向异常缓冲区写入格式化异常信息（可变参数版本）
 * 参数：
 *   format - 格式化字符串
 *   ... - 可变参数列表
 * 返回值：无
 * 说明：封装WriteExcBufVa，提供标准printf风格的接口，自动处理参数列表
 */
VOID WriteExcInfoToBuf(const CHAR *format, ...)
{
    va_list arglist;                           /* 可变参数列表 */

    va_start(arglist, format);                  /* 初始化参数列表 */
    WriteExcBufVa(format, arglist);            /* 调用内部实现函数 */
    va_end(arglist);                           /* 清理参数列表 */
}

/*
 * 功能：注册异常信息处理钩子函数和缓冲区
 * 参数：
 *   startAddr - 异常信息存储起始地址
 *   space - 存储空间大小（字节）
 *   buf - 异常信息缓冲区指针
 *   hook - 读写钩子函数指针，为NULL时使用默认实现
 * 返回值：无
 * 说明：必须在系统初始化阶段调用，用于设置异常信息的存储机制，若启用VFS则初始化文件系统
 */
VOID LOS_ExcInfoRegHook(UINT32 startAddr, UINT32 space, CHAR *buf, log_read_write_fn hook)
{
    /* 参数合法性检查：钩子函数和缓冲区不可为NULL */
    if ((hook == NULL) || (buf == NULL)) {
        PRINT_ERR("Buf or hook is null.\n");
        return;
    }

    /* 初始化全局变量 */
    g_recordAddr = startAddr;
    g_recordSpace = space;
    g_excInfoBuf = buf;
    g_excInfoRW = hook;

#ifdef LOSCFG_FS_VFS                           /* 若启用虚拟文件系统(VFS) */
    los_vfs_init();                            /* 初始化VFS，用于文件系统操作 */
#endif
}

/*
 * 功能：异常发生时读写异常信息（预留接口）
 * 参数：
 *   startAddr - 存储起始地址
 *   space - 存储空间大小
 *   flag - 操作标志（1表示读取，0表示写入，具体由钩子函数定义）
 *   buf - 数据缓冲区
 * 返回值：无
 * 说明：在异常处理上下文中调用，用户可在此处实现异常信息的持久化存储（如写入文件）
 */
/* Be called in the exception. */
VOID OsReadWriteExceptionInfo(UINT32 startAddr, UINT32 space, UINT32 flag, CHAR *buf)
{
    /* 参数合法性检查：缓冲区不可为NULL且空间大小必须大于0 */
    if ((buf == NULL) || (space == 0)) {
        PRINT_ERR("buffer is null or space is zero\n");
        return;
    }
    // user can write exception information to files here
}

/*
 * 功能：记录异常发生时间并写入缓冲区
 * 参数：无
 * 返回值：无
 * 说明：仅在启用VFS时有效，使用localtime获取本地时间，格式化后写入异常缓冲区
 */
VOID OsRecordExcInfoTime(VOID)
{
#ifdef LOSCFG_FS_VFS                           /* 依赖文件系统配置项 */
#define NOW_TIME_LENGTH 24                     /* 时间字符串长度：YYYY-MM-DD HH:MM:SS + 终止符 */
    time_t t;                                  /* 时间戳变量 */
    struct tm *tmTime = NULL;                  /* 分解时间结构体指针 */
    CHAR nowTime[NOW_TIME_LENGTH];             /* 时间字符串缓冲区 */

    (VOID)time(&t);                            /* 获取当前时间戳 */
    tmTime = localtime(&t);                    /* 转换为本地时间结构体 */
    if (tmTime == NULL) {                      /* 转换失败则返回 */
        return;
    }
    (VOID)memset_s(nowTime, sizeof(nowTime), 0, sizeof(nowTime)); /* 清空缓冲区 */
    /* 格式化时间字符串为"YYYY-MM-DD HH:MM:SS"格式 */
    (VOID)strftime(nowTime, NOW_TIME_LENGTH, "%Y-%m-%d %H:%M:%S", tmTime);
#undef NOW_TIME_LENGTH                         /* 取消宏定义 */
    WriteExcInfoToBuf("%s \n", nowTime);      /* 写入时间信息到异常缓冲区 */
#endif
}

#ifdef LOSCFG_SHELL                            /* 若启用Shell命令支持 */
/*
 * 功能：Shell命令处理函数，读取并打印异常信息
 * 参数：
 *   argc - 命令参数个数
 *   argv - 命令参数数组
 * 返回值：INT32类型，成功返回LOS_OK，内存分配失败返回LOS_NOK
 * 说明：忽略命令参数，从注册的钩子函数读取异常信息并打印，使用对齐内存分配
 */
INT32 OsShellCmdReadExcInfo(INT32 argc, CHAR **argv)
{
#define EXCINFO_ALIGN_SIZE 64                  /* 内存对齐大小，64字节对齐 */
    UINT32 recordSpace = GetRecordSpace();     /* 获取异常信息存储大小 */

    (VOID)argc;                                /* 未使用参数，避免编译警告 */
    (VOID)argv;

    /* 分配对齐内存缓冲区，大小为recordSpace+1（额外1字节用于终止符） */
    CHAR *buf = (CHAR *)LOS_MemAllocAlign((VOID *)OS_SYS_MEM_ADDR, recordSpace + 1, EXCINFO_ALIGN_SIZE);
    if (buf == NULL) {                         /* 内存分配失败 */
        return LOS_NOK;
    }
    (VOID)memset_s(buf, recordSpace + 1, 0, recordSpace + 1); /* 清空缓冲区 */

    log_read_write_fn hook = GetExcInfoRW();   /* 获取注册的读写钩子函数 */
    if (hook != NULL) {
        hook(GetRecordAddr(), recordSpace, 1, buf); /* 调用钩子函数读取异常信息（flag=1表示读取） */
    }
    PRINTK("%s\n", buf);                      /* 打印异常信息 */
    (VOID)LOS_MemFree((void *)OS_SYS_MEM_ADDR, buf); /* 释放内存 */
    buf = NULL;                                /* 避免野指针 */
    return LOS_OK;
}

/*
 * Shell命令注册宏 - 将异常信息读取命令注册到系统
 * 参数说明：
 *   readExcInfo_shellcmd - 命令结构体名称
 *   CMD_TYPE_EX - 命令类型（扩展命令）
 *   "excInfo" - 命令名称，在Shell中执行该命令可读取异常信息
 *   0 - 命令权限（0表示普通用户可执行）
 *   (CmdCallBackFunc)OsShellCmdReadExcInfo - 命令回调函数
 */
SHELLCMD_ENTRY(readExcInfo_shellcmd, CMD_TYPE_EX, "excInfo", 0, (CmdCallBackFunc)OsShellCmdReadExcInfo);
#endif
#endif

