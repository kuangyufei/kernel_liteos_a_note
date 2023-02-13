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

void TestPosixSpawnFileActionsAddfchdirNp(void)
{
    posix_spawn_file_actions_t fa;
    printf("start ---- TestPosixSpawnFileActionsAddfchdirNp\n");
    DT_Enable_Support_Loop(1);
    
    DT_FUZZ_START(0, FREQUENCY, const_cast<char *>("TestPosixSpawnFileActionsAddfchdirNp"), 0) {
        fa.__pad0[0] = *(s32 *)DT_SetGetS32(&g_Element[0], INITVALUE_S32);
        fa.__pad0[1] = *(s32 *)DT_SetGetS32(&g_Element[1], INITVALUE_S32);
        fa.__pad0[2] = *(s32 *)DT_SetGetS32(&g_Element[2], INITVALUE_S32);
        for (int i = 0; i < 16; i++) {
            fa.__pad[i] = *(s32 *)DT_SetGetS32(&g_Element[i+3], INITVALUE_S32);
        }
        char *action = DT_SetGetBlob(&g_Element[18], LENGTH_STRING, MAXLENGTH_STRING, INITVALUE_STRING);
        fa.__actions=(void *)action;
        s32 tmpfd = *(s32 *)DT_SetGetS32(&g_Element[19], INITVALUE_S32);
        posix_spawn_file_actions_addfchdir_np(&fa, tmpfd);
    }
    DT_FUZZ_END();
    printf("end --- TestPosixSpawnFileActionsAddfchdirNp\n");
}
