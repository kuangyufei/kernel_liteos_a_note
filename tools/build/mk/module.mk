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

OBJOUT := $(BUILD)$(dir $(subst $(LITEOSTOPDIR),,$(shell pwd)))$(MODULE_NAME)

MODULE := $(OUT)/lib/lib$(MODULE_NAME).a

# create a separate list of objects per source type

LOCAL_CSRCS := $(filter %.c,$(LOCAL_SRCS))
LOCAL_CPPSRCS := $(filter %.cpp,$(LOCAL_SRCS))
LOCAL_ASMSRCS := $(filter %.S,$(LOCAL_SRCS))
LOCAL_ASMSRCS2 := $(filter %.s,$(LOCAL_SRCS))
LOCAL_CCSRCS := $(filter %.cc,$(LOCAL_SRCS))

LOCAL_COBJS := $(patsubst %.c,$(OBJOUT)/%.o,$(LOCAL_CSRCS))
LOCAL_CPPOBJS := $(patsubst %.cpp,$(OBJOUT)/%.o,$(LOCAL_CPPSRCS))
LOCAL_ASMOBJS := $(patsubst %.S,$(OBJOUT)/%.o,$(LOCAL_ASMSRCS))
LOCAL_ASMOBJS2 := $(patsubst %.s,$(OBJOUT)/%.o,$(LOCAL_ASMSRCS2))
LOCAL_CCOBJS := $(patsubst %.cc,$(OBJOUT)/%.o,$(LOCAL_CCSRCS))

LOCAL_OBJS := $(LOCAL_COBJS) $(LOCAL_CPPOBJS) $(LOCAL_ASMOBJS) $(LOCAL_ASMOBJS2) $(LOCAL_CCOBJS)

LOCAL_CGCH := $(patsubst %.h,%.h.gch,$(LOCAL_CHS))
LOCAL_CPPGCH := $(patsubst %.h,%.h.gch,$(LOCAL_CPPHS))

all : $(MODULE)

define ECHO =
ifeq ($$(HIDE),@)
_$(1) := $($(1))
$(1) = echo "  $(1)" $$(patsubst $$(OUT)/%,%,$$@) && $$(_$(1))
endif
endef
$(foreach cmd,CC GPP AS AR,$(eval $(call ECHO,$(cmd))))

LOCAL_FLAGS += -MD -MP
-include $(LOCAL_OBJS:%.o=%.d)

$(LOCAL_COBJS): $(OBJOUT)/%.o: %.c
	$(HIDE)$(OBJ_MKDIR)
	$(HIDE)$(CC) $(LITEOS_CFLAGS) $(LOCAL_FLAGS) $(LOCAL_CFLAGS) -c $< -o $@

$(LOCAL_CPPOBJS): $(OBJOUT)/%.o: %.cpp
	$(HIDE)$(OBJ_MKDIR)
	$(HIDE)$(GPP) $(LITEOS_CXXFLAGS) $(LOCAL_FLAGS) $(LOCAL_CPPFLAGS) -c $< -o $@

$(LOCAL_ASMOBJS): $(OBJOUT)/%.o: %.S
	$(HIDE)$(OBJ_MKDIR)
	$(HIDE)$(CC) $(LITEOS_CFLAGS) $(LOCAL_FLAGS) $(LOCAL_ASFLAGS) -c $< -o $@

$(LOCAL_ASMOBJS2): $(OBJOUT)/%.o: %.s
	$(HIDE)$(OBJ_MKDIR)
	$(HIDE)$(AS) $(LITEOS_ASFLAGS) $(LOCAL_FLAGS) $(LOCAL_ASFLAGS) -c $< -o $@

$(LOCAL_CCOBJS): $(OBJOUT)/%.o: %.cc
	$(HIDE)$(OBJ_MKDIR)
	$(HIDE)$(GPP) $(LITEOS_CXXFLAGS) $(LOCAL_FLAGS) $(LOCAL_CPPFLAGS) -c $< -o $@

$(LOCAL_CGCH): %.h.gch : %.h
	$(HIDE)$(CC) $(LITEOS_CFLAGS) $(LOCAL_FLAGS) $(LOCAL_CFLAGS) $> $^

$(LOCAL_CPPGCH): %.h.gch : %.h
	$(HIDE)$(GPP) $(LITEOS_CXXFLAGS) $(LOCAL_FLAGS) $(LOCAL_CPPFLAGS) -x c++-header $> $^

LOCAL_GCH := $(LOCAL_CGCH) $(LOCAL_CPPGCH)

$(LOCAL_OBJS): $(LOCAL_GCH)
$(MODULE): $(LOCAL_OBJS)
	$(HIDE)$(OBJ_MKDIR)
	$(HIDE)$(AR) $(ARFLAGS) $@ $^

clean:
	$(HIDE)$(RM) $(MODULE) $(OBJOUT) $(LOCAL_GCH) *.bak *~

.PHONY: all clean

# clear some variables we set here
LOCAL_CSRCS :=
LOCAL_CPPSRCS :=
LOCAL_ASMSRCS :=
LOCAL_COBJS :=
LOCAL_CPPOBJS :=
LOCAL_ASMOBJS :=
LOCAL_ASMOBJS2 :=

# LOCAL_OBJS is passed back
#LOCAL_OBJS :=
