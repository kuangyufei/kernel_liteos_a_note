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
 * @file    los_exc.c
 * @brief 异常接管主文件
 * @link
 * @verbatim
   基本概念 
	   异常接管是操作系统对运行期间发生的异常情况（芯片硬件异常）进行处理的一系列动作，
	   例如打印异常发生时当前函数的调用栈信息、CPU现场信息、任务的堆栈情况等。
   
	   异常接管作为一种调测手段，可以在系统发生异常时给用户提供有用的异常信息，譬如异常类型、
	   发生异常时的系统状态等，方便用户定位分析问题。
   
	   异常接管，在系统发生异常时的处理动作为：显示异常发生时正在运行的任务信息 
	   （包括任务名、任务号、堆栈大小等），以及CPU现场等信息。 
   
   运作机制 
	   每个函数都有自己的栈空间，称为栈帧。调用函数时，会创建子函数的栈帧， 
	   同时将函数入参、局部变量、寄存器入栈。栈帧从高地址向低地址生长。 
	   
	   以ARM32 CPU架构为例，每个栈帧中都会保存PC、LR、SP和FP寄存器的历史值。 
	   ARM处理器中的R13被用作SP 
   
   堆栈分析 
	   LR寄存器（Link Register），链接寄存器，指向函数的返回地址。 
	   R11：可以用作通用寄存器，在开启特定编译选项时可以用作帧指针寄存器FP，用来实现栈回溯功能。 
	   GNU编译器（gcc）默认将R11作为存储变量的通用寄存器，因而默认情况下无法使用FP的栈回溯功能。 
		   为支持调用栈解析功能，需要在编译参数中添加-fno-omit-frame-pointer选项，提示编译器将R11作为FP使用。 
	   FP寄存器（Frame Point），帧指针寄存器，指向当前函数的父函数的栈帧起始地址。利用该寄存器可以得到父函数的栈帧， 
		   从栈帧中获取父函数的FP，就可以得到祖父函数的栈帧，以此类推，可以追溯程序调用栈，得到函数间的调用关系。 
	   当系统发生异常时，系统打印异常函数的栈帧中保存的寄存器内容，以及父函数、祖父函数的 
		   栈帧中的LR、FP寄存器内容，用户就可以据此追溯函数间的调用关系，定位异常原因。 
	   异常接管对系统运行期间发生的芯片硬件异常进行处理，不同芯片的异常类型存在差异，具体异常类型可以查看芯片手册。	
	   
   异常接管一般的定位步骤如下： 
	   打开编译后生成的镜像反汇编（asm）文件。 
	   搜索PC指针（指向当前正在执行的指令）在asm中的位置，找到发生异常的函数。 
	   根据LR值查找异常函数的父函数。 
	   重复步骤3，得到函数间的调用关系，找到异常原因。 
   
   注意事项 
	   要查看调用栈信息，必须添加编译选项宏-fno-omit-frame-pointer支持stack frame，否则编译时FP寄存器是关闭的。 
   

	@endverbatim   

 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-11-20
 */

#include "los_exc.h"
#include "los_memory_pri.h"
#include "los_printf_pri.h"
#include "los_task_pri.h"
#include "los_percpu_pri.h"
#include "los_hw_pri.h"
#ifdef LOSCFG_SAVE_EXCINFO
#include "los_excinfo_pri.h"
#endif
#include "los_sys_stack_pri.h"
#ifdef LOSCFG_COREDUMP
#include "los_coredump.h"
#endif
#ifdef LOSCFG_GDB
#include "gdb_int.h"
#endif
#include "los_mp.h"
#include "los_vm_map.h"
#include "los_vm_dump.h"
#include "los_arch_mmu.h"
#include "los_vm_phys.h"
#include "los_vm_fault.h"
#include "los_vm_common.h"
#ifdef LOSCFG_KERNEL_DYNLOAD
#include "los_load_elf.h"
#endif
#include "arm.h"
#include "los_bitmap.h"
#include "los_process_pri.h"
#include "los_exc_pri.h"
#include "los_sched_pri.h"
#ifdef LOSCFG_FS_VFS
#include "console.h"
#endif
#ifdef LOSCFG_BLACKBOX
#include "los_blackbox.h"
#endif


#define INVALID_CPUID  0xFFFF
#define OS_EXC_VMM_NO_REGION  0x0U
#define OS_EXC_VMM_ALL_REGION 0x1U

STATIC UINTPTR g_minAddr;
STATIC UINTPTR g_maxAddr;
STATIC UINT32 g_currHandleExcCpuid = INVALID_CPUID;
VOID OsExcHook(UINT32 excType, ExcContext *excBufAddr, UINT32 far, UINT32 fsr);
UINT32 g_curNestCount[LOSCFG_KERNEL_CORE_NUM] = { 0 };
BOOL g_excFromUserMode[LOSCFG_KERNEL_CORE_NUM];
STATIC EXC_PROC_FUNC g_excHook = (EXC_PROC_FUNC)OsExcHook;
#ifdef LOSCFG_KERNEL_SMP
STATIC SPIN_LOCK_INIT(g_excSerializerSpin);
STATIC UINT32 g_currHandleExcPID = OS_INVALID_VALUE;
STATIC UINT32 g_nextExcWaitCpu = INVALID_CPUID;
#endif

#define OS_MAX_BACKTRACE    15U
#define DUMPSIZE            128U
#define DUMPREGS            12U
#define COM_REGS            4U
#define INSTR_SET_MASK      0x01000020U
#define THUMB_INSTR_LEN     2U
#define ARM_INSTR_LEN       4U
#define POINTER_SIZE        4U
#define WNR_BIT             11U
#define FSR_FLAG_OFFSET_BIT 10U
#define FSR_BITS_BEGIN_BIT  3U


#define GET_FS(fsr) (((fsr) & 0xFU) | (((fsr) & (1U << 10)) >> 6))
#define GET_WNR(dfsr) ((dfsr) & (1U << 11))

#define IS_VALID_ADDR(ptr) (((ptr) >= g_minAddr) &&       \
                            ((ptr) <= g_maxAddr) && \
                            (IS_ALIGNED((ptr), sizeof(CHAR *))))

/**
 * @brief 
 * @verbatim
    6种异常情况下对应的栈,每一种异常模式都有其独立的堆栈,用不同的堆栈指针来索引,
    这样当ARM进入异常模式的时候，程序就可以把一般通用寄存器压入堆栈，返回时再出栈，
    保证了各种模式下程序的状态的完整性

    用户模式，运行应用程序的普通模式。限制你的内存访问并且不能直接读取硬件设备。 
    超级用户模式(SVC 模式)，主要用于 SWI(软件中断)和 OS(操作系统)。这个模式有额外的特权，
        允许你进一步控制计算机。例如，你必须进入超级用户模式来读取一个插件(podule)。
        这不能在用户模式下完成。 
    中断模式(IRQ 模式)，用来处理发起中断的外设。这个模式也是有特权的。导致 IRQ 的设备有
        键盘、 VSync (在发生屏幕刷新的时候)、IOC 定时器、串行口、硬盘、软盘、等等... 
    快速中断模式(FIQ 模式)，用来处理发起快速中断的外设。这个模式是有特权的。导致 FIQ 的设备有
        处理数据的软盘，串行端口。  
        
    IRQ 和 FIQ 之间的区别是对于 FIQ 你必须尽快处理你事情并离开这个模式。
    IRQ 可以被 FIQ 所中断但 IRQ 不能中断 FIQ
 * @endverbatim
 */

/*
 * 异常处理栈信息数组
 * 定义不同模式下的异常栈基地址、大小和名称
 */
STATIC const StackInfo g_excStack[] = {
    { &__svc_stack,   OS_EXC_SVC_STACK_SIZE,   "svc_stack" },  /* SVC模式栈信息：基地址、大小(OS_EXC_SVC_STACK_SIZE)、名称 */
    { &__exc_stack,   OS_EXC_STACK_SIZE,       "exc_stack" }   /* 通用异常栈信息：基地址、大小(OS_EXC_STACK_SIZE)、名称 */
};

/**
 * @brief 获取系统当前状态
 * @return UINT32 系统状态标志：
 *         - OS_SYSTEM_NORMAL: 正常状态
 *         - OS_SYSTEM_EXC_CURR_CPU: 当前CPU处于异常状态
 *         - OS_SYSTEM_EXC_OTHER_CPU: 其他CPU处于异常状态
 */
UINT32 OsGetSystemStatus(VOID)
{
    UINT32 flag;
    UINT32 cpuid = g_currHandleExcCpuid;  /* 当前正在处理异常的CPU ID */

    if (cpuid == INVALID_CPUID) {         /* 无效CPU ID，表示无异常处理 */
        flag = OS_SYSTEM_NORMAL;
    } else if (cpuid == ArchCurrCpuid()) { /* 当前CPU正在处理异常 */
        flag = OS_SYSTEM_EXC_CURR_CPU;
    } else {                              /* 其他CPU正在处理异常 */
        flag = OS_SYSTEM_EXC_OTHER_CPU;
    }

    return flag;
}

/**
 * @brief 解码故障状态(FS)位
 * @param bitsFS 故障状态位(FSR寄存器中的相关位)
 * @return INT32 处理结果(LOS_OK)
 * @note 根据ARM架构参考手册，FS位用于标识不同类型的内存访问故障
 */
STATIC INT32 OsDecodeFS(UINT32 bitsFS)
{
    switch (bitsFS) {
        case 0x05:  /* 0b00101: 段转换故障 */
        case 0x07:  /* 0b00111: 页转换故障 */
            PrintExcInfo("Translation fault, %s\n", (bitsFS & 0x2) ? "page" : "section");
            break;
        case 0x09:  /* 0b01001: 段域故障 */
        case 0x0b:  /* 0b01011: 页面域故障 */
            PrintExcInfo("Domain fault, %s\n", (bitsFS & 0x2) ? "page" : "section");
            break;
        case 0x0d:  /* 0b01101: 段权限故障 */
        case 0x0f:  /* 0b01111: 页面权限故障 */
            PrintExcInfo("Permission fault, %s\n", (bitsFS & 0x2) ? "page" : "section");
            break;
        default:    /* 未知故障类型 */
            PrintExcInfo("Unknown fault! FS:0x%x. "
                         "Check IFSR and DFSR in ARM Architecture Reference Manual.\n",
                         bitsFS);
            break;
    }

    return LOS_OK;
}

/**
 * @brief 解码指令故障状态寄存器(IFSR)
 * @param regIFSR 指令故障状态寄存器值
 * @return INT32 处理结果(LOS_OK)
 */
STATIC INT32 OsDecodeInstructionFSR(UINT32 regIFSR)
{
    INT32 ret;
    UINT32 bitsFS = GET_FS(regIFSR); /* 提取FS位[4]+[3:0] */

    ret = OsDecodeFS(bitsFS);
    return ret;
}

/**
 * @brief 解码数据故障状态寄存器(DFSR)
 * @param regDFSR 数据故障状态寄存器值
 * @return INT32 处理结果(LOS_OK)
 */
STATIC INT32 OsDecodeDataFSR(UINT32 regDFSR)
{
    INT32 ret = 0;
    UINT32 bitWnR = GET_WNR(regDFSR); /* 提取写/读标志位[11] */
    UINT32 bitsFS = GET_FS(regDFSR);  /* 提取FS位[4]+[3:0] */

    if (bitWnR) {                     /* 写操作导致的异常 */
        PrintExcInfo("Abort caused by a write instruction. ");
    } else {                          /* 读操作导致的异常 */
        PrintExcInfo("Abort caused by a read instruction. ");
    }

    if (bitsFS == 0x01) { /* 0b00001: 对齐故障 */
        PrintExcInfo("Alignment fault.\n");
        return ret;
    }
    ret = OsDecodeFS(bitsFS);
    return ret;
}

#ifdef LOSCFG_KERNEL_VM  /* 启用虚拟内存时编译 */
/**
 * @brief ARM架构共享页面故障处理
 * @param excType 异常类型
 * @param frame 异常上下文结构体指针
 * @param far 故障地址寄存器值
 * @param fsr 故障状态寄存器值
 * @return UINT32 处理结果：
 *         - LOS_OK: 处理成功
 *         - LOS_ERRNO_VM_NOT_FOUND: 未找到对应的虚拟内存区域
 */
UINT32 OsArmSharedPageFault(UINT32 excType, ExcContext *frame, UINT32 far, UINT32 fsr)
{
    BOOL instructionFault = FALSE;    /* 指令故障标志 */
    UINT32 pfFlags = 0;               /* 页面故障标志组合 */
    UINT32 fsrFlag;                   /* 故障状态标志 */
    BOOL write = FALSE;               /* 写操作标志 */
    UINT32 ret;

    PRINT_INFO("page fault entry!!!\n");
    if (OsGetSystemStatus() == OS_SYSTEM_EXC_CURR_CPU) { /* 当前CPU已处于异常状态 */
        return LOS_ERRNO_VM_NOT_FOUND;
    }
#if defined(LOSCFG_KERNEL_SMP) && defined(LOSCFG_DEBUG_VERSION)
    /* SMP调试版本：检查调度器锁状态 */
    BOOL irqEnable = !(LOS_SpinHeld(&g_taskSpin) && OsSchedIsLock());
    if (irqEnable) {
        ArchIrqEnable();              /* 使能中断 */
    } else {
        PrintExcInfo("[ERR][%s] may be held scheduler lock when entering [%s] on cpu [%u]\n",
                     OsCurrTaskGet()->taskName, __FUNCTION__, ArchCurrCpuid());
    }
#else
    ArchIrqEnable();                  /* 非SMP版本直接使能中断 */
#endif
    if (excType == OS_EXCEPT_PREFETCH_ABORT) { /* 预取指令异常 */
        instructionFault = TRUE;
    } else {
        write = !!BIT_GET(fsr, WNR_BIT); /* 提取写/读标志 */
    }

    /* 组合FSR标志位：bit[10] + bits[3:0] */
    fsrFlag = ((BIT_GET(fsr, FSR_FLAG_OFFSET_BIT) ? 0b10000 : 0) | BITS_GET(fsr, FSR_BITS_BEGIN_BIT, 0));
    switch (fsrFlag) {
        case 0b00101:  /* 段转换故障 */
        case 0b00111:  /* 页转换故障 */
        case 0b01101:  /* 段权限故障 */
        case 0b01111: { /* 页权限故障 */
            BOOL user = (frame->regCPSR & CPSR_MODE_MASK) == CPSR_MODE_USR; /* 用户模式判断 */
            pfFlags |= write ? VM_MAP_PF_FLAG_WRITE : 0;       /* 写标志 */
            pfFlags |= user ? VM_MAP_PF_FLAG_USER : 0;         /* 用户模式标志 */
            pfFlags |= instructionFault ? VM_MAP_PF_FLAG_INSTRUCTION : 0; /* 指令故障标志 */
            pfFlags |= VM_MAP_PF_FLAG_NOT_PRESENT;             /* 页面不存在标志 */
            OsSigIntLock();
            ret = OsVmPageFaultHandler(far, pfFlags, frame);    /* 调用页面故障处理函数 */
            OsSigIntUnlock();
            break;
        }
        default: /* 其他故障类型：刷新TLB条目 */
            OsArmWriteTlbimvaais(ROUNDDOWN(far, PAGE_SIZE));
            ret = LOS_OK;
            break;
    }
#if defined(LOSCFG_KERNEL_SMP) && defined(LOSCFG_DEBUG_VERSION)
    if (irqEnable) {
        ArchIrqDisable();             /* 恢复中断禁用状态 */
    }
#else
    ArchIrqDisable();
#endif
    return ret;
}
#endif

/**
 * @brief 异常类型处理
 * @param excType 异常类型
 * @param excBufAddr 异常上下文缓冲区地址
 * @param far 故障地址寄存器值
 * @param fsr 故障状态寄存器值
 */
STATIC VOID OsExcType(UINT32 excType, ExcContext *excBufAddr, UINT32 far, UINT32 fsr)
{
    /* 未定义指令或软件中断异常处理：调整PC指针到异常指令位置 */
    if ((excType == OS_EXCEPT_UNDEF_INSTR) || (excType == OS_EXCEPT_SWI)) {
        if ((excBufAddr->regCPSR & INSTR_SET_MASK) == 0) { /* ARM指令集模式 */
            excBufAddr->PC = excBufAddr->PC - ARM_INSTR_LEN; /* ARM指令长度4字节 */
        } else if ((excBufAddr->regCPSR & INSTR_SET_MASK) == 0x20) { /* Thumb指令集模式 */
            excBufAddr->PC = excBufAddr->PC - THUMB_INSTR_LEN; /* Thumb指令长度2字节 */
        }
    }

    if (excType == OS_EXCEPT_PREFETCH_ABORT) { /* 预取指令异常 */
        PrintExcInfo("prefetch_abort fault fsr:0x%x, far:0x%0+8x\n", fsr, far);
        (VOID)OsDecodeInstructionFSR(fsr);
    } else if (excType == OS_EXCEPT_DATA_ABORT) { /* 数据访问异常 */
        PrintExcInfo("data_abort fsr:0x%x, far:0x%0+8x\n", fsr, far);
        (VOID)OsDecodeDataFSR(fsr);
    }
}

/* 异常类型字符串描述数组 */
STATIC const CHAR *g_excTypeString[] = {
    "reset",                     /* 0: 复位异常 */
    "undefined instruction",     /* 1: 未定义指令异常 */
    "software interrupt",        /* 2: 软件中断(SWI) */
    "prefetch abort",            /* 3: 预取指令终止 */
    "data abort",                /* 4: 数据访问终止 */
    "fiq",                       /* 5: 快速中断请求 */
    "address abort",             /* 6: 地址终止 */
    "irq"                        /* 7: 中断请求 */
};

#ifdef LOSCFG_KERNEL_VM  /* 启用虚拟内存时编译 */
/**
 * @brief 获取文本段区域基地址
 * @param region 内存映射区域结构体指针
 * @param runProcess 进程控制块指针
 * @return VADDR_T 文本段基地址
 */
STATIC VADDR_T OsGetTextRegionBase(LosVmMapRegion *region, LosProcessCB *runProcess)
{
    struct Vnode *curVnode = NULL;       /* 当前Vnode */
    struct Vnode *lastVnode = NULL;      /* 上一个Vnode */
    LosVmMapRegion *curRegion = NULL;    /* 当前内存区域 */
    LosVmMapRegion *lastRegion = NULL;   /* 上一个内存区域 */

    if ((region == NULL) || (runProcess == NULL)) {
        return 0;
    }

    if (!LOS_IsRegionFileValid(region)) { /* 非文件映射区域直接返回基地址 */
        return region->range.base;
    }

    lastRegion = region;
    do {
        curRegion = lastRegion;
        /* 查找前一个内存区域 */
        lastRegion = LOS_RegionFind(runProcess->vmSpace, curRegion->range.base - 1);
        /* 上一区域不存在或非文件映射区域，退出循环 */
        if ((lastRegion == NULL) || !LOS_IsRegionFileValid(lastRegion)) {
            goto DONE;
        }
        curVnode = curRegion->unTypeData.rf.vnode;
        lastVnode = lastRegion->unTypeData.rf.vnode;
    } while (curVnode == lastVnode); /* 属于同一文件(Vnode相同)则继续查找 */

DONE:
#ifdef LOSCFG_KERNEL_DYNLOAD  /* 动态加载支持 */
    if (curRegion->range.base == EXEC_MMAP_BASE) { /* 可执行映射区域 */
        return 0;
    }
#endif
    return curRegion->range.base;
}
#endif

/**
 * @brief 打印异常时的系统信息
 * @param excType 异常类型
 * @param excBufAddr 异常上下文缓冲区地址
 */
STATIC VOID OsExcSysInfo(UINT32 excType, const ExcContext *excBufAddr)
{
    LosTaskCB *runTask = OsCurrTaskGet();      /* 当前任务控制块 */
    LosProcessCB *runProcess = OsCurrProcessGet(); /* 当前进程控制块 */

    PrintExcInfo("excType: %s\n"
                 "processName       = %s\n"
                 "processID         = %u\n"
#ifdef LOSCFG_KERNEL_VM
                 "process aspace    = 0x%08x -> 0x%08x\n"
#endif
                 "taskName          = %s\n"
                 "taskID            = %u\n",
                 g_excTypeString[excType],  /* 异常类型字符串 */
                 runProcess->processName,   /* 进程名称 */
                 runProcess->processID,     /* 进程ID */
#ifdef LOSCFG_KERNEL_VM
                 runProcess->vmSpace->base, /* 进程虚拟地址空间基地址 */
                 runProcess->vmSpace->base + runProcess->vmSpace->size, /* 进程虚拟地址空间结束地址 */
#endif
                 runTask->taskName,         /* 任务名称 */
                 runTask->taskID);          /* 任务ID */

#ifdef LOSCFG_KERNEL_VM
    if (OsProcessIsUserMode(runProcess)) { /* 用户模式任务 */
        PrintExcInfo("task user stack   = 0x%08x -> 0x%08x\n",
                     runTask->userMapBase, runTask->userMapBase + runTask->userMapSize);
    } else
#endif
    { /* 内核模式任务 */
        PrintExcInfo("task kernel stack = 0x%08x -> 0x%08x\n",
                     runTask->topOfStack, runTask->topOfStack + runTask->stackSize);
    }

    PrintExcInfo("pc    = 0x%x ", excBufAddr->PC); /* 程序计数器 */
#ifdef LOSCFG_KERNEL_VM
    LosVmMapRegion *region = NULL;
    if (g_excFromUserMode[ArchCurrCpuid()] == TRUE) { /* 用户模式异常 */
        if (LOS_IsUserAddress((vaddr_t)excBufAddr->PC)) { /* PC在用户地址空间 */
            region = LOS_RegionFind(runProcess->vmSpace, (VADDR_T)excBufAddr->PC);
            if (region != NULL) {
                PrintExcInfo("in %s ---> 0x%x", OsGetRegionNameOrFilePath(region),
                             (VADDR_T)excBufAddr->PC - OsGetTextRegionBase(region, runProcess));
            }
        }

        PrintExcInfo("\nulr   = 0x%x ", excBufAddr->ULR); /* 用户链接寄存器 */
        region = LOS_RegionFind(runProcess->vmSpace, (VADDR_T)excBufAddr->ULR);
        if (region != NULL) {
            PrintExcInfo("in %s ---> 0x%x", OsGetRegionNameOrFilePath(region),
                         (VADDR_T)excBufAddr->ULR - OsGetTextRegionBase(region, runProcess));
        }
        PrintExcInfo("\nusp   = 0x%x", excBufAddr->USP); /* 用户栈指针 */
    } else
#endif
    { /* 内核模式异常 */
        PrintExcInfo("\nklr   = 0x%x\n"
                     "ksp   = 0x%x\n",
                     excBufAddr->LR,  /* 内核链接寄存器 */
                     excBufAddr->SP); /* 内核栈指针 */
    }

    PrintExcInfo("\nfp    = 0x%x\n", excBufAddr->R11); /* 帧指针(R11) */
}

/**
 * @brief 打印异常时的寄存器信息
 * @param excBufAddr 异常上下文缓冲区地址
 * @note 将寄存器信息分两部分打印，避免依赖内存模块
 */
STATIC VOID OsExcRegsInfo(const ExcContext *excBufAddr)
{
    PrintExcInfo("R0    = 0x%x\n"
                 "R1    = 0x%x\n"
                 "R2    = 0x%x\n"
                 "R3    = 0x%x\n"
                 "R4    = 0x%x\n"
                 "R5    = 0x%x\n"
                 "R6    = 0x%x\n",
                 excBufAddr->R0, excBufAddr->R1, excBufAddr->R2, excBufAddr->R3,
                 excBufAddr->R4, excBufAddr->R5, excBufAddr->R6);
    PrintExcInfo("R7    = 0x%x\n"
                 "R8    = 0x%x\n"
                 "R9    = 0x%x\n"
                 "R10   = 0x%x\n"
                 "R11   = 0x%x\n"
                 "R12   = 0x%x\n"
                 "CPSR  = 0x%x\n",
                 excBufAddr->R7, excBufAddr->R8, excBufAddr->R9, excBufAddr->R10,
                 excBufAddr->R11, excBufAddr->R12, excBufAddr->regCPSR);
}

/**
 * @brief 注册异常处理钩子函数
 * @param excHook 异常处理钩子函数指针
 * @return UINT32 操作结果(LOS_OK)
 * @note 钩子函数在异常发生时被调用，用于扩展异常处理功能
 */
LITE_OS_SEC_TEXT_INIT UINT32 LOS_ExcRegHook(EXC_PROC_FUNC excHook)
{
    UINT32 intSave;

    intSave = LOS_IntLock();          /* 关中断保护 */
    g_excHook = excHook;              /* 设置全局异常钩子 */
    LOS_IntRestore(intSave);          /* 恢复中断 */

    return LOS_OK;
}

/**
 * @brief 获取已注册的异常处理钩子函数
 * @return EXC_PROC_FUNC 异常处理钩子函数指针
 */
EXC_PROC_FUNC OsExcRegHookGet(VOID)
{
    return g_excHook;
}

#ifdef LOSCFG_KERNEL_VM  // 仅当启用内核虚拟内存管理功能时编译此代码块
/**
 * @brief 转储异常场景下的虚拟地址区域映射信息
 * @param[in] space 虚拟内存空间指针，包含MMU页表信息
 * @param[in] region 虚拟内存映射区域结构体，描述一段连续的虚拟地址范围
 * @details 遍历区域内所有页面，查询其物理地址映射关系，打印连续映射的虚拟地址、内核地址和大小
 */
STATIC VOID OsDumpExcVaddrRegion(LosVmSpace *space, LosVmMapRegion *region)
{
    INT32 i, numPages, pageCount;                     // 循环变量i，总页数numPages，连续页计数pageCount
    paddr_t addr, oldAddr, startVaddr, startPaddr;   // 当前物理地址addr，上一物理地址oldAddr，起始虚拟地址startVaddr，起始物理地址startPaddr
    vaddr_t pageBase;                                // 当前页面的虚拟地址基址
    BOOL mmuFlag = FALSE;                            // 是否需要打印MMU表头的标志位，初始为FALSE

    numPages = region->range.size >> PAGE_SHIFT;     // 计算总页数 = 区域大小 / 页面大小（右移12位即除以4KB）
    mmuFlag = TRUE;                                  // 设置需要打印表头
    // 遍历区域内所有页面，收集连续物理地址映射信息
    for (pageCount = 0, startPaddr = 0, startVaddr = 0, i = 0; i < numPages; i++) {
        pageBase = region->range.base + i * PAGE_SIZE;  // 计算当前页面的虚拟地址（基地址 + 偏移量）
        addr = 0;                                       // 物理地址初始化为0
        // 查询MMU页表获取虚拟地址对应的物理地址
        if (LOS_ArchMmuQuery(&space->archMmu, pageBase, &addr, NULL) != LOS_OK) {
            if (startPaddr == 0) {                      // 如果尚未记录起始物理地址，则跳过当前未映射页面
                continue;
            }
        } else if (startPaddr == 0) {                   // 首次找到有效映射，记录起始地址
            startVaddr = pageBase;                      // 记录起始虚拟地址
            startPaddr = addr;                          // 记录起始物理地址
            oldAddr = addr;                             // 记录上一物理地址
            pageCount++;                                // 连续页计数加1
            if (numPages > 1) {                         // 如果总页数大于1，继续下一页查询
                continue;
            }
        } else if (addr == (oldAddr + PAGE_SIZE)) {     // 当前物理地址是上一地址的连续页（物理地址+4KB）
            pageCount++;                                // 连续页计数加1
            oldAddr = addr;                             // 更新上一物理地址
            if (i < (numPages - 1)) {                   // 如果不是最后一页，继续下一页查询
                continue;
            }
        }
        // 打印表头（仅首次打印）
        if (mmuFlag == TRUE) {
            PrintExcInfo("       uvaddr       kvaddr       mapped size\n");  // 虚拟地址 | 内核地址 | 映射大小
            mmuFlag = FALSE;                            // 重置表头标志位，避免重复打印
        }
        // 打印连续映射区域信息：起始虚拟地址，内核虚拟地址（物理地址转换），总大小（连续页数*4KB）
        PrintExcInfo("       0x%08x   0x%08x   0x%08x\n",
                     startVaddr, LOS_PaddrToKVaddr(startPaddr), (UINT32)pageCount << PAGE_SHIFT);
        pageCount = 0;                                  // 重置连续页计数
        startPaddr = 0;                                 // 重置起始物理地址
    }
}

/**
 * @brief 转储进程使用的内存区域信息
 * @param[in] runProcess 当前运行进程控制块指针
 * @param[in] runspace 当前进程的虚拟内存空间指针
 * @param[in] vmmFlags 内存转储标志位：OS_EXC_VMM_ALL_REGION表示转储所有区域详细信息
 * @details 遍历进程虚拟内存空间中的所有区域，打印区域基地址和大小，可选打印区域内页面映射详情
 */
STATIC VOID OsDumpProcessUsedMemRegion(LosProcessCB *runProcess, LosVmSpace *runspace, UINT16 vmmFlags)
{
    LosVmMapRegion *region = NULL;                     // 虚拟内存映射区域结构体指针
    LosRbNode *pstRbNodeTemp = NULL;                   // 红黑树遍历临时节点
    LosRbNode *pstRbNodeNext = NULL;                   // 红黑树遍历下一个节点
    UINT32 count = 0;                                  // 区域计数器

    /* 遍历红黑树形式的区域列表 */
    RB_SCAN_SAFE(&runspace->regionRbTree, pstRbNodeTemp, pstRbNodeNext)  // 安全遍历红黑树宏，避免遍历中修改树结构导致异常
        region = (LosVmMapRegion *)pstRbNodeTemp;                        // 将红黑树节点转换为区域结构体
        // 打印区域序号、基地址（虚拟地址）和大小（字节）
        PrintExcInfo("%3u -> regionBase: 0x%08x regionSize: 0x%08x\n", count, region->range.base, region->range.size);
        if (vmmFlags == OS_EXC_VMM_ALL_REGION) {                         // 如果需要转储所有区域详情
            OsDumpExcVaddrRegion(runspace, region);                      // 调用函数打印区域内页面映射信息
        }
        count++;                                                          // 区域计数器加1
        (VOID)OsRegionOverlapCheckUnlock(runspace, region);              // 解锁区域重叠检查（避免死锁）
    RB_SCAN_SAFE_END(&space->regionRbTree, pstRbNodeTemp, pstRbNodeNext)  // 红黑树遍历结束宏
}

/**
 * @brief 转储当前进程使用的内存节点信息
 * @param[in] vmmFlags 内存转储标志位，传递给OsDumpProcessUsedMemRegion函数
 * @details 获取当前运行进程，检查是否为用户态进程，然后转储其虚拟内存区域信息
 */
STATIC VOID OsDumpProcessUsedMemNode(UINT16 vmmFlags)
{
    LosProcessCB *runProcess = NULL;                   // 当前运行进程控制块指针
    LosVmSpace *runspace = NULL;                       // 当前进程的虚拟内存空间指针

    runProcess = OsCurrProcessGet();                   // 获取当前运行的进程
    if (runProcess == NULL) {                          // 无效进程检查
        return;
    }

    if (!OsProcessIsUserMode(runProcess)) {            // 仅处理用户态进程，内核进程不打印内存信息
        return;
    }

    PrintExcInfo("\n   ******Current process %u vmm regions: ******\n", runProcess->processID);  // 打印进程ID和区域标题

    runspace = runProcess->vmSpace;                    // 获取进程的虚拟内存空间
    if (!runspace) {                                   // 虚拟内存空间无效检查
        return;
    }

    OsDumpProcessUsedMemRegion(runProcess, runspace, vmmFlags);  // 调用函数转储进程内存区域
    return;
}
#endif  // LOSCFG_KERNEL_VM

/**
 * @brief 转储异常上下文寄存器指向的内存内容
 * @param[in] excBufAddr 异常上下文缓冲区地址，包含寄存器值
 * @details 仅在内核模式异常时执行，遍历通用寄存器，对有效地址的内存区域进行十六进制转储
 */
VOID OsDumpContextMem(const ExcContext *excBufAddr)
{
    UINT32 count = 0;                                 // 寄存器计数器
    const UINT32 *excReg = NULL;                      // 寄存器指针
    if (g_excFromUserMode[ArchCurrCpuid()] == TRUE) { // 如果是用户模式异常则不转储内存
        return;
    }

    // 转储R0-R3寄存器指向的内存（COM_REGS=4个寄存器）
    for (excReg = &(excBufAddr->R0); count < COM_REGS; excReg++, count++) {
        if (IS_VALID_ADDR(*excReg)) {                 // 检查地址有效性（非空且对齐）
            PrintExcInfo("\ndump mem around R%u:%p", count, (*excReg));
            // 转储DUMPSIZE字节内存，从目标地址-DUMPSIZE/2开始（DUMPSIZE=256字节）
            OsDumpMemByte(DUMPSIZE, ((*excReg) - (DUMPSIZE >> 1)));
        }
    }
    // 转储R4-R11寄存器指向的内存（DUMPREGS=12个寄存器）
    for (excReg = &(excBufAddr->R4); count < DUMPREGS; excReg++, count++) {
        if (IS_VALID_ADDR(*excReg)) {
            PrintExcInfo("\ndump mem around R%u:%p", count, (*excReg));
            OsDumpMemByte(DUMPSIZE, ((*excReg) - (DUMPSIZE >> 1)));
        }
    }
    // 转储R12寄存器指向的内存
    if (IS_VALID_ADDR(excBufAddr->R12)) {
        PrintExcInfo("\ndump mem around R12:%p", excBufAddr->R12);
        OsDumpMemByte(DUMPSIZE, (excBufAddr->R12 - (DUMPSIZE >> 1)));
    }
    // 转储SP栈指针指向的内存
    if (IS_VALID_ADDR(excBufAddr->SP)) {
        PrintExcInfo("\ndump mem around SP:%p", excBufAddr->SP);
        OsDumpMemByte(DUMPSIZE, (excBufAddr->SP - (DUMPSIZE >> 1)));
    }
}

/**
 * @brief 恢复异常处理前的系统状态
 * @details 重置异常标志位、中断计数和调度锁，SMP模式下恢复CPU运行状态
 */
STATIC VOID OsExcRestore(VOID)
{
    UINT32 currCpuid = ArchCurrCpuid();               // 当前CPU核心ID

    g_excFromUserMode[currCpuid] = FALSE;             // 清除用户模式异常标志
    g_intCount[currCpuid] = 0;                        // 重置中断嵌套计数
    g_curNestCount[currCpuid] = 0;                    // 重置异常嵌套计数
#ifdef LOSCFG_KERNEL_SMP                               // 对称多处理配置
    OsCpuStatusSet(CPU_RUNNING);                      // 设置CPU状态为运行中
#endif
    OsSchedLockSet(0);                                // 解除调度锁（参数0表示无锁）
}

/**
 * @brief 用户模式异常处理入口
 * @param[in] excBufAddr 异常上下文缓冲区地址
 * @details 处理用户进程异常，包括SMP同步、核心转储、进程退出等流程
 */
STATIC VOID OsUserExcHandle(ExcContext *excBufAddr)
{
    UINT32 intSave;                                   // 中断状态保存变量
    UINT32 currCpu = ArchCurrCpuid();                 // 当前CPU核心ID
    LosTaskCB *runTask = OsCurrTaskGet();             // 当前运行任务控制块
    LosProcessCB *runProcess = OsCurrProcessGet();    // 当前运行进程控制块

    if (g_excFromUserMode[ArchCurrCpuid()] == FALSE) { // 非用户模式异常直接返回
        return;
    }

#ifdef LOSCFG_KERNEL_SMP                               // SMP模式下的异常CPU同步
    LOS_SpinLock(&g_excSerializerSpin);               // 获取异常序列化自旋锁
    if (g_nextExcWaitCpu != INVALID_CPUID) {          // 如果有等待处理的异常CPU
        g_currHandleExcCpuid = g_nextExcWaitCpu;      // 更新当前处理异常的CPU
        g_nextExcWaitCpu = INVALID_CPUID;             // 清除等待CPU
    } else {
        g_currHandleExcCpuid = INVALID_CPUID;         // 无异常CPU处理
    }
    g_currHandleExcPID = OS_INVALID_VALUE;            // 重置异常进程ID
    LOS_SpinUnlock(&g_excSerializerSpin);             // 释放自旋锁
#else
    g_currHandleExcCpuid = INVALID_CPUID;             // 非SMP模式下无效CPUID
#endif

#ifdef LOSCFG_KERNEL_SMP                               // SMP模式下唤醒控制台发送任务
#ifdef LOSCFG_FS_VFS
    OsWakeConsoleSendTask();
#endif
#endif

#ifdef LOSCFG_BLACKBOX                                // 黑匣子调试功能
    BBoxNotifyError("USER_CRASH", MODULE_SYSTEM, "Crash in user", 0); // 记录用户崩溃事件
#endif
    SCHEDULER_LOCK(intSave);                          // 关闭调度器（保存中断状态）
#ifdef LOSCFG_SAVE_EXCINFO                           // 保存异常信息配置
    OsProcessExitCodeCoreDumpSet(runProcess);         // 设置进程核心转储退出码
#endif
    OsProcessExitCodeSignalSet(runProcess, SIGUSR2);  // 设置进程信号退出码（用户自定义信号2）

    /* 当前进程退出过程中发生异常 */
    if (runProcess->processStatus & OS_PROCESS_FLAG_EXIT) {
        SCHEDULER_UNLOCK(intSave);                    // 恢复调度器
        /* 异常处理所有操作应保持在此操作之前 */
        OsExcRestore();                               // 恢复系统状态
        OsRunningTaskToExit(runTask, OS_PRO_EXIT_OK); // 将运行中任务标记为退出
    } else {
        SCHEDULER_UNLOCK(intSave);                    // 恢复调度器

        /* 异常处理所有操作应保持在此操作之前 */
        OsExcRestore();                               // 恢复系统状态
        /* 终止用户异常进程 */
        LOS_Exit(OS_PRO_EXIT_OK);                     // 进程正常退出
    }

    /* 用户模式异常处理失败，正常情况下不应到达此处 */
    g_curNestCount[currCpu]++;                        // 异常嵌套计数加1
    g_intCount[currCpu]++;                            // 中断计数加1
    PrintExcInfo("User mode exception ends unscheduled!\n");
}

/* 验证帧指针有效性或检查范围起始和结束地址 */
STATIC INLINE BOOL IsValidFP(UINTPTR regFP, UINTPTR start, UINTPTR end, vaddr_t *vaddr)
{
    VADDR_T kvaddr = regFP;                           // 内核虚拟地址（初始化为传入的帧指针）

    // 检查帧指针是否在有效范围内且按指针大小对齐（POINTER_SIZE=4字节）
    if (!((regFP > start) && (regFP < end) && IS_ALIGNED(regFP, sizeof(CHAR *)))) {
        return FALSE;
    }

#ifdef LOSCFG_KERNEL_VM                                // 内核虚拟内存配置
    PADDR_T paddr;                                    // 物理地址变量
    if (g_excFromUserMode[ArchCurrCpuid()] == TRUE) { // 用户模式下需要地址转换
        LosProcessCB *runProcess = OsCurrProcessGet();
        LosVmSpace *runspace = runProcess->vmSpace;   // 进程虚拟内存空间
        if (runspace == NULL) {
            return FALSE;
        }

        // 查询MMU页表获取物理地址
        if (LOS_ArchMmuQuery(&runspace->archMmu, regFP, &paddr, NULL) != LOS_OK) {
            return FALSE;
        }

        kvaddr = (PADDR_T)(UINTPTR)LOS_PaddrToKVaddr(paddr); // 物理地址转内核虚拟地址
    }
#endif
    if (vaddr != NULL) {
        *vaddr = kvaddr;                              // 输出内核虚拟地址
    }

    return TRUE;
}

/**
 * @brief 查找帧指针所属的有效栈空间
 * @param[in] regFP 待验证的帧指针（R11）
 * @param[out] start 栈空间起始地址
 * @param[out] end 栈空间结束地址
 * @param[out] vaddr 帧指针对应的内核虚拟地址
 * @return BOOL 查找结果：TRUE表示找到有效栈，FALSE表示失败
 * @details 按优先级查找用户栈、任务栈和异常栈，验证帧指针是否在栈范围内
 */
STATIC INLINE BOOL FindSuitableStack(UINTPTR regFP, UINTPTR *start, UINTPTR *end, vaddr_t *vaddr)
{
    UINT32 index, stackStart, stackEnd;               // 循环索引和栈地址变量
    BOOL found = FALSE;                               // 查找结果标志
    LosTaskCB *taskCB = NULL;                         // 任务控制块指针
    const StackInfo *stack = NULL;                    // 栈信息结构体指针
    vaddr_t kvaddr;                                   // 内核虚拟地址

#ifdef LOSCFG_KERNEL_VM                                // 用户模式下优先查找用户栈
    if (g_excFromUserMode[ArchCurrCpuid()] == TRUE) {
        taskCB = OsCurrTaskGet();
        stackStart = taskCB->userMapBase;             // 用户栈基地址
        stackEnd = taskCB->userMapBase + taskCB->userMapSize; // 用户栈结束地址
        if (IsValidFP(regFP, stackStart, stackEnd, &kvaddr) == TRUE) {
            found = TRUE;
            goto FOUND;                               // 找到后跳转到结果处理
        }
        return found;
    }
#endif

    /* 在任务栈中查找 */
    for (index = 0; index < g_taskMaxNum; index++) {   // 遍历所有任务（g_taskMaxNum为最大任务数）
        taskCB = &g_taskCBArray[index];
        if (OsTaskIsUnused(taskCB)) {                 // 跳过未使用的任务
            continue;
        }

        stackStart = taskCB->topOfStack;              // 任务栈顶地址
        stackEnd = taskCB->topOfStack + taskCB->stackSize; // 任务栈结束地址
        if (IsValidFP(regFP, stackStart, stackEnd, &kvaddr) == TRUE) {
            found = TRUE;
            goto FOUND;
        }
    }

    /* 在异常栈中查找 */
    for (index = 0; index < sizeof(g_excStack) / sizeof(StackInfo); index++) {
        stack = &g_excStack[index];
        stackStart = (UINTPTR)stack->stackTop;        // 异常栈顶地址
        stackEnd = stackStart + LOSCFG_KERNEL_CORE_NUM * stack->stackSize; // 异常栈结束地址（多核）
        if (IsValidFP(regFP, stackStart, stackEnd, &kvaddr) == TRUE) {
            found = TRUE;
            goto FOUND;
        }
    }

FOUND:                                                // 结果处理标签
    if (found == TRUE) {
        *start = stackStart;                          // 输出栈起始地址
        *end = stackEnd;                              // 输出栈结束地址
        *vaddr = kvaddr;                              // 输出内核虚拟地址
    }

    return found;
}

/**
 * @brief 获取用户模式指令指针（IP）对应的模块信息
 * @param[in] ip 指令指针地址
 * @param[out] info IP信息结构体，包含文件路径和偏移量
 * @return BOOL 获取结果：TRUE表示成功，FALSE表示失败
 * @details 解析用户空间IP地址对应的可执行模块路径和内部偏移量
 */
BOOL OsGetUsrIpInfo(UINTPTR ip, IpInfo *info)
{
    if (info == NULL) {                               // 参数校验
        return FALSE;
    }
#ifdef LOSCFG_KERNEL_VM                                // 仅在虚拟内存模式下有效
    BOOL ret = FALSE;                                 // 返回值
    const CHAR *name = NULL;                          // 模块名称/路径
    LosVmMapRegion *region = NULL;                    // 虚拟内存区域
    LosProcessCB *runProcess = OsCurrProcessGet();    // 当前进程

    if (LOS_IsUserAddress((VADDR_T)ip) == FALSE) {     // 检查是否为用户地址
        info->ip = ip;                                // 内核地址直接使用原始值
        name = "kernel";                             // 模块名称为内核
        ret = FALSE;
        goto END;                                     // 跳转到结果处理
    }

    region = LOS_RegionFind(runProcess->vmSpace, (VADDR_T)ip); // 查找IP所在的内存区域
    if (region == NULL) {
        info->ip = ip;                                // 无效区域使用原始IP
        name = "invalid";                            // 模块名称为无效
        ret = FALSE;
        goto END;
    }

    // 计算IP在模块内的偏移量（减去代码段基地址）
    info->ip = ip - OsGetTextRegionBase(region, runProcess);
    name = OsGetRegionNameOrFilePath(region);         // 获取区域对应的模块路径
    ret = TRUE;
    if (strcmp(name, "/lib/libc.so") != 0) {         // 过滤libc库（避免过多输出）
        PRINT_ERR("ip = 0x%x, %s\n", info->ip, name);
    }
END:                                                  // 结果处理标签
    info->len = strlen(name);                         // 模块名称长度
    // 复制模块路径到输出结构体（REGION_PATH_MAX=256字节）
    if (strncpy_s(info->f_path, REGION_PATH_MAX, name, REGION_PATH_MAX - 1) != EOK) {
        info->f_path[0] = '\0';                      // 复制失败时清空路径
        info->len = 0;
        PRINT_ERR("copy f_path failed, %s\n", name);
    }
    return ret;
#else
    info->ip = ip;                                    // 非虚拟内存模式直接返回IP
    return FALSE;
#endif
}

/**
 * @brief 获取函数调用栈跟踪信息
 * @param[in] regFP 帧指针（R11）
 * @param[out] callChain 调用链数组，存储IP信息
 * @param[in] maxDepth 最大跟踪深度
 * @return UINT32 实际跟踪到的调用层数
 * @details 通过解析栈帧链（FP->LR->FP）递归获取调用栈，支持用户/内核模式和不同编译器
 */
UINT32 BackTraceGet(UINTPTR regFP, IpInfo *callChain, UINT32 maxDepth)
{
    UINTPTR tmpFP, backLR;                            // 临时帧指针和返回地址
    UINTPTR stackStart, stackEnd;                     // 栈范围
    UINTPTR backFP = regFP;                           // 当前回溯的帧指针
    UINT32 count = 0;                                 // 调用层数计数器
    BOOL ret;                                         // OsGetUsrIpInfo返回值
    VADDR_T kvaddr;                                   // 内核虚拟地址

    // 查找帧指针所属的栈空间
    if (FindSuitableStack(regFP, &stackStart, &stackEnd, &kvaddr) == FALSE) {
        if (callChain == NULL) {
            PrintExcInfo("traceback error fp = 0x%x\n", regFP); // 打印跟踪错误
        }
        return 0;
    }

    /*
     * 检查是否为叶子函数
     * 通常帧指针指向链接寄存器的地址，但叶子函数中没有函数调用，编译器不会存储链接寄存器，
     * 但仍会存储和更新帧指针。这种情况下需要找到正确的帧指针位置。
     */
    tmpFP = *(UINTPTR *)(UINTPTR)kvaddr;              // 解引用获取下一个FP
    if (IsValidFP(tmpFP, stackStart, stackEnd, NULL) == TRUE) {
        backFP = tmpFP;                               // 修正为有效的下一个FP
        if (callChain == NULL) {
            PrintExcInfo("traceback fp fixed, trace using   fp = 0x%x\n", backFP);
        }
    }

    // 循环解析栈帧链，直到FP无效或达到最大深度
    while (IsValidFP(backFP, stackStart, stackEnd, &kvaddr) == TRUE) {
        tmpFP = backFP;                               // 保存当前FP
#ifdef LOSCFG_COMPILER_CLANG_LLVM                      // Clang/LLVM编译器栈布局
        backFP = *(UINTPTR *)(UINTPTR)kvaddr;         // FP指向的是上一个FP
        // 检查LR地址有效性（FP + 4字节）
        if (IsValidFP(tmpFP + POINTER_SIZE, stackStart, stackEnd, &kvaddr) == FALSE) {
            if (callChain == NULL) {
                PrintExcInfo("traceback backLR check failed, backLP: 0x%x\n", tmpFP + POINTER_SIZE);
            }
            return 0;
        }
        backLR = *(UINTPTR *)(UINTPTR)kvaddr;         // 获取LR（返回地址）
#else                                                  // GCC编译器栈布局
        backLR = *(UINTPTR *)(UINTPTR)kvaddr;         // FP指向的是LR
        // 检查上一个FP地址有效性（FP - 4字节）
        if (IsValidFP(tmpFP - POINTER_SIZE, stackStart, stackEnd, &kvaddr) == FALSE) {
            if (callChain == NULL) {
                PrintExcInfo("traceback backFP check failed, backFP: 0x%x\n", tmpFP - POINTER_SIZE);
            }
            return 0;
        }
        backFP = *(UINTPTR *)(UINTPTR)kvaddr;         // 获取上一个FP
#endif
        IpInfo info = {0};                            // IP信息结构体
        ret = OsGetUsrIpInfo((VADDR_T)backLR, &info); // 获取LR对应的模块信息
        if (callChain == NULL) {                      // 直接打印模式
            PrintExcInfo("traceback %u -- lr = 0x%x    fp = 0x%x ", count, backLR, backFP);
            if (ret) {
#ifdef LOSCFG_KERNEL_VM
                PrintExcInfo("lr in %s --> 0x%x\n", info.f_path, info.ip); // 打印模块路径和偏移
#else
                PrintExcInfo("\n");
#endif
            } else {
                PrintExcInfo("\n");
            }
        } else {                                      // 调用链数组模式
            (VOID)memcpy_s(&callChain[count], sizeof(IpInfo), &info, sizeof(IpInfo));
        }
        count++;
        if ((count == maxDepth) || (backFP == tmpFP)) { // 达到最大深度或循环引用时退出
            break;
        }
    }
    return count;
}

/**
 * @brief 调用栈跟踪辅助函数（无输出数组版本）
 * @param[in] regFP 帧指针（R11）
 * @details 调用BackTraceGet打印调用栈信息，最大深度OS_MAX_BACKTRACE（32层）
 */
VOID BackTraceSub(UINTPTR regFP)
{
    (VOID)BackTraceGet(regFP, NULL, OS_MAX_BACKTRACE);
}

/**
 * @brief 调用栈跟踪入口函数
 * @param[in] regFP 帧指针（R11）
 * @details 打印调用栈头部信息并启动跟踪
 */
VOID BackTrace(UINT32 regFP)
{
    PrintExcInfo("*******backtrace begin*******\n");

    BackTraceSub(regFP);
}

/**
 * @brief 异常栈信息初始化
 * @details 注册异常栈信息到系统，用于栈验证和跟踪
 */
VOID OsExcInit(VOID)
{
    OsExcStackInfoReg(g_excStack, sizeof(g_excStack) / sizeof(g_excStack[0]));
}

/**
 * @brief 异常处理钩子函数
 * @param[in] excType 异常类型（如数据中止、未定义指令等）
 * @param[in] excBufAddr 异常上下文缓冲区地址
 * @param[in] far 故障地址寄存器（Fault Address Register）
 * @param[in] fsr 故障状态寄存器（Fault Status Register）
 * @details 异常处理主流程：打印异常信息、调用栈跟踪、内存检查、核心转储和进程退出
 */
VOID OsExcHook(UINT32 excType, ExcContext *excBufAddr, UINT32 far, UINT32 fsr)
{
    OsExcType(excType, excBufAddr, far, fsr);         // 打印异常类型信息
    OsExcSysInfo(excType, excBufAddr);                // 打印系统状态信息
    OsExcRegsInfo(excBufAddr);                        // 打印寄存器信息

    BackTrace(excBufAddr->R11);                       // 通过R11（FP）跟踪调用栈

    (VOID)OsShellCmdTskInfoGet(OS_ALL_TASK_MASK, NULL, OS_PROCESS_INFO_ALL); // 获取所有任务信息

#ifndef LOSCFG_DEBUG_VERSION                           // 非调试版本条件编译
    if (g_excFromUserMode[ArchCurrCpuid()] != TRUE) { // 仅内核模式异常打印
#endif
#ifdef LOSCFG_KERNEL_VM
        OsDumpProcessUsedMemNode(OS_EXC_VMM_NO_REGION); // 转储进程内存节点（不包含详细页表）
#endif
        OsExcStackInfo();                             // 打印异常栈信息
#ifndef LOSCFG_DEBUG_VERSION
    }
#endif

    OsDumpContextMem(excBufAddr);                     // 转储寄存器指向的内存

    (VOID)OsShellCmdMemCheck(0, NULL);                // 执行内存检查

#ifdef LOSCFG_COREDUMP                                // 核心转储功能
    LOS_CoreDumpV2(excType, excBufAddr);              // 生成核心转储文件
#endif

    OsUserExcHandle(excBufAddr);                      // 处理用户模式异常（进程退出）
}

/**
 * @brief 调用栈信息打印函数
 * @details 遍历当前任务栈，提取有效的返回地址（LR）并打印调用链
 */
VOID OsCallStackInfo(VOID)
{
    UINT32 count = 0;                                 // 调用层数计数器
    LosTaskCB *runTask = OsCurrTaskGet();             // 当前任务控制块
    UINTPTR stackBottom = runTask->topOfStack + runTask->stackSize; // 栈底地址
    UINT32 *stackPointer = (UINT32 *)stackBottom;     // 栈指针（从栈底向上遍历）

    PrintExcInfo("runTask->stackPointer = 0x%x\n"
                 "runTask->topOfStack = 0x%x\n"
                 "text_start:0x%x,text_end:0x%x\n",
                 stackPointer, runTask->topOfStack, &__text_start, &__text_end);

    // 遍历栈空间，查找代码段范围内的有效返回地址
    while ((stackPointer > (UINT32 *)runTask->topOfStack) && (count < OS_MAX_BACKTRACE)) {
        // 检查地址是否在代码段范围内且按指针大小对齐
        if ((*stackPointer > (UINTPTR)(&__text_start)) &&
            (*stackPointer < (UINTPTR)(&__text_end)) &&
            IS_ALIGNED((*stackPointer), POINTER_SIZE)) {
            // 检查前一个地址是否为有效的帧指针
            if ((*(stackPointer - 1) > (UINT32)runTask->topOfStack) &&
                (*(stackPointer - 1) < stackBottom) &&
                IS_ALIGNED((*(stackPointer - 1)), POINTER_SIZE)) {
                count++;
                PrintExcInfo("traceback %u -- lr = 0x%x\n", count, *stackPointer);
            }
        }
        stackPointer--;                               // 栈指针上移（向栈顶方向）
    }
    PRINTK("\n");
}

/**
 * @brief 指定任务的调用栈跟踪
 * @param[in] taskID 任务ID
 * @details 根据任务ID查找任务控制块，获取其栈指针并跟踪调用栈
 */
VOID OsTaskBackTrace(UINT32 taskID)
{
    LosTaskCB *taskCB = NULL;                         // 任务控制块指针

    if (OS_TID_CHECK_INVALID(taskID)) {               // 任务ID有效性检查
        PRINT_ERR("\r\nTask ID is invalid!\n");
        return;
    }
    taskCB = OS_TCB_FROM_TID(taskID);                 // 通过任务ID获取TCB
    if (OsTaskIsUnused(taskCB) || (taskCB->taskEntry == NULL)) { // 任务未创建或无效
        PRINT_ERR("\r\nThe task is not created!\n");
        return;
    }
    PRINTK("TaskName = %s\n", taskCB->taskName);     // 打印任务名称
    PRINTK("TaskID = 0x%x\n", taskCB->taskID);       // 打印任务ID
    // 从任务栈指针中提取R11（FP）并跟踪调用栈
    BackTrace(((TaskContext *)(taskCB->stackPointer))->R11); /* R11 : FP */
}

/**
 * @brief 系统调用栈跟踪函数
 * @details 获取当前任务的帧指针（R11）并启动调用栈跟踪
 */
VOID OsBackTrace(VOID)
{
    UINT32 regFP = Get_Fp();                          // 获取当前帧指针（R11）
    LosTaskCB *runTask = OsCurrTaskGet();             // 当前运行任务
    PrintExcInfo("OsBackTrace fp = 0x%x\n", regFP);   // 打印帧指针
    PrintExcInfo("runTask->taskName = %s\n", runTask->taskName); // 打印任务名称
    PrintExcInfo("runTask->taskID = %u\n", runTask->taskID);     // 打印任务ID
    BackTrace(regFP);                                 // 启动调用栈跟踪
}

#ifdef LOSCFG_GDB  // 若启用GDB调试功能
/**
 * @brief 未定义指令异常处理入口函数
 * @param excBufAddr 异常上下文缓冲区地址
 * @details 调整PC寄存器（LR在未定义模式下为PC+4），调用GDB钩子函数，若未处理则调用系统异常钩子
 */
VOID OsUndefIncExcHandleEntry(ExcContext *excBufAddr)
{
    excBufAddr->PC -= 4;  /* lr in undef is pc + 4 */  // 调整PC值，因为未定义模式下LR寄存器值为PC+4

    if (gdb_undef_hook(excBufAddr, OS_EXCEPT_UNDEF_INSTR)) {  // 调用GDB未定义指令异常钩子函数
        return;                                             // 若钩子处理则返回
    }

    if (g_excHook != NULL) {  // 若系统异常钩子存在
        /* far, fsr are unused in exc type of OS_EXCEPT_UNDEF_INSTR */
        g_excHook(OS_EXCEPT_UNDEF_INSTR, excBufAddr, 0, 0);  // 调用系统异常钩子，传递未定义指令异常类型
    }
    while (1) {}  // 异常处理后进入死循环
}

#if __LINUX_ARM_ARCH__ >= 7  // 若ARM架构版本 >= 7
/**
 * @brief 预取中止异常处理入口函数
 * @param excBufAddr 异常上下文缓冲区地址
 * @details 调整PC寄存器（LR在预取中止模式下为PC+4），读取IFAR/IFSR寄存器，调用GDB和系统异常钩子
 */
VOID OsPrefetchAbortExcHandleEntry(ExcContext *excBufAddr)
{
    UINT32 far;  // 故障地址寄存器值
    UINT32 fsr;  // 故障状态寄存器值

    excBufAddr->PC -= 4;  /* lr in prefetch abort is pc + 4 */  // 调整PC值，因为预取中止模式下LR寄存器值为PC+4

    if (gdbhw_hook(excBufAddr, OS_EXCEPT_PREFETCH_ABORT)) {  // 调用GDB硬件预取中止异常钩子函数
        return;                                             // 若钩子处理则返回
    }

    if (g_excHook != NULL) {  // 若系统异常钩子存在
        far = OsArmReadIfar();  // 读取指令故障地址寄存器(IFAR)
        fsr = OsArmReadIfsr();  // 读取指令故障状态寄存器(IFSR)
        g_excHook(OS_EXCEPT_PREFETCH_ABORT, excBufAddr, far, fsr);  // 调用系统异常钩子，传递预取中止异常信息
    }
    while (1) {}  // 异常处理后进入死循环
}

/**
 * @brief 数据中止异常处理入口函数
 * @param excBufAddr 异常上下文缓冲区地址
 * @details 调整PC寄存器（LR在数据中止模式下为PC+8），读取DFAR/DFSR寄存器，调用GDB和系统异常钩子
 */
VOID OsDataAbortExcHandleEntry(ExcContext *excBufAddr)
{
    UINT32 far;  // 故障地址寄存器值
    UINT32 fsr;  // 故障状态寄存器值

    excBufAddr->PC -= 8;  /* lr in data abort is pc + 8 */  // 调整PC值，因为数据中止模式下LR寄存器值为PC+8

    if (gdbhw_hook(excBufAddr, OS_EXCEPT_DATA_ABORT)) {  // 调用GDB硬件数据中止异常钩子函数
        return;                                          // 若钩子处理则返回
    }

    if (g_excHook != NULL) {  // 若系统异常钩子存在
        far = OsArmReadDfar();  // 读取数据故障地址寄存器(DFAR)
        fsr = OsArmReadDfsr();  // 读取数据故障状态寄存器(DFSR)
        g_excHook(OS_EXCEPT_DATA_ABORT, excBufAddr, far, fsr);  // 调用系统异常钩子，传递数据中止异常信息
    }
    while (1) {}  // 异常处理后进入死循环
}
#endif /* __LINUX_ARM_ARCH__ */
#endif /* LOSCFG_GDB */

#ifdef LOSCFG_KERNEL_SMP  // 若启用SMP（对称多处理）功能
#define EXC_WAIT_INTER 50U   /* 异常等待时间间隔，单位毫秒 */  // 50毫秒
#define EXC_WAIT_TIME  2000U /* 异常最大等待时间，单位毫秒 */  // 2000毫秒（2秒）

/**
 * @brief 等待所有其他CPU停止运行
 * @param cpuid 当前CPU ID
 * @details 循环检查所有CPU状态，等待其他CPU进入halt状态或超时
 */
STATIC VOID WaitAllCpuStop(UINT32 cpuid)
{
    UINT32 i;          // CPU迭代索引
    UINT32 time = 0;   // 已等待时间，单位毫秒

    while (time < EXC_WAIT_TIME) {  // 若未超过最大等待时间
        for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {  // 遍历所有CPU核心
            if ((i != cpuid) && !OsCpuStatusIsHalt(i)) {  // 若当前CPU不是本CPU且未halt
                LOS_Mdelay(EXC_WAIT_INTER);  // 等待指定间隔
                time += EXC_WAIT_INTER;      // 累加等待时间
                break;                       // 跳出循环重新检查
            }
        }
        /* Other CPUs are all haletd or in the exc. */
        if (i == LOSCFG_KERNEL_CORE_NUM) {  // 若所有其他CPU均已halt
            break;                          // 跳出等待循环
        }
    }
    return;
}

/**
 * @brief 等待其他核心完成异常处理
 * @param currCpuid 当前CPU ID
 * @details 通过自旋锁同步，确保当前CPU获得异常处理权限或进入等待状态
 */
STATIC VOID OsWaitOtherCoresHandleExcEnd(UINT32 currCpuid)
{
    while (1) {  // 无限循环等待
        LOS_SpinLock(&g_excSerializerSpin);  // 获取异常序列化自旋锁
        if ((g_currHandleExcCpuid == INVALID_CPUID) || (g_currHandleExcCpuid == currCpuid)) {
            g_currHandleExcCpuid = currCpuid;          // 设置当前异常处理CPU ID
            g_currHandleExcPID = OsCurrProcessGet()->processID;  // 记录当前进程ID
            LOS_SpinUnlock(&g_excSerializerSpin);      // 释放自旋锁
            break;                                     // 跳出等待循环
        }

        if (g_nextExcWaitCpu == INVALID_CPUID) {  // 若下一个等待CPU未设置
            g_nextExcWaitCpu = currCpuid;         // 设置当前CPU为下一个等待CPU
        }
        LOS_SpinUnlock(&g_excSerializerSpin);  // 释放自旋锁
        LOS_Mdelay(EXC_WAIT_INTER);            // 等待指定间隔后重试
    }
}

/**
 * @brief SMP模式下检查所有CPU异常状态
 * @details 协调多CPU异常处理，设置当前异常处理CPU，发送IPI中断停止其他核心，处理进程异常冲突
 */
STATIC VOID OsCheckAllCpuStatus(VOID)
{
    UINT32 currCpuid = ArchCurrCpuid();  // 获取当前CPU ID
    UINT32 ret, target;                  // ret:函数返回值, target:IPI目标CPU掩码

    OsCpuStatusSet(CPU_EXC);             // 设置当前CPU状态为异常处理中
    LOCKDEP_CLEAR_LOCKS();               // 清除锁依赖跟踪信息

    LOS_SpinLock(&g_excSerializerSpin);  // 获取异常序列化自旋锁
    /* Only the current CPU anomaly */
    if (g_currHandleExcCpuid == INVALID_CPUID) {  // 若当前无CPU处理异常
        g_currHandleExcCpuid = currCpuid;          // 设置当前CPU为异常处理CPU
        g_currHandleExcPID = OsCurrProcessGet()->processID;  // 记录当前进程ID
        LOS_SpinUnlock(&g_excSerializerSpin);      // 释放自旋锁
#ifndef LOSCFG_SAVE_EXCINFO  // 若未启用异常信息保存
        if (g_excFromUserMode[currCpuid] == FALSE) {  // 若异常来自内核模式
            target = (UINT32)(OS_MP_CPU_ALL & ~CPUID_TO_AFFI_MASK(currCpuid));  // 计算目标CPU掩码（排除当前CPU）
            HalIrqSendIpi(target, LOS_MP_IPI_HALT);  // 发送HALT类型IPI中断给其他CPU
        }
#endif
    } else if (g_excFromUserMode[currCpuid] == TRUE) {  // 若当前CPU异常来自用户模式
        /* Both cores raise exceptions, and the current core is a user-mode exception.
         * Both cores are abnormal and come from the same process
         */
        if (OsCurrProcessGet()->processID == g_currHandleExcPID) {  // 若与当前处理异常的进程ID相同
            LOS_SpinUnlock(&g_excSerializerSpin);  // 释放自旋锁
            OsExcRestore();                        // 恢复异常前状态
            ret = LOS_TaskDelete(OsCurrTaskGet()->taskID);  // 删除当前任务
            LOS_Panic("%s suspend task :%u failed: 0x%x\n", __FUNCTION__, OsCurrTaskGet()->taskID, ret);  // 任务删除失败则Panic
        }
        LOS_SpinUnlock(&g_excSerializerSpin);  // 释放自旋锁

        OsWaitOtherCoresHandleExcEnd(currCpuid);  // 等待其他核心完成异常处理
    } else {  // 当前CPU异常来自内核模式且已有其他CPU处理异常
        if ((g_currHandleExcCpuid < LOSCFG_KERNEL_CORE_NUM) && (g_excFromUserMode[g_currHandleExcCpuid] == TRUE)) {
            // 若当前处理异常的CPU是用户模式异常，则抢占异常处理权
            g_currHandleExcCpuid = currCpuid;          // 更新当前异常处理CPU为内核模式CPU
            LOS_SpinUnlock(&g_excSerializerSpin);      // 释放自旋锁
            target = (UINT32)(OS_MP_CPU_ALL & ~CPUID_TO_AFFI_MASK(currCpuid));  // 计算目标CPU掩码
            HalIrqSendIpi(target, LOS_MP_IPI_HALT);    // 发送HALT类型IPI中断
        } else {
            LOS_SpinUnlock(&g_excSerializerSpin);  // 释放自旋锁
            while (1) {}  // 进入死循环等待
        }
    }
#ifndef LOSCFG_SAVE_EXCINFO  // 若未启用异常信息保存
    /* use halt ipi to stop other active cores */
    if (g_excFromUserMode[ArchCurrCpuid()] == FALSE) {  // 若当前异常来自内核模式
        WaitAllCpuStop(currCpuid);  // 等待所有其他CPU停止
    }
#endif
}
#endif  /* LOSCFG_KERNEL_SMP */

/**
 * @brief 检查CPU异常状态入口函数
 * @details 根据是否启用SMP选择不同的CPU状态检查实现
 */
STATIC VOID OsCheckCpuStatus(VOID)
{
#ifdef LOSCFG_KERNEL_SMP
    OsCheckAllCpuStatus();  // SMP模式：检查所有CPU状态
#else
    g_currHandleExcCpuid = ArchCurrCpuid();  // 非SMP模式：直接设置当前CPU为异常处理CPU
#endif
}

/**
 * @brief 异常处理前的准备工作
 * @param excBufAddr 异常上下文缓冲区地址
 * @details 根据异常模式（用户/内核）设置地址空间范围，检查CPU状态，等待控制台任务完成
 */
LITE_OS_SEC_TEXT VOID STATIC OsExcPriorDisposal(ExcContext *excBufAddr)
{
    if ((excBufAddr->regCPSR & CPSR_MASK_MODE) == CPSR_USER_MODE) {  // 检查CPSR寄存器判断异常模式
        g_minAddr = USER_ASPACE_BASE;                               // 用户模式：设置用户地址空间基址
        g_maxAddr = USER_ASPACE_BASE + USER_ASPACE_SIZE;            // 用户模式：设置用户地址空间大小（十进制：USER_ASPACE_SIZE）
        g_excFromUserMode[ArchCurrCpuid()] = TRUE;                  // 标记异常来自用户模式
    } else {
        g_minAddr = KERNEL_ASPACE_BASE;                              // 内核模式：设置内核地址空间基址
        g_maxAddr = KERNEL_ASPACE_BASE + KERNEL_ASPACE_SIZE;         // 内核模式：设置内核地址空间大小（十进制：KERNEL_ASPACE_SIZE）
        g_excFromUserMode[ArchCurrCpuid()] = FALSE;                  // 标记异常来自内核模式
    }

    OsCheckCpuStatus();  // 检查CPU异常状态

#ifdef LOSCFG_KERNEL_SMP
#ifdef LOSCFG_FS_VFS
    /* Wait for the end of the Console task to avoid multicore printing code */
    OsWaitConsoleSendTaskPend(OsCurrTaskGet()->taskID);  // 等待控制台发送任务挂起，避免多核打印冲突
#endif
#endif
}

/**
 * @brief 打印异常头部信息
 * @param far 故障地址
 * @details 根据异常来源（用户/内核）打印不同头部信息，调试模式下转储内存区域
 */
LITE_OS_SEC_TEXT_INIT STATIC VOID OsPrintExcHead(UINT32 far)
{
#ifdef LOSCFG_BLACKBOX
#ifdef LOSCFG_SAVE_EXCINFO
    SetExcInfoIndex(0);  // 初始化异常信息索引
#endif
#endif
#ifdef LOSCFG_KERNEL_VM  // 若启用虚拟内存
    /* You are not allowed to add any other print information before this exception information */
    if (g_excFromUserMode[ArchCurrCpuid()] == TRUE) {  // 异常来自用户模式
#ifdef LOSCFG_DEBUG_VERSION  // 调试版本
        VADDR_T vaddr = ROUNDDOWN(far, PAGE_SIZE);  // 将故障地址向下对齐到页大小（十进制：PAGE_SIZE）
        LosVmSpace *space = LOS_SpaceGet(vaddr);    // 获取虚拟地址空间
        if (space != NULL) {
            LOS_DumpMemRegion(vaddr);  // 转储内存区域信息
        }
#endif
        PrintExcInfo("##################excFrom: User!####################\n");  // 打印用户模式异常标记
    } else
#endif  /* LOSCFG_KERNEL_VM */
    {
        PrintExcInfo("##################excFrom: kernel!###################\n");  // 打印内核模式异常标记
    }
}

#ifdef LOSCFG_SAVE_EXCINFO  // 若启用异常信息保存
/**
 * @brief 保存系统状态
 * @param intCount 中断计数指针
 * @param lockCount 锁计数指针
 * @details 保存当前中断计数和调度锁计数，并临时清零
 */
STATIC VOID OsSysStateSave(UINT32 *intCount, UINT32 *lockCount)
{
    *intCount = g_intCount[ArchCurrCpuid()];  // 保存当前CPU中断计数
    *lockCount = OsSchedLockCountGet();       // 保存调度锁计数
    g_intCount[ArchCurrCpuid()] = 0;          // 临时清零中断计数
    OsSchedLockSet(0);                        // 临时解锁调度
}

/**
 * @brief 恢复系统状态
 * @param intCount 中断计数
 * @param lockCount 锁计数
 * @details 恢复之前保存的中断计数和调度锁计数
 */
STATIC VOID OsSysStateRestore(UINT32 intCount, UINT32 lockCount)
{
    g_intCount[ArchCurrCpuid()] = intCount;  // 恢复中断计数
    OsSchedLockSet(lockCount);               // 恢复调度锁计数
}
#endif  /* LOSCFG_SAVE_EXCINFO */

/*
 * Description : EXC handler entry
 * Input       : excType    --- exc type
 *               excBufAddr --- address of EXC buf
 */
/**
 * @brief 异常处理入口函数
 * @param excType 异常类型
 * @param excBufAddr 异常上下文缓冲区地址
 * @param far 故障地址
 * @param fsr 故障状态寄存器值
 * @details 异常处理主流程：调度锁、异常预处理、信息打印、钩子函数调用、系统状态保存与恢复
 */
LITE_OS_SEC_TEXT_INIT VOID OsExcHandleEntry(UINT32 excType, ExcContext *excBufAddr, UINT32 far, UINT32 fsr)
{
#ifdef LOSCFG_SAVE_EXCINFO
    UINT32 intCount;   // 中断计数
    UINT32 lockCount;  // 锁计数
#endif

    /* Task scheduling is not allowed during exception handling */
    OsSchedLock();  // 禁用任务调度

    g_curNestCount[ArchCurrCpuid()]++;  // 增加异常嵌套计数

    OsExcPriorDisposal(excBufAddr);  // 异常处理前准备

    OsPrintExcHead(far);  // 打印异常头部信息

#ifdef LOSCFG_KERNEL_SMP
    OsAllCpuStatusOutput();  // SMP模式：输出所有CPU状态
#endif

#ifdef LOSCFG_SAVE_EXCINFO
    log_read_write_fn func = GetExcInfoRW();  // 获取异常信息读写函数
#endif

    if (g_excHook != NULL) {  // 若异常钩子函数存在
        if (g_curNestCount[ArchCurrCpuid()] == 1) {  // 若为第一层异常
#ifdef LOSCFG_SAVE_EXCINFO
            if (func != NULL) {  // 若异常信息读写函数存在
#ifndef LOSCFG_BLACKBOX
                SetExcInfoIndex(0);  // 初始化异常信息索引
#endif
                OsSysStateSave(&intCount, &lockCount);  // 保存系统状态
                OsRecordExcInfoTime();                  // 记录异常发生时间
                OsSysStateRestore(intCount, lockCount);  // 恢复系统状态
            }
#endif
            g_excHook(excType, excBufAddr, far, fsr);  // 调用异常钩子函数
        } else {
            OsCallStackInfo();  // 多层异常：仅打印调用栈信息
        }

#ifdef LOSCFG_SAVE_EXCINFO
        if (func != NULL) {  // 若异常信息读写函数存在
            PrintExcInfo("Be sure flash space bigger than GetExcInfoIndex():0x%x\n", GetExcInfoIndex());  // 打印异常信息大小提示
            OsSysStateSave(&intCount, &lockCount);  // 保存系统状态
            func(GetRecordAddr(), GetRecordSpace(), 0, GetExcInfoBuf());  // 写入异常信息到存储
            OsSysStateRestore(intCount, lockCount);  // 恢复系统状态
        }
#endif
    }

#ifdef LOSCFG_SHELL_CMD_DEBUG  // 若启用shell调试命令
    SystemRebootFunc rebootHook = OsGetRebootHook();  // 获取重启钩子函数
    if ((OsSystemExcIsReset() == TRUE) && (rebootHook != NULL)) {
        LOS_Mdelay(3000);  /* 3000: System dead, delay 3 seconds after system restart */  // 延迟3秒后重启
        rebootHook();      // 调用重启钩子
    }
#endif

#ifdef LOSCFG_BLACKBOX  // 若启用黑匣子功能
    BBoxNotifyError(EVENT_PANIC, MODULE_SYSTEM, "Crash in kernel", 1);  // 通知内核崩溃事件
#endif
    while (1) {}  // 异常处理后进入死循环
}

/**
 * @brief 系统Panic函数
 * @param fmt 格式化字符串
 * @param ... 可变参数列表
 * @details 打印Panic信息并触发软中断进入异常处理
 */
__attribute__((noinline)) VOID LOS_Panic(const CHAR *fmt, ...)
{
    va_list ap;  // 可变参数列表
    va_start(ap, fmt);  // 初始化可变参数
    OsVprintf(fmt, ap, EXC_OUTPUT);  // 打印Panic信息到异常输出
    va_end(ap);  // 结束可变参数处理
    __asm__ __volatile__("swi 0");  // 触发软中断，进入异常处理
    while (1) {}  // 进入死循环
}

/* stack protector */
USED UINT32 __stack_chk_guard = 0xd00a0dff;  // 栈保护随机值（十进制：3487265279）

/**
 * @brief 栈溢出检查失败处理函数
 * @details 检测到栈溢出时打印错误信息并触发Panic
 */
VOID __stack_chk_fail(VOID)
{
    /* __builtin_return_address is a builtin function, building in gcc */
    LOS_Panic("stack-protector: Kernel stack is corrupted in: %p\n",
              __builtin_return_address(0));  // 打印栈损坏地址并Panic
}

/**
 * @brief 记录链接寄存器(LR)值用于回溯
 * @param LR LR值存储数组
 * @param LRSize 数组大小
 * @param recordCount 需记录的LR数量
 * @param jumpCount 需跳过的初始LR数量
 * @details 遍历栈帧获取LR值，支持不同编译器（GCC/Clang）的栈帧结构
 */
VOID LOS_RecordLR(UINTPTR *LR, UINT32 LRSize, UINT32 recordCount, UINT32 jumpCount)
{
    UINT32 count = 0;          // 已记录LR数量
    UINT32 index = 0;          // 栈帧索引
    UINT32 stackStart, stackEnd;  // 栈起始和结束地址
    LosTaskCB *taskCB = NULL;  // 任务控制块指针
    UINTPTR framePtr, tmpFramePtr, linkReg;  // 帧指针、临时帧指针、链接寄存器值

    if (LR == NULL) {  // 参数校验：LR数组为空则返回
        return;
    }
    /* if LR array is not enough,just record LRSize. */
    if (LRSize < recordCount) {  // 若数组大小不足，调整记录数量
        recordCount = LRSize;
    }

    taskCB = OsCurrTaskGet();          // 获取当前任务控制块
    stackStart = taskCB->topOfStack;   // 栈起始地址（栈顶）
    stackEnd = stackStart + taskCB->stackSize;  // 栈结束地址（栈底）

    framePtr = Get_Fp();  // 获取当前帧指针(FPSCR)
    // 遍历栈帧：帧指针在栈范围内且按指针大小对齐
    while ((framePtr > stackStart) && (framePtr < stackEnd) && IS_ALIGNED(framePtr, sizeof(CHAR *))) {
        tmpFramePtr = framePtr;  // 保存当前帧指针
#ifdef LOSCFG_COMPILER_CLANG_LLVM  // Clang编译器
        linkReg = *(UINTPTR *)(tmpFramePtr + sizeof(UINTPTR));  // LR位于帧指针+4字节处
#else  // GCC编译器
        linkReg = *(UINTPTR *)framePtr;  // LR位于帧指针处
#endif
        if (index >= jumpCount) {  // 跳过前jumpCount个LR
            LR[count++] = linkReg;  // 记录LR值
            if (count == recordCount) {  // 达到需记录数量则退出
                break;
            }
        }
        index++;  // 栈帧索引递增
#ifdef LOSCFG_COMPILER_CLANG_LLVM  // Clang编译器
        framePtr = *(UINTPTR *)framePtr;  // 下一个帧指针 = 当前帧指针指向的值
#else  // GCC编译器
        framePtr = *(UINTPTR *)(tmpFramePtr - sizeof(UINTPTR));  // 下一个帧指针 = 当前帧指针-4字节指向的值
#endif
    }

    /* if linkReg is not enough,clean up the last of the effective LR as the end. */
    if (count < recordCount) {  // 若记录数量不足
        LR[count] = 0;  // 用0标记结束
    }
}

