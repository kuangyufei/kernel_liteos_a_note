git add -A
git commit -m  '2021/5/28 同步官方源码 -- 本次官方改动不大,主要针对一些错误单词拼写纠正
    百万汉字注解 + 百篇博客分析 => 挖透鸿蒙内核源码
    国内:https://weharmony.gitee.io
    国外:https://weharmony.github.io
'

git push  origin master
git push  gitee_origin master
git push  github_origin master
git push  coding_origin master

#git remote add github_origin git@github.com:kuangyufei/kernel_liteos_a_note.git 
#git remote add gitee_origin git@gitee.com:weharmony/kernel_liteos_a_note.git
#git remote add origin git@codechina.csdn.net:kuangyufei/kernel_liteos_a_note.git
#git remote add coding_origin git@e.coding.net:weharmony/harmony/kernel_liteos_a_note.git
