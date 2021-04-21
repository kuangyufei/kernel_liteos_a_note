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

############################# SRCs #################################
HWI_SRC     :=
MMU_SRC     :=
NET_SRC     :=
TIMER_SRC   :=
HRTIMER_SRC :=
USB_SRC     :=

############################# HI3516DV300 Options#################################

########################## HI3518EV300 Options##############################

########################## Qemu ARM Virt Options##############################

LITEOS_BASELIB       += -lbsp

LITEOS_PLATFORM      := $(subst $\",,$(LOSCFG_PLATFORM))

PLATFORM_BSP_BASE := $(LITEOSTOPDIR)/platform

PLATFORM_INCLUDE := -I $(LITEOSTOPDIR)/../../$(LOSCFG_BOARD_CONFIG_PATH) \
                    -I $(LITEOSTOPDIR)/../../$(LOSCFG_BOARD_CONFIG_PATH)/include \
                    -I $(PLATFORM_BSP_BASE)/../kernel/common \
                    -I $(PLATFORM_BSP_BASE)/../../../drivers/liteos/platform/pm \
                    -I $(PLATFORM_BSP_BASE)/hw/include \
                    -I $(PLATFORM_BSP_BASE)/include

#
#-include $(LITEOSTOPDIR)/platform/bsp/board/$(LITEOS_PLATFORM)/board.mk
#

LIB_SUBDIRS             += $(PLATFORM_BSP_BASE)
LITEOS_PLATFORM_INCLUDE += $(PLATFORM_INCLUDE)
LITEOS_CXXINCLUDE       += $(PLATFORM_INCLUDE)
