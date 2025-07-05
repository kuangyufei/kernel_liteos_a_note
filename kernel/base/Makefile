# Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
# Copyright (c) 2020-2023 Huawei Device Co., Ltd. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this list of
#    conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this list
#    of conditions and the following disclaimer in the documentation and/or other materials
#    provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors may be used
#    to endorse or promote products derived from this software without specific prior written
#    permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# 包含顶层配置文件，定义编译选项、路径等全局变量
include $(LITEOSTOPDIR)/config.mk
# 包含HDF驱动框架适配层的Makefile，用于内核与驱动的集成
include $(LITEOSTOPDIR)/../../drivers/hdf_core/adapter/khdf/liteos/lite.mk

# 设置模块名称为当前目录名（kernel/base）
MODULE_NAME := $(notdir $(shell pwd))

# 定义本地源文件列表，使用wildcard通配符匹配多个目录下的.c文件
LOCAL_SRCS := 	$(wildcard ipc/*.c)          # IPC（进程间通信）模块源文件
		$(wildcard core/*.c)         # 内核核心模块源文件
		$(wildcard mem/membox/*.c)   # 内存管理-membox分配器源文件
		$(wildcard mem/common/*.c)   # 内存管理-通用功能源文件
		$(wildcard om/*.c)           # 内核对象模型（Object Model）源文件
		$(wildcard misc/*.c)         # 杂项工具模块源文件
		$(wildcard mem/tlsf/*.c)     # 内存管理-TLSF（Two-Level Segregated Fit）分配器源文件
		$(wildcard mp/*.c)           # 多处理器（Multi-Processor）支持源文件
		$(wildcard sched/*.c)        # 调度器模块源文件
		$(wildcard vm/*.c)           # 虚拟内存（Virtual Memory）管理源文件

# 定义本地编译标志
LOCAL_FLAGS := $(LITEOS_CFLAGS_INTERWORK)  # 包含LITEOS交叉编译通用标志
               -Wno-frame-address        # 禁用"frame-address"警告（避免特定编译器版本的警告干扰）

# 包含模块编译规则，由LITEOS构建系统提供，用于生成目标文件和链接
include $(MODULE)
