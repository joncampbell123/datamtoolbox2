
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
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
	int sz;

	if (p_bs == NULL) return 0;

	/* check the size of the JMP instruction. it can't be FAT32 if it implies that the struct is less than 90 bytes. */
	sz = libmsfat_bs_struct_length(p_bs);
	if (sz < 90) return 0;

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
	int sz;

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

	/* does it jump out of range? */
	/* NOTE: MS-DOS 1.x has a JMP between 41 and 49 but the boot sector is defined differently.
	 *       This code doesn't attempt to read those disks, because more heuristics are needed to
	 *       detect and support that revision of FAT. MS-DOS 3.31 seems to have a 42-byte BPB. */
	sz = libmsfat_bs_struct_length(p_bs);
	if (sz > 192 || sz < 42) FAIL("JMP instruction implies structure is too large or too small");

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
		/* NTS: the BPB is 54 bytes starting with MS-DOS 3.x. MS-DOS 2.x has executable code
		 *      at the same location and a 45 byte BPB, therefore TotSec32 does not exist. */
		tmp16 = le16toh(p_bs->BPB_common.BPB_TotSec16);
		if (sz >= 54) tmp32 = le32toh(p_bs->BPB_common.BPB_TotSec32);
		else tmp32 = 0;
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

int libmsfat_bs_struct_length(const struct libmsfat_bootsector *p_bs) {
	if (p_bs->BS_header.BS_jmpBoot[0] == 0xEB && p_bs->BS_header.BS_jmpBoot[2] == 0x90) {
		// 0xEB <rel> 0x90
		// aka
		// JMP short <offset>
		// NOP
		// Microsoft documentation officially says the JMP instruction is followed by a NOP.
		//
		// JMP short encoding:
		// 0xEB <offset>
		//
		// The offset becomes boot sector + 2 because JMP short in Intel instruction encoding
		// is relative to the end of the JMP instruction.
		return (int)((unsigned int)p_bs->BS_header.BS_jmpBoot[1] + 2);
	}
	else if (p_bs->BS_header.BS_jmpBoot[0] == 0xE9) {
		// 0xE9 <two byte offset>
		// aka
		// JMP near <offset>
		//
		// JMP near encoding
		// 0xE9 <16-bit relative offset>
		//
		// The offset becomes boot sector + 3 because JMP short is relative to end of JMP isntruction
		return (int)((unsigned int)le16toh(*((uint16_t*)(p_bs->BS_header.BS_jmpBoot+1))) + 2);
	}

	return -1;
}

/* NTS: this function assumes you have already checked the BPB is FAT12/FAT16 */
int libmsfat_bs_fat1216_bootsig_present(const struct libmsfat_bootsector *p_bs) {
	int sz;

	if (p_bs == NULL) return 0;
	sz = libmsfat_bs_struct_length(p_bs);
	if (sz < 54) return 0;
	return (p_bs->at36.BPB_FAT.BS_BootSig == 0x29)?1:0;
}

/* NTS: function assumes you have already checked if bootsig is present and BPB is FAT12/FAT16! */
int libmsfat_bs_fat1216_BS_VolID_exists(const struct libmsfat_bootsector *p_bs) {
	int sz;

	if (p_bs == NULL) return 0;
	sz = libmsfat_bs_struct_length(p_bs);
	return (sz >= (39+4))?1:0;
}

/* NTS: function assumes you have already checked if bootsig is present and BPB is FAT12/FAT16! */
int libmsfat_bs_fat1216_BS_VolLab_exists(const struct libmsfat_bootsector *p_bs) {
	int sz;

	if (p_bs == NULL) return 0;
	sz = libmsfat_bs_struct_length(p_bs);
	return (sz >= (43+11))?1:0;
}

/* NTS: function assumes you have already checked if bootsig is present and BPB is FAT12/FAT16! */
int libmsfat_bs_fat1216_BS_FilSysType_exists(const struct libmsfat_bootsector *p_bs) {
	int sz;

	if (p_bs == NULL) return 0;
	sz = libmsfat_bs_struct_length(p_bs);
	return (sz >= (54+8))?1:0;
}

/* NTS: this function assumes you have already checked the BPB is FAT12/FAT16 */
int libmsfat_bs_fat1216_BS_BootSig_present(const struct libmsfat_bootsector *p_bs) {
	int sz;

	if (p_bs == NULL) return 0;
	sz = libmsfat_bs_struct_length(p_bs);
	return (sz >= 54)?1:0; /* MS-DOS 3.x or higher */
}

/* NTS: this function assumes you have already checked the BPB is FAT12/FAT16 */
int libmsfat_bs_fat1216_BPB_TotSec32_present(const struct libmsfat_bootsector *p_bs) {
	int sz;

	if (p_bs == NULL) return 0;
	sz = libmsfat_bs_struct_length(p_bs);
	return (sz >= 54)?1:0; /* MS-DOS 3.x or higher */
}

// NTS: This function assumes you have already validates the boot sector is valid!
int libmsfat_bs_compute_disk_locations(struct libmsfat_disk_locations_and_info *nfo,const struct libmsfat_bootsector *p_bs) {
	if (nfo == NULL || p_bs == NULL) return -1;
	memset(nfo,0,sizeof(*nfo));

	nfo->BytesPerSector = le16toh(p_bs->BPB_common.BPB_BytsPerSec);
	if (nfo->BytesPerSector == 0 || nfo->BytesPerSector > 4096) return -1;

	// NTS: Despite Microsoft docs, this code *could* support non-power-of-2 sector/cluster values
	nfo->Sectors_Per_Cluster = p_bs->BPB_common.BPB_SecPerClus;
	if (nfo->Sectors_Per_Cluster == 0) return -1;

	if (p_bs->BPB_common.BPB_RsvdSecCnt == 0) return -1;

	/* Microsoft official docs: If TotSec16 != 0, then use 16-bit value, else use TotSec32 */
	if (p_bs->BPB_common.BPB_TotSec16 != 0)
		nfo->TotalSectors = le16toh(p_bs->BPB_common.BPB_TotSec16);
	else if (libmsfat_bs_fat1216_BPB_TotSec32_present(p_bs))
		nfo->TotalSectors = le32toh(p_bs->BPB_common.BPB_TotSec32);
	else
		nfo->TotalSectors = (uint32_t)0;
	if (nfo->TotalSectors == 0)
		return -1;

	/* Microsoft official docs: If FATSz16 != 0, then use 16-bit value, else use FATSz32 */
	if (p_bs->BPB_common.BPB_FATSz16 != 0)
		nfo->FAT_table_size = le16toh(p_bs->BPB_common.BPB_FATSz16);
	else if (libmsfat_bs_is_fat32(p_bs))
		nfo->FAT_table_size = le32toh(p_bs->at36.BPB_FAT32.BPB_FATSz32);
	else
		nfo->FAT_table_size = 0;
	if (nfo->FAT_table_size == 0)
		return -1;

	nfo->FAT_offset = le16toh(p_bs->BPB_common.BPB_RsvdSecCnt);
	nfo->FAT_tables = p_bs->BPB_common.BPB_NumFATs;
	// NTS: root dir size calculation is valid for FAT32 as well
	nfo->RootDirectory_size = (((uint32_t)le16toh(p_bs->BPB_common.BPB_RootEntCnt) * (uint32_t)32) + (uint32_t)nfo->BytesPerSector - (uint32_t)1) / (uint32_t)nfo->BytesPerSector;
	// NTS: We will clear the Root Directory offset/size fields if FAT32 later on
	nfo->RootDirectory_offset = nfo->FAT_offset + ((uint32_t)nfo->FAT_tables * (uint32_t)nfo->FAT_table_size) + nfo->RootDirectory_size; 
	nfo->Data_offset = nfo->RootDirectory_offset + nfo->RootDirectory_size;

	// how big is the data area?
	nfo->Data_size = nfo->TotalSectors;
	if (nfo->Data_size <= nfo->Data_offset) return -1; // at least one sector!
	nfo->Data_size -= nfo->Data_offset;

	// how many clusters? this is vital to determining FAT type.
	nfo->Total_clusters = nfo->Data_size / nfo->Sectors_Per_Cluster;
	if (nfo->Total_clusters == (uint32_t)0) return -1;

	// So, what's the FAT type? (based on official Microsoft docs)
	if (nfo->Total_clusters < 4085)
		nfo->FAT_size = 12;
	else if (nfo->Total_clusters < 65525)
		nfo->FAT_size = 16;
	else
		nfo->FAT_size = 32;

	if (nfo->FAT_size == 32) {
		if (nfo->RootDirectory_size != (uint32_t)0) return -1; // Hey!
		nfo->RootDirectory_offset = (uint32_t)0;
		nfo->fat32.RootDirectory_cluster = le32toh(p_bs->at36.BPB_FAT32.BPB_RootClus);
		nfo->fat32.BPB_FSInfo = le32toh(p_bs->at36.BPB_FAT32.BPB_FSInfo);
	}

	return 0;
}
