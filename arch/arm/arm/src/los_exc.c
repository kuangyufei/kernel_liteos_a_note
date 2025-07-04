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

#define INVALID_CPUID 0xFFFF
#define OS_EXC_VMM_NO_REGION 0x0U
#define OS_EXC_VMM_ALL_REGION 0x1U

STATIC UINTPTR g_minAddr;
STATIC UINTPTR g_maxAddr;
STATIC UINT32 g_currHandleExcCpuid = INVALID_CPUID;
VOID OsExcHook(UINT32 excType, ExcContext *excBufAddr, UINT32 far, UINT32 fsr);
UINT32 g_curNestCount[LOSCFG_KERNEL_CORE_NUM] = {0};       ///< 记录当前嵌套异常的数量
BOOL g_excFromUserMode[LOSCFG_KERNEL_CORE_NUM];            ///< 记录CPU core 异常来自用户态还是内核态 TRUE为用户态,默认为内核态
STATIC EXC_PROC_FUNC g_excHook = (EXC_PROC_FUNC)OsExcHook; ///< 全局异常处理钩子
#ifdef LOSCFG_KERNEL_SMP
STATIC SPIN_LOCK_INIT(g_excSerializerSpin); //初始化异常系列化自旋锁
STATIC UINT32 g_currHandleExcPID = OS_INVALID_VALUE;
STATIC UINT32 g_nextExcWaitCpu = INVALID_CPUID;
#endif

#define OS_MAX_BACKTRACE 15U //打印栈内容的数目
#define DUMPSIZE 128U
#define DUMPREGS 12U
#define COM_REGS            4U
#define INSTR_SET_MASK 0x01000020U
#define THUMB_INSTR_LEN 2U
#define ARM_INSTR_LEN 4U
#define POINTER_SIZE 4U
#define WNR_BIT 11U
#define FSR_FLAG_OFFSET_BIT 10U
#define FSR_BITS_BEGIN_BIT 3U

#define GET_FS(fsr) (((fsr)&0xFU) | (((fsr) & (1U << 10)) >> 6))
#define GET_WNR(dfsr) ((dfsr) & (1U << 11))

#define IS_VALID_ADDR(ptr) (((ptr) >= g_minAddr) && \
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
STATIC const StackInfo g_excStack[] = {
    {&__svc_stack, OS_EXC_SVC_STACK_SIZE, "svc_stack"}, //8K 主管模式堆栈.有些指令只能在SVC模式下运行
    {&__exc_stack, OS_EXC_STACK_SIZE, "exc_stack"}      //4K 异常处理堆栈
};
/// 获取系统状态
UINT32 OsGetSystemStatus(VOID)
{
    UINT32 flag;
    UINT32 cpuid = g_currHandleExcCpuid; //全局变量 当前执行CPU ID

    if (cpuid == INVALID_CPUID) { //当前CPU处于空闲状态的情况
        flag = OS_SYSTEM_NORMAL;
    } else if (cpuid == ArchCurrCpuid()) { //碰到了正在执行此处代码的CPU core
        flag = OS_SYSTEM_EXC_CURR_CPU; //当前CPU
    } else {
        flag = OS_SYSTEM_EXC_OTHER_CPU; //其他CPU
    }

    return flag;
}

STATIC INT32 OsDecodeFS(UINT32 bitsFS)
{
    switch (bitsFS) {
    case 0x05: /* 0b00101 */
    case 0x07: /* 0b00111 */
        PrintExcInfo("Translation fault, %s\n", (bitsFS & 0x2) ? "page" : "section");
        break;
    case 0x09: /* 0b01001 */
    case 0x0b: /* 0b01011 */
        PrintExcInfo("Domain fault, %s\n", (bitsFS & 0x2) ? "page" : "section");
        break;
    case 0x0d: /* 0b01101 */
    case 0x0f: /* 0b01111 */
        PrintExcInfo("Permission fault, %s\n", (bitsFS & 0x2) ? "page" : "section");
        break;
    default:
        PrintExcInfo("Unknown fault! FS:0x%x. "
                     "Check IFSR and DFSR in ARM Architecture Reference Manual.\n",
                     bitsFS);
        break;
    }

    return LOS_OK;
}

STATIC INT32 OsDecodeInstructionFSR(UINT32 regIFSR)
{
    INT32 ret;
    UINT32 bitsFS = GET_FS(regIFSR); /* FS bits[4]+[3:0] */

    ret = OsDecodeFS(bitsFS);
    return ret;
}

/**
 * @brief 解析数据异常状态寄存器 (DFSR) 的值，输出异常原因信息。
 *
 * 该函数会根据 DFSR 寄存器的值判断异常是由读指令还是写指令引起的，
 * 并进一步判断异常类型，如对齐错误或其他类型的错误，最后调用 OsDecodeFS 函数解析错误码。
 *
 * @param regDFSR 数据异常状态寄存器 (DFSR) 的值。
 * @return INT32 操作结果，成功返回 0，失败返回相应错误码。
 */
STATIC INT32 OsDecodeDataFSR(UINT32 regDFSR)
{
    INT32 ret = 0; // 初始化返回值为 0，表示操作成功
    UINT32 bitWnR = GET_WNR(regDFSR); /* WnR bit[11]，获取写/读标志位，1 表示写操作，0 表示读操作 */
    UINT32 bitsFS = GET_FS(regDFSR);  /* FS bits[4]+[3:0]，获取错误状态位 */

    if (bitWnR) { // 判断是否为写操作引起的异常
        PrintExcInfo("Abort caused by a write instruction. "); // 输出写操作引起异常的信息
    } else {
        PrintExcInfo("Abort caused by a read instruction. "); // 输出读操作引起异常的信息
    }

    if (bitsFS == 0x01) { /* 0b00001，判断是否为对齐错误 */
        PrintExcInfo("Alignment fault.\n"); // 输出对齐错误信息
        return ret; // 直接返回结果
    }
    ret = OsDecodeFS(bitsFS); // 调用 OsDecodeFS 函数解析错误状态位
    return ret; // 返回解析结果
}

#ifdef LOSCFG_KERNEL_VM

/**
 * @brief 处理 ARM 架构下的共享页错误异常。
 *
 * 该函数会根据异常类型、异常上下文、错误地址和错误状态寄存器的值，
 * 对共享页错误异常进行处理。处理过程中会根据不同的错误状态进行不同的操作，
 * 如调用页错误处理函数或刷新 TLB 等。
 *
 * @param excType 异常类型，如预取指令异常、数据异常等。
 * @param frame 异常上下文，包含寄存器等信息。
 * @param far 错误地址寄存器的值，指示发生错误的地址。
 * @param fsr 错误状态寄存器的值，包含错误的详细信息。
 * @return UINT32 处理结果，成功返回 LOS_OK，失败返回相应错误码。
 */
UINT32 OsArmSharedPageFault(UINT32 excType, ExcContext *frame, UINT32 far, UINT32 fsr)
{
    BOOL instructionFault = FALSE; // 标记是否为取指异常
    UINT32 pfFlags = 0;            // 页错误标志
    UINT32 fsrFlag;                // 从 FSR 寄存器提取的标志
    BOOL write = FALSE;            // 标记是否为写操作引起的异常
    UINT32 ret;                    // 函数返回值

    PRINT_INFO("page fault entry!!!\n"); // 打印页错误进入信息

    if (OsGetSystemStatus() == OS_SYSTEM_EXC_CURR_CPU) { // 检查当前系统是否处于当前 CPU 异常状态
        return LOS_ERRNO_VM_NOT_FOUND; // 若是，返回虚拟内存未找到错误
    }

#if defined(LOSCFG_KERNEL_SMP) && defined(LOSCFG_DEBUG_VERSION)
    BOOL irqEnable = !(LOS_SpinHeld(&g_taskSpin) && OsSchedIsLock()); // 判断中断是否应该启用
    if (irqEnable) {
        ArchIrqEnable(); // 启用中断
    } else {
        PrintExcInfo("[ERR][%s] may be held scheduler lock when entering [%s] on cpu [%u]\n",
                     OsCurrTaskGet()->taskName, __FUNCTION__, ArchCurrCpuid()); // 打印可能持有调度锁的错误信息
    }
#else
    ArchIrqEnable(); // 启用中断
#endif

    if (excType == OS_EXCEPT_PREFETCH_ABORT) { // 判断是否为预取指令异常
        instructionFault = TRUE; // 标记为取指异常
    } else {
        write = !!BIT_GET(fsr, WNR_BIT); // 从 FSR 寄存器获取写/读标志，判断是否为写操作引起的异常
    }

    // 从 FSR 寄存器提取标志位
    fsrFlag = ((BIT_GET(fsr, FSR_FLAG_OFFSET_BIT) ? 0b10000 : 0) | BITS_GET(fsr, FSR_BITS_BEGIN_BIT, 0));
    switch (fsrFlag) {
        case 0b00101:
        /* translation fault */
        case 0b00111:
        /* translation fault */
        case 0b01101:
        /* permission fault */
        case 0b01111: {
        /* permission fault */
            BOOL user = (frame->regCPSR & CPSR_MODE_MASK) == CPSR_MODE_USR; // 判断是否为用户模式
            pfFlags |= write ? VM_MAP_PF_FLAG_WRITE : 0; // 根据是否为写操作设置标志
            pfFlags |= user ? VM_MAP_PF_FLAG_USER : 0;   // 根据是否为用户模式设置标志
            pfFlags |= instructionFault ? VM_MAP_PF_FLAG_INSTRUCTION : 0; // 根据是否为取指异常设置标志
            pfFlags |= VM_MAP_PF_FLAG_NOT_PRESENT; // 设置页面不存在标志

            OsSigIntLock(); // 锁定信号中断
            ret = OsVmPageFaultHandler(far, pfFlags, frame); // 调用页错误处理函数
            OsSigIntUnlock(); // 解锁信号中断
            break;
        }
        default:
            OsArmWriteTlbimvaais(ROUNDDOWN(far, PAGE_SIZE)); // 刷新 TLB
            ret = LOS_OK; // 返回成功
            break;
    }

#if defined(LOSCFG_KERNEL_SMP) && defined(LOSCFG_DEBUG_VERSION)
    if (irqEnable) {
        ArchIrqDisable(); // 禁用中断
    }
#else
    ArchIrqDisable(); // 禁用中断
#endif

    return ret; // 返回处理结果
}
#endif
/**
 * @brief 根据异常类型处理异常，并打印异常信息。
 *
 * 该函数会根据不同的异常类型，对异常上下文进行调整，
 * 并打印相应的异常信息，同时解析异常状态寄存器的值。
 *
 * @param excType 异常类型，如未定义指令异常、软件中断异常等。
 * @param excBufAddr 异常上下文，包含寄存器等信息。
 * @param far 错误地址寄存器的值，指示发生错误的地址。
 * @param fsr 错误状态寄存器的值，包含错误的详细信息。
 */
STATIC VOID OsExcType(UINT32 excType, ExcContext *excBufAddr, UINT32 far, UINT32 fsr)
{
    // 未定义的异常处理或软件中断
    if ((excType == OS_EXCEPT_UNDEF_INSTR) || (excType == OS_EXCEPT_SWI)) {
        if ((excBufAddr->regCPSR & INSTR_SET_MASK) == 0) { // 判断工作状态是否为 ARM 模式
            excBufAddr->PC = excBufAddr->PC - ARM_INSTR_LEN; // 在 ARM 模式下，PC 回退 ARM 指令长度（4 字节）
        } else if ((excBufAddr->regCPSR & INSTR_SET_MASK) == 0x20) { // 判断工作状态是否为 Thumb 模式
            excBufAddr->PC = excBufAddr->PC - THUMB_INSTR_LEN; // 在 Thumb 模式下，PC 回退 Thumb 指令长度（2 字节）
        }
    }

    if (excType == OS_EXCEPT_PREFETCH_ABORT) { // 处理取指异常
        PrintExcInfo("prefetch_abort fault fsr:0x%x, far:0x%0+8x\n", fsr, far); // 打印取指异常信息，包含错误状态寄存器和错误地址寄存器的值
        (VOID) OsDecodeInstructionFSR(fsr); // 解析指令异常状态寄存器的值
    } else if (excType == OS_EXCEPT_DATA_ABORT) { // 处理数据异常
        PrintExcInfo("data_abort fsr:0x%x, far:0x%0+8x\n", fsr, far); // 打印数据异常信息，包含错误状态寄存器和错误地址寄存器的值
        (VOID) OsDecodeDataFSR(fsr); // 解析数据异常状态寄存器的值
    }
}

STATIC const CHAR *g_excTypeString[] = {
    ///< 异常类型的字符说明,在鸿蒙内核中什么才算是异常? 看这里
    "reset",                 //复位异常源    - SVC模式（Supervisor保护模式）
    "undefined instruction", //未定义指令异常源- und模式
    "software interrupt",    //软中断异常源 - SVC模式
    "prefetch abort",        //取指异常源 - abort模式
    "data abort",            //数据异常源 - abort模式
    "fiq",                   //快中断异常源 - FIQ模式
    "address abort",         //地址异常源 - abort模式
    "irq"                    //中断异常源 - IRQ模式
};

#ifdef LOSCFG_KERNEL_VM
/**
 * @brief 获取文本段区域的基地址。
 *
 * 该函数会根据给定的虚拟内存映射区域和进程控制块，查找并返回文本段区域的基地址。
 * 如果区域无效或关联文件无效，则直接返回区域的起始地址。
 *
 * @param region 虚拟内存映射区域指针。
 * @param runProcess 进程控制块指针。
 * @return VADDR_T 文本段区域的基地址，若参数无效则返回 0。
 */
STATIC VADDR_T OsGetTextRegionBase(LosVmMapRegion *region, LosProcessCB *runProcess)
{
    struct Vnode *curVnode = NULL;  // 当前区域关联的虚拟节点指针
    struct Vnode *lastVnode = NULL; // 上一个区域关联的虚拟节点指针
    LosVmMapRegion *curRegion = NULL;  // 当前虚拟内存映射区域指针
    LosVmMapRegion *lastRegion = NULL; // 上一个虚拟内存映射区域指针

    // 检查传入的区域指针和进程控制块指针是否有效，若无效则返回 0
    if ((region == NULL) || (runProcess == NULL)) {
        return 0;
    }

    // 检查区域关联的文件是否有效，若无效则直接返回区域的起始地址
    if (!LOS_IsRegionFileValid(region)) {
        return region->range.base;
    }

    lastRegion = region; // 初始化上一个区域为传入的区域
    do {
        curRegion = lastRegion; // 当前区域等于上一个区域
        // 查找上一个区域起始地址减 1 处的区域
        lastRegion = LOS_RegionFind(runProcess->vmSpace, curRegion->range.base - 1);
        // 若上一个区域无效或关联文件无效，则跳出循环
        if ((lastRegion == NULL) || !LOS_IsRegionFileValid(lastRegion)) {
            goto DONE;
        }
        curVnode = curRegion->unTypeData.rf.vnode; // 获取当前区域关联的虚拟节点
        lastVnode = lastRegion->unTypeData.rf.vnode; // 获取上一个区域关联的虚拟节点
    } while (curVnode == lastVnode); // 若当前和上一个区域关联的虚拟节点相同，则继续循环

DONE:
#ifdef LOSCFG_KERNEL_DYNLOAD
    // 若当前区域的起始地址等于动态加载的基地址，则返回 0
    if (curRegion->range.base == EXEC_MMAP_BASE) {
        return 0;
    }
#endif
    return curRegion->range.base; // 返回当前区域的起始地址
}
#endif
/**
 * @brief 打印异常系统信息
 * 
 * @param excType 
 * @param excBufAddr 
 * @return STATIC 
 */
STATIC VOID OsExcSysInfo(UINT32 excType, const ExcContext *excBufAddr)
{
    LosTaskCB *runTask = OsCurrTaskGet();          //获取当前任务
    LosProcessCB *runProcess = OsCurrProcessGet(); //获取当前进程

    PrintExcInfo("excType: %s\n"
                 "processName       = %s\n"
                 "processID         = %u\n"
#ifdef LOSCFG_KERNEL_VM
                 "process aspace    = 0x%08x -> 0x%08x\n"
#endif
                 "taskName          = %s\n"
                 "taskID            = %u\n",
                 g_excTypeString[excType],
                 runProcess->processName,
                 runProcess->processID,
#ifdef LOSCFG_KERNEL_VM
                 runProcess->vmSpace->base,
                 runProcess->vmSpace->base + runProcess->vmSpace->size,
#endif
                 runTask->taskName,
                 runTask->taskID);
    //这里可以看出一个任务有两个运行栈空间
#ifdef LOSCFG_KERNEL_VM
    if (OsProcessIsUserMode(runProcess)) { //用户态栈空间,对于栈而言,栈底的地址要小于栈顶的地址
        PrintExcInfo("task user stack   = 0x%08x -> 0x%08x\n", //用户态栈空间由用户空间提供
                     runTask->userMapBase, runTask->userMapBase + runTask->userMapSize);
    } else
#endif
    {
        PrintExcInfo("task kernel stack = 0x%08x -> 0x%08x\n", //
                     runTask->topOfStack, runTask->topOfStack + runTask->stackSize);
    }

    PrintExcInfo("pc    = 0x%x ", excBufAddr->PC);
#ifdef LOSCFG_KERNEL_VM
    LosVmMapRegion *region = NULL;
    if (g_excFromUserMode[ArchCurrCpuid()] == TRUE) { //当前CPU处于用户模式
        if (LOS_IsUserAddress((vaddr_t)excBufAddr->PC)) {  //pc寄存器处于用户空间
            region = LOS_RegionFind(runProcess->vmSpace, (VADDR_T)excBufAddr->PC); //找到所在线性区
            if (region != NULL) {
                PrintExcInfo("in %s ---> 0x%x", OsGetRegionNameOrFilePath(region),
                             (VADDR_T)excBufAddr->PC - OsGetTextRegionBase(region, runProcess));
            }
        }

        PrintExcInfo("\nulr   = 0x%x ", excBufAddr->ULR);                       //打印用户模式下程序的返回地址
        region = LOS_RegionFind(runProcess->vmSpace, (VADDR_T)excBufAddr->ULR); //通过返回地址找到线性区
        if (region != NULL) {
            PrintExcInfo("in %s ---> 0x%x", OsGetRegionNameOrFilePath(region),
                         (VADDR_T)excBufAddr->ULR - OsGetTextRegionBase(region, runProcess));
        }
        PrintExcInfo("\nusp   = 0x%x", excBufAddr->USP); //打印用户模式下栈指针
    } else
#endif
    {
        PrintExcInfo("\nklr   = 0x%x\n" //注意：只有一个内核空间和内核堆空间，而每个用户进程就有自己独立的用户空间。
                     "ksp   = 0x%x\n",  //用户空间的虚拟地址范围是一样的，只是映射到不同的物理内存页。
                     excBufAddr->LR,    //直接打印程序的返回地址
                     excBufAddr->SP);   //直接打印栈指针
    }

    PrintExcInfo("\nfp    = 0x%x\n", excBufAddr->R11);
}
/**
 * @brief 异常情况下打印各寄存器的信息
 * 
 * @param excBufAddr 
 * @return STATIC 
 */
STATIC VOID OsExcRegsInfo(const ExcContext *excBufAddr)
{
    /*
     * Split register information into two parts: 
     * Ensure printing does not rely on memory modules.
     */
    //将寄存器信息分为两部分：确保打印不依赖内存模块。
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
///注册异常处理钩子
LITE_OS_SEC_TEXT_INIT UINT32 LOS_ExcRegHook(EXC_PROC_FUNC excHook)
{
    UINT32 intSave;

    intSave = LOS_IntLock();
    g_excHook = excHook;
    LOS_IntRestore(intSave);

    return LOS_OK;
}
///获取hook函数
EXC_PROC_FUNC OsExcRegHookGet(VOID)
{
    return g_excHook;
}
///dump 虚拟空间下异常虚拟地址线性区
#ifdef LOSCFG_KERNEL_VM
STATIC VOID OsDumpExcVaddrRegion(LosVmSpace *space, LosVmMapRegion *region)
{
    INT32 i, numPages, pageCount;
    paddr_t addr, oldAddr, startVaddr, startPaddr;
    vaddr_t pageBase;
    BOOL mmuFlag = FALSE;

    numPages = region->range.size >> PAGE_SHIFT;
    mmuFlag = TRUE;
    for (pageCount = 0, startPaddr = 0, startVaddr = 0, i = 0; i < numPages; i++) {
        pageBase = region->range.base + i * PAGE_SIZE;
        addr = 0;
        if (LOS_ArchMmuQuery(&space->archMmu, pageBase, &addr, NULL) != LOS_OK) {
            if (startPaddr == 0) {
                continue;
            }
        } else if (startPaddr == 0) {
            startVaddr = pageBase;
            startPaddr = addr;
            oldAddr = addr;
            pageCount++;
            if (numPages > 1) {
                continue;
            }
        } else if (addr == (oldAddr + PAGE_SIZE)) {
            pageCount++;
            oldAddr = addr;
            if (i < (numPages - 1)) {
                continue;
            }
        }
        if (mmuFlag == TRUE) {
            PrintExcInfo("       uvaddr       kvaddr       mapped size\n");
            mmuFlag = FALSE;
        }
        PrintExcInfo("       0x%08x   0x%08x   0x%08x\n",
                     startVaddr, LOS_PaddrToKVaddr(startPaddr), (UINT32)pageCount << PAGE_SHIFT);
        pageCount = 0;
        startPaddr = 0;
    }
}
///dump 进程使用的内存线性区
STATIC VOID OsDumpProcessUsedMemRegion(LosProcessCB *runProcess, LosVmSpace *runspace, UINT16 vmmFlags)
{
    LosVmMapRegion *region = NULL;
    LosRbNode *pstRbNodeTemp = NULL;
    LosRbNode *pstRbNodeNext = NULL;
    UINT32 count = 0;

    /* search the region list */
    RB_SCAN_SAFE(&runspace->regionRbTree, pstRbNodeTemp, pstRbNodeNext)
    region = (LosVmMapRegion *)pstRbNodeTemp;
    PrintExcInfo("%3u -> regionBase: 0x%08x regionSize: 0x%08x\n", count, region->range.base, region->range.size);
        if (vmmFlags == OS_EXC_VMM_ALL_REGION) {
            OsDumpExcVaddrRegion(runspace, region);
        }
    count++;
    (VOID) OsRegionOverlapCheckUnlock(runspace, region);
    RB_SCAN_SAFE_END(&space->regionRbTree, pstRbNodeTemp, pstRbNodeNext)
}
///dump 进程使用的内存节点
STATIC VOID OsDumpProcessUsedMemNode(UINT16 vmmFlags)
{
    LosProcessCB *runProcess = NULL;
    LosVmSpace *runspace = NULL;

    runProcess = OsCurrProcessGet();
    if (runProcess == NULL) {
        return;
    }

    if (!OsProcessIsUserMode(runProcess)) {
        return;
    }

    PrintExcInfo("\n   ******Current process %u vmm regions: ******\n", runProcess->processID);

    runspace = runProcess->vmSpace;
    if (!runspace) {
        return;
    }

    OsDumpProcessUsedMemRegion(runProcess, runspace, vmmFlags);
    return;
}
#endif

/** 打印异常上下文
 * @brief dump 上下文内存,注意内核异常不能简单的映射理解为应用的异常,异常对内核来说是一个很常见操作,
    \n 比如任务的切换对内核来说就是一个异常处理
 * @param excBufAddr 
 * @return VOID 
 */
VOID OsDumpContextMem(const ExcContext *excBufAddr)
{
    UINT32 count = 0;
    const UINT32 *excReg = NULL;
    if (g_excFromUserMode[ArchCurrCpuid()] == TRUE) {
        return;
    }

    for (excReg = &(excBufAddr->R0); count < COM_REGS; excReg++, count++) {
        if (IS_VALID_ADDR(*excReg)) {
            PrintExcInfo("\ndump mem around R%u:%p", count, (*excReg));
            OsDumpMemByte(DUMPSIZE, ((*excReg) - (DUMPSIZE >> 1)));
        }
    }
    for (excReg = &(excBufAddr->R4); count < DUMPREGS; excReg++, count++) {
        if (IS_VALID_ADDR(*excReg)) {
            PrintExcInfo("\ndump mem around R%u:%p", count, (*excReg));
            OsDumpMemByte(DUMPSIZE, ((*excReg) - (DUMPSIZE >> 1)));
        }
    }
    if (IS_VALID_ADDR(excBufAddr->R12)) {
        PrintExcInfo("\ndump mem around R12:%p", excBufAddr->R12);
        OsDumpMemByte(DUMPSIZE, (excBufAddr->R12 - (DUMPSIZE >> 1)));
    }
    if (IS_VALID_ADDR(excBufAddr->SP)) {
        PrintExcInfo("\ndump mem around SP:%p", excBufAddr->SP);
        OsDumpMemByte(DUMPSIZE, (excBufAddr->SP - (DUMPSIZE >> 1)));
    }
}
///异常恢复,继续执行
STATIC VOID OsExcRestore(VOID)
{
    UINT32 currCpuid = ArchCurrCpuid();

    g_excFromUserMode[currCpuid] = FALSE;
    g_intCount[currCpuid] = 0;
    g_curNestCount[currCpuid] = 0;
#ifdef LOSCFG_KERNEL_SMP
    OsCpuStatusSet(CPU_RUNNING);
#endif
    OsSchedLockSet(0);
}
///用户态异常处理函数
STATIC VOID OsUserExcHandle(ExcContext *excBufAddr)
{
    UINT32 intSave;
    UINT32 currCpu = ArchCurrCpuid();
    LosTaskCB *runTask = OsCurrTaskGet();
    LosProcessCB *runProcess = OsCurrProcessGet();

    if (g_excFromUserMode[ArchCurrCpuid()] == FALSE) { //内核态直接退出,不处理了.
        return;
    }

#ifdef LOSCFG_KERNEL_SMP
    LOS_SpinLock(&g_excSerializerSpin);
    if (g_nextExcWaitCpu != INVALID_CPUID) {
        g_currHandleExcCpuid = g_nextExcWaitCpu;
        g_nextExcWaitCpu = INVALID_CPUID;
    } else {
        g_currHandleExcCpuid = INVALID_CPUID;
    }
    g_currHandleExcPID = OS_INVALID_VALUE;
    LOS_SpinUnlock(&g_excSerializerSpin);
#else
    g_currHandleExcCpuid = INVALID_CPUID;
#endif

#ifdef LOSCFG_KERNEL_SMP
#ifdef LOSCFG_FS_VFS
    OsWakeConsoleSendTask();
#endif
#endif

#ifdef LOSCFG_BLACKBOX
    BBoxNotifyError("USER_CRASH", MODULE_SYSTEM, "Crash in user", 0);
#endif
    SCHEDULER_LOCK(intSave);
#ifdef LOSCFG_SAVE_EXCINFO
    OsProcessExitCodeCoreDumpSet(runProcess);
#endif
    OsProcessExitCodeSignalSet(runProcess, SIGUSR2);

    /* An exception was raised by a task that is not the current main thread during the exit process of
     * the current process.
     */
    if (runProcess->processStatus & OS_PROCESS_FLAG_EXIT) {
        SCHEDULER_UNLOCK(intSave);
        /* Exception handling All operations should be kept prior to that operation */
        OsExcRestore();
        OsRunningTaskToExit(runTask, OS_PRO_EXIT_OK);
    } else {
        SCHEDULER_UNLOCK(intSave);

        /* Exception handling All operations should be kept prior to that operation */
        OsExcRestore();
        /* kill user exc process */
        LOS_Exit(OS_PRO_EXIT_OK); //进程退出
    }

    /* User mode exception handling failed , which normally does not exist */ //用户态的异常处理失败，通常情况下不会发生
    g_curNestCount[currCpu]++;
    g_intCount[currCpu]++;
    PrintExcInfo("User mode exception ends unscheduled!\n");
}
///此函数用于验证fp或验证检查开始和结束范围
//sp是上级函数即调用者的堆栈首地址，fp是上级函数的堆栈结束地址
/* this function is used to validate fp or validate the checking range start and end. */
STATIC INLINE BOOL IsValidFP(UINTPTR regFP, UINTPTR start, UINTPTR end, vaddr_t *vaddr)
{
    VADDR_T kvaddr = regFP;

    if (!((regFP > start) && (regFP < end) && IS_ALIGNED(regFP, sizeof(CHAR *)))) {
        return FALSE;
    }

#ifdef LOSCFG_KERNEL_VM
    PADDR_T paddr;
    if (g_excFromUserMode[ArchCurrCpuid()] == TRUE) {
        LosProcessCB *runProcess = OsCurrProcessGet();
        LosVmSpace *runspace = runProcess->vmSpace;
        if (runspace == NULL) {
            return FALSE;
        }

        if (LOS_ArchMmuQuery(&runspace->archMmu, regFP, &paddr, NULL) != LOS_OK) {
            return FALSE;
        }

        kvaddr = (PADDR_T)(UINTPTR)LOS_PaddrToKVaddr(paddr);
    }
#endif
    if (vaddr != NULL) {
        *vaddr = kvaddr;
    }

    return TRUE;
}
///找到一个合适的栈
STATIC INLINE BOOL FindSuitableStack(UINTPTR regFP, UINTPTR *start, UINTPTR *end, vaddr_t *vaddr)
{
    UINT32 index, stackStart, stackEnd;
    BOOL found = FALSE;
    LosTaskCB *taskCB = NULL;
    const StackInfo *stack = NULL;
    vaddr_t kvaddr;

#ifdef LOSCFG_KERNEL_VM
    if (g_excFromUserMode[ArchCurrCpuid()] == TRUE) {//当前CPU在用户态执行发生异常
        taskCB = OsCurrTaskGet();
        stackStart = taskCB->userMapBase;                     //用户态栈基地址,即用户态栈顶
        stackEnd = taskCB->userMapBase + taskCB->userMapSize; //用户态栈结束地址
        if (IsValidFP(regFP, stackStart, stackEnd, &kvaddr) == TRUE) {
            found = TRUE;
            goto FOUND;
        }
        return found;
    }
#endif

    /* Search in the task stacks | 找任务的内核态栈*/
    for (index = 0; index < g_taskMaxNum; index++) {
        taskCB = &g_taskCBArray[index];
        if (OsTaskIsUnused(taskCB)) {
            continue;
        }

        stackStart = taskCB->topOfStack;                   //内核态栈顶
        stackEnd = taskCB->topOfStack + taskCB->stackSize; //内核态栈底
        if (IsValidFP(regFP, stackStart, stackEnd, &kvaddr) == TRUE) {
            found = TRUE;
            goto FOUND;
        }
    }

    /* Search in the exc stacks */ //从异常栈中找
    for (index = 0; index < sizeof(g_excStack) / sizeof(StackInfo); index++) {
        stack = &g_excStack[index];
        stackStart = (UINTPTR)stack->stackTop;
        stackEnd = stackStart + LOSCFG_KERNEL_CORE_NUM * stack->stackSize;
        if (IsValidFP(regFP, stackStart, stackEnd, &kvaddr) == TRUE) {
            found = TRUE;
            goto FOUND;
        }
    }

FOUND:
    if (found == TRUE) {
        *start = stackStart;
        *end = stackEnd;
        *vaddr = kvaddr;
    }

    return found;
}

BOOL OsGetUsrIpInfo(UINTPTR ip, IpInfo *info)
{
    if (info == NULL) {
        return FALSE;
    }
#ifdef LOSCFG_KERNEL_VM
    BOOL ret = FALSE;
    const CHAR *name = NULL;
    LosVmMapRegion *region = NULL;
    LosProcessCB *runProcess = OsCurrProcessGet();

    if (LOS_IsUserAddress((VADDR_T)ip) == FALSE) {
        info->ip = ip;
        name = "kernel";
        ret = FALSE;
        goto END;
    }

    region = LOS_RegionFind(runProcess->vmSpace, (VADDR_T)ip);
    if (region == NULL) {
        info->ip = ip;
        name = "invalid";
        ret = FALSE;
        goto END;
    }

    info->ip = ip - OsGetTextRegionBase(region, runProcess);
    name = OsGetRegionNameOrFilePath(region);
    ret = TRUE;
    if (strcmp(name, "/lib/libc.so") != 0) {
        PRINT_ERR("ip = 0x%x, %s\n", info->ip, name);
    }
END:
    info->len = strlen(name);
    if (strncpy_s(info->f_path, REGION_PATH_MAX, name, REGION_PATH_MAX - 1) != EOK) {
        info->f_path[0] = '\0';
        info->len = 0;
        PRINT_ERR("copy f_path failed, %s\n", name);
    }
    return ret;
#else
    info->ip = ip;
    return FALSE;
#endif
}

UINT32 BackTraceGet(UINTPTR regFP, IpInfo *callChain, UINT32 maxDepth)
{
    UINTPTR tmpFP, backLR;
    UINTPTR stackStart, stackEnd;
    UINTPTR backFP = regFP;
    UINT32 count = 0;
    BOOL ret;
    VADDR_T kvaddr;

    if (FindSuitableStack(regFP, &stackStart, &stackEnd, &kvaddr) == FALSE) {
        if (callChain == NULL) {
            PrintExcInfo("traceback error fp = 0x%x\n", regFP);
        }
        return 0;
    }

    /*
     * Check whether it is the leaf function.
     * Generally, the frame pointer points to the address of link register, while in the leaf function,
     * there's no function call, and compiler will not store the link register, but the frame pointer
     * will still be stored and updated. In that case we needs to find the right position of frame pointer.
     */
    tmpFP = *(UINTPTR *)(UINTPTR)kvaddr;
    if (IsValidFP(tmpFP, stackStart, stackEnd, NULL) == TRUE) {
        backFP = tmpFP;
        if (callChain == NULL) {
            PrintExcInfo("traceback fp fixed, trace using   fp = 0x%x\n", backFP);
        }
    }

    while (IsValidFP(backFP, stackStart, stackEnd, &kvaddr) == TRUE) {
        tmpFP = backFP;
#ifdef LOSCFG_COMPILER_CLANG_LLVM
        backFP = *(UINTPTR *)(UINTPTR)kvaddr;
        if (IsValidFP(tmpFP + POINTER_SIZE, stackStart, stackEnd, &kvaddr) == FALSE) {
            if (callChain == NULL) {
                PrintExcInfo("traceback backLR check failed, backLP: 0x%x\n", tmpFP + POINTER_SIZE);
            }
            return 0;
        }
        backLR = *(UINTPTR *)(UINTPTR)kvaddr;
#else
        backLR = *(UINTPTR *)(UINTPTR)kvaddr;
        if (IsValidFP(tmpFP - POINTER_SIZE, stackStart, stackEnd, &kvaddr) == FALSE) {
            if (callChain == NULL) {
                PrintExcInfo("traceback backFP check failed, backFP: 0x%x\n", tmpFP - POINTER_SIZE);
            }
            return 0;
        }
        backFP = *(UINTPTR *)(UINTPTR)kvaddr;
#endif
        IpInfo info = {0};
        ret = OsGetUsrIpInfo((VADDR_T)backLR, &info);
        if (callChain == NULL) {
            PrintExcInfo("traceback %u -- lr = 0x%x    fp = 0x%x ", count, backLR, backFP);
            if (ret) {
#ifdef LOSCFG_KERNEL_VM
                PrintExcInfo("lr in %s --> 0x%x\n", info.f_path, info.ip);
#else
                PrintExcInfo("\n");
#endif
            } else {
                PrintExcInfo("\n");
            }
        } else {
            (VOID)memcpy_s(&callChain[count], sizeof(IpInfo), &info, sizeof(IpInfo));
        }
        count++;
        if ((count == maxDepth) || (backFP == tmpFP)) {
            break;
        }
    }
    return count;
}

VOID BackTraceSub(UINTPTR regFP)
{
    (VOID) BackTraceGet(regFP, NULL, OS_MAX_BACKTRACE);
}
///打印调用栈信息
VOID BackTrace(UINT32 regFP) //fp:R11寄存器
{
    PrintExcInfo("*******backtrace begin*******\n");

    BackTraceSub(regFP);
}
///异常接管模块的初始化
VOID OsExcInit(VOID)
{
    OsExcStackInfoReg(g_excStack, sizeof(g_excStack) / sizeof(g_excStack[0])); //异常模式下注册内核栈信息
}
///由注册后回调,发送异常情况下会回调这里执行,见于 OsUndefIncExcHandleEntry, OsExcHandleEntry ==函数
VOID OsExcHook(UINT32 excType, ExcContext *excBufAddr, UINT32 far, UINT32 fsr)
{                                             //参考文档 https://gitee.com/openharmony/docs/blob/master/kernel/%E7%94%A8%E6%88%B7%E6%80%81%E5%BC%82%E5%B8%B8%E4%BF%A1%E6%81%AF%E8%AF%B4%E6%98%8E.md
    OsExcType(excType, excBufAddr, far, fsr); //1.打印异常的类型
    OsExcSysInfo(excType, excBufAddr);        //2.打印异常的基本信息
    OsExcRegsInfo(excBufAddr);                //3.打印异常的寄存器信息

    BackTrace(excBufAddr->R11); //4.打印调用栈信息,

    (VOID) OsShellCmdTskInfoGet(OS_ALL_TASK_MASK, NULL, OS_PROCESS_INFO_ALL); //打印进程线程基本信息 相当于执行下 shell task -a 命令

#ifndef LOSCFG_DEBUG_VERSION //打开debug开关
    if (g_excFromUserMode[ArchCurrCpuid()] != TRUE) {
#endif
#ifdef LOSCFG_KERNEL_VM
        OsDumpProcessUsedMemNode(OS_EXC_VMM_NO_REGION);
#endif
        OsExcStackInfo(); //	打印任务栈的信息
#ifndef LOSCFG_DEBUG_VERSION
    }
#endif

    OsDumpContextMem(excBufAddr); // 打印上下文

    (VOID) OsShellCmdMemCheck(0, NULL); //检查内存,相当于执行 shell memcheck 命令

#ifdef LOSCFG_COREDUMP
    LOS_CoreDumpV2(excType, excBufAddr);
#endif

    OsUserExcHandle(excBufAddr); //用户态下异常的处理
}
///打印调用栈信息
VOID OsCallStackInfo(VOID)
{
    UINT32 count = 0;
    LosTaskCB *runTask = OsCurrTaskGet();
    UINTPTR stackBottom = runTask->topOfStack + runTask->stackSize; //内核态的 栈底 = 栈顶 + 大小
    UINT32 *stackPointer = (UINT32 *)stackBottom;

    PrintExcInfo("runTask->stackPointer = 0x%x\n"
                 "runTask->topOfStack = 0x%x\n"
                 "text_start:0x%x,text_end:0x%x\n",
                 stackPointer, runTask->topOfStack, &__text_start, &__text_end);
    //打印OS_MAX_BACKTRACE多一条栈信息,注意stack中存放的是函数调用地址和指令的地址
    while ((stackPointer > (UINT32 *)runTask->topOfStack) && (count < OS_MAX_BACKTRACE)) {
        if ((*stackPointer > (UINTPTR)(&__text_start)) && //正常情况下 sp的内容都是文本段的内容
            (*stackPointer < (UINTPTR)(&__text_end)) &&
            IS_ALIGNED((*stackPointer), POINTER_SIZE)) { //sp的内容是否对齐, sp指向指令的地址
            if ((*(stackPointer - 1) > (UINT32)runTask->topOfStack) &&
                (*(stackPointer - 1) < stackBottom) && //@note_why 这里为什么要对 stackPointer - 1 进行判断
                IS_ALIGNED((*(stackPointer - 1)), POINTER_SIZE)) {
                count++;
                PrintExcInfo("traceback %u -- lr = 0x%x\n", count, *stackPointer);
            }
        }
        stackPointer--;
    }
    PRINTK("\n");
}
/***********************************************
R11寄存器(frame pointer)
在程序执行过程中（通常是发生了某种意外情况而需要进行调试），通过SP和FP所限定的stack frame，
就可以得到母函数的SP和FP，从而得到母函数的stack frame（PC，LR，SP，FP会在函数调用的第一时间压栈），
以此追溯，即可得到所有函数的调用顺序。
***********************************************/
VOID OsTaskBackTrace(UINT32 taskID) //任务栈信息追溯
{
    LosTaskCB *taskCB = NULL;

    if (OS_TID_CHECK_INVALID(taskID)) {
        PRINT_ERR("\r\nTask ID is invalid!\n");
        return;
    }
    taskCB = OS_TCB_FROM_TID(taskID);
    if (OsTaskIsUnused(taskCB) || (taskCB->taskEntry == NULL)) {
        PRINT_ERR("\r\nThe task is not created!\n");
        return;
    }
    PRINTK("TaskName = %s\n", taskCB->taskName);
    PRINTK("TaskID = 0x%x\n", taskCB->taskID);
    BackTrace(((TaskContext *)(taskCB->stackPointer))->R11); /* R11 : FP */
}

VOID OsBackTrace(VOID)
{
    UINT32 regFP = Get_Fp();
    LosTaskCB *runTask = OsCurrTaskGet();
    PrintExcInfo("OsBackTrace fp = 0x%x\n", regFP);
    PrintExcInfo("runTask->taskName = %s\n", runTask->taskName);
    PrintExcInfo("runTask->taskID = %u\n", runTask->taskID);
    BackTrace(regFP);
}
///未定义的异常处理函数，由汇编调用 见于 los_hw_exc.s
#ifdef LOSCFG_GDB
VOID OsUndefIncExcHandleEntry(ExcContext *excBufAddr)
{
    excBufAddr->PC -= 4; /* lr in undef is pc + 4 */

    if (gdb_undef_hook(excBufAddr, OS_EXCEPT_UNDEF_INSTR)) {
        return;
    }

    if (g_excHook != NULL) {//是否注册异常打印函数
        /* far, fsr are unused in exc type of OS_EXCEPT_UNDEF_INSTR */
        g_excHook(OS_EXCEPT_UNDEF_INSTR, excBufAddr, 0, 0); //回调函数,详见: OsExcHook
    }
    while (1) {}
}

///预取指令异常处理函数，由汇编调用 见于 los_hw_exc.s
#if __LINUX_ARM_ARCH__ >= 7
VOID OsPrefetchAbortExcHandleEntry(ExcContext *excBufAddr)
{
    UINT32 far;
    UINT32 fsr;

    excBufAddr->PC -= 4; /* lr in prefetch abort is pc + 4 */

    if (gdbhw_hook(excBufAddr, OS_EXCEPT_PREFETCH_ABORT)) {
        return;
    }

    if (g_excHook != NULL) {
        far = OsArmReadIfar();                                     //far 为CP15的 C6寄存器 详见:https://blog.csdn.net/kuangyufei/article/details/108994081
        fsr = OsArmReadIfsr();                                     //fsr 为CP15的 C5寄存器
        g_excHook(OS_EXCEPT_PREFETCH_ABORT, excBufAddr, far, fsr); //回调函数,详见: OsExcHook
    }
    while (1) {}
}

///数据中止异常处理函数，由汇编调用 见于 los_hw_exc.s
VOID OsDataAbortExcHandleEntry(ExcContext *excBufAddr)
{
    UINT32 far;
    UINT32 fsr;

    excBufAddr->PC -= 8; /* lr in data abort is pc + 8 */

    if (gdbhw_hook(excBufAddr, OS_EXCEPT_DATA_ABORT)) {
        return;
    }

    if (g_excHook != NULL) {
        far = OsArmReadDfar();
        fsr = OsArmReadDfsr();
        g_excHook(OS_EXCEPT_DATA_ABORT, excBufAddr, far, fsr); ////回调函数,详见: OsExcHook
    }
    while (1) {}
}
#endif /* __LINUX_ARM_ARCH__ */
#endif /* LOSCFG_GDB */

#ifdef LOSCFG_KERNEL_SMP
#define EXC_WAIT_INTER 50U  //异常等待间隔时间
#define EXC_WAIT_TIME 2000U //异常等待时间

///等待所有CPU停止
STATIC VOID WaitAllCpuStop(UINT32 cpuid)
{
    UINT32 i;
    UINT32 time = 0;

    while (time < EXC_WAIT_TIME) {
        for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
            if ((i != cpuid) && !OsCpuStatusIsHalt(i)) {
                LOS_Mdelay(EXC_WAIT_INTER);
                time += EXC_WAIT_INTER;
                break;
            }
        }
        /* Other CPUs are all haletd or in the exc. */
        if (i == LOSCFG_KERNEL_CORE_NUM) {
            break;
        }
    }
    return;
}

STATIC VOID OsWaitOtherCoresHandleExcEnd(UINT32 currCpuid)
{
    while (1) {
        LOS_SpinLock(&g_excSerializerSpin);
        if ((g_currHandleExcCpuid == INVALID_CPUID) || (g_currHandleExcCpuid == currCpuid)) {
            g_currHandleExcCpuid = currCpuid;
            g_currHandleExcPID = OsCurrProcessGet()->processID;
            LOS_SpinUnlock(&g_excSerializerSpin);
            break;
        }

        if (g_nextExcWaitCpu == INVALID_CPUID) {
            g_nextExcWaitCpu = currCpuid;
        }
        LOS_SpinUnlock(&g_excSerializerSpin);
        LOS_Mdelay(EXC_WAIT_INTER);
    }
}
///检查所有CPU的状态
STATIC VOID OsCheckAllCpuStatus(VOID)
{
    UINT32 currCpuid = ArchCurrCpuid();
    UINT32 ret, target;

    OsCpuStatusSet(CPU_EXC);
    LOCKDEP_CLEAR_LOCKS();

    LOS_SpinLock(&g_excSerializerSpin);
    /* Only the current CPU anomaly */
    if (g_currHandleExcCpuid == INVALID_CPUID) {
        g_currHandleExcCpuid = currCpuid;
        g_currHandleExcPID = OsCurrProcessGet()->processID;
        LOS_SpinUnlock(&g_excSerializerSpin);
#ifndef LOSCFG_SAVE_EXCINFO
        if (g_excFromUserMode[currCpuid] == FALSE) {
            target = (UINT32)(OS_MP_CPU_ALL & ~CPUID_TO_AFFI_MASK(currCpuid));
            HalIrqSendIpi(target, LOS_MP_IPI_HALT);
        }
#endif
    } else if (g_excFromUserMode[currCpuid] == TRUE) {
        /* Both cores raise exceptions, and the current core is a user-mode exception.
         * Both cores are abnormal and come from the same process
         */
        if (OsCurrProcessGet()->processID == g_currHandleExcPID) {
            LOS_SpinUnlock(&g_excSerializerSpin);
            OsExcRestore();
            ret = LOS_TaskDelete(OsCurrTaskGet()->taskID);
            LOS_Panic("%s supend task :%u failed: 0x%x\n", __FUNCTION__, OsCurrTaskGet()->taskID, ret);
        }
        LOS_SpinUnlock(&g_excSerializerSpin);

        OsWaitOtherCoresHandleExcEnd(currCpuid);
    } else {
        if ((g_currHandleExcCpuid < LOSCFG_KERNEL_CORE_NUM) && (g_excFromUserMode[g_currHandleExcCpuid] == TRUE)) {
            g_currHandleExcCpuid = currCpuid;
            LOS_SpinUnlock(&g_excSerializerSpin);
            target = (UINT32)(OS_MP_CPU_ALL & ~CPUID_TO_AFFI_MASK(currCpuid));
            HalIrqSendIpi(target, LOS_MP_IPI_HALT);
        } else {
            LOS_SpinUnlock(&g_excSerializerSpin);
            while (1) {}
        }
    }
#ifndef LOSCFG_SAVE_EXCINFO
    /* use halt ipi to stop other active cores */
    if (g_excFromUserMode[ArchCurrCpuid()] == FALSE) {
        WaitAllCpuStop(currCpuid);
    }
#endif
}
#endif

STATIC VOID OsCheckCpuStatus(VOID)
{
#ifdef LOSCFG_KERNEL_SMP
    OsCheckAllCpuStatus();
#else
    g_currHandleExcCpuid = ArchCurrCpuid();
#endif
}
///执行期间的优先处理 excBufAddr为CPU异常上下文，
LITE_OS_SEC_TEXT VOID STATIC OsExcPriorDisposal(ExcContext *excBufAddr)
{
    if ((excBufAddr->regCPSR & CPSR_MASK_MODE) == CPSR_USER_MODE) { //用户模式下，访问地址不能出用户空间
        g_minAddr = USER_ASPACE_BASE;                    //可访问地址范围的开始地址，
        g_maxAddr = USER_ASPACE_BASE + USER_ASPACE_SIZE; //地址范围的结束地址，就是用户空间
        g_excFromUserMode[ArchCurrCpuid()] = TRUE;       //当前CPU执行切到用户态
    } else {
        g_minAddr = KERNEL_ASPACE_BASE;                      //可访问地址范围的开始地址
        g_maxAddr = KERNEL_ASPACE_BASE + KERNEL_ASPACE_SIZE; //可访问地址范围的结束地址
        g_excFromUserMode[ArchCurrCpuid()] = FALSE;          //当前CPU执行切到内核态
    }

    OsCheckCpuStatus();

#ifdef LOSCFG_KERNEL_SMP
#ifdef LOSCFG_FS_VFS
    /* Wait for the end of the Console task to avoid multicore printing code */
    OsWaitConsoleSendTaskPend(OsCurrTaskGet()->taskID); //等待控制台任务结束，以避免多核打印代码
#endif
#endif
}
///异常信息头部打印
LITE_OS_SEC_TEXT_INIT STATIC VOID OsPrintExcHead(UINT32 far)
{
#ifdef LOSCFG_BLACKBOX
#ifdef LOSCFG_SAVE_EXCINFO
    SetExcInfoIndex(0);
#endif
#endif
#ifdef LOSCFG_KERNEL_VM
    /* You are not allowed to add any other print information before this exception information */
    if (g_excFromUserMode[ArchCurrCpuid()] == TRUE) {
#ifdef LOSCFG_DEBUG_VERSION
        VADDR_T vaddr = ROUNDDOWN(far, PAGE_SIZE);
        LosVmSpace *space = LOS_SpaceGet(vaddr);
        if (space != NULL) {
            LOS_DumpMemRegion(vaddr);
        }
#endif
        PrintExcInfo("##################excFrom: User!####################\n"); //用户态异常信息说明
    } else
#endif
    {
        PrintExcInfo("##################excFrom: kernel!###################\n"); //内核态异常信息说明
    }
}

#ifdef LOSCFG_SAVE_EXCINFO
STATIC VOID OsSysStateSave(UINT32 *intCount, UINT32 *lockCount)
{
    *intCount = g_intCount[ArchCurrCpuid()];
    *lockCount = OsSchedLockCountGet();
    g_intCount[ArchCurrCpuid()] = 0;
    OsSchedLockSet(0);
}

STATIC VOID OsSysStateRestore(UINT32 intCount, UINT32 lockCount)
{
    g_intCount[ArchCurrCpuid()] = intCount;
    OsSchedLockSet(lockCount);
}
#endif

/*
 * Description : EXC handler entry
 * Input       : excType    --- exc type
 *               excBufAddr --- address of EXC buf
 */
LITE_OS_SEC_TEXT_INIT VOID OsExcHandleEntry(UINT32 excType, ExcContext *excBufAddr, UINT32 far, UINT32 fsr)
{ //异常信息处理入口
#ifdef LOSCFG_SAVE_EXCINFO
    UINT32 intCount;
    UINT32 lockCount;
#endif
    /* Task scheduling is not allowed during exception handling */
    OsSchedLock();

    g_curNestCount[ArchCurrCpuid()]++;

    OsExcPriorDisposal(excBufAddr);

    OsPrintExcHead(far); //打印异常信息头部信息 ##################excFrom: User!####################

#ifdef LOSCFG_KERNEL_SMP
    OsAllCpuStatusOutput(); //打印各CPU core的状态
#endif

#ifdef LOSCFG_SAVE_EXCINFO
    log_read_write_fn func = GetExcInfoRW(); //获取异常信息读写函数,用于打印异常信息栈
#endif

    if (g_excHook != NULL) {//全局异常钩子函数
        if (g_curNestCount[ArchCurrCpuid()] == 1) {//说明只有一个异常
#ifdef LOSCFG_SAVE_EXCINFO
            if (func != NULL) {
#ifndef LOSCFG_BLACKBOX
                SetExcInfoIndex(0);
#endif
                OsSysStateSave(&intCount, &lockCount);
                OsRecordExcInfoTime();
                OsSysStateRestore(intCount, lockCount);
            }
#endif
            g_excHook(excType, excBufAddr, far, fsr);
        } else { //说明出现了异常嵌套的情况
            OsCallStackInfo(); //打印栈内容
        }

#ifdef LOSCFG_SAVE_EXCINFO
        if (func != NULL) {
            PrintExcInfo("Be sure flash space bigger than GetExcInfoIndex():0x%x\n", GetExcInfoIndex());
            OsSysStateSave(&intCount, &lockCount);
            func(GetRecordAddr(), GetRecordSpace(), 0, GetExcInfoBuf());
            OsSysStateRestore(intCount, lockCount);
        }
#endif
    }

#ifdef LOSCFG_SHELL_CMD_DEBUG
    SystemRebootFunc rebootHook = OsGetRebootHook();
    if ((OsSystemExcIsReset() == TRUE) && (rebootHook != NULL)) {
        LOS_Mdelay(3000); /* 3000: System dead, delay 3 seconds after system restart */
        rebootHook();
    }
#endif

#ifdef LOSCFG_BLACKBOX
    BBoxNotifyError(EVENT_PANIC, MODULE_SYSTEM, "Crash in kernel", 1);
#endif
    while (1) {}
}

__attribute__((noinline)) VOID LOS_Panic(const CHAR *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    OsVprintf(fmt, ap, EXC_OUTPUT);
    va_end(ap);
    __asm__ __volatile__("swi 0"); //触发断异常
    while (1) {}
}

/* stack protector */
USED UINT32 __stack_chk_guard = 0xd00a0dff;

VOID __stack_chk_fail(VOID)
{
    /* __builtin_return_address is a builtin function, building in gcc */
    LOS_Panic("stack-protector: Kernel stack is corrupted in: %p\n",
              __builtin_return_address(0));
}

VOID LOS_RecordLR(UINTPTR *LR, UINT32 LRSize, UINT32 recordCount, UINT32 jumpCount)
{
    UINT32 count = 0;
    UINT32 index = 0;
    UINT32 stackStart, stackEnd;
    LosTaskCB *taskCB = NULL;
    UINTPTR framePtr, tmpFramePtr, linkReg;

    if (LR == NULL) {
        return;
    }
    /* if LR array is not enough,just record LRSize. */
    if (LRSize < recordCount) {
        recordCount = LRSize;
    }

    taskCB = OsCurrTaskGet();
    stackStart = taskCB->topOfStack;
    stackEnd = stackStart + taskCB->stackSize;

    framePtr = Get_Fp();
    while ((framePtr > stackStart) && (framePtr < stackEnd) && IS_ALIGNED(framePtr, sizeof(CHAR *))) {
        tmpFramePtr = framePtr;
#ifdef LOSCFG_COMPILER_CLANG_LLVM
        linkReg = *(UINTPTR *)(tmpFramePtr + sizeof(UINTPTR));
#else
        linkReg = *(UINTPTR *)framePtr;
#endif
        if (index >= jumpCount) {
            LR[count++] = linkReg;
            if (count == recordCount) {
                break;
            }
        }
        index++;
#ifdef LOSCFG_COMPILER_CLANG_LLVM
        framePtr = *(UINTPTR *)framePtr;
#else
        framePtr = *(UINTPTR *)(tmpFramePtr - sizeof(UINTPTR));
#endif
    }

    /* if linkReg is not enough,clean up the last of the effective LR as the end. */
    if (count < recordCount) {
        LR[count] = 0;
    }
}
