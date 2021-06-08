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

system=$(uname -s)
ROOTFS_DIR=$1
FSTYPE=$2
ROOTFS_IMG=${ROOTFS_DIR}"_"${FSTYPE}".img"
JFFS2_TOOL=mkfs.jffs2
WIN_JFFS2_TOOL=mkfs.jffs2.exe
YAFFS2_TOOL=mkyaffs2image100
VFAT_TOOL=mkfs.vfat
ROMFS_TOOL=genromfs
MCOPY_TOOL=mcopy

tool_check() {
local ret='0'
command -v "$1" >/dev/null 2>&1 || { local ret='1'; }
if [ "$ret" -ne 0 ]; then
    echo "$1 tool is not exit, please install it" >&2
fi
return 0
}

chmod -R 755 ${ROOTFS_DIR}
if [ -f "${ROOTFS_DIR}/bin/init" ]; then
    chmod 700 ${ROOTFS_DIR}/bin/init 2> /dev/null
fi
if [ -f "${ROOTFS_DIR}/bin/shell" ]; then
    chmod 700 ${ROOTFS_DIR}/bin/shell 2> /dev/null
fi

if [ "${FSTYPE}" = "jffs2" ]; then
    if [ "${system}" != "Linux" ] ; then
        tool_check ${WIN_JFFS2_TOOL}
        ${WIN_JFFS2_TOOL} -q -o ${ROOTFS_IMG} -d ${ROOTFS_DIR} --pagesize=4096
    else
        tool_check ${JFFS2_TOOL}
        ${JFFS2_TOOL} -q -o ${ROOTFS_IMG} -d ${ROOTFS_DIR} --pagesize=4096
    fi
elif [ "${FSTYPE}" = "yaffs2" ]; then
    tool_check ${YAFFS2_TOOL}
    ${YAFFS2_TOOL}  ${ROOTFS_DIR} ${ROOTFS_IMG} 2k 24bit
elif [ "${FSTYPE}" = "romfs" ]; then
    tool_check ${ROMFS_TOOL}
    ${ROMFS_TOOL}  -d ${ROOTFS_DIR} -f ${ROOTFS_IMG}
elif [ "${FSTYPE}" = "vfat" ]; then
    if [ "${system}" != "Linux" ] ; then
        echo "Unsupported fs type!" >&2
    else
        tool_check ${VFAT_TOOL}
        tool_check ${MCOPY_TOOL}
        BLK_SIZE=512
        CLT_SIZE=2048
        FAT_TAB_NUM=2
        CLT_CNT=$(( ${CLT_SIZE} / ${BLK_SIZE} ))
        if [ $# -eq 3 ]; then
            IMG_SIZE=$3
        else
            FAT32_ITEM_SIZE=4
            RESV_CNT=38
            IMG_MIN_SIZE=1048576
            DU_DIR_SIZE=$(( $(echo $(du -s ${ROOTFS_DIR} | awk '{print $1}')) * 1024 ))
            DIR_NUM=$(( $(echo $(ls -lR ${ROOTFS_DIR} | grep "^d" | wc -l | awk '{print $1}')) + 1 ))
            DIR_SIZE=$(( ${DU_DIR_SIZE} + ${DIR_NUM} * 4096 ))
            IMG_SIZE=$(( ${DIR_SIZE} / (1 - ${FAT_TAB_NUM} * ${FAT32_ITEM_SIZE} / ${CLT_SIZE}) + ${RESV_CNT} * ${BLK_SIZE}))
            if [ ${IMG_SIZE} -le ${IMG_MIN_SIZE} ]; then
                IMG_SIZE=${IMG_MIN_SIZE}
            fi
        fi
        IMG_CNT=$(( (${IMG_SIZE} + ${BLK_SIZE} - 1) / ${BLK_SIZE} ))
        echo mtools_skip_check=1 >> ~/.mtoolsrc
        dd if=/dev/zero of=${ROOTFS_IMG} count=${IMG_CNT} bs=${BLK_SIZE}
        ${VFAT_TOOL} ${ROOTFS_IMG} -s ${CLT_CNT} -f ${FAT_TAB_NUM} -S ${BLK_SIZE} > /dev/null
        ${MCOPY_TOOL} -i ${ROOTFS_IMG} ${ROOTFS_DIR}/* -/ ::/
    fi
else
    echo "Unsupported fs type!" >&2
fi
