/*!
 * @file    los_vdso.c
 * @brief
 * @link 	 http://weharmonyos.com/openharmony/zh-cn/device-dev/kernel/kernel-small-bundles-share.html
 			 http://lishiwen4.github.io/linux/vdso-and-syscall
		  	 https://vvl.me/2019/06/linux-syscall-and-vsyscall-vdso-in-x86/
 * @endlink
 * @verbatim
  基本概念:
	  VDSO（Virtual Dynamic Shared Object，虚拟动态共享库）相对于普通的动态共享库，区别在于
	  其so文件不保存在文件系统中，存在于系统镜像中，由内核在运行时确定并提供给应用程序，故称为虚拟动态共享库。
	  OpenHarmony系统通过VDSO机制实现上层用户态程序可以快速读取内核相关数据的一种通道方法，
	  可用于实现部分系统调用的加速，也可用于实现非系统敏感数据（硬件配置、软件配置）的快速读取。

	  运行机制:
	  VDSO其核心思想就是内核看护一段内存，并将这段内存映射（只读）进用户态应用程序的地址空间，
	  应用程序通过链接vdso.so后，将某些系统调用替换为直接读取这段已映射的内存从而避免系统调用达到加速的效果。
	  VDSO总体可分为数据页与代码页两部分：
		  数据页提供内核映射给用户进程的内核时数据；
		  代码页提供屏蔽系统调用的主要逻辑；

      如下图所示，当前VDSO机制有以下几个主要步骤：

		① 内核初始化时进行VDSO数据页的创建；

		② 内核初始化时进行VDSO代码页的创建；

		③ 根据系统时钟中断不断将内核一些数据刷新进VDSO的数据页；

		④ 用户进程创建时将代码页映射进用户空间；

		⑤ 用户程序在动态链接时对VDSO的符号进行绑定；

		⑥ 当用户程序进行特定系统调用时（例如clock_gettime(CLOCK_REALTIME_COARSE， &ts)），VDSO代码页会将其拦截；

		⑦ VDSO代码页将正常系统调用转为直接读取映射好的VDSO数据页；

		⑧ 从VDSO数据页中将数据传回VDSO代码页；

		⑨ 将从VDSO数据页获取到的数据作为结果返回给用户程序；
 @endverbatim
 * @image html https://gitee.com/weharmonyos/resources/raw/master/82/vdso.jpg
 * @attention 当前VDSO机制支持LibC库clock_gettime接口的CLOCK_REALTIME_COARSE与CLOCK_MONOTONIC_COARSE功能，
 	clock_gettime接口的使用方法详见POSIX标准。用户调用C库接口clock_gettime(CLOCK_REALTIME_COARSE, &ts)
 	或者clock_gettime(CLOCK_MONOTONIC_COARSE, &ts)即可使用VDSO机制。
	使用VDSO机制得到的时间精度会与系统tick中断的精度保持一致，适用于对时间没有高精度要求且短时间内会
	高频触发clock_gettime或gettimeofday系统调用的场景，若有高精度要求，不建议采用VDSO机制。
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-11-24
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

#include "los_vdso_pri.h"
#include "los_vdso_datapage.h"
#include "los_init.h"
#include "los_vm_map.h"
#include "los_vm_lock.h"
#include "los_vm_phys.h"
#include "los_process_pri.h"


LITE_VDSO_DATAPAGE VdsoDataPage g_vdsoDataPage __attribute__((__used__));///< 数据页提供内核映射给用户进程的内核时数据

STATIC size_t g_vdsoSize; ///< 虚拟动态共享库大小
/// vdso初始化
UINT32 OsVdsoInit(VOID)
{//so文件不保存在文件系统中，而是存在于系统镜像中,镜像中的位置在代码区和数据区中间
    g_vdsoSize = &__vdso_text_end - &__vdso_data_start;//VDSO 会映射两块内存区域：代码段 [vdso]，只读数据区 [vvar] 数据页
	//先检查这是不是一个 ELF文件
    if (memcmp((CHAR *)(&__vdso_text_start), ELF_HEAD, ELF_HEAD_LEN)) {//.incbin OHOS_VDSO_SO 将原封不动的一个二进制文件编译到当前文件中 
        PRINT_ERR("VDSO Init Failed!\n");
        return LOS_NOK;
    }
    return LOS_OK;
}

LOS_MODULE_INIT(OsVdsoInit, LOS_INIT_LEVEL_KMOD_EXTENDED);//注册vdso模块
/// 映射,这里先通过内核地址找到 vdso的物理地址,再将物理地址映射到进程的线性区.
/// 结论是每个进程都可以拥有自己的 vdso区,映射到同一个块物理地址.
STATIC INT32 OsVdsoMap(LosVmSpace *space, size_t len, PADDR_T paddr, VADDR_T vaddr, UINT32 flag)
{
    STATUS_T ret;

    while (len > 0) {
        ret = LOS_ArchMmuMap(&(space->archMmu), vaddr, paddr, 1, flag);
        if (ret != 1) {
            PRINT_ERR("VDSO Load Failed! : LOS_ArchMmuMap!\n");
            return LOS_NOK;
        }
        paddr += PAGE_SIZE;
        vaddr += PAGE_SIZE;
        len -= PAGE_SIZE;
    }
    return LOS_OK;
}

/*!
 * @brief OsVdsoLoad 为指定进程加载vdso	
 * 本质是将系统镜像中的vsdo部分映射到进程空间
 * @param processCB	
 * @return	
 *
 * @see
 */
vaddr_t OsVdsoLoad(const LosProcessCB *processCB)
{
    INT32 ret = -1;
    LosVmMapRegion *vdsoRegion = NULL;
    UINT32 flag = VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_EXECUTE;
	//用户区,可读可执行
    if ((processCB == NULL) || (processCB->vmSpace == NULL)) {
        return 0;
    }

    (VOID)LOS_MuxAcquire(&processCB->vmSpace->regionMux);
	//由虚拟空间提供加载内存
    vdsoRegion = LOS_RegionAlloc(processCB->vmSpace, 0, g_vdsoSize, flag, 0);//申请进程空间 g_vdsoSize 大小线性区,用于映射
    if (vdsoRegion == NULL) {
        PRINT_ERR("%s %d, region alloc failed in vdso load\n", __FUNCTION__, __LINE__);
        goto LOCK_RELEASE;
    }
    vdsoRegion->regionFlags |= VM_MAP_REGION_FLAG_VDSO;//标识为 vdso区
	//为vsdo做好映射,如此通过访问进程的虚拟地址就可以访问到真正的vsdo
    ret = OsVdsoMap(processCB->vmSpace, g_vdsoSize, LOS_PaddrQuery((VOID *)(&__vdso_data_start)),
                    vdsoRegion->range.base, flag);//__vdso_data_start 为 vdso 的起始地址
    if (ret != LOS_OK) {//映射失败就释放线性区
        ret = LOS_RegionFree(processCB->vmSpace, vdsoRegion);
        if (ret) {
            PRINT_ERR("%s %d, free region failed, ret = %d\n", __FUNCTION__, __LINE__, ret);
        }
        ret = -1;
    }

LOCK_RELEASE:
    (VOID)LOS_MuxRelease(&processCB->vmSpace->regionMux);
    if (ret == LOS_OK) {
        return (vdsoRegion->range.base + PAGE_SIZE);
    }
    return 0;
}
/// 对数据页加锁
STATIC VOID LockVdsoDataPage(VdsoDataPage *vdsoDataPage)
{
    vdsoDataPage->lockCount = 1;
    DMB;
}
/// 对数据页加锁
STATIC VOID UnlockVdsoDataPage(VdsoDataPage *vdsoDataPage)
{
    DMB;
    vdsoDataPage->lockCount = 0;
}
 
/*!
 * @brief OsVdsoTimevalUpdate	
 * 更新时间,根据系统时钟中断不断将内核一些数据刷新进VDSO的数据页；
 * @return	
 *
 * @see OsTickHandler 函数
 */
VOID OsVdsoTimevalUpdate(VOID)
{
    VdsoDataPage *kVdsoDataPage = (VdsoDataPage *)(&__vdso_data_start);//获取vdso 数据区

    LockVdsoDataPage(kVdsoDataPage);//锁住数据页
    OsVdsoTimeGet(kVdsoDataPage);	//更新数据页时间
    UnlockVdsoDataPage(kVdsoDataPage);//解锁数据页
}
