# **kernel_liteos_a_note: 鸿蒙内核源码注释中文版**   
每个码农，职业生涯, 都应精读一遍内核源码 .献给那些对内核源码真正有兴趣的人.兴趣是最好的老师!

## **做了些什么呢**
**kernel_liteos_a_note** 是在鸿蒙官方内核开源项目 **[kernel_liteos_a](https://gitee.com/openharmony/kernel_liteos_a)** 基础上给源码加上中文注释的版本,目前已完成部分模块,正在持续加注中....

* ### **为何想给源码加上中文注释** 
    
    源于注者大学时阅读linux2.6内核痛苦经历,一直有个心结,想让更多对内核感兴趣的同学减少阅读时间,加速源码的理解,不至于过早的放弃.但因过程种种,一直没有成行,基本要放弃这件事了.
    9月11日鸿蒙正式开源,重新燃起了注者多年的这把火,有那么点一发可收拾了 :|P
    
* ### **致敬鸿蒙内核开发者**
  
    感谢开放原子开源基金会,鸿蒙内核开发者提供了如此优秀的源码,让笔者一了多年的夙愿,津津乐道于此.越深入精读内核源码,越能感受到设计者的精巧用心,创新突破. 向开发者致敬ppp. 可以毫不夸张的说 kernel_liteos_a 可作为大学C语言,数据结构,操作系统,汇编语言 四门课程的教学项目.如此宝库,不深入研究实在是太可惜了.
    
* ### **系列篇和源码注释怎么更新**

    好记性不如烂笔头,笔者把研究过程心得写成鸿蒙源码分析系列篇,如此源码注释结合系列篇文章理解鸿蒙内核实现会更彻底.
    
    系列篇文章查看: 进入>> 鸿蒙源码分析系列篇 [CSDN站](https://blog.csdn.net/kuangyufei) | [OSCHINA站](https://my.oschina.net/u/3751245) 查看, 正在持续更新中..., 感谢CSDN, OSCHINA 对博文的推荐.
    
    注释中文版查看: 进入>> 鸿蒙内核源码注释中文版 [CSDN仓库](https://codechina.csdn.net/kuangyufei/kernel_liteos_a_note) | [Gitee仓库 ](https://gitee.com/weharmony/kernel_liteos_a_note) | [Github仓库](https://github.com/kuangyufei/kernel_liteos_a_note) | [Coding仓库](https://weharmony.coding.net/public/harmony/kernel_liteos_a_note/git/files) 查看,四大仓库同步更新. 正在持续加注中....
    
    精读内核源码当然是件很困难的事,但正因为很难才值得去做! 内心不渴望的永远不可能靠近自己.别再去纠结而没有行动.笔者一直坚信兴趣是最好的老师,加注就是在做自己感兴趣的事, 希望感兴趣的各位能看到.如果能让更多人参与到内核的研究,减少学习的成本,哪怕就节省一天的时间,这么多人能节省多少时间, 这是件多好玩,多有意义的事情啊.

    系列篇和源码注释一直在反复修改更新,工作量很大,然兴趣所致,乐此不疲. 其中所写所注仅代表个人观点,肯定会有错漏之处,请多指正完善.

* ### **加注释方式是怎样的?**

    因鸿蒙内核本身只有很少的注释, 所以笔者不能去破坏原有的结构,注释以不对原有代码侵入为前提,源码所有英文部分都是原有鸿蒙注释,所有中文部分都是笔者的注释,尽量不去增加代码的行数,不破坏文件的结构,注释多类似以下的方式,如图:
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/20201022075449282.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L2t1YW5neXVmZWk=,size_16,color_FFFFFF,t_70#pic_center)

    另外笔者用字符画了一些图方便理解,直接嵌入到头文件中,比如虚拟内存的全景图,因没有这些图是很难理解内存是如何管理的,后续还会陆续加入更多的图方便理解.   

    ![在这里插入图片描述](https://img-blog.csdnimg.cn/20201022075929701.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L2t1YW5neXVmZWk=,size_16,color_FFFFFF,t_70#pic_center)

* ### **新增的zzz目录是干什么的?**

    中文加注版比官方版多了一个zzz的目录,里面放了一些笔者使用的文件,比如测试代码,它与内核代码无关,大家可以忽略它,取名zzz是为了排在最后,减少对原有代码目录级的侵人,zzz的想法源于微信中名称为AAA的那批用户,你的微信里也有吗? :|P

 * ### **笔者联系方式**

    邮箱: kuangyufei@126.com 私信请不要问一些没基础能不能学? 如何看 鸿蒙 PK Android ? 用了多少linux源码之类的问题. 因为时间太宝贵了,还有大量的工作要完成. 也请不要没有经过深度思考就人云亦云 . 如果非要纠结就想想QQ和微信的关系? 为何有了QQ还得有个微信,而且得由不同的BG来开发. 去翻翻微信刚出来那会有多少质疑的声音吧. 笔者坚信鸿蒙未来一定可以很成功,是它坚定的追随者和传播者.

 * ### **既然选择了远方,就不要怕天高路远,行动起来!**
