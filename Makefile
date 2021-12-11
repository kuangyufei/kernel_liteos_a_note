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

LITEOSTOPDIR := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))
export LITEOSTOPDIR

APPS = apps
ROOTFS = rootfs
LITEOS_TARGET = liteos
LITEOS_LIBS_TARGET = libs
KCONFIG_CMDS := $(notdir $(wildcard $(dir $(shell which menuconfig))*config))

ohos_kernel ?= liteos_a
$(foreach line,$(shell hb env | sed 's/\[OHOS INFO\]/ohos/g;s/ /_/g;s/:_/=/g' || true),$(eval $(line)))
ifneq ($(ohos_kernel),liteos_a)
$(error The selected product ($(ohos_product)) is not a liteos_a kernel type product)
endif

ifeq ($(PRODUCT_PATH),)
PRODUCT_PATH:=$(ohos_product_path)
endif

ifeq ($(DEVICE_PATH),)
DEVICE_PATH:=$(ohos_device_path)
endif

ifeq ($(TEE:1=y),y)
tee = _tee
endif
ifeq ($(RELEASE:1=y),y)
CONFIG ?= $(PRODUCT_PATH)/kernel_configs/release$(tee).config
else
CONFIG ?= $(PRODUCT_PATH)/kernel_configs/debug$(tee).config
endif

KCONFIG_CONFIG ?= $(CONFIG)
SYSROOT_PATH ?= $(OUT)/sysroot

# export subdir Makefile related environment variables
export SYSROOT_PATH
export PRODUCT_PATH
export DEVICE_PATH

# export kconfig related environment variables
export CONFIG_=LOSCFG_
export srctree=$(LITEOSTOPDIR)

include $(LITEOSTOPDIR)/config.mk

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

define HELP =
Usage: make [TARGET]... [PARAMETER=VALUE]...

Targets:
    help:       display this help and exit
    clean:      clean compiled objects
    cleanall:   clean all build outputs
    all:        make liteos kernel image and rootfs image (Default target)
    $(APPS):       build all apps
    $(ROOTFS):     make a original rootfs image
    $(LITEOS_LIBS_TARGET):       compile all kernel modules (libraries)
    $(LITEOS_TARGET):     make liteos kernel image
    update_config:  update product kernel config (use menuconfig)
    xxconfig:   invoke xxconfig command of kconfiglib (xxconfig is one of $(KCONFIG_CMDS))

Parameters:
    FSTYPE:     value should be one of (jffs2 vfat yaffs2)
    TEE:        boolean value(1 or y for true), enable tee
    RELEASE:    boolean value(1 or y for true), build release version
    CONFIG:     kernel config file to be use
    args:       arguments for xxconfig command
endef
export HELP

all: $(LITEOS_TARGET) $(ROOTFS)

help:
	$(HIDE)echo "$$HELP"

sysroot:
	$(HIDE)echo "sysroot:" $(abspath $(SYSROOT_PATH))
ifeq ($(origin SYSROOT_PATH),file)
	$(HIDE)mkdir -p $(SYSROOT_PATH)/build && cd $(SYSROOT_PATH)/build && \
	ln -snf $(LITEOSTOPDIR)/../../prebuilts/lite/sysroot/build/Makefile && \
	$(MAKE) TARGETS=liteos_a_user \
		ARCH=$(ARCH) \
		TARGET=$(LOSCFG_LLVM_TARGET) \
		ARCH_CFLAGS="$(LITEOS_CORE_COPTS) -w" \
		TOPDIR="$(LITEOSTOPDIR)/../.." \
		SYSROOTDIR="$(SYSROOT_PATH)" \
		$(if $(LOSCFG_COMPILER_CLANG_LLVM),CLANG="$(LITEOS_COMPILER_PATH)clang",GCC="$(CC)") \
		BUILD_DEBUG=$(if $(patsubst y,,$(or $(RELEASE:1=y),n)),true,false)
endif

$(filter-out menuconfig,$(KCONFIG_CMDS)):
	$(HIDE)$@ $(args)

$(LITEOS_CONFIG_FILE): $(KCONFIG_CONFIG)
	$(HIDE)env KCONFIG_CONFIG=$< genconfig --config-out $@ --header-path $(LITEOS_MENUCONFIG_H)

update_config menuconfig:
	$(HIDE)test -f "$(CONFIG)" && cp -v "$(CONFIG)" .config && menuconfig $(args) && savedefconfig --out "$(CONFIG)"

$(LITEOS_LIBS_TARGET): sysroot
	$(HIDE)for dir in $(LIB_SUBDIRS); do $(MAKE) -C $$dir all || exit 1; done

$(LITEOS_TARGET): $(OUT)/$(LITEOS_TARGET)
$(LITEOS_TARGET): $(OUT)/$(LITEOS_TARGET).map
#$(LITEOS_TARGET): $(OUT)/$(LITEOS_TARGET).objsize
$(LITEOS_TARGET): $(OUT)/$(LITEOS_TARGET).bin
$(LITEOS_TARGET): $(OUT)/$(LITEOS_TARGET).sym.sorted
$(LITEOS_TARGET): $(OUT)/$(LITEOS_TARGET).asm
#$(LITEOS_TARGET): $(OUT)/$(LITEOS_TARGET).size

$(OUT)/$(LITEOS_TARGET): $(LITEOS_LIBS_TARGET)
	$(LD) $(LITEOS_LDFLAGS) $(LITEOS_TABLES_LDFLAGS) -Map=$@.map -o $@ --start-group $(LITEOS_LIBDEP) --end-group
$(OUT)/$(LITEOS_TARGET).map: $(OUT)/$(LITEOS_TARGET)
$(OUT)/$(LITEOS_TARGET).objsize: $(LITEOS_LIBS_TARGET)
	$(SIZE) -t --common $(OUT)/lib/*.a >$@
$(OUT)/$(LITEOS_TARGET).bin: $(OUT)/$(LITEOS_TARGET)
	$(OBJCOPY) -O binary $< $@
$(OUT)/$(LITEOS_TARGET).sym.sorted: $(OUT)/$(LITEOS_TARGET)
	$(OBJDUMP) -t $< |sort >$@
$(OUT)/$(LITEOS_TARGET).asm: $(OUT)/$(LITEOS_TARGET)
	$(OBJDUMP) -d $< >$@
$(OUT)/$(LITEOS_TARGET).size: $(OUT)/$(LITEOS_TARGET)
	$(NM) -S --size-sort $< >$@

$(APPS): sysroot
	$(HIDE)$(MAKE) -C apps all

$(ROOTFS): $(APPS)
	$(HIDE)mkdir -p $(OUT)/musl
ifeq ($(LOSCFG_COMPILER_CLANG_LLVM), y)
	$(HIDE)cp -fp $$($(CC) $(LITEOS_CFLAGS) -print-file-name=libc.so) $(OUT)/musl
	$(HIDE)cp -fp $$($(GPP) $(LITEOS_CXXFLAGS) -print-file-name=libc++.so) $(OUT)/musl
else
	$(HIDE)cp -fp $$($(CC) $(LITEOS_CFLAGS) -print-file-name=libc.so) $(OUT)/musl
	$(HIDE)cp -fp $$($(CC) $(LITEOS_CFLAGS) -print-file-name=libgcc_s.so.1) $(OUT)/musl
	$(HIDE)cp -fp $$($(GPP) $(LITEOS_CXXFLAGS) -print-file-name=libstdc++.so.6) $(OUT)/musl
endif
	$(HIDE)$(LITEOS_SCRIPTPATH)/make_rootfs/rootfsdir.sh $(OUT) $(ROOTFS_DIR)
	$(HIDE)shopt -s nullglob && $(STRIP) $(ROOTFS_DIR)/bin/* $(ROOTFS_DIR)/lib/*
ifneq ($(VERSION),)
	$(HIDE)$(LITEOS_SCRIPTPATH)/make_rootfs/releaseinfo.sh "$(VERSION)" $(ROOTFS_DIR)
endif
	$(HIDE)$(LITEOS_SCRIPTPATH)/make_rootfs/rootfsimg.sh $(ROOTFS_DIR) $(FSTYPE)
	$(HIDE)cd $(ROOTFS_DIR)/.. && zip -r $(ROOTFS_ZIP) $(ROOTFS)

clean:
	$(HIDE)if [ -d $(SYSROOT_PATH)/build ]; then $(MAKE) -C $(SYSROOT_PATH)/build clean; fi
	$(HIDE)for dir in $(LIB_SUBDIRS) apps; do $(MAKE) -C $$dir clean || exit 1; done
	$(HIDE)$(RM) $(LITEOS_MENUCONFIG_H)
	$(HIDE)echo "clean $(LOSCFG_PLATFORM) finish"

cleanall: clean
	$(HIDE)$(RM) $(LITEOSTOPDIR)/out $(LITEOS_CONFIG_FILE)
	$(HIDE)echo "clean all done"

.PHONY: all clean cleanall sysroot help update_config
.PHONY: $(LITEOS_TARGET) $(ROOTFS) $(APPS) $(KCONFIG_CMDS) $(LITEOS_LIBS_TARGET) $(KCONFIG_CONFIG)
