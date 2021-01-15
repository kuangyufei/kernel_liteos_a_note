#!/usr/bin/env sh

# 确保脚本抛出遇到的错误
set -e

# 生成静态文件
yarn build

# 进入生成的文件夹
cd docs/.vuepress/dist

# 如果是发布到自定义域名
# echo 'www.example.com' > CNAME

git init
git add -A
git commit -m 'deploy'

# 如果发布到 https://<USERNAME>.github.io
git push -f git@github.com:weharmony/weharmony.github.io.git master #https://weharmony.github.io
git push -f git@gitee.com:weharmony/www.weharmonyos.com.git master #https://harmonyos.21yunbox.com/,https://www.weharmonyos.com

# 如果发布到 https://<USERNAME>.github.io/<REPO>
# git push -f git@github.com:kuangyufei/kernel_liteos_a_note.git master:gh-pages

# 进入markdown的文件夹,推送三大平台wiki
cd ../../guide
git init
git add -A
git commit -m 'deploy wiki'
git push -f git@gitee.com:weharmony/kernel_liteos_a_note.wiki.git master
git push -f git@github.com:kuangyufei/kernel_liteos_a_note.wiki.git  master
git push -f git@codechina.csdn.net:kuangyufei/kernel_liteos_a_note.wiki.git master
#git push -f git@e.coding.net:weharmony/harmony/kernel_liteos_a_note.wiki.git master


cd -