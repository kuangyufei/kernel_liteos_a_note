#!/usr/bin/env python2
# -*- coding: utf-8 -*-

#
# Copyright (c) 2021 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import sys
import os
import argparse
import commands
import io

def find_string(excinfo_file, string):
    res = ''
    with open(excinfo_file, 'r+') as f:
        for lines in f:
            if string in lines:
                res = lines
                break
    return res

def is_kernel_exc(excinfo_file):
    res = find_string(excinfo_file, 'excFrom: kernel')
    print(res)
    return res != ''

def is_user_exc(excinfo_file):
    res = find_string(excinfo_file, 'excFrom: User')
    print(res)
    return res != ''

def parse_string_line(excinfo_file, string):
    line = find_string(excinfo_file, string)
    if line == '':
        print("%s is not in %s\n" %(string, excinfo_file))
        return ''
    line = line.replace('\n', '')
    strlist = line.split(' ')
    return strlist

def parse_kernel_pc_klr(excinfo_file, ohos_image_file, string, addr2line_cmd, objdump_cmd):
    #parse pc
    f = open(excinfo_file, 'r+')
    start = 0
    for lines in f.readlines():
        if 'excFrom: kernel' in lines:
            if start == 1:
                break
            start = 1
        if start and string in lines:
            lines = lines[lines.find(string):]
            strlist = lines.split()
            cmd = objdump_cmd + ohos_image_file + ' | grep ' + strlist[2][2:] + ': -B 10 -A 5 -w'
            ret = commands.getoutput(cmd)
            print(ret)
            cmd = addr2line_cmd + ohos_image_file + ' ' + strlist[2]
            ret = commands.getoutput(cmd)
            ret = ret.split('\n')
            print('<' + string + '>' + ret[0] + ' <' + strlist[2] + '>\n')
            f.close() 
            return 0
    f.close()
    return -1

def parse_kernel_lr(excinfo_file, ohos_image_file, addr2line_cmd):
    f = open(excinfo_file, 'r+')
    start = 0
    index = 1
    for lines in f.readlines():
        if 'excFrom: kernel' in lines:
            if start == 1:
                break
            start = 1
        if start and 'lr =' in lines:
            lines = lines[lines.find('lr ='):]
            strlist = lines.split()
            cmd = addr2line_cmd + ohos_image_file + ' ' + strlist[2]
            ret = commands.getoutput(cmd)
            ret = ret.split('\n')
            print('<%.2d'%index + '>' + ret[0] + ' <' + strlist[2] + '>')
            index = index + 1

    f.close()

def parse_kernel_exc(excinfo_file, ohos_image_file, addr2line_cmd, objdump_cmd):
    #parse pc, klr
    ret1 = parse_kernel_pc_klr(excinfo_file, ohos_image_file, 'pc', addr2line_cmd, objdump_cmd)
    ret2 = parse_kernel_pc_klr(excinfo_file, ohos_image_file, 'klr', addr2line_cmd, objdump_cmd)
    #parse lr
    parse_kernel_lr(excinfo_file, ohos_image_file, addr2line_cmd)
    return ret1 and ret2

def parse_user_pc_ulr(excinfo_file, rootfs_dir, string, addr2line_cmd, objdump_cmd):
    #parse pc
    f = open(excinfo_file, 'r+')
    start = 0
    for lines in f.readlines():
        if 'excFrom: User' in lines:
            if start == 1:
                break
            start = 1
        if start and string in lines:
            lines = lines[lines.find(string):]
            strlist = lines.split()
            if len(strlist) < 7:
                print('%s is error'%string)
                f.close()
                return 0
            cmd = objdump_cmd + rootfs_dir + strlist[4] + ' | grep ' + strlist[6][2:] + ': -B 10 -A 5 -w'
            ret = commands.getoutput(cmd)
            print(ret)
            cmd = addr2line_cmd + rootfs_dir + strlist[4] + ' ' + strlist[6]
            #print(cmd)
            ret = commands.getoutput(cmd)
            ret = ret.split('\n')
            print('<' + string + '>' + ret[0] + ' <' + strlist[6] + '>' + '<' + strlist[4] + '>\n')
            f.close() 
            return 0
    f.close()
    return -1

def parse_user_lr(excinfo_file, rootfs_dir, addr2line_cmd):
    f = open(excinfo_file, 'r+')
    start = 0
    index = 1
    for lines in f.readlines():
        if 'excFrom: User' in lines:
            if start == 1:
                break
            start = 1
        if start and 'lr =' in lines:
            lines = lines[lines.find('lr ='):]
            strlist = lines.split()
            if len(strlist) < 11:
                print('%s is error'%strlist)
                f.close()
                return
            cmd = addr2line_cmd + rootfs_dir + strlist[8] + ' ' + strlist[10]
            res = commands.getoutput(cmd)
            res = res.split('\n')
            print('<%.2d>'%index + res[0] + ' <' + strlist[10] + '>' + '<' + strlist[8] + '>')
            index = index + 1

    f.close()

def parse_user_exc(excinfo_file, rootfs_dir, addr2line_cmd, objdump_cmd):
    #parse pc ulr
    ret1 = parse_user_pc_ulr(excinfo_file, rootfs_dir, 'pc', addr2line_cmd, objdump_cmd)
    ret2 = parse_user_pc_ulr(excinfo_file, rootfs_dir, 'ulr', addr2line_cmd, objdump_cmd)
    #parse lr
    parse_user_lr(excinfo_file, rootfs_dir, addr2line_cmd)
    return ret1 and ret2

def parse_backtrace(backtrace_file, ohos_image_file, addr2line_cmd):
    f = open(backtrace_file, 'r+')
    find = -1
    start = 0
    index = 1
    for lines in f.readlines():
        if 'backtrace begin' in lines:
            if start == 1:
                break
            start = 1
        if start and 'lr =' in lines:
            lines = lines[lines.find('lr ='):]
            strlist = lines.split()
            cmd = addr2line_cmd + ohos_image_file + ' ' + strlist[2]
            ret = commands.getoutput(cmd)
            ret = ret.split('\n')
            print('\n<%.2d'%index + '>' + ret[0] + ' <' + strlist[2] + '>')
            index = index + 1
            find = 0

    f.close()
    return find

def parse_excinfo(excinfo_file, ohos_image_file, rootfs_dir, addr2line_cmd, objdump_cmd):
    cmd = 'dos2unix ' + excinfo_file
    commands.getoutput(cmd)
    kernel_exc = is_kernel_exc(excinfo_file)
    user_exc = is_user_exc(excinfo_file)

    if kernel_exc == False and user_exc == False:
        if parse_backtrace(excinfo_file, ohos_image_file, addr2line_cmd) != 0:
            print("%s is not a excinfo or backtrace file\n"%excinfo_file)
            return -1
        else:
            return 0
    if user_exc:
        if rootfs_dir != None:
            return parse_user_exc(excinfo_file, rootfs_dir, addr2line_cmd, objdump_cmd)
        else:
            print('error: rootfs_dir is none')
            return -1
    return parse_kernel_exc(excinfo_file, ohos_image_file, addr2line_cmd, objdump_cmd)

def parse_compiler(compiler):
    addr2line = ''
    addr2line_cmd = ''
    objdump = ''
    objdump_cmd = ''
    cmd = 'which ' + compiler
    ret = commands.getoutput(cmd)
    if ret == '':
        print('%s is not exist'%compiler)
        return None
    index1 = ret.rfind('gcc')
    index2 = ret.rfind('clang')
    if index1 != -1:
        addr2line = ret[0:index1] + 'addr2line'
        objdump = ret[0:index1] + 'objdump'
    elif index2 != -1:
        index3 = ret.rfind('/')
        addr2line = ret[0:index3 + 1] + 'llvm-addr2line'
        objdump = ret[0:index3 + 1] + 'llvm-objdump'
    else:
        print('%s is not arm-xxx-xxx-gcc or clang'%compiler)
        return None
    addr2line_cmd = addr2line + ' -C -f -e '
    objdump_cmd = objdump + ' -d '
    return [addr2line_cmd, objdump_cmd]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--f', help = 'excinfo file or backtrace file')
    parser.add_argument('--e', help = 'elf system image file')
    parser.add_argument('--r', help = 'the path of rootfs')
    parser.add_argument('--c', help = 'compiler [arm-xxx-xxx-gcc/clang]')
    args = parser.parse_args()

    if args.f == None or args.e == None:
        print("input error\n")
        parser.print_help()
        return -1

    excinfo_file = args.f
    ohos_image_file = args.e
    rootfs_dir = args.r

    addr2line_cmd = 'llvm-addr2line -C -f -e '
    objdump_cmd = 'llvm-objdump -d '
    if args.c != None:
        cmd = parse_compiler(args.c)
        if cmd == None:
            return -1
        addr2line_cmd = cmd[0]
        objdump_cmd = cmd[1]
    return parse_excinfo(excinfo_file, ohos_image_file, rootfs_dir, addr2line_cmd, objdump_cmd)

if __name__ == "__main__":
    sys.exit(main())
