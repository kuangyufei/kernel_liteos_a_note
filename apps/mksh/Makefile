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

include $(APPSTOPDIR)/config.mk

BUILD_DIR := $(OUT)/mksh_build
BUILD_LOG := $(BUILD_DIR)/build.log

CFLAGS += -DMKSH_DISABLE_TTY_WARNING -DMKSH_SMALL=1 -DMKSH_ASSUME_UTF8=1 -DMKSH_SMALL_BUT_FAST=0 -DMKSH_S_NOVI=1 -DHAVE_CAN_FSTACKPROTECTORSTRONG=1
CFLAGS += -DMKSH_LESS_CMDLINE_EDITING -DMKSH_LESS_BUILDINS -DMKSH_NO_INITCOMS -DADAPT_FOR_LITEOS_A
CFLAGS += -Wno-error
LDFLAGS += -Wl,--gc-sections

all:
ifneq ($(wildcard $(BUILD_DIR)/Rebuild.sh),)
	$(HIDE)echo "not clean, rebuilding now"
	$(HIDE)cd $(BUILD_DIR) && sh ./Rebuild.sh > $(BUILD_LOG) 2>&1
else
	$(HIDE)mkdir -p $(BUILD_DIR)
	$(HIDE)$(CP) $(LITEOSTHIRDPARTY)/mksh/. $(BUILD_DIR)
	$(HIDE)cd $(BUILD_DIR) && CC="$(CC)" TARGET_OS=OpenHarmony CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" sh ./Build.sh -r > $(BUILD_LOG) 2>&1
endif
	$(HIDE)mkdir -p $(OUT)/bin/ $(OUT)/etc/
	$(HIDE)$(STRIP) $(BUILD_DIR)/mksh -o $(OUT)/bin/mksh
	$(HIDE)$(CP) $(BUILD_DIR)/.mkshrc $(OUT)/etc/

clean:
	$(HIDE)$(RM) $(BUILD_DIR)

.PHONY: all clean
