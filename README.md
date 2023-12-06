
[![](https://weharmonyos.oss-cn-hangzhou.aliyuncs.com/resources/common/io.png)](http://weharmonyos.com/)

**中文注解鸿蒙内核 | [kernel_liteos_a_note](https://gitee.com/weharmony/kernel_liteos_a_note)** 是在 `OpenHarmony` 的 [kernel_liteos_a](https://gitee.com/openharmony/kernel_liteos_a) 基础上给内核源码加上中文注解的版本，同步官方代码迭代推进。

## 为何要精读内核源码？
* 码农的学职生涯，都应精读一遍内核源码。以浇筑好计算机知识大厦的地基，地基纵深的坚固程度，很大程度能决定未来大厦能盖多高。那为何一定要精读细品呢？
* 因为内核代码本身并不太多，都是浓缩的精华，精读是让各个知识点高频出现，不孤立成点状记忆，没有足够连接点的知识点是很容易忘的，点点成线，线面成体，连接越多，记得越牢，如此短时间内容易结成一张高浓度，高密度的系统化知识网，训练大脑肌肉记忆，驻入大脑直觉区，想抹都抹不掉，终生携带，随时调取。跟骑单车一样，一旦学会，即便多年不骑，照样跨上就走，游刃有余。
## 热爱是所有的理由和答案
* 因大学时阅读 linux 2.6 内核痛并快乐的经历，一直有个心愿，对底层基础技术进行一次系统性的整理，方便自己随时翻看，同时让更多对底层感兴趣的小伙伴减少时间，加速对计算机系统级的理解，而不至于过早的放弃。但因过程种种，多年一直没有行动，基本要放弃这件事了。恰逢 2020/9/10 鸿蒙正式开源，重新激活了多年的心愿，就有那么点如黄河之水一发不可收拾了。

* 包含三部分内容：**注源**，**写博** ，**画图**， 目前对内核源码的注解完成 80% ，博客分析完成80+篇，百图画鸿蒙完成20张，空闲时间几乎被占用，时间不够用，但每天都很充实，连做梦鸿蒙系统都在鱼贯而入。是件很有挑战的事，时间单位以年计，已持续一年半，期间得到众多小伙伴的支持与纠错，在此谢过 ! :P
## (〃･ิ‿･ิ)ゞ鸿蒙内核开发者
* 感谢开放原子开源基金会，致敬鸿蒙内核开发者提供了如此优秀的源码，一了多年的夙愿，津津乐道于此。从内核一行行的代码中能深深感受到开发者各中艰辛与坚持，及鸿蒙生态对未来的价值，这些是张嘴就来的网络喷子们永远不能体会到的。可以毫不夸张的说鸿蒙内核源码可作为大学：C语言，数据结构，操作系统，汇编语言，计算机系统结构，计算机组成原理，微机接口 七门课程的教学项目。如此宝库，不深入研究实在是暴殄天物，于心不忍，坚信鸿蒙大势所趋，未来可期，其必定成功，也必然成功，誓做其坚定的追随者和传播者。
  
## 理解内核的三个层级

* 普通概念映射级：这一级不涉及专业知识，用大众所熟知的公共认知就能听明白是个什么概念，也就是说用一个普通人都懂的概念去诠释或者映射一个他们从没听过的概念。让陌生的知识点与大脑中烂熟于心的知识点建立多重链接，加深记忆。说别人能听得懂的话这很重要。一个没学过计算机知识的卖菜大妈就不可能知道内核的基本运作了吗？不一定。在系列篇中试图用故事，打比方，去引导这一层级的认知，希望能卷入更多的人来关注基础软件，人多了场子热起来了创新就来了。
* 专业概念抽象级：对抽象的专业逻辑概念具体化认知， 比如虚拟内存，老百姓是听不懂的，学过计算机的人都懂，具体怎么实现的很多人又都不懂了，但这并不妨碍成为一个优秀的上层应用开发者，因为虚拟内存已经被抽象出来，目的是要屏蔽上层对它具体实现的认知。试图用百篇博客系列篇去拆解那些已经被抽象出来的专业概念， 希望能卷入更多对内核感兴趣的应用软件人才流入基础软硬件生态， 应用软件咱们是无敌宇宙，但基础软件却很薄弱。
* 具体微观代码级：这一级是具体到每一行代码的实现，到了用代码指令级的地步，这段代码是什么意思？为什么要这么设计？有没有更好的方案？[鸿蒙内核源码注解分析](https://gitee.com/weharmony/kernel_liteos_a_note) 试图从细微处去解释代码实现层，英文真的是天生适合设计成编程语言的人类语言，计算机的01码映射到人类世界的26个字母，诞生了太多的伟大奇迹。但我们的母语注定了很大部分人存在着自然语言层级的理解映射，希望内核注解分析能让更多爱好者节约时间成本，哪怕节约一分钟也是这件事莫大的意义。

## 四个维度解剖内核
为了全方位剖析内核，在 **画图**，**写文**，**注源**，**成册** 四个方向做了努力，试图以**讲故事**，**画图表**，**写文档**，**拆源码** 立体的方式表述内核。很喜欢易中天老师的一句话:研究方式不等于表述方式。底层技术并不枯燥，它可以很有意思，它可以是我们生活中的场景。

### 一： 百图画鸿蒙 | 一图一主干 | 骨骼系统
* 如果把鸿蒙比作人，百图目的是要画出其骨骼系统。
* 百图系列每张图都是心血之作，耗时甚大，能用一张就绝不用两张，所以会画的比较复杂，高清图会很大，可在公众号中回复 **百图** 获取`3`倍超高清最新图。`v**.xx`代表图的版本，请留意图的更新。
* 例如： **双向链表** 是内核最重要的结构体，站长更愿意将它比喻成人的左右手，其意义是通过寄生在宿主结构体上来体现，可想象成在宿主结构体装上一对对勤劳的双手，它真的很会来事，超级活跃分子，为宿主到处拉朋友，建圈子。其插入 | 删除 | 遍历操作是它最常用的社交三大件，若不理解透彻在分析源码过程中很容易卡壳。虽在网上能找到很多它的图,但怎么看都不是自己想要的，干脆重画了它的主要操作。
* ![](https://weharmonyos.oss-cn-hangzhou.aliyuncs.com/resources/100pic/1_list.png) 

### 二： 百文说内核 | 抓住主脉络 | 肌肉器官

* 百文相当于摸出内核的肌肉和器官系统，让人开始丰满有立体感，因是直接从注释源码起步，在加注释过程中，每每有心得处就整理,慢慢形成了以下文章。内容立足源码，常以生活场景打比方尽可能多的将内核知识点置入某种场景，具有画面感，容易理解记忆。说别人能听得懂的话很重要! 百篇博客绝不是百度教条式的在说一堆诘屈聱牙的概念，那没什么意思。更希望让内核变得栩栩如生，倍感亲切。
* 与代码需不断`debug`一样，文章内容会存在不少错漏之处，请多包涵，但会反复修正，持续更新，`v**.xx` 代表文章序号和修改的次数，精雕细琢，言简意赅，力求打造精品内容。
* 百文在 < 鸿蒙研究站 | 开源中国 | 博客园 | 51cto | csdn | 知乎 | 掘金 > 站点发布，公众号回复 **百文** 可方便阅读。
  
* ![](https://weharmonyos.oss-cn-hangzhou.aliyuncs.com/resources/common/cate.png)
  

**基础知识** 
* [v01.12 鸿蒙内核源码分析(双向链表) | 谁是内核最重要结构体](http://weharmonyos.com/blog/01.html)
* [v02.01 鸿蒙内核源码分析(内核概念) | 名不正则言不顺](http://weharmonyos.com/blog/02.html)
* [v03.02 鸿蒙内核源码分析(源码结构) | 宏观尺度看内核结构](http://weharmonyos.com/blog/03.html)
* [v04.01 鸿蒙内核源码分析(地址空间) | 内核如何看待空间](http://weharmonyos.com/blog/04.html)
* [v05.03 鸿蒙内核源码分析(计时单位) | 内核如何看待时间](http://weharmonyos.com/blog/05.html)
* [v06.01 鸿蒙内核源码分析(优雅的宏) | 编译器也喜欢复制粘贴 ](http://weharmonyos.com/blog/06.html)
* [v07.01 鸿蒙内核源码分析(钩子框架) | 万物皆可HOOK ](http://weharmonyos.com/blog/07.html)
* [v08.04 鸿蒙内核源码分析(位图管理) | 一分钱被掰成八半使用](http://weharmonyos.com/blog/08.html)
* [v09.01 鸿蒙内核源码分析(POSIX) | 操作系统界的话事人 ](http://weharmonyos.com/blog/09.html)
* [v10.01 鸿蒙内核源码分析(main函数) | 要走了无数码农的第一次 ](http://weharmonyos.com/blog/10.html)

**进程管理** 
* [v11.04 鸿蒙内核源码分析(调度故事) | 大郎，该喝药了](http://weharmonyos.com/blog/11.html)
* [v12.03 鸿蒙内核源码分析(进程控制块) | 可怜天下父母心](http://weharmonyos.com/blog/12.html)
* [v13.01 鸿蒙内核源码分析(进程空间) | 有爱的地方才叫家 ](http://weharmonyos.com/blog/13.html)
* [v14.01 鸿蒙内核源码分析(线性区) | 人要有空间才能好好相处](http://weharmonyos.com/blog/14.html)
* [v15.01 鸿蒙内核源码分析(红黑树) | 众里寻他千百度 ](http://weharmonyos.com/blog/15.html)
* [v16.06 鸿蒙内核源码分析(进程管理) | 家家有本难念的经](http://weharmonyos.com/blog/16.html)
* [v17.05 鸿蒙内核源码分析(Fork进程) | 一次调用 两次返回](http://weharmonyos.com/blog/17.html)
* [v18.02 鸿蒙内核源码分析(进程回收) | 临终托孤的短命娃](http://weharmonyos.com/blog/18.html)
* [v19.03 鸿蒙内核源码分析(Shell编辑) | 两个任务 三个阶段](http://weharmonyos.com/blog/19.html)
* [v20.01 鸿蒙内核源码分析(Shell解析) | 应用窥伺内核的窗口](http://weharmonyos.com/blog/20.html)

**任务管理** 
* [v21.07 鸿蒙内核源码分析(任务控制块) | 内核最重要的概念](http://weharmonyos.com/blog/21.html)
* [v22.05 鸿蒙内核源码分析(并发并行) | 如何搞清楚它俩区分](http://weharmonyos.com/blog/22.html)
* [v23.03 鸿蒙内核源码分析(就绪队列) | 美好的事物永远值得等待](http://weharmonyos.com/blog/23.html)
* [v24.08 鸿蒙内核源码分析(调度机制) | 公平是相对的](http://weharmonyos.com/blog/24.html)
* [v25.05 鸿蒙内核源码分析(任务管理) | 如何管理任务池](http://weharmonyos.com/blog/25.html)
* [v26.03 鸿蒙内核源码分析(用栈方式) | 谁来提供程序运行场地](http://weharmonyos.com/blog/26.html)
* [v27.02 鸿蒙内核源码分析(软件定时器) | 内核最高级任务竟是它](http://weharmonyos.com/blog/27.html)
* [v28.01 鸿蒙内核源码分析(控制台) | 一个让很多人模糊的概念](http://weharmonyos.com/blog/28.html)
* [v29.01 鸿蒙内核源码分析(远程登录) | 内核如何接待远方的客人](http://weharmonyos.com/blog/29.html)
* [v30.01 鸿蒙内核源码分析(协议栈) | 正在制作中 ... ](http://weharmonyos.com/blog/30.html)

**内存管理** 
* [v31.02 鸿蒙内核源码分析(内存规则) | 内存管理到底在管什么](http://weharmonyos.com/blog/31.html)
* [v32.04 鸿蒙内核源码分析(物理内存) | 真实的可不一定精彩](http://weharmonyos.com/blog/32.html)
* [v33.04 鸿蒙内核源码分析(内存概念) | RAM & ROM & Flash](http://weharmonyos.com/blog/33.html)
* [v34.03 鸿蒙内核源码分析(虚实映射) | 映射是伟大的发明](http://weharmonyos.com/blog/34.html)
* [v35.02 鸿蒙内核源码分析(页表管理) | 映射关系保存在哪](http://weharmonyos.com/blog/35.html)
* [v36.03 鸿蒙内核源码分析(静态分配) | 很简单的一位小朋友](http://weharmonyos.com/blog/36.html)
* [v37.01 鸿蒙内核源码分析(TLFS算法) | 图表解读TLFS原理 ](http://weharmonyos.com/blog/37.html)
* [v38.01 鸿蒙内核源码分析(内存池管理) | 如何高效切割合并内存块 ](http://weharmonyos.com/blog/38.html)
* [v39.04 鸿蒙内核源码分析(原子操作) | 谁在守护指令执行的完整性](http://weharmonyos.com/blog/39.html)
* [v40.01 鸿蒙内核源码分析(圆整对齐) | 正在制作中 ... ](http://weharmonyos.com/blog/40.html)

**通讯机制** 
* [v41.04 鸿蒙内核源码分析(通讯总览) | 内核跟人一样都喜欢八卦](http://weharmonyos.com/blog/41.html)
* [v42.08 鸿蒙内核源码分析(自旋锁) | 死等丈夫归来的贞洁烈女](http://weharmonyos.com/blog/42.html)
* [v43.05 鸿蒙内核源码分析(互斥锁) | 有你没她 相安无事](http://weharmonyos.com/blog/43.html)
* [v44.02 鸿蒙内核源码分析(快锁使用) | 用户态负责快锁逻辑](http://weharmonyos.com/blog/44.html)
* [v45.02 鸿蒙内核源码分析(快锁实现) | 内核态负责快锁调度](http://weharmonyos.com/blog/45.html)
* [v46.01 鸿蒙内核源码分析(读写锁) | 内核如何实现多读单写](http://weharmonyos.com/blog/46.html)
* [v47.05 鸿蒙内核源码分析(信号量) | 谁在解决任务间的同步](http://weharmonyos.com/blog/47.html)
* [v48.07 鸿蒙内核源码分析(事件机制) | 多对多任务如何同步](http://weharmonyos.com/blog/48.html)
* [v49.05 鸿蒙内核源码分析(信号生产) | 年过半百 活力十足](http://weharmonyos.com/blog/49.html)
* [v50.03 鸿蒙内核源码分析(信号消费) | 谁让CPU连续四次换栈运行](http://weharmonyos.com/blog/50.html)
* [v51.03 鸿蒙内核源码分析(消息队列) | 进程间如何异步传递大数据](http://weharmonyos.com/blog/51.html)
* [v52.02 鸿蒙内核源码分析(消息封装) | 剖析LiteIpc(上)进程通讯内容](http://weharmonyos.com/blog/52.html)
* [v53.01 鸿蒙内核源码分析(消息映射) | 剖析LiteIpc(下)进程通讯机制](http://weharmonyos.com/blog/53.html)
* [v54.01 鸿蒙内核源码分析(共享内存) | 进程间最快通讯方式](http://weharmonyos.com/blog/54.html)

**文件系统** 
* [v55.02 鸿蒙内核源码分析(文件概念) | 为什么说一切皆是文件](http://weharmonyos.com/blog/55.html)
* [v56.04 鸿蒙内核源码分析(文件故事) | 用图书管理说文件系统](http://weharmonyos.com/blog/56.html)
* [v57.06 鸿蒙内核源码分析(索引节点) | 谁是文件系统最重要的概念](http://weharmonyos.com/blog/57.html)
* [v58.02 鸿蒙内核源码分析(VFS) | 文件系统的话事人](http://weharmonyos.com/blog/58.html)
* [v59.04 鸿蒙内核源码分析(文件句柄) | 你为什么叫句柄](http://weharmonyos.com/blog/59.html)
* [v60.07 鸿蒙内核源码分析(根文件系统) | 谁先挂到`/`谁就是老大](http://weharmonyos.com/blog/60.html)
* [v61.05 鸿蒙内核源码分析(挂载机制) | 谁根逐流不掉队](http://weharmonyos.com/blog/61.html)
* [v62.05 鸿蒙内核源码分析(管道文件) | 如何降低数据流动成本](http://weharmonyos.com/blog/62.html)
* [v63.03 鸿蒙内核源码分析(文件映射) | 正在制作中 ... ](http://weharmonyos.com/blog/63.html)
* [v64.01 鸿蒙内核源码分析(写时拷贝) | 正在制作中 ... ](http://weharmonyos.com/blog/64.html)

**硬件架构** 
* [v65.01 鸿蒙内核源码分析(芯片模式) | 回顾芯片行业各位大佬](http://weharmonyos.com/blog/65.html)
* [v66.03 鸿蒙内核源码分析(ARM架构) | ARMv7 & Cortex(A|R|M)](http://weharmonyos.com/blog/66.html)
* [v67.01 鸿蒙内核源码分析(指令集) | CICS PK RICS](http://weharmonyos.com/blog/67.html)
* [v68.01 鸿蒙内核源码分析(协处理器) | CPU的好帮手 ](http://weharmonyos.com/blog/68.html)
* [v69.05 鸿蒙内核源码分析(工作模式) | 角色不同 责任不同](http://weharmonyos.com/blog/69.html)
* [v70.06 鸿蒙内核源码分析(寄存器) | 世界被它们玩出了花](http://weharmonyos.com/blog/70.html)
* [v71.03 鸿蒙内核源码分析(多核管理) | 并发真正的基础](http://weharmonyos.com/blog/71.html)
* [v72.05 鸿蒙内核源码分析(中断概念) | 海公公的日常工作](http://weharmonyos.com/blog/72.html)
* [v73.04 鸿蒙内核源码分析(中断管理) | 没中断太可怕](http://weharmonyos.com/blog/73.html)

**内核汇编** 
* [v74.01 鸿蒙内核源码分析(编码方式) | 机器指令是如何编码的 ](http://weharmonyos.com/blog/74.html)
* [v75.03 鸿蒙内核源码分析(汇编基础) | CPU上班也要打卡](http://weharmonyos.com/blog/75.html)
* [v76.04 鸿蒙内核源码分析(汇编传参) | 如何传递复杂的参数](http://weharmonyos.com/blog/76.html)
* [v77.01 鸿蒙内核源码分析(链接脚本) | 正在制作中 ... ](http://weharmonyos.com/blog/77.html)
* [v78.01 鸿蒙内核源码分析(内核启动) | 从汇编到main()](http://weharmonyos.com/blog/78.html)
* [v79.01 鸿蒙内核源码分析(进程切换) | 正在制作中 ... ](http://weharmonyos.com/blog/79.html)
* [v80.03 鸿蒙内核源码分析(任务切换) | 看汇编如何切换任务](http://weharmonyos.com/blog/80.html)
* [v81.05 鸿蒙内核源码分析(中断切换) | 系统因中断活力四射](http://weharmonyos.com/blog/81.html)
* [v82.06 鸿蒙内核源码分析(异常接管) | 社会很单纯 复杂的是人](http://weharmonyos.com/blog/82.html)
* [v83.01 鸿蒙内核源码分析(缺页中断) | 正在制作中 ... ](http://weharmonyos.com/blog/83.html)

**编译运行** 
* [v84.02 鸿蒙内核源码分析(编译过程) | 简单案例说透中间过程](http://weharmonyos.com/blog/84.html)
* [v85.03 鸿蒙内核源码分析(编译构建) | 编译鸿蒙防掉坑指南](http://weharmonyos.com/blog/85.html)
* [v86.04 鸿蒙内核源码分析(GN语法) | 如何构建鸿蒙系统](http://weharmonyos.com/blog/86.html)
* [v87.03 鸿蒙内核源码分析(忍者无敌) | 忍者的特点就是一个字](http://weharmonyos.com/blog/87.html)
* [v88.04 鸿蒙内核源码分析(ELF格式) | 应用程序入口并非main](http://weharmonyos.com/blog/88.html)
* [v89.03 鸿蒙内核源码分析(ELF解析) | 敢忘了她姐俩你就不是银](http://weharmonyos.com/blog/89.html)
* [v90.04 鸿蒙内核源码分析(静态链接) | 一个小项目看中间过程](http://weharmonyos.com/blog/90.html)
* [v91.04 鸿蒙内核源码分析(重定位) | 与国际接轨的对外发言人](http://weharmonyos.com/blog/91.html)
* [v92.01 鸿蒙内核源码分析(动态链接) | 正在制作中 ... ](http://weharmonyos.com/blog/92.html)
* [v93.05 鸿蒙内核源码分析(进程映像) | 程序是如何被加载运行的](http://weharmonyos.com/blog/93.html)
* [v94.01 鸿蒙内核源码分析(应用启动) | 正在制作中 ... ](http://weharmonyos.com/blog/94.html)
* [v95.06 鸿蒙内核源码分析(系统调用) | 开发者永远的口头禅](http://weharmonyos.com/blog/95.html)
* [v96.01 鸿蒙内核源码分析(VDSO) | 正在制作中 ... ](http://weharmonyos.com/blog/96.html)

**调测工具** 
* [v97.01 鸿蒙内核源码分析(模块监控) | 正在制作中 ... ](http://weharmonyos.com/blog/97.html)
* [v98.01 鸿蒙内核源码分析(日志跟踪) | 正在制作中 ... ](http://weharmonyos.com/blog/98.html)
* [v99.01 鸿蒙内核源码分析(系统安全) | 正在制作中 ... ](http://weharmonyos.com/blog/99.html)
* [v100.01 鸿蒙内核源码分析(测试用例) | 正在制作中 ... ](http://weharmonyos.com/blog/100.html)

**前因后果** 
* [v101.03 鸿蒙内核源码分析(总目录) | 精雕细琢 锤炼精品](http://weharmonyos.com/blog/101.html)
* [v102.05 鸿蒙内核源码分析(源码注释) | 每天死磕一点点](http://weharmonyos.com/blog/102.html)
* [v103.05 鸿蒙内核源码分析(静态站点) | 码农都不爱写注释和文档](http://weharmonyos.com/blog/103.html)
* [v104.01 鸿蒙内核源码分析(参考手册) | 阅读内核源码必备工具](http://weharmonyos.com/blog/104.html)

### 三： 百万注内核 | 处处扣细节 | 细胞血管
* 百万汉字注解内核目的是要看清楚其毛细血管，细胞结构，等于在拿放大镜看内核。内核并不神秘，带着问题去源码中找答案是很容易上瘾的，你会发现很多文章对一些问题的解读是错误的，或者说不深刻难以自圆其说，你会慢慢形成自己新的解读，而新的解读又会碰到新的问题，如此层层递进，滚滚向前，拿着放大镜根本不愿意放手。
* 因鸿蒙内核6W+代码量，本身只有较少的注释， 中文注解以不对原有代码侵入为前提，源码中所有英文部分都是原有注释，所有中文部分都是中文版的注释，同时为方便同步官方版本的更新，尽量不去增加代码的行数，不破坏文件的结构，注释多类似以下的方式:

  * 在重要模块的`.c/.h`文件开始位置先对模块功能做整体的介绍，例如异常接管模块注解如图所示:
  
    ![](https://weharmonyos.oss-cn-hangzhou.aliyuncs.com/resources/13/ycjg.png)
    注解过程中查阅了很多的资料和书籍，在具体代码处都附上了参考链接。
  * 绘制字符图帮助理解模块 ，例如 虚拟内存区域分布没有图很难理解。
    ![](https://weharmonyos.oss-cn-hangzhou.aliyuncs.com/resources/13/vm.png)
  * 而函数级注解会详细到重点行，甚至每一行， 例如申请互斥锁的主体函数，不可谓不重要，而官方注释仅有一行，如图所示
    
    ![](https://weharmonyos.oss-cn-hangzhou.aliyuncs.com/resources/13/sop.png)

* 注解创建了一些特殊记号，可直接搜索查看
  - [x] 搜索 `@note_pic` 可查看绘制的全部字符图
  - [x] 搜索 `@note_why` 是尚未看明白的地方，有看明白的，请[新建 Pull Request](https://gitee.com/weharmony/kernel_liteos_a_note/pull/new/weharmony:master...weharmony:master)完善
  - [x] 搜索 `@note_thinking` 是一些的思考和建议
  - [x] 搜索 `@note_#if0` 是由第三方项目提供不在内核源码中定义的极为重要结构体，为方便理解而添加的。
  - [x] 搜索 `@note_link` 是网址链接，方便理解模块信息，来源于官方文档，百篇博客，外部链接
  - [x] 搜索 `@note_good` 是给源码点赞的地方

### 四： 参考手册 | Doxygen呈现 | 诊断

在中文加注版基础上构建了参考手册，如此可以看到毛细血管级的网络图，注解支持 [doxygen](https://www.doxygen.nl) 格式标准。
* 图为内核`main`的调用关系直观展现，如果没有这张图，光`main`一个函数就够喝一壶。 `main`本身是由汇编指令 `bl main`调用
  ![](https://weharmonyos.oss-cn-hangzhou.aliyuncs.com/resources/73/1.png)
  可前往 >> [鸿蒙研究站 | 参考手册 ](http://doxygen.weharmonyos.com/index.html) 体验

* 图为内核所有结构体索引，点击可查看每个结构变量细节
  ![](https://weharmonyos.oss-cn-hangzhou.aliyuncs.com/resources/73/6.png)
  可前往 >> [鸿蒙研究站 | 结构体索引 ](http://doxygen.weharmonyos.com/classes.html) 体验

## 鸿蒙论坛 | 干净.营养.不盲从
* 搭个论坛貌似不合时宜, 但站长却固执的认为它是技术人最好的沟通方式, 它不像群各种叨絮使人焦虑被逼的屏蔽它, 它更像个异性知己,懂你给你留足空间思考,从不扰乱你的生活,鸿蒙论坛会一直存在,并坚持自己的风格(干净.营养.不盲从)。选择 Discuz 是因为它太优秀, 一个沉淀了20年的开源平台,被所谓的时代遗忘实在是太过可惜。哪天您得空了就去逛逛吧 , 它可能并没有那么糟糕。
*  [![](https://weharmonyos.oss-cn-hangzhou.aliyuncs.com/resources/bbs/bbs.png)](http://bbs.weharmonyos.com)
  
## 四大码仓发布 | 源码同步官方
内核注解同时在 [gitee](https://gitee.com/weharmony/kernel_liteos_a_note) | [github](https://github.com/kuangyufei/kernel_liteos_a_note) | [coding](https://weharmony.coding.net/public/harmony/kernel_liteos_a_note/git/files) | [gitcode](https://gitcode.net/kuangyufei/kernel_liteos_a_note) 发布，并与官方源码按月保持同步，同步历史如下:
* `2023/11/24` -- 几处小的修改
* `2023/10/11` -- 近五个月官方很少更新
* `2023/05/26` -- BUILD.gn 相关
* `2023/04/10` -- 调度算法优化，加入deadline
* `2023/03/01` -- 增加网络容器和容器限额功能
* `2023/02/13` -- 支持proc/self目录
* `2023/01/14` -- 同步官方代码，支持PID容器
* `2022/11/01` -- 删除 PLATFORM_QEMU_ARM_VIRT_CA7 侵入内核的所有代码
* `2022/09/21` -- 主线代码没有变化,只完善了测试用例
* `2022/07/18` -- 开机代码微调
* `2022/06/03` -- 增加 jffs2 编译选项
* `2022/05/09` -- 标准库(musl , newlib) 目录调整
* `2022/04/16` -- 任务调度模块有很大更新
* `2022/03/23` -- 新增各CPU核自主管理中断, 定时器模块较大调整
* `2022/02/18` -- 官方无代码更新, 只有测试用例的完善
* `2022/01/20` -- 同步官方代码,本次官方对测试用例和MMU做了较大调整
* `2021/12/20` -- 增加`LMS`模块，完善`PM，Fat Cache`
* `2021/11/12` -- 加入`epoll`支持，对`shell`模块有较大调整，微调`process`，`task`，更正单词拼写错误
* `2021/10/21` -- 增加性能优化模块`perf`，优化了文件映射模块
* `2021/09/14` -- `common`，`extended`等几个目录结构和Makefile调整
* `2021/08/19` -- 各目录增加了`BUILD。gn`文件，文件系统部分文件调整 
* `2021/07/15` -- 改动不大，新增`blackbox`，`hidumper`，对一些宏规范化使用 
* `2021/06/27` -- 对文件系统/设备驱动改动较大，目录结构进行了重新整理
* `2021/06/08` -- 对编译构建，任务，信号模块有较大的改动
* `2021/05/28` -- 改动不大，主要针对一些错误单词拼写纠正
* `2021/05/13` -- 对系统调用，任务切换，信号处理，异常接管，文件管理，`shell`做了较大更新，代码结构更清晰
* `2021/04/21` -- 官方优化了很多之前吐槽的地方，点赞
* `2020/09/16` -- 中文注解版起点

## 注解子系统仓库
  
在给鸿蒙内核源码加注过程中发现仅仅注解内核仓库还不够，因为它关联了其他子系统，若对这些子系统不了解是很难完整的注解鸿蒙内核，所以也对这些关联仓库进行了部分注解，这些仓库包括:

* [编译构建子系统 | build_lite](https://gitee.com/weharmony/build_lite_note)  
* [协议栈 | lwip](https://gitee.com/weharmony/third_party_lwip) 
* [文件系统 | NuttX](https://gitee.com/weharmony/third_party_NuttX)
* [标准库 | musl](https://gitee.com/weharmony/third_party_musl) 

## 关于 zzz 目录
中文加注版比官方版无新增文件，只多了一个`zzz`的目录，里面放了一些加注所需文件，它与内核代码无关，可以忽略它，取名`zzz`是为了排在最后，减少对原有代码目录级的侵入，`zzz` 的想法源于微信中名称为`AAA`的那帮朋友，你的微信里应该也有他们熟悉的身影吧 :-)
```
/kernel/liteos_a_note
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
├── figures                # 内核架构图
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
├── security               # 安全特性相关的代码，包括进程权限管理和虚拟id映射管理
├── shell                  # 接收用户输入的命令，内核去执行
├── syscall                # 系统调用
├── testsuilts             # 测试套件
├── tools                  # 构建工具及相关配置和代码
└── zzz                    # 中文注解版新增目录
```
## 官方文档 | 静态站点呈现

* 研究鸿蒙需不断的翻阅资料，吸取精华，其中官方文档必不可少， 为更好的呈现 **OpenHarmony开发文档** ， 特意做了静态站点 [ >> 鸿蒙研究站 | 官方文档](http://open.weharmonyos.com) 来方便查阅官方资料，与官方资料保持同步更新。

* 左侧导航栏，右边索引区
  ![](https://weharmonyos.oss-cn-hangzhou.aliyuncs.com/resources/52/4.png)

## 任正非演讲稿【1994-2019】
* 咱们搞技术的光搞好技术还不够，得学点管理，读点大师的文章，任总无疑是目前国内最伟大的企业家，读他的文章，可以让您少奋斗十年，我只恨自己读到的太晚，白白浪费了大好时光，痛定思痛，整理了任总历年的文章，共440余篇供您阅读，也可以扫码购买一本纸质书，一来能方便阅读，二来也支持下作者，放在床头，时刻进步，只需您一顿饭钱
* [>> 获取任正非演讲稿纸质书440余篇（附赠电子版）](https://weharmonyos.oss-cn-hangzhou.aliyuncs.com/resources/huawei/mini69.png)
* [总目录 | 奋斗者永远年轻](http://weharmonyos.com/ren/sum.html)
* [读任总讲话 少奋斗十年](http://weharmonyos.com/ren/)
* [从任正非思想看华为研发管理变革成长之路](http://weharmonyos.com/ren/R&D.html)
### 1994年
* [赴美考察散记](http://weharmonyos.com/ren/1994/19940118_赴美考察散记.html)
* [团结奋斗 再创华为佳绩](http://weharmonyos.com/ren/1994/19940126_团结奋斗_再创华为佳绩.html)
* [胜利祝酒辞](http://weharmonyos.com/ren/1994/19940605_胜利祝酒辞.html)
* [对中国农话网与交换机产业的一点看法](http://weharmonyos.com/ren/1994/19940621_对中国农话网与交换机产业的一点看法.html)
* [从二则空难事故看员工培训的重要性](http://weharmonyos.com/ren/1994/19941225_从二则空难事故看员工培训的重要性.html)
* [致新员工书](http://weharmonyos.com/ren/1994/19941225_致新员工书.html)

### 1995年
* [坚定不移地坚持发展的方向](http://weharmonyos.com/ren/1995/19950109_坚定不移地坚持发展的方向.html)
* [不前进就免职](http://weharmonyos.com/ren/1995/19950110_不前进就免职.html)
* [励精图治 再创辉煌](http://weharmonyos.com/ren/1995/19950110_励精图治，再创辉煌.html)
* [英雄好汉站出来](http://weharmonyos.com/ren/1995/19950110_英雄好汉站出来.html)
* [上海电话信息技术和业务管理研讨会致谢词](http://weharmonyos.com/ren/1995/19950618_上海电话信息技术和业务管理研讨会致谢词.html)
* [要建立一个均衡的平台](http://weharmonyos.com/ren/1995/199507_要建立一个均衡的平台.html)
* [在第四届国际电子通信展华为庆祝酒会的发言](http://weharmonyos.com/ren/1995/19951116_在第四届国际电子通信展华为庆祝酒会的发言.html)
* [解放思想，迎接96年市场大战](http://weharmonyos.com/ren/1995/19951118_解放思想，迎接96年市场大战.html)
* [目前我们的形势和任务](http://weharmonyos.com/ren/1995/19951226_目前我们的形势和任务.html)

### 1996年
* [当干部是一种责任](http://weharmonyos.com/ren/1996/19960128_当干部是一种责任.html)
* [我们要向市场、开发、创造性工作倾斜](http://weharmonyos.com/ren/1996/19960402_我们要向市场、开发、创造性工作倾斜.html)
* [反骄破满，在思想上艰苦奋斗](http://weharmonyos.com/ren/1996/19960406_反骄破满，在思想上艰苦奋斗.html)
* [加强合作走向世界](http://weharmonyos.com/ren/1996/19960502_加强合作走向世界.html)
* [要树立服务意识、品牌意识、群体意识](http://weharmonyos.com/ren/1996/19960608_要树立服务意识、品牌意识、群体意识.html)
* [再论反骄破满，在思想上艰苦奋斗](http://weharmonyos.com/ren/1996/19960630_再论反骄破满，在思想上艰苦奋斗.html)
* [我们是要向前迈进一小步，而不是一次大飞跃](http://weharmonyos.com/ren/1996/19960725_我们是要向前迈进一小步，而不是一次大飞跃.html)
* [胜负无定数，敢搏成七分](http://weharmonyos.com/ren/1996/19960811_胜负无定数，敢搏成七分.html)
* [秘书体系是信息桥](http://weharmonyos.com/ren/1996/19960828_秘书体系是信息桥.html)
* [赴俄参展杂记](http://weharmonyos.com/ren/1996/19960828_赴俄参展杂记.html)
* [做好基础工作，逐步实现全面质量管理](http://weharmonyos.com/ren/1996/19961113_做好基础工作，逐步实现全面质量管理.html)
* [实行低重心管理，层层级级都要在做实上下功夫](http://weharmonyos.com/ren/1996/19961115_实行低重心管理，层层级级都要在做实上下功夫.html)
* [培训——通向华为明天的重要阶梯](http://weharmonyos.com/ren/1996/19961121_培训——通向华为明天的重要阶梯.html)
* [管理改革，任重道远](http://weharmonyos.com/ren/1996/19961202_管理改革，任重道远.html)
* [坚持顾客导向同步世界潮流](http://weharmonyos.com/ren/1996/19961213_坚持顾客导向同步世界潮流.html)
* [团结起来接受挑战，克服自我溶入大我](http://weharmonyos.com/ren/1996/19961228_团结起来接受挑战，克服自我溶入大我.html)
* [不要叶公好龙](http://weharmonyos.com/ren/1996/1996_不要叶公好龙.html)

### 1997年
* [不要忘记英雄](http://weharmonyos.com/ren/1997/19970123_不要忘记英雄.html)
* [资源是会枯竭的，唯有文化才能生生不息](http://weharmonyos.com/ren/1997/19970207_资源是会枯竭的，唯有文化才能生生不息.html)
* [加强用户服务中心建设，不断提高用户服务水平](http://weharmonyos.com/ren/1997/19970217_加强用户服务中心建设，不断提高用户服务水平.html)
* [秘书如何定位](http://weharmonyos.com/ren/1997/19970222_秘书如何定位.html)
* [坚定不移地推行ISO9000](http://weharmonyos.com/ren/1997/19970226_坚定不移地推行ISO9000.html)
* [胜则举杯相庆 败则拼死相救](http://weharmonyos.com/ren/1997/19970226_胜则举杯相庆败则拼死相救.html)
* [悼念杨琳](http://weharmonyos.com/ren/1997/19970330_悼念杨琳.html)
* [自强不息，荣辱与共，促进管理的进步](http://weharmonyos.com/ren/1997/19970410_自强不息，荣辱与共，促进管理的进步.html)
* [谈学习](http://weharmonyos.com/ren/1997/19970428_谈学习.html)
* [走过亚欧分界线](http://weharmonyos.com/ren/1997/19970428_走过亚欧分界线.html)
* [加强夏收管理 促进增产增收](http://weharmonyos.com/ren/1997/199705_加强夏收管理促进增产增收.html)
* [呼唤英雄](http://weharmonyos.com/ren/1997/19970626_呼唤英雄.html)
* [为提高电信网营运水平而努力](http://weharmonyos.com/ren/1997/19970715_为提高电信网营运水平而努力.html)
* [提升自我，找到切入点，迎接人生新挑战](http://weharmonyos.com/ren/1997/19970912_提升自我，找到切入点，迎接人生新挑战.html)
* [当代青年怎样爱国](http://weharmonyos.com/ren/1997/19970916_当代青年怎样爱国.html)
* [在《委员会管理法》评审上的重要讲话](http://weharmonyos.com/ren/1997/1997_在《委员会管理法》评审上的重要讲话.html)
* [建立一个适应企业生存发展的组织和机制](http://weharmonyos.com/ren/1997/1997_建立一个适应企业生存发展的组织和机制.html)
* [我们需要什么样的干部](http://weharmonyos.com/ren/1997/1997_我们需要什么样的干部.html)
* [苦练基本功，争做维护专家](http://weharmonyos.com/ren/1997/1997_苦练基本功，争做维护专家.html)

### 1998年
* [狭路相逢勇者生](http://weharmonyos.com/ren/1998/19980116_狭路相逢勇者生.html)
* [不做昙花一现的英雄](http://weharmonyos.com/ren/1998/1998012_不做昙花一现的英雄.html)
* [我们向美国人民学习什么](http://weharmonyos.com/ren/1998/19980220_我们向美国人民学习什么.html)
* [要从必然王国，走向自由王国](http://weharmonyos.com/ren/1998/19980328_要从必然王国，走向自由王国.html)
* [华为基本法](http://weharmonyos.com/ren/1998/199803_华为基本法.html)
* [在自我批判中进步](http://weharmonyos.com/ren/1998/199803_在自我批判中进步.html)
* [希望寄托在你们身上](http://weharmonyos.com/ren/1998/199805_希望寄托在你们身上.html)
* [华为的红旗到底能打多久](http://weharmonyos.com/ren/1998/19980620_华为的红旗到底能打多久.html)
* [小改进、大奖励](http://weharmonyos.com/ren/1998/19980716_小改进、大奖励.html)
* [全心全意对产品负责全心全意为客户服务](http://weharmonyos.com/ren/1998/199809_全心全意对产品负责全心全意为客户服务.html)
* [印度随笔](http://weharmonyos.com/ren/1998/19981209_印度随笔.html)

### 1999年
* [创业创新必须以提升企业核心竞争力为中心](http://weharmonyos.com/ren/1999/19990208_创业创新必须以提升企业核心竞争力为中心.html)
* [在IPD动员大会上的讲话](http://weharmonyos.com/ren/1999/19990417_在IPD动员大会上的讲话.html)
* [在实践中培养和选拔干部](http://weharmonyos.com/ren/1999/199904_在实践中培养和选拔干部.html)
* [“中国人今天说不”图片新闻感想](http://weharmonyos.com/ren/1999/19990520_“中国人今天说不”图片新闻感想.html)
* [能工巧匠是我们企业的宝贵财富](http://weharmonyos.com/ren/1999/19990520_能工巧匠是我们企业的宝贵财富.html)
* [答新员工问](http://weharmonyos.com/ren/1999/19990720_任正非答新员工问.html)

### 2000年
* [与身处逆境员工的对话录](http://weharmonyos.com/ren/2000/20000114_与身处逆境员工的对话录.html)
* [凤凰展翅再创辉煌](http://weharmonyos.com/ren/2000/20000128_凤凰展翅再创辉煌.html)
* [一个职业管理者的责任和使命](http://weharmonyos.com/ren/2000/20000320_一个职业管理者的责任和使命.html)
* [活下去，是企业的硬道理](http://weharmonyos.com/ren/2000/20000408_活下去，是企业的硬道理.html)
* [沟通从心灵开始](http://weharmonyos.com/ren/2000/20000717_沟通从心灵开始.html)
* [创新是华为发展的不竭动力](http://weharmonyos.com/ren/2000/20000720_创新是华为发展的不竭动力.html)
* [为什么要自我批判](http://weharmonyos.com/ren/2000/20000901_为什么要自我批判.html)
* [在秘书资格颁证大会上的讲话](http://weharmonyos.com/ren/2000/2000_任正非在秘书资格颁证大会上的讲话.html)

### 2001年
* [雄赳赳 气昂昂 跨过太平洋](http://weharmonyos.com/ren/2001/20010118_雄赳赳气昂昂跨过太平洋.html)
* [华为的冬天](http://weharmonyos.com/ren/2001/20010201_华为的冬天.html)
* [我的父亲母亲](http://weharmonyos.com/ren/2001/20010208_我的父亲母亲.html)
* [北国之春](http://weharmonyos.com/ren/2001/20010424_北国之春.html)

### 2002年
* [迎接挑战，苦练内功，迎接春天的到来](http://weharmonyos.com/ren/2002/2002_迎接挑战，苦练内功，迎接春天的到来.html)

### 2003年
* [在理性与平实中存活.md](http://weharmonyos.com/ren/2003/20030525_在理性与平实中存活.html)
* [产品发展的路标是客户需求导向,企业管理的目标是流程化的组织建设](http://weharmonyos.com/ren/2003/20030526_产品发展的路标是客户需求导向企业管理的目标是流程化的组织建设.html)
* [发挥核心团队作用，不断提高人均效益](http://weharmonyos.com/ren/2003/2003_发挥核心团队作用，不断提高人均效益.html)
* [在自我批判指导委员会座谈会上的讲话](http://weharmonyos.com/ren/2003/2003_在自我批判指导委员会座谈会上的讲话.html)

### 2004年
* [持续提高人均效益，建设高绩效企业文化](http://weharmonyos.com/ren/2004/20040115_持续提高人均效益建设高绩效企业文化.html)
* [华为公司的核心价值观](http://weharmonyos.com/ren/2004/20040428_华为公司的核心价值观.html)

### 2005年
* [华为与对手做朋友 海外不打价格战](http://weharmonyos.com/ren/2005/20050726_华为与对手做朋友海外不打价格战.html)

### 2006年
* [全流程降低成本和费用 提高盈利能力](http://weharmonyos.com/ren/2006/20060430_全流程降低成本和费用提高盈利能力.html)
* [在华为收购港湾时的谈话纪要](http://weharmonyos.com/ren/2006/20060510_在华为收购港湾时的谈话纪要.html)
* [我的青春岁月](http://weharmonyos.com/ren/2006/20060719_我的青春岁月.html)
* [天道酬勤](http://weharmonyos.com/ren/2006/20060721_天道酬勤.html)
* [冰岛游记](http://weharmonyos.com/ren/2006/200608_冰岛游记.html)
* [上甘岭是不会自然产生将军的 但将军都曾经是英雄](http://weharmonyos.com/ren/2006/20060904_上甘岭是不会自然产生将军的但将军都曾经是英雄.html)
* [华为大学要成为将军的摇篮](http://weharmonyos.com/ren/2006/20061120_华为大学要成为将军的摇篮.html)
* [在BT系统部、英国代表处汇报会上的讲话](http://weharmonyos.com/ren/2006/20061214_在BT系统部、英国代表处汇报会上的讲话.html)
* [实事求是的科研方向与二十年的艰苦努力](http://weharmonyos.com/ren/2006/20061218_实事求是的科研方向与二十年的艰苦努力.html)

### 2007年
* [对区域监控工作的讲话纪要](http://weharmonyos.com/ren/2007/20070108_对区域监控工作的讲话纪要.html)
* [财经的变革是华为公司的变革，不是财务系统的变革](http://weharmonyos.com/ren/2007/20070108_财经的变革是华为公司的变革，不是财务系统的变革.html)
* [要快乐的度过充满困难的一生](http://weharmonyos.com/ren/2007/200702_要快乐的度过充满困难的一生.html)
* [在行政采购及信息安全问题座谈会上的讲话](http://weharmonyos.com/ren/2007/20070315_在行政采购及信息安全问题座谈会上的讲话.html)
* [听取艰苦地区生活专项问题汇报会上的讲话](http://weharmonyos.com/ren/2007/20070409_听取艰苦地区生活专项问题汇报会上的讲话.html)
* [以生动活泼的方式传递奋斗者为主体的文化](http://weharmonyos.com/ren/2007/20070612_以生动活泼的方式传递奋斗者为主体的文化.html)
* [上甘岭在你心中，无论何时何地都可以产生英雄](http://weharmonyos.com/ren/2007/20070703_上甘岭在你心中，无论何时何地都可以产生英雄.html)
* [敢于胜利，才能善于胜利](http://weharmonyos.com/ren/2007/20070713_敢于胜利，才能善于胜利.html)
* [完善和提高对艰苦地区员工的保障措施](http://weharmonyos.com/ren/2007/20070721_完善和提高对艰苦地区员工的保障措施.html)
* [关于员工培训工作的谈话](http://weharmonyos.com/ren/2007/20070803_关于员工培训工作的谈话.html)
* [变革最重要的问题是一定要落地](http://weharmonyos.com/ren/2007/20070808_变革最重要的问题是一定要落地.html)
* [内控体系建设就是要穿美国鞋，不打补丁](http://weharmonyos.com/ren/2007/20070824_内控体系建设就是要穿美国鞋，不打补丁.html)
* [将军如果不知道自己错在哪里，就永远不会成为将军](http://weharmonyos.com/ren/2007/20070914_将军如果不知道自己错在哪里，就永远不会成为将军.html)

### 2008年
* [珍惜生命，要从自己关爱自己做起](http://weharmonyos.com/ren/2008/20070728_珍惜生命，要从自己关爱自己做起.html)
* [在EMT例会“08年公司业务计划与预算”汇报上的讲话](http://weharmonyos.com/ren/2008/20080131_在EMT例会“08年公司业务计划与预算”汇报上的讲话.html)
* [看莫斯科保卫战有感](http://weharmonyos.com/ren/2008/20080215_看莫斯科保卫战有感.html)
* [人生是美好的，但过程确实是痛苦的](http://weharmonyos.com/ren/2008/20080410_人生是美好的，但过程确实是痛苦的.html)
* [让青春的火花，点燃无愧无悔的人生](http://weharmonyos.com/ren/2008/20080531_让青春的火花，点燃无愧无悔的人生.html)
* [让青春的生命放射光芒](http://weharmonyos.com/ren/2008/20080613_让青春的生命放射光芒.html)
* [大家都是共和国的英雄](http://weharmonyos.com/ren/2008/20080616_大家都是共和国的英雄.html)
* [在PSST体系干部大会上的讲话](http://weharmonyos.com/ren/2008/20080705_任正非在PSST体系干部大会上的讲话.html)
* [逐步加深理解“以客户为中心，以奋斗者为本”的企业文化](http://weharmonyos.com/ren/2008/20080715_逐步加深理解“以客户为中心，以奋斗者为本”的企业文化.html)
* [在地区部向EMT进行2008年年中述职会议上的讲话](http://weharmonyos.com/ren/2008/20080721_在地区部向EMT进行2008年年中述职会议上的讲话.html)
* [理解国家，做好自己](http://weharmonyos.com/ren/2008/200807_理解国家，做好自己.html)
* [从泥坑里爬起来的人就是圣人](http://weharmonyos.com/ren/2008/20080902_从泥坑里爬起来的人就是圣人.html)
* [从汶川特大地震一片瓦砾中，一座百年前建的教堂不倒所想到的](http://weharmonyos.com/ren/2008/20080922_从汶川特大地震一片瓦砾中，一座百年前建的教堂不倒所想到的.html)
* [围绕客户PO打通，支撑“回款、收入、项目预核算”](http://weharmonyos.com/ren/2008/20081229_围绕客户PO打通，支撑“回款、收入、项目预核算”.html)
* [2008年新年祝词](http://weharmonyos.com/ren/2008/2008_2008年新年祝词.html)

### 2009年
* [开放、妥协与灰度](http://weharmonyos.com/ren/2009/20090115_开放、妥协与灰度.html)
* [让听得见炮声的人来决策](http://weharmonyos.com/ren/2009/20090116_让听得见炮声的人来决策.html)
* [谁来呼唤炮火，如何及时提供炮火支援](http://weharmonyos.com/ren/2009/20090116_谁来呼唤炮火，如何及时提供炮火支援.html)
* [与IFS项目组及财经体系员工座谈纪要](http://weharmonyos.com/ren/2009/20090206_任正非与IFS项目组及财经体系员工座谈纪要.html)
* [在驻外行政人员培训交流沟通会上的座谈纪要](http://weharmonyos.com/ren/2009/20090312_在驻外行政人员培训交流沟通会上的座谈纪要.html)
* [市场经济是最好的竞争方式，经济全球化是不可阻挡的潮流](http://weharmonyos.com/ren/2009/20090324_市场经济是最好的竞争方式，经济全球化是不可阻挡的潮流.html)
* [与PMS高端项目经理的座谈纪要](http://weharmonyos.com/ren/2009/20090327_与PMS高端项目经理的座谈纪要.html)
* [深淘滩，低作堰](http://weharmonyos.com/ren/2009/20090424_深淘滩，低作堰.html)
* [具有“长期持续艰苦奋斗的牺牲精神，永恒不变的艰苦奋斗的工作作风”是成为一个将军最基本条件](http://weharmonyos.com/ren/2009/20090827_具有“长期持续艰苦奋斗的牺牲精神，永恒不变的艰苦奋斗的工作作风”是成为一个将军最基本条件.html)
* [CFO要走向流程化和职业化，支撑公司及时、准确、优质、低成本交付](http://weharmonyos.com/ren/2009/20091026_CFO要走向流程化和职业化，支撑公司及时、准确、优质、低成本交付.html)
* [加快CFO队伍建设，支撑IFS推行落地](http://weharmonyos.com/ren/2009/20091127_加快CFO队伍建设，支撑IFS推行落地.html)

### 2010年
* [以客户为中心，加大平台投入，开放合作，实现共赢](http://weharmonyos.com/ren/2010/2000_以客户为中心，加大平台投入，开放合作，实现共赢.html)
* [春风送暖入屠苏](http://weharmonyos.com/ren/2010/20091231_春风送暖入屠苏.html)
* [以客户为中心，以奋斗者为本，长期坚持艰苦奋斗是我们胜利之本](http://weharmonyos.com/ren/2010/20100120_以客户为中心，以奋斗者为本，长期坚持艰苦奋斗是我们胜利之本.html)
* [在全球行政人员年度表彰暨经验交流大会座谈纪](http://weharmonyos.com/ren/2010/20100304_在全球行政人员年度表彰暨经验交流大会座谈纪要.html)
* [拉通项目四算，支撑项目层面经营管理](http://weharmonyos.com/ren/2010/20100430_拉通项目四算，支撑项目层面经营管理.html)
* [干部要担负起公司价值观的传承](http://weharmonyos.com/ren/2010/20100715_干部要担负起公司价值观的传承.html)
* [开放、合作、自我批判，做容千万家的天下英雄](http://weharmonyos.com/ren/2010/20100910_开放、合作、自我批判，做容千万家的天下英雄.html)
* [世博结束了，我们胜利了](http://weharmonyos.com/ren/2010/20101031_世博结束了，我们胜利了.html)
* [改善和媒体的关系](http://weharmonyos.com/ren/2010/20101125z_改善和媒体的关系.html)
* [做事要霸气，做人要谦卑，要按消费品的规律，敢于追求最大的增长和胜利](http://weharmonyos.com/ren/2010/20101203_做事要霸气，做人要谦卑，要按消费品的规律，敢于追求最大的增长和胜利.html)
* [五彩云霞飞遍天涯](http://weharmonyos.com/ren/2010/20101206_五彩云霞飞遍天涯.html)
* [以“选拔制”建设干部队伍，按流程梳理和精简组织，推进组织公开性和均衡性建设](http://weharmonyos.com/ren/2010/20110104_以“选拔制”建设干部队伍，按流程梳理和精简组织，推进组织公开性和均衡性建设.html)

### 2011年
* [成功不是未来前进的可靠向导](http://weharmonyos.com/ren/2011/20110117_成功不是未来前进的可靠向导.html)
* [关于珍爱生命与职业责任的讲话](http://weharmonyos.com/ren/2011/20110223_关于珍爱生命与职业责任的讲话.html)
* [华为关于如何与奋斗者分享利益的座谈会纪要](http://weharmonyos.com/ren/2011/20110414_华为关于如何与奋斗者分享利益的座谈会纪要.html)
* [与罗马尼亚账务共享中心座谈会纪要](http://weharmonyos.com/ren/2011/20110614_任总与罗马尼亚账务共享中心座谈会纪要.html)
* [与华为大学第10期干部高级管理研讨班学员座谈纪要](http://weharmonyos.com/ren/2011/20110727_与华为大学第10期干部高级管理研讨班学员座谈纪要.html)
* [与财经体系员工座谈讲话](http://weharmonyos.com/ren/2011/20111019_任正非与财经体系员工座谈讲话.html)
* [与同等学历认证班学员座谈纪要](http://weharmonyos.com/ren/2011/20111026_与同等学历认证班学员座谈纪要.html)
* [力出一孔，要集中优势资源投入在主航道上，敢于去争取更大的机会与差距](http://weharmonyos.com/ren/2011/20111031_力出一孔，要集中优势资源投入在主航道上，敢于去争取更大的机会与差距.html)
* [一江春水向东流](http://weharmonyos.com/ren/2011/20111225_一江春水向东流.html)

### 2012年
* [不要盲目扩张，不要自以为已经强大](http://weharmonyos.com/ren/2012/20120119_不要盲目扩张，不要自以为已经强大.html)
* [绝对考核的目的是团结多数人](http://weharmonyos.com/ren/2012/20120319_绝对考核的目的是团结多数人.html)
* [紧紧围绕客户，在战略层面宣传公司](http://weharmonyos.com/ren/2012/20120412_紧紧围绕客户，在战略层面宣传公司.html)
* [董事会领导下的CEO轮值制度辨](http://weharmonyos.com/ren/2012/20120423_董事会领导下的CEO轮值制度辨.html)
* [中国没有创新土壤不开放就是死亡](http://weharmonyos.com/ren/2012/20120712_中国没有创新土壤不开放就是死亡.html)
* [面向未来，以客户痛点为切入点，全球化展示](http://weharmonyos.com/ren/2012/201209_面向未来，以客户痛点为切入点，全球化展示.html)
* [东南非多国管理部向任总汇报工作纪要](http://weharmonyos.com/ren/2012/20121115_东南非多国管理部向任总汇报工作纪要.html)
* [与毛里求斯员工座谈会议纪要](http://weharmonyos.com/ren/2012/20121120_与毛里求斯员工座谈会议纪要.html)
* [与华为大学教育学院座谈会纪要](http://weharmonyos.com/ren/2012/20121219_与华为大学教育学院座谈会纪要.html)

### 2013年
* [力出一孔，利出一孔](http://weharmonyos.com/ren/2013/20121231_力出一孔，利出一孔.html)
* [在小国表彰会上的讲话](http://weharmonyos.com/ren/2013/20130114_在小国表彰会上的讲话.html)
* [和广州代表处座谈纪要](http://weharmonyos.com/ren/2013/20130219_任总和广州代表处座谈纪要.html)
* [家人永远不接班](http://weharmonyos.com/ren/2013/20130330_家人永远不接班.html)
* [要敢于超越美国公司，最多就是输](http://weharmonyos.com/ren/2013/20130517_要敢于超越美国公司，最多就是输.html)
* [在海外行政后勤服务管理思路汇报会上的讲话](http://weharmonyos.com/ren/2013/20130527_在海外行政后勤服务管理思路汇报会上的讲话.html)
* [财经流程建设汇报纪要](http://weharmonyos.com/ren/2013/20130627_财经流程建设向任总汇报纪要.html)
* [要培养一支能打仗、打胜仗的队伍](http://weharmonyos.com/ren/2013/20130719_要培养一支能打仗、打胜仗的队伍.html)
* [在重装旅集训营座谈会上的讲话](http://weharmonyos.com/ren/2013/20130723_在重装旅集训营座谈会上的讲话.html)
* [在GTS客户培训服务座谈会上的讲话](http://weharmonyos.com/ren/2013/20130904_在GTS客户培训服务座谈会上的讲话.html)
* [最好的防御就是进攻](http://weharmonyos.com/ren/2013/20130905_最好的防御就是进攻.html)
* [团结一切可以团结的力量](http://weharmonyos.com/ren/2013/20130911_团结一切可以团结的力量.html)
* [提倡节俭办晚会，节约会议成本](http://weharmonyos.com/ren/2013/20130911_提倡节俭办晚会，节约会议成本.html)
* [财经汇报纪要](http://weharmonyos.com/ren/2013/20130929_财经向任正非汇报纪要.html)
* [用乌龟精神，追上龙飞船](http://weharmonyos.com/ren/2013/20131019_用乌龟精神，追上龙飞船.html)
* [与CEC成员座谈会的讲话](http://weharmonyos.com/ren/2013/20131031_与CEC成员座谈会的讲话.html)
* [在GTS网规网优业务座谈会上的讲话](http://weharmonyos.com/ren/2013/20131105_在GTS网规网优业务座谈会上的讲话.html)
* [在华为大学教育学院工作汇报会上的讲话](http://weharmonyos.com/ren/2013/20131106_在华为大学教育学院工作汇报会上的讲话.html)
* [在IP交付保障团队座谈会上的讲话](http://weharmonyos.com/ren/2013/20131111_在IP交付保障团队座谈会上的讲话.html)
* [我一贯不是一个低调的人](http://weharmonyos.com/ren/2013/20131125_我一贯不是一个低调的人.html)
* [在EMT办公例会上的讲话](http://weharmonyos.com/ren/2013/20131129_在EMT办公例会上的讲话.html)
* [在公司内控与风险管理“三层防线”优化方案汇报的讲话](http://weharmonyos.com/ren/2013/20131204_在公司内控与风险管理“三层防线”优化方案汇报的讲话.html)
* [关于“严格、有序、简化的认真管理是实现超越的关键”的座谈纪要](http://weharmonyos.com/ren/2013/20131219_关于“严格、有序、简化的认真管理是实现超越的关键”的座谈纪要.html)
* [聚焦商业成功，英雄不问出处](http://weharmonyos.com/ren/2013/20131222_聚焦商业成功，英雄不问出处.html)
* [在运营商网络BG战略务虚会上的讲话及主要讨论发言](http://weharmonyos.com/ren/2013/20131228在运营商网络BG战略务虚会上的讲话及主要讨论发言.html)
* [在大规模消灭腐败进展汇报会上的讲话](http://weharmonyos.com/ren/2013/20131230_任正非在大规模消灭腐败进展汇报会上的讲话.html)
* [CFO标准与培养机制汇报纪要](http://weharmonyos.com/ren/2013/2013_CFO标准与培养机制向任总汇报纪要.html)

### 2014年
* [风物长宜放眼量](http://weharmonyos.com/ren/2014/20140105_风物长宜放眼量.html)
* [做谦虚的领导者](http://weharmonyos.com/ren/2014/20140113_做谦虚的领导者.html)
* [在董事赋能研讨会上与候选专职董事交流讲话](http://weharmonyos.com/ren/2014/20140122_在董事赋能研讨会上与候选专职董事交流讲话.html)
* [握紧拳头才有力量](http://weharmonyos.com/ren/2014/20140129_握紧拳头才有力量.html)
* [自我批判，不断超越](http://weharmonyos.com/ren/2014/20140219_自我批判，不断超越.html)
* [在外派伙食补助汇报会上的讲话](http://weharmonyos.com/ren/2014/20140225_在外派伙食补助汇报会上的讲话.html)
* [在关于重装旅组织汇报会议上的讲话](http://weharmonyos.com/ren/2014/20140307_在关于重装旅组织汇报会议上的讲话.html)
* [在大机会时代，千万不要机会主义](http://weharmonyos.com/ren/2014/20140311_在大机会时代，千万不要机会主义.html)
* [三年，从士兵到将军](http://weharmonyos.com/ren/2014/20140314三年，从士兵到将军.html)
* [在华大建设思路汇报会上的讲话](http://weharmonyos.com/ren/2014/20140327_在华大建设思路汇报会上的讲话.html)
* [在日本研究所工作汇报会上的讲话](http://weharmonyos.com/ren/2014/20140331_在日本研究所工作汇报会上的讲话.html)
* [与巴西代表处及巴供中心座谈纪要](http://weharmonyos.com/ren/2014/20140409_与巴西代表处及巴供中心座谈纪要.html)
* [在巴西代表处的讲话](http://weharmonyos.com/ren/2014/20140410_在巴西代表处的讲话.html)
* [在秘鲁代表处座谈会上的讲话](http://weharmonyos.com/ren/2014/20140411_在秘鲁代表处座谈会上的讲话.html)
* [在拉美北地区部、哥伦比亚代表处工作汇报会上的讲话](http://weharmonyos.com/ren/2014/20140413_在拉美北地区部、哥伦比亚代表处工作汇报会上的讲话.html)
* [在华为上研所专家座谈会上的讲话](http://weharmonyos.com/ren/2014/20140416_在华为上研所专家座谈会上的讲话.html)
* [在后备干部项目管理与经营短训项目座谈会上的讲话](http://weharmonyos.com/ren/2014/20140424_在后备干部项目管理与经营短训项目座谈会上的讲话.html)
* [英国媒体会谈纪要](http://weharmonyos.com/ren/2014/20140502_英国媒体会谈纪要.html)
* [喜马拉雅山的水为什么不能流入亚马逊河](http://weharmonyos.com/ren/2014/20140509_喜马拉雅山的水为什么不能流入亚马逊河.html)
* [简化管理，选拔使用有全局观的干部，数据及信息透明](http://weharmonyos.com/ren/2014/20140509_简化管理，选拔使用有全局观的干部，数据及信息透明.html)
* [在巡视松山湖制造现场的讲话纪要](http://weharmonyos.com/ren/2014/20140510_在巡视松山湖制造现场的讲话纪要.html)
* [推动行政服务业务变革，试点流程责任制](http://weharmonyos.com/ren/2014/20140513_推动行政服务业务变革，试点流程责任制.html)
* [在海外子公司董事会推行工作汇报会上的讲话](http://weharmonyos.com/ren/2014/20140527_在海外子公司董事会推行工作汇报会上的讲话.html)
* [在丹麦代表处员工座谈会上的讲话](http://weharmonyos.com/ren/2014/20140603_在丹麦代表处员工座谈会上的讲话.html)
* [在西欧地区部工作汇报会上的讲话](http://weharmonyos.com/ren/2014/20140604_在西欧地区部工作汇报会上的讲话.html)
* [在德国LTC教导队训战班座谈会上的讲话](http://weharmonyos.com/ren/2014/20140605_在德国LTC教导队训战班座谈会上的讲话.html)
* [在保加利亚代表处工作汇报会上的讲话](http://weharmonyos.com/ren/2014/20140606_在保加利亚代表处工作汇报会上的讲话.html)
* [为什么我们今天还要向“蓝血十杰”学习](http://weharmonyos.com/ren/2014/20140616_为什么我们今天还要向“蓝血十杰”学习.html)
* [洞庭湖装不下太平洋的水](http://weharmonyos.com/ren/2014/20140619_洞庭湖装不下太平洋的水.html)
* [在“关于内部网络安全工作方向“的决议](http://weharmonyos.com/ren/2014/20140624_在“关于内部网络安全工作方向“的决议.html)
* [在人力资源工作汇报会上的讲话](http://weharmonyos.com/ren/2014/20140624_在人力资源工作汇报会上的讲话.html)
* [第一次就把事情做对](http://weharmonyos.com/ren/2014/20140707_第一次就把事情做对.html)
* [在变革项目激励政策汇报会上的讲话](http://weharmonyos.com/ren/2014/20140723_在变革项目激励政策汇报会上的讲话.html)
* [在小国综合管理变革汇报会上的讲话](http://weharmonyos.com/ren/2014/20140723_在小国综合管理变革汇报会上的讲话.html)
* [在项目管理资源池第一期学员座谈会上的讲话](http://weharmonyos.com/ren/2014/20140723_在项目管理资源池第一期学员座谈会上的讲话.html)
* [在2014年7月25日EMT办公会议上的讲话](http://weharmonyos.com/ren/2014/20140725_在2014年7月25日EMT办公会议上的讲话.html)
* [在销售项目经理资源池第一期学员座谈会上的讲话](http://weharmonyos.com/ren/2014/20140826_在销售项目经理资源池第一期学员座谈会上的讲话.html)
* [在“班长的战争”对华为的启示和挑战汇报会上的讲话](http://weharmonyos.com/ren/2014/20140923_在“班长的战争”对华为的启示和挑战汇报会上的讲话.html)
* [在公司近期激励导向和激励原则汇报会上的讲话](http://weharmonyos.com/ren/2014/20140923_在公司近期激励导向和激励原则汇报会上的讲话.html)
* [在多个汇困国家调研中的讲话](http://weharmonyos.com/ren/2014/201409_在多个汇困国家调研中的讲话.html)
* [与CEC就非物质激励工作优化的座谈纪要](http://weharmonyos.com/ren/2014/20141104_与CEC就非物质激励工作优化的座谈纪要.html)
* [在行政流程责任制试点进展汇报会上的讲话](http://weharmonyos.com/ren/2014/20141104_在行政流程责任制试点进展汇报会上的讲话.html)
* [遍地英雄下夕烟，六亿神州尽舜尧](http://weharmonyos.com/ren/2014/20141106_遍地英雄下夕烟，六亿神州尽舜尧.html)
* [坚持为世界创造价值，为价值而创新](http://weharmonyos.com/ren/2014/20141114_坚持为世界创造价值，为价值而创新.html)
* [在“从中心仓到站点打通”工作汇报会上的讲话](http://weharmonyos.com/ren/2014/20141205_在“从中心仓到站点打通”工作汇报会上的讲话.html)
* [依托欧美先进软件包构建高质量的IT系统](http://weharmonyos.com/ren/2014/20141218_依托欧美先进软件包构建高质量的IT系统.html)
* [在艰苦地区及岗位管理部工作汇报会上的讲话](http://weharmonyos.com/ren/2014/20141218在艰苦地区及岗位管理部工作汇报会上的讲话.html)

### 2015年
* [在与法务部、董秘及无线员工座谈会上的讲话](http://weharmonyos.com/ren/2015/20150108_在与法务部、董秘及无线员工座谈会上的讲话.html)
* [打造运营商BG“三朵云”，将一线武装到牙齿](http://weharmonyos.com/ren/2015/20150109_打造运营商BG“三朵云”，将一线武装到牙齿.html)
* [变革的目的就是要多产粮食和增加土地肥力](http://weharmonyos.com/ren/2015/20150116_变革的目的就是要多产粮食和增加土地肥力.html)
* [最大的敌人是我们自己](http://weharmonyos.com/ren/2015/20150125_最大的敌人是我们自己.html)
* [改善艰苦地区条件，加快循环赋能，为公司筑起第二道防线](http://weharmonyos.com/ren/2015/20150127_改善艰苦地区条件，加快循环赋能，为公司筑起第二道防线.html)
* [主航道组织坚持矩阵化管理，非主航道组织去矩阵化管理](http://weharmonyos.com/ren/2015/20150310_非主航道组织要率先实现流程责任制，通过流程责任来选拔管理者，淘汰不作为员工，为公司管理进步摸索经验.html)
* [在2015年全球行政工作年会上的讲话](http://weharmonyos.com/ren/2015/20150318_在2015年全球行政工作年会上的讲话.html)
* [在变革战略预备队进展汇报上的讲话](http://weharmonyos.com/ren/2015/20150331_在变革战略预备队进展汇报上的讲话.html)
* [埃森哲董事长Pierre拜访的会谈纪要](http://weharmonyos.com/ren/2015/20150408_埃森哲董事长Pierre拜访任正非的会谈纪要.html)
* [在变革战略预备队及进展汇报座谈上的讲话](http://weharmonyos.com/ren/2015/20150504_在变革战略预备队及进展汇报座谈上的讲话.html)
* [与BCG顾问交流会谈纪要](http://weharmonyos.com/ren/2015/20150506_与BCG顾问交流会谈纪要.html)
* [在变革战略预备队誓师及颁奖典礼上的座谈纪要](http://weharmonyos.com/ren/2015/20150508_在变革战略预备队誓师及颁奖典礼上的座谈纪要.html)
* [在监管重装旅座谈会上的讲话](http://weharmonyos.com/ren/2015/20150515_在监管重装旅座谈会上的讲话.html)
* [在公司质量工作汇报会上的讲话](http://weharmonyos.com/ren/2015/20150520_在公司质量工作汇报会上的讲话.html)
* [在固网产业趋势及进展汇报会上的讲话](http://weharmonyos.com/ren/2015/20150702_在固网产业趋势及进展汇报会上的讲话.html)
* [在合同场景师建设思路汇报上的讲话](http://weharmonyos.com/ren/2015/20150713_在合同场景师建设思路汇报上的讲话.html)
* [在子公司监督型董事会管理层监督方案及试点情况汇报上的讲话](http://weharmonyos.com/ren/2015/20150713_在子公司监督型董事会管理层监督方案及试点情况汇报上的讲话.html)
* [与英国研究所、北京研究所、伦敦财经风险管控中心座谈的纪要](http://weharmonyos.com/ren/2015/20150714_与英国研究所、北京研究所、伦敦财经风险管控中心座谈的纪要.html)
* [在战略预备队业务汇报上的讲话](http://weharmonyos.com/ren/2015/20150730_在战略预备队业务汇报上的讲话.html)
* [在变革战略预备队第三期誓师典礼上的讲话](http://weharmonyos.com/ren/2015/20150731_在变革战略预备队第三期誓师典礼上的讲话.html)
* [构建先进装备，沉淀核心能力，在更高维度打赢下一场战争](http://weharmonyos.com/ren/2015/20150812_构建先进装备，沉淀核心能力，在更高维度打赢下一场战争.html)
* [脚踏实地，做挑战自我的长跑者](http://weharmonyos.com/ren/2015/20150827_脚踏实地，做挑战自我的长跑者.html)
* [在2015年8月28日EMT办公会议上的讲话](http://weharmonyos.com/ren/2015/20150828_在2015年8月28日EMT办公会议上的讲话.html)
* [在波士顿咨询汇报行政服务变革（国家驻点成果及未来变革方向）的讲话](http://weharmonyos.com/ren/2015/20150902_在波士顿咨询汇报行政服务变革（国家驻点成果及未来变革方向）的讲话.html)
* [接受福布斯中文网采访](http://weharmonyos.com/ren/2015/20150906_任正非接受福布斯中文网采访.html)
* [在战略预备队誓师典礼暨优秀队员表彰大会上的讲话](http://weharmonyos.com/ren/2015/20150907_在战略预备队誓师典礼暨优秀队员表彰大会上的讲话.html)
* [在公共及政府事务部2015年工作汇报会的讲话](http://weharmonyos.com/ren/2015/20150923_在公共及政府事务部2015年工作汇报会的讲话.html)
* [在2015年9月24日EMT办公会议上的讲话](http://weharmonyos.com/ren/2015/20150924_在2015年9月24日EMT办公会议上的讲话.html)
* [在GTS站点信息库、地理信息库、网络动态运行信息库和集成交付平台建设汇报会上的讲话](http://weharmonyos.com/ren/2015/20150928_在GTS站点信息库、地理信息库、网络动态运行信息库和集成交付平台建设汇报会上的讲话.html)
* [最终的竞争是质量的竞争](http://weharmonyos.com/ren/2015/20151010_最终的竞争是质量的竞争.html)
* [将军是打出来的](http://weharmonyos.com/ren/2015/20151023_将军是打出来的.html)
* [小国要率先实现精兵战略，让听得见炮声的人呼唤炮火](http://weharmonyos.com/ren/2015/20151027_小国要率先实现精兵战略，让听得见炮声的人呼唤炮火.html)
* [聚焦主航道，在战略机会点上抢占机会](http://weharmonyos.com/ren/2015/20151031_聚焦主航道，在战略机会点上抢占机会.html)
* [彭剑锋专访纪要](http://weharmonyos.com/ren/2015/20151218_彭剑锋专访任正非纪要.html)
* [致新员工书](http://weharmonyos.com/ren/2015/201509致新员工书.html)
* [在调查工作授权及流程优化汇报上的讲话](http://weharmonyos.com/ren/2015/20151023_在调查工作授权及流程优化汇报上的讲话.html)

### 2016年
* [决胜取决于坚如磐石的信念，信念来自专注](http://weharmonyos.com/ren/2016/20160113_决胜取决于坚如磐石的信念，信念来自专注.html)
* [革的目的就是更简单、更及时、更准确](http://weharmonyos.com/ren/2016/20160118_变革的目的就是更简单、更及时、更准确.html)
* [在2016年1月25日EMT办公会议上的讲话](http://weharmonyos.com/ren/2016/20160125_在2016年1月25日EMT办公会议上的讲话.html)
* [做“成吉思汗的马掌”，支撑我们服务世界的雄心](http://weharmonyos.com/ren/2016/20160214_做“成吉思汗的马掌”，支撑我们服务世界的雄心.html)
* [巴塞罗那通信展小型恳谈会纪要](http://weharmonyos.com/ren/2016/20160223_巴塞罗那通信展小型恳谈会纪要.html)
* [多路径多梯次跨越“上甘岭”攻进无人区](http://weharmonyos.com/ren/2016/20160227_多路径多梯次跨越“上甘岭”攻进无人区.html)
* [28年只对准一个城墙口冲锋](http://weharmonyos.com/ren/2016/20160305_28年只对准一个城墙口冲锋.html)
* [关于改善艰苦国家的作战环境](http://weharmonyos.com/ren/2016/20160307_关于改善艰苦国家的作战环境.html)
* [在巴展总结会议上的讲话](http://weharmonyos.com/ren/2016/20160331_在巴展总结会议上的讲话.html)
* [与日本代表处、日本研究所员工座谈纪要](http://weharmonyos.com/ren/2016/20160405_与日本代表处、日本研究所员工座谈纪要.html)
* [在专业服务业务策略汇报上的讲话](http://weharmonyos.com/ren/2016/20160428_在专业服务业务策略汇报上的讲话.html)
* [与Fellow座谈会上的讲话](http://weharmonyos.com/ren/2016/20160505_任正非与Fellow座谈会上的讲话.html)
* [在中亚地区部员工座谈会上的讲话](http://weharmonyos.com/ren/2016/20160523_在中亚地区部员工座谈会上的讲话.html)
* [为祖国百年科技振兴而努力奋斗](http://weharmonyos.com/ren/2016/20160528_为祖国百年科技振兴而努力奋斗.html)
* [以创新为核心竞争力，为祖国百年科技振兴而奋斗](http://weharmonyos.com/ren/2016/20160530_以创新为核心竞争力，为祖国百年科技振兴而奋斗.html)
* [前进的路上不会铺满了鲜花](http://weharmonyos.com/ren/2016/20160712_前进的路上不会铺满了鲜花.html)
* [在签证业务变革进展汇报上的讲话](http://weharmonyos.com/ren/2016/20160804_在签证业务变革进展汇报上的讲话.html)
* [聚焦战略平台，加强血液流动，夺取未来胜利](http://weharmonyos.com/ren/2016/20160805_聚焦战略平台，加强血液流动，夺取未来胜利.html)
* [在诺亚方舟实验室座谈会上的讲话](http://weharmonyos.com/ren/2016/20160810_在诺亚方舟实验室座谈会上的讲话.html)
* [IPD的本质是从机会到商业变现](http://weharmonyos.com/ren/2016/20160813_IPD的本质是从机会到商业变现.html)
* [公司必须持续不断的、永恒的促进组织血液流动，增强优秀干部、专家的循环赋能](http://weharmonyos.com/ren/2016/20160815_公司必须持续不断的、永恒的促进组织血液流动，增强优秀干部、专家的循环赋能.html)
* [美丽的爱尔兰是软件的大摇篮](http://weharmonyos.com/ren/2016/20160819_美丽的爱尔兰是软件的大摇篮.html)
* [关于行政与慧通工作讲话的纪要](http://weharmonyos.com/ren/2016/20161021_关于行政与慧通工作讲话的纪要.html)
* [建立对作业类员工的科学管理方法与评价体系，导向多产粮食](http://weharmonyos.com/ren/2016/20161025_建立对作业类员工的科学管理方法与评价体系，导向多产粮食.html)
* [在质量与流程IT管理部员工座谈会上的讲话](http://weharmonyos.com/ren/2016/20161026_在质量与流程IT管理部员工座谈会上的讲话.html)
* [在运营商三朵云2.0阶段进展汇报会上的讲话](http://weharmonyos.com/ren/2016/20161026_在运营商三朵云2.0阶段进展汇报会上的讲话.html)
* [春江水暖鸭先知，不破楼兰誓不还](http://weharmonyos.com/ren/2016/20161028_春江水暖鸭先知，不破楼兰誓不还.html)
* [聚焦主航道，眼望星空，朋友越多天下越大](http://weharmonyos.com/ren/2016/20161031_聚焦主航道，眼望星空，朋友越多天下越大.html)
* [在中亚（塔吉克斯坦、土耳其、白俄）代表处汇报会议上的讲话](http://weharmonyos.com/ren/2016/201610_在中亚（塔吉克斯坦、土耳其、白俄）代表处汇报会议上的讲话.html)
* [聚焦平台，加强血液流动，敢于破格，抢占世界高地](http://weharmonyos.com/ren/2016/20161115_聚焦平台，加强血液流动，敢于破格，抢占世界高地.html)
* [与合同场景师座谈会上的讲话](http://weharmonyos.com/ren/2016/20161121_与合同场景师座谈会上的讲话.html)
* [人力资源政策要朝着熵减的方向发展](http://weharmonyos.com/ren/2016/20161130_人力资源政策要朝着熵减的方向发展.html)
* [内外合规多打粮，保驾护航赢未来](http://weharmonyos.com/ren/2016/20161201_内外合规多打粮，保驾护航赢未来.html)
* [在法国研究所座谈交流纪要](http://weharmonyos.com/ren/2016/20161217_在法国研究所座谈交流纪要.html)
* [在法国美学研究所的讲话](http://weharmonyos.com/ren/2016/20161217_在法国美学研究所的讲话.html)
* [在存货账实相符项目汇报会上的讲话](http://weharmonyos.com/ren/2016/20161222_在存货账实相符项目汇报会上的讲话.html)

### 2017年
* [在人工智能应用GTS研讨会上的讲话](http://weharmonyos.com/ren/2017/20170107_在人工智能应用GTS研讨会上的讲话.html)
* [在2017年市场工作大会上的讲话](http://weharmonyos.com/ren/2017/20170111_在2017年市场工作大会上的讲话.html)
* [在消费者BG年度大会上的讲话](http://weharmonyos.com/ren/2017/20170117_在消费者BG年度大会上的讲话.html)
* [在健康指导中心业务变革项目阶段汇报会上的讲话](http://weharmonyos.com/ren/2017/20170120_在健康指导中心业务变革项目阶段汇报会上的讲话.html)
* [在北部非洲（尼日尔、布基纳法索）汇报会议的讲话](http://weharmonyos.com/ren/2017/201701_在北部非洲（尼日尔、布基纳法索）汇报会议的讲话.html)
* [在厄瓜多尔代表处的讲话](http://weharmonyos.com/ren/2017/20170203_在厄瓜多尔代表处的讲话.html)
* [在玻利维亚代表处的讲话](http://weharmonyos.com/ren/2017/20170205_在玻利维亚代表处的讲话.html)
* [在巴拉圭的讲话](http://weharmonyos.com/ren/2017/20170206_在巴拉圭的讲话.html)
* [在泰国与地区部负责人、在尼泊尔与员工座谈的讲话](http://weharmonyos.com/ren/2017/20170215_在泰国与地区部负责人、在尼泊尔与员工座谈的讲话.html)
* [在泛网络区域组织变革优化总结与规划汇报的讲话](http://weharmonyos.com/ren/2017/20170220_在泛网络区域组织变革优化总结与规划汇报的讲话.html)
* [在高研班和战略预备队汇报会上的讲话](http://weharmonyos.com/ren/2017/20170224_在高研班和战略预备队汇报会上的讲话.html)
* [我们的目的是实现高质量](http://weharmonyos.com/ren/2017/20170314_我们的目的是实现高质量.html)
* [客户需求导向，提升公司E2E系统竞争力](http://weharmonyos.com/ren/2017/20170316_客户需求导向，提升公司E2E系统竞争力.html)
* [在公司监督与管控体系延伸建设思考汇报会上的讲话](http://weharmonyos.com/ren/2017/20170327_在公司监督与管控体系延伸建设思考汇报会上的讲话.html)
* [在支付系统员工座谈会上的讲话](http://weharmonyos.com/ren/2017/20170328_在支付系统员工座谈会上的讲话.html)
* [与广州代表处部分员工晚餐会的讲话](http://weharmonyos.com/ren/2017/20170406_与广州代表处部分员工晚餐会的讲话.html)
* [与网络能源产品线部分员工晚餐会的讲话](http://weharmonyos.com/ren/2017/20170410_与网络能源产品线部分员工晚餐会的讲话.html)
* [在战略预备队座谈会上的讲话](http://weharmonyos.com/ren/2017/20170418_在战略预备队座谈会上的讲话.html)
* [在道德遵从委员会第二次代表大会的讲话](http://weharmonyos.com/ren/2017/20170424_在道德遵从委员会第二次代表大会的讲话.html)
* [在职员类定位与差异化管理汇报会上的讲话](http://weharmonyos.com/ren/2017/20170427_在职员类定位与差异化管理汇报会上的讲话.html)
* [在哈佛商学院全球高管论坛上的演讲](http://weharmonyos.com/ren/2017/201704_在哈佛商学院全球高管论坛上的演讲.html)
* [在健康指导与应急保障业务整合及签证变革进展汇报会上的讲话](http://weharmonyos.com/ren/2017/20170527_在健康指导与应急保障业务整合及签证变革进展汇报会上的讲话.html)
* [在继任计划工作汇报会上的讲话](http://weharmonyos.com/ren/2017/20170527_在继任计划工作汇报会上的讲话.html)
* [什么叫精神文明，什么叫物质文明](http://weharmonyos.com/ren/2017/20170601_什么叫精神文明，什么叫物质文明.html)
* [方向要大致正确，组织要充满活](http://weharmonyos.com/ren/2017/20170602_方向要大致正确，组织要充满活力.html)
* [在曼谷座谈纪要](http://weharmonyos.com/ren/2017/20170621_与任总在曼谷座谈纪要.html)
* [与德国代表处交流纪要](http://weharmonyos.com/ren/2017/20170622_与德国代表处交流纪要.html)
* [与波兰代表处交流纪要](http://weharmonyos.com/ren/2017/20170623_与波兰代表处交流纪要.html)
* [在俄罗斯代表处讲话纪要](http://weharmonyos.com/ren/2017/20170624_在俄罗斯代表处讲话纪要.html)
* [在华为平安园区项目汇报会上的讲话](http://weharmonyos.com/ren/2017/20170705_在华为平安园区项目汇报会上的讲话.html)
* [在战略预备队述职会上的讲话](http://weharmonyos.com/ren/2017/20170711_在战略预备队述职会上的讲话.html)
* [在三季度区域总裁会议上的讲话](http://weharmonyos.com/ren/2017/20170712_在三季度区域总裁会议上的讲话.html)
* [在新员工入职培训座谈会上的讲话](http://weharmonyos.com/ren/2017/20170712_在新员工入职培训座谈会上的讲话.html)
* [与韩国办事处交流纪要](http://weharmonyos.com/ren/2017/20170718_与韩国办事处交流纪要.html)
* [与徐直军在消费者BG2017年中市场大会上的讲话](http://weharmonyos.com/ren/2017/20170723_任正非与徐直军在消费者BG2017年中市场大会上的讲话.html)
* [在人力资源管理纲要2.0沟通会上的讲话](http://weharmonyos.com/ren/2017/20170807_在人力资源管理纲要2.0沟通会上的讲话.html)
* [在行政服务解决“小鬼难缠”工作进展汇报上的讲话](http://weharmonyos.com/ren/2017/20170807_在行政服务解决“小鬼难缠”工作进展汇报上的讲话.html)
* [科学的量化、简化管理，关注技能与经验的积累，培育工匠文化](http://weharmonyos.com/ren/2017/20170807_科学的量化、简化管理，关注技能与经验的积累，培育工匠文化.html)
* [在子公司监督型董事会年中工作会议上的讲话](http://weharmonyos.com/ren/2017/20170809_在子公司监督型董事会年中工作会议上的讲话.html)
* [在冰岛与四位弟兄咖啡细语](http://weharmonyos.com/ren/2017/20170816_在冰岛与四位弟兄咖啡细语.html)
* [在规范职能组织权力工作组座谈会上的讲话](http://weharmonyos.com/ren/2017/20170821_在规范职能组织权力工作组座谈会上的讲话.html)
* [与采购干部座谈会上的讲话](http://weharmonyos.com/ren/2017/20170824_与采购干部座谈会上的讲话.html)
* [开放创新，吸纳全球人才，构建“为我所知、为我所用、为我所有”的全球能力布局](http://weharmonyos.com/ren/2017/20170826_开放创新，吸纳全球人才，构建“为我所知、为我所用、为我所有”的全球能力布局.html)
* [在诺亚方舟实验室使能工程部成立会上的讲话](http://weharmonyos.com/ren/2017/20170828_在诺亚方舟实验室使能工程部成立会上的讲话.html)
* [在GTS人工智能实践进展汇报会上的讲话](http://weharmonyos.com/ren/2017/20170829_任总在GTS人工智能实践进展汇报会上的讲话.html)
* [在“合同在代表处审结”工作汇报会上的讲话](http://weharmonyos.com/ren/2017/20170829_在“合同在代表处审结”工作汇报会上的讲话.html)
* [在英国代表处的讲话](http://weharmonyos.com/ren/2017/20170906_在英国代表处的讲话.html)
* [在伦敦FRCC听取贸易合规和金融合规汇报的讲话](http://weharmonyos.com/ren/2017/20170913_任总在伦敦FRCC听取贸易合规和金融合规汇报的讲话.html)
* [与接入网团队座谈会上的讲话](http://weharmonyos.com/ren/2017/20170914_任总与接入网团队座谈会上的讲话.html)
* [在捷克代表处讲话](http://weharmonyos.com/ren/2017/20170917_在捷克代表处讲话.html)
* [在加拿大代表处的讲话](http://weharmonyos.com/ren/2017/20171003_在加拿大代表处的讲话.html)
* [一杯咖啡吸收宇宙能量，一桶浆糊粘接世界智慧](http://weharmonyos.com/ren/2017/20171006_一杯咖啡吸收宇宙能量，一桶浆糊粘接世界智慧.html)
* [在会议标准化及服务提升项目汇报会上的讲话](http://weharmonyos.com/ren/2017/20171018_在会议标准化及服务提升项目汇报会上的讲话.html)
* [在2017年第四季度地区部总裁会议上的讲话](http://weharmonyos.com/ren/2017/20171019_在2017年第四季度地区部总裁会议上的讲话.html)
* [在消费者BG业务汇报及骨干座谈会上的讲话](http://weharmonyos.com/ren/2017/20171024_在消费者BG业务汇报及骨干座谈会上的讲话.html)
* [关于人力资源管理纲要2.0修订与研讨的讲话纪要](http://weharmonyos.com/ren/2017/20171113_关于人力资源管理纲要2.0修订与研讨的讲话纪要.html)
* [在公司愿景与使命研讨会上的讲话](http://weharmonyos.com/ren/2017/20171120_在公司愿景与使命研讨会上的讲话.html)
* [什么是“一杯咖啡吸收宇宙能量'](http://weharmonyos.com/ren/2017/20171211_什么是“一杯咖啡吸收宇宙能量'.html)
* [什么是确定性工作](http://weharmonyos.com/ren/2017/20171213_什么是确定性工作.html)
* [确定性工作精细化、自动化](http://weharmonyos.com/ren/2017/20171215_确定性工作精细化、自动化.html)
* [在落实日落法及清理机关说NO](http://weharmonyos.com/ren/2017/20171218_在落实日落法及清理机关说NO.html)
* [熵减的过程是痛苦的，前途是光明的](http://weharmonyos.com/ren/2017/20171219_熵减的过程是痛苦的，前途是光明的.html)
* [在听取展厅工作汇报时，关于咨询师的讲话](http://weharmonyos.com/ren/2017/20171225_在听取展厅工作汇报时，关于咨询师的讲话.html)


### 2018年
* [在财务部分员工座谈会上的讲话](http://weharmonyos.com/ren/2018/20180116_在财务部分员工座谈会上的讲话.html)
* [从泥坑中爬起来的是圣人](http://weharmonyos.com/ren/2018/20180117_从泥坑中爬起来的是圣人.html)
* [与财务、基建、行政座谈会上的讲话](http://weharmonyos.com/ren/2018/20180118_与财务、基建、行政座谈会上的讲话.html)
* [“落实日落法及清理机关说NO”秘书处座谈会上的讲话](http://weharmonyos.com/ren/2018/20180205_与“落实日落法及清理机关说NO”秘书处座谈会上的讲话.html)
* [在员工关系变革工作进展汇报上的讲话](http://weharmonyos.com/ren/2018/20180209_在员工关系变革工作进展汇报上的讲话.html)
* [在斯里兰卡代表处的讲话](http://weharmonyos.com/ren/2018/20180223_在斯里兰卡代表处的讲话.html)
* [从系统工程角度出发规划华为大生产体系架构，建设世界一流的先进生产系统](http://weharmonyos.com/ren/2018/20180224_从系统工程角度出发规划华为大生产体系架构，建设世界一流的先进生产系统.html)
* [在蒙古办事处汇报会议上的讲话](http://weharmonyos.com/ren/2018/20180302_在蒙古办事处汇报会议上的讲话.html)
* [在2018年全球行政年会上的座谈纪要](http://weharmonyos.com/ren/2018/20180314_任正非在2018年全球行政年会上的座谈纪要.html)
* [研发要做智能世界的“发动机”](http://weharmonyos.com/ren/2018/20180321_研发要做智能世界的“发动机”.html)
* [在黎巴嫩代表处的讲话](http://weharmonyos.com/ren/2018/20180411_在黎巴嫩代表处的讲话.html)
* [关于人力资源组织运作优化的讲话](http://weharmonyos.com/ren/2018/20180420_关于人力资源组织运作优化的讲话.html)
* [在战略预备队述职会上的讲话](http://weharmonyos.com/ren/2018/20180426_任正非在战略预备队述职会上的讲话.html)
* [励精图治，十年振兴](http://weharmonyos.com/ren/2018/20180515_励精图治，十年振兴.html)
* [在剑桥和伊普斯维奇研究所座谈纪要](http://weharmonyos.com/ren/2018/20180605_在剑桥和伊普斯维奇研究所座谈纪要.html)
* [BG机关要缩短经线，畅通纬线，强化作战组织的能力考核与选拔，精减非作战组织的规模及运作](http://weharmonyos.com/ren/2018/20180622_BG机关要缩短经线，畅通纬线，强化作战组织的能力考核与选拔，精减非作战组织的规模及运作.html)
* [在2018年IRB战略务虚研讨会的讲话](http://weharmonyos.com/ren/2018/20180623_在2018年IRB战略务虚研讨会的讲话.html)
* [与平台协调委员会座谈纪要](http://weharmonyos.com/ren/2018/20180627_与平台协调委员会座谈纪要.html)
* [在卢旺达饭店与员工聊天记录](http://weharmonyos.com/ren/2018/20180701在卢旺达饭店与员工聊天记录.html)
* [在攀登珠峰的路上沿途下蛋](http://weharmonyos.com/ren/2018/20180713_在攀登珠峰的路上沿途下蛋.html)
* [在GTS人工智能实践进展汇报会上的讲话](http://weharmonyos.com/ren/2018/20180718在GTS人工智能实践进展汇报会上的讲话.html)
* [从人类文明的结晶中，找到解决世界问题的钥匙](http://weharmonyos.com/ren/2018/20180929_从人类文明的结晶中，找到解决世界问题的钥匙.html)
* [坚持多路径、多梯次、多场景化的研发路线，攻上“上甘岭”，实现5G战略领先](http://weharmonyos.com/ren/2018/20181017_坚持多路径、多梯次、多场景化的研发路线，攻上“上甘岭”，实现5G战略领先.html)
* [鼓足干劲，力争上游，不畏一切艰苦困苦](http://weharmonyos.com/ren/2018/20181019_鼓足干劲，力争上游，不畏一切艰苦困苦.html)
* [寂寞的英雄是伟大的英雄，我们要鼓励新旧循环](http://weharmonyos.com/ren/2018/20181107_寂寞的英雄是伟大的英雄，我们要鼓励新旧循环.html)
* [加强与国内大学合作，吸纳全球优秀人才，共同推动中国基础研究](http://weharmonyos.com/ren/2018/20181119_加强与国内大学合作，吸纳全球优秀人才，共同推动中国基础研究.html)
* [在日本研究所业务汇报](http://weharmonyos.com/ren/2018/20181121_任正非在日本研究所业务汇报.html)

### 2019年
* [深圳答记者问](http://weharmonyos.com/ren/2019/20190521深圳答记者问.html)
* [任总接受《金融时报》采访纪要](http://weharmonyos.com/ren/2019/20190624_任总接受《金融时报》采访纪要.html)
* [任正非接受《经济学人》采访纪要](http://weharmonyos.com/ren/2019/20190910任正非接受《经济学人》采访纪要.html)
## 公众号 | 捐助名单
* [ >> 获取公众号](https://weharmonyos.oss-cn-hangzhou.aliyuncs.com/resources/common/qrcode.jpg)
* [ >> 历史捐助](./donate.md)