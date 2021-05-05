[OpenHarmony开发者文档 | 同步最新鸿蒙官方文档 | OpenHarmony | HarmonyOS](http://openharmony.21cloudbox.com/) **[ < 国内访问](http://openharmony.21cloudbox.com/)[ | 国外访问 >](http://openharmony.github.com/)**

[![在这里插入图片描述](https://gitee.com/weharmony/docs/raw/master/pic/other/io.png)](https://gitee.com/weharmony/kernel_liteos_a_note)

百篇博客系列篇.本篇为:

* [v13.xx 鸿蒙内核源码分析(源码注释篇) | 鸿蒙必定成功,也必然成功 ](https://my.oschina.net/u/3751245/blog/4686747) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/109251754)[  .h](https://weharmony.gitee.io/13_源码注释篇.html) [  .o](https://my.oschina.net/weharmony)**

### 几点说明

* [kernel_liteos_a_note](https://gitee.com/weharmony/kernel_liteos_a_note) 是在 开放原子开源基金会 旗下孵化项目 OpenHarmony 的 [kernel_liteos_a](https://gitee.com/openharmony/kernel_liteos_a) (鸿蒙内核项目)基础上给源码加上中文注解的版本.加注版与官方版本保持每月同步.

* OpenHarmony开发者文档 | 同步最新鸿蒙官方文档 | 轻松高效搞定鸿蒙 [ < 国内访问](http://weharmony.gitee.io/history.html)[ | 国外访问 >](http://openharmony.21cloudbox.com/) 是对开放原子开源基金会旗下孵化项目 OpenHarmony 的文档 [docs](https://gitee.com/openharmony/docs) 做的静态站点导读,支持侧边栏,面包屑,从此非常方便的查看官方文档,大大提高学习和开发效率.

* [OpenHarmony全量代码仓库](https://gitee.com/weharmony/OpenHarmony) 是 开放原子开源基金会旗下孵化项目 OpenHarmony 的109个子项目的所有代码.鸿蒙官方是使用`repo`管理众多`git`项目,`repo`在`linux`下很方便,但在`windows`上谁用谁知道,使用会有相当的困难,所以将官方所有项目整合成一个.git工程,如此使用`git`方式便能下载整个鸿蒙系统源码,方便学习使用.本仓库与官方仓库保持同步.[OpenHarmony全量代码仓库](https://gitee.com/weharmony/OpenHarmony)已编译通过,
    ```
    ....
    [OHOS INFO] [1587/1590] STAMP obj/test/xts/acts/build_lite/acts_generate_module_data.stamp
    [OHOS INFO] [1588/1590] ACTION //test/xts/acts/build_lite:acts(//build/lite/toolchain:linux_x86_64_ohos_clang)
    [OHOS INFO] [1589/1590] STAMP obj/test/xts/acts/build_lite/acts.stamp
    [OHOS INFO] [1590/1590] STAMP obj/build/lite/ohos.stamp
    [OHOS INFO] ipcamera_hispark_aries build success
    root@5e3abe332c5a:/home/harmony#
    ```
* 博客站点更新速度:[ 国内](http://weharmony.gitee.io) = [ 国外](http://weharmony.gitee.io) > [ oschina ](https://my.oschina.net/weharmony) > [ 51cto  ](https://harmonyos.51cto.com/column/34) > [ csdn ](https://blog.csdn.net/kuangyufei) 
  
* 下载.鸿蒙源码分析.工具文档 [ < 国内](http://weharmony.gitee.io/history.html)[ | 国外 >](http://weharmony.github.io/history.html)
      
### **为何要精读内核源码?**
* 码农的学职生涯,都应精读一遍内核源码.以浇筑好计算机知识大厦的地基,地基纵深的坚固程度,很大程度能决定未来大厦能盖多高.那为何一定要精读细品呢?
* 因为内核代码本身并不太多,都是浓缩的精华,精读是让各个知识点高频出现,不孤立成点状记忆,没有足够连接点的知识点是很容易忘的,点点成线,线面成体,连接越多,记得越牢,如此短时间内容易结成一张高浓度,高密度的系统化知识网,训练大脑肌肉记忆,驻入大脑直觉区,想抹都抹不掉,终生携带,随时调取.跟骑单车一样,一旦学会,即便多年不骑,照样跨上就走,游刃有余.
### **热爱是所有的理由和答案**
* 因大学时阅读 `linux 2.6` 内核痛并快乐的经历,一直有个心愿,如何让更多对内核感兴趣的朋友减少阅读时间,加速对计算机系统级的理解,而不至于过早的放弃.但因过程种种,多年一直没有行动,基本要放弃这件事了.恰逢 **2020/9/10** 鸿蒙正式开源,重新激活了多年的心愿,就有那么点如黄河之水一发不可收拾了. 
* 到 **2021/3/10** 刚好半年, 对内核源码的注解已完成了 **70%** ,对内核源码的博客分析已完成了**40篇**, 每天都很充实,很兴奋,连做梦内核代码都在鱼贯而入.如此疯狂地做一件事还是当年谈恋爱的时候, 只因热爱, 热爱是所有的理由和答案. :P 
### **(〃･ิ‿･ิ)ゞ鸿蒙内核开发者**
* 感谢开放原子开源基金会,致敬鸿蒙内核开发者提供了如此优秀的源码,一了多年的夙愿,津津乐道于此.精读内核源码加注并整理成档是件很有挑战的事,时间上要以月甚至年为单位,但正因为很难才值得去做! 干困难事,方有所得;专注聚焦,必有所获. 
* 从内核一行行的代码中能深深感受到开发者各中艰辛与坚持,及鸿蒙生态对未来的价值,这些是张嘴就来的网络喷子们永远不能体会到的.可以毫不夸张的说鸿蒙内核源码可作为大学 **C语言**,**数据结构**,**操作系统**,**汇编语言**,**计算机组成原理** 五门课程的教学项目.如此宝库,不深入研究实在是暴殄天物,于心不忍,注者坚信鸿蒙大势所趋,未来可期,其必定成功,也必然成功,誓做其坚定的追随者和传播者.
### **理解内核的三个层级**
* **普通概念映射级:** 这一级不涉及专业知识,用大众所熟知的公共认知就能听明白是个什么概念,也就是说用一个普通人都懂的概念去诠释或者映射一个他们从没听过的概念.让陌生的知识点与大脑中烂熟于心的知识点建立多重链接,加深记忆.说别人能听得懂的话这很重要!!! 一个没学过计算机知识的卖菜大妈就不可能知道内核的基本运作了吗? 不一定!在系列篇中试图用 **[鸿蒙内核源码分析(总目录)之故事篇](https://my.oschina.net/weharmony)** 去引导这一层级的认知,希望能卷入更多的人来关注基础软件,尤其是那些资本大鳄,加大对基础软件的投入.
* **专业概念抽象级:** 对抽象的专业逻辑概念具体化认知, 比如虚拟内存,老百姓是听不懂的,学过计算机的人都懂,具体怎么实现的很多人又都不懂了,但这并不妨碍成为一个优秀的上层应用开发者,因为虚拟内存已经被抽象出来,目的是要屏蔽上层对它具体实现的认知.试图用 **[鸿蒙内核源码分析(总目录)百篇博客](https://my.oschina.net/weharmony)** 去拆解那些已经被抽象出来的专业概念, 希望能卷入更多对内核感兴趣的应用软件人才流入基础软硬件生态, 应用软件咱们是无敌宇宙,但基础软件却很薄弱.
* **具体微观代码级:** 这一级是具体到每一行代码的实现,到了用代码指令级的地步,这段代码是什么意思?为什么要这么设计?有没有更好的方案? **[鸿蒙内核源码注解分析](https://gitee.com/weharmony/kernel_liteos_a_note)** 试图从细微处去解释代码实现层,英文真的是天生适合设计成编程语言的人类语言,计算机的01码映射到人类世界的26个字母,诞生了太多的伟大奇迹.但我们的母语注定了很大部分人存在着自然语言层级的理解映射,希望鸿蒙内核源码注解分析能让更多爱好者快速的理解内核,共同进步.
### **加注方式是怎样的？**

*  因鸿蒙内核6W+代码量,本身只有较少的注释, 中文注解以不对原有代码侵入为前提,源码中所有英文部分都是原有注释,所有中文部分都是中文版的注释,同时为方便同步官方版本的更新,尽量不去增加代码的行数,不破坏文件的结构,注释多类似以下的方式:

    在重要模块的.c/.h文件开始位置先对模块功能做整体的介绍,例如异常接管模块注解如图所示:

    ![在这里插入图片描述](https://gitee.com/weharmony/docs/raw/master/pic/other/ycjg.png)

    注解过程中查阅了很多的资料和书籍,在具体代码处都附上了参考链接.

* 而函数级注解会详细到重点行,甚至每一行, 例如申请互斥锁的主体函数,不可谓不重要,而官方注释仅有一行,如图所示

    ![在这里插入图片描述](https://gitee.com/weharmony/docs/raw/master/pic/other/sop.png)

* 另外画了一些字符图方便理解,直接嵌入到头文件中,比如虚拟内存的全景图,因没有这些图是很难理解虚拟内存是如何管理的.

    ![在这里插入图片描述](https://gitee.com/weharmony/docs/raw/master/pic/other/vm.png)
### **有哪些特殊的记号**
* 搜索 **`@note_pic`** 可查看绘制的全部字符图
* 搜索 **`@note_why`** 是尚未看明白的地方，有看明白的，请Pull Request完善
* 搜索 **`@note_thinking`** 是一些的思考和建议
* 搜索 **`@note_#if0`** 是由第三方项目提供不在内核源码中定义的极为重要结构体，为方便理解而添加的。
* 搜索 **`@note_good`** 是给源码点赞的地方
### **新增zzz目录**
* 中文加注版比官方版无新增文件,只多了一个zzz的目录,里面放了一些文件,它与内核代码无关,大家可以忽略它,取名zzz是为了排在最后,减少对原有代码目录级的侵入,zzz的想法源于微信中名称为AAA的那帮朋友,你的微信里应该也有他们熟悉的身影吧 :|P
### **同步官方源码历史**
* 每月同步一次
* `2021/4/21` -- 官方优化了很多之前吐槽的地方,点赞.
* `2020/9/16` -- 中文注解版起点
### **百篇博客.往期回顾**
> 在加注过程中,整理出以下文章.内容立足源码,常以生活场景打比方尽可能多的将内核知识点置入某种场景,具有画面感,容易理解记忆.
说别人能听得懂的话很重要! 百篇博客绝不是百度教条式的在说一堆诘屈聱牙的概念,那没什么意思.更希望让内核变得栩栩如生,倍感亲切.确实有难度,自不量力,但已经出发,回头已是不可能的了.:P
与写代码有bug需不断debug一样,文章和注解内容会将错漏之处反复修正,持续更新,`.xx`代表修改的次数,精雕细琢,言简意赅,力求打造精品内容. 

* [v51.xx 鸿蒙内核源码分析(ELF格式篇) | 应用程序入口并不是main ](https://my.oschina.net/weharmony/blog/5030288) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/116097977)[  .h](https://weharmony.gitee.io/51_ELF格式篇.html)[  .o](https://my.oschina.net/weharmony)** 
  
* [v50.xx 鸿蒙内核源码分析(编译环境篇) | 编译鸿蒙看这篇或许真的够了 ](https://my.oschina.net/weharmony/blog/5028613) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/116042551)[  .h](https://weharmony.gitee.io/50_编译环境篇.html) [  .o](https://my.oschina.net/weharmony)** 
  
* [v49.xx 鸿蒙内核源码分析(信号消费篇) | 谁让CPU连续四次换栈运行 ](https://my.oschina.net/weharmony/blog/5027224) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/115958293)[  .h](https://weharmony.gitee.io/49_信号消费篇.html) [  .o](https://my.oschina.net/weharmony)** 

* [v48.xx 鸿蒙内核源码分析(信号生产篇) | 年过半百,依然活力十足 ](https://my.oschina.net/weharmony/blog/5022149) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/115768099)[  .h](https://weharmony.gitee.io/48_信号生产篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v47.xx 鸿蒙内核源码分析(进程回收篇) | 临终前如何向老祖宗托孤 ](https://my.oschina.net/weharmony/blog/5017716) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/115672752)[  .h](https://weharmony.gitee.io/47_进程回收篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v46.xx 鸿蒙内核源码分析(特殊进程篇) | 龙生龙凤生凤老鼠生儿会打洞 ](https://my.oschina.net/weharmony/blog/5014444) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/115556505)[  .h](https://weharmony.gitee.io/46_特殊进程篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v45.xx 鸿蒙内核源码分析(Fork篇) | 一次调用,两次返回 ](https://my.oschina.net/weharmony/blog/5010301) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/115467961)[  .h](https://weharmony.gitee.io/45_Fork篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v44.xx 鸿蒙内核源码分析(中断管理篇) | 江湖从此不再怕中断 ](https://my.oschina.net/weharmony/blog/4995800) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/115130055)[  .h](https://weharmony.gitee.io/44_中断管理篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v43.xx 鸿蒙内核源码分析(中断概念篇) | 海公公的日常工作 ](https://my.oschina.net/weharmony/blog/4992750) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/115014442)[  .h](https://weharmony.gitee.io/43_中断概念篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v42.xx 鸿蒙内核源码分析(中断切换篇) | 系统因中断活力四射](https://my.oschina.net/weharmony/blog/4990948) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/114988891)[  .h](https://weharmony.gitee.io/42_中断切换篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v41.xx 鸿蒙内核源码分析(任务切换篇) | 看汇编如何切换任务 ](https://my.oschina.net/weharmony/blog/4988628) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/114890180)[  .h](https://weharmony.gitee.io/41_任务切换篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v40.xx 鸿蒙内核源码分析(汇编汇总篇) | 汇编可爱如邻家女孩 ](https://my.oschina.net/weharmony/blog/4977924) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/114597179)[  .h](https://weharmony.gitee.io/40_汇编汇总篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v39.xx 鸿蒙内核源码分析(异常接管篇) | 社会很单纯,复杂的是人 ](https://my.oschina.net/weharmony/blog/4973016) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/114438285)[  .h](https://weharmony.gitee.io/39_异常接管篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v38.xx 鸿蒙内核源码分析(寄存器篇) | 小强乃宇宙最忙存储器 ](https://my.oschina.net/weharmony/blog/4969487) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/114326994)[  .h](https://weharmony.gitee.io/38_寄存器篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v37.xx 鸿蒙内核源码分析(系统调用篇) | 开发者永远的口头禅 ](https://my.oschina.net/weharmony/blog/4967613) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/114285166)[  .h](https://weharmony.gitee.io/37_系统调用篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v36.xx 鸿蒙内核源码分析(工作模式篇) | CPU是韦小宝,七个老婆 ](https://my.oschina.net/weharmony/blog/4965052) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/114168567)[  .h](https://weharmony.gitee.io/36_工作模式篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v35.xx 鸿蒙内核源码分析(时间管理篇) | 谁是内核基本时间单位 ](https://my.oschina.net/weharmony/blog/4956163) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113867785)[  .h](https://weharmony.gitee.io/35_时间管理篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v34.xx 鸿蒙内核源码分析(原子操作篇) | 谁在为原子操作保驾护航 ](https://my.oschina.net/weharmony/blog/4955290) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113850603)[  .h](https://weharmony.gitee.io/34_原子操作篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v33.xx 鸿蒙内核源码分析(消息队列篇) | 进程间如何异步传递大数据 ](https://my.oschina.net/weharmony/blog/4952961) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113815355)[  .h](https://weharmony.gitee.io/33_消息队列篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v32.xx 鸿蒙内核源码分析(CPU篇) | 整个内核就是一个死循环 ](https://my.oschina.net/weharmony/blog/4952034) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113782749)[  .h](https://weharmony.gitee.io/32_CPU篇.html) [  .o](https://my.oschina.net/weharmony)**  

* [v31.xx 鸿蒙内核源码分析(定时器篇) | 哪个任务的优先级最高 ](https://my.oschina.net/weharmony/blog/4951625) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113774260)[  .h](https://weharmony.gitee.io/31_定时器篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v30.xx 鸿蒙内核源码分析(事件控制篇) | 任务间多对多的同步方案 ](https://my.oschina.net/weharmony/blog/4950956) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113759481)[  .h](https://weharmony.gitee.io/30_事件控制篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v29.xx 鸿蒙内核源码分析(信号量篇) | 谁在负责解决任务的同步 ](https://my.oschina.net/weharmony/blog/4949720) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113744267)[  .h](https://weharmony.gitee.io/29_信号量篇.html) [  .o](https://my.oschina.net/weharmony)**
  
* [v28.xx 鸿蒙内核源码分析(进程通讯篇) | 九种进程间通讯方式速揽 ](https://my.oschina.net/weharmony/blog/4947398) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113700751)[  .h](https://weharmony.gitee.io/28_进程通讯篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v27.xx 鸿蒙内核源码分析(互斥锁篇) | 比自旋锁丰满的互斥锁 ](https://my.oschina.net/weharmony/blog/4945465) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113660357)[  .h](https://weharmony.gitee.io/27_互斥锁篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v26.xx 鸿蒙内核源码分析(自旋锁篇) | 自旋锁当立贞节牌坊 ](https://my.oschina.net/weharmony/blog/4944129) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113616250)[  .h](https://weharmony.gitee.io/26_自旋锁篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v25.xx 鸿蒙内核源码分析(并发并行篇) | 听过无数遍的两个概念 ](https://my.oschina.net/u/3751245/blog/4940329) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113516222)[  .h](https://weharmony.gitee.io/25_并发并行篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v24.xx 鸿蒙内核源码分析(进程概念篇) | 进程在管理哪些资源 ](https://my.oschina.net/u/3751245/blog/4937521) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113395872)[  .h](https://weharmony.gitee.io/24_进程概念篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v23.xx 鸿蒙内核源码分析(汇编传参篇) | 如何传递复杂的参数 ](https://my.oschina.net/u/3751245/blog/4927892) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/113265990)[  .h](https://weharmony.gitee.io/23_汇编传参篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v22.xx 鸿蒙内核源码分析(汇编基础篇) | CPU在哪里打卡上班 ](https://my.oschina.net/u/3751245/blog/4920361) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/112986628)[  .h](https://weharmony.gitee.io/22_汇编基础篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v21.xx 鸿蒙内核源码分析(线程概念篇) | 是谁在不断的折腾CPU ](https://my.oschina.net/u/3751245/blog/4915543) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/112870193)[  .h](https://weharmony.gitee.io/21_线程概念篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v20.xx 鸿蒙内核源码分析(用栈方式篇) | 程序运行场地由谁提供 ](https://my.oschina.net/u/3751245/blog/4893388) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/112534331)[  .h](https://weharmony.gitee.io/20_用栈方式篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v19.xx 鸿蒙内核源码分析(位图管理篇) | 谁能一分钱分两半花 ](https://my.oschina.net/u/3751245/blog/4888467) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/112394982)[  .h](https://weharmony.gitee.io/19_位图管理篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v18.xx 鸿蒙内核源码分析(源码结构篇) | 内核每个文件的含义 ](https://my.oschina.net/u/3751245/blog/4869137) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/111938348)[  .h](https://weharmony.gitee.io/18_源码结构篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v17.xx 鸿蒙内核源码分析(物理内存篇) | 怎么管理物理内存 ](https://my.oschina.net/u/3751245/blog/4842408) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/111765600)[  .h](https://weharmony.gitee.io/17_物理内存篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v16.xx 鸿蒙内核源码分析(内存规则篇) | 内存管理到底在管什么 ](https://my.oschina.net/u/3751245/blog/4698384) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/109437223)[  .h](https://weharmony.gitee.io/16_内存规则篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v15.xx 鸿蒙内核源码分析(内存映射篇) | 虚拟内存虚在哪里 ](https://my.oschina.net/u/3751245/blog/4694841) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/109032636)[  .h](https://weharmony.gitee.io/15_内存映射篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v14.xx 鸿蒙内核源码分析(内存汇编篇) | 谁是虚拟内存实现的基础 ](https://my.oschina.net/u/3751245/blog/4692156) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108994081)[  .h](https://weharmony.gitee.io/14_内存汇编篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v13.xx 鸿蒙内核源码分析(源码注释篇) | 鸿蒙必定成功,也必然成功 ](https://my.oschina.net/u/3751245/blog/4686747) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/109251754)[  .h](https://weharmony.gitee.io/13_源码注释篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v12.xx 鸿蒙内核源码分析(内存管理篇) | 虚拟内存全景图是怎样的 ](https://my.oschina.net/u/3751245/blog/4652284) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108821442)[  .h](https://weharmony.gitee.io/12_内存管理篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v11.xx 鸿蒙内核源码分析(内存分配篇) | 内存有哪些分配方式 ](https://my.oschina.net/u/3751245/blog/4646802) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108989906)[  .h](https://weharmony.gitee.io/11_内存分配篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v10.xx 鸿蒙内核源码分析(内存主奴篇) | 皇上和奴才如何相处 ](https://my.oschina.net/u/3751245/blog/4646802) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108723672)[  .h](https://weharmony.gitee.io/10_内存主奴篇.html) [  .o](https://my.oschina.net/weharmony)**
  
* [v09.xx 鸿蒙内核源码分析(调度故事篇) | 用故事说内核调度过程 ](https://my.oschina.net/u/3751245/blog/4634668) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108745174)[  .h](https://weharmony.gitee.io/09_调度故事篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v08.xx 鸿蒙内核源码分析(总目录) | 百万汉字注解 百篇博客分析 ](https://my.oschina.net/weharmony/blog/4626852) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108727970)[  .h](https://weharmony.gitee.io/08_总目录.html) [  .o](https://my.oschina.net/weharmony)**
  
* [v07.xx 鸿蒙内核源码分析(调度机制篇) | 任务是如何被调度执行的 ](https://my.oschina.net/u/3751245/blog/4623040) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108705968)[  .h](https://weharmony.gitee.io/07_调度机制篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v06.xx 鸿蒙内核源码分析(调度队列篇) | 内核有多少个调度队列 ](https://my.oschina.net/u/3751245/blog/4606916) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108626671)[  .h](https://weharmony.gitee.io/06_调度队列篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v05.xx 鸿蒙内核源码分析(任务管理篇) | 任务池是如何管理的 ](https://my.oschina.net/u/3751245/blog/4603919) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108661248)[  .h](https://weharmony.gitee.io/05_任务管理篇.html) [  .o](https://my.oschina.net/weharmony)**
  
* [v04.xx 鸿蒙内核源码分析(任务调度篇) | 任务是内核调度的单元 ](https://my.oschina.net/weharmony/blog/4595539) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108621428)[  .h](https://weharmony.gitee.io/04_任务调度篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v03.xx 鸿蒙内核源码分析(时钟任务篇) | 触发调度谁的贡献最大 ](https://my.oschina.net/u/3751245/blog/4574493) **[  | 51](https://harmonyos.51cto.com/column/34)[ .c](https://blog.csdn.net/kuangyufei/article/details/108603468)[  .h](https://weharmony.gitee.io/03_时钟任务篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v02.xx 鸿蒙内核源码分析(进程管理篇) | 谁在管理内核资源 ](https://my.oschina.net/u/3751245/blog/4574429) **[  | 51](https://harmonyos.51cto.com/posts/3926)[ .c](https://blog.csdn.net/kuangyufei/article/details/108595941)[  .h](https://weharmony.gitee.io/02_进程管理篇.html) [  .o](https://my.oschina.net/weharmony)**

* [v01.xx 鸿蒙内核源码分析(双向链表篇) | 谁是内核最重要结构体 ](https://my.oschina.net/u/3751245/blog/4572304) **[  | 51](https://harmonyos.51cto.com/posts/3925)[ .c](https://blog.csdn.net/kuangyufei/article/details/108585659)[  .h](https://weharmony.gitee.io/01_双向链表篇.html) [  .o](https://my.oschina.net/weharmony)**

### 关于 51 .c .h .o
看系列篇文章会常看到 `51 .c .h .o`,希望这对大家阅读不会造成影响. 
分别对应以下四个站点的首个字符,感谢这些站点一直以来对系列篇的支持和推荐,尤其是 **oschina gitee** ,很喜欢它的界面风格,简洁大方,让人感觉到开源的伟大!
* [51cto](https://harmonyos.51cto.com/column/34)
* [csdn](https://blog.csdn.net/kuangyufei)
* [harmony](https://weharmony.gitee.io/)
* [oschina](https://my.oschina.net/weharmony)
  
而巧合的是`.c .h .o`是C语言的头/源/目标文件,这就很有意思了,冥冥之中似有天数,将这四个宝贝以这种方式融合在一起. `51 .c .h .o` , 我要CHO ,嗯嗯,hin 顺口 : )

### 百万汉字注解.百篇博客分析
[百万汉字注解 >> 精读鸿蒙源码,中文注解分析, 深挖地基工程,大脑永久记忆,四大码仓每日同步更新](https://gitee.com/weharmony/kernel_liteos_a_note)[< gitee ](https://gitee.com/weharmony/kernel_liteos_a_note)[| github ](https://github.com/kuangyufei/kernel_liteos_a_note)[| csdn ](https://codechina.csdn.net/kuangyufei/kernel_liteos_a_note)[| coding >](https://weharmony.coding.net/public/harmony/kernel_liteos_a_note/git/files)

[百篇博客分析 >> 故事说内核,问答式导读,生活式比喻,表格化说明,图形化展示,主流站点定期更新中](http://weharmony.gitee.io)[< 51cto  ](https://harmonyos.51cto.com/column/34)[| csdn ](https://blog.csdn.net/kuangyufei)[| harmony ](http://weharmony.gitee.io/history.html)[ | osc >](https://my.oschina.net/weharmony)

### 关注不迷路.代码即人生
[![鸿蒙内核源码分析](https://gitee.com/weharmony/docs/raw/master/pic/other/so1so.png)](https://gitee.com/weharmony/docs/raw/master/pic/other/so1so.png)

[热爱是所有的理由和答案 - turing](https://weharmony.gitee.io/)

原创不易,欢迎转载,但麻烦请注明出处.