include $(LITEOSTOPDIR)/config.mk

MODULE_NAME := c

ARCH = arm
TOPDIR = $(LITEOSTOPDIR)/../..
MUSLDIR = $(TOPDIR)/third_party/musl
MUSLPORTINGDIR = $(MUSLDIR)/porting/liteos_a/kernel
OPTRTDIR = $(TOPDIR)/third_party/optimized-routines

MUSL_SRCS =

MUSLPORTING_SRCS = \
	src/*/$(ARCH)/*.[csS] \
	src/*/*.c

LOCAL_SRCS = $(wildcard $(addprefix $(MUSLPORTINGDIR)/,$(MUSLPORTING_SRCS)))
LOCAL_SRCS := $(filter-out $(subst $(MUSLPORTINGDIR),$(MUSLDIR),$(LOCAL_SRCS)),$(addprefix $(MUSLDIR)/,$(MUSL_SRCS)) $(LOCAL_SRCS))

ifeq ($(LOSCFG_ARCH_ARM_VER), "armv7-a")
LOCAL_SRCS := $(filter-out $(addprefix $(MUSLPORTINGDIR)/src/string/,memchr.c memcpy.c strcmp.c strcpy.c strlen.c), $(LOCAL_SRCS))
LOCAL_SRCS += \
	$(OPTRTDIR)/string/arm/memchr.S \
	$(OPTRTDIR)/string/arm/memcpy.S \
	$(OPTRTDIR)/string/arm/strcmp.S \
	$(OPTRTDIR)/string/arm/strcpy.c \
	$(OPTRTDIR)/string/arm/strlen-armv6t2.S
endif

LOCAL_SRCS += $(wildcard src/*.c src/*/*.c)

LOCAL_INCLUDE := \
	-I $(LITEOSTOPDIR)/syscall \
	-I $(LITEOSTOPDIR)/bsd/dev/random \
	-I $(LITEOSTOPDIR)/bsd/compat/linuxkpi/include

LOCAL_INCLUDE += $(addprefix -I$(MUSLPORTINGDIR)/, src/include src/internal)

LOCAL_FLAGS := $(LOCAL_INCLUDE) $(LITEOS_GCOV_OPTS)
ifeq ($(LOSCFG_COMPILER_CLANG_LLVM), y)
LOCAL_FLAGS +=-Wno-char-subscripts -Wno-unknown-pragmas
else
LOCAL_FLAGS += -frounding-math -Wno-unused-but-set-variable -Wno-unknown-pragmas
endif

include $(MODULE)
