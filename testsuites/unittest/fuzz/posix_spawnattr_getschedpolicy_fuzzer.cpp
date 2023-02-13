/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
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

#include <spawn.h>
#include <secodeFuzz.h>
#include "fuzzertest.h"

void TestPosixSpawnattrGetschedpolicy(void)
{
    posix_spawnattr_t attr;
    printf("start ---- TestPosixSpawnattrGetschedpolicy\n");
    DT_Enable_Support_Loop(1);
    
    DT_FUZZ_START(0, FREQUENCY, const_cast<char *>("TestPosixSpawnattrGetschedpolicy"), 0) {
        char *datainput = DT_SetGetFixBlob(&g_Element[0], sizeof(posix_spawnattr_t), sizeof(posix_spawnattr_t),
            (char *)&attr);
        s32 policy = *(s32 *)DT_SetGetS32(&g_Element[1], INITVALUE_S32);
        posix_spawnattr_getschedpolicy((posix_spawnattr_t *)datainput, &policy);
    }
    DT_FUZZ_END();
    printf("end --- TestPosixSpawnattrGetschedpolicy\n");
}
