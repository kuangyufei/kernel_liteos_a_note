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
 * @image html https://gitee.com/weharmonyos/resources/raw/master/51/1.png
   @verbatim
   1. 内核将应用程序ELF文件的PT_LOAD段信息映射至进程空间。对于ET_EXEC类型的文件，根据PT_LOAD段中p_vaddr进行固定地址映射；
   		对于ET_DYN类型（位置无关的可执行程序，通过编译选项“-fPIE”得到）的文件，内核通过mmap接口选择base基址进行映射（load_addr = base + p_vaddr）。
   2. 若应用程序是静态链接的（静态链接不支持编译选项“-fPIE”），设置堆栈信息后跳转至应用程序ELF文件中e_entry指定的地址并运行；
   		若程序是动态链接的，应用程序ELF文件中会有PT_INTERP段，保存动态链接器的路径信息（ET_DYN类型）。musl的动态链接器是libc-musl.so的一部分，
   		libc-musl.so的入口即动态链接器的入口。内核通过mmap接口选择base基址进行映射，设置堆栈信息后跳转至base + e_entry（该e_entry为动态链接器的入口）
   		地址并运行动态链接器。
   3. 动态链接器自举并查找应用程序依赖的所有共享库并对导入符号进行重定位，最后跳转至应用程序的e_entry（或base + e_entry），开始运行应用程序。
   @endverbatim
 * @image html https://gitee.com/weharmonyos/resources/raw/master/51/2.png
   @verbatim
   1. 加载器与链接器调用mmap映射PT_LOAD段；
   2. 内核调用map_pages接口查找并映射pagecache已有的缓存；
   3. 程序执行时，内存若无所需代码或数据时触发缺页中断，将elf文件内容读入内存，并将该内存块加入pagecache；
   4. 将已读入文件内容的内存块与虚拟地址区间做映射；
   5. 程序继续执行；
   至此，程序将在不断地缺页中断中执行。
   @endverbatim
 * @version 
 * @author  weharmonyos.com
 * @date    2021-11-19
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

#include "los_exec_elf.h"
#ifdef LOSCFG_SHELL
#include "show.h"
#endif
#include "los_vm_phys.h"
#include "los_vm_map.h"
#include "los_vm_dump.h"
/// 运行ELF
STATIC INT32 OsExecve(const ELFLoadInfo *loadInfo)
{
    if ((loadInfo == NULL) || (loadInfo->elfEntry == 0)) {
        return LOS_NOK;
    }
	//任务运行的两个硬性要求:1.提供入口指令 2.运行栈空间.
    return OsExecStart((TSK_ENTRY_FUNC)(loadInfo->elfEntry), (UINTPTR)loadInfo->stackTop,
                       loadInfo->stackBase, loadInfo->stackSize);
}

#ifdef LOSCFG_SHELL
STATIC INT32 OsGetRealPath(const CHAR *fileName, CHAR *buf, UINT32 maxLen)
{
    CHAR *workingDirectory = NULL;
    UINT32 len, workPathLen, newLen;

    if (access(fileName, F_OK) < 0) {
        workingDirectory = OsShellGetWorkingDirectory();
        if (workingDirectory == NULL) {
            goto ERR_FILE;
        }
        len = strlen(fileName);
        workPathLen = strlen(workingDirectory);
        newLen = len + 1 + workPathLen + 1;
        if (newLen >= maxLen) {
            return -ENOENT;
        }
        if (strncpy_s(buf, maxLen, workingDirectory, workPathLen) != EOK) {
            PRINT_ERR("strncpy_s failed, errline: %d!\n", __LINE__);
            return -ENOENT;
        }
        buf[workPathLen] = '/';
        if (strncpy_s(buf + workPathLen + 1, maxLen - workPathLen - 1, fileName, len) != EOK) {
            PRINT_ERR("strncpy_s failed, errline: %d!\n", __LINE__);
            return -ENOENT;
        }
        buf[newLen] = '\0';
        if (access(buf, F_OK) < 0) {
            goto ERR_FILE;
        }
    }

    return LOS_OK;

ERR_FILE:
    return -ENOENT;
}
#endif
//拷贝用户参数至内核空间
STATIC INT32 OsCopyUserParam(ELFLoadInfo *loadInfo, const CHAR *fileName, CHAR *kfileName, UINT32 maxSize)
{
    UINT32 strLen;
    errno_t err;

    if (LOS_IsUserAddress((VADDR_T)(UINTPTR)fileName)) {//在用户空间
        err = LOS_StrncpyFromUser(kfileName, fileName, PATH_MAX + 1);//拷贝至内核空间
        if (err == -EFAULT) {
            return err;
        } else if (err > PATH_MAX) {
            PRINT_ERR("%s[%d], filename len exceeds maxlen: %d\n", __FUNCTION__, __LINE__, PATH_MAX);
            return -ENAMETOOLONG;
        }
    } else if (LOS_IsKernelAddress((VADDR_T)(UINTPTR)fileName)) {//已经在内核空间
        strLen = strlen(fileName);
        err = memcpy_s(kfileName, PATH_MAX, fileName, strLen);//拷贝至内核空间
        if (err != EOK) {
            PRINT_ERR("%s[%d], Copy failed! err: %d\n", __FUNCTION__, __LINE__, err);
            return -EFAULT;
        }
    } else {
        return -EINVAL;
    }

    loadInfo->fileName = kfileName;//文件名指向内核空间
    return LOS_OK;
}

/*!
 * @brief LOS_DoExecveFile	
 * 根据fileName执行一个新的用户程序 LOS_DoExecveFile接口一般由用户通过execve系列接口利用系统调用机制调用创建新的进程，内核不能直接调用该接口启动新进程。
 * @param argv	程序执行所需的参数序列，以NULL结尾。无需参数时填入NULL。
 * @param envp	程序执行所需的新的环境变量序列，以NULL结尾。无需新的环境变量时填入NULL。
 * @param fileName	二进制可执行文件名，可以是路径名。
 * @return	
 *
 * @see
 */
INT32 LOS_DoExecveFile(const CHAR *fileName, CHAR * const *argv, CHAR * const *envp)
{
    ELFLoadInfo loadInfo = { 0 };
    CHAR kfileName[PATH_MAX + 1] = { 0 };//此时已陷入内核态,所以局部变量都在内核空间
    INT32 ret;
#ifdef LOSCFG_SHELL
    CHAR buf[PATH_MAX + 1] = { 0 };
#endif
	//先判断参数地址是否来自用户空间,此处必须要来自用户空间,内核是不能直接调用本函数
    if ((fileName == NULL) || ((argv != NULL) && !LOS_IsUserAddress((VADDR_T)(UINTPTR)argv)) ||
        ((envp != NULL) && !LOS_IsUserAddress((VADDR_T)(UINTPTR)envp))) {
        return -EINVAL;
    }
    ret = OsCopyUserParam(&loadInfo, fileName, kfileName, PATH_MAX);//拷贝用户空间数据至内核空间
    if (ret != LOS_OK) {
        return ret;
    }

#ifdef LOSCFG_SHELL
    if (OsGetRealPath(kfileName, buf, (PATH_MAX + 1)) != LOS_OK) {//获取绝对路径
        return -ENOENT;
    }
    if (buf[0] != '\0') {
        loadInfo.fileName = buf;
    }
#endif

    loadInfo.newSpace = OsCreateUserVmSpace();//创建一个用户空间,用于开启新的进程
    if (loadInfo.newSpace == NULL) {
        PRINT_ERR("%s %d, failed to allocate new vm space\n", __FUNCTION__, __LINE__);
        return -ENOMEM;
    }

    loadInfo.argv = argv;//参数数组
    loadInfo.envp = envp;//环境数组

    ret = OsLoadELFFile(&loadInfo);//加载ELF文件
    if (ret != LOS_OK) {
        return ret;
    }
	//对当前进程旧虚拟空间和文件进行回收
    ret = OsExecRecycleAndInit(OsCurrProcessGet(), loadInfo.fileName, loadInfo.oldSpace, loadInfo.oldFiles);
    if (ret != LOS_OK) {
        (VOID)LOS_VmSpaceFree(loadInfo.oldSpace);//释放虚拟空间
        goto OUT;
    }

    ret = OsExecve(&loadInfo);//运行ELF内容
    if (ret != LOS_OK) {
        goto OUT;
    }

    return loadInfo.stackTop;

OUT:
    (VOID)LOS_Exit(OS_PRO_EXIT_OK);
    return ret;
}
