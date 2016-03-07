/*
 * Copyright (C) 2015-2016 The PASTA Team
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

#include "decryptor/nand.h"
#include "fatfs/ff.h"
#include "cfw.h"
#include "common.h"
#include "draw.h"
#include "elf.h"
#include "hid.h"
#include "platform.h"
#include "reboot.h"

static int loadFirm(TCHAR *path, UINT *fsz)
{
	FIL fd;
	FRESULT r;

	r = f_open(&fd, path, FA_READ);
	if (r != FR_OK)
		return r;

	r = f_read(&fd, (void *)FIRM_ADDR, f_size(&fd), fsz);
	if (r != FR_OK)
		return r;

	f_close(&fd);

	return ((FirmHdr *)FIRM_ADDR)->magic == 0x4D524946 ? 0 : -1;
}

u32 cfw(u32 param)
{
	const Elf32_Addr line = 32;
	char path[_MAX_LFN + 1];
	const char *shstrtab;
	unsigned int tid;
	int r;
	Elf32_Ehdr *ehdr;
	Elf32_Shdr *shdr, *btmShdr;
	Elf32_Addr cur, btm;
	FIL fd;
	UINT br, fsz;

	r = GetUnitPlatform();
	switch (r) {
		case PLATFORM_N3DS:
			tid = TID_KTR_NATIVE_FIRM;
			break;

		case PLATFORM_3DS:
			tid = TID_CTR_NATIVE_FIRM;
			break;

		default:
			Debug("Unknown Platform: %d", r);
			return 1;
	}

	sprintf(path, "/" FIRM_PATH_FMT, TID_HI_FIRM, tid);
	r = loadFirm(path, &fsz);
	if (r) {
		Debug("Failed to load NATIVE_FIRM: %d", r);
		return r;
	}

	((FirmHdr *)FIRM_ADDR)->arm9Entry = 0x0801B01C;

	sprintf(path, "/" FIRM_PATCH_PATH_FMT, TID_HI_FIRM, tid);
	r = f_open(&fd, path, FA_READ);
	if (r != FR_OK)
		goto patchFail;

	r = f_read(&fd, (void *)PATCH_ADDR, PATCH_SIZE, &br);
	if (r != FR_OK)
		goto patchFail;

	f_close(&fd);

	ehdr = (void *)PATCH_ADDR;
	shdr = (void *)(PATCH_ADDR + ehdr->e_shoff);
	shstrtab = (char *)PATCH_ADDR + shdr[ehdr->e_shstrndx].sh_offset;
	for (btmShdr = shdr + ehdr->e_shnum; shdr != btmShdr; shdr++) {
		if (!strcmp(shstrtab + shdr->sh_name, ".patch.p9.reboot.body")) {
			memcpy((void *)ehdr->e_entry,
				(void *)(PATCH_ADDR + shdr->sh_offset),
				shdr->sh_size);
/*
			// Drain write buffer
			__asm__ volatile ("mcr p15, 0, %0, c7, c10, 4" :: "r"(0));

			cur = ehdr->e_entry & ~(line - 1);
			btm = ehdr->e_entry + shdr->sh_size;
			while (cur < btm) {
				__asm__ volatile (
					// Clean Dcache
					"mcr p15, 0, %0, c7, c10, 1\n\t"

					// Flush Icache
					"mcr p15, 0, %0, c7, c5, 1"
					:: "r"(cur));

				cur += line;
			}
*/

			((void (*)(uint32_t, void *, uintptr_t))ehdr->e_entry)(
				emunand_header * NAND_SECTOR_SIZE, NULL, 0x1FFFFFF8);

			__builtin_unreachable();
		}
	}

	Debug(".patch.p9.reboot.body not found");
	return 1;

patchFail:
	Debug("Failed to load the patch: %d", r);
	return r;
}
