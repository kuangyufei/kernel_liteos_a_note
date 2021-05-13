
include $(LITEOSTESTTOPDIR)/config.mk

MODULE_NAME := ipctest

LOCAL_INCLUDE := \
    -I $(LITEOSTESTTOPDIR)/kernel/include \
    -I $(LITEOSTESTTOPDIR)/kernel/sample/kernel_base/ipc/mux \
    -I $(LITEOSTESTTOPDIR)/kernel/sample/kernel_base/ipc/sem \
    -I $(LITEOSTESTTOPDIR)/kernel/sample/kernel_base/ipc/event \
    -I $(LITEOSTESTTOPDIR)/kernel/sample/kernel_base/ipc/queue \
    -I $(LITEOSTESTTOPDIR)/kernel/sample/kernel_base/ipc/rwlock

SRC_MODULES := sem event queue mux rwlock

ifeq ($(LOSCFG_KERNEL_SMP), y)
SMP_MODULES := sem/smp event/smp queue/smp mux/smp rwlock/smp
endif

ifeq ($(LOSCFG_TEST_LLT), y)
LLT_MODULES := sem/llt event/llt queue/llt mux/llt
endif

ifeq ($(LOSCFG_TEST_PRESSURE), y)
PRESSURE_MODULES := sem/pressure event/pressure queue/pressure mux/pressure
endif

ifeq ($(LOSCFG_TEST_SMOKE), y)
SMOKE_MODULES := sem/smoke event/smoke queue/smoke mux/smoke rwlock/smoke
endif

ifeq ($(LOSCFG_TEST_FULL), y)
FULL_MODULES := sem/full event/full queue/full mux/full
endif

ifeq ($(LOSCFG_TEST_MANUAL_SHELL), y)
MANUAL_MODULES := sem/manual event/manual queue/manual
endif

ifeq ($(LOSCFG_TEST_MANUAL_TEST), y)
MANUAL_MODULES :=sem/manual event/manual queue/manual mux/manual
endif

LOCAL_MODULES := $(SRC_MODULES) $(LLT_MODULES) $(PRESSURE_MODULES) $(SMOKE_MODULES) $(FULL_MODULES) $(SMP_MODULES) $(MANUAL_MODULES)

LOCAL_SRCS := $(foreach dir,$(LOCAL_MODULES),$(wildcard $(dir)/*.c))
LOCAL_CHS := $(foreach dir,$(LOCAL_MODULES),$(wildcard $(dir)/*.h))

LOCAL_FLAGS :=  $(LOCAL_INCLUDE)  -Wno-error

include $(MODULE)
