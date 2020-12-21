[鸿蒙内核源码注释中文版 【 Gitee仓 ](https://gitee.com/weharmony/kernel_liteos_a_note) | [ CSDN仓 ](https://codechina.csdn.net/kuangyufei/kernel_liteos_a_note) | [ Github仓 ](https://github.com/kuangyufei/kernel_liteos_a_note) | [ Coding仓 】](https://weharmony.coding.net/public/harmony/kernel_liteos_a_note/git/files)  给 HarmonyOS 源码加上中文注解,详细阐述架构设计和代码实现细节, 快速精读鸿蒙内核源码, 掌握整个鸿蒙内核运行机制,四大码仓每日同步更新.

[鸿蒙源码分析系列篇 【 CSDN ](https://blog.csdn.net/kuangyufei/article/details/108727970) [| OSCHINA ](https://my.oschina.net/u/3751245/blog/4626852) [| WIKI 】](https://gitee.com/weharmony/kernel_liteos_a_note/wikis/pages) 从 HarmonyOS 架构层视角整理成文, 并首创用生活场景讲故事的方式试图去解构内核，一窥究竟。

# **[kernel\_liteos\_a_note](https://gitee.com/weharmony/kernel_liteos_a_note): 鸿蒙内核源码注释中文版**

[![star](https://gitee.com/weharmony/kernel_liteos_a_note/badge/star.svg?theme=dark)](https://gitee.com/weharmony/kernel_liteos_a_note)[![fork](https://gitee.com/weharmony/kernel_liteos_a_note/badge/fork.svg?theme=dark)](https://gitee.com/weharmony/kernel_liteos_a_note)

项目中文注解鸿蒙官方内核源码,图文并茂,详细阐述鸿蒙架构和代码设计细节,每个码农,学职生涯,都应精读一遍内核源码.
精读内核源码最大的好处是:将孤立知识点织成一张高浓度,高密度底层网,对计算机底层体系化理解形成永久记忆,从此高屋建瓴分析/解决问题.

## **做了些什么呢**

**[kernel\_liteos\_a_note](https://gitee.com/weharmony/kernel_liteos_a_note)** 是在鸿蒙官方开源项目 **[OpenHarmony/kernel\_liteos\_a](https://gitee.com/openharmony/kernel_liteos_a)** 基础上给源码加上中文注解的版本,目前几大核心模块加注已基本完成,**整体加注完成70%**,其余正持续加注完善中...

-   ### **为何想给鸿蒙源码加上中文注释**
    
    源于大学时阅读linux 2.6 内核痛苦经历,一直有个心愿,想让更多计算机尤其是内核感兴趣的减少阅读时间,加速对计算机系统级的理解,不至于过早的放弃.但因过程种种,一直没有成行,基本要放弃这件事了. 但9月10日鸿蒙正式开源,重新激活了注者多年的心愿,就有那么点一发不可收拾了 :|P
    
-   ### **致敬鸿蒙内核开发者**
    
    感谢开放原子开源基金会,鸿蒙内核开发者提供了如此优秀的源码,一了多年的夙愿,津津乐道于此.越深入精读内核源码,越能感受到设计者的精巧用心,创新突破, 向开发者致敬. 可以毫不夸张的说鸿蒙内核源码可作为大学C语言,数据结构,操作系统,汇编语言 四门课程的教学项目.如此宝库,不深入研究实在是暴殄天物,于心不忍.

-   ### **在加注的源码中有哪些特殊的记号**

    搜索 **[@note_pic]()** 可查看绘制的全部字符图

    搜索 **[@note_why]()** 是注者尚未看明白的地方，如果您看明白了，请告诉注者完善

    搜索 **[@note_thinking]()** 是注者的思考和吐槽鸿蒙源码的地方
    
-   ### **理解内核的三个层级**
    
    笔者认为理解内核可分三个层级:
    
    第一: **普通概念映射级** 这一级不涉及专业知识,用大众所熟知的公共认知就能听明白是个什么概念,也就是说用一个普通人都懂的概念去诠释或者映射一个他们从没听过的概念.说别人能听得懂的话这很重要!!! 一个没学过计算机知识的卖菜大妈就不可能知道内核的基本运作了吗? NO!,笔者在系列篇中试图用 **[鸿蒙源码分析系列篇|张大爷系列故事【 CSDN](https://blog.csdn.net/kuangyufei/article/details/108727970) [| OSCHINA](https://my.oschina.net/u/3751245/blog/4626852) [| WIKI 】](https://gitee.com/weharmony/kernel_liteos_a_note/wikis/pages)** 去引导这一层级的认知,希望能卷入更多的人来关注基础软件,尤其是那些有钱的投资人加大对基础软件的投入.
    
    第二: **专业概念抽象级** 这一级是抽象出一个专业的逻辑概念,让学过点计算机知识的人能听得懂,可以不用去了解具体的细节点, 比如虚拟内存,老百姓是听不懂的,学过计算机的人都懂,具体怎么实现的很多人又都不懂了,但这并不妨碍成为一个优秀的上层应用程序员,笔者试图用 **[鸿蒙源码分析系列篇 【 CSDN](https://blog.csdn.net/kuangyufei/article/details/108727970) [| OSCHINA](https://my.oschina.net/u/3751245/blog/4626852) [| WIKI 】](https://gitee.com/weharmony/kernel_liteos_a_note/wikis/pages)** 从宏观尺度去构建这一层级的认知,希望能卷入更多对内核感兴趣的应用软件人才流入基础软件生态, 应用软件咱们是无敌宇宙,但基础软件却很薄弱.
    
    第三: **具体微观代码级** 这一级是具体到每一行代码的实现,到了用代码指令级的地步,这段代码是什么意思?为什么要这么设计? **[鸿蒙内核源码注释中文版 kernel\_liteos\_a_note](https://gitee.com/weharmony/kernel_liteos_a_note)** 试图从细微处去拆解这一层级的认知,英文是天生适合设计成编程语言的人类语言,计算机的01码映射到人类世界的26个字母,诞生了太多的伟大奇迹.但我们的母语注定了很大部分人存在着自然语言层级的映射,希望注释中文版能让更多爱好者快速的理解内核,参与到基础软件的研究和开发中.
    
    鸿蒙是面向未来设计的系统,高瞻远瞩,格局远大,设计精良, 海量知识点, 目前做的只是冰山一角,把研究过程心得写成鸿蒙源码分析系列篇,如此源码中文注释+系列篇文章 将加速理解鸿蒙内核实现过程.
    
-   ### **加注释方式是怎样的?**
    
    因鸿蒙内核6W+代码量,本身只有很少的注释, 中文注解以不对原有代码侵入为前提,源码中所有英文部分都是原有鸿蒙注释,所有中文部分都是笔者的注释,尽量不去增加代码的行数,不破坏文件的结构,笔者试图把每个知识点当场讲透彻,注释多类似以下的方式, 如图举例: 这是申请互斥锁的主体函数,不可谓不重要,但官方注释仅有一行.
    
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/2020102821375267.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L2t1YW5neXVmZWk=,size_16,color_FFFFFF,t_70#pic_center)
    
    另外用字符画了一些图方便理解,直接嵌入到头文件中,比如虚拟内存的全景图,因没有这些图是很难理解内存是如何管理的,后续还会陆续加入更多的图方便理解.
    
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/20201028154344813.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L2t1YW5neXVmZWk=,size_16,color_FFFFFF,t_70#pic_center)
    
-   ### **干困难事,必有所得**
    
    精读内核源码当然是件很困难的事,时间上要以月为单位,正因为很难才值得去做! 内心不渴望的永远不可能靠近自己.笔者一直坚信兴趣是最好的老师,加注也是在做自己感兴趣的事.如果能让更多人参与到内核的研究,减少学习的成本,哪怕就节省一天的时间,这么多人能节省多少时间, 这是件多好玩,多有意义的事情. 从内核一行行的代码中能深深体会到开发者各中艰辛与坚持,及鸿蒙生态对未来的价值,笔者坚信鸿蒙大势所趋,未来可期,是其坚定的追随者和传播者.
    
-   ### **新增的zzz目录是干什么的?**
    
    中文加注版比官方版无新增文件,只多了一个zzz的目录,里面放了一些笔者使用的文件,它与内核代码无关,大家可以忽略它,取名zzz是为了排在最后,减少对原有代码目录级的侵入,zzz的想法源于微信中名称为AAA的那批牛人,你的微信里应该也有他们熟悉的身影吧 :|P  

-   ### **参与贡献**
    1. Fork 本仓库 >> 新建 Feat_xxx 分支 >> 提交代码注解 >> 新建 Pull Request

    2. [新建 Issue](https://gitee.com/weharmony/kernel_liteos_a_note/issues)
-   ### **联系方式**
    kuangyufei@126.com,有事请邮件/私信笔者, 抱歉因加注占用了全部空闲时间,所以无法微信回复.
    
    ---

    系列篇文章 进入 >\> [鸿蒙系统源码分析(总目录) 【 CSDN](https://blog.csdn.net/kuangyufei/article/details/108727970) | [OSCHINA](https://my.oschina.net/u/3751245/blog/4626852) [| WIKI 】](https://gitee.com/weharmony/kernel_liteos_a_note/wikis/pages)查看
    
    注释中文版 进入 >\> [鸿蒙内核源码注释中文版 【 Gitee仓](https://gitee.com/weharmony/kernel_liteos_a_note) | [CSDN仓](https://codechina.csdn.net/kuangyufei/kernel_liteos_a_note) | [Github仓](https://github.com/kuangyufei/kernel_liteos_a_note) | [Coding仓 】](https://weharmony.coding.net/public/harmony/kernel_liteos_a_note/git/files)阅读
    