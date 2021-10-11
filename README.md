子曰：“里仁为美。择不处仁，焉得知？”《论语》：里仁篇
[![在这里插入图片描述](https://gitee.com/weharmony/docs/raw/master/pic/other/io1.png)](http://weharmonyos.com/)

百篇博客系列篇.本篇为:

v13.xx 鸿蒙内核源码分析(源码注释篇) | 每天死磕一点点


前因后果相关篇为: 

* [v08.03 鸿蒙内核源码分析(总目录) | 百万汉字注解 百篇博客分析](https://my.oschina.net/weharmony/blog/4626852)
* [v09.04 鸿蒙内核源码分析(调度故事) | 用故事说内核调度过程](https://my.oschina.net/weharmony/blog/4634668)
* [v10.03 鸿蒙内核源码分析(内存主奴) | 皇上和奴才如何相处](https://my.oschina.net/weharmony/blog/4646802)
* [v13.05 鸿蒙内核源码分析(源码注释) | 每天死磕一点点](https://my.oschina.net/weharmony/blog/4686747)
* [v18.02 鸿蒙内核源码分析(源码结构) | 内核文件各自含义](https://my.oschina.net/weharmony/blog/4869137)
* [v52.05 鸿蒙内核源码分析(静态站点) | 五一哪也没去在干这事](https://my.oschina.net/weharmony/blog/5042657)


#### 几点说明

**中文注解鸿蒙内核 | [kernel_liteos_a_note](https://gitee.com/weharmony/kernel_liteos_a_note)**  

是在 `OpenHarmony` 的 [kernel_liteos_a](https://gitee.com/openharmony/kernel_liteos_a) 基础上给内核源码加上中文注解的版本.与官方源码按月保持同步,同步历史如下:

  * `2021/10/09` -- 增加性能优化模块`perf`,优化了文件映射模块
  * `2021/09/14` -- common,extended等几个目录结构和Makefile调整
  * `2021/08/19` -- 各目录增加了`BUILD.gn`文件,文件系统部分文件调整 
  * `2021/07/15` -- 改动不大,新增`blackbox`,`hidumper`,对一些宏规范化使用 
  * `2021/06/27` -- 对文件系统/设备驱动改动较大,目录结构进行了重新整理
  * `2021/06/08` -- 对编译构建,任务,信号模块有较大的改动
  * `2021/05/28` -- 改动不大,主要针对一些错误单词拼写纠正
  * `2021/05/13` -- 对系统调用，任务切换，信号处理，异常接管，文件管理，shell做了较大更新，代码结构更清晰
  * `2021/04/21` -- 官方优化了很多之前吐槽的地方，点赞
  * `2020/09/16` -- 中文注解版起点

**鸿蒙研究站 | [weharmonyos.com](http://weharmonyos.com)**

分成三部分内容

  *  [OpenHarmony开发者文档](http://weharmonyos.com/openharmony) 是对官方文档 [docs](https://gitee.com/openharmony/docs) 做的非常炫酷的静态站点，支持侧边栏/面包屑/搜索/中英文，非常方便的查看官方文档，大大提高学习和开发效率
  *  [百篇博客分析鸿蒙内核](http://weharmonyos.com/weharmony) 是对内核源码注解过程中整理出来的内容输出. 
  *  整理的一些分析内核的工具和书籍 如: [鸿蒙源码分析.离线文档](http://weharmonyos.com/history.html) , [GNU汇编](http://weharmonyos.com/book/assembly.html), [gn参考手册](http://weharmonyos.com/gn/docs/)
  
**子系统注解仓库**
  
  在给鸿蒙内核源码加注过程中发现仅仅注解内核仓库还不够,因为它关联了其他子系统,若对这些子系统不了解是很难完整的注解鸿蒙内核,所以也对这些关联仓库进行了部分注解,这些仓库包括:
  
  * [编译构建子系统 | build_lite](https://gitee.com/weharmony/build_lite_note)  
  * [协议栈 | lwip](https://gitee.com/weharmony/third_party_lwip) 
  * [文件系统 | NuttX](https://gitee.com/weharmony/third_party_NuttX)
  * [标准库 | musl](https://gitee.com/weharmony/third_party_musl) 


      
#### 为何要精读内核源码?
* 码农的学职生涯，都应精读一遍内核源码。以浇筑好计算机知识大厦的地基，地基纵深的坚固程度，很大程度能决定未来大厦能盖多高。那为何一定要精读细品呢?
* 因为内核代码本身并不太多，都是浓缩的精华，精读是让各个知识点高频出现，不孤立成点状记忆，没有足够连接点的知识点是很容易忘的，点点成线，线面成体，连接越多，记得越牢，如此短时间内容易结成一张高浓度，高密度的系统化知识网，训练大脑肌肉记忆，驻入大脑直觉区，想抹都抹不掉，终生携带，随时调取。跟骑单车一样，一旦学会，即便多年不骑，照样跨上就走，游刃有余。
#### 热爱是所有的理由和答案
* 因大学时阅读 `linux 2.6` 内核痛并快乐的经历，一直有个心愿，如何让更多对内核感兴趣的朋友减少阅读时间，加速对计算机系统级的理解，而不至于过早的放弃。但因过程种种，多年一直没有行动，基本要放弃这件事了。恰逢 `2020/9/10` 鸿蒙正式开源，重新激活了多年的心愿，就有那么点如黄河之水一发不可收拾了。 
* 目前对内核源码的注解已完成了 `70%` ，对内核源码的博客分析已完成了`70+篇`， 每天都很充实，很兴奋，连做梦内核代码都在鱼贯而入。如此疯狂地做一件事还是当年谈恋爱的时候， 只因热爱， 热爱是所有的理由和答案。 :P 
#### (〃･ิ‿･ิ)ゞ鸿蒙内核开发者
* 感谢开放原子开源基金会，致敬鸿蒙内核开发者提供了如此优秀的源码，一了多年的夙愿，津津乐道于此。精读内核源码加注并整理成档是件很有挑战的事，时间上要以月甚至年为单位，但正因为很难才值得去做! 干困难事，方有所得;专注聚焦，必有所获。 
* 从内核一行行的代码中能深深感受到开发者各中艰辛与坚持，及鸿蒙生态对未来的价值，这些是张嘴就来的网络喷子们永远不能体会到的。可以毫不夸张的说鸿蒙内核源码可作为大学: C语言，数据结构，操作系统，汇编语言，计算机系统结构，计算机组成原理 六门课程的教学项目。如此宝库，不深入研究实在是暴殄天物，于心不忍，注者坚信鸿蒙大势所趋，未来可期，其必定成功，也必然成功，誓做其坚定的追随者和传播者。
#### 理解内核的三个层级
* 普通概念映射级: 这一级不涉及专业知识，用大众所熟知的公共认知就能听明白是个什么概念，也就是说用一个普通人都懂的概念去诠释或者映射一个他们从没听过的概念。让陌生的知识点与大脑中烂熟于心的知识点建立多重链接，加深记忆。说别人能听得懂的话这很重要!!! 一个没学过计算机知识的卖菜大妈就不可能知道内核的基本运作了吗? 不一定!在系列篇中试图用 v08.xx 鸿蒙内核源码分析(总目录) 之故事篇 去引导这一层级的认知，希望能卷入更多的人来关注基础软件，尤其是那些资本大鳄，加大对基础软件的投入。
* 专业概念抽象级: 对抽象的专业逻辑概念具体化认知， 比如虚拟内存，老百姓是听不懂的，学过计算机的人都懂，具体怎么实现的很多人又都不懂了，但这并不妨碍成为一个优秀的上层应用开发者，因为虚拟内存已经被抽象出来，目的是要屏蔽上层对它具体实现的认知。试图用 鸿蒙内核源码分析(总目录 去拆解那些已经被抽象出来的专业概念， 希望能卷入更多对内核感兴趣的应用软件人才流入基础软硬件生态， 应用软件咱们是无敌宇宙，但基础软件却很薄弱。
* 具体微观代码级: 这一级是具体到每一行代码的实现，到了用代码指令级的地步，这段代码是什么意思?为什么要这么设计?有没有更好的方案? [鸿蒙内核源码注解分析](https://gitee.com/weharmony/kernel_liteos_a_note) 试图从细微处去解释代码实现层，英文真的是天生适合设计成编程语言的人类语言，计算机的01码映射到人类世界的26个字母，诞生了太多的伟大奇迹。但我们的母语注定了很大部分人存在着自然语言层级的理解映射，希望鸿蒙内核源码注解分析能让更多爱好者快速的理解内核，共同进步。
#### 加注方式是怎样的？

*  因鸿蒙内核6W+代码量，本身只有较少的注释， 中文注解以不对原有代码侵入为前提，源码中所有英文部分都是原有注释，所有中文部分都是中文版的注释，同时为方便同步官方版本的更新，尽量不去增加代码的行数，不破坏文件的结构，注释多类似以下的方式:

    在重要模块的.c/.h文件开始位置先对模块功能做整体的介绍，例如异常接管模块注解如图所示:

    ![在这里插入图片描述](https://gitee.com/weharmony/docs/raw/master/pic/other/ycjg.png)

    注解过程中查阅了很多的资料和书籍，在具体代码处都附上了参考链接。

* 而函数级注解会详细到重点行，甚至每一行， 例如申请互斥锁的主体函数，不可谓不重要，而官方注释仅有一行，如图所示

    ![在这里插入图片描述](https://gitee.com/weharmony/docs/raw/master/pic/other/sop.png)

* 另外画了一些字符图方便理解，直接嵌入到头文件中，比如虚拟内存的全景图，因没有这些图是很难理解虚拟内存是如何管理的。

    ![在这里插入图片描述](https://gitee.com/weharmony/docs/raw/master/pic/other/vm.png)
#### 有哪些特殊的记号
- [x] 搜索 `@note_pic` 可查看绘制的全部字符图
- [x] 搜索 `@note_why` 是尚未看明白的地方，有看明白的，请Pull Request完善
- [x] 搜索 `@note_thinking` 是一些的思考和建议
- [x] 搜索 `@note_#if0` 是由第三方项目提供不在内核源码中定义的极为重要结构体，为方便理解而添加的。
- [x] 搜索 `@note_good` 是给源码点赞的地方

#### 目录结构
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

#### **百篇博客分析.深挖内核地基**
* 给鸿蒙内核源码加注释过程中，整理出以下文章。内容立足源码，常以生活场景打比方尽可能多的将内核知识点置入某种场景，具有画面感，容易理解记忆。说别人能听得懂的话很重要! 百篇博客绝不是百度教条式的在说一堆诘屈聱牙的概念，那没什么意思。更希望让内核变得栩栩如生，倍感亲切.确实有难度，自不量力，但已经出发，回头已是不可能的了。　😛
* 与代码有bug需不断debug一样，文章和注解内容会存在不少错漏之处，请多包涵，但会反复修正，持续更新，v**.xx 代表文章序号和修改的次数，精雕细琢，言简意赅，力求打造精品内容。

按时间顺序:

* [v01.12 鸿蒙内核源码分析(双向链表) | 谁是内核最重要结构体](https://my.oschina.net/weharmony/blog/4572304)
* [v02.06 鸿蒙内核源码分析(进程管理) | 谁在管理内核资源](https://my.oschina.net/weharmony/blog/4574429)
* [v03.06 鸿蒙内核源码分析(时钟任务) | 触发调度谁的贡献最大](https://my.oschina.net/weharmony/blog/4574493)
* [v04.03 鸿蒙内核源码分析(任务调度) | 内核调度的单元是谁](https://my.oschina.net/weharmony/blog/4595539)
* [v05.05 鸿蒙内核源码分析(任务管理) | 任务池是如何管理的](https://my.oschina.net/weharmony/blog/4603919)
* [v06.03 鸿蒙内核源码分析(调度队列) | 内核调度需要排队](https://my.oschina.net/weharmony/blog/4606916)
* [v07.08 鸿蒙内核源码分析(调度机制) | 任务是如何被调度执行的](https://my.oschina.net/weharmony/blog/4623040)
* [v08.03 鸿蒙内核源码分析(总目录) | 百万汉字注解 百篇博客分析](https://my.oschina.net/weharmony/blog/4626852)
* [v09.04 鸿蒙内核源码分析(调度故事) | 用故事说内核调度过程](https://my.oschina.net/weharmony/blog/4634668)
* [v10.03 鸿蒙内核源码分析(内存主奴) | 皇上和奴才如何相处](https://my.oschina.net/weharmony/blog/4646802)
* [v11.03 鸿蒙内核源码分析(内存分配) | 内存有哪些分配方式](https://my.oschina.net/weharmony/blog/4646802)
* [v12.04 鸿蒙内核源码分析(内存管理) | 虚拟内存全景图是怎样的](https://my.oschina.net/weharmony/blog/4652284)
* [v13.05 鸿蒙内核源码分析(源码注释) | 每天死磕一点点](https://my.oschina.net/weharmony/blog/4686747)
* [v14.02 鸿蒙内核源码分析(内存汇编) | 谁是虚拟内存实现的基础](https://my.oschina.net/weharmony/blog/4692156)
* [v15.03 鸿蒙内核源码分析(内存映射) | 映射真是个好东西](https://my.oschina.net/weharmony/blog/4694841)
* [v16.02 鸿蒙内核源码分析(内存规则) | 内存管理到底在管什么](https://my.oschina.net/weharmony/blog/4698384)
* [v17.04 鸿蒙内核源码分析(物理内存) | 怎么管理物理内存](https://my.oschina.net/weharmony/blog/4842408)
* [v18.02 鸿蒙内核源码分析(源码结构) | 内核文件各自含义](https://my.oschina.net/weharmony/blog/4869137)
* [v19.04 鸿蒙内核源码分析(位图管理) | 特节俭的苦命孩子](https://my.oschina.net/weharmony/blog/4888467)
* [v20.03 鸿蒙内核源码分析(用栈方式) | 谁来提供程序运行场地](https://my.oschina.net/weharmony/blog/4893388)
* [v21.07 鸿蒙内核源码分析(线程概念) | 是谁在不断的折腾CPU](https://my.oschina.net/weharmony/blog/4915543)
* [v22.03 鸿蒙内核源码分析(汇编基础) | CPU上班也要打卡](https://my.oschina.net/weharmony/blog/4920361)
* [v23.04 鸿蒙内核源码分析(汇编传参) | 如何传递复杂的参数](https://my.oschina.net/weharmony/blog/4927892)
* [v24.03 鸿蒙内核源码分析(进程概念) | 怎么更好的理解进程](https://my.oschina.net/weharmony/blog/4937521)
* [v25.05 鸿蒙内核源码分析(并发并行) | 听过无数遍的两个概念](https://my.oschina.net/weharmony/blog/4940329)
* [v26.08 鸿蒙内核源码分析(自旋锁) | 当立贞节牌坊的好同志](https://my.oschina.net/weharmony/blog/4944129)
* [v27.05 鸿蒙内核源码分析(互斥锁) | 比自旋锁丰满的互斥锁](https://my.oschina.net/weharmony/blog/4945465)
* [v28.04 鸿蒙内核源码分析(进程通讯) | 九种进程间通讯方式速揽](https://my.oschina.net/weharmony/blog/4947398)
* [v29.05 鸿蒙内核源码分析(信号量) | 谁在解决任务间的同步](https://my.oschina.net/weharmony/blog/4949720)
* [v30.07 鸿蒙内核源码分析(事件控制) | 任务间多对多的同步方案](https://my.oschina.net/weharmony/blog/4950956)
* [v31.02 鸿蒙内核源码分析(定时器) | 内核优先级最高任务是它](https://my.oschina.net/weharmony/blog/4951625)
* [v32.03 鸿蒙内核源码分析(CPU) | 整个内核是一个死循环](https://my.oschina.net/weharmony/blog/4952034)
* [v33.03 鸿蒙内核源码分析(消息队列) | 进程间如何异步传递大数据](https://my.oschina.net/weharmony/blog/4952961)
* [v34.04 鸿蒙内核源码分析(原子操作) | 谁在为完整性保驾护航](https://my.oschina.net/weharmony/blog/4955290)
* [v35.03 鸿蒙内核源码分析(时间管理) | 内核基本时间单位是哪位](https://my.oschina.net/weharmony/blog/4956163)
* [v36.05 鸿蒙内核源码分析(工作模式) | 它是程序界的韦小宝](https://my.oschina.net/weharmony/blog/4965052)
* [v37.06 鸿蒙内核源码分析(系统调用) | 开发者永远的口头禅](https://my.oschina.net/weharmony/blog/4967613)
* [v38.06 鸿蒙内核源码分析(寄存器) | 全宇宙就只佩服它](https://my.oschina.net/weharmony/blog/4969487)
* [v39.06 鸿蒙内核源码分析(异常接管) | 社会很单纯 复杂的是人](https://my.oschina.net/weharmony/blog/4973016)
* [v40.03 鸿蒙内核源码分析(汇编汇总) | 汇编可爱如邻家女孩](https://my.oschina.net/weharmony/blog/4977924)
* [v41.03 鸿蒙内核源码分析(任务切换) | 看汇编如何切换任务](https://my.oschina.net/weharmony/blog/4988628)
* [v42.05 鸿蒙内核源码分析(中断切换) | 系统因中断活力四射](https://my.oschina.net/weharmony/blog/4990948)
* [v43.05 鸿蒙内核源码分析(中断概念) | 海公公的日常工作](https://my.oschina.net/weharmony/blog/4992750)
* [v44.04 鸿蒙内核源码分析(中断管理) | 江湖从此不再怕中断](https://my.oschina.net/weharmony/blog/4995800)
* [v45.05 鸿蒙内核源码分析(Fork) | 一次调用 两次返回](https://my.oschina.net/weharmony/blog/5010301)
* [v46.05 鸿蒙内核源码分析(特殊进程) | 老鼠生儿会打洞](https://my.oschina.net/weharmony/blog/5014444)
* [v47.02 鸿蒙内核源码分析(进程回收) | 临终向老祖宗托孤](https://my.oschina.net/weharmony/blog/5017716)
* [v48.05 鸿蒙内核源码分析(信号生产) | 年过半百 活力十足](https://my.oschina.net/weharmony/blog/5022149)
* [v49.03 鸿蒙内核源码分析(信号消费) | 谁让CPU连续四次换栈运行](https://my.oschina.net/weharmony/blog/5027224)
* [v50.03 鸿蒙内核源码分析(编译环境) | 编译鸿蒙防掉坑指南](https://my.oschina.net/weharmony/blog/5028613)
* [v51.04 鸿蒙内核源码分析(ELF格式) | 应用程序入口并非main](https://my.oschina.net/weharmony/blog/5030288)
* [v52.05 鸿蒙内核源码分析(静态站点) | 五一哪也没去在干这事](https://my.oschina.net/weharmony/blog/5042657)
* [v53.03 鸿蒙内核源码分析(ELF解析) | 敢忘了她姐俩你就不是银](https://my.oschina.net/weharmony/blog/5048746)
* [v54.04 鸿蒙内核源码分析(静态链接) | 一个小项目看中间过程](https://my.oschina.net/weharmony/blog/5049918)
* [v55.04 鸿蒙内核源码分析(重定位) | 与国际接轨的对外部发言人](https://my.oschina.net/weharmony/blog/5055124)
* [v56.05 鸿蒙内核源码分析(进程映像) | 程序是如何被加载运行的](https://my.oschina.net/weharmony/blog/5060359)
* [v57.02 鸿蒙内核源码分析(编译过程) | 简单案例窥视中间过程](https://my.oschina.net/weharmony/blog/5064209)
* [v58.03 鸿蒙内核源码分析(环境脚本) | 编译鸿蒙原来如此简单](https://my.oschina.net/weharmony/blog/5132725)
* [v59.04 鸿蒙内核源码分析(构建工具) | 顺瓜摸藤调试构建过程](https://my.oschina.net/weharmony/blog/5135157)
* [v60.04 鸿蒙内核源码分析(gn应用) | 鸿蒙用什么工具构建](https://my.oschina.net/weharmony/blog/5137565)
* [v61.03 鸿蒙内核源码分析(忍者ninja) | 忍者的特点就是一个字](https://my.oschina.net/weharmony/blog/5139034)
* [v62.02 鸿蒙内核源码分析(文件概念) | 为什么说一切皆是文件](https://my.oschina.net/weharmony/blog/5152858)
* [v63.04 鸿蒙内核源码分析(文件系统) | 用图书管理说文件系统](https://my.oschina.net/weharmony/blog/5165752)
* [v64.06 鸿蒙内核源码分析(索引节点) | 谁是文件系统最重要的概念](https://my.oschina.net/weharmony/blog/5168716)
* [v65.05 鸿蒙内核源码分析(挂载目录) | 为何文件系统需要挂载](https://my.oschina.net/weharmony/blog/5172566)
* [v66.07 鸿蒙内核源码分析(根文件系统) | 谁先挂到/谁就是...](https://my.oschina.net/weharmony/blog/5177087)
* [v67.03 鸿蒙内核源码分析(字符设备) | 绝大多数设备都是...](https://my.oschina.net/weharmony/blog/5200946)
* [v68.02 鸿蒙内核源码分析(VFS) | 文件系统是个大家庭](https://my.oschina.net/weharmony/blog/5211662)
* [v69.04 鸿蒙内核源码分析(文件句柄) | 你为什么叫句柄](https://my.oschina.net/weharmony/blog/5253251)
* [v70.05 鸿蒙内核源码分析(管道文件) | 如何降低数据流动成本](https://my.oschina.net/weharmony/blog/5258434)
* [v71.03 鸿蒙内核源码分析(Shell编辑) | 两个任务 三个阶段](https://my.oschina.net/weharmony/blog/5269307)
* [v72.01 鸿蒙内核源码分析(Shell解析) | 应用窥伺内核的窗口](https://my.oschina.net/weharmony/blog/5269307)


按功能模块:

|前因后果|基础工具|加载运行|进程管理
|:-:|:-:|:-:|:-:|
[总目录](https://my.oschina.net/weharmony/blog/4626852) [调度故事](https://my.oschina.net/weharmony/blog/4634668) [内存主奴](https://my.oschina.net/weharmony/blog/4646802) [源码注释](https://my.oschina.net/weharmony/blog/4686747) [源码结构](https://my.oschina.net/weharmony/blog/4869137) [静态站点](https://my.oschina.net/weharmony/blog/5042657) |[双向链表](https://my.oschina.net/weharmony/blog/4572304) [位图管理](https://my.oschina.net/weharmony/blog/4888467) [用栈方式](https://my.oschina.net/weharmony/blog/4893388) [定时器](https://my.oschina.net/weharmony/blog/4951625) [原子操作](https://my.oschina.net/weharmony/blog/4955290) [时间管理](https://my.oschina.net/weharmony/blog/4956163) |[ELF格式](https://my.oschina.net/weharmony/blog/5030288) [ELF解析](https://my.oschina.net/weharmony/blog/5048746) [静态链接](https://my.oschina.net/weharmony/blog/5049918) [重定位](https://my.oschina.net/weharmony/blog/5055124) [进程映像](https://my.oschina.net/weharmony/blog/5060359) |[进程管理](https://my.oschina.net/weharmony/blog/4574429) [进程概念](https://my.oschina.net/weharmony/blog/4937521) [Fork](https://my.oschina.net/weharmony/blog/5010301) [特殊进程](https://my.oschina.net/weharmony/blog/5014444) [进程回收](https://my.oschina.net/weharmony/blog/5017716) [信号生产](https://my.oschina.net/weharmony/blog/5022149) [信号消费](https://my.oschina.net/weharmony/blog/5027224) [Shell编辑](https://my.oschina.net/weharmony/blog/5269307) [Shell解析](https://my.oschina.net/weharmony/blog/5269307) |
|编译构建|进程通讯|内存管理|任务管理
[编译环境](https://my.oschina.net/weharmony/blog/5028613) [编译过程](https://my.oschina.net/weharmony/blog/5064209) [环境脚本](https://my.oschina.net/weharmony/blog/5132725) [构建工具](https://my.oschina.net/weharmony/blog/5135157) [gn应用](https://my.oschina.net/weharmony/blog/5137565) [忍者ninja](https://my.oschina.net/weharmony/blog/5139034) |[自旋锁](https://my.oschina.net/weharmony/blog/4944129) [互斥锁](https://my.oschina.net/weharmony/blog/4945465) [进程通讯](https://my.oschina.net/weharmony/blog/4947398) [信号量](https://my.oschina.net/weharmony/blog/4949720) [事件控制](https://my.oschina.net/weharmony/blog/4950956) [消息队列](https://my.oschina.net/weharmony/blog/4952961) |[内存分配](https://my.oschina.net/weharmony/blog/4646802) [内存管理](https://my.oschina.net/weharmony/blog/4652284) [内存汇编](https://my.oschina.net/weharmony/blog/4692156) [内存映射](https://my.oschina.net/weharmony/blog/4694841) [内存规则](https://my.oschina.net/weharmony/blog/4698384) [物理内存](https://my.oschina.net/weharmony/blog/4842408) |[时钟任务](https://my.oschina.net/weharmony/blog/4574493) [任务调度](https://my.oschina.net/weharmony/blog/4595539) [任务管理](https://my.oschina.net/weharmony/blog/4603919) [调度队列](https://my.oschina.net/weharmony/blog/4606916) [调度机制](https://my.oschina.net/weharmony/blog/4623040) [线程概念](https://my.oschina.net/weharmony/blog/4915543) [并发并行](https://my.oschina.net/weharmony/blog/4940329) [CPU](https://my.oschina.net/weharmony/blog/4952034) [系统调用](https://my.oschina.net/weharmony/blog/4967613) [任务切换](https://my.oschina.net/weharmony/blog/4988628) |
|文件系统|硬件架构
[文件概念](https://my.oschina.net/weharmony/blog/5152858) [文件系统](https://my.oschina.net/weharmony/blog/5165752) [索引节点](https://my.oschina.net/weharmony/blog/5168716) [挂载目录](https://my.oschina.net/weharmony/blog/5172566) [根文件系统](https://my.oschina.net/weharmony/blog/5177087) [字符设备](https://my.oschina.net/weharmony/blog/5200946) [VFS](https://my.oschina.net/weharmony/blog/5211662) [文件句柄](https://my.oschina.net/weharmony/blog/5253251) [管道文件](https://my.oschina.net/weharmony/blog/5258434) |[汇编基础](https://my.oschina.net/weharmony/blog/4920361) [汇编传参](https://my.oschina.net/weharmony/blog/4927892) [工作模式](https://my.oschina.net/weharmony/blog/4965052) [寄存器](https://my.oschina.net/weharmony/blog/4969487) [异常接管](https://my.oschina.net/weharmony/blog/4973016) [汇编汇总](https://my.oschina.net/weharmony/blog/4977924) [中断切换](https://my.oschina.net/weharmony/blog/4990948) [中断概念](https://my.oschina.net/weharmony/blog/4992750) [中断管理](https://my.oschina.net/weharmony/blog/4995800) |
#### **百万汉字注解.精读内核源码**
四大码仓中文注解 . 定期同步官方代码

[![WeHarmony/kernel_liteos_a_note](https://gitee.com/weharmony/kernel_liteos_a_note/widgets/widget_card.svg?colors=4183c4,ffffff,ffffff,e3e9ed,666666,9b9b9b)](https://gitee.com/weharmony/kernel_liteos_a_note)

鸿蒙研究站( weharmonyos ) | 每天死磕一点点，原创不易，欢迎转载，请注明出处。若能支持点赞更好，感谢每一份支持。
#### **关注不迷路.代码即人生**
![鸿蒙内核源码分析](https://gitee.com/weharmony/docs/raw/master/pic/other/so1so.png)

**QQ群 790015635 | 入群密码 666**
