# Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
# Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this list of
# conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this list
# of conditions and the following disclaimer in the documentation and/or other materials
# provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors may be used
# to endorse or promote products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

TESTLIB_SUBDIRS += kernel
LITEOS_BASELIB += -lktest

ifeq ($(LOSCFG_TESTSUIT_SHELL), y)
LITEOS_CMACRO  += -DLITEOS_TESTSUIT_SHELL
else ifeq ($(LOSCFG_TEST), y)
LITEOS_CMACRO  += -DLITEOS_TEST_AUTO
else ifeq ($(LOSCFG_TEST_MANUAL_TEST),y)
LITEOS_CMACRO  += -DLOSCFG_TEST_MANUAL_TEST
endif


SRC_MODULES :=
LLT_MODULES :=
SMOKE_MODULES :=
PRESSURE_MODULES :=
FULL_MODULES :=
ifeq ($(LOSCFG_TEST_MUTIL), y)
LITEOS_CMACRO  += -DLOSCFG_TEST_MUTIL
LOSCFG_TEST_MUTIL := y
endif

ifeq ($(LOSCFG_TEST_KERNEL_BASE), y)
LITEOS_CMACRO  += -DLOSCFG_TEST_KERNEL_BASE
endif

ifeq ($(LOSCFG_TEST_KERNEL_BASE_IPC), y)
TESTLIB_SUBDIRS +=  kernel/sample/kernel_base/ipc
LITEOS_BASELIB += -lipctest
LITEOS_CMACRO += -DLOSCFG_TEST_KERNEL_BASE_IPC
endif

ifeq ($(LOSCFG_TEST_KERNEL_BASE_CORE), y)
TESTLIB_SUBDIRS +=  kernel/sample/kernel_base/core
LITEOS_BASELIB += -lcoretest
LITEOS_CMACRO += -DLOSCFG_TEST_KERNEL_BASE_CORE
endif
ifeq ($(LOSCFG_TEST_KERNEL_BASE_MP), y)
TESTLIB_SUBDIRS +=  kernel/sample/kernel_base/mp
LITEOS_BASELIB += -lmptest
LITEOS_CMACRO += -DLOSCFG_TEST_KERNEL_BASE_MP
endif
ifeq ($(LOSCFG_TEST_KERNEL_BASE_MEM), y)
TESTLIB_SUBDIRS +=  kernel/sample/kernel_base/mem
LITEOS_BASELIB += -lmemtest
LITEOS_CMACRO += -DLOSCFG_TEST_KERNEL_BASE_MEM
endif
ifeq ($(LOSCFG_TEST_KERNEL_BASE_VM), y)
TESTLIB_SUBDIRS +=  kernel/sample/kernel_base/vm
LITEOS_BASELIB += -lvmtest
LITEOS_CMACRO += -DLOSCFG_TEST_KERNEL_BASE_VM
endif
ifeq ($(LOSCFG_TEST_KERNEL_BASE_MISC), y)
TESTLIB_SUBDIRS +=  kernel/sample/kernel_base/misc
LITEOS_BASELIB += -lmisctest
LITEOS_CMACRO += -DLOSCFG_TEST_KERNEL_BASE_MISC
endif
ifeq ($(LOSCFG_TEST_KERNEL_BASE_OM), y)
TESTLIB_SUBDIRS +=  kernel/sample/kernel_base/om
LITEOS_BASELIB += -lomtest
LITEOS_CMACRO += -DLOSCFG_TEST_KERNEL_BASE_OM
endif
ifeq ($(LOSCFG_TEST_KERNEL_BASE_ATOMIC), y)
TESTLIB_SUBDIRS +=  kernel/sample/kernel_base/atomic
LITEOS_BASELIB += -latomictest
LITEOS_CMACRO += -DLOSCFG_TEST_KERNEL_BASE_ATOMIC
endif
ifeq ($(LOSCFG_TEST_KERNEL_EXTEND), y)
LITEOS_CMACRO += -DLOSCFG_TEST_KERNEL_EXTEND
endif
ifeq ($(LOSCFG_TEST_KERNEL_EXTEND_CPP), y)
TESTLIB_SUBDIRS +=  kernel/sample/kernel_extend/cpp
LITEOS_BASELIB += -lcpptest
LITEOS_CMACRO += -DLOSCFG_TEST_KERNEL_EXTEND_CPP
endif
ifeq ($(LOSCFG_TEST_KERNEL_EXTEND_CPUP), y)
TESTLIB_SUBDIRS +=  kernel/sample/kernel_extend/cpup
LITEOS_BASELIB += -lcpuptest
LITEOS_CMACRO += -DLOSCFG_TEST_KERNEL_EXTEND_CPUP
endif
ifeq ($(LOSCFG_TEST_KERNEL_EXTEND_EXC), y)
TESTLIB_SUBDIRS +=  kernel/sample/kernel_extend/exc
LITEOS_BASELIB += -lexctest
LITEOS_CMACRO += -DLOSCFG_TEST_KERNEL_EXTEND_EXC
endif
ifeq ($(LOSCFG_TEST_KERNEL_EXTEND_UNALIGNACCESS), y)
TESTLIB_SUBDIRS +=  kernel/sample/kernel_extend/unalignaccess
LITEOS_BASELIB += -lunalignaccesstest
LITEOS_CMACRO += -DLOSCFG_TEST_KERNEL_EXTEND_UNALIGNACCESS
endif
ifeq ($(LOSCFG_TEST_KERNEL_EXTEND_MMU), y)
TESTLIB_SUBDIRS +=  kernel/sample/kernel_extend/mmu
LITEOS_BASELIB += -lmmutest
LITEOS_CMACRO += -DLOSCFG_TEST_KERNEL_EXTEND_MMU
endif
ifeq ($(LOSCFG_TEST_KERNEL_EXTEND_DYNLOAD), y)
TESTLIB_SUBDIRS +=  kernel/sample/kernel_extend/dynload
LITEOS_BASELIB += -ldynloadtest
LITEOS_CMACRO += -DLOSCFG_TEST_KERNEL_EXTEND_DYNLOAD
endif
ifeq ($(LOSCFG_TEST_KERNEL_EXTEND_MPU), y)
TESTLIB_SUBDIRS +=  kernel/sample/kernel_extend/mpu
LITEOS_BASELIB += -lmputest
LITEOS_CMACRO += -DLOSCFG_TEST_KERNEL_EXTEND_MPU
endif
ifeq ($(LOSCFG_TEST_KERNEL_EXTEND_RUNSTOP), y)
TESTLIB_SUBDIRS +=  kernel/sample/kernel_extend/runstop
LITEOS_BASELIB += -lrunstoptest
LITEOS_CMACRO += -DLOSCFG_TEST_KERNEL_EXTEND_RUNSTOP
endif
ifeq ($(LOSCFG_TEST_KERNEL_EXTEND_SCATTER), y)
TESTLIB_SUBDIRS +=  kernel/sample/kernel_extend/scatter
LITEOS_BASELIB += -lscattertest
LITEOS_CMACRO += -DLOSCFG_TEST_KERNEL_EXTEND_SCATTER
endif
ifeq ($(LOSCFG_TEST_KERNEL_EXTEND_TICKLESS), y)
TESTLIB_SUBDIRS +=  kernel/sample/kernel_extend/tickless
LITEOS_BASELIB += -lticklesstest
LITEOS_CMACRO += -DLOSCFG_TEST_KERNEL_EXTEND_TICKLESS
endif
ifeq ($(LOSCFG_TEST_KERNEL_EXTEND_TRACE), y)
TESTLIB_SUBDIRS +=  kernel/sample/kernel_extend/trace
LITEOS_BASELIB += -ltracetest
LITEOS_CMACRO += -DLOSCFG_TEST_KERNEL_EXTEND_TRACE
endif

ifeq ($(LOSCFG_TEST_POSIX), y)
TESTLIB_SUBDIRS +=  kernel/sample/posix
LITEOS_BASELIB += -lposixtest
LITEOS_CMACRO += -DLOSCFG_TEST_POSIX
endif
ifeq ($(LOSCFG_TEST_POSIX_MEM), y)
TESTLIB_SUBDIRS +=  kernel/sample/posix/mem
LITEOS_BASELIB += -lmemtest
LITEOS_CMACRO += -DLOSCFG_TEST_POSIX_MEM
endif
ifeq ($(LOSCFG_TEST_POSIX_MQUEUE), y)
TESTLIB_SUBDIRS +=  kernel/sample/posix/mqueue
LITEOS_BASELIB += -lmqueuetest
LITEOS_CMACRO += -DLOSCFG_TEST_POSIX_MQUEUE
endif
ifeq ($(LOSCFG_TEST_POSIX_MUTEX), y)
TESTLIB_SUBDIRS +=  kernel/sample/posix/mutex
LITEOS_BASELIB += -lmutextest
LITEOS_CMACRO += -DLOSCFG_TEST_POSIX_MUTEX
endif
ifeq ($(LOSCFG_TEST_POSIX_PTHREAD), y)
TESTLIB_SUBDIRS +=  kernel/sample/posix/pthread
LITEOS_BASELIB += -lpthreadtest
LITEOS_CMACRO += -DLOSCFG_TEST_POSIX_PTHREAD
endif
ifeq ($(LOSCFG_TEST_POSIX_SCHED), y)
TESTLIB_SUBDIRS +=  kernel/sample/posix/sched
LITEOS_BASELIB += -lschedtest
LITEOS_CMACRO += -DLOSCFG_TEST_POSIX_SCHED
endif
ifeq ($(LOSCFG_TEST_POSIX_SEM), y)
TESTLIB_SUBDIRS +=  kernel/sample/posix/sem
LITEOS_BASELIB += -lsemtest
LITEOS_CMACRO += -DLOSCFG_TEST_POSIX_SEM
endif
ifeq ($(LOSCFG_TEST_POSIX_SWTMR), y)
TESTLIB_SUBDIRS +=  kernel/sample/posix/swtmr
LITEOS_BASELIB += -lswtmrtest
LITEOS_CMACRO += -DLOSCFG_TEST_POSIX_SWTMR
endif
ifeq ($(LOSCFG_TEST_LINUX), y)
TESTLIB_SUBDIRS +=  kernel/sample/linux
LITEOS_BASELIB += -llinuxtest
LITEOS_CMACRO += -DLOSCFG_TEST_LINUX
endif
ifeq ($(LOSCFG_TEST_LINUX_HRTIMER), y)
TESTLIB_SUBDIRS +=  kernel/sample/linux/hrtimer
LITEOS_BASELIB += -lhrtimertest
LITEOS_CMACRO += -DLOSCFG_TEST_LINUX
endif

ifeq ($(LOSCFG_TEST_FS), y)
LITEOS_CMACRO += -DLOSCFG_TEST_FS
endif

ifeq ($(LOSCFG_TEST_FS_VFS), y)
TESTLIB_SUBDIRS +=  kernel/sample/fs/vfs
LITEOS_BASELIB += -lvfstest
LITEOS_CMACRO += -DLOSCFG_TEST_FS_VFS
endif

ifeq ($(LOSCFG_TEST_FS_JFFS), y)
TESTLIB_SUBDIRS +=  kernel/sample/fs/jffs
LITEOS_BASELIB += -ljffstest
LITEOS_CMACRO += -DLOSCFG_TEST_FS_JFFS
endif

ifeq ($(LOSCFG_TEST_FS_FAT), y)
TESTLIB_SUBDIRS +=  kernel/sample/fs/vfat
LITEOS_BASELIB += -lvfattest
LITEOS_CMACRO += -DLOSCFG_TEST_FS_FAT
endif

ifeq ($(LOSCFG_TEST_FS_FAT_FAT32), y)
LITEOS_CMACRO += -DLOSCFG_TEST_FS_FAT_FAT32
endif

ifeq ($(LOSCFG_TEST_FAT32_FSCK), y)
LITEOS_CMACRO += -DLOSCFG_TEST_FAT32_FSCK
endif

ifeq ($(LOSCFG_TEST_FS_VIRPART), y)
TESTLIB_SUBDIRS +=  kernel/sample/fs/virpart
LITEOS_BASELIB += -lvirparttest
LITEOS_CMACRO += -DLOSCFG_TEST_FS_VIRPART
endif

ifeq ($(LOSCFG_TEST_FS_NFS), y)
TESTLIB_SUBDIRS +=  kernel/sample/fs/nfs
LITEOS_BASELIB += -lnfstest
LITEOS_CMACRO += -DLOSCFG_TEST_FS_NFS
endif

ifeq ($(LOSCFG_TEST_FS_PROC), y)
TESTLIB_SUBDIRS +=  kernel/sample/fs/proc
LITEOS_BASELIB += -lproctest
LITEOS_CMACRO += -DLOSCFG_TEST_FS_PROC
endif

ifeq ($(LOSCFG_TEST_FS_RAMFS), y)
TESTLIB_SUBDIRS +=  kernel/sample/fs/ramfs
LITEOS_BASELIB += -lramfstest
LITEOS_CMACRO += -DLOSCFG_TEST_FS_RAMFS
endif

ifeq ($(LOSCFG_TEST_MTD_JFFS), y)
TESTLIB_SUBDIRS +=  kernel/sample/mtd/spinor/
LITEOS_BASELIB += -lspinortest
LITEOS_CMACRO += -DLOSCFG_TEST_MTD_JFFS
endif

ifeq ($(LOSCFG_TEST_MTD_FAT), y)
TESTLIB_SUBDIRS +=  kernel/sample/mtd/fat
LITEOS_BASELIB += -lfattest
LITEOS_CMACRO += -DLOSCFG_TEST_MTD_FAT
endif

ifeq ($(LOSCFG_TEST_MTD_DISK), y)
TESTLIB_SUBDIRS +=  kernel/sample/mtd/disk
LITEOS_BASELIB += -ldisktest
LITEOS_CMACRO += -DLOSCFG_TEST_MTD_DISK
endif

ifeq ($(LOSCFG_TEST_MTD_FAT_VIRPART), y)
TESTLIB_SUBDIRS +=  kernel/sample/mtd/dvirpart
LITEOS_BASELIB += -ldvirparttest
LITEOS_CMACRO += -DLOSCFG_TEST_MTD_FAT_VIRPART
endif

ifeq ($(LOSCFG_TEST_DRIVERBASE), y)
TESTLIB_SUBDIRS += kernel/sample/drivers/base
TESTLIB_SUBDIRS += kernel/sample/drivers/regulator
TESTLIB_SUBDIRS += kernel/sample/drivers/cpufreq
TESTLIB_SUBDIRS += kernel/sample/drivers/devfreq
LITEOS_BASELIB += -lbasetest -lregulatortest -lcpufreqtest -ldevfreqtest
LITEOS_CMACRO += -DLOSCFG_TEST_DRIVER_BASE
endif

ifeq ($(LOSCFG_TEST_LIBC), y)
TESTLIB_SUBDIRS +=  kernel/sample/libc
LITEOS_BASELIB += -llibctest
LITEOS_CMACRO += -DLOSCFG_TEST_LIBC
endif

ifeq ($(LOSCFG_TEST_LIBM), y)
TESTLIB_SUBDIRS +=  kernel/sample/libm
LITEOS_BASELIB += -llibmtest
LITEOS_CMACRO += -DLOSCFG_TEST_LIBM
endif

ifeq ($(LOSCFG_TEST_SHELL), y)
TESTLIB_SUBDIRS +=  kernel/sample/shell
LITEOS_BASELIB += -lshelltest
LITEOS_CMACRO += -DLOSCFG_TEST_SHELL
endif

ifeq ($(LOSCFG_TEST_HOST_MASS_DEVICE), y)
TESTLIB_SUBDIRS +=  kernel/sample/performance/usb
LITEOS_BASELIB += -lusbtest
LITEOS_CMACRO += -DLOSCFG_TEST_HOST_MASS_DEVICE
endif

ifeq ($(LOSCFG_TEST_MMC), y)
TESTLIB_SUBDIRS +=  kernel/sample/drivers/mmc
LITEOS_BASELIB += -lmmctest
endif

ifeq ($(LOSCFG_TEST_AUTO_USB), y)
TESTLIB_SUBDIRS +=  kernel/sample/drivers/usb/auto
LITEOS_BASELIB += -lautotest
endif

ifeq ($(LOSCFG_TEST_DEVICE_MASS_GADGET), y)
TESTLIB_SUBDIRS += kernel/sample/drivers/usb/storage
LITEOS_BASELIB += -lstoragetest
LITEOS_CMACRO += -DLOSCFG_TEST_DEVICE_MASS_GADGET
endif

ifeq ($(LOSCFG_TEST_UVC_GADGET), y)
TESTLIB_SUBDIRS += kernel/sample/drivers/usb/uvc
LITEOS_BASELIB += -luvctest
LITEOS_CMACRO += -DLOSCFG_TEST_UVC_GADGET
endif

ifeq ($(LOSCFG_TEST_SMP_USB), y)
TESTLIB_SUBDIRS += kernel/sample/drivers/usb/usbsmp
LITEOS_BASELIB += -lusbsmptest
LITEOS_CMACRO += -DLOSCFG_TEST_SMP_USB
endif

ifeq ($(LOSCFG_TEST_UAC_GADGET), y)
TESTLIB_SUBDIRS += kernel/sample/drivers/usb/uac
LITEOS_BASELIB += -luactest
LITEOS_CMACRO += -DLOSCFG_TEST_UAC_GADGET
endif

ifeq ($(LOSCFG_TEST_CAMERA_GADGET), y)
TESTLIB_SUBDIRS += kernel/sample/drivers/usb/camera
LITEOS_BASELIB += -lcameratest
LITEOS_CMACRO += -DLOSCFG_TEST_CAMERA_GADGET
endif

ifeq ($(LOSCFG_TEST_HID_GADGET), y)
TESTLIB_SUBDIRS += kernel/sample/drivers/usb/hid
LITEOS_BASELIB += -lhidtest
LITEOS_CMACRO += -DLOSCFG_TEST_HID_GADGET
endif

ifeq ($(LOSCFG_TEST_HUB_GADGET), y)
TESTLIB_SUBDIRS += kernel/sample/drivers/usb/hub
LITEOS_BASELIB += -lhubtest
LITEOS_CMACRO += -DLOSCFG_TEST_HUB_GADGET
endif

ifeq ($(LOSCFG_TEST_SERIAL_GADGET), y)
TESTLIB_SUBDIRS += kernel/sample/drivers/usb/serial
LITEOS_BASELIB += -lserialtest
LITEOS_CMACRO += -DLOSCFG_TEST_SERIAL_GADGET
endif

ifeq ($(LOSCFG_TEST_DFU_GADGET), y)
TESTLIB_SUBDIRS += kernel/sample/drivers/usb/dfu
LITEOS_BASELIB += -ldfutest
LITEOS_CMACRO += -DLOSCFG_TEST_DFU_GADGET
endif

ifeq ($(LOSCFG_TEST_MUTILDEVICE_GADGET), y)
TESTLIB_SUBDIRS += kernel/sample/drivers/usb/multidevice
LITEOS_BASELIB += -lmultidevicetest
LITEOS_CMACRO += -DLOSCFG_TEST_MUTILDEVICE_GADGET
endif

ifeq ($(LOSCFG_DRIVERS_USB_ETHERNET_GADGET), y)
TESTLIB_SUBDIRS += kernel/sample/drivers/usb/rndis
LITEOS_BASELIB += -lrndistest
LITEOS_CMACRO += -DLOSCFG_DRIVERS_USB_ETHERNET_GADGET
endif

ifeq ($(LOSCFG_DRIVERS_USB_HOST_UVC), y)
TESTLIB_SUBDIRS += kernel/sample/drivers/usb/video
LITEOS_BASELIB += -lvideotest
LITEOS_CMACRO += -DLOSCFG_DRIVERS_USB_HOST_UVC
endif

ifeq ($(LOSCFG_TEST_PERFORMANCE), y)
TESTLIB_SUBDIRS +=  kernel/sample/performance/kernel
LITEOS_BASELIB += -lperformancetest
LITEOS_CMACRO += -DLOSCFG_TEST_PERFORMANCE
ifeq ($(LOSCFG_TEST_PERFORMANCE_CORE), y)
LITEOS_CMACRO += -DLOSCFG_TEST_PERFORMANCE_CORE
endif
ifeq ($(LOSCFG_TEST_PERFORMANCE_MEM), y)
LITEOS_CMACRO += -DLOSCFG_TEST_PERFORMANCE_MEM
endif
ifeq ($(LOSCFG_TEST_PERFORMANCE_FS), y)
LITEOS_CMACRO += -DLOSCFG_TEST_PERFORMANCE_FS
endif
ifeq ($(LOSCFG_TEST_PERFORMANCE_USB), y)
LITEOS_CMACRO += -DLOSCFG_TEST_PERFORMANCE_USB
endif
ifeq ($(LOSCFG_TEST_PERFORMANCE_NET), y)
LITEOS_CMACRO += -DLOSCFG_TEST_PERFORMANCE_NET
endif
endif

ifeq ($(LOSCFG_TEST_NET), y)
    LITEOS_CMACRO += -DTEST_NET
    ifeq ($(LOSCFG_PLATFORM_HI3559)$(LOSCFG_ARCH_CORTEX_A17), yy)
        LITEOS_BASELIB += -lipcm -lipcm_net
    endif
    LITEOS_BASELIB += -lnettest
    TESTLIB_SUBDIRS += kernel/sample/net
endif

ifeq ($(LOSCFG_TEST_LWIP), y)
    LITEOS_CMACRO += -DTEST_LWIP
    LITEOS_BASELIB += -llwiptest
ifeq ($(LOSCFG_NET_LWIP_SACK_2_0), y)
    TESTLIB_SUBDIRS    += kernel/sample/lwip-2.0
else
    TESTLIB_SUBDIRS    += kernel/sample/lwip
endif
endif

ifeq ($(LOSCFG_TEST_PLATFORM), y)
TESTLIB_SUBDIRS +=  kernel/sample/platform
LITEOS_BASELIB += -lplatformtest
LITEOS_CMACRO  += -DLOSCFG_TEST_PLATFORM
endif

ifeq ($(LOSCFG_FUZZ_DT), y)
TESTLIB_SUBDIRS +=  kernel/sample/fuzz
LITEOS_BASELIB += -lfuzzDTtest
LITEOS_CMACRO += -DLOSCFG_FUZZ_DT
endif

ifeq ($(LOSCFG_TEST_MANUAL_TEST), y)
TESTLIB_SUBDIRS +=  kernel/sample/kernel_base/ipc
LITEOS_BASELIB += -lipctest
LITEOS_CMACRO += -DLOSCFG_TEST_MANUAL_TEST
endif

ifeq ($(LOSCFG_3RDPARTY_TEST), y)
    LITEOS_CMACRO += -DLOSCFG_3RDPARTY_TEST
    ifeq ($(LOSCFG_3RDPARTY_TINYXML), y)
        LITEOS_BASELIB += -ltinyxmltest
        LITEOS_CMACRO += -DLOSCFG_3RDPARTY_TINYXML_TEST
        TESTLIB_SUBDIRS += kernel/sample/3rdParty/tinyxml
    endif

    ifeq ($(LOSCFG_3RDPARTY_INIPARSER), y)
        LITEOS_BASELIB += -liniparsertest
        LITEOS_CMACRO += -DLOSCFG_3RDPARTY_INIPARSER_TEST
        TESTLIB_SUBDIRS += kernel/sample/3rdParty/iniparser
    endif

    ifeq ($(LOSCFG_3RDPARTY_CJSON), y)
        LITEOS_BASELIB += -lcJSONtest
        LITEOS_CMACRO += -DLOSCFG_3RDPARTY_CJSON_TEST
        TESTLIB_SUBDIRS += kernel/sample/3rdParty/cJSON
    endif

    ifeq ($(LOSCFG_3RDPARTY_ICONV), y)
        LITEOS_BASELIB += -liconvtest
        LITEOS_CMACRO += -DLITEOS_3RDPARTY_ICONV_TEST
        TESTLIB_SUBDIRS += kernel/sample/3rdParty/iconv
    endif

    ifeq ($(LOSCFG_3RDPARTY_OPENSSL), y)
        LITEOS_BASELIB += -lopenssltest
        LITEOS_CMACRO += -DLITEOS_3RDPARTY_OPENSSL_TEST
        TESTLIB_SUBDIRS += kernel/sample/3rdParty/openssl
    endif

    ifeq ($(LOSCFG_3RDPARTY_OPUS), y)
        LITEOS_BASELIB += -lopustest
        LITEOS_CMACRO += -DLITEOS_3RDPARTY_OPUS_TEST
        TESTLIB_SUBDIRS += kernel/sample/3rdParty/opus
    endif

    ifeq ($(LOSCFG_3RDPARTY_BIDIREFC), y)
        LITEOS_BASELIB += -lbidirefctest
        LITEOS_CMACRO += -DLITEOS_3RDPARTY_BIDIREFC_TEST
        TESTLIB_SUBDIRS += kernel/sample/3rdParty/bidirefc
    endif

    ifeq ($(LOSCFG_3RDPARTY_FREETYPE), y)
        LITEOS_BASELIB += -lfreetypetest
        LITEOS_CMACRO += -DLITEOS_3RDPARTY_FREETYPE_TEST
        TESTLIB_SUBDIRS += kernel/sample/3rdParty/freetype
    endif

    ifeq ($(LOSCFG_3RDPARTY_JSONCPP), y)
        LITEOS_BASELIB += -ljsoncpptest
        LITEOS_CMACRO += -DLITEOS_3RDPARTY_JSONCPP_TEST
        TESTLIB_SUBDIRS += kernel/sample/3rdParty/jsoncpp
    endif

    ifeq ($(LOSCFG_3RDPARTY_THTTPD), y)
        LITEOS_BASELIB += -lthttpdtest
        LITEOS_CMACRO += -DLITEOS_3RDPARTY_THTTPD_TEST
        TESTLIB_SUBDIRS += kernel/sample/3rdParty/thttpd
    endif

    ifeq ($(LOSCFG_3RDPARTY_SQLITE), y)
        LITEOS_BASELIB += -lsqlitetest
        LITEOS_CMACRO += -DLITEOS_3RDPARTY_SQLITE_TEST
        TESTLIB_SUBDIRS += kernel/sample/3rdParty/sqlite
    endif

    ifeq ($(LOSCFG_3RDPARTY_FFMPEG), y)
        LITEOS_BASELIB += -lffmpegtest
        LITEOS_CMACRO += -DLITEOS_3RDPARTY_FFMPEG_TEST
        TESTLIB_SUBDIRS += kernel/sample/3rdParty/ffmpeg
    endif

    ifeq ($(LOSCFG_3RDPARTY_LUA), y)
        LITEOS_BASELIB += -lluatest
        LITEOS_CMACRO += -DLITEOS_3RDPARTY_LUA_TEST
        TESTLIB_SUBDIRS += kernel/sample/3rdParty/lua
    endif

    ifeq ($(LOSCFG_3RDPARTY_DIRECTFB), y)
        LITEOS_BASELIB += -ldirectfbtest
        LITEOS_CMACRO += -DLITEOS_3RDPARTY_DIRECTFB_TEST
        TESTLIB_SUBDIRS += kernel/sample/3rdParty/directfb
    endif

    ifeq ($(LOSCFG_3RDPARTY_JPEG), y)
        LITEOS_BASELIB += -ljpegtest
        LITEOS_CMACRO += -DLOSCFG_3RDPARTY_JPEG_TEST
        TESTLIB_SUBDIRS += kernel/sample/3rdParty/jpeg
    endif

    ifeq ($(LOSCFG_3RDPARTY_PNG), y)
        LITEOS_BASELIB += -lpngtest
        LITEOS_CMACRO += -DLOSCFG_3RDPARTY_PNG_TEST
        TESTLIB_SUBDIRS += kernel/sample/3rdParty/png
    endif

    ifeq ($(LOSCFG_3RDPARTY_OPENEXIFJPEG), y)
        LITEOS_BASELIB += -lOpenExifJpegtest
        LITEOS_CMACRO += -DLOSCFG_3RDPARTY_OPENEXIFJPEG_TEST
        TESTLIB_SUBDIRS += kernel/sample/3rdParty/OpenExifJpeg
    endif

    ifeq ($(LOSCFG_3RDPARTY_XML2), y)
        LITEOS_BASELIB += -lxml2test
        LITEOS_CMACRO += -DLITEOS_3RDPARTY_XML2_TEST
        TESTLIB_SUBDIRS += kernel/sample/3rdParty/xml2
    endif

    ifeq ($(LOSCFG_3RDPARTY_ZBAR), y)
        LITEOS_BASELIB += -lzbartest
        LITEOS_CMACRO += -DLITEOS_3RDPARTY_ZBAR_TEST
        TESTLIB_SUBDIRS += kernel/sample/3rdParty/zbar
    endif

    ifeq ($(LOSCFG_3RDPARTY_HARFBUZZ), y)
        LITEOS_BASELIB += -lharfbuzztest
        LITEOS_CMACRO += -DLOSCFG_3RDPARTY_HARFBUZZ_TEST
        TESTLIB_SUBDIRS += kernel/sample/3rdParty/harfbuzz
    endif

    ifeq ($(LOSCFG_3RDPARTY_CURL), y)
        LITEOS_LD_OPTS += -ucurl_shellcmd
        LITEOS_BASELIB += -lcurltest
        LITEOS_CMACRO += -DLITEOS_3RDPARTY_CURL_TEST
        TESTLIB_SUBDIRS += kernel/sample/3rdParty/curl
    endif
endif

