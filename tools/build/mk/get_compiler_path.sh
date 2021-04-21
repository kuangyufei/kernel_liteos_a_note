#!/bin/bash
#
# Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
# Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
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

set -e
declare CROSS_COMPILER="$1"
declare HMOS_TOP_DIR="$2"
declare gcc_path=${HMOS_TOP_DIR}/../../prebuilts/gcc/linux-x86/arm/arm-linux-ohoseabi-gcc
declare windows_gcc_path=${HMOS_TOP_DIR}/../../prebuilts/gcc/win-x86/arm/arm-linux-ohoseabi-gcc
function get_compiler_path()
{
    local system=$(uname -s)
    local user_gcc="${CROSS_COMPILER}"gcc
    local gcc_install_path=$(which "${user_gcc}")

    if [ "$system" != "Linux" ] ; then
        if [ -e "${windows_gcc_path}" ] ; then
            gcc_install_path=$windows_gcc_path
        else
            gcc_install_path=$(dirname $gcc_install_path)/../
        fi
    else
        if [ -e "${gcc_path}" ] ; then
            gcc_install_path=$gcc_path
        else
            gcc_install_path=$(dirname $gcc_install_path)/../
        fi
    fi

    echo "$gcc_install_path"
}
get_compiler_path
