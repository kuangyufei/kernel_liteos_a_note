[鸿蒙内核源码注释中文版 【 Gitee仓 ](https://gitee.com/weharmony/kernel_liteos_a_note) | [ CSDN仓 ](https://codechina.csdn.net/kuangyufei/kernel_liteos_a_note) | [ Github仓 ](https://github.com/kuangyufei/kernel_liteos_a_note) | [ Coding仓 】](https://weharmony.coding.net/public/harmony/kernel_liteos_a_note/git/files) 项目中文注解鸿蒙官方内核源码,图文并茂,详细阐述鸿蒙架构和代码设计细节.每个码农,学职生涯,都应精读一遍内核源码.精读内核源码最大的好处是:将孤立知识点织成一张高浓度,高密度底层网,对计算机底层体系化理解形成永久记忆,从此高屋建瓴分析/解决问题.

[鸿蒙源码分析系列篇 【 CSDN ](https://blog.csdn.net/kuangyufei/article/details/108727970) [| OSCHINA ](https://my.oschina.net/u/3751245/blog/4626852) [| WIKI 】](https://gitee.com/weharmony/kernel_liteos_a_note/wikis/pages) 从 HarmonyOS 架构层视角整理成文, 并首创用生活场景讲故事的方式试图去解构内核，一窥究竟。

---

## **[kernel\_liteos\_a_note](https://gitee.com/weharmony/kernel_liteos_a_note): 鸿蒙内核源码注释中文版 -> 点击目录和文件查看源码的详细中文注解**  

---

- [kernel_liteos_a_note](https://gitee.com/weharmony/kernel_liteos_a_note/)
  * [kernel](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/)
    + [base](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/)
    	+ [core](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/core/)
    		+ [los_bitmap.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/core/los_bitmap.c) -> []() -> 位图管理器,按位管理变量,例如:进程调度位图(process->threadScheduleMap),页面标识(page->flag)等
    		+ [los_process.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/core/los_process.c) -> [鸿蒙内核源码分析(进程管理篇)](https://blog.csdn.net/kuangyufei/article/details/108595941) -> 进程是内核的资源管理单元,负责管理 多任务,虚拟空间,文件描述符 == 
    		+ [los_sortlink.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/core/los_sortlink.c) -> []() -> 排序链表的实现,通常用于对定时任务和线程的排序
    		+ [los_swtmr.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/core/los_swtmr.c) -> []() -> 软时钟的实现,每个CPU都创建了一个专用于跑软件定时器的任务
    		+ [los_sys.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/core/los_sys.c) -> []() ->
    		+ [los_task.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/core/los_task.c) -> [鸿蒙内核源码分析(Task管理篇)](https://blog.csdn.net/kuangyufei/article/details/108661248) -> Task是内核调度的单元,等同于posix的线程(thread)概念
    		+ [los_tick.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/core/los_tick.c) -> []() ->
    		+ [los_timeslice.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/core/los_timeslice.c) -> []() ->
    	+ [ipc](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/ipc/) -> []() ->
    		+ [los_event.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/ipc/base/los_event.c) -> []() ->
    		+ [los_futex.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/ipc/base/los_futex.c) -> []() ->
    		+ [los_ipcdebug.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/ipc/base/los_ipcdebug.c) -> []() ->
    		+ [los_mux.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/ipc/base/los_mux.c) -> []() ->
    		+ [los_queue.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/ipc/base/los_queue.c) -> []() ->
    		+ [los_queue_debug.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/ipc/base/los_queue_debug.c) -> []() ->
    		+ [los_sem.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/ipc/base/los_sem.c) -> []() ->
    		+ [los_sem_debug.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/ipc/base/los_sem_debug.c) -> []() ->
    		+ [los_signal.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/ipc/base/) -> []() ->
        + [mem](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/mem/los_signal.c) -> []() ->
    	+ [misc](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/misc/) -> []() ->
    		+ [kill_shellcmd.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/misc/kill_shellcmd.c) -> []() ->
            + [los_misc.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/misc/los_misc.c) -> []() ->
            + [los_stackinfo.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/misc/los_stackinfo.c) -> []() ->
            + [mempt_shellcmd.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/misc/mempt_shellcmd.c) -> []() ->
            + [swtmr_shellcmd.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/misc/swtmr_shellcmd.c) -> []() ->
            + [sysinfo_shellcmd.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/misc/sysinfo_shellcmd.c) -> []() ->
            + [task_shellcmd.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/misc/task_shellcmd.c) -> []() ->
            + [vm_shellcmd.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/misc/vm_shellcmd.c) -> []() ->
        + [mp](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/mp/) -> []() ->
            + [los_lockdep.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/mp/los_lockdep.c) -> []() ->
            + [los_mp.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/mp/los_mp.c) -> []() ->
            + [los_percpu.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/mp/los_percpu.c) -> []() ->
            + [los_stat.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/mp/los_stat.c) -> []() ->
        + [om](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/om/) -> []() ->
            + [los_err.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/om/los_err.c) -> []() ->
        + [sched/sched_sq](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/sched/sched_sq/) -> []() ->
            + [los_priqueue.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/sched/sched_sq/los_priqueue.c) -> []() ->
            + [los_sched.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/sched/sched_sq/los_sched.c) -> []() ->
        + [vm](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/vm/) -> []() ->
            + [los_vm_boot.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/vm/los_vm_boot.c) -> []() ->
            + [los_vm_dump.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/vm/los_vm_dump.c) -> []() ->
            + [los_vm_fault.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/vm/los_vm_fault.c) -> []() ->
            + [los_vm_filemap.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/vm/los_vm_filemap.c) -> []() ->
            + [los_vm_iomap.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/vm/los_vm_iomap.c) -> []() ->
            + [los_vm_map.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/vm/los_vm_map.c) -> []() ->
            + [los_vm_page.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/vm/los_vm_page.c) -> []() ->
            + [los_vm_phys.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/vm/los_vm_phys.c) -> []() ->
            + [los_vm_scan.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/vm/los_vm_scan.c) -> []() ->
            + [los_vm_syscall.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/vm/los_vm_syscall.c) -> []() ->
            + [oom.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/vm/oom.c) -> []() ->
            + [shm.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/base/vm/shm.c) -> []() ->
    + [common](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/common/) -> []() ->
        + [console.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/common/console.c) -> []() ->
        + [hwi_shell.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/common/hwi_shell.c) -> []() ->
        + [los_cir_buf.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/common/los_cir_buf.c) -> []() ->
        + [los_config.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/common/los_config.c) -> []() ->
        + [los_exc_interaction.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/common/los_exc_interaction.c) -> []() ->
        + [los_excinfo.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/common/los_excinfo.c) -> []() ->
        + [los_hilog.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/common/los_hilog.c) -> []() ->
        + [los_magickey.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/common/los_magickey.c) -> []() ->
        + [los_printf.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/common/los_printf.c) -> []() ->
        + [los_rootfs.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/common/los_rootfs.c) -> []() ->
        + [los_seq_buf.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/common/los_seq_buf.c) -> []() ->
        + [virtual_serial.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/common/virtual_serial.c) -> []() ->
    + [extended](https://gitee.com/weharmony/kernel_liteos_a_note/kernel/extended/tree/master/) -> []() ->
        + [cppsupport](https://gitee.com/weharmony/kernel_liteos_a_note/kernel/extended/tree/master/cppsupport/) -> []() ->
            + [los_cppsupport.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/extended/cppsupport/los_cppsupport.c) -> []() ->
        + [cpup](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/extended/cpup/) -> []() ->
            + [cpup_shellcmd.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/extended/cpup/cpup_shellcmd.c) -> []() ->
            + [los_cpup.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/extended/cpup/los_cpup.c) -> []() ->
        + [dynload/src](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/extended/dynload/src/) []() ->
            + [los_exec_elf.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/extended/dynload/src/los_exec_elf.c) -> []() ->
            + [los_load_elf.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/extended/dynload/src/los_load_elf.c) -> []() ->    
        + [liteipc](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/extended/liteipc/) -> []() ->
            + [hm_liteipc.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/extended/liteipc/hm_liteipc.c) -> []() ->
        + [tickless](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/extended/tickless/) -> []() ->
            + [los_tickless.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/extended/tickless/los_tickless.c) -> []() ->
        + [trace](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/extended/trace/) -> []() ->
            + [los_trace.c](https://gitee.com/weharmony/kernel_liteos_a_notetree/master/kernel/extended/los_trace.c) -> []() ->
        + [vdso](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/extended/vdso/) -> []() ->
            + [src](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/extended/vdso/src/) -> []() ->
                + [los_vdso.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/extended/vdso/src/los_vdso.c) -> []() ->
                + [los_vdso_text.S](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/extended/vdso/src/los_vdso_text.S) -> []() ->
            + [usr](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/extended/vdso/usr/) -> []() ->
                + [los_vdso_sys.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/extended/vdso/usr/los_vdso_sys.c) -> []() ->      
    + [user/src](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/user/src/) -> []() ->
    	+ [los_user_init.c](https://gitee.com/weharmony/kernel_liteos_a_note/tree/master/kernel/user/src/los_user_init.c) -> []() ->







---

系列篇文章 进入 >\> [鸿蒙系统源码分析(总目录) 【 CSDN](https://blog.csdn.net/kuangyufei/article/details/108727970) | [OSCHINA](https://my.oschina.net/u/3751245/blog/4626852) [| WIKI 】](https://gitee.com/weharmony/kernel_liteos_a_note/wikis/pages)查看
    
 注释中文版 进入 >\> [鸿蒙内核源码注释中文版 【 Gitee仓](https://gitee.com/weharmony/kernel_liteos_a_note) | [CSDN仓](https://codechina.csdn.net/kuangyufei/kernel_liteos_a_note) | [Github仓](https://github.com/kuangyufei/kernel_liteos_a_note) | [Coding仓 】](https://weharmony.coding.net/public/harmony/kernel_liteos_a_note/git/files)阅读
    