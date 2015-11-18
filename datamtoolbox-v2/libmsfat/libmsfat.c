
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include <datamtoolbox-v2/libmsfat/libmsfat.h>

int libmsfat_sanity_check() {
	struct libmsfat_bootsector bs;

	if (sizeof(bs) != 90) return -1;
	if (sizeof(bs.BS_header) != 11) return -1;
	if (sizeof(bs.BPB_common) != 25) return -1;
	if (sizeof(bs.at36.BPB_FAT) != 26) return -1;
	if (sizeof(bs.at36.BPB_FAT32) != 54) return -1;
	if (sizeof(bs.at36) != 54) return -1;
	if (offsetof(struct libmsfat_bootsector,BS_header) != 0) return -1;
	if (offsetof(struct libmsfat_bootsector,BPB_common) != 11) return -1;
	if (offsetof(struct libmsfat_bootsector,at36) != 36) return -1;
	if (offsetof(struct libmsfat_bootsector,at36.BPB_FAT) != 36) return -1;
	if (offsetof(struct libmsfat_bootsector,at36.BPB_FAT32) != 36) return -1;

	return 0;
}

int libmsfat_bs_is_fat32(const struct libmsfat_bootsector *p_bs) {
	if (p_bs == NULL) return 0;

	/* NTS: No byte swapping is needed to compare against zero.
	 * We're checking for values that, if zero, would render FAT12/FAT16 unreadable to systems that do not understand FAT32.
	 * Especially important is the FATSz16 field. */
	if (p_bs->BPB_common.BPB_RootEntCnt == 0U && p_bs->BPB_common.BPB_TotSec16 == 0U && p_bs->BPB_common.BPB_FATSz16 == 0U)
		return 1;

	return 0;
}

int libmsfat_bs_is_valid(const struct libmsfat_bootsector *p_bs,const char **err_str) {
	uint32_t tmp32;
	uint16_t tmp16;
	uint8_t tmp8;

#define WARN(x) do {\
	fprintf(stderr,"libmsfat warning: %s\n",(x)); \
	} while(0)
#define FAIL(x) do {\
	if (err_str) *err_str = x; \
	return 0; \
	} while(0)

	if (p_bs == NULL) FAIL("p_bs==NULL");

	/* "Jump instruction to boot code" */
	if (p_bs->BS_header.BS_jmpBoot[0] == 0xEB && p_bs->BS_header.BS_jmpBoot[2] == 0x90)
		{ /* ok */ }
	else if (p_bs->BS_header.BS_jmpBoot[0] == 0xE9)
		{ /* ok */ }
	else
		FAIL("Boot sector must start with recognizeable JMP opcode");

	/* Count of bytes per sector. This value may take on only the following values: 512, 1024, 2048, 4096 */
	tmp16 = le16toh(p_bs->BPB_common.BPB_BytsPerSec);
	if (!(tmp16 == 512 || tmp16 == 1024 || tmp16 == 2048 || tmp16 == 4096))
		FAIL("BPB_BytsPerSec invalid value");

	/* Number of sectors per allocation unit. Must be a nonzero power of 2. */
	tmp8 = p_bs->BPB_common.BPB_SecPerClus;
	if (!(tmp8 == 1 || tmp8 == 2 || tmp8 == 4 || tmp8 == 8 || tmp8 == 16 || tmp8 == 32 || tmp8 == 64 || tmp8 == 128))
		FAIL("BPB_SecPerClus must be nonzero and a power of 2");

	/* bytes per sector x sectors per cluster must not exceed 32KB by standard.
	 * some allow 64KB per cluster. we tenatively allow it, but disallow larger blocks */
	{
		unsigned long tmp = (unsigned long)p_bs->BPB_common.BPB_SecPerClus *
			(unsigned long)le16toh(p_bs->BPB_common.BPB_BytsPerSec);

		if (tmp > 0x10000UL) FAIL("BPB_BytsPerSec * BPB_SecPerClus exceeds 64KB");
	}

	/* Reserved sector count. Must never be zero */
	tmp16 = le16toh(p_bs->BPB_common.BPB_RsvdSecCnt);
	if (tmp16 == 0) FAIL("BPB_RsvdSecCnt == 0");

	/* Count of FAT data structures */
	tmp8 = p_bs->BPB_common.BPB_NumFATs;
	if (tmp8 == 0 || tmp8 > 15) FAIL("BPB_NumFATs zero or out of range");

	/* media type byte */
	tmp8 = p_bs->BPB_common.BPB_Media;
	if (!(tmp8 == 0xF0 || tmp8 >= 0xF8)) FAIL("BPB_Media not a valid type");

	/* next, our tests diverge depending on FAT12/FAT16/FAT32 */
	if (libmsfat_bs_is_fat32(p_bs)) {
		/* number of entries in the root directory */
		tmp16 = le16toh(p_bs->BPB_common.BPB_RootEntCnt);
		if (tmp16 != 0) FAIL("BPB_RootEntCnt != 0 [FAT32]");

		/* total sector count. for FAT32, this must be zero. */
		tmp16 = le16toh(p_bs->BPB_common.BPB_TotSec16);
		if (tmp16 != 0) FAIL("BPB_TotSec16 != 0 [FAT32]");

		/* Number of sectors occupied by one FAT */
		tmp16 = le16toh(p_bs->BPB_common.BPB_FATSz16);
		if (tmp16 != 0) FAIL("BPB_FATSz16 != 0 [FAT32]");
		tmp32 = le32toh(p_bs->at36.BPB_FAT32.BPB_FATSz32);
		if (tmp32 == 0) FAIL("BPB_FATSz32 == 0 [FAT32]");

		/* Version number (hibyte=major lowbyte=minor) */
		tmp16 = le16toh(p_bs->at36.BPB_FAT32.BPB_FSVer);
		if (tmp16 != 0) FAIL("BPB_FSVer unrecognized version [FAT32]\n");

		/* cluster number where the root directory starts */
		tmp32 = le32toh(p_bs->at36.BPB_FAT32.BPB_RootClus);
		if (tmp32 < 2) FAIL("BPB_RootClus is zero or invalid cluster value [FAT32]\n");
	}
	else {
		/* number of entries in the root directory */
		tmp16 = le16toh(p_bs->BPB_common.BPB_RootEntCnt);
		if (tmp16 == 0) FAIL("BPB_RootEntCnt == 0 [FAT12/16]");

		/* total sector count */
		tmp16 = le16toh(p_bs->BPB_common.BPB_TotSec16);
		tmp32 = le32toh(p_bs->BPB_common.BPB_TotSec32);
		if (tmp32 != 0) {
			// expected: BPB_TotSec32 != 0 and BPB_TotSec16 == 0
			// also:     BPB_TotSec32 != 0, BPB_TotSec32 < 65536, BPB_TotSec16 == BPB_TotSec32
			if (tmp16 != 0) {
				if (tmp32 >= 0xFFFFUL)
					WARN("BPB_TotSec32 != 0 and >= 64K, BPB_TotSec16 nonzero");
				else if (tmp32 <= 0xFFFFUL && tmp32 != (uint32_t)tmp16)
					WARN("BPB_TotSec32 != 0 and less than 64K, BPB_TotSec16 != BPB_TotSec32");
			}
		}
		else {
			if (tmp16 == 0) FAIL("BPB_TotSec16 == 0 and BPB_TotSec32 == 0");
		}

		/* Number of sectors occupied by one FAT */
		tmp16 = le16toh(p_bs->BPB_common.BPB_FATSz16);
		if (tmp16 == 0) FAIL("BPB_FATSz16 == 0 [FAT12/16]");
	}

	if (err_str) *err_str = NULL;
	return 1;
#undef WARN
#undef FAIL
}

int libmsfat_boot_sector_is_valid(const unsigned char *sector/*512 bytes*/,const char **err_str) {
#define FAIL(x) {\
	if (err_str) *err_str = x; \
	return 0; \
	}

	if (sector == NULL) FAIL("sector==NULL");
	if (sector[0x1FE] != 0x55 || sector[0x1FF] != 0xAA) FAIL("Signature at the end of the boot sector is not 0x55 0xAA");

	{
		const struct libmsfat_bootsector *p_bs = (const struct libmsfat_bootsector*)sector;
		return libmsfat_bs_is_valid(p_bs,err_str);
	}
#undef FAIL
}

