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
 * @image 
 * @attention 当前VDSO机制支持LibC库clock_gettime接口的CLOCK_REALTIME_COARSE与CLOCK_MONOTONIC_COARSE功能，
 	clock_gettime接口的使用方法详见POSIX标准。用户调用C库接口clock_gettime(CLOCK_REALTIME_COARSE, &ts)
 	或者clock_gettime(CLOCK_MONOTONIC_COARSE, &ts)即可使用VDSO机制。
	使用VDSO机制得到的时间精度会与系统tick中断的精度保持一致，适用于对时间没有高精度要求且短时间内会
	高频触发clock_gettime或gettimeofday系统调用的场景，若有高精度要求，不建议采用VDSO机制。
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2025-07-17
 */

#include "los_vdso_pri.h"
#include "los_vdso_datapage.h"
#include "los_init.h"
#include "los_vm_map.h"
#include "los_vm_lock.h"
#include "los_vm_phys.h"
#include "los_process_pri.h"
/**
 * @brief VDSO数据页全局实例
 * @details 存储内核导出的用户空间可见数据，使用LITE_VDSO_DATAPAGE宏定义确保正确的内存属性
 * @note __attribute__((__used__)) 用于防止编译器优化删除未直接引用的全局变量
 */
LITE_VDSO_DATAPAGE VdsoDataPage g_vdsoDataPage __attribute__((__used__));

/**
 * @brief VDSO区域大小全局变量
 * @details 存储VDSO代码和数据区域的总大小，单位为字节
 */
STATIC size_t g_vdsoSize;

/**
 * @brief VDSO初始化函数
 * @details 计算VDSO区域大小并验证ELF头特征，确保VDSO镜像有效性
 * @return UINT32 初始化结果
 *         LOS_OK：初始化成功
 *         LOS_NOK：初始化失败（ELF头验证不通过）
 * @note 该函数在系统启动阶段被调用，属于内核扩展模块初始化
 */
UINT32 OsVdsoInit(VOID)
{
    // 计算VDSO总大小：从数据段起始到代码段结束
    g_vdsoSize = &__vdso_text_end - &__vdso_data_start;

    // 验证VDSO的ELF头特征（前ELF_HEAD_LEN字节）
    if (memcmp((CHAR *)(&__vdso_text_start), ELF_HEAD, ELF_HEAD_LEN)) {
        PRINT_ERR("VDSO Init Failed!\n");  // 打印初始化失败错误信息
        return LOS_NOK;                     // 返回初始化失败
    }
    return LOS_OK;  // 返回初始化成功
}

/**
 * @brief 模块初始化宏：注册VDSO初始化函数
 * @details 将OsVdsoInit函数注册为内核扩展模块，在LOS_INIT_LEVEL_KMOD_EXTENDED阶段执行
 * @note 初始化级别确保VDSO在进程管理模块之后、用户进程启动之前完成初始化
 */
LOS_MODULE_INIT(OsVdsoInit, LOS_INIT_LEVEL_KMOD_EXTENDED);

/**
 * @brief VDSO内存映射函数
 * @details 将VDSO物理内存区域映射到进程虚拟地址空间，支持跨多页映射
 * @param space [输入] 进程虚拟地址空间结构体指针
 * @param len [输入] 映射长度（字节），必须是PAGE_SIZE的整数倍
 * @param paddr [输入] 物理起始地址，必须页对齐
 * @param vaddr [输入] 虚拟起始地址，必须页对齐
 * @param flag [输入] 映射标志位，包括权限和属性设置
 * @return INT32 映射结果
 *         LOS_OK：映射成功
 *         LOS_NOK：映射失败（页表操作错误）
 * @note 循环映射每个页，直到完成全部长度映射或发生错误
 */
STATIC INT32 OsVdsoMap(LosVmSpace *space, size_t len, PADDR_T paddr, VADDR_T vaddr, UINT32 flag)
{
    STATUS_T ret;  // 页表映射操作返回值

    // 循环映射每一页（4KB），直到剩余长度为0
    while (len > 0) {
        // 映射单个页表项，1表示映射1个页
        ret = LOS_ArchMmuMap(&(space->archMmu), vaddr, paddr, 1, flag);
        if (ret != 1) {  // 映射失败（返回值不等于1表示映射页数为0）
            PRINT_ERR("VDSO Load Failed! : LOS_ArchMmuMap!\n");
            return LOS_NOK;
        }
        paddr += PAGE_SIZE;  // 物理地址向后移动一页（4KB）
        vaddr += PAGE_SIZE;  // 虚拟地址向后移动一页（4KB）
        len -= PAGE_SIZE;    // 剩余长度减少一页（4KB）
    }
    return LOS_OK;  // 所有页映射成功
}

/**
 * @brief 加载VDSO到进程地址空间
 * @details 为指定进程分配虚拟地址空间并映射VDSO区域，返回用户空间入口地址
 * @param processCB [输入] 进程控制块指针，指定要加载VDSO的目标进程
 * @return vaddr_t VDSO用户空间入口地址；失败返回0
 * @note VDSO区域权限为用户可读写执行（VM_MAP_REGION_FLAG_PERM_*组合）
 */
vaddr_t OsVdsoLoad(const LosProcessCB *processCB)
{
    INT32 ret = -1;  // 函数返回值，初始化为-1（失败）
    LosVmMapRegion *vdsoRegion = NULL;  // VDSO内存区域描述符
    // VDSO区域权限标志：用户空间、可读、可执行（无写权限）
    UINT32 flag = VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_EXECUTE;

    // 参数合法性检查：进程控制块和虚拟地址空间必须有效
    if ((processCB == NULL) || (processCB->vmSpace == NULL)) {
        return 0;  // 参数无效，返回0表示失败
    }

    (VOID)LOS_MuxAcquire(&processCB->vmSpace->regionMux);  // 获取虚拟地址空间互斥锁

    // 分配VDSO内存区域：从0地址开始查找，长度为g_vdsoSize，权限为flag
    vdsoRegion = LOS_RegionAlloc(processCB->vmSpace, 0, g_vdsoSize, flag, 0);
    if (vdsoRegion == NULL) {  // 区域分配失败
        PRINT_ERR("%s %d, region alloc failed in vdso load\n", __FUNCTION__, __LINE__);
        goto LOCK_RELEASE;  // 跳转到释放锁标签
    }
    vdsoRegion->regionFlags |= VM_MAP_REGION_FLAG_VDSO;  // 标记为VDSO特殊区域

    // 映射VDSO物理内存到虚拟地址空间
    ret = OsVdsoMap(processCB->vmSpace, g_vdsoSize, LOS_PaddrQuery((VOID *)(&__vdso_data_start)),
                    vdsoRegion->range.base, flag);
    if (ret != LOS_OK) {  // 映射失败，需要回滚操作
        ret = LOS_RegionFree(processCB->vmSpace, vdsoRegion);  // 释放已分配的区域
        if (ret) {  // 区域释放失败
            PRINT_ERR("%s %d, free region failed, ret = %d\n", __FUNCTION__, __LINE__, ret);
        }
        ret = -1;  // 设置返回值为-1（失败）
    }

LOCK_RELEASE:  // 锁释放标签
    (VOID)LOS_MuxRelease(&processCB->vmSpace->regionMux);  // 释放虚拟地址空间互斥锁
    if (ret == LOS_OK) {
        // 返回VDSO用户空间入口地址（跳过数据页，指向代码页起始）
        return (vdsoRegion->range.base + PAGE_SIZE);
    }
    return 0;  // 失败返回0
}

/**
 * @brief 锁定VDSO数据页
 * @details 通过设置lockCount和内存屏障确保数据更新的原子性和可见性
 * @param vdsoDataPage [输入] VDSO数据页指针
 * @note DMB指令确保所有之前的内存访问完成后才执行后续操作
 */
STATIC VOID LockVdsoDataPage(VdsoDataPage *vdsoDataPage)
{
    vdsoDataPage->lockCount = 1;  // 设置锁定标志
    DMB;  // 数据内存屏障，防止指令重排序
}

/**
 * @brief 解锁VDSO数据页
 * @details 通过内存屏障和清除lockCount允许其他线程访问数据页
 * @param vdsoDataPage [输入] VDSO数据页指针
 * @note DMB指令确保所有内存更新完成后才清除锁定标志
 */
STATIC VOID UnlockVdsoDataPage(VdsoDataPage *vdsoDataPage)
{
    DMB;  // 数据内存屏障，确保之前的写操作对其他CPU可见
    vdsoDataPage->lockCount = 0;  // 清除锁定标志
}

/**
 * @brief 更新VDSO数据页中的时间值
 * @details 加锁保护下更新实时时钟和单调时钟值，确保用户空间读取的时间一致性
 * @note 该函数通常由时钟中断处理程序定期调用
 */
VOID OsVdsoTimevalUpdate(VOID)
{
    // 获取内核VDSO数据页基地址（__vdso_data_start为链接脚本定义的符号）
    VdsoDataPage *kVdsoDataPage = (VdsoDataPage *)(&__vdso_data_start);

    LockVdsoDataPage(kVdsoDataPage);    // 锁定数据页防止并发访问
    OsVdsoTimeGet(kVdsoDataPage);       // 获取当前时间并更新到数据页
    UnlockVdsoDataPage(kVdsoDataPage);  // 解锁数据页允许用户空间访问
}