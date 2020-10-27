git add -A
git commit -m  '互斥锁部分两个重要函数的注解,task在运行过程中优先级会变化,变化由taskCB->priBitMap记录
鸿蒙内核源码分析系列 https://blog.csdn.net/kuangyufei https://my.oschina.net/u/3751245
鸿蒙内核源码注释中文版 【 CSDN仓 | Gitee仓 | Github仓 | Coding仓】四大仓库每日同步更新
给鸿蒙内核源码逐行加上中文注解,详细阐述设计细节, 助你快速精读 HarmonyOS 内核源码, 掌握整个鸿蒙内核运行机制.'

git push origin
git push gitee_origin master
git push github_origin master
git push coding_origin master

#git remote add github_origin git@github.com:kuangyufei/kernel_liteos_a_note.git 
#git remote add gitee_origin git@gitee.com:weharmony/kernel_liteos_a_note.git
#git remote add origin git@codechina.csdn.net:kuangyufei/kernel_liteos_a_note.git
#git remote add coding_origin git@e.coding.net:weharmony/harmony/kernel_liteos_a_note.git