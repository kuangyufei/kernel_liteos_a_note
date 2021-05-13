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


-include $(LITEOSTOPDIR)/tools/build/config.mk
-include $(LITEOSTESTTOPDIR)/build/los_test_config.mk
LITEOS_CFLAGS += -I $(LITEOSTOPDIR)/lib/libc/musl/include \
    -I $(LITEOSTOPDIR)/lib/libc/musl/obj/include \
    -I $(LITEOSTOPDIR)/lib/libc/musl/arch/arm \
    -I $(LITEOSTOPDIR)/lib/libc/musl/arch/generic \
    -I $(LITEOSTHIRDPARTY)/bounds_checking_function/include \
    -I $(LITEOSTOPDIR)/security/cap/ \
    -I $(LITEOSTOPDIR)/security/vid/ \
    -I $(LITEOSTOPDIR)/platform/include \
    -I $(LITEOSTOPDIR)/kernel/base/include\
    -I $(LITEOSTOPDIR)/kernel/include \
    -I $(LITEOSTOPDIR)/kernel/extended/include \
    -I $(LITEOSTOPDIR)/fs/vfs \
    -I $(LITEOSTHIRDPARTY)/FatFs/source  \
    -I $(LITEOSTOPDIR)/fs/proc/include \
    -I $(LITEOSTOPDIR)/fs/jffs2/os_adapt \
    -I $(LITEOSTHIRDPARTY)/NuttX/fs/nfs/ \
    -I $(LITEOSTOPDIR)/bsd/compat/linuxkpi/include
