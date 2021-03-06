/*
 * Copyright (C) 2015 The PASTA Team
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

#include <stddef.h>
#include <stdint.h>
#include "reboot.h"
#include "elf.h"
#include <emunand.h>

#define SET_MPU_REGION(id, base, size, enable)	{	\
	__asm__ volatile ("mcr p15, 0, %0, c6, c" #id ", 0\n"	\
		:: "r"(((base) & 0xFFFFF000) | ((size) & 0x3E) | ((enable) ? 1 : 0)));	\
	}

#define PERM_DATA "2"
#define PERM_INSTR "3"
#define READ_MPU_PERM(v, type)	{	\
	__asm__("mrc p15, 0, %0, c5, c0, " type "\n" : "=r"(*(v)));	\
}
#define WRITE_MPU_PERM(v, type)	{	\
	__asm__("mcr p15, 0, %0, c5, c0, " type "\n" :: "r"(v));	\
}

static void SET_MPU_PERM_RWFORALL(uint32_t *v, uint8_t area)
{
	const uint8_t bits = 4;
	uint8_t shift;

	shift = area * bits;
	// 3 means the area is readable and writable for everyone.
	*v = (*v & ~(((1 << bits) - 1) << shift)) | (3 << shift);
}

#define MPU_DCACHE "0"
#define MPU_ICACHE "1"
#define FETCH "c2"
#define WRITE "c3"
#define READ_MPU_CACHABLE(v, id, type) {	\
	__asm__("mrc p15, 0, %0, " type ", c0, " id "\n" : "=r"(*(v)));	\
}
#define WRITE_MPU_CACHABLE(v, id, type) {	\
	__asm__ volatile ("mcr p15, 0, %0, " type ", c0, " id "\n" :: "r"(v));	\
}

static void drainWriteBuffer()
{
	__asm__ volatile ("mcr p15, 0, %0, c7, c10, 4\n" :: "r"(0));
}

static void cleanDcacheLine(void *p)
{
	__asm__ volatile ("mcr p15, 0, %0, c7, c10, 1\n" :: "r"(p));
}

static void flushIcacheLine(void *p)
{
	__asm__ volatile ("mcr p15, 0, %0, c7, c5, 1\n" :: "r"(p));
}

static void setupMpu()
{
	uint32_t i;

	SET_MPU_REGION(3, 0x10000000, 52, 1);

	READ_MPU_PERM(&i, PERM_DATA);
	SET_MPU_PERM_RWFORALL(&i, 4);
	WRITE_MPU_PERM(i, PERM_DATA);

	READ_MPU_PERM(&i, PERM_INSTR);
	SET_MPU_PERM_RWFORALL(&i, 4);
	WRITE_MPU_PERM(i, PERM_INSTR);

	SET_MPU_REGION(4, 0x18000000, 52, 1);

	READ_MPU_CACHABLE(&i, MPU_DCACHE, FETCH);
	WRITE_MPU_CACHABLE(i | (1 << 4) | (1 << 5), MPU_DCACHE, FETCH);

	READ_MPU_CACHABLE(&i, MPU_ICACHE, FETCH);
	WRITE_MPU_CACHABLE(i | (1 << 4) | (1 << 5), MPU_ICACHE, FETCH);

	READ_MPU_CACHABLE(&i, MPU_DCACHE, WRITE);
	WRITE_MPU_CACHABLE(i | (1 << 4) | (1 << 5), MPU_DCACHE, WRITE);
}

static void *memcpy16(void *dst, const void *src, size_t n)
{
	const uint16_t *_src;
	uint16_t *_dst;
	uintptr_t btm;

	_dst = dst;
	_src = src;
	btm = (uintptr_t)dst + n;
	while ((uintptr_t)_dst < btm) {
		*_dst = *_src;
		_dst++;
		_src++;
	}

	return _dst;
}

static void *memcpy32(void *dst, const void *src, size_t n)
{
	const uint32_t *_src;
	uint32_t *_dst;
	uintptr_t btm;

	_dst = dst;
	_src = src;
	btm = (uintptr_t)dst + n;
	while ((uintptr_t)_dst < btm) {
		*_dst = *_src;
		_dst++;
		_src++;
	}

	return _dst;
}

static void loadFirm()
{
	const FirmSeg *seg;
	unsigned int i;

	seg = ((FirmHdr *)FIRM_ADDR)->segs;
	for (i = 0; i < FIRM_SEG_NUM; i++) {
		memcpy32((void *)seg->addr, (void *)FIRM_ADDR + seg->offset, seg->size);
		seg++;
	}
}

static int memcmp(const void *s1, const void *s2, size_t n)
{
	int d;

	while (n > 0) {
		d = *(unsigned char *)s1 - *(unsigned char *)s2;
		if (d)
			return d;

		s1 = (unsigned char *)s1 + 1;
		s2 = (unsigned char *)s2 + 1;
		n--;
	}

	return 0;
}

static void patchFirm(uint32_t sector)
{
	static const char patchNandPrefix[] = ".patch.p9.nand";
#ifndef PLATFORM_KTR
	static const char patchKeyxStr[] = ".patch.p9.keyx";
#endif
	const Elf32_Ehdr *ehdr;
	const Elf32_Shdr *shdr, *btm;
	const char *shstrtab, *sh_name;
	uintptr_t dst, src;

	ehdr = (void *)PATCH_ADDR;
	shdr = (void *)(PATCH_ADDR + ehdr->e_shoff);
	shstrtab = (char *)PATCH_ADDR + shdr[ehdr->e_shstrndx].sh_offset;
	for (btm = shdr + ehdr->e_shnum; shdr != btm; shdr++) {
		if (!(shdr->sh_flags & SHF_ALLOC) || shdr->sh_type != SHT_PROGBITS)
			continue;

		sh_name = shstrtab + shdr->sh_name;
		if (sector <= 0 && !memcmp(sh_name, patchNandPrefix, sizeof(patchNandPrefix) - 1))
			continue;

		memcpy16((void *)shdr->sh_addr,
			(void *)PATCH_ADDR + shdr->sh_offset,
			shdr->sh_size);
	}

	if (sector > 0)
		nandSector = sector;
}

static void flushFirmData()
{
	uintptr_t dstCur, dstBtm;
	const FirmSeg *seg;
	unsigned int i;

	seg = ((FirmHdr *)FIRM_ADDR)->segs;
	for (i = 0; i < FIRM_SEG_NUM; i++) {
		dstCur = seg->addr;
		for (dstBtm = seg->addr + seg->size; dstCur < dstBtm; dstCur += 32)
			cleanDcacheLine((void *)dstCur);

		seg++;
	}

	drainWriteBuffer();
}

static void flushFirmInstr()
{
	uintptr_t dstCur, dstBtm;
	const FirmSeg *seg;
	unsigned int i;

	seg = ((FirmHdr *)FIRM_ADDR)->segs;
	for (i = 0; i < FIRM_SEG_NUM; i++) {
		if (!seg->isArm11) {
			dstCur = seg->addr;
			for (dstBtm = seg->addr + seg->size; dstCur < dstBtm; dstCur += 32)
				flushIcacheLine((void *)dstCur);
		}

		seg++;
	}
}

static void arm11Enter(uint32_t *arm11EntryDst)
{
	*arm11EntryDst = ((FirmHdr *)FIRM_ADDR)->arm11Entry;
	cleanDcacheLine(arm11EntryDst);
	drainWriteBuffer();
}

static _Noreturn void arm9Enter()
{
	__asm__ volatile ("ldr pc, %0\n" :: "m"(((FirmHdr *)FIRM_ADDR)->arm9Entry));
	__builtin_unreachable();
}

_Noreturn void __attribute__((section(".text.start")))
rebootFunc(uint32_t sector, uint32_t *arm11EntryDst)
{
	setupMpu();
	loadFirm();
	patchFirm(sector);
	flushFirmData();
	arm11Enter(arm11EntryDst);
	flushFirmInstr();
	arm9Enter();
}
