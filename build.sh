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

echo "sh param:$1,$2,$3,$4,$5,$6,$7"
destination=".config"
config_file=""
tee=""
outdir="../..$3/test_info/gen/kernel/test"
if [ "$5" = "tee" ]; then
    tee="_tee"
fi
product_name="$(basename $7)"
source="tools/build/config/${product_name}_release.config"
if [ "$2" = "clang" ]; then
    if [ "$4" = "debug" ]; then
        config_file="${product_name}_$2$tee.config"
        source="tools/build/config/debug/$config_file"
    else
        config_file="${product_name}_$2_release$tee.config"
        source="tools/build/config/$config_file"
    fi
elif [ "$2" = "gcc" ]; then
    if [ "$4" = "debug" ]; then
        config_file="${product_name}_debug_shell$tee.config"
        source="tools/build/config/$config_file"
    else
        config_file="${product_name}_release$tee.config"
        source="tools/build/config/$config_file"
    fi
fi
if [ -d "./out" ]; then
    rm -rf ./out
fi
if [ -f "$destination" ]; then
    rm -rf $destination
fi
if [ ! -f "$source" ]; then
    source="$7/config/sys/$config_file"
fi
cp $source $destination

mkdir -p $outdir
cp kernel_test.sources $outdir

