[鸿蒙内核源码注释中文版 【 CSDN仓](https://codechina.csdn.net/kuangyufei/kernel_liteos_a_note) | [Gitee仓](https://gitee.com/weharmony/kernel_liteos_a_note) | [Github仓](https://github.com/kuangyufei/kernel_liteos_a_note) | [Coding仓】](https://weharmony.coding.net/public/harmony/kernel_liteos_a_note/git/files) 给 HarmonyOS 源码逐行加上中文注解,详细阐述设计细节, 助你快速精读 HarmonyOS 内核源码, 掌握整个鸿蒙内核运行机制,四大码仓每天同步更新.

[鸿蒙源码分析系列篇 【 CSDN](https://blog.csdn.net/kuangyufei)[ | OSCHINA】](https://my.oschina.net/u/3751245) 从 HarmonyOS 架构层视角整理成文, 并首创用生活场景讲故事的方式试图去解构内核，一窥究竟。

内容仅代表个人观点,会反复修正,出精品注解,写精品文章,一律原创,转载需注明出处,不修改内容,错漏之处欢迎指正第一时间加以完善。

# **[kernel_liteos_a_note](https://gitee.com/weharmony/kernel_liteos_a_note): 鸿蒙内核源码注释中文版**   
每个码农,职业生涯,都应精读一遍内核源码. 鸿蒙内核源码就是很好的精读项目.一旦熟悉内核代码的实现,将迅速拔高对计算机整体理解,从此高屋建瓴看问题.

## **做了些什么呢**
**[kernel_liteos_a_note](https://gitee.com/weharmony/kernel_liteos_a_note)** 是在鸿蒙官方开源项目 **[OpenHarmony/kernel_liteos_a](https://gitee.com/openharmony/kernel_liteos_a)** 基础上给源码加上中文注解的版本,目前已完成部分模块,正持续加注中...

* ### **为何想给鸿蒙源码加上中文注释** 
    
    源于注者大学时阅读linux 2.6 内核痛苦经历,一直有个心愿,想让更多计算机尤其是内核感兴趣的减少阅读时间,加速对计算机系统级的理解,不至于过早的放弃.但因过程种种,一直没有成行,基本要放弃这件事了.
    但9月10日鸿蒙正式开源,重新激活了注者多年的心愿,就有那么点一发不可收拾了 :|P

* ### **致敬鸿蒙内核开发者**
  
    感谢开放原子开源基金会,鸿蒙内核开发者提供了如此优秀的源码,一了多年的夙愿,津津乐道于此.越深入精读内核源码,越能感受到设计者的精巧用心,创新突破. 向开发者致敬. 可以毫不夸张的说 **[OpenHarmony/kernel_liteos_a](https://gitee.com/openharmony/kernel_liteos_a)** 可作为大学C语言,数据结构,操作系统,汇编语言 四门课程的教学项目.如此宝库,不深入研究实在是太可惜了.
    
* ### **理解内核的三个层级**

    笔者认为理解内核需分三个层级:

    第一: **普通概念映射级** 这一级不涉及专业知识,用大众所熟知的公共认知就能听明白是个什么概念,也就是说用一个普通人都懂的概念去诠释或者映射一个他们从没听过的概念.说别人能听得懂的话这很重要!!! 一个没学过计算机知识的卖菜大妈就不可能知道内核的基本运作了吗? NO!,笔者在系列篇中试图用 **[鸿蒙源码分析系列篇|张大爷系列故事【 CSDN](https://blog.csdn.net/kuangyufei)[ | OSCHINA】](https://my.oschina.net/u/3751245)** 去构建这一层级的认知,希望能卷入更多的人来关注基础软件,尤其是那些有钱的投资人加大对国家基础软件的投入.

    第二: **专业概念抽象级** 这一级是抽象出一个专业的逻辑概念,让学过点计算机知识的人能听得懂,可以不用去了解具体的细节点, 比如虚拟内存,老百姓是听不懂的,学过计算机的人都懂,具体怎么实现的很多人又都不懂了,但这并不妨碍成为一个优秀的上层应用程序员,笔者试图用 **[鸿蒙源码分析系列篇 【 CSDN](https://blog.csdn.net/kuangyufei)[ | OSCHINA】](https://my.oschina.net/u/3751245)** 去构建这一层级的认知,希望能卷入更多对内核感兴趣的应用软件人才流入基础软件生态, 应用软件咱们是无敌宇宙,但基础软件却很薄弱.

    第三: **具体微观代码级** 这一级是具体到每一行代码的实现,到了用代码指令级的地步, **[鸿蒙内核源码注释中文版 kernel_liteos_a_note](https://gitee.com/weharmony/kernel_liteos_a_note)** 试图解构这一层级的认知,英文是天生适合设计成编程语言的人类语言,计算机的01码映射到人类世界的26个字母,诞生了太多的伟大奇迹.但我们的母语注定了很大部分人存在着语言层级的映射,希望注释中文版能让更多爱好者参与进来一起研究,拔高咱基础软件的地位.
    
    鸿蒙是面向未来设计的系统,高瞻远瞩,格局远大,设计精良, 知识点巨多, 把研究过程心得写成鸿蒙源码分析系列篇,如此 源码中文注释+系列篇文章 将加速理解鸿蒙内核实现过程.

    系列篇文章 进入 >> [鸿蒙系统源码分析(总目录) 【CSDN](https://blog.csdn.net/kuangyufei) | [OSCHINA】](https://my.oschina.net/u/3751245)查看,两大站点持续更新....感谢CSDN和OSCHINA对博客的推荐支持.

    注释中文版 进入>> [鸿蒙内核源码注释中文版 【 CSDN仓](https://codechina.csdn.net/kuangyufei/kernel_liteos_a_note) | [Gitee仓](https://gitee.com/weharmony/kernel_liteos_a_note) | [Github仓](https://github.com/kuangyufei/kernel_liteos_a_note) | [Coding仓】](https://weharmony.coding.net/public/harmony/kernel_liteos_a_note/git/files)阅读,四大仓库每日同步更新....


* ### **加注释方式是怎样的?**

    因鸿蒙内核6W+代码量,本身只有很少的注释, 中文注解以不对原有代码侵入为前提,源码所有英文部分都是原有鸿蒙注释,所有中文部分都是笔者的注释,尽量不去增加代码的行数,不破坏文件的结构,注释多类似以下的方式,如图:

    ![在这里插入图片描述](https://img-blog.csdnimg.cn/20201022075449282.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L2t1YW5neXVmZWk=,size_16,color_FFFFFF,t_70#pic_center)

    另外用字符画了一些图方便理解,直接嵌入到头文件中,比如虚拟内存的全景图,因没有这些图是很难理解内存是如何管理的,后续还会陆续加入更多的图方便理解.   

    ![在这里插入图片描述](https://img-blog.csdnimg.cn/20201022075929701.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L2t1YW5neXVmZWk=,size_16,color_FFFFFF,t_70#pic_center)

* ### **仰望星空还是埋头走路**
    
    精读内核源码当然是件很困难的事,时间上要以月为单位,但正因为很难才值得去做! 内心不渴望的永远不可能靠近自己.笔者一直坚信兴趣是最好的老师,加注也是在做自己感兴趣的事.如果能让更多人参与到内核的研究,减少学习的成本,哪怕就节省一天的时间,这么多人能节省多少时间, 这是件多好玩,多有意义的事情啊.
    时代需要仰望星空的人,但也需要埋头走路的人, 从鸿蒙一行行的代码中笔者能深深体会到各中艰辛和坚持,及时鸿蒙对未来的价值,只因心中有目标,就不怕道阻且长.

* ### **新增的zzz目录是干什么的?**

    中文加注版比官方版只多了一个zzz的目录,里面放了一些笔者使用的文件,它与内核代码无关,大家可以忽略它,取名zzz是为了排在最后,减少对原有代码目录级的侵入,zzz的想法源于微信中名称为AAA的那批牛人,你的微信里应该也有他们熟悉的身影 :|P

 * ### **笔者联系方式**

    邮箱: kuangyufei@126.com 私信请不要问一些没基础能不能学? 如何看待 鸿蒙 PK Android ? 用了多少linux源码之类的问题. 因为时间太宝贵, 大量的工作要完成. 不建议没经过深度思考就人云亦云,亦步亦趋. 如果非要纠结就想想QQ和微信的关系? 为何有了QQ还得有个微信,而且得由不同的BG来开发. 去翻翻微信刚出来那会有多少看不懂而质疑的声音. 笔者坚信鸿蒙未来一定可以很成功,誓做鸿蒙坚定的追随者和传播者.

 * ### **既然选择了远方,就不要怕天高路远,行动起来!**

    系列篇文章 进入 >> [鸿蒙系统源码分析(总目录) 【CSDN](https://blog.csdn.net/kuangyufei) | [OSCHINA】](https://my.oschina.net/u/3751245)查看

    注释中文版 进入>> [鸿蒙内核源码注释中文版 【 CSDN仓](https://codechina.csdn.net/kuangyufei/kernel_liteos_a_note) | [Gitee仓](https://gitee.com/weharmony/kernel_liteos_a_note) | [Github仓](https://github.com/kuangyufei/kernel_liteos_a_note) | [Coding仓】](https://weharmony.coding.net/public/harmony/kernel_liteos_a_note/git/files)阅读