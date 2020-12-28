[鸿蒙内核源码注释中文版 【 Gitee仓 ](https://gitee.com/weharmony/kernel_liteos_a_note) | [ CSDN仓 ](https://codechina.csdn.net/kuangyufei/kernel_liteos_a_note) | [ Github仓 ](https://github.com/kuangyufei/kernel_liteos_a_note) | [ Coding仓 】](https://weharmony.coding.net/public/harmony/kernel_liteos_a_note/git/files) 项目中文注解鸿蒙官方内核源码,图文并茂,详细阐述鸿蒙架构和代码设计细节.每个码农,学职生涯,都应精读一遍内核源码.精读内核源码最大的好处是:将孤立知识点织成一张高浓度,高密度底层网,对计算机底层体系化理解形成永久记忆,从此高屋建瓴分析/解决问题.

[鸿蒙源码分析系列篇 【 CSDN ](https://blog.csdn.net/kuangyufei/article/details/108727970) [| OSCHINA ](https://my.oschina.net/u/3751245/blog/4626852) [| WIKI 】](https://gitee.com/weharmony/kernel_liteos_a_note/wikis/pages) 从 HarmonyOS 架构层视角整理成文, 并首创用生活场景讲故事的方式试图去解构内核，一窥究竟。

---

- kernel_liteos_a_note
  * kernel
    + base
    	+ core
    		+ los_bitmap.c
    		+ los_process.c
    		+ los_sortlink.c
    		+ los_swtmr.c
    		+ los_sys.c
    		+ los_task.c
    		+ los_tick.c
    		+ los_timeslice.c
    	+ ipc
    		+ los_event.c
    		+ los_futex.c
    		+ los_ipcdebug.c
    		+ los_mux.c
    		+ los_queue.c
    		+ los_queue_debug.c
    		+ los_sem.c
    		+ los_sem_debug.c
    		+ los_signal.c
        + mem
    	+ misc
    		+ kill_shellcmd.c
            + los_misc.c
            + los_stackinfo.c
            + mempt_shellcmd.c
            + swtmr_shellcmd.c
            + sysinfo_shellcmd.c
            + task_shellcmd.c
            + vm_shellcmd.c
        + mp
            + los_lockdep.c
            + los_mp.c
            + los_percpu.c
            + los_stat.c
        + om
            + los_err.c
        + sched\sched_sq
            + los_priqueue.c
            + los_sched.c
        + vm
            + los_vm_boot.c
            + los_vm_dump.c
            + los_vm_fault.c
            + los_vm_filemap.c
            + los_vm_iomap.c
            + los_vm_map.c
            + los_vm_page.c
            + los_vm_phys.c
            + los_vm_scan.c
            + los_vm_syscall.c
            + oom.c
            + shm.c
    + common
        + console.c
        + hwi_shell.c
        + los_cir_buf.c
        + los_config.c
        + los_exc_interaction.c
        + los_excinfo.c
        + los_hilog.c
        + los_magickey.c
        + los_printf.c
        + los_rootfs.c
        + los_seq_buf.c
        + virtual_serial.c
    + extended
        + cppsupport
            + los_cppsupport.c
        + cpup
            + cpup_shellcmd.c
            + los_cpup.c
        + dynload\src
            + los_exec_elf.c
            + los_load_elf.c           
        + liteipc
            + hm_liteipc.c
        + tickless
            + los_tickless.c
        + trace
            + los_trace.c
        + vdso
            + src
                + los_vdso.c
                + los_vdso_text.S
            + usr 
                + los_vdso_sys.c      
    + user\src
    	+ los_user_init.c







---

系列篇文章 进入 >\> [鸿蒙系统源码分析(总目录) 【 CSDN](https://blog.csdn.net/kuangyufei/article/details/108727970) | [OSCHINA](https://my.oschina.net/u/3751245/blog/4626852) [| WIKI 】](https://gitee.com/weharmony/kernel_liteos_a_note/wikis/pages)查看
    
 注释中文版 进入 >\> [鸿蒙内核源码注释中文版 【 Gitee仓](https://gitee.com/weharmony/kernel_liteos_a_note) | [CSDN仓](https://codechina.csdn.net/kuangyufei/kernel_liteos_a_note) | [Github仓](https://github.com/kuangyufei/kernel_liteos_a_note) | [Coding仓 】](https://weharmony.coding.net/public/harmony/kernel_liteos_a_note/git/files)阅读
    