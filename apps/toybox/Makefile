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

BUILD_DIR := $(OUT)/toybox_build
BUILD_LOG := $(BUILD_DIR)/build.log

CFLAGS += -Wno-error

all:
ifneq ($(wildcard $(BUILD_DIR)),)
	$(HIDE)echo "not clean, rebuilding now"
else
	$(HIDE)mkdir -p $(BUILD_DIR)
	$(HIDE)$(CP) $(LITEOSTHIRDPARTY)/toybox/. $(BUILD_DIR)
	$(HIDE)$(CP) $(LITEOSTHIRDPARTY)/toybox/porting/liteos_a/. $(BUILD_DIR)
	$(HIDE)$(CP) liteos_a_custom.config $(BUILD_DIR)/../
endif
	$(HIDE)unset KCONFIG_CONFIG CROSS_COMPILE && \
	env CC="$(CC)" OUTNAME="$(OUT)/bin/toybox" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" STRIP="$(STRIP)" \
	make -C $(BUILD_DIR) toybox -j> $(BUILD_LOG) 2>&1

clean:
	$(HIDE)$(RM) $(BUILD_DIR)

.PHONY: all clean
