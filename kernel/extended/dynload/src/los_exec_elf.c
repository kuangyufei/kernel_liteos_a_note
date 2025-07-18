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
/*!
 * @file    los_exec_elf.c
 * @brief 
 * @link
   @verbatim
   基本概念
	   OpenHarmony系统的动态加载与链接机制主要是由内核加载器以及动态链接器构成，内核加载器用于加载应用程序以及动态链接器，
	   动态链接器用于加载应用程序所依赖的共享库，并对应用程序和共享库进行符号重定位。与静态链接相比，动态链接是将应用程序
	   与动态库推迟到运行时再进行链接的一种机制。
   动态链接的优势
       1. 多个应用程序可以共享一份代码，最小加载单元为页，相对静态链接可以节约磁盘和内存空间。
	   2. 共享库升级时，理论上将旧版本的共享库覆盖即可（共享库中的接口向下兼容），无需重新链接。
	   3. 加载地址可以进行随机化处理，防止攻击，保证安全性。
   运行机制	   
   @endverbatim
 * @image html 
   @verbatim
   1. 内核将应用程序ELF文件的PT_LOAD段信息映射至进程空间。对于ET_EXEC类型的文件，根据PT_LOAD段中p_vaddr进行固定地址映射；
   		对于ET_DYN类型（位置无关的可执行程序，通过编译选项“-fPIE”得到）的文件，内核通过mmap接口选择base基址进行映射（load_addr = base + p_vaddr）。
   2. 若应用程序是静态链接的（静态链接不支持编译选项“-fPIE”），设置堆栈信息后跳转至应用程序ELF文件中e_entry指定的地址并运行；
   		若程序是动态链接的，应用程序ELF文件中会有PT_INTERP段，保存动态链接器的路径信息（ET_DYN类型）。musl的动态链接器是libc-musl.so的一部分，
   		libc-musl.so的入口即动态链接器的入口。内核通过mmap接口选择base基址进行映射，设置堆栈信息后跳转至base + e_entry（该e_entry为动态链接器的入口）
   		地址并运行动态链接器。
   3. 动态链接器自举并查找应用程序依赖的所有共享库并对导入符号进行重定位，最后跳转至应用程序的e_entry（或base + e_entry），开始运行应用程序。
   @endverbatim
 * @image html 
   @verbatim
   1. 加载器与链接器调用mmap映射PT_LOAD段；
   2. 内核调用map_pages接口查找并映射pagecache已有的缓存；
   3. 程序执行时，内存若无所需代码或数据时触发缺页中断，将elf文件内容读入内存，并将该内存块加入pagecache；
   4. 将已读入文件内容的内存块与虚拟地址区间做映射；
   5. 程序继续执行；
   至此，程序将在不断地缺页中断中执行。
   @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2025-07-18
 */

#include "los_exec_elf.h"
#ifdef LOSCFG_SHELL
#include "show.h"
#endif
#include "los_vm_phys.h"
#include "los_vm_map.h"
#include "los_vm_dump.h"

/**
 * @brief ELF程序执行入口函数
 * @param loadInfo ELF加载信息结构体指针
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATIC INT32 OsExecve(const ELFLoadInfo *loadInfo)
{
    if ((loadInfo == NULL) || (loadInfo->elfEntry == 0)) {  // 参数有效性检查：加载信息或入口地址为空
        return LOS_NOK;
    }

    // 调用执行启动函数，传入入口地址、栈顶、栈基和栈大小
    return OsExecStart((TSK_ENTRY_FUNC)(loadInfo->elfEntry), (UINTPTR)loadInfo->stackTop,
                       loadInfo->stackBase, loadInfo->stackSize);
}

#ifdef LOSCFG_SHELL
/**
 * @brief 获取文件真实路径（仅shell配置启用时有效）
 * @param fileName 文件名
 * @param buf 输出缓冲区
 * @param maxLen 缓冲区最大长度
 * @return 成功返回LOS_OK，失败返回-ENOENT
 */
STATIC INT32 OsGetRealPath(const CHAR *fileName, CHAR *buf, UINT32 maxLen)
{
    CHAR *workingDirectory = NULL;                          // 当前工作目录
    UINT32 len, workPathLen, newLen;                       // 文件名长度、工作目录长度、新路径长度

    if (access(fileName, F_OK) < 0) {                       // 检查文件是否存在
        workingDirectory = OsShellGetWorkingDirectory();    // 获取shell工作目录
        if (workingDirectory == NULL) {                     // 工作目录获取失败
            goto ERR_FILE;
        }
        len = strlen(fileName);                             // 获取文件名长度
        workPathLen = strlen(workingDirectory);             // 获取工作目录长度
        newLen = len + 1 + workPathLen + 1;                 // 计算新路径总长度（工作目录+分隔符+文件名+结束符）
        if (newLen >= maxLen) {                             // 检查缓冲区是否溢出
            return -ENOENT;
        }
        // 拷贝工作目录到缓冲区
        if (strncpy_s(buf, maxLen, workingDirectory, workPathLen) != EOK) {
            PRINT_ERR("strncpy_s failed, errline: %d!\n", __LINE__);
            return -ENOENT;
        }
        buf[workPathLen] = '/';                             // 添加路径分隔符
        // 拷贝文件名到缓冲区
        if (strncpy_s(buf + workPathLen + 1, maxLen - workPathLen - 1, fileName, len) != EOK) {
            PRINT_ERR("strncpy_s failed, errline: %d!\n", __LINE__);
            return -ENOENT;
        }
        buf[newLen] = '\0';                                // 添加字符串结束符
        if (access(buf, F_OK) < 0) {                        // 检查拼接后的路径是否存在
            goto ERR_FILE;
        }
    }

    return LOS_OK;

ERR_FILE:                                                   // 文件不存在错误标签
    return -ENOENT;
}
#endif

/**
 * @brief 拷贝用户空间参数到内核空间
 * @param loadInfo ELF加载信息结构体指针
 * @param fileName 用户空间文件名
 * @param kfileName 内核空间文件名缓冲区
 * @param maxSize 缓冲区最大大小
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC INT32 OsCopyUserParam(ELFLoadInfo *loadInfo, const CHAR *fileName, CHAR *kfileName, UINT32 maxSize)
{
    UINT32 strLen;                                          // 字符串长度
    errno_t err;                                            // 错误码
    static UINT32 userFirstInitFlag = 0;                    // 首次用户进程初始化标志

    if (LOS_IsUserAddress((VADDR_T)(UINTPTR)fileName)) {     // 检查文件名是否在用户地址空间
        // 从用户空间拷贝字符串到内核空间
        err = LOS_StrncpyFromUser(kfileName, fileName, PATH_MAX + 1);
        if (err == -EFAULT) {                               // 拷贝失败
            return err;
        } else if (err > PATH_MAX) {                        // 文件名长度超过最大值
            PRINT_ERR("%s[%d], filename len exceeds maxlen: %d\n", __FUNCTION__, __LINE__, PATH_MAX);
            return -ENAMETOOLONG;
        }
    } else if (LOS_IsKernelAddress((VADDR_T)(UINTPTR)fileName) && (userFirstInitFlag == 0)) {  // 内核地址空间且首次初始化
        /**
         * 第一个用户进程由内核空间的OsUserInit->execve(/bin/init)创建
         * 第一个用户进程创建后，不应再进入此分支
         */
        userFirstInitFlag = 1;                              // 设置初始化完成标志
        strLen = strlen(fileName);                          // 获取文件名长度
        err = memcpy_s(kfileName, PATH_MAX, fileName, strLen);  // 拷贝文件名到内核缓冲区
        if (err != EOK) {                                   // 拷贝失败
            PRINT_ERR("%s[%d], Copy failed! err: %d\n", __FUNCTION__, __LINE__, err);
            return -EFAULT;
        }
    } else {                                                // 地址空间无效
        return -EINVAL;
    }

    loadInfo->fileName = kfileName;                         // 设置加载信息中的文件名
    return LOS_OK;
}

/**
 * @brief 执行ELF文件主函数
 * @param fileName 要执行的文件名
 * @param argv 参数数组
 * @param envp 环境变量数组
 * @return 成功返回栈顶地址，失败返回错误码
 */
INT32 LOS_DoExecveFile(const CHAR *fileName, CHAR * const *argv, CHAR * const *envp)
{
    ELFLoadInfo loadInfo = { 0 };                           // ELF加载信息结构体初始化
    CHAR kfileName[PATH_MAX + 1] = { 0 };                    // 内核空间文件名缓冲区
    INT32 ret;                                              // 返回值
#ifdef LOSCFG_SHELL
    CHAR buf[PATH_MAX + 1] = { 0 };                         // 路径缓冲区（仅shell配置启用时有效）
#endif

    // 参数有效性检查：文件名不为空，参数和环境变量数组位于用户空间
    if ((fileName == NULL) || ((argv != NULL) && !LOS_IsUserAddress((VADDR_T)(UINTPTR)argv)) ||
        ((envp != NULL) && !LOS_IsUserAddress((VADDR_T)(UINTPTR)envp))) {
        return -EINVAL;
    }
    // 拷贝用户参数到内核空间
    ret = OsCopyUserParam(&loadInfo, fileName, kfileName, PATH_MAX);
    if (ret != LOS_OK) {
        return ret;
    }

#ifdef LOSCFG_SHELL
    // 获取文件真实路径
    if (OsGetRealPath(kfileName, buf, (PATH_MAX + 1)) != LOS_OK) {
        return -ENOENT;
    }
    if (buf[0] != '\0') {                                  // 真实路径有效
        loadInfo.fileName = buf;
    }
#endif

    // 创建用户虚拟地址空间
    loadInfo.newSpace = OsCreateUserVmSpace();
    if (loadInfo.newSpace == NULL) {                        // 虚拟地址空间创建失败
        PRINT_ERR("%s %d, failed to allocate new vm space\n", __FUNCTION__, __LINE__);
        return -ENOMEM;
    }

    loadInfo.argv = argv;                                   // 设置参数数组
    loadInfo.envp = envp;                                   // 设置环境变量数组

    // 加载ELF文件
    ret = OsLoadELFFile(&loadInfo);
    if (ret != LOS_OK) {
        return ret;
    }

    // 回收并初始化进程资源
    ret = OsExecRecycleAndInit(OsCurrProcessGet(), loadInfo.fileName, loadInfo.oldSpace, loadInfo.oldFiles);
    if (ret != LOS_OK) {
        (VOID)LOS_VmSpaceFree(loadInfo.oldSpace);           // 释放旧虚拟地址空间
        goto OUT;
    }

    // 执行ELF程序
    ret = OsExecve(&loadInfo);
    if (ret != LOS_OK) {
        goto OUT;
    }

    return loadInfo.stackTop;                               // 返回栈顶地址

OUT:                                                        // 退出标签
    (VOID)LOS_Exit(OS_PRO_EXIT_OK);                         // 退出进程
    return ret;
}
