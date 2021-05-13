/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
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
#ifndef _IT_TEST_IO_H
#define _IT_TEST_IO_H

#include "osTest.h"
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "string.h"
#include "termios.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "locale.h"
#include "wctype.h"
#include "wchar.h"
#include "stdarg.h"
#include "semaphore.h"
#include "ftw.h"
#include "aio.h"
#include "shadow.h"
#include "pty.h"
#include "dirent.h"
#include "poll.h"
#include "grp.h"
#include "pwd.h"
#include "sys/uio.h"
#include "syslog.h"

extern int CloseRmAllFile(int fd[], char filePathName[][50], int cnt);
extern char *g_ioTestPath;

extern VOID ItTestIo001(VOID);
extern VOID ItTestIo002(VOID);
extern VOID ItTestIo003(VOID);
extern VOID ItTestIo004(VOID);
extern VOID ItTestIo005(VOID);
extern VOID ItTestIo006(VOID);
extern VOID ItTestIo007(VOID);
extern VOID ItTestIo008(VOID);
extern VOID ItTestIo009(VOID);
extern VOID ItTestIo010(VOID);
extern VOID ItTestIo011(VOID);
extern VOID ItTestIo012(VOID);
extern VOID ItTestIo013(VOID);

extern VOID ItLocaleFreelocale001(void);
extern VOID ItLocaleLocaleconv001(void);
extern VOID ItStdioFputws001(void);
extern VOID ItStdioFwprintf001(void);
extern VOID ItStdioFtruncate001(void);
extern VOID ItStdioFtw001(void);
extern VOID ItStdlibOpenpty001(void);
extern VOID ItStdlibPtsname001(void);
extern VOID ItStdioGetcUnlocked001(void);
extern VOID ItStdioGetcharUnlocked001(void);
extern VOID ItStdioGetw001(void);
extern VOID ItStdioGetwchar001(void);
extern VOID ItStdioLioListio001(void); // linux erro
extern VOID ItStdioMblen001(void);
extern VOID ItStdioMbrlen001(void);
extern VOID ItStdioMbstowcs001(void);
extern VOID ItStdioMbsnrtowcs001(void);
extern VOID ItStdioPutcUnlocked001(void);
extern VOID ItStdioPutcharUnlocked001(void);
extern VOID ItStdioPutgrent001(void);
extern VOID ItStdioPutpwent001(void);
extern VOID ItStdioPutspent001(void);
extern VOID ItStdioPutwc001(void);
extern VOID ItStdioPutwchar001(void);
extern VOID ItStdioReadv001(void);
extern VOID ItStdioRindex001(void);
extern VOID ItStdioSelect002(void);
extern VOID ItStdioSetgrent001(void);
extern VOID ItStdioSetlogmask001(void);
extern VOID ItStdioSetmntent001(void);
extern VOID ItStdlibGcvt001(void);
extern VOID ItStdlibOpenpty001(void);
extern VOID ItStdlibPoll001(void);
extern VOID ItStdlibPoll002(void);
extern VOID ItStdlibPoll003(void);
extern VOID ItStdlibPtsname001(void);
extern VOID IT_STDIO_HASMNTOPT_001(void);
extern VOID IO_TEST_NGETTEXT_001(void);
extern VOID IO_TEST_EPOLL_001(void);
extern VOID IO_TEST_LOCALE_001(void);
extern VOID IO_TEST_LOCALE_002(void);
extern VOID IO_TEST_CONFSTR_001(void);
extern VOID IO_TEST_NL_LANGINFO_001(VOID);
extern VOID IO_TEST_STRCASECMP_L_001(VOID);
extern VOID IO_TEST_STRCASECMP_L_002(VOID);
extern VOID IO_TEST_DUPLOCALE_001(void);
extern VOID IO_TEST_NL_LANGINFO_l_001(VOID);
extern VOID IO_TEST_DNGETTEXT_001(VOID);
extern VOID IO_TEST_DNGETTEXT_002(VOID);
extern VOID IO_TEST_DCNGETTEXT_001(VOID);
extern VOID IO_TEST_DCNGETTEXT_002(VOID);
extern VOID IO_TEST_DCGETTEXT_001(VOID);
extern VOID IO_TEST_DCGETTEXT_002(VOID);
extern VOID IO_TEST_GETTEXT_001(VOID);
extern VOID IO_TEST_PPOLL_001(void);
extern VOID IO_TEST_PPOLL_002(void);
extern VOID IO_TEST_PSELECT_001(void);
extern VOID IO_TEST_STRFMON_L_001(VOID);
extern VOID IO_TEST_STRFMON_L_002(VOID);


#endif
