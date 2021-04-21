include $(LITEOSTOPDIR)/config.mk

MODULE_NAME := c

LOCAL_MODULES = \
	musl/src/ctype \
	musl/src/env \
	musl/src/errno \
	musl/src/exit \
	musl/src/internal \
	musl/src/linux \
	musl/src/locale \
	musl/src/malloc \
	musl/src/multibyte \
	musl/src/network \
	musl/src/prng \
	musl/src/sched \
	musl/src/stdio \
	musl/src/stdlib \
	musl/src/string \
	musl/src/math \
	musl/src/time

LOCAL_SRCS = $(wildcard musl/src/ctype/*.c) \
	$(wildcard musl/src/env/*.c) \
	$(wildcard musl/src/errno/*.c) \
	$(wildcard musl/src/exit/*.c) \
	$(wildcard musl/src/linux/*.c) \
	$(wildcard musl/src/locale/*.c) \
	$(wildcard musl/src/malloc/*.c) \
	$(wildcard musl/src/multibyte/*.c) \
	$(wildcard musl/src/network/*.c) \
	$(wildcard musl/src/prng/*.c) \
	$(wildcard musl/src/sched/*.c) \
	$(wildcard musl/src/stdlib/*.c)

ifeq ($(LOSCFG_ARCH_ARM_VER), "armv7-a")
LOCAL_SRCS += \
	optimization/arm/memchr.S \
	optimization/arm/memcpy.S \
	optimization/arm/strcmp.S \
	optimization/arm/strcpy.c \
	optimization/arm/strlen.S
endif

LOCAL_SRCS_MATH = $(wildcard musl/src/math/*.c)
LOCAL_SRCS_INTERNAL = $(wildcard musl/src/internal/*.c)
LOCAL_SRCS_STDIO = $(wildcard musl/src/stdio/*.c)
LOCAL_SRCS_STRING = $(wildcard musl/src/string/*.c)
LOCAL_SRCS_TIME = $(wildcard musl/src/time/*.c)

ifeq ($(LOSCFG_ARCH_ARM_VER), "armv7-a")
LOCAL_FILTER_SRCS_STRING = memchr.c memcpy.c strcmp.c strcpy.c strlen.c
LOCAL_SRCS_STRING := $(filter-out $(LOCAL_FILTER_SRCS_STRING),$(wildcard musl/src/string/*.c))
endif

LOCAL_INCLUDE := \
	-I $(LITEOSTOPDIR)/lib/libc/musl/include \
	-I $(LITEOSTOPDIR)/lib/libc/musl/arch/arm \
	-I $(LITEOSTOPDIR)/lib/libc/musl/arch/generic \
	-I $(LITEOSTOPDIR)/lib/libc/musl/obj/include \
	-I $(LITEOSTOPDIR)/lib/libc/musl/src/include \
	-I $(LITEOSTOPDIR)/lib/libc/musl/src/dirent \
	-I $(LITEOSTOPDIR)/lib/libc/musl/src/errno \
	-I $(LITEOSTOPDIR)/lib/libc/musl/src/internal \
	-I $(LITEOSTOPDIR)/lib/libc/musl/src/multibyte \
	-I $(LITEOSTOPDIR)/lib/libc/musl/src/time \
	-I $(LITEOSTOPDIR)/syscall \
	-I $(LITEOSTOPDIR)/bsd/dev/random \
	-I $(LITEOSTOPDIR)/bsd/compat/linuxkpi/include

ifeq ($(LOSCFG_LLTSER), y)
LOCAL_INCLUDE += -I $(LITEOSTOPDIR)/tools/gcov_ser
endif

LOCAL_FLAGS := $(LOCAL_INCLUDE)
ifeq ($(LOSCFG_COMPILER_CLANG_LLVM), y)
LOCAL_FLAGS +=-Wno-char-subscripts -Wno-unknown-pragmas
else
LOCAL_FLAGS += -frounding-math -Wno-unused-but-set-variable -Wno-unknown-pragmas
endif

ifeq ($(OS), Linux)
OBJOUT := $(BUILD)$(dir $(subst $(LITEOSTOPDIR),,$(shell pwd)))$(MODULE_NAME)
LOCAL_PWD := $(shell pwd)
else
TEMPLITEOSTOPDIR:=$(shell cygpath -u $(LITEOSTOPDIR))
OBJOUT := $(BUILD)$(dir $(subst $(TEMPLITEOSTOPDIR),,$(shell pwd)))$(MODULE_NAME)
LOCAL_PWD := $(shell cygpath -m $(shell pwd))
endif

ifeq ($(LOCAL_SO), y)
LIBSO := $(OUT)/lib/lib$(MODULE_NAME).so
LIBA := $(OUT)/lib/lib$(MODULE_NAME).a
else
LIBSO :=
LIBA := $(OUT)/lib/lib$(MODULE_NAME).a
endif
LIB := $(LIBA) $(LIBSO)

# create a separate list of objects per source type

LOCAL_CSRCS := $(filter %.c,$(LOCAL_SRCS))
LOCAL_CSRCS_MATH := $(filter %.c,$(LOCAL_SRCS_MATH))
LOCAL_CSRCS_INTERNAL := $(filter %.c,$(LOCAL_SRCS_INTERNAL))
LOCAL_CSRCS_STDIO := $(filter %.c,$(LOCAL_SRCS_STDIO))
LOCAL_CSRCS_STRING := $(filter %.c,$(LOCAL_SRCS_STRING))
LOCAL_CSRCS_TIME := $(filter %.c,$(LOCAL_SRCS_TIME))

LOCAL_ASMSRCS := $(filter %.S,$(LOCAL_SRCS))

LOCAL_COBJS := $(patsubst %.c,$(OBJOUT)/%.o,$(LOCAL_CSRCS))
LOCAL_COBJS_MATH := $(patsubst %.c,$(OBJOUT)/%.o,$(LOCAL_CSRCS_MATH))
LOCAL_COBJS_INTERNAL := $(patsubst %.c,$(OBJOUT)/%.o,$(LOCAL_CSRCS_INTERNAL))
LOCAL_COBJS_STDIO := $(patsubst %.c,$(OBJOUT)/%.o,$(LOCAL_CSRCS_STDIO))
LOCAL_COBJS_STRING := $(patsubst %.c,$(OBJOUT)/%.o,$(LOCAL_CSRCS_STRING))
LOCAL_COBJS_TIME := $(patsubst %.c,$(OBJOUT)/%.o,$(LOCAL_CSRCS_TIME))

LOCAL_ASMOBJS := $(patsubst %.S,$(OBJOUT)/%.o,$(LOCAL_ASMSRCS))

LOCAL_OBJS := $(LOCAL_COBJS) $(LOCAL_ASMOBJS) $(LOCAL_COBJS_MATH) $(LOCAL_COBJS_INTERNAL) $(LOCAL_COBJS_STDIO) $(LOCAL_COBJS_STRING) $(LOCAL_COBJS_TIME)
LOCAL_CGCH := $(patsubst %.h,%.h.gch,$(LOCAL_CHS))
LOCAL_CPPGCH := $(patsubst %.h,%.h.gch,$(LOCAL_CPPHS))

all : $(LIB)

$(LOCAL_COBJS): $(OBJOUT)/%.o: %.c sub_math
	$(HIDE)$(OBJ_MKDIR)
	$(HIDE)$(CC) $(LITEOS_CFLAGS) $(LOCAL_FLAGS) -c $< -o $@

$(LOCAL_ASMOBJS): $(OBJOUT)/%.o: %.S
	$(HIDE)$(OBJ_MKDIR)
	$(HIDE)$(CC) $(LITEOS_CFLAGS) $(LOCAL_FLAGS) -c $< -o $@

sub_math:
	$(HIDE)$(MAKE) -C musl/src/math all LOCAL_COBJS_MATH="$(LOCAL_COBJS_MATH)" OBJOUT="$(OBJOUT)" LOCAL_FLAGS="$(LOCAL_FLAGS)"

sub_internal: sub_math
	$(HIDE)$(MAKE) -C musl/src/internal all LOCAL_COBJS_INTERNAL="$(LOCAL_COBJS_INTERNAL)" OBJOUT="$(OBJOUT)" LOCAL_FLAGS="$(LOCAL_FLAGS)"

sub_stdio: sub_math
	$(HIDE)$(MAKE) -C musl/src/stdio all LOCAL_COBJS_STDIO="$(LOCAL_COBJS_STDIO)" OBJOUT="$(OBJOUT)" LOCAL_FLAGS="$(LOCAL_FLAGS)"

sub_string: sub_math
	$(HIDE)$(MAKE) -C musl/src/string all LOCAL_COBJS_STRING="$(LOCAL_COBJS_STRING)" OBJOUT="$(OBJOUT)" LOCAL_FLAGS="$(LOCAL_FLAGS)"

sub_time: sub_math
	$(HIDE)$(MAKE) -C musl/src/time all LOCAL_COBJS_TIME="$(LOCAL_COBJS_TIME)" OBJOUT="$(OBJOUT)" LOCAL_FLAGS="$(LOCAL_FLAGS)"

$(LOCAL_CGCH): %.h.gch : %.h
	$(HIDE)$(CC) $(LITEOS_CFLAGS) $(LOCAL_FLAGS) $> $^

$(LOCAL_CPPGCH): %.h.gch : %.h
	$(HIDE)$(GPP) $(LITEOS_CXXFLAGS) $(LOCAL_FLAGS) $(LOCAL_CPPFLAGS) -x c++-header $> $^

LOCAL_GCH := $(LOCAL_CGCH) $(LOCAL_CPPGCH)

$(LOCAL_OBJS): $(LOCAL_GCH)
$(LIBA): $(LOCAL_OBJS) sub_math sub_internal sub_stdio sub_string sub_time
	$(HIDE)$(OBJ_MKDIR)

ifeq ($(OS), Linux)
	$(HIDE)$(AR) $(ARFLAGS) $@ $(LOCAL_OBJS)
else
ifeq ($(LOCAL_MODULES),)
	$(HIDE)$(AR) $(ARFLAGS) $@ $(LOCAL_OBJS)
else
	$(HIDE)for i in $(LOCAL_MODULES); do \
		pushd $(OBJOUT)/$$i 1>/dev/null; \
		$(AR) $(ARFLAGS) $@ *.o;\
		popd 1>/dev/null;\
	done
endif
endif

ifeq ($(LOCAL_SO), y)
$(LIBSO): $(LOCAL_OBJS)
	$(HIDE)$(CC) $(LITEOS_CFLAGS) -fPIC -shared $^ -o $@
endif

clean:
	$(HIDE)$(RM) $(LIB) $(OBJOUT) $(LOCAL_GCH) *.bak *~

.PHONY: all clean sub_math sub_internal sub_stdio sub_string sub_time

# clear some variables we set here
LOCAL_CSRCS :=
LOCAL_COBJS :=

