git add -A
git commit -m  '开始注解与内核相关联的两个项目(lwip协议栈和NuttX文件系统). 
    百万汉字注解 + 百篇博客分析 => 挖透鸿蒙内核源码
    博客输出站点(国内):https://weharmonyos.com 或 http://8.134.122.205
    博客输出站点(国外):https://weharmony.github.io
    注解文件系统:https://gitee.com/weharmony/third_party_NuttX
    注解协议栈:https://gitee.com/weharmony/third_party_lwip
'

git push  origin master
git push  gitee_origin master
git push  github_origin master
git push  coding_origin master

#git remote add github_origin git@github.com:kuangyufei/kernel_liteos_a_note.git 
#git remote add gitee_origin git@gitee.com:weharmony/kernel_liteos_a_note.git
#git remote add origin git@codechina.csdn.net:kuangyufei/kernel_liteos_a_note.git
#git remote add coding_origin git@e.coding.net:weharmony/harmony/kernel_liteos_a_note.git
