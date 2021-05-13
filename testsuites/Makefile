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

LITEOSTESTTOPDIR := $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)
export OS=$(shell uname -s)
ifneq ($(OS), Linux)
LITEOSTESTTOPDIR := $(shell dirname $(subst \,/,$(LITEOSTESTTOPDIR))/./)
endif

LITEOSTOPDIR := $(LITEOSTESTTOPDIR)/..
export LITEOSTOPDIR
export LITEOSTESTTOPDIR

-include $(LITEOSTESTTOPDIR)/config.mk

RM = -rm -rf
MAKE = make
__LIBS = libs
APPS = apps
KERNEL = kernel
TEST = test
TEST_KERNEL = test_kernel
TEST_APPS = test_apps
ROOTFSDIR = rootfsdir
ROOTFS = rootfs
HIDE := @

LITEOS_TEST_TARGET = liteos_ktest
LITEOS_TEST_LIBC = test_libs
LITEOS_LIBS_TARGET = libs_target

LITEOS_TEST_AUTOCONFIG_H = $(LITEOSTESTTOPDIR)/include/generated/autoconf.h
LITEOS_TEST_KERNEL_MENUCONFIG_H = $(LITEOSTESTTOPDIR)/kernel/include
#######LITEOS_TEST_USER_MENUCONFIG_H = $(LITEOSTESTTOPDIR)/apps/include
ifeq ($(LOSCFG_KERNEL_TEST), y)
LITEOS_TEST_MENUCONFIG_H = $(LITEOS_TEST_KERNEL_MENUCONFIG_H)/test_menuconfig.h
#######else
#######LITEOS_TEST_MENUCONFIG_H = $(LITEOS_TEST_USER_MENUCONFIG_H)/test_menuconfig.h
endif

ifeq ($(LOSCFG_STORAGE_SPINOR), y)
FSTYPE = jffs2
endif
ifeq ($(LOSCFG_STORAGE_EMMC), y)
FSTYPE = vfat
endif
ROOTFS_DIR = $(OUT)/rootfs
ROOTFS_ZIP = $(OUT)/rootfs.zip
VERSION =

LITEOS_LIBDEP   := $(LITEOS_BASELIB)

$(TEST_KERNEL): $(LITEOS_TEST_TARGET)

$(LITEOS_TEST_TARGET): $(LITEOS_TEST_LIBC)
	$(LD) $(LITEOS_LDFLAGS) $(LITEOS_TABLES_LDFLAGS) $(LITEOS_DYNLDFLAGS) -Map=$(OUT)/$@.map -o $(OUT)/$@ --start-group $(LITEOS_LIBDEP) --end-group
	$(OBJCOPY) -O binary $(OUT)/$@ $(LITEOS_TARGET_DIR)/$@.bin
	$(OBJDUMP) -t $(OUT)/$@ |sort >$(OUT)/$@.sym.sorted
	$(OBJDUMP) -d $(OUT)/$@ >$(OUT)/$@.asm

$(LITEOS_TEST_LIBC): $(KERNEL)
	$(HIDE)for dir in $(TESTLIB_SUBDIRS); \
		do $(MAKE) -C $$dir all || exit 1; \
	done
	$(HIDE)echo "=============== make lib done  ==============="

$(KERNEL): $(LITEOS_TEST_AUTOCONFIG_H)
	$(HIDE)$(MAKE) -C ../ lib || exit 1

##### make test menuconfig #####
export CONFIG_=LOSCFG_
MENUCONFIG_PATH = $(LITEOSTOPDIR)/tools/menuconfig
TEST_KCONFIG_FILE_PATH = $(LITEOSTESTTOPDIR)/Kconfig

menuconfig:$(MENUCONFIG_PATH)/mconf
	$< $(TEST_KCONFIG_FILE_PATH)

genconfig:$(MENUCONFIG_PATH)/conf
	$(HIDE)mkdir -p include/config include/generated
	$< --silentoldconfig $(TEST_KCONFIG_FILE_PATH)
	mv -f $(LITEOS_TEST_AUTOCONFIG_H) $(LITEOS_TEST_MENUCONFIG_H)

##### menuconfig end #######

genconfig:$(MENUCONFIG_PATH)/conf

$(LITEOS_TEST_AUTOCONFIG_H):
ifneq ($(LITEOS_TEST_MENUCONFIG_H), $(wildcard $(LITEOS_TEST_MENUCONFIG_H)))
	$(HIDE)$(MAKE) genconfig
endif

clean:
	$(HIDE)$(MAKE) -C ../ clean || exit 1
	$(HIDE)for dir in $(TESTLIB_SUBDIRS); \
		do $(MAKE) -C $$dir clean || exit 1; \
	done
	$(HIDE)$(RM) include
	$(HIDE)$(RM) $(LITEOS_TEST_MENUCONFIG_H)
	$(HIDE)$(RM) $(OUT)
.PHONY: test_apps test_kernel clean
