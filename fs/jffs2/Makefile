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

include $(LITEOSTOPDIR)/config.mk

MODULE_NAME := $(notdir $(shell pwd))

LINUX_PATH := $(LITEOSTOPDIR)/../linux/linux-5.10
LOCAL_PATH := $(shell pwd)
TEMP_SRC_PATH := $(OUT)/jffs2_build

LOCAL_SRCS := \
		$(TEMP_SRC_PATH)/fs/jffs2/background.c \
		$(TEMP_SRC_PATH)/fs/jffs2/build.c \
		$(TEMP_SRC_PATH)/fs/jffs2/compr.c \
		$(TEMP_SRC_PATH)/fs/jffs2/compr_rtime.c \
		$(TEMP_SRC_PATH)/fs/jffs2/compr_rubin.c \
		$(TEMP_SRC_PATH)/fs/jffs2/compr_zlib.c \
		$(TEMP_SRC_PATH)/fs/jffs2/debug.c \
		$(TEMP_SRC_PATH)/fs/jffs2/dir.c \
		$(TEMP_SRC_PATH)/fs/jffs2/erase.c \
		$(TEMP_SRC_PATH)/fs/jffs2/file.c \
		$(TEMP_SRC_PATH)/fs/jffs2/fs.c \
		$(TEMP_SRC_PATH)/fs/jffs2/gc.c \
		$(TEMP_SRC_PATH)/fs/jffs2/malloc.c \
		$(TEMP_SRC_PATH)/fs/jffs2/nodelist.c \
		$(TEMP_SRC_PATH)/fs/jffs2/nodemgmt.c \
		$(TEMP_SRC_PATH)/fs/jffs2/read.c \
		$(TEMP_SRC_PATH)/fs/jffs2/readinode.c \
		$(TEMP_SRC_PATH)/fs/jffs2/scan.c \
		$(TEMP_SRC_PATH)/fs/jffs2/summary.c \
		$(TEMP_SRC_PATH)/fs/jffs2/super.c \
		$(TEMP_SRC_PATH)/fs/jffs2/write.c \
		$(TEMP_SRC_PATH)/fs/jffs2/writev.c \
		$(LOCAL_PATH)/src/jffs2_hash.c \
		$(LOCAL_PATH)/src/vfs_jffs2.c \

LOCAL_INCLUDE := \
		-I $(LITEOSTOPDIR)/fs/jffs2/include \
		-I $(TEMP_SRC_PATH)/fs/jffs2 \
		-I $(TEMP_SRC_PATH)/fs
LOCAL_FLAGS := $(LOCAL_INCLUDE)

.PHONY: patch patch_clean
$(LOCAL_SRCS): patch

patch: patch_clean
	cp $(LINUX_PATH)/fs/jffs2 $(TEMP_SRC_PATH)/fs/. -r
	cd $(TEMP_SRC_PATH) && patch -p1 < $(LOCAL_PATH)/jffs2.patch

patch_clean:
	$(HIDE) $(RM) -rf $(TEMP_SRC_PATH)

include $(MODULE)

