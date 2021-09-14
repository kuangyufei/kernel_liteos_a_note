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

LOCAL_SRCS := los_trace.c
LOCAL_SRCS += $(wildcard cnv/*.c)

LOCAL_INCLUDE := \
        -I $(LITEOSTOPDIR)/kernel/extended/trace \
        -I $(LITEOSTOPDIR)/kernel/extended/trace/pipeline \
        -I $(LITEOSTOPDIR)/kernel/extended/trace/cnv

ifeq ($(LOSCFG_RECORDER_MODE_OFFLINE), y)
LOCAL_SRCS += trace_offline.c
endif

ifeq ($(LOSCFG_RECORDER_MODE_ONLINE), y)
LOCAL_SRCS += trace_online.c
endif

ifeq ($(LOSCFG_TRACE_CLIENT_INTERACT), y)
LOCAL_SRCS += $(wildcard pipeline/*.c)
endif

ifeq ($(LOSCFG_TRACE_PIPELINE_SERIAL), y)
LOCAL_SRCS += $(wildcard pipeline/serial/*.c)
LOCAL_INCLUDE += -I $(LITEOSTOPDIR)/kernel/extended/trace/pipeline/serial
endif

LOCAL_FLAGS := $(LOCAL_INCLUDE)

include $(MODULE)

