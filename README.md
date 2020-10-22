# kernel_liteos_a_note: *鸿蒙内核源码加注中文版*   

每个码农，职业生涯, 都应精度一遍内核源码

## 做了些什么呢

kernel_liteos_a_note是基于鸿蒙开源内核kernel_liteos_a源码的注释中文版本,目前已基本完成进程,task,内存管理模块的加注,正在持续加注中....

* **为何想给源码加上中文注释** 
    
   源于注者大学时阅读linux2.6内核痛苦经历,一直有个心结,想让更多对内核感兴趣的同学减少阅读时间,加速源码的理解,不至于过早的放弃.但因过程种种,一直没有成行,基本要放弃这件事了.
   9月11日鸿蒙正式开源,重新燃起了注者多年的这把火,有那么点一发可收拾了 :|P
    
* **致敬鸿蒙内核开发者**
  
   感谢开放原子开源基金会,鸿蒙内核开发者提供了如此优秀的源码,让笔者一了多年的夙愿,津津乐道与此.越深入精读内核源码,越能感受到设计者的精巧用心,创新突破. 向开发者致敬ppp. 毫不夸张的说 kernel_liteos_a 可作为大学C语言,数据结构,操作系统,汇编语言 四门课程的教学项目.不深入研究会觉得可惜了.
    
* **博文和源码注释怎么更新**

    好记性不如烂笔头,笔者把研究过程心得写了 [鸿蒙源码分析系列篇(CSDN)](https://blog.csdn.net/kuangyufei) ,[鸿蒙源码分析系列篇(oschina)](https://my.oschina.net/u/3751245)  持续更新中...,感谢CSDN,oschina 对博文的推荐
    加注源码在 [CSDN仓库](https://codechina.csdn.net/kuangyufei/kernel_liteos_a_note) [Gitee仓库 ](https://gitee.com/weharmony/kernel_liteos_a_note)[Github仓库](https://github.com/kuangyufei/kernel_liteos_a_note) 更新,后面的会稍微延期.
    
    精读内核源码当然是件很困难的事,但正因为很难才值得去做啊! 内心不渴望的永远不可能会接近自己.不要去纠结而没有行动.笔者一直坚信兴趣是最好的老师,也是在做自己感兴趣的事, 希望感兴趣的人能看到.如果能让更多人参与到内核的研究,减少学习的成本,哪怕就节省一天的时间,这么多人能节省多少时间, 这是件多好玩,多有意义的事情啊.

* **加注释方式**
* 
    因鸿蒙内核本身有很少的注释, 所以笔者不能去破坏原有的结构,以减少对原有代码的侵入,可理解为英文部分都是原有注释,中文部分都是笔者的注释,类似以下的方式,没有增加代码的行数.
![在这里插入图片描述](https://img-blog.csdnimg.cn/20201022075449282.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L2t1YW5neXVmZWk=,size_16,color_FFFFFF,t_70#pic_center)
另外笔者用字符也画了一些图方便理解,直接嵌入到头文件中,比如虚拟内存的整体图和用户空间图,没有这些图就很难理解内存是如何管理的.   
![在这里插入图片描述](https://img-blog.csdnimg.cn/20201022075929701.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L2t1YW5neXVmZWk=,size_16,color_FFFFFF,t_70#pic_center)

* **zzz目录是什么?**

中文加注版比官方版多了一个zzz的目录,里面放了一些笔者使用的文件,比如测试代码,与内核代码无关,大家可以忽略它,取名zzz目录排在最后,也是为了减少对原有代码的目录级的入侵,zzz的想法来源于微信中的 AAA的那批用户, :|P

 * **笔者联系方式**

 邮箱: kuangyufei@126.com 私信请不要问一些没基础能不能学? 如何看 鸿蒙 PK Android ? 看看有多少linux源码之类的问题. 不要没有经过深度思考就人云亦云 . 如果非要纠结就想想 QQ和微信的关系? 为何有了QQ还得有个微信,而且得由不同的BG来开发. 去翻翻微信刚出来那会有多少质疑的声音.笔者坚信鸿蒙未来一定可以成功,是它坚定的追随者和传播者.

 * **既然选择了远方,就不要怕天高路远,行动起来!**
***
