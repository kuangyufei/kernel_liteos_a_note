/*
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
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

#include "los_bootargs.h"
#include "los_base.h"
#include "string.h"

#if defined(LOSCFG_STORAGE_SPINOR) || defined(LOSCFG_STORAGE_SPINAND) || defined(LOSCFG_PLATFORM_QEMU_ARM_VIRT_CA7)
#include "mtd_list.h"
#endif

#ifdef LOSCFG_PLATFORM_QEMU_ARM_VIRT_CA7
#include "cfiflash.h"
#endif

#ifdef LOSCFG_STORAGE_EMMC
#include "disk.h"
#endif

STATIC CHAR *g_cmdLine = NULL;
STATIC UINT64 g_alignSize = 0;
STATIC struct BootArgs g_bootArgs[MAX_ARGS_NUM] = {0};

INT32 LOS_GetCmdLine()
{
    int ret;

    g_cmdLine = (CHAR *)malloc(COMMAND_LINE_SIZE);
    if (g_cmdLine == NULL) {
        PRINT_ERR("Malloc g_cmdLine space error!\n");
        return LOS_NOK;
    }

#ifdef LOSCFG_STORAGE_EMMC
    los_disk *emmcDisk = los_get_mmcdisk_bytype(EMMC);
    if (emmcDisk == NULL) {
        PRINT_ERR("Get EMMC disk failed!\n");
        goto ERROUT;
    }
    g_alignSize = EMMC_SEC_SIZE;
    ret = los_disk_read(emmcDisk->disk_id, g_cmdLine, COMMAND_LINE_ADDR / EMMC_SEC_SIZE,
                        COMMAND_LINE_SIZE / EMMC_SEC_SIZE, TRUE);
    if (ret == 0) {
        return LOS_OK;
    }
#endif

#ifdef LOSCFG_STORAGE_SPINOR
    struct MtdDev *mtd = GetMtd("spinor");
    if (mtd == NULL) {
        PRINT_ERR("Get spinor mtd failed!\n");
        goto ERROUT;
    }
    g_alignSize = mtd->eraseSize;
    ret = mtd->read(mtd, COMMAND_LINE_ADDR, COMMAND_LINE_SIZE, g_cmdLine);
    if (ret == COMMAND_LINE_SIZE) {
        return LOS_OK;
    }
#endif

#ifdef LOSCFG_STORAGE_SPINAND
    struct MtdDev *mtd = GetMtd("nand");
    if (mtd == NULL) {
        PRINT_ERR("Get nand mtd failed!\n");
        goto ERROUT;
    }
    g_alignSize = mtd->eraseSize;
    ret = mtd->read(mtd, COMMAND_LINE_ADDR, COMMAND_LINE_SIZE, g_cmdLine);
    if (ret == COMMAND_LINE_SIZE) {
        return LOS_OK;
    }
#endif

#ifdef LOSCFG_PLATFORM_QEMU_ARM_VIRT_CA7
    struct MtdDev *mtd = GetCfiMtdDev();
    if (mtd == NULL) {
        PRINT_ERR("Get CFI mtd failed!\n");
        goto ERROUT;
    }
    g_alignSize = mtd->eraseSize;
    ret = mtd->read(mtd, CFIFLASH_BOOTARGS_ADDR, COMMAND_LINE_SIZE, g_cmdLine);
    if (ret == COMMAND_LINE_SIZE) {
        return LOS_OK;
    }
#endif

    PRINT_ERR("Read cmdline error!\n");
ERROUT:
    free(g_cmdLine);
    g_cmdLine = NULL;
    return LOS_NOK;
}

VOID LOS_FreeCmdLine()
{
    if (g_cmdLine != NULL) {
        free(g_cmdLine);
        g_cmdLine = NULL;
    }
}

STATIC INT32 GetBootargs(CHAR **args)
{
#ifdef LOSCFG_BOOTENV_RAM
    *args = OsGetArgsAddr();
    return LOS_OK;
#else
    INT32 i;
    INT32 len = 0;
    CHAR *tmp = NULL;
    const CHAR *bootargsName = "bootargs=";

    if (g_cmdLine == NULL) {
        PRINT_ERR("Should call LOS_GetCmdLine() first!\n");
        return LOS_NOK;
    }

    for (i = 0; i < COMMAND_LINE_SIZE; i += len + 1) {
        len = strlen(g_cmdLine + i);
        tmp = strstr(g_cmdLine + i, bootargsName);
        if (tmp != NULL) {
            *args = tmp + strlen(bootargsName);
            return LOS_OK;
        }
    }
    PRINT_ERR("Cannot find bootargs!\n");
    return LOS_NOK;
#endif
}

INT32 LOS_ParseBootargs()
{
    INT32 idx = 0;
    INT32 ret;
    CHAR *args = NULL;
    CHAR *argName = NULL;
    CHAR *argValue = NULL;

    ret = GetBootargs(&args);
    if (ret != LOS_OK) {
        return LOS_NOK;
    }

    while ((argValue = strsep(&args, " ")) != NULL) {
        argName = strsep(&argValue, "=");
        if (argValue == NULL) {
            /* If the argument is not compliance with the format 'foo=bar' */
            g_bootArgs[idx].argName = argName;
            g_bootArgs[idx].argValue = argName;
        } else {
            g_bootArgs[idx].argName = argName;
            g_bootArgs[idx].argValue = argValue;
        }
        if (++idx >= MAX_ARGS_NUM) {
            /* Discard the rest arguments */
            break;
        }
    }
    return LOS_OK;
}

INT32 LOS_GetArgValue(CHAR *argName, CHAR **argValue)
{
    INT32 idx = 0;

    while (idx < MAX_ARGS_NUM) {
        if (g_bootArgs[idx].argName == NULL) {
            break;
        }
        if (strcmp(argName, g_bootArgs[idx].argName) == 0) {
            *argValue = g_bootArgs[idx].argValue;
            return LOS_OK;
        }
        idx++;
    }

    return LOS_NOK;
}

UINT64 LOS_GetAlignsize()
{
    return g_alignSize;
}

UINT64 LOS_SizeStrToNum(CHAR *value)
{
    UINT64 num = 0;

    /* If the string is a hexadecimal value */
    if (sscanf_s(value, "0x%x", &num) > 0) {
        value += strlen("0x");
        if (strspn(value, "0123456789abcdefABCDEF") < strlen(value)) {
            goto ERROUT;
        }
        return num;
    }

    /* If the string is a decimal value in unit *Bytes */
    INT32 ret = sscanf_s(value, "%d", &num);
    INT32 decOffset = strspn(value, "0123456789");
    CHAR *endPos = value + decOffset;
    if ((ret <= 0) || (decOffset < (strlen(value) - 1))) {
        goto ERROUT;
    }

    if (strlen(endPos) == 0) {
        return num;
    } else if (strcasecmp(endPos, "k") == 0) {
        num = num * BYTES_PER_KBYTE;
    } else if (strcasecmp(endPos, "m") == 0) {
        num = num * BYTES_PER_MBYTE;
    } else if (strcasecmp(endPos, "g") == 0) {
        num = num * BYTES_PER_GBYTE;
    } else {
        goto ERROUT;
    }

    return num;

ERROUT:
    PRINT_ERR("Invalid value string \"%s\"!\n", value);
    return num;
}