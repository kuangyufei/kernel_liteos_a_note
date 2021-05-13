
include $(LITEOSTESTTOPDIR)/config.mk

MODULE_NAME := coretest

LOCAL_INCLUDE := \
    -I $(LITEOSTESTTOPDIR)/kernel/include \
    -I $(LITEOSTESTTOPDIR)/kernel/sample/kernel_base/core/task \
    -I $(LITEOSTESTTOPDIR)/kernel/sample/kernel_base/core/swtmr \
    -I $(LITEOSTESTTOPDIR)/kernel/sample/kernel_base/core/hwi \
    -I $(LITEOSTESTTOPDIR)/kernel/sample/kernel_base/core/hwi_nesting

SRC_MODULES := task swtmr hwi hwi_nesting

ifeq ($(LOSCFG_KERNEL_SMP), y)
SMP_MODULES := task/smp swtmr/smp hwi/smp task/float
endif

ifeq ($(LOSCFG_TEST_LLT), y)
LLT_MODULES := task/llt swtmr/llt
endif

ifeq ($(LOSCFG_TEST_PRESSURE), y)
PRESSURE_MODULES := task/pressure swtmr/pressure
endif

ifeq ($(LOSCFG_TEST_SMOKE), y)
SMOKE_MODULES := task/smoke swtmr/smoke
endif

ifeq ($(LOSCFG_TEST_FULL), y)
FULL_MODULES := task/full swtmr/full hwi_nesting/full
endif

ifeq ($(LOSCFG_TEST_MANUAL_SHELL), y)
MANUAL_MODULES :=task/manual
endif

LOCAL_MODULES := $(SRC_MODULES) $(LLT_MODULES) $(PRESSURE_MODULES) $(SMOKE_MODULES) $(FULL_MODULES) $(SMP_MODULES) $(MANUAL_MODULES)

LOCAL_SRCS := $(foreach dir,$(LOCAL_MODULES),$(wildcard $(dir)/*.c))
LOCAL_CHS := $(foreach dir,$(LOCAL_MODULES),$(wildcard $(dir)/*.h))

LOCAL_FLAGS :=  $(LOCAL_INCLUDE)  -Wno-error

include $(MODULE)
