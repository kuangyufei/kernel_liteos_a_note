
SRC_MODULES := fs/proc

ifeq ($(LOSCFG_USER_TEST_LLT), y)
LLT_MODULES := fs/proc/llt
endif

ifeq ($(LOSCFG_USER_TEST_SMOKE), y)
SMOKE_MODULES := fs/proc/smoke
endif

ifeq ($(LOSCFG_USER_TEST_FULL), y)
FULL_MODULES := fs/proc/full
endif

ifeq ($(LOSCFG_USER_TEST_PRESSURE), y)
PRESSURE_MODULES := fs/proc/pressure
endif

LOCAL_MODULES := $(SRC_MODULES) $(LLT_MODULES) $(PRESSURE_MODULES) $(SMOKE_MODULES) $(FULL_MODULES)

LOCAL_SRCS += $(foreach dir,$(LOCAL_MODULES),$(wildcard $(dir)/*.c))
LOCAL_CHS += $(foreach dir,$(LOCAL_MODULES),$(wildcard $(dir)/*.h))

LOCAL_FLAGS += -I./fs/proc
