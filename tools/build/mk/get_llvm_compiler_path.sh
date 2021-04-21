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
declare TEMP="$1"
declare TEMP2="$2"
declare llvm_path_linux=${TEMP2}/../../prebuilts/clang/ohos/linux-x86_64/llvm
declare llvm_path_windows=${TEMP2}/../../prebuilts/clang/ohos/windows-x86_64/llvm
function get_compiler_path()
{
    local system=$(uname -s)
    local user_clang=clang
    local clang_install_path=$(which "${user_clang}")
    if [ "$system" == "Linux" ] ; then
        if [ -e "${llvm_path_linux}" ] ; then
            echo "${llvm_path_linux}"
        elif [ -n "${clang_install_path}" ] ; then
            clang_install_path=$(dirname ${clang_install_path})/../
            echo "${clang_install_path}"
        else
            echo "WARNING:Set llvm/bin path in PATH."
        fi
    else
        if [ -e "${llvm_path_windows}" ] ; then
            echo "${llvm_path_windows}"
        elif [ -n "${clang_install_path}" ] ; then
            clang_install_path=$(dirname ${clang_install_path})/../
            echo "${clang_install_path}"
        else
            echo "WARNING:Set llvm/bin path in PATH."
        fi
    fi
}
get_compiler_path
