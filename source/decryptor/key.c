/*
 * This was part of Decrypt9WIP, written by Archshit, d03k and others.
 * Copyright (C) 2016 173210 <root.3.173210@live.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "decryptor/aes.h"
#include "fatfs/ff.h"
#include "common.h"
#include "draw.h"
#include "fs.h"

static const char dir[] = WORK_DIR "/key";

#define SUFFIX "H.BIN"

u32 LoadKeyXFromFile(u32 keyslot)
{
    char filename[32];
    u8 keyX[16] = {0};
    
    snprintf(filename, 31, "%s/%02X" SUFFIX, dir, (unsigned int) keyslot);
    if (!FileOpen(filename)) {
        Debug("Loading %s: not found", filename);
        return 1;
    }
    if (FileRead(keyX, 16, 0) != 16) {
        Debug("Loading %s: bad file", filename);
        FileClose();
        return 1;
    }
    FileClose();
    setup_aeskeyX(keyslot, keyX);
    Debug("Loading %s: ok", filename);
    
    return 0;
}

FRESULT LoadAllKeyXFromFile()
{
    char fpath[_MAX_LFN + 1];
    char *p;
    long keyslot;
    u8 key[AES_BLOCK_SIZE];
    FILINFO fno;
    FRESULT r;
    FIL f;
    DIR d;
    UINT br;

    r = f_opendir(&d, dir);
    if (r == FR_NO_PATH)
        return FR_OK;
    else if (r != FR_OK)
        return r;

    fno.lfname = NULL;

    while (1) {
        r = f_readdir(&d, &fno);
        if (r != FR_OK) {
            Debug("Failed to read directory %s: %d", dir, r);
            f_closedir(&d);

            return r;
        }

	if (fno.fname[0] == 0)
		break;

        keyslot = strtol(fno.fname, &p, 16);
        if (strcmp(p, SUFFIX))
            continue;

        snprintf(fpath, _MAX_LFN, "%s/%s", dir, fno.fname);
        r = f_open(&f, fpath, FA_READ);
        if (r) {
            Debug("Failed to open %s: %d", fpath, r);
            f_closedir(&d);

            return r;
        }

        r = f_read(&f, key, sizeof(key), &br);
        if (r) {
            Debug("Failed to read %s: %d", fpath, r);
            f_close(&f);
            f_closedir(&d);
            return r;
        }

        if (br < sizeof(key)) {
            Debug("%s is too small. expected: %d, result: %d",
                  fpath, br, sizeof(key));
            f_close(&f);
            f_closedir(&d);

            return -1;
        }

        f_close(&f);
        setup_aeskeyX(keyslot, key);
    }

    f_closedir(&d);
    return FR_OK;
}
