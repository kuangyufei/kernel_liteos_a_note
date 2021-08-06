#!/bin/bash

#Copyright (c) 2020-2021 Huawei Device Co., Ltd.
#Licensed under the Apache License, Version 2.0 (the "License");
#you may not use this file except in compliance with the License.
#You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
#Unless required by applicable law or agreed to in writing, software
#distributed under the License is distributed on an "AS IS" BASIS,
#WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#See the License for the specific language governing permissions and
#limitations under the License.

LOAD_BASE="0x2000000"
LLVM_ADDR2LINE=llvm-addr2line
GCC_ADDR2LINE=addr2line

get_line()
{
    SYM_ADDR=$(echo $1 | awk '{print $2}')
    ELF_OFFSET=`echo ${SYM_ADDR} | cut -d '[' -f2 | cut -d ']' -f1`
    FILE_LINE=`${ADDR2LINE} -f -e $2 ${ELF_OFFSET} | awk 'NR==2'`
    if [[ "${FILE_LINE}" == *"?"* ]]; then
        typeset ELF_OFFSET=$((ELF_OFFSET+LOAD_BASE))
        ELF_OFFSET=$(echo "obase=16;${ELF_OFFSET}" | bc)
        FILE_LINE=`${ADDR2LINE} -f -e $2 ${ELF_OFFSET} | awk 'NR==2'`
    fi
    echo ${FILE_LINE}
}

parse_line()
{
    FILE_PATH=$(echo $1 | awk '{print $4}')
    ELF_FILE=${FILE_PATH##*/}
    ELF_FILE=${ELF_FILE//[$'\t\r\n']}
    FLAG=false
    for i in $2; do
        if [ "${ELF_FILE}" = "$i" ]; then
            if [ ! -f "$i" ]; then
                echo "Error: no such file: $i"
                exit 1
            fi
            FILE_LINE=`get_line "$1" $i`
            if [[ "${FILE_LINE}" == *"?"* ]] || [ -z "${FILE_LINE}" ]; then
                echo "        * Error: you need ensure whether file: "$i" was compiled with -g or not! *"
                exit 1
            fi
            LINE=`echo $1 | tr -d '\r'`
            LINE=$(echo ${LINE} | awk '{print $1,$2}')
            echo "        "${LINE}" at "${FILE_LINE}
            FLAG=true
            break
        fi
    done
    if [[ "${FLAG}" == "false" ]]; then
        echo "        "$1
    fi
}

if [ $# -le 1 ]; then
    echo "Usage: ./out2line.sh text.txt elf1 elf2 ..."
    exit 1
fi

read -n5 -p "Compiler is [gcc/llvm]: " ANSWER
case ${ANSWER} in
(gcc | GCC)
    which ${GCC_ADDR2LINE} >/dev/null 2>&1
    if [ $? -eq 0 ]; then
        ADDR2LINE=${GCC_ADDR2LINE}
    else
        echo "${GCC_ADDR2LINE} command not found!"
        exit 1
    fi;;
(llvm | LLVM)
    which ${LLVM_ADDR2LINE} >/dev/null 2>&1
    if [ $? -eq 0 ]; then
        ADDR2LINE=${LLVM_ADDR2LINE}
    else
        echo "${LLVM_ADDR2LINE} command not found! Trying to use ${GCC_ADDR2LINE}!"
        which ${GCC_ADDR2LINE} >/dev/null 2>&1
        if [ $? -eq 0 ]; then
            ADDR2LINE=${GCC_ADDR2LINE}
        else
            echo "${GCC_ADDR2LINE} command not found!"
            exit 1
        fi
    fi;;
(*)
    echo "Error choise!"
    exit 1
esac
echo -e "Now using ${ADDR2LINE} ...\n"

PARAM_LIST="${*:2}"
cat $1 | while read line; do
    HEAD_STRING=$(echo ${line} | awk '{print $1}')
    if [[ "${HEAD_STRING}" == *"#"* ]]; then
        parse_line "${line}" "${PARAM_LIST}"
    else
        if [[ "${HEAD_STRING}" == *"["* ]]; then
            echo "    "${line}
        else
            echo ${line}
        fi
    fi
done

