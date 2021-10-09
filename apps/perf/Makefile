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

APP_NAME := $(notdir $(shell pwd))

SECUREC_DIR := $(LITEOSTHIRDPARTY)/bounds_checking_function

LOCAL_SRCS = $(wildcard src/*.c)
LOCAL_SRCS += $(wildcard $(SECUREC_DIR)/src/*.c)

LOCAL_INCLUDE := \
    -I include \
    -I $(SECUREC_DIR)/include

LOCAL_FLAGS += $(LOCAL_INCLUDE)

ifeq ($(LOSCFG_PERF_HW_PMU), y)
CFLAGS += -DLOSCFG_PERF_HW_PMU
endif

ifeq ($(LOSCFG_PERF_TIMED_PMU), y)
CFLAGS += -DLOSCFG_PERF_TIMED_PMU
endif

ifeq ($(LOSCFG_PERF_SW_PMU), y)
CFLAGS += -DLOSCFG_PERF_SW_PMU
endif

ifeq ($(LOSCFG_FS_VFS), y)
CFLAGS += -DLOSCFG_FS_VFS
endif

CFLAGS += -DLOSCFG_PERF_BUFFER_SIZE=$(LOSCFG_PERF_BUFFER_SIZE)
include $(APP)
