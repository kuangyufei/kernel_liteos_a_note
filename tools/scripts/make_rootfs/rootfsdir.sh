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

OUT=$1
ROOTFS_DIR=$2
OUT_DIR=$3
BIN_DIR=${OUT}/bin
LIB_DIR=${OUT}/musl
ETC_DIR=${OUT}/etc
NEED_COPYTO_OUTDIR=(shell toybox mksh tftp)

mkdir -p ${ROOTFS_DIR}/bin ${ROOTFS_DIR}/lib ${ROOTFS_DIR}/usr/bin ${ROOTFS_DIR}/usr/lib ${ROOTFS_DIR}/etc \
${ROOTFS_DIR}/app ${ROOTFS_DIR}/data ${ROOTFS_DIR}/proc ${ROOTFS_DIR}/dev ${ROOTFS_DIR}/data/system ${ROOTFS_DIR}/data/system/param \
${ROOTFS_DIR}/system ${ROOTFS_DIR}/system/internal ${ROOTFS_DIR}/system/external ${OUT_DIR}/bin ${OUT_DIR}/libs ${OUT_DIR}/etc
if [ -d "${BIN_DIR}" ] && [ "$(ls -A "${BIN_DIR}")" != "" ]; then
    cp -f ${BIN_DIR}/* ${ROOTFS_DIR}/bin
    for el in ${NEED_COPYTO_OUTDIR[@]}
    do
        if [ -e ${BIN_DIR}/$el ] && [ "${BIN_DIR}/$el" != "${OUT_DIR}/bin/$el" ]; then
            cp -u ${BIN_DIR}/$el ${OUT_DIR}/bin/$el
        fi
    done
fi
cp -f ${LIB_DIR}/* ${ROOTFS_DIR}/lib
cp -u ${LIB_DIR}/* ${OUT_DIR}/libs

if [ -e ${ETC_DIR}/.mkshrc ]; then
cp -f ${ETC_DIR}/.mkshrc ${ROOTFS_DIR}/etc
cp -u ${ETC_DIR}/.mkshrc ${OUT_DIR}/etc
fi
