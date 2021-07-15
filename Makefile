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

LITEOSTOPDIR := $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)
export OS=$(shell uname -s)
ifneq ($(OS), Linux)
LITEOSTOPDIR := $(shell dirname $(subst \,/,$(LITEOSTOPDIR))/./)
endif

LITEOSTHIRDPARTY := $(LITEOSTOPDIR)/../../third_party

export LITEOSTOPDIR
export LITEOSTHIRDPARTY

RM = -rm -rf
MAKE = make
__LIBS = libs
APPS = apps
ROOTFSDIR = rootfsdir
ROOTFS = rootfs

LITEOS_TARGET = liteos
LITEOS_LIBS_TARGET = libs_target
LITEOS_PLATFORM_BASE = $(LITEOSTOPDIR)/platform

export CONFIG_=LOSCFG_
ifeq ($(PRODUCT_PATH),)
export PRODUCT_PATH=$(LITEOSTOPDIR)/../../device/hisilicon/drivers
endif

ifeq ($(shell which menuconfig),)
$(shell pip install --user kconfiglib >/dev/null)
endif
$(shell env CONFIG_=$(CONFIG_) PRODUCT_PATH=$(PRODUCT_PATH) olddefconfig >/dev/null)

-include $(LITEOSTOPDIR)/tools/build/config.mk

ifeq ($(LOSCFG_STORAGE_SPINOR), y)
FSTYPE = jffs2
endif
ifeq ($(LOSCFG_STORAGE_EMMC), y)
FSTYPE = vfat
endif
ifeq ($(LOSCFG_STORAGE_SPINAND), y)
FSTYPE = yaffs2
endif
ifeq ($(LOSCFG_PLATFORM_QEMU_ARM_VIRT_CA7), y)
FSTYPE = jffs2
endif
ROOTFS_DIR = $(OUT)/rootfs
ROOTFS_ZIP = $(OUT)/rootfs.zip
VERSION =

SYSROOT_PATH ?= $(LITEOSTOPDIR)/../../prebuilts/lite/sysroot
export SYSROOT_PATH

all: $(OUT) $(BUILD) $(LITEOS_TARGET) $(APPS)
lib: $(OUT) $(BUILD) $(LITEOS_LIBS_TARGET)

help:
	$(HIDE)echo "-------------------------------------------------------"
	$(HIDE)echo "1.====make help:    get help infomation of make"
	$(HIDE)echo "2.====make:         make a debug version based the .config"
	$(HIDE)echo "3.====make debug:   make a debug version based the .config"
	$(HIDE)echo "4.====make release: make a release version for all platform"
	$(HIDE)echo "5.====make release PLATFORM=xxx:  make a release version only for platform xxx"
	$(HIDE)echo "6.====make rootfsdir: make a original rootfs dir"
	$(HIDE)echo "7.====make rootfs FSTYPE=***: make a original rootfs img"
	$(HIDE)echo "8.====make test: make the testsuits_app and put it into the rootfs dir"
	$(HIDE)echo "9.====make test_apps FSTYPE=***: make a rootfs img with the testsuits_app in it"
	$(HIDE)echo "xxx should be one of (hi3516cv300 hi3516ev200 hi3556av100/cortex-a53_aarch32 hi3559av100/cortex-a53_aarch64)"
	$(HIDE)echo "*** should be one of (jffs2)"
	$(HIDE)echo "-------------------------------------------------------"

debug:
	$(HIDE)echo "=============== make a debug version  ==============="
	$(HIDE) $(MAKE) all

release:
ifneq ($(PLATFORM),)
	$(HIDE)echo "=============== make a release version for platform $(PLATFORM) ==============="
	$(HIDE)$(SCRIPTS_PATH)/mklibversion.sh $(PLATFORM)
else
	$(HIDE)echo "================make a release version for all platform ==============="
	$(HIDE)$(SCRIPTS_PATH)/mklibversion.sh
endif

##### make sysroot #####
sysroot:
ifeq ($(LOSCFG_COMPILER_CLANG_LLVM), y)
ifeq ($(wildcard $(SYSROOT_PATH)/usr/include/$(LLVM_TARGET)/),)
	$(HIDE)$(MAKE) -C $(SYSROOT_PATH)/build TARGETS=liteos_a_user
endif
	$(HIDE)echo "sysroot:" $(abspath $(SYSROOT_PATH))
endif

##### make dynload #####
-include $(LITEOS_MK_PATH)/dynload.mk

#-----need move when make version-----#
##### make lib #####
$(__LIBS): $(OUT) $(CXX_INCLUDE)

$(OUT): $(LITEOS_MENUCONFIG_H)
	$(HIDE)mkdir -p $(OUT)/lib
	$(HIDE)$(CC) -I$(LITEOSTOPDIR)/kernel/base/include -I$(LITEOSTOPDIR)/../../$(LOSCFG_BOARD_CONFIG_PATH) \
		-I$(LITEOS_PLATFORM_BASE)/include -imacros $< -E $(LITEOS_PLATFORM_BASE)/board.ld.S \
		-o $(LITEOS_PLATFORM_BASE)/board.ld -P

$(BUILD):
	$(HIDE)mkdir -p $(BUILD)

$(LITEOS_LIBS_TARGET): $(__LIBS) sysroot
	$(HIDE)for dir in $(LIB_SUBDIRS); \
		do $(MAKE) -C $$dir all || exit 1; \
	done
	$(HIDE)echo "=============== make lib done  ==============="

##### make menuconfig #####
menuconfig:
	$(HIDE)menuconfig
##### menuconfig end #######

$(LITEOS_MENUCONFIG_H): .config
	$(HIDE)genconfig

$(LITEOS_TARGET): $(__LIBS) sysroot
	$(HIDE)touch $(LOSCFG_ENTRY_SRC)

	$(HIDE)for dir in $(LITEOS_SUBDIRS); \
	do $(MAKE) -C $$dir all || exit 1; \
	done

	$(LD) $(LITEOS_LDFLAGS) $(LITEOS_TABLES_LDFLAGS) $(LITEOS_DYNLDFLAGS) -Map=$(OUT)/$@.map -o $(OUT)/$@ --start-group $(LITEOS_LIBDEP) --end-group
#	$(SIZE) -t --common $(OUT)/lib/*.a >$(OUT)/$@.objsize
	$(OBJCOPY) -O binary $(OUT)/$@ $(LITEOS_TARGET_DIR)/$@.bin
	$(OBJDUMP) -t $(OUT)/$@ |sort >$(OUT)/$@.sym.sorted
	$(OBJDUMP) -d $(OUT)/$@ >$(OUT)/$@.asm
#	$(NM) -S --size-sort $(OUT)/$@ >$(OUT)/$@.size

$(APPS): $(LITEOS_TARGET) sysroot
	$(HIDE)$(MAKE) -C apps all

prepare:
	$(HIDE)mkdir -p $(OUT)/musl
ifeq ($(LOSCFG_COMPILER_CLANG_LLVM), y)
	$(HIDE)cp -f $$($(CC) --target=$(LLVM_TARGET) --sysroot=$(SYSROOT_PATH) $(LITEOS_CFLAGS) -print-file-name=libc.so) $(OUT)/musl
	$(HIDE)cp -f $$($(GPP) --target=$(LLVM_TARGET) --sysroot=$(SYSROOT_PATH) $(LITEOS_CXXFLAGS) -print-file-name=libc++.so) $(OUT)/musl
else
	$(HIDE)cp -f $(LITEOS_COMPILER_PATH)/target/usr/lib/libc.so $(OUT)/musl
	$(HIDE)cp -f $(LITEOS_COMPILER_PATH)/arm-linux-musleabi/lib/libstdc++.so.6 $(OUT)/musl
	$(HIDE)cp -f $(LITEOS_COMPILER_PATH)/arm-linux-musleabi/lib/libgcc_s.so.1 $(OUT)/musl
	$(STRIP) $(OUT)/musl/*
endif

$(ROOTFSDIR): prepare $(APPS)
	$(HIDE)$(MAKE) clean -C apps
	$(HIDE)$(LITEOSTOPDIR)/tools/scripts/make_rootfs/rootfsdir.sh $(OUT)/bin $(OUT)/musl $(ROOTFS_DIR) $(LITEOS_TARGET_DIR)
ifneq ($(VERSION),)
	$(HIDE)$(LITEOSTOPDIR)/tools/scripts/make_rootfs/releaseinfo.sh "$(VERSION)" $(ROOTFS_DIR) $(LITEOS_TARGET_DIR)
endif

$(ROOTFS): $(ROOTFSDIR)
	$(HIDE)$(LITEOSTOPDIR)/tools/scripts/make_rootfs/rootfsimg.sh $(ROOTFS_DIR) $(FSTYPE)
	$(HIDE)cd $(ROOTFS_DIR)/.. && zip -r $(ROOTFS_ZIP) $(ROOTFS)

clean:
	$(HIDE)for dir in $(LITEOS_SUBDIRS); \
		do $(MAKE) -C $$dir clean|| exit 1; \
	done
	$(HIDE)$(MAKE) -C apps clean
	$(HIDE)$(RM) $(__OBJS) $(LITEOS_TARGET) $(BUILD) $(LITEOS_MENUCONFIG_H) *.bak *~
	$(HIDE)$(RM) include/config include/generated
	$(HIDE)$(MAKE) cleanrootfs
	$(HIDE)echo "clean $(LITEOS_PLATFORM) finish"

cleanall: clean
	$(HIDE)$(RM) $(LITEOSTOPDIR)/out $(LITEOS_PLATFORM_BASE)/board.ld
	$(HIDE)echo "clean all done"

cleanrootfs:
	$(HIDE)$(RM) $(OUT)/rootfs
	$(HIDE)$(RM) $(OUT)/rootfs.zip
	$(HIDE)$(RM) $(OUT)/rootfs.img

update_all_config:
	$(HIDE)shopt -s globstar && for f in tools/build/config/**/*.config ; \
		do \
			echo updating $$f; \
			test -f $$f && cp $$f .config && olddefconfig && savedefconfig --out $$f; \
		done

update_config:
	$(HIDE)test -f "$(CONFIG)" && cp "$(CONFIG)" .config && menuconfig && savedefconfig --out "$(CONFIG)"

.PHONY: all lib clean cleanall $(LITEOS_TARGET) debug release help update_all_config update_config
.PHONY: prepare sysroot cleanrootfs $(ROOTFS) $(ROOTFSDIR) $(APPS) menuconfig $(LITEOS_LIBS_TARGET) $(__LIBS) $(OUT)
