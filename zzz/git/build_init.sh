### 精度内核源码,需其他项目源码配合,拉取内核依赖的第三方代码,目录关系如下
#   - build 
#       | - lite
#   - device 
#       | - hisilicon
#       | - qemu
#   - kernel
#       | - liteos_a
#   - third_party
#       | - lwip
#       | - Nuttx
#       | - ...
### 
cd ../../../../
mkdir build
git clone git@gitee.com:weharmony/build_lite.git build/lite

cd -
