[![在这里插入图片描述](https://gitee.com/weharmony/docs/raw/master/pic/other/io.png)](http://weharmonyos.com/)

百篇博客系列篇.本篇为:

* [v13.xx 鸿蒙内核源码分析(源码注释篇) | 鸿蒙必定成功，也必然成功 ](https://my.oschina.net/u/3751245/blog/4686747) **[  | 51](https://harmonyos.51cto.com/posts/4049)[ .c](https://blog.csdn.net/kuangyufei/article/details/109251754)[  .h](http://weharmonyos.com/13_源码注释篇.html) [  .o](https://my.oschina.net/weharmony)**


### 几点说明

* **[kernel_liteos_a_note](https://gitee.com/weharmony/kernel_liteos_a_note) | 中文注解鸿蒙内核** 

  是在 `OpenHarmony` 的 [kernel_liteos_a](https://gitee.com/openharmony/kernel_liteos_a) 基础上给内核源码加上中文注解的版本.加注版与官方源码按月保持同步,同步历史如下:
  * `2021/7/30` -- 主要对各目录增加了`BUILD.gn`文件 
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
* 到 **2021/3/10** 刚好半年， 对内核源码的注解已完成了 **70%** ，对内核源码的博客分析已完成了**40篇**， 每天都很充实，很兴奋，连做梦内核代码都在鱼贯而入。如此疯狂地做一件事还是当年谈恋爱的时候， 只因热爱， 热爱是所有的理由和答案。 :P 
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

### **百篇博客.往期回顾**
在加注过程中，整理出以下文章。内容立足源码，常以生活场景打比方尽可能多的将内核知识点置入某种场景，具有画面感，容易理解记忆。说别人能听得懂的话很重要! 百篇博客绝不是百度教条式的在说一堆诘屈聱牙的概念，那没什么意思。更希望让内核变得栩栩如生，倍感亲切.确实有难度，自不量力，但已经出发，回头已是不可能的了。　:P

与代码有bug需不断debug一样，文章和注解内容会存在不少错漏之处，请多包涵，但会反复修正，持续更新，`.xx`代表修改的次数，精雕细琢，言简意赅，力求打造精品内容。

* [v62.xx 鸿蒙内核源码分析(文件概念篇) | 为什么说一切皆是文件 ](https://my.oschina.net/weharmony/blog/5152858) **[  | 51](https://harmonyos.51cto.com/posts/7460)[ .c](https://blog.csdn.net/kuangyufei/article/details/119217155)[  .h](http://weharmonyos.com/blog/62_文件概念篇.html)[ .o](https://my.oschina.net/weharmony)**

* [v61.xx 鸿蒙内核源码分析(忍者ninja篇) | 都忍者了能不快吗 ](https://my.oschina.net/weharmony/blog/5139034) **[  | 51](https://harmonyos.51cto.com/posts/7328)[ .c](https://blog.csdn.net/kuangyufei/article/details/118970589)[  .h](http://weharmonyos.com/blog/61_忍者ninja篇.html)[ .o](https://my.oschina.net/weharmony)**

* [v60.xx 鸿蒙内核源码分析(gn应用篇) | gn语法及在鸿蒙的使用 ](https://my.oschina.net/weharmony/blog/5137565) **[  | 51](https://harmonyos.51cto.com/posts/7310)[ .c](https://blog.csdn.net/kuangyufei/article/details/118932416)[  .h](http://weharmonyos.com/blog/60_gn应用篇.html)[ .o](https://my.oschina.net/weharmony)**

* [v59.xx 鸿蒙内核源码分析(构建工具篇) | 顺瓜摸藤调试鸿蒙构建过程 ](https://my.oschina.net/weharmony/blog/5135157) **[  | 51](https://harmonyos.51cto.com/posts/7287)[ .c](https://blog.csdn.net/kuangyufei/article/details/118878233)[  .h](http://weharmonyos.com/blog/59_构建工具篇.html)[ .o](https://my.oschina.net/weharmony)**

* [v58.xx 鸿蒙内核源码分析(环境脚本篇) | 编译鸿蒙原来如此简单 ](https://my.oschina.net/weharmony/blog/5132725) **[  | 51](https://harmonyos.51cto.com/posts/7248)[ .c](https://blog.csdn.net/kuangyufei/article/details/118765692)[  .h](http://weharmonyos.com/blog/58_编译脚本篇.html)[ .o](https://my.oschina.net/weharmony)**

* [v57.xx 鸿蒙内核源码分析(编译过程篇) | 简单案例窥视GCC编译全过程 ](https://my.oschina.net/weharmony/blog/5064209) **[  | 51](https://harmonyos.51cto.com/posts/5032)[ .c](https://blog.csdn.net/kuangyufei/article/details/117419679)[  .h](http://weharmonyos.com/blog/57_编译过程篇.html)[ .o](https://my.oschina.net/weharmony)**
  
* [v56.xx 鸿蒙内核源码分析(进程映像篇) | ELF是如何被加载运行的? ](https://my.oschina.net/weharmony/blog/5060359) **[  | 51](https://harmonyos.51cto.com/posts/4815)[ .c](https://blog.csdn.net/kuangyufei/article/details/117325933)[  .h](http://weharmonyos.com/blog/56_进程映像篇.html)[ .o](https://my.oschina.net/weharmony)**

* [v55.xx 鸿蒙内核源码分析(重定位篇) | 与国际接轨的对外部发言人 ](https://my.oschina.net/weharmony/blog/5055124) **[  | 51](https://harmonyos.51cto.com/posts/4519)[ .c](https://blog.csdn.net/kuangyufei/article/details/117110422)[  .h](http://weharmonyos.com/blog/55_重定位篇.html)[  .o](https://my.oschina.net/weharmony)**
  
* [v54.xx 鸿蒙内核源码分析(静态链接篇) | 完整小项目看透静态链接过程 ](https://my.oschina.net/weharmony/blog/5049918) **[  | 51](https://harmonyos.51cto.com/posts/4430)[ .c](https://blog.csdn.net/kuangyufei/article/details/116835578)[  .h](http://weharmonyos.com/blog/54_静态链接篇.html)[  .o](https://my.oschina.net/weharmony)**

* [v53.xx 鸿蒙内核源码分析(ELF解析篇) | 你要忘了她姐俩你就不是银 ](https://my.oschina.net/weharmony/blog/5048746) **[  | 51](https://harmonyos.51cto.com/posts/4413)[ .c](https://blog.csdn.net/kuangyufei/article/details/116781446)[  .h](http://weharmonyos.com/blog/53_ELF解析篇.html)[  .o](https://my.oschina.net/weharmony)**
  
* [v52.xx 鸿蒙内核源码分析(静态站点篇) | 五一哪也没去就干了这事 ](https://my.oschina.net/weharmony/blog/5042657) **[  | 51](https://harmonyos.51cto.com/posts/4312)[ .c](https://blog.csdn.net/kuangyufei/article/details/116517461)[  .h](http://weharmonyos.com/blog/52_静态站点篇.html)[  .o](https://my.oschina.net/weharmony)**
    
* [v51.xx 鸿蒙内核源码分析(ELF格式篇) | 应用程序入口并不是main ](https://my.oschina.net/weharmony/blog/5030288) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/116097977)[  .h](http://weharmonyos.com/blog/51_ELF格式篇.html)[  .o](https://my.oschina.net/weharmony)** 
  
* [v50.xx 鸿蒙内核源码分析(编译环境篇) | docker编译鸿蒙真的很香 ](https://my.oschina.net/weharmony/blog/5028613) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/116042551)[  .h](http://weharmonyos.com/blog/50_编译环境篇.html) [  .o](https://my.oschina.net/weharmony)** 
  
* [v49.xx 鸿蒙内核源码分析(信号消费篇) | 谁让CPU连续四次换栈运行 ](https://my.oschina.net/weharmony/blog/5027224) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/115958293)[  .h](http://weharmonyos.com/blog/49_信号消费篇.html) [  .o](https://my.oschina.net/weharmony)** 

* [v48.xx 鸿蒙内核源码分析(信号生产篇) | 年过半百，依然活力十足 ](https://my.oschina.net/weharmony/blog/5022149) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/115768099)[  .h](http://weharmonyos.com/blog/48_信号生产篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v47.xx 鸿蒙内核源码分析(进程回收篇) | 临终前如何向老祖宗托孤 ](https://my.oschina.net/weharmony/blog/5017716) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/115672752)[  .h](http://weharmonyos.com/blog/47_进程回收篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v46.xx 鸿蒙内核源码分析(特殊进程篇) | 龙生龙凤生凤老鼠生儿会打洞 ](https://my.oschina.net/weharmony/blog/5014444) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/115556505)[  .h](http://weharmonyos.com/blog/46_特殊进程篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v45.xx 鸿蒙内核源码分析(Fork篇) | 一次调用，两次返回 ](https://my.oschina.net/weharmony/blog/5010301) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/115467961)[  .h](http://weharmonyos.com/blog/45_Fork篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v44.xx 鸿蒙内核源码分析(中断管理篇) | 江湖从此不再怕中断 ](https://my.oschina.net/weharmony/blog/4995800) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/115130055)[  .h](http://weharmonyos.com/blog/44_中断管理篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v43.xx 鸿蒙内核源码分析(中断概念篇) | 海公公的日常工作 ](https://my.oschina.net/weharmony/blog/4992750) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/115014442)[  .h](http://weharmonyos.com/blog/43_中断概念篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v42.xx 鸿蒙内核源码分析(中断切换篇) | 系统因中断活力四射](https://my.oschina.net/weharmony/blog/4990948) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/114988891)[  .h](http://weharmonyos.com/blog/42_中断切换篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v41.xx 鸿蒙内核源码分析(任务切换篇) | 看汇编如何切换任务 ](https://my.oschina.net/weharmony/blog/4988628) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/114890180)[  .h](http://weharmonyos.com/blog/41_任务切换篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v40.xx 鸿蒙内核源码分析(汇编汇总篇) | 汇编可爱如邻家女孩 ](https://my.oschina.net/weharmony/blog/4977924) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/114597179)[  .h](http://weharmonyos.com/blog/40_汇编汇总篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v39.xx 鸿蒙内核源码分析(异常接管篇) | 社会很单纯，复杂的是人 ](https://my.oschina.net/weharmony/blog/4973016) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/114438285)[  .h](http://weharmonyos.com/blog/39_异常接管篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v38.xx 鸿蒙内核源码分析(寄存器篇) | 小强乃宇宙最忙存储器 ](https://my.oschina.net/weharmony/blog/4969487) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/114326994)[  .h](http://weharmonyos.com/blog/38_寄存器篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v37.xx 鸿蒙内核源码分析(系统调用篇) | 开发者永远的口头禅 ](https://my.oschina.net/weharmony/blog/4967613) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/114285166)[  .h](http://weharmonyos.com/blog/37_系统调用篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v36.xx 鸿蒙内核源码分析(工作模式篇) | CPU是韦小宝，七个老婆 ](https://my.oschina.net/weharmony/blog/4965052) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/114168567)[  .h](http://weharmonyos.com/blog/36_工作模式篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v35.xx 鸿蒙内核源码分析(时间管理篇) | 谁是内核基本时间单位 ](https://my.oschina.net/weharmony/blog/4956163) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113867785)[  .h](http://weharmonyos.com/blog/35_时间管理篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v34.xx 鸿蒙内核源码分析(原子操作篇) | 谁在为原子操作保驾护航 ](https://my.oschina.net/weharmony/blog/4955290) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113850603)[  .h](http://weharmonyos.com/blog/34_原子操作篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v33.xx 鸿蒙内核源码分析(消息队列篇) | 进程间如何异步传递大数据 ](https://my.oschina.net/weharmony/blog/4952961) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113815355)[  .h](http://weharmonyos.com/blog/33_消息队列篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v32.xx 鸿蒙内核源码分析(CPU篇) | 整个内核就是一个死循环 ](https://my.oschina.net/weharmony/blog/4952034) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113782749)[  .h](http://weharmonyos.com/blog/32_CPU篇.html) [  .o](https://my.oschina.net/weharmony)**  

* [v31.xx 鸿蒙内核源码分析(定时器篇) | 哪个任务的优先级最高 ](https://my.oschina.net/weharmony/blog/4951625) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113774260)[  .h](http://weharmonyos.com/blog/31_定时器篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v30.xx 鸿蒙内核源码分析(事件控制篇) | 任务间多对多的同步方案 ](https://my.oschina.net/weharmony/blog/4950956) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113759481)[  .h](http://weharmonyos.com/blog/30_事件控制篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v29.xx 鸿蒙内核源码分析(信号量篇) | 谁在负责解决任务的同步 ](https://my.oschina.net/weharmony/blog/4949720) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113744267)[  .h](http://weharmonyos.com/blog/29_信号量篇.html) [  .o](https://my.oschina.net/weharmony)**
  
* [v28.xx 鸿蒙内核源码分析(进程通讯篇) | 九种进程间通讯方式速揽 ](https://my.oschina.net/weharmony/blog/4947398) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113700751)[  .h](http://weharmonyos.com/blog/28_进程通讯篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v27.xx 鸿蒙内核源码分析(互斥锁篇) | 比自旋锁丰满的互斥锁 ](https://my.oschina.net/weharmony/blog/4945465) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113660357)[  .h](http://weharmonyos.com/blog/27_互斥锁篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v26.xx 鸿蒙内核源码分析(自旋锁篇) | 自旋锁当立贞节牌坊 ](https://my.oschina.net/weharmony/blog/4944129) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113616250)[  .h](http://weharmonyos.com/blog/26_自旋锁篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v25.xx 鸿蒙内核源码分析(并发并行篇) | 听过无数遍的两个概念 ](https://my.oschina.net/u/3751245/blog/4940329) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113516222)[  .h](http://weharmonyos.com/blog/25_并发并行篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v24.xx 鸿蒙内核源码分析(进程概念篇) | 进程在管理哪些资源 ](https://my.oschina.net/u/3751245/blog/4937521) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113395872)[  .h](http://weharmonyos.com/blog/24_进程概念篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v23.xx 鸿蒙内核源码分析(汇编传参篇) | 如何传递复杂的参数 ](https://my.oschina.net/u/3751245/blog/4927892) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113265990)[  .h](http://weharmonyos.com/blog/23_汇编传参篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v22.xx 鸿蒙内核源码分析(汇编基础篇) | CPU在哪里打卡上班 ](https://my.oschina.net/u/3751245/blog/4920361) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/112986628)[  .h](http://weharmonyos.com/blog/22_汇编基础篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v21.xx 鸿蒙内核源码分析(线程概念篇) | 是谁在不断的折腾CPU ](https://my.oschina.net/u/3751245/blog/4915543) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/112870193)[  .h](http://weharmonyos.com/blog/21_线程概念篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v20.xx 鸿蒙内核源码分析(用栈方式篇) | 程序运行场地由谁提供 ](https://my.oschina.net/u/3751245/blog/4893388) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/112534331)[  .h](http://weharmonyos.com/blog/20_用栈方式篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v19.xx 鸿蒙内核源码分析(位图管理篇) | 谁能一分钱分两半花 ](https://my.oschina.net/u/3751245/blog/4888467) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/112394982)[  .h](http://weharmonyos.com/blog/19_位图管理篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v18.xx 鸿蒙内核源码分析(源码结构篇) | 内核每个文件的含义 ](https://my.oschina.net/u/3751245/blog/4869137) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/111938348)[  .h](http://weharmonyos.com/blog/18_源码结构篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v17.xx 鸿蒙内核源码分析(物理内存篇) | 怎么管理物理内存 ](https://my.oschina.net/u/3751245/blog/4842408) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/111765600)[  .h](http://weharmonyos.com/blog/17_物理内存篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v16.xx 鸿蒙内核源码分析(内存规则篇) | 内存管理到底在管什么 ](https://my.oschina.net/u/3751245/blog/4698384) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/109437223)[  .h](http://weharmonyos.com/blog/16_内存规则篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v15.xx 鸿蒙内核源码分析(内存映射篇) | 虚拟内存虚在哪里 ](https://my.oschina.net/u/3751245/blog/4694841) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/109032636)[  .h](http://weharmonyos.com/blog/15_内存映射篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v14.xx 鸿蒙内核源码分析(内存汇编篇) | 谁是虚拟内存实现的基础 ](https://my.oschina.net/u/3751245/blog/4692156) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108994081)[  .h](http://weharmonyos.com/blog/14_内存汇编篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v13.xx 鸿蒙内核源码分析(源码注释篇) | 鸿蒙必定成功，也必然成功 ](https://my.oschina.net/u/3751245/blog/4686747) **[  | 51](https://harmonyos.51cto.com/posts/4049)[ .c](https://blog.csdn.net/kuangyufei/article/details/109251754)[  .h](http://weharmonyos.com/blog/13_源码注释篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v12.xx 鸿蒙内核源码分析(内存管理篇) | 虚拟内存全景图是怎样的 ](https://my.oschina.net/u/3751245/blog/4652284) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108821442)[  .h](http://weharmonyos.com/blog/12_内存管理篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v11.xx 鸿蒙内核源码分析(内存分配篇) | 内存有哪些分配方式 ](https://my.oschina.net/u/3751245/blog/4646802) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108989906)[  .h](http://weharmonyos.com/blog/11_内存分配篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v10.xx 鸿蒙内核源码分析(内存主奴篇) | 皇上和奴才如何相处 ](https://my.oschina.net/u/3751245/blog/4646802) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108723672)[  .h](http://weharmonyos.com/blog/10_内存主奴篇.html) [  .o](https://my.oschina.net/weharmony)**
  
* [v09.xx 鸿蒙内核源码分析(调度故事篇) | 用故事说内核调度过程 ](https://my.oschina.net/u/3751245/blog/4634668) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108745174)[  .h](http://weharmonyos.com/blog/09_调度故事篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v08.xx 鸿蒙内核源码分析(总目录) | 百万汉字注解 百篇博客分析 ](https://my.oschina.net/weharmony/blog/4626852) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108727970)[  .h](http://weharmonyos.com/blog/08_总目录.html) [  .o](https://my.oschina.net/weharmony)**
  
* [v07.xx 鸿蒙内核源码分析(调度机制篇) | 任务是如何被调度执行的 ](https://my.oschina.net/u/3751245/blog/4623040) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108705968)[  .h](http://weharmonyos.com/blog/07_调度机制篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v06.xx 鸿蒙内核源码分析(调度队列篇) | 内核有多少个调度队列 ](https://my.oschina.net/u/3751245/blog/4606916) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108626671)[  .h](http://weharmonyos.com/blog/06_调度队列篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v05.xx 鸿蒙内核源码分析(任务管理篇) | 任务池是如何管理的 ](https://my.oschina.net/u/3751245/blog/4603919) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108661248)[  .h](http://weharmonyos.com/blog/05_任务管理篇.html) [  .o](https://my.oschina.net/weharmony)**
  
* [v04.xx 鸿蒙内核源码分析(任务调度篇) | 任务是内核调度的单元 ](https://my.oschina.net/weharmony/blog/4595539) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108621428)[  .h](http://weharmonyos.com/blog/04_任务调度篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v03.xx 鸿蒙内核源码分析(时钟任务篇) | 触发调度谁的贡献最大 ](https://my.oschina.net/u/3751245/blog/4574493) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108603468)[  .h](http://weharmonyos.com/blog/03_时钟任务篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v02.xx 鸿蒙内核源码分析(进程管理篇) | 谁在管理内核资源 ](https://my.oschina.net/u/3751245/blog/4574429) **[  | 51](https://harmonyos.51cto.com/posts/3926)[ .c](https://blog.csdn.net/kuangyufei/article/details/108595941)[  .h](http://weharmonyos.com/blog/02_进程管理篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v01.xx 鸿蒙内核源码分析(双向链表篇) | 谁是内核最重要结构体 ](https://my.oschina.net/u/3751245/blog/4572304) **[  | 51](https://harmonyos.51cto.com/posts/3925)[ .c](https://blog.csdn.net/kuangyufei/article/details/108585659)[  .h](http://weharmonyos.com/blog/01_双向链表篇.html) [  .o](https://my.oschina.net/weharmony)**

### 关于 51 .c .h .o
看系列篇文章会常看到 `51 .c .h .o`，希望这对大家阅读不会造成影响. 
分别对应以下四个站点的首个字符，感谢这些站点一直以来对系列篇的支持和推荐，尤其是 **oschina gitee** ，很喜欢它的界面风格，简洁大方，让人感觉到开源的伟大!
* [51cto](https://harmonyos.51cto.com/column/34)
* [csdn](https://blog.csdn.net/kuangyufei)
* [harmony](http://weharmonyos.com/)
* [oschina](https://my.oschina.net/weharmony)
  
而巧合的是`.c .h .o`是C语言的头/源/目标文件，这就很有意思了，冥冥之中似有天数，将这四个宝贝以这种方式融合在一起. `51 .c .h .o` ， 我要CHO ，嗯嗯，hin 顺口 : )

### 百万汉字注解.百篇博客分析
[百万汉字注解 >> 精读鸿蒙源码，中文注解分析， 深挖地基工程，大脑永久记忆，四大码仓每日同步更新](https://gitee.com/weharmony/kernel_liteos_a_note)[< gitee ](https://gitee.com/weharmony/kernel_liteos_a_note)[| github ](https://github.com/kuangyufei/kernel_liteos_a_note)[| csdn ](https://codechina.csdn.net/kuangyufei/kernel_liteos_a_note)[| coding >](https://weharmony.coding.net/public/harmony/kernel_liteos_a_note/git/files)

[百篇博客分析 >> 故事说内核，问答式导读，生活式比喻，表格化说明，图形化展示，主流站点定期更新中](http://weharmonyos.com)[< 51cto  ](https://harmonyos.51cto.com/column/34)[| csdn ](https://blog.csdn.net/kuangyufei)[| harmony ](http://weharmonyos.com/)[ | osc >](https://my.oschina.net/weharmony)

### 关注不迷路.代码即人生
[![鸿蒙内核源码分析](https://gitee.com/weharmony/docs/raw/master/pic/other/so1so.png)](https://gitee.com/weharmony/docs/raw/master/pic/other/so1so.png)

[热爱是所有的理由和答案 - turing](http://weharmonyos.com/)

原创不易，欢迎转载，但麻烦请注明出处.