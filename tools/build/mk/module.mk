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

OBJOUT = $(OUT)/obj
TOPDIR = $(abspath $(LITEOSTOPDIR)/../..)

TARGET = $(OUT)/lib/lib$(MODULE_NAME).a

LOCAL_OBJS = $(addprefix $(OBJOUT)/,$(addsuffix .o,$(basename $(subst $(TOPDIR)/,,$(abspath $(LOCAL_SRCS))))))

all : $(TARGET)

ifeq ($(HIDE),@)
ECHO = $1 = echo "  $1" $$(patsubst $$(OUT)/%,%,$$@) && $($1)
$(foreach cmd,CC GPP AS AR LD,$(eval $(call ECHO,$(cmd))))
endif

LOCAL_FLAGS += -MD -MP
-include $(LOCAL_OBJS:%.o=%.d)

$(OBJOUT)/%.o: $(TOPDIR)/%.c
	$(HIDE)$(CC) $(CFLAGS) $(LOCAL_FLAGS) $(LOCAL_CFLAGS) -c $< -o $@

$(OBJOUT)/%.o: $(TOPDIR)/%.cpp
	$(HIDE)$(GPP) $(CXXFLAGS) $(LOCAL_FLAGS) $(LOCAL_CPPFLAGS) -c $< -o $@

$(OBJOUT)/%.o: $(TOPDIR)/%.S
	$(HIDE)$(CC) $(CFLAGS) $(LOCAL_FLAGS) $(LOCAL_ASFLAGS) -c $< -o $@

$(OBJOUT)/%.o: $(TOPDIR)/%.s
	$(HIDE)$(AS) $(ASFLAGS) $(LOCAL_FLAGS) $(LOCAL_ASFLAGS) -c $< -o $@

$(OBJOUT)/%.o: $(TOPDIR)/%.cc
	$(HIDE)$(GPP) $(CXXFLAGS) $(LOCAL_FLAGS) $(LOCAL_CPPFLAGS) -c $< -o $@

$(OUT)/lib/lib%.a: $(LOCAL_OBJS)
	$(HIDE)$(AR) $(ARFLAGS) $@ $^

$(OUT)/bin/%: $(LOCAL_OBJS)
	$(HIDE)$(GPP) $(LDFLAGS) -o $@ $^

$(OUT)/%/:;$(HIDE)mkdir -p $@

$(LOCAL_OBJS): | $(dir $(LOCAL_OBJS))
$(TARGET): | $(dir $(TARGET))

clean:
	$(HIDE)$(RM) $(TARGET) $(LOCAL_OBJS)

.PHONY: all clean
