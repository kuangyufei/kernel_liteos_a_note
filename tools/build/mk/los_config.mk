# Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
# Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this list of
#    conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this list
#    of conditions and the following disclaimer in the documentation and/or other materials
#    provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors may be used
#    to endorse or promote products derived from this software without specific prior written
#    permission.
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

-include $(LITEOS_CONFIG_FILE)

ifeq ($(ARCH),)
ARCH = $(error ARCH not set!)
endif

## variable define ##
HIDE = @
RM = -rm -rf
OUT = $(or $(OUTDIR),$(LITEOSTOPDIR)/out/$(LOSCFG_PRODUCT_NAME:"%"=%))
MODULE = $(LITEOSTOPDIR)/tools/build/mk/module.mk
LITEOS_SCRIPTPATH = $(LITEOSTOPDIR)/tools/scripts
LITEOSTHIRDPARTY = $(LITEOSTOPDIR)/../../third_party

## compiler relative ##
get_compiler_path = $(or $(wildcard $(1)),$(dir $(shell which $(CROSS_COMPILE)as)))
ifeq ($(LOSCFG_COMPILER_CLANG_LLVM), y)
CROSS_COMPILE ?= llvm-
LITEOS_COMPILER_PATH ?= $(call get_compiler_path,$(LITEOSTOPDIR)/../../prebuilts/clang/ohos/linux-x86_64/llvm/bin/)
LLVM_TARGET = $(if $(LOSCFG_LLVM_TARGET),-target $(LOSCFG_LLVM_TARGET),)
LLVM_SYSROOT = $(if $(SYSROOT_PATH),--sysroot=$(SYSROOT_PATH),)
CC  = $(LITEOS_COMPILER_PATH)clang $(LLVM_TARGET) $(LLVM_SYSROOT)
AS  = $(LITEOS_COMPILER_PATH)$(CROSS_COMPILE)as
AR  = $(LITEOS_COMPILER_PATH)$(CROSS_COMPILE)ar
LD  = $(LITEOS_COMPILER_PATH)ld.lld
GPP = $(LITEOS_COMPILER_PATH)clang++ $(LLVM_TARGET) $(LLVM_SYSROOT)
OBJCOPY = $(LITEOS_COMPILER_PATH)$(CROSS_COMPILE)objcopy -R .bss
OBJDUMP = $(LITEOS_COMPILER_PATH)$(CROSS_COMPILE)objdump
SIZE = $(LITEOS_COMPILER_PATH)$(CROSS_COMPILE)size
NM = $(LITEOS_COMPILER_PATH)$(CROSS_COMPILE)nm
STRIP = $(LITEOS_COMPILER_PATH)$(CROSS_COMPILE)strip
else ifeq ($(LOSCFG_COMPILER_GCC), y)
CROSS_COMPILE ?= $(LOSCFG_CROSS_COMPILE)
LITEOS_COMPILER_PATH ?= $(call get_compiler_path,$(LITEOSTOPDIR)/../../prebuilts/gcc/linux-x86/arm/arm-linux-ohoseabi-gcc/bin/)
CC  = $(LITEOS_COMPILER_PATH)$(CROSS_COMPILE)gcc
AS  = $(LITEOS_COMPILER_PATH)$(CROSS_COMPILE)as
AR  = $(LITEOS_COMPILER_PATH)$(CROSS_COMPILE)ar
LD  = $(LITEOS_COMPILER_PATH)$(CROSS_COMPILE)ld
GPP = $(LITEOS_COMPILER_PATH)$(CROSS_COMPILE)g++
OBJCOPY = $(LITEOS_COMPILER_PATH)$(CROSS_COMPILE)objcopy
OBJDUMP = $(LITEOS_COMPILER_PATH)$(CROSS_COMPILE)objdump
SIZE = $(LITEOS_COMPILER_PATH)$(CROSS_COMPILE)size
NM = $(LITEOS_COMPILER_PATH)$(CROSS_COMPILE)nm
STRIP = $(LITEOS_COMPILER_PATH)$(CROSS_COMPILE)strip
else
CC  = echo $(info compiler type not set!)
endif

## c as cxx ld options ##
LITEOS_ASOPTS :=
LITEOS_COPTS_BASE :=
LITEOS_COPTS_EXTRA :=
LITEOS_COPTS_DEBUG :=
LITEOS_CXXOPTS :=
LITEOS_CXXOPTS_BASE :=
LITEOS_LD_OPTS :=
LITEOS_GCOV_OPTS :=
## macro define ##
LITEOS_CMACRO := -D__LITEOS__
LITEOS_CXXMACRO :=
## head file path and ld path ##
LITEOS_PLATFORM_INCLUDE :=
LITEOS_CXXINCLUDE :=
LITEOS_LD_PATH :=
LITEOS_LD_SCRIPT :=
## c as cxx ld flags ##
LITEOS_ASFLAGS :=
LITEOS_CFLAGS :=
LITEOS_LDFLAGS :=
LITEOS_CXXFLAGS :=
## depended lib ##
LITEOS_BASELIB :=
LITEOS_LIBDEP :=
## directory ##
LIB_SUBDIRS :=

####################################### CPU Option Begin #########################################
include $(LITEOSTOPDIR)/arch/cpu.mk
####################################### CPU Option End #########################################

############################# Platform Option Begin#################################
include $(LITEOSTOPDIR)/platform/bsp.mk
############################# Platform Option End #################################

####################################### Kernel Option Begin ###########################################
LITEOS_BASELIB += -lbase
LIB_SUBDIRS       += kernel/base
LITEOS_KERNEL_INCLUDE   := -I $(LITEOSTOPDIR)/kernel/include \
                           -I $(LITEOSTOPDIR)/kernel/base/include

LITEOS_BASELIB += -lcommon
LIB_SUBDIRS       += kernel/common
LITEOS_KERNEL_INCLUDE   += -I $(LITEOSTOPDIR)/kernel/common

ifeq ($(LOSCFG_KERNEL_CPPSUPPORT), y)
    LITEOS_BASELIB += -lcppsupport
    LIB_SUBDIRS       += kernel/extended/cppsupport
    LITEOS_CPPSUPPORT_INCLUDE   += -I $(LITEOSTOPDIR)/kernel/extended/cppsupport
endif

ifeq ($(LOSCFG_KERNEL_CPUP), y)
    LITEOS_BASELIB   += -lcpup
    LIB_SUBDIRS         += kernel/extended/cpup
    LITEOS_CPUP_INCLUDE := -I $(LITEOSTOPDIR)/kernel/extended/cpup
endif

ifeq ($(LOSCFG_KERNEL_DYNLOAD), y)
    LITEOS_BASELIB   += -ldynload
    LIB_SUBDIRS      += kernel/extended/dynload
    LITEOS_DYNLOAD_INCLUDE   += -I $(LITEOSTOPDIR)/kernel/extended/dynload/include
endif

ifeq ($(LOSCFG_KERNEL_VDSO), y)
    LITEOS_BASELIB   += -lvdso
    LIB_SUBDIRS      += kernel/extended/vdso/usr
    LIB_SUBDIRS      += kernel/extended/vdso/src
    LITEOS_VDSO_INCLUDE   += -I $(LITEOSTOPDIR)/kernel/extended/vdso/include
endif

ifeq ($(LOSCFG_KERNEL_TRACE), y)
    LITEOS_BASELIB    += -ltrace
    LIB_SUBDIRS       += kernel/extended/trace
endif

ifeq ($(LOSCFG_KERNEL_HOOK), y)
    LITEOS_BASELIB += -lhook
    LIB_SUBDIRS       += kernel/extended/hook
    LITEOS_HOOK_INCLUDE   += -I $(LITEOSTOPDIR)/kernel/extended/hook/include
endif

ifeq ($(LOSCFG_KERNEL_LITEIPC), y)
    LITEOS_BASELIB     += -lliteipc
    LIB_SUBDIRS           += kernel/extended/liteipc
    LITEOS_LITEIPC_INCLUDE   += -I $(LITEOSTOPDIR)/kernel/extended/liteipc
endif

ifeq ($(LOSCFG_KERNEL_PIPE), y)
    LITEOS_BASELIB     += -lpipes
    LIB_SUBDIRS           += kernel/extended/pipes
    LITEOS_PIPE_INCLUDE   += -I $(LITEOSTOPDIR)/../../third_party/NuttX/drivers/pipes
endif

ifeq ($(LOSCFG_KERNEL_PM), y)
    LITEOS_BASELIB        += -lpower
    LIB_SUBDIRS           += kernel/extended/power
    LITEOS_PIPE_INCLUDE   += -I $(LITEOSTOPDIR)/kernel/extended/power
endif

ifeq ($(LOSCFG_KERNEL_SYSCALL), y)
    LITEOS_BASELIB += -lsyscall
    LIB_SUBDIRS += syscall
endif
LIB_SUBDIRS += kernel/user

################################### Kernel Option End ################################

#################################### Lib Option Begin ###############################
LITEOS_BASELIB   += -lscrew
LIB_SUBDIRS         += lib/libscrew
LITEOS_LIBSCREW_INCLUDE += -I $(LITEOSTOPDIR)/lib/libscrew/include

ifeq ($(LOSCFG_LIB_LIBC), y)
    LIB_SUBDIRS           += lib/libc
    LITEOS_BASELIB        += -lc
    LITEOS_LIBC_INCLUDE   += \
        -isystem $(LITEOSTHIRDPARTY)/musl/porting/liteos_a/kernel/include

    LIB_SUBDIRS           += lib/libsec
    LITEOS_BASELIB        += -lsec
    LITEOS_LIBC_INCLUDE   += \
        -I $(LITEOSTHIRDPARTY)/bounds_checking_function/include
    LITEOS_CMACRO         += -DSECUREC_IN_KERNEL=0
endif

ifeq ($(LOSCFG_LIB_ZLIB), y)
    LITEOS_BASELIB += -lz
    LIB_SUBDIRS    += lib/zlib
    LITEOS_ZLIB_INCLUDE += -I $(LITEOSTHIRDPARTY)/zlib
endif
################################### Lib Option End ######################################

####################################### Compat Option Begin #########################################
ifeq ($(LOSCFG_COMPAT_POSIX), y)
    LITEOS_BASELIB += -lposix
    LIB_SUBDIRS       += compat/posix
    LITEOS_POSIX_INCLUDE   += \
        -I $(LITEOSTOPDIR)/compat/posix/include
endif

ifeq ($(LOSCFG_COMPAT_BSD), y)
    LITEOS_BASELIB += -lbsd
    LIB_SUBDIRS    += bsd
    LITEOS_BSD_INCLUDE   += -I $(LITEOSTOPDIR)/bsd
    LITEOS_BASELIB += -llinuxkpi
    LIB_SUBDIRS       += bsd/compat/linuxkpi
    LITEOS_LINUX_INCLUDE += -I $(LITEOSTOPDIR)/bsd/compat/linuxkpi/include \
                            -I $(LITEOSTOPDIR)/bsd \
                            -I $(LITEOSTOPDIR)/bsd/kern
endif
######################################## Compat Option End ############################################


#################################### FS Option Begin ##################################
ifeq ($(LOSCFG_FS_VFS), y)
    LITEOS_BASELIB += -lvfs -lmulti_partition
ifeq ($(LOSCFG_FS_VFS_BLOCK_DEVICE), y)
    LITEOS_BASELIB += -lbch
    LIB_SUBDIRS       += $(LITEOSTOPDIR)/drivers/char/bch
endif
    LIB_SUBDIRS       += fs/vfs drivers/mtd/multi_partition
    LITEOS_VFS_INCLUDE   += -I $(LITEOSTOPDIR)/fs/include \
                            -I $(LITEOSTOPDIR)/fs/vfs/include
    LITEOS_VFS_INCLUDE   += -I $(LITEOSTOPDIR)/fs/vfs/include/operation
    LITEOS_VFS_MTD_INCLUDE := -I $(LITEOSTOPDIR)/drivers/mtd/multi_partition/include
endif

ifeq ($(LOSCFG_FS_FAT), y)
    LITEOS_BASELIB  += -lfat
    LIB_SUBDIRS     += fs/fat
    LITEOS_FAT_INCLUDE += -I $(LITEOSTHIRDPARTY)/FatFs/source
endif

ifeq ($(LOSCFG_FS_FAT_VIRTUAL_PARTITION), y)
    LITEOS_BASELIB += -lvirpart
    LIB_SUBDIRS += fs/fat/virpart
    LITEOS_FAT_VIRPART_INCLUDE += -I $(LITEOSTOPDIR)/fs/fat/virpart/include
endif

ifeq ($(LOSCFG_FS_FAT_DISK), y)
    LITEOS_BASELIB += -ldisk
    LIB_SUBDIRS += $(LITEOSTOPDIR)/drivers/block/disk
    LITEOS_VFS_DISK_INCLUDE := -I $(LITEOSTOPDIR)/drivers/block/disk/include
endif

ifeq ($(LOSCFG_FS_FAT_CACHE), y)
    LITEOS_BASELIB  += -lbcache
    LIB_SUBDIRS     += fs/vfs/bcache
    LITEOS_FAT_CACHE_INCLUDE += -I $(LITEOSTOPDIR)/fs/vfs/include/bcache
endif


ifeq ($(LOSCFG_FS_RAMFS), y)
    LITEOS_BASELIB  += -lramfs
    LIB_SUBDIRS     += fs/ramfs
endif

ifeq ($(LOSCFG_FS_ROMFS), y)
    LITEOS_BASELIB  += -lromfs
    LIB_SUBDIRS     += fs/romfs
endif

ifeq ($(LOSCFG_FS_NFS), y)
    LITEOS_BASELIB  += -lnfs
    LIB_SUBDIRS     += fs/nfs
endif

ifeq ($(LOSCFG_FS_PROC), y)
    LITEOS_BASELIB  += -lproc
    LIB_SUBDIRS     += fs/proc
    LITEOS_PROC_INCLUDE += -I $(LITEOSTOPDIR)/fs/proc/include
endif


ifeq ($(LOSCFG_FS_JFFS), y)
    LITEOS_BASELIB  += -ljffs2
    LIB_SUBDIRS     += fs/jffs2
endif

ifeq ($(LOSCFG_PLATFORM_ROOTFS), y)
    LITEOS_BASELIB  += -lrootfs
    LIB_SUBDIRS     += fs/rootfs
    LITEOS_PLATFORM_INCLUDE     += -I $(LITEOSTOPDIR)/fs/rootfs
endif

ifeq ($(LOSCFG_PLATFORM_PATCHFS), y)
    LITEOS_BASELIB  += -lpatchfs
    LIB_SUBDIRS     += fs/patchfs
endif

ifeq ($(LOSCFG_FS_ZPFS), y)
    LITEOS_BASELIB  += -lzpfs
    LIB_SUBDIRS     += fs/zpfs
endif
#################################### FS Option End ##################################


################################### Net Option Begin ###################################
ifeq ($(LOSCFG_NET_LWIP_SACK), y)
ifeq ($(LOSCFG_NET_LWIP_SACK_2_1), y)
    LWIPDIR := $(LITEOSTHIRDPARTY)/lwip/src
    LITEOS_BASELIB += -llwip
    LIB_SUBDIRS       += net/lwip-2.1
    LITEOS_LWIP_SACK_INCLUDE   += \
        -I $(LITEOSTOPDIR)/net/lwip-2.1/porting/include \
        -I $(LWIPDIR)/include \
        -I $(LITEOSTOPDIR)/net/mac
else ifeq ($(LOSCFG_NET_LWIP_SACK_2_0), y)
    LWIPDIR := $(LITEOSTHIRDPARTY)/lwip_enhanced/src
    LITEOS_BASELIB += -llwip
    LIB_SUBDIRS       += $(LWIPDIR)
    LITEOS_LWIP_SACK_INCLUDE   += \
        -I $(LWIPDIR)/include \
        -I $(LITEOSTOPDIR)/net/mac
    LITEOS_CMACRO += -DLWIP_CONFIG_FILE=\"lwip/lwipopts.h\" -DLWIP_LITEOS_A_COMPAT
else
    $(error "unknown lwip version")
endif
endif

#################################### Net Option End####################################
LITEOS_DRIVERS_BASE_PATH := $(LITEOSTOPDIR)/../../drivers/liteos
################################## Driver Option Begin #################################
ifeq ($(LOSCFG_DRIVERS_HDF), y)
include $(LITEOSTOPDIR)/../../drivers/adapter/khdf/liteos/hdf_lite.mk
endif

ifeq ($(LOSCFG_DRIVERS_HIEVENT), y)
    LITEOS_BASELIB     += -lhievent
    LIB_SUBDIRS           += $(LITEOS_DRIVERS_BASE_PATH)/hievent
    LITEOS_HIEVENT_INCLUDE   += -I $(LITEOS_DRIVERS_BASE_PATH)/hievent/include
endif

ifeq ($(LOSCFG_DRIVERS_TZDRIVER), y)
    LITEOS_BASELIB   += -ltzdriver -lmbedtls
    LIB_SUBDIRS         += $(LITEOS_DRIVERS_BASE_PATH)/tzdriver  $(LITEOSTOPDIR)/lib/libmbedtls
    LITEOS_TZDRIVER_INCLUDE += -I $(LITEOS_DRIVERS_BASE_PATH)/tzdriver/include
endif

ifeq ($(LOSCFG_DRIVERS_MEM), y)
    LITEOS_BASELIB += -lmem
    LIB_SUBDIRS       += $(LITEOSTOPDIR)/drivers/char/mem
    LITEOS_DEV_MEM_INCLUDE = -I $(LITEOSTOPDIR)/drivers/char/mem/include
endif

ifeq ($(LOSCFG_DRIVERS_TRACE), y)
    LITEOS_BASELIB += -ltrace_dev
    LIB_SUBDIRS       += $(LITEOSTOPDIR)/drivers/char/trace
endif

ifeq ($(LOSCFG_DRIVERS_QUICKSTART), y)
    LITEOS_BASELIB += -lquickstart
    LIB_SUBDIRS       += $(LITEOSTOPDIR)/drivers/char/quickstart
    LITEOS_DEV_QUICKSTART_INCLUDE = -I $(LITEOSTOPDIR)/drivers/char/quickstart/include
endif

ifeq ($(LOSCFG_DRIVERS_RANDOM), y)
    LITEOS_BASELIB += -lrandom
    LIB_SUBDIRS    += $(LITEOSTOPDIR)/drivers/char/random
    LITEOS_RANDOM_INCLUDE += -I $(LITEOSTOPDIR)/drivers/char/random/include
endif

ifeq ($(LOSCFG_DRIVERS_USB), y)
    LITEOS_BASELIB  += -lusb_base
    LIB_SUBDIRS     += $(LITEOSTOPDIR)/bsd/dev/usb
    LITEOS_USB_INCLUDE += -I $(LITEOSTOPDIR)/bsd/dev/usb
    LITEOS_CMACRO   += -DUSB_DEBUG_VAR=5
endif

ifeq ($(LOSCFG_DRIVERS_VIDEO), y)
    LITEOS_BASELIB += -lvideo
    LIB_SUBDIRS       += $(LITEOSTOPDIR)/drivers/char/video
    LITEOS_VIDEO_INCLUDE += -I $(LITEOSTHIRDPARTY)/NuttX/include/nuttx/video
endif

############################## Driver Option End #######################################

############################## Dfx Option Begin#######################################
ifeq ($(LOSCFG_BASE_CORE_HILOG), y)
    LITEOS_BASELIB      += -lhilog
    LIB_SUBDIRS         += $(LITEOSTOPDIR)/../../base/hiviewdfx/hilog_lite/frameworks/featured
    LIB_SUBDIRS         += $(LITEOSTOPDIR)/kernel/extended/hilog
    LITEOS_HILOG_INCLUDE  += -I $(LITEOSTOPDIR)/../../base/hiviewdfx/hilog_lite/interfaces/native/kits
    LITEOS_HILOG_INCLUDE  += -I $(LITEOSTOPDIR)/../../base/hiviewdfx/hilog_lite/interfaces/native/kits/hilog
    LITEOS_HILOG_INCLUDE  += -I $(LITEOSTOPDIR)/kernel/extended/hilog
endif
ifeq ($(LOSCFG_BLACKBOX), y)
    LITEOS_BASELIB     += -lblackbox
    LIB_SUBDIRS           += $(LITEOSTOPDIR)/kernel/extended/blackbox
    LITEOS_BLACKBOX_INCLUDE  += -I $(LITEOSTOPDIR)/kernel/extended/blackbox
endif
ifeq ($(LOSCFG_HIDUMPER), y)
    LITEOS_BASELIB     += -lhidumper
    LIB_SUBDIRS           += $(LITEOSTOPDIR)/kernel/extended/hidumper
    LITEOS_HIDUMPER_INCLUDE  += -I $(LITEOSTOPDIR)/kernel/extended/hidumper
endif
############################## Dfx Option End #######################################

############################# Tools && Debug Option Begin ##############################
ifeq ($(LOSCFG_COMPRESS), y)
    LITEOS_BASELIB    += -lcompress
    LIB_SUBDIRS       += tools/compress
endif

ifneq ($(LOSCFG_DEBUG_VERSION), y)
    LITEOS_COPTS_DEBUG  += -DNDEBUG
endif

ifeq ($(LOSCFG_COMPILE_DEBUG), y)
    LITEOS_COPTS_OPTIMIZE = -O0
    LITEOS_COPTS_OPTION  = -g -gdwarf-2
endif
ifeq ($(LOSCFG_COMPILE_OPTIMIZE), y)
    LITEOS_COPTS_OPTIMIZE = -O2
endif
ifeq ($(LOSCFG_COMPILE_OPTIMIZE_SIZE), y)
    ifeq ($(LOSCFG_COMPILER_CLANG_LLVM), y)
        LITEOS_COPTS_OPTIMIZE = -Oz
    else
        LITEOS_COPTS_OPTIMIZE = -Os
    endif
endif
ifeq ($(LOSCFG_COMPILE_LTO), y)
    ifeq ($(LOSCFG_COMPILER_CLANG_LLVM), y)
        LITEOS_COPTS_OPTIMIZE += -flto=thin
    else
        LITEOS_COPTS_OPTIMIZE += -flto
    endif
endif
    LITEOS_COPTS_DEBUG  += $(LITEOS_COPTS_OPTION) $(LITEOS_COPTS_OPTIMIZE)
    LITEOS_CXXOPTS_BASE += $(LITEOS_COPTS_OPTION) $(LITEOS_COPTS_OPTIMIZE)
    LITEOS_ASOPTS   += $(LITEOS_COPTS_OPTION)

ifeq ($(LOSCFG_SHELL), y)
    LITEOS_BASELIB += -lshell
    LIB_SUBDIRS       += shell
    LITEOS_SHELL_INCLUDE  += -I $(LITEOSTOPDIR)/shell/full/include
endif


ifeq ($(LOSCFG_NET_TELNET), y)
    LITEOS_BASELIB += -ltelnet
    LIB_SUBDIRS       += net/telnet
    LITEOS_TELNET_INCLUDE   += \
        -I $(LITEOSTOPDIR)/net/telnet/include
endif
############################# Tools && Debug Option End #################################

############################# Security Option Begin ##############################
LITEOS_SSP = -fno-stack-protector
ifeq ($(LOSCFG_CC_STACKPROTECTOR), y)
    LITEOS_SSP = -fstack-protector --param ssp-buffer-size=4
endif

ifeq ($(LOSCFG_CC_STACKPROTECTOR_STRONG), y)
    LITEOS_SSP = -fstack-protector-strong
endif

ifeq ($(LOSCFG_CC_STACKPROTECTOR_ALL), y)
    LITEOS_SSP = -fstack-protector-all
endif

ifeq ($(LOSCFG_SECURITY), y)
    LIB_SUBDIRS += security
    LITEOS_BASELIB += -lsecurity
ifeq ($(LOSCFG_SECURITY_CAPABILITY), y)
    LITEOS_SECURITY_CAP_INC := -I $(LITEOSTOPDIR)/security/cap
endif
ifeq ($(LOSCFG_SECURITY_VID), y)
    LITEOS_SECURITY_VID_INC := -I $(LITEOSTOPDIR)/security/vid
endif
endif

############################# Security Option End ##############################

LITEOS_EXTKERNEL_INCLUDE   := $(LITEOS_CPPSUPPORT_INCLUDE) $(LITEOS_DYNLOAD_INCLUDE) \
                              $(LITEOS_TICKLESS_INCLUDE)   $(LITEOS_HOOK_INCLUDE)\
                              $(LITEOS_VDSO_INCLUDE)       $(LITEOS_LITEIPC_INCLUDE) \
                              $(LITEOS_PIPE_INCLUDE)       $(LITEOS_CPUP_INCLUDE)
LITEOS_COMPAT_INCLUDE      := $(LITEOS_POSIX_INCLUDE) $(LITEOS_LINUX_INCLUDE) \
                              $(LITEOS_BSD_INCLUDE)
LITEOS_FS_INCLUDE          := $(LITEOS_VFS_INCLUDE)        $(LITEOS_FAT_CACHE_INCLUDE) \
                              $(LITEOS_VFS_MTD_INCLUDE)    $(LITEOS_VFS_DISK_INCLUDE) \
                              $(LITEOS_PROC_INCLUDE)       $(LITEOS_FAT_VIRPART_INCLUDE) \
                              $(LITEOS_FAT_INCLUDE)
LITEOS_NET_INCLUDE         := $(LITEOS_LWIP_SACK_INCLUDE)
LITEOS_LIB_INCLUDE         := $(LITEOS_LIBC_INCLUDE)       $(LITEOS_LIBM_INCLUDE) \
                              $(LITEOS_ZLIB_INCLUDE)       $(LITEOS_LIBSCREW_INCLUDE)
LITEOS_DRIVERS_INCLUDE     := $(LITEOS_CELLWISE_INCLUDE)   $(LITEOS_GPIO_INCLUDE) \
                              $(LITEOS_HIDMAC_INCLUDE)     $(LITEOS_HIETH_SF_INCLUDE) \
                              $(LITEOS_HIGMAC_INCLUDE)     $(LITEOS_I2C_INCLUDE) \
                              $(LITEOS_LCD_INCLUDE)        $(LITEOS_MMC_INCLUDE) \
                              $(LITEOS_MTD_SPI_NOR_INCLUDE) $(LITEOS_MTD_NAND_INCLUDE) \
                              $(LITEOS_RANDOM_INCLUDE)     $(LITEOS_RTC_INCLUDE) \
                              $(LITEOS_SPI_INCLUDE)        $(LITEOS_USB_INCLUDE) \
                              $(LITEOS_WTDG_INCLUDE)       $(LITEOS_DBASE_INCLUDE) \
                              $(LITEOS_CPUFREQ_INCLUDE)    $(LITEOS_DEVFREQ_INCLUDE) \
                              $(LITEOS_REGULATOR_INCLUDE)  $(LITEOS_VIDEO_INCLUDE) \
                              $(LITEOS_DRIVERS_HDF_INCLUDE) $(LITEOS_TZDRIVER_INCLUDE) \
                              $(LITEOS_HIEVENT_INCLUDE)    $(LITEOS_DEV_MEM_INCLUDE) \
                              $(LITEOS_DEV_QUICKSTART_INCLUDE)
LITEOS_DFX_INCLUDE    := $(LITEOS_HILOG_INCLUDE) \
                         $(LITEOS_BLACKBOX_INCLUDE) \
                         $(LITEOS_HIDUMPER_INCLUDE)

LITEOS_SECURITY_INCLUDE    := $(LITEOS_SECURITY_CAP_INC) $(LITEOS_SECURITY_VID_INC)
LOSCFG_TOOLS_DEBUG_INCLUDE := $(LITEOS_SHELL_INCLUDE)  $(LITEOS_UART_INCLUDE) \
                              $(LITEOS_TELNET_INCLUDE)

LITEOS_COMMON_OPTS  := -fno-pic -fno-builtin -nostdinc -nostdlib -Wall -Werror -fms-extensions -fno-omit-frame-pointer -Wno-address-of-packed-member -Winvalid-pch

LITEOS_CXXOPTS_BASE += $(LITEOS_COMMON_OPTS) -std=c++11 -nostdinc++ -fexceptions -fpermissive -fno-use-cxa-atexit -frtti

LITEOS_COPTS_BASE   += $(LITEOS_COMMON_OPTS) $(LITEOS_SSP) -fno-strict-aliasing -fno-common -fsigned-char -mno-unaligned-access
ifneq ($(LOSCFG_COMPILER_CLANG_LLVM), y)
LITEOS_COPTS_BASE   += -fno-aggressive-loop-optimizations
endif

LITEOS_COPTS_EXTRA  += -std=c99 -Wpointer-arith -Wstrict-prototypes -ffunction-sections -fdata-sections -fno-exceptions -fno-short-enums
ifeq ($(LOSCFG_ARCH_ARM_AARCH32), y)
ifneq ($(LOSCFG_COMPILER_CLANG_LLVM), y)
LITEOS_COPTS_EXTRA  += -mthumb-interwork
endif
endif

ifeq ($(LOSCFG_THUMB), y)
ifeq ($(LOSCFG_COMPILER_CLANG_LLVM), y)
LITEOS_CFLAGS_INTERWORK := -mthumb -mimplicit-it=thumb
else
LITEOS_CFLAGS_INTERWORK := -mthumb -Wa,-mimplicit-it=thumb
endif
endif

# kernel configuration macros
LITEOS_CMACRO     += -imacros "$(LITEOS_MENUCONFIG_H)"

ifneq ($(LOSCFG_COMPILER_CLANG_LLVM), y)
LITEOS_LD_OPTS += -nostartfiles
endif
LITEOS_LD_OPTS += -static --gc-sections
LITEOS_LD_PATH += -L$(OUT)/lib

ifeq ($(LOSCFG_COMPILER_CLANG_LLVM), y)
LITEOS_LD_SCRIPT := -T$(LITEOSTOPDIR)/tools/build/liteos_llvm.ld
else
LITEOS_LD_SCRIPT := -T$(LITEOSTOPDIR)/tools/build/liteos.ld
endif

##compiler##
LITEOS_BASELIB     += $(shell $(CC) $(LITEOS_CORE_COPTS) "-print-libgcc-file-name")
LITEOS_LIB_INCLUDE += -isystem $(shell $(CC) $(LITEOS_CORE_COPTS) "-print-file-name=include")
