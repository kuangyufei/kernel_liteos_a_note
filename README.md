[![在这里插入图片描述](https://gitee.com/weharmony/docs/raw/master/pic/other/io.png)](http://weharmonyos.com/)

百篇博客系列篇.本篇为:

* [v13.xx 鸿蒙内核源码分析(源码注释篇) | 鸿蒙必定成功，也必然成功 ](https://my.oschina.net/u/3751245/blog/4686747) **[  | 51](https://harmonyos.51cto.com/posts/4049)[ .c](https://blog.csdn.net/kuangyufei/article/details/109251754)[  .h](http://weharmonyos.com/13_源码注释篇.html) [  .o](https://my.oschina.net/weharmony)**


### 几点说明

* **[kernel_liteos_a_note](https://gitee.com/weharmony/kernel_liteos_a_note) | 中文注解鸿蒙内核**  是在 `OpenHarmony` 的 [kernel_liteos_a](https://gitee.com/openharmony/kernel_liteos_a) 基础上给内核源码加上中文注解的版本.与官方源码按月保持同步,同步历史如下:
  * `2021/8/19` -- 各目录增加了`BUILD.gn`文件,文件系统部分文件调整 
  * `2021/7/15` -- 改动不大,新增`blackbox`,`hidumper`,对一些宏规范化使用 
  * `2021/6/27` -- 对文件系统/设备驱动改动较大,目录结构进行了重新整理
  * `2021/6/08` -- 对编译构建,任务,信号模块有较大的改动
  * `2021/5/28` -- 改动不大,主要针对一些错误单词拼写纠正
  * `2021/5/13` -- 对系统调用，任务切换，信号处理，异常接管，文件管理，shell做了较大更新，代码结构更清晰
  * `2021/4/21` -- 官方优化了很多之前吐槽的地方，点赞
  * `2020/9/16` -- 中文注解版起点

* **[http://weharmonyos.com](http://weharmonyos.com) | 鸿蒙研究站**
  
  分成三部分内容
  *  [OpenHarmony开发者文档](http://weharmonyos.com/openharmony) 是对官方文档 [docs](https://gitee.com/openharmony/docs) 做的非常炫酷的静态站点，支持侧边栏/面包屑/搜索/中英文，非常方便的查看官方文档，大大提高学习和开发效率
  *  [百篇博客分析鸿蒙内核](http://weharmonyos.com/weharmony) 是对内核源码注解过程中整理出来的内容输出. 
  *  整理的一些分析内核的工具和书籍 如: [鸿蒙源码分析.离线文档](http://weharmonyos.com/history.html) , [GNU汇编](http://weharmonyos.com/book/assembly.html), [gn参考手册](http://weharmonyos.com/gn/docs/)
  
* **子系统注解仓库**
  
  在给鸿蒙内核源码加注过程中发现仅仅注解内核仓库还不够,因为它关联了其他子系统,若对这些子系统不了解是很难完整的注解鸿蒙内核,所以也对这些关联仓库进行了部分注解,这些仓库包括:
  * **[编译构建子系统 | build_lite](https://gitee.com/weharmony/build_lite_note)**  
  * **[协议栈 | lwip](https://gitee.com/weharmony/third_party_lwip)** 
  * **[文件系统 | NuttX](https://gitee.com/weharmony/third_party_NuttX)** 
  * **[标准库 | musl](https://gitee.com/weharmony/third_party_musl)** 


      
### **为何要精读内核源码?**
* 码农的学职生涯，都应精读一遍内核源码。以浇筑好计算机知识大厦的地基，地基纵深的坚固程度，很大程度能决定未来大厦能盖多高。那为何一定要精读细品呢?
* 因为内核代码本身并不太多，都是浓缩的精华，精读是让各个知识点高频出现，不孤立成点状记忆，没有足够连接点的知识点是很容易忘的，点点成线，线面成体，连接越多，记得越牢，如此短时间内容易结成一张高浓度，高密度的系统化知识网，训练大脑肌肉记忆，驻入大脑直觉区，想抹都抹不掉，终生携带，随时调取。跟骑单车一样，一旦学会，即便多年不骑，照样跨上就走，游刃有余。
### **热爱是所有的理由和答案**
* 因大学时阅读 `linux 2.6` 内核痛并快乐的经历，一直有个心愿，如何让更多对内核感兴趣的朋友减少阅读时间，加速对计算机系统级的理解，而不至于过早的放弃。但因过程种种，多年一直没有行动，基本要放弃这件事了。恰逢 **2020/9/10** 鸿蒙正式开源，重新激活了多年的心愿，就有那么点如黄河之水一发不可收拾了。 
* 目前对内核源码的注解已完成了 **70%** ，对内核源码的博客分析已完成了**65+篇**， 每天都很充实，很兴奋，连做梦内核代码都在鱼贯而入。如此疯狂地做一件事还是当年谈恋爱的时候， 只因热爱， 热爱是所有的理由和答案。 :P 
### **(〃･ิ‿･ิ)ゞ鸿蒙内核开发者**
* 感谢开放原子开源基金会，致敬鸿蒙内核开发者提供了如此优秀的源码，一了多年的夙愿，津津乐道于此。精读内核源码加注并整理成档是件很有挑战的事，时间上要以月甚至年为单位，但正因为很难才值得去做! 干困难事，方有所得;专注聚焦，必有所获。 
* 从内核一行行的代码中能深深感受到开发者各中艰辛与坚持，及鸿蒙生态对未来的价值，这些是张嘴就来的网络喷子们永远不能体会到的。可以毫不夸张的说鸿蒙内核源码可作为大学 **C语言**，**数据结构**，**操作系统**，**汇编语言**，**计算机系统结构**，**计算机组成原理** 六门课程的教学项目。如此宝库，不深入研究实在是暴殄天物，于心不忍，注者坚信鸿蒙大势所趋，未来可期，其必定成功，也必然成功，誓做其坚定的追随者和传播者。
### **理解内核的三个层级**
* **普通概念映射级:** 这一级不涉及专业知识，用大众所熟知的公共认知就能听明白是个什么概念，也就是说用一个普通人都懂的概念去诠释或者映射一个他们从没听过的概念。让陌生的知识点与大脑中烂熟于心的知识点建立多重链接，加深记忆。说别人能听得懂的话这很重要!!! 一个没学过计算机知识的卖菜大妈就不可能知道内核的基本运作了吗? 不一定!在系列篇中试图用 **[鸿蒙内核源码分析(总目录)之故事篇](https://my.oschina.net/weharmony)** 去引导这一层级的认知，希望能卷入更多的人来关注基础软件，尤其是那些资本大鳄，加大对基础软件的投入。
* **专业概念抽象级:** 对抽象的专业逻辑概念具体化认知， 比如虚拟内存，老百姓是听不懂的，学过计算机的人都懂，具体怎么实现的很多人又都不懂了，但这并不妨碍成为一个优秀的上层应用开发者，因为虚拟内存已经被抽象出来，目的是要屏蔽上层对它具体实现的认知。试图用 **[鸿蒙内核源码分析(总目录)百篇博客](https://my.oschina.net/weharmony)** 去拆解那些已经被抽象出来的专业概念， 希望能卷入更多对内核感兴趣的应用软件人才流入基础软硬件生态， 应用软件咱们是无敌宇宙，但基础软件却很薄弱。
* **具体微观代码级:** 这一级是具体到每一行代码的实现，到了用代码指令级的地步，这段代码是什么意思?为什么要这么设计?有没有更好的方案? **[鸿蒙内核源码注解分析](https://gitee.com/weharmony/kernel_liteos_a_note)** 试图从细微处去解释代码实现层，英文真的是天生适合设计成编程语言的人类语言，计算机的01码映射到人类世界的26个字母，诞生了太多的伟大奇迹。但我们的母语注定了很大部分人存在着自然语言层级的理解映射，希望鸿蒙内核源码注解分析能让更多爱好者快速的理解内核，共同进步。
### **加注方式是怎样的？**

*  因鸿蒙内核6W+代码量，本身只有较少的注释， 中文注解以不对原有代码侵入为前提，源码中所有英文部分都是原有注释，所有中文部分都是中文版的注释，同时为方便同步官方版本的更新，尽量不去增加代码的行数，不破坏文件的结构，注释多类似以下的方式:

    在重要模块的.c/.h文件开始位置先对模块功能做整体的介绍，例如异常接管模块注解如图所示:

    ![在这里插入图片描述](https://gitee.com/weharmony/docs/raw/master/pic/other/ycjg.png)

    注解过程中查阅了很多的资料和书籍，在具体代码处都附上了参考链接。

* 而函数级注解会详细到重点行，甚至每一行， 例如申请互斥锁的主体函数，不可谓不重要，而官方注释仅有一行，如图所示

    ![在这里插入图片描述](https://gitee.com/weharmony/docs/raw/master/pic/other/sop.png)

* 另外画了一些字符图方便理解，直接嵌入到头文件中，比如虚拟内存的全景图，因没有这些图是很难理解虚拟内存是如何管理的。

    ![在这里插入图片描述](https://gitee.com/weharmony/docs/raw/master/pic/other/vm.png)
### **有哪些特殊的记号**
* 搜索 **`@note_pic`** 可查看绘制的全部字符图
* 搜索 **`@note_why`** 是尚未看明白的地方，有看明白的，请Pull Request完善
* 搜索 **`@note_thinking`** 是一些的思考和建议
* 搜索 **`@note_#if0`** 是由第三方项目提供不在内核源码中定义的极为重要结构体，为方便理解而添加的。
* 搜索 **`@note_good`** 是给源码点赞的地方
### 目录结构
```
/kernel/liteos_a
├── apps                   # 用户态的init和shell应用程序
├── arch                   # 体系架构的目录，如arm等
│   └── arm                # arm架构代码
├── bsd                    # freebsd相关的驱动和适配层模块代码引入，例如USB等
├── compat                 # 内核接口兼容性目录
│   └── posix              # posix相关接口
├── drivers                # 内核驱动
│   └── char               # 字符设备
│       ├── mem            # 访问物理IO设备驱动
│       ├── quickstart     # 系统快速启动接口目录
│       ├── random         # 随机数设备驱动
│       └── video          # framebuffer驱动框架
├── fs                     # 文件系统模块，主要来源于NuttX开源项目
│   ├── fat                # fat文件系统
│   ├── jffs2              # jffs2文件系统
│   ├── include            # 对外暴露头文件存放目录
│   ├── nfs                # nfs文件系统
│   ├── proc               # proc文件系统
│   ├── ramfs              # ramfs文件系统
│   └── vfs                # vfs层
├── kernel                 # 进程、内存、IPC等模块
│   ├── base               # 基础内核，包括调度、内存等模块
│   ├── common             # 内核通用组件
│   ├── extended           # 扩展内核，包括动态加载、vdso、liteipc等模块
│   ├── include            # 对外暴露头文件存放目录
│   └── user               # 加载init进程
├── lib                    # 内核的lib库
├── net                    # 网络模块，主要来源于lwip开源项目
├── platform               # 支持不同的芯片平台代码，如Hi3516DV300等
│   ├── hw                 # 时钟与中断相关逻辑代码
│   ├── include            # 对外暴露头文件存放目录
│   └── uart               # 串口相关逻辑代码
├── platform               # 支持不同的芯片平台代码，如Hi3516DV300等
├── security               # 安全特性相关的代码，包括进程权限管理和虚拟id映射管理
├── syscall                # 系统调用
├── tools                  # 构建工具及相关配置和代码
└── zzz                    # 中文加注版比官方版无新增文件，只多了一个zzz的目录，里面放了一些图片/文件/工具，
                           # 它与内核代码无关，大家可以忽略它，取名zzz是为了排在最后，减少对原有代码目录级的侵入，
                           # zzz 的想法源于微信中名称为AAA的那帮朋友，你的微信里应该也有他们熟悉的身影吧 :|P
```

### **鸿蒙内核源码分析.总目录**

[v08.xx 鸿蒙内核源码分析(总目录) | 百万汉字注解 百篇博客分析 ](https://my.oschina.net/weharmony/blog/4626852) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108727970)[  .h](http://weharmonyos.com/blog/08_总目录.html) [  .o](https://my.oschina.net/weharmony)**

### 百万汉字注解.百篇博客分析
[百万汉字注解 >> 精读鸿蒙源码，中文注解分析， 深挖地基工程，大脑永久记忆，四大码仓每日同步更新](https://gitee.com/weharmony/kernel_liteos_a_note)[< gitee ](https://gitee.com/weharmony/kernel_liteos_a_note)[| github ](https://github.com/kuangyufei/kernel_liteos_a_note)[| csdn ](https://codechina.csdn.net/kuangyufei/kernel_liteos_a_note)[| coding >](https://weharmony.coding.net/public/harmony/kernel_liteos_a_note/git/files)

[百篇博客分析 >> 故事说内核，问答式导读，生活式比喻，表格化说明，图形化展示，主流站点定期更新中](http://weharmonyos.com)[< 51cto  ](https://harmonyos.51cto.com/column/34)[| csdn ](https://blog.csdn.net/kuangyufei)[| harmony ](http://weharmonyos.com/)[ | osc >](https://my.oschina.net/weharmony)

### 关注不迷路.代码即人生
[![鸿蒙内核源码分析](https://gitee.com/weharmony/docs/raw/master/pic/other/so1so.png)](https://gitee.com/weharmony/docs/raw/master/pic/other/so1so.png)

**QQ群:790015635 | 入群请填: 666**

原创不易，欢迎转载，但请注明出处.