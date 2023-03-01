/*
 * Copyright (c) 2023-2023 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _IT_CONTAINER_TEST_H
#define _IT_CONTAINER_TEST_H

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <regex>
#include <csignal>
#include <sys/syscall.h>
#include <sys/capability.h>
#include <cstring>
#include "osTest.h"
#include "mqueue.h"
#include "sys/time.h"
#include "sys/shm.h"
#include "sys/types.h"

const int EXIT_CODE_ERRNO_1 = 1;
const int EXIT_CODE_ERRNO_2 = 2;
const int EXIT_CODE_ERRNO_3 = 3;
const int EXIT_CODE_ERRNO_4 = 4;
const int EXIT_CODE_ERRNO_5 = 5;
const int EXIT_CODE_ERRNO_6 = 6;
const int EXIT_CODE_ERRNO_7 = 7;
const int EXIT_CODE_ERRNO_8 = 8;
const int EXIT_CODE_ERRNO_9 = 9;
const int EXIT_CODE_ERRNO_10 = 10;
const int EXIT_CODE_ERRNO_11 = 11;
const int EXIT_CODE_ERRNO_12 = 12;
const int EXIT_CODE_ERRNO_13 = 13;
const int EXIT_CODE_ERRNO_14 = 14;
const int EXIT_CODE_ERRNO_15 = 15;
const int EXIT_CODE_ERRNO_16 = 16;
const int EXIT_CODE_ERRNO_17 = 17;
const int EXIT_CODE_ERRNO_255 = 255;
const int CONTAINER_FIRST_PID = 1;
const int CONTAINER_SECOND_PID = 2;
const int CONTAINER_THIRD_PID = 3;

const int MQUEUE_TEST_SIZE = 50;
const int MQUEUE_TEST_MAX_MSG = 255;

const int SHM_TEST_DATA_SIZE = 1024;
const int SHM_TEST_KEY1 = 1234;
const int SHM_TEST_OPEN_PERM = 0666;
const int CLONE_STACK_MMAP_FLAG = MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK;

extern const char *USERDATA_DIR_NAME;
extern const char *ACCESS_FILE_NAME;
extern const char *MNT_ACCESS_FILE_NAME;
extern const char *USERDATA_DEV_NAME;
extern const char *FS_TYPE;

extern const int BIT_ON_RETURN_VALUE;
extern const int STACK_SIZE;
extern const int CHILD_FUNC_ARG;

const int MQUEUE_STANDARD_NAME_LENGTH  = 255;

extern "C" {
#define CLONE_NEWTIME   0x00000080
}

int WriteFile(const char *filepath, const char *buf);
int ReadFile(const char *filepath, char *buf);
int GetLine(char *buf, int count, int maxLen, char **array);

int ChildFunction(void *args);

pid_t CloneWrapper(int (*func)(void *), int flag, void *args);

int WaitChild(pid_t pid, int *status, int errNo1, int errNo2);

std::string GenContainerLinkPath(int pid, const std::string& containerType);

extern std::string ReadlinkContainer(int pid, const std::string& containerType);

class MQueueFinalizer {
public:
    explicit MQueueFinalizer(mqd_t mqueueParent, const std::string& mqname)
    {
        m_mqueueParent = mqueueParent;
        m_mqname = mqname;
    }
    ~MQueueFinalizer()
    {
        if (m_mqueueParent >= 0) {
            mq_close(m_mqueueParent);
            mq_unlink(m_mqname.c_str());
        }
    }
private:
    mqd_t m_mqueueParent;
    std::string m_mqname;
};

class ShmFinalizer {
public:
    explicit ShmFinalizer(void* shm, int shmid)
    {
        m_shm = shm;
        m_shmid = shmid;
    }
    ~ShmFinalizer()
    {
        shmdt(m_shm);
        shmctl(m_shmid, IPC_RMID, nullptr);
    }
private:
    void* m_shm;
    int m_shmid;
};

void ItUserContainer001(void);
void ItUserContainer002(void);
void ItUserContainer003(void);
void ItUserContainer004(void);
void ItUserContainer005(void);
void ItUserContainer006(void);
void ItUserContainer007(void);
void ItContainer001(void);
void ItContainerChroot001(void);
void ItContainerChroot002(void);
void ItPidContainer023(void);
void ItPidContainer025(void);
void ItPidContainer026(void);
void ItPidContainer027(void);
void ItPidContainer028(void);
void ItPidContainer029(void);
void ItPidContainer030(void);
void ItPidContainer031(void);
void ItPidContainer032(void);
void ItPidContainer033(void);
void ItUtsContainer001(void);
void ItUtsContainer002(void);
void ItUtsContainer004(void);
void ItUtsContainer005(void);
void ItUtsContainer006(void);
void ItUtsContainer007(void);
void ItUtsContainer008(void);
void ItMntContainer001(void);
void ItMntContainer002(void);
void ItMntContainer003(void);
void ItMntContainer004(void);
void ItMntContainer005(void);
void ItMntContainer006(void);
void ItMntContainer007(void);
void ItMntContainer008(void);
void ItMntContainer009(void);
void ItMntContainer010(void);
void ItIpcContainer001(void);
void ItIpcContainer002(void);
void ItIpcContainer003(void);
void ItIpcContainer004(void);
void ItIpcContainer005(void);
void ItIpcContainer006(void);
void ItIpcContainer007(void);
void ItIpcContainer008(void);
void ItTimeContainer001(void);
void ItTimeContainer002(void);
void ItTimeContainer003(void);
void ItTimeContainer004(void);
void ItTimeContainer005(void);
void ItTimeContainer006(void);
void ItTimeContainer007(void);
void ItTimeContainer008(void);
void ItTimeContainer009(void);
void ItTimeContainer010(void);
void ItPidContainer001(void);
void ItPidContainer002(void);
void ItPidContainer003(void);
void ItPidContainer004(void);
void ItPidContainer005(void);
void ItPidContainer006(void);
void ItPidContainer007(void);
void ItPidContainer008(void);
void ItPidContainer009(void);
void ItPidContainer010(void);
void ItPidContainer011(void);
void ItPidContainer012(void);
void ItPidContainer013(void);
void ItPidContainer014(void);
void ItPidContainer015(void);
void ItPidContainer016(void);
void ItPidContainer017(void);
void ItPidContainer018(void);
void ItPidContainer019(void);
void ItPidContainer020(void);
void ItPidContainer021(void);
void ItPidContainer022(void);
void ItPidContainer024(void);
void ItUtsContainer003(void);
void ItNetContainer001(void);
void ItNetContainer002(void);
void ItNetContainer003(void);
void ItNetContainer004(void);
void ItNetContainer005(void);
void ItNetContainer006(void);
void ItNetContainer007(void);
void ItNetContainer008(void);
void ItNetContainer009(void);
void ItNetContainer010(void);
void ItNetContainer011(void);
void ItNetContainer012(void);
#endif /* _IT_CONTAINER_TEST_H */
