# Copyright (c) 2023-2023 Huawei Device Co., Ltd. All rights reserved.
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

LOCAL_SRCS := $(wildcard *.c)

ifneq ($(LOSCFG_IPC_CONTAINER), y)
LOCAL_SRCS += $(filter-out los_ipc_container.c, $(LOCAL_SRCS))
endif

ifneq ($(LOSCFG_MNT_CONTAINER), y)
LOCAL_SRCS += $(filter-out los_mnt_container.c, $(LOCAL_SRCS))
endif

ifneq ($(LOSCFG_NET_CONTAINER), y)
LOCAL_SRCS += $(filter-out los_net_container.c, $(LOCAL_SRCS))
endif

ifneq ($(LOSCFG_PID_CONTAINER), y)
LOCAL_SRCS += $(filter-out los_pid_container.c, $(LOCAL_SRCS))
endif

ifneq ($(LOSCFG_TIME_CONTAINER), y)
LOCAL_SRCS += $(filter-out los_time_container.c, $(LOCAL_SRCS))
endif

ifneq ($(LOSCFG_USER_CONTAINER), y)
LOCAL_SRCS += $(filter-out los_credentials.c los_user_container.c, $(LOCAL_SRCS))
endif

ifneq ($(LOSCFG_UTS_CONTAINER), y)
LOCAL_SRCS += $(filter-out los_uts_container.c, $(LOCAL_SRCS))
endif

include $(MODULE)