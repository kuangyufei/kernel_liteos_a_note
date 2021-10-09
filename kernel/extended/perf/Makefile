include $(LITEOSTOPDIR)/config.mk

MODULE_NAME := $(notdir $(shell pwd))

LOCAL_SRCS := $(wildcard *.c)

ifeq ($(LOSCFG_PERF_HW_PMU), y)
LOCAL_SRCS += $(wildcard pmu/perf_hw_pmu.c)
endif

ifeq ($(LOSCFG_PERF_TIMED_PMU), y)
LOCAL_SRCS += $(wildcard pmu/perf_timed_pmu.c)
endif

ifeq ($(LOSCFG_PERF_SW_PMU), y)
LOCAL_SRCS += $(wildcard pmu/perf_sw_pmu.c)
endif

LOCAL_FLAGS := $(LOCAL_INCLUDE)

include $(MODULE)

