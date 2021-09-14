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

# common flags config
BASE_OPTS := -D_FORTIFY_SOURCE=2 -D_GNU_SOURCE

ASFLAGS   :=
CFLAGS    := $(LITEOS_COPTS) $(BASE_OPTS) -fPIE
CXXFLAGS  := $(LITEOS_CXXOPTS) $(BASE_OPTS) -fPIE
LDFLAGS   := $(LITEOS_CORE_COPTS) -pie -Wl,-z,relro,-z,now -O2

CFLAGS    := $(filter-out -fno-pic -fno-builtin -nostdinc -nostdlib,$(CFLAGS))
CXXFLAGS  := $(filter-out -fno-pic -fno-builtin -nostdinc -nostdlib -nostdinc++,$(CXXFLAGS))

# alias variable config
HIDE := @
MAKE := make
RM := rm -rf
CP := cp -rf
MV := mv -f

APP := $(APPSTOPDIR)/app.mk

##build modules config##
APP_SUBDIRS :=

ifeq ($(LOSCFG_SHELL), y)
APP_SUBDIRS += shell
APP_SUBDIRS += mksh
APP_SUBDIRS += toybox
endif

ifeq ($(LOSCFG_USER_INIT_DEBUG), y)
APP_SUBDIRS += init
endif

ifeq ($(LOSCFG_NET_LWIP_SACK_TFTP), y)
APP_SUBDIRS += tftp
endif

ifeq ($(LOSCFG_DRIVERS_TRACE), y)
APP_SUBDIRS += trace
endif
