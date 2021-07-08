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
mkdir third_party
git clone git@gitee.com:weharmony/third_party_FatFs.git third_party/FatFs
git clone git@gitee.com:weharmony/third_party_FreeBSD.git third_party/FreeBSD
git clone git@gitee.com:weharmony/third_party_Linux_Kernel.git third_party/Linux_Kernel
git clone git@gitee.com:weharmony/third_party_lwip.git third_party/lwip
git clone git@gitee.com:weharmony/third_party_mtd_utils.git third_party/mtd-utils
git clone git@gitee.com:weharmony/third_party_musl.git third_party/musl
git clone git@gitee.com:weharmony/third_party_NuttX.git third_party/NuttX
git clone git@gitee.com:weharmony/third_party_zlib.git third_party/zlib
cd -
