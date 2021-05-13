
include $(LITEOSTESTTOPDIR)/config.mk

MODULE_NAME := $(notdir $(shell pwd))test

LOCAL_INCLUDE := \
    -I $(LITEOSTOPDIR)/compat/posix/src \
    -I $(LITEOSTESTTOPDIR)/kernel/include \
    -I $(LITEOSTESTTOPDIR)/kernel/sample/posix/mutex \
    -I $(LITEOSTESTTOPDIR)/kernel/sample/posix/pthread

SRC_MODULES := mqueue mutex sem pthread swtmr sched mem

ifeq ($(LOSCFG_TEST_LLT), y)
LLT_MODULES := mqueue/llt mutex/llt sem/llt pthread/llt swtmr/llt sched/llt mem/llt
endif

ifeq ($(LOSCFG_TEST_SMOKE), y)
SMOKE_MODULES := mqueue/smoke mutex/smoke sem/smoke pthread/smoke swtmr/smoke sched/smoke mem/smoke
endif

ifeq ($(LOSCFG_TEST_FULL), y)
FULL_MODULES := mqueue/full mutex/full sem/full pthread/full swtmr/full sched/full mem/full
endif

ifeq ($(LOSCFG_TEST_PRESSURE), y)
PRESSURE_MODULES := mqueue/pressure mutex/pressure sem/pressure pthread/pressure swtmr/pressure sched/pressure mem/pressure
endif

LOCAL_MODULES := $(SRC_MODULES) $(LLT_MODULES) $(PRESSURE_MODULES) $(SMOKE_MODULES) $(FULL_MODULES)

LOCAL_SRCS := $(foreach dir,$(LOCAL_MODULES),$(wildcard $(dir)/*.c))
LOCAL_CHS := $(foreach dir,$(LOCAL_MODULES),$(wildcard $(dir)/*.h))

LOCAL_FLAGS :=  $(LOCAL_INCLUDE)  -Wno-error

include $(MODULE)
