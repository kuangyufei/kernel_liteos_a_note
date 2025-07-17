/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2022 Huawei Device Co., Ltd. All rights reserved.
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

#include "los_magickey.h"
#include "console.h"
#include "los_task_pri.h"
#include "los_process_pri.h"

#ifdef LOSCFG_ENABLE_MAGICKEY

#define MAGIC_KEY_NUM 5
STATIC VOID OsMagicHelp(VOID);
STATIC VOID OsMagicTaskShow(VOID);
STATIC VOID OsMagicPanic(VOID);
STATIC VOID OsMagicMemCheck(VOID);

STATIC MagicKeyOp g_magicMemCheckOp = {
    .opHandler = OsMagicMemCheck,
    .helpMsg = "Check system memory(ctrl+e) ",
    .magicKey = 0x05 /* ctrl + e */
};

STATIC MagicKeyOp g_magicPanicOp = {
    .opHandler = OsMagicPanic,
    .helpMsg = "System panic(ctrl+p) ",
    .magicKey = 0x10 /* ctrl + p */
};

STATIC MagicKeyOp g_magicTaskShowOp = {
    .opHandler = OsMagicTaskShow,
    .helpMsg = "Show task information(ctrl+t) ",
    .magicKey = 0x14 /* ctrl + t */
};

STATIC MagicKeyOp g_magicHelpOp = {
    .opHandler = OsMagicHelp,
    .helpMsg = "Show all magic op key(ctrl+z) ",
    .magicKey = 0x1a /* ctrl + z */
};

/*
 * NOTICE:Suggest don't use
 * ctrl+h/backspace=0x8,
 * ctrl+i/tab=0x9,
 * ctrl+m/enter=0xd,
 * ctrl+n/shift out=0xe,
 * ctrl+o/shift in=0xf,
 * ctrl+[/esc=0x1b,
 * ctrl+] used for telnet command mode;
 */
STATIC MagicKeyOp *g_magicOpTable[MAGIC_KEY_NUM] = {
    &g_magicMemCheckOp, /* ctrl + e */
    &g_magicPanicOp,    /* ctrl + p */
    &g_magicTaskShowOp, /* ctrl + t */
    &g_magicHelpOp,     /* ctrl + z */
    NULL
};

STATIC VOID OsMagicHelp(VOID)
{
    INT32 i;
    PRINTK("HELP: ");
    for (i = 0; g_magicOpTable[i] != NULL; ++i) {
        PRINTK("%s ", g_magicOpTable[i]->helpMsg);
    }
    PRINTK("\n");
    return;
}

STATIC VOID OsMagicTaskShow(VOID)
{
    (VOID)OsShellCmdTskInfoGet(OS_ALL_TASK_MASK, NULL, OS_PROCESS_INFO_ALL);
    return;
}

STATIC VOID OsMagicPanic(VOID)
{
    LOS_Panic("Magic key :\n");
    return;
}

STATIC VOID OsMagicMemCheck(VOID)
{
    if (LOS_MemIntegrityCheck(m_aucSysMem1) == LOS_OK) {
        PrintExcInfo("system memcheck over, all passed!\n");
    }
    return;
}
#endif

INT32 CheckMagicKey(CHAR key, UINT16 consoleId)
{
#ifdef LOSCFG_ENABLE_MAGICKEY
    INT32 i;
    STATIC UINT32 magicKeySwitch = 0;

    if (key == 0x03) { /* ctrl + c */
        KillPgrp(consoleId);
        return 0;
    } else if (key == 0x12) { /* ctrl + r */
        magicKeySwitch = ~magicKeySwitch;
        if (magicKeySwitch != 0) {
            PrintExcInfo("Magic key on\n");
        } else {
            PrintExcInfo("Magic key off\n");
        }
        return 1;
    }
    if (magicKeySwitch != 0) {
        for (i = 0; i < MAGIC_KEY_NUM; i++) {
            if (g_magicOpTable[i] != NULL && key == g_magicOpTable[i]->magicKey) {
                (g_magicOpTable[i])->opHandler();
                return 1;
            }
        }
    }
#endif
    return 0;
}
