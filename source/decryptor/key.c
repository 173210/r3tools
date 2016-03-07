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
#include "common.h"
#include "draw.h"
#include "fs.h"

u32 LoadKeyXFromFile(u32 keyslot)
{
    char filename[32];
    u8 keyX[16] = {0};
    
    snprintf(filename, 31, WORK_DIR "/key/%02Xh.bin", (unsigned int) keyslot);
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
