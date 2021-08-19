include $(LITEOSTOPDIR)/config.mk

MODULE_NAME := c

ARCH = arm
TOPDIR = $(LITEOSTOPDIR)/../..
MUSLDIR = $(TOPDIR)/third_party/musl
MUSLPORTINGDIR = $(MUSLDIR)/porting/liteos_a/kernel
OPTRTDIR = $(TOPDIR)/third_party/optimized-routines

MUSL_SRCS =
LOCAL_OPT_DIR = src/arch/$(ARCH)
LOCAL_OPT_SRCS =
LOCAL_FILTER_SRCS =

MUSLPORTING_SRCS = \
	src/*/$(ARCH)/*.[csS] \
	src/*/*.c

LOCAL_SRCS = $(wildcard $(addprefix $(MUSLPORTINGDIR)/,$(MUSLPORTING_SRCS)))
LOCAL_SRCS := $(filter-out $(subst $(MUSLPORTINGDIR),$(MUSLDIR),$(LOCAL_SRCS)),$(addprefix $(MUSLDIR)/,$(MUSL_SRCS)) $(LOCAL_SRCS))
LOCAL_SRCS += $(wildcard src/*.c src/*.S)
# Sources optimized for specific architectures
LOCAL_OPT_SRCS := $(wildcard $(LOCAL_OPT_DIR)/*.c, $(LOCAL_OPT_DIR)/*.S)
# Enumerate common src files with the same name as the optimized srcs
LOCAL_FILTER_SRCS := $(addprefix $(MUSLPORTINGDIR)/src/string/,$(subst $(LOCAL_OPT_DIR)/,,$(LOCAL_OPT_SRCS)))
LOCAL_FILTER_SRCS += $(addprefix src/,$(subst $(LOCAL_OPT_DIR)/,,$(LOCAL_OPT_SRCS)))
LOCAL_FILTER_SRCS := $(subst .S,.c, $(LOCAL_FILTER_SRCS))

ifeq ($(LOSCFG_ARCH_ARM_VER), "armv7-a")
LOCAL_SRCS := $(filter-out $(addprefix $(MUSLPORTINGDIR)/src/string/,memchr.c memcpy.c strcmp.c strcpy.c strlen.c), $(LOCAL_SRCS))
LOCAL_SRCS += \
	$(OPTRTDIR)/string/arm/memchr.S \
	$(OPTRTDIR)/string/arm/memcpy.S \
	$(OPTRTDIR)/string/arm/strcmp.S \
	$(OPTRTDIR)/string/arm/strcpy.c \
	$(OPTRTDIR)/string/arm/strlen-armv6t2.S
# Replace the general srcs of the same name with specially optimized srcs
LOCAL_SRCS += $(LOCAL_OPT_SRCS)
LOCAL_SRCS := $(filter-out $(LOCAL_FILTER_SRCS),$(LOCAL_SRCS))
endif

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
