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

board_name=${1}
ohos_build_compiler=${2}
root_build_dir=${3}
ohos_build_type=${4}
tee_enable=${5}
device_company=${6}
product_path=${7}
outdir=${8}
ohos_version=${9}
sysroot_path=${10}
arch_cflags=${11}
device_path=${12}

echo "${board_name}" "${device_company}"
echo "sh param:" "$@"

function main() {
    destination=".config"
    tee=""
    if [ "${tee_enable}" = "true" ]; then
        tee="_tee"
    fi

    config_file="${product_path}/config/${ohos_build_type}${tee}.config"
    if [ -f "${config_file}" ]; then
        cp "${config_file}" "${destination}"
        return
    fi

    product_name=$(basename "${product_path}")
    config_file="${product_name}_release.config"
    if [ "${ohos_build_compiler}" = "clang" ]; then
        if [ "${ohos_build_type}" = "debug" ]; then
            config_file="debug/${product_name}_${ohos_build_compiler}${tee}.config"
        else
            config_file="${product_name}_${ohos_build_compiler}_release${tee}.config"
        fi
    elif [ "${ohos_build_compiler}" = "gcc" ]; then
        if [ "${ohos_build_type}" = "debug" ]; then
            config_file="${product_name}_debug_shell${tee}.config"
        else
            config_file="${product_name}_release${tee}.config"
        fi
    fi
    cp "tools/build/config/${config_file}" "${destination}"
}

if [ "x" != "x${sysroot_path}" ]; then
    export SYSROOT_PATH=${sysroot_path}
fi

if [ "x" != "x${arch_cflags}" ]; then
    export ARCH_CFLAGS="${arch_cflags}"
fi

export OUTDIR="${outdir}"
export PRODUCT_PATH="${product_path}"
export DEVICE_PATH="${device_path}"

main && \
make clean && \
make -j rootfs VERSION="${ohos_version}"
