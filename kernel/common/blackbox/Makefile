include $(LITEOSTOPDIR)/config.mk

MODULE_NAME := $(notdir $(shell pwd))

LOCAL_SRCS := $(wildcard *.c)

LOCAL_INCLUDE := \
        -I $(LITEOSTOPDIR)/kernel/common \
        -I $(LITEOSTOPDIR)/kernel/common/blackbox \
        -I $(LITEOSTOPDIR)/syscall \
        -I $(LITEOSTOPDIR)/kernel/base/include

LOCAL_FLAGS := $(LOCAL_INCLUDE) $(LITEOS_GCOV_OPTS)

include $(MODULE)
