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

MKSH := mksh

MKSH_DIR := $(shell pwd)/
LITEOSTOPDIR = $(MKSH_DIR)/../../
include $(MKSH_DIR)/../config.mk

APPS_OUT := $(OUT)/bin
ETC_OUT := $(OUT)/etc
BUILD_DIR := $(MKSH_DIR)/build
BUILD_LOG := $(MKSH_DIR)/build.log
TARGET_OS := OpenHarmony

LOCAL_CFLAGS := -flto -fdata-sections -ffunction-sections -fstack-protector-strong -D_FORTIFY_SOURCE=2
LOCAL_CFLAGS += -DMKSH_DISABLE_TTY_WARNING -DMKSH_SMALL=1 -DMKSH_ASSUME_UTF8=1 -DMKSH_SMALL_BUT_FAST=0 -DMKSH_S_NOVI=1 -DHAVE_CAN_FSTACKPROTECTORSTRONG=1
LOCAL_CFLAGS += -DMKSH_LESS_CMDLINE_EDITING -DMKSH_LESS_BUILDINS -DMKSH_NO_INITCOMS -DADAPT_FOR_LITEOS_A
LOCAL_LDFLAGS := -Wl,--gc-sections -flto -O2

ifeq ($(LOSCFG_COMPILER_CLANG_LLVM), y)
LOCAL_CFLAGS += -Oz --target=$(LLVM_TARGET) $(LLVM_SYSROOT)
else
LOCAL_CFLAGS += -O2
endif

all:$(MKSH)

$(MKSH):
ifneq ($(wildcard $(BUILD_DIR)/Rebuild.sh),)
	$(HIDE)echo "not clean, rebuilding now"
	$(HIDE)chmod +x $(BUILD_DIR)/Rebuild.sh
	$(HIDE)cd $(BUILD_DIR) && ./Rebuild.sh > $(BUILD_LOG) 2>&1
else
	$(HIDE)mkdir -p $(BUILD_DIR)
	$(HIDE)$(CP) $(LITEOSTHIRDPARTY)/$(MKSH)/. $(BUILD_DIR)
	$(HIDE)chmod +x $(BUILD_DIR)/Build.sh
	$(HIDE)cd $(BUILD_DIR) && CC=$(CC) TARGET_OS=$(TARGET_OS) CFLAGS="$(LOCAL_CFLAGS)" LDFLAGS="$(LOCAL_LDFLAGS)" ./Build.sh -r > $(BUILD_LOG) 2>&1
endif
	$(HIDE)$(CP) -rf $(BUILD_DIR)/$(MKSH) .
	$(HIDE)$(STRIP) $(MKSH)
	$(HIDE)mkdir -p $(APPS_OUT)
	$(HIDE)$(CP) $(MKSH) $(APPS_OUT)
	$(HIDE)mkdir -p $(ETC_OUT)
	$(HIDE)$(CP) -rf $(BUILD_DIR)/.mkshrc $(ETC_OUT)/
	$(HIDE)$(RM) $(BUILD_DIR) $(BUILD_LOG)

clean:
	$(HIDE)$(RM) $(MKSH) $(BUILD_DIR) $(BUILD_LOG)


.PHONY: all $(MKSH) clean
