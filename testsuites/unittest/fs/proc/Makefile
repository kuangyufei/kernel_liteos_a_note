
include $(LITEOSTOPDIR)/config.mk

MODULE_NAME := $(notdir $(shell pwd))test

LOCAL_INCLUDE := \
    -I $(LITEOSTOPDIR)/test/kernel/include \
    -I $(LITEOSTOPDIR)/fs/proc/include \
    -I $(LITEOSTOPDIR)/fs/include \
    -I $(LITEOSTOPDIR)/test/kernel/sample/fs/proc

SRC_MODULES := .

ifeq ($(LOSCFG_USER_TEST_LLT), y)
LLT_MODULES := llt
endif

ifeq ($(LOSCFG_USER_TEST_PRESSURE), y)
PRESSURE_MODULES := pressure
endif

ifeq ($(LOSCFG_USER_TEST_SMOKE), y)
SMOKE_MODULES := smoke
endif

ifeq ($(LOSCFG_USER_TEST_FULL), y)
FULL_MODULES := full
endif

LOCAL_MODULES := $(SRC_MODULES) $(LLT_MODULES) $(PRESSURE_MODULES) $(SMOKE_MODULES) $(FULL_MODULES)
LOCAL_SRCS := $(foreach dir,$(LOCAL_MODULES),$(wildcard $(dir)/*.c))
LOCAL_CHS := $(foreach dir,$(LOCAL_MODULES),$(wildcard $(dir)/*.h))

LOCAL_FLAGS :=  $(LOCAL_INCLUDE)  -Wno-error

include $(MODULE)
