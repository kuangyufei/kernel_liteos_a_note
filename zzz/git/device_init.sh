### 精度内核源码,需其他项目源码配合,拉取 device 目录下代码,目录关系如下
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
mkdir device
# hisilicon
git clone git@gitee.com:openharmony/device_hisilicon_build.git device/hisilicon/build
git clone git@gitee.com:openharmony/device_hisilicon_drivers.git device/hisilicon/drivers
git clone git@gitee.com:openharmony/device_hisilicon_hardware.git device/hisilicon/hardware
git clone git@gitee.com:openharmony/device_hisilicon_hispark_pegasus.git device/hisilicon/hispark_pegasus
git clone git@gitee.com:openharmony/device_hisilicon_hispark_taurus.git device/hisilicon/hispark_taurus
git clone git@gitee.com:openharmony/device_hisilicon_hispark_aries.git device/hisilicon/hispark_aries
git clone git@gitee.com:openharmony/security_itrustee_ree_lite.git device/hisilicon/itrustee/itrustee_ree_lite
git clone git@gitee.com:openharmony/device_hisilicon_modules.git device/hisilicon/modules
git clone git@gitee.com:openharmony/device_hisilicon_third_party_ffmpeg.git device/hisilicon/third_party/ffmpeg
git clone git@gitee.com:openharmony/device_hisilicon_third_party_uboot.git device/hisilicon/third_party/uboot

# qemu
git clone git@gitee.com:weharmony/device_qemu.git device/qemu

cd -
