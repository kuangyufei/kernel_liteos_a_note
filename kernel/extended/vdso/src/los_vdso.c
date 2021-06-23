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
/*
	vdso 新型系统调用机制 ,虚拟动态共享库VDSO就是Virtual Dynamic Shared Object，
就是内核提供的虚拟的.so,这类.so文件不在磁盘上，而是包含在内核里面。内核把包含
某一.so的物理页在程序启动的时候映射入其进程的内存空间，对应的程序就可以当普通的.so来使用里头的函数

参考:http://lishiwen4.github.io/linux/vdso-and-syscall
	 https://vvl.me/2019/06/linux-syscall-and-vsyscall-vdso-in-x86/

#cat /proc/self/maps
*/
LITE_VDSO_DATAPAGE VdsoDataPage g_vdsoDataPage __attribute__((__used__));//使用这个数据区

STATIC size_t g_vdsoSize;
//vdso初始化
UINT32 OsVdsoInit(VOID)
{
    g_vdsoSize = &__vdso_text_end - &__vdso_data_start;//vDSO 会映射两块内存区域：代码段 [vdso]，只读数据区 [vvar] 数据页

    if (memcmp((CHAR *)(&__vdso_text_start), ELF_HEAD, ELF_HEAD_LEN)) {
        PRINT_ERR("VDSO Init Failed!\n");
        return LOS_NOK;
    }
    return LOS_OK;
}

LOS_MODULE_INIT(OsVdsoInit, LOS_INIT_LEVEL_KMOD_EXTENDED);
//映射,这里先通过内核地址找到 vdso的物理地址,再将物理地址映射到进程的线性区.
//结论是每个进程都可以拥有自己的 vdso区,映射到同一个块物理地址.
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
//为每个进程加载 vsdo
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
    vdsoRegion = LOS_RegionAlloc(processCB->vmSpace, 0, g_vdsoSize, flag, 0);
    if (vdsoRegion == NULL) {
        PRINT_ERR("%s %d, region alloc failed in vdso load\n", __FUNCTION__, __LINE__);
        goto LOCK_RELEASE;
    }
    vdsoRegion->regionFlags |= VM_MAP_REGION_FLAG_VDSO;//标识为 vdso区
	//为vsdo做好映射,如此通过访问进程的虚拟地址就可以访问到真正的vsdo
    ret = OsVdsoMap(processCB->vmSpace, g_vdsoSize, LOS_PaddrQuery((VOID *)(&__vdso_data_start)),
                    vdsoRegion->range.base, flag);
    if (ret != LOS_OK) {
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
//对数据页加锁
STATIC VOID LockVdsoDataPage(VdsoDataPage *vdsoDataPage)
{
    vdsoDataPage->lockCount = 1;
    DMB;
}
//对数据页加锁
STATIC VOID UnlockVdsoDataPage(VdsoDataPage *vdsoDataPage)
{
    DMB;
    vdsoDataPage->lockCount = 0;
}
//更新时间,
VOID OsVdsoTimevalUpdate(VOID)
{
    VdsoDataPage *kVdsoDataPage = (VdsoDataPage *)(&__vdso_data_start);//获取vdso 数据区

    LockVdsoDataPage(kVdsoDataPage);
    OsVdsoTimeGet(kVdsoDataPage);
    UnlockVdsoDataPage(kVdsoDataPage);
}
