#if defined(_MSC_VER)
# include <datamtoolbox-v2/polyfill/ms_posix_stfu.h>
#endif
#if !defined(_MSC_VER)
# include <unistd.h>
# include <endian.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#if defined(_MSC_VER)
# include <datamtoolbox-v2/polyfill/ms_cpp.h>
#endif
#include <datamtoolbox-v2/polyfill/lseek.h>
#include <datamtoolbox-v2/polyfill/unix.h>

#include <datamtoolbox-v2/libmsfat/libmsfat.h>

int libmsfat_sanity_check() {
	struct libmsfat_msdos_time_t time_e;
	struct libmsfat_msdos_date_t date_e;
	struct libmsfat_bootsector bs;
	struct libmsfat_dirent_t de;

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
	if (sizeof(de) != 32) return -1;
	if (sizeof(de.a) != 32) return -1;
	if (sizeof(de.a.n) != 32) return -1;
	if (sizeof(de.a.lfn) != 32) return -1;
	if (sizeof(time_e) != 2) return -1;
	if (sizeof(date_e) != 2) return -1;
	if (sizeof(struct libmsfat_fat32_fsinfo_t) != 512) return -1;

	/* bitfield tests */
	time_e.a.raw = 0x01 << 0;
	if (time_e.a.f.seconds2 != 0x01 || time_e.a.f.minutes != 0 || time_e.a.f.hours != 0) return -1;
	time_e.a.raw = 0x1F << 0;
	if (time_e.a.f.seconds2 != 0x1F || time_e.a.f.minutes != 0 || time_e.a.f.hours != 0) return -1;
	time_e.a.raw = 0x01 << 5;
	if (time_e.a.f.seconds2 != 0 || time_e.a.f.minutes != 0x01 || time_e.a.f.hours != 0) return -1;
	time_e.a.raw = 0x3F << 5;
	if (time_e.a.f.seconds2 != 0 || time_e.a.f.minutes != 0x3F || time_e.a.f.hours != 0) return -1;
	time_e.a.raw = 0x01 << 11;
	if (time_e.a.f.seconds2 != 0 || time_e.a.f.minutes != 0 || time_e.a.f.hours != 0x01) return -1;
	time_e.a.raw = 0x1F << 11;
	if (time_e.a.f.seconds2 != 0 || time_e.a.f.minutes != 0 || time_e.a.f.hours != 0x1F) return -1;

	/* bitfield tests */
	date_e.a.raw = 0x01 << 0;
	if (date_e.a.f.day_of_month != 0x01 || date_e.a.f.month_of_year != 0 || date_e.a.f.years_since_1980 != 0) return -1;
	date_e.a.raw = 0x1F << 0;
	if (date_e.a.f.day_of_month != 0x1F || date_e.a.f.month_of_year != 0 || date_e.a.f.years_since_1980 != 0) return -1;
	date_e.a.raw = 0x01 << 5;
	if (date_e.a.f.day_of_month != 0 || date_e.a.f.month_of_year != 0x01 || date_e.a.f.years_since_1980 != 0) return -1;
	date_e.a.raw = 0x0F << 5;
	if (date_e.a.f.day_of_month != 0 || date_e.a.f.month_of_year != 0x0F || date_e.a.f.years_since_1980 != 0) return -1;
	date_e.a.raw = 0x01 << 9;
	if (date_e.a.f.day_of_month != 0 || date_e.a.f.month_of_year != 0 || date_e.a.f.years_since_1980 != 0x01) return -1;
	date_e.a.raw = 0x7F << 9;
	if (date_e.a.f.day_of_month != 0 || date_e.a.f.month_of_year != 0 || date_e.a.f.years_since_1980 != 0x7F) return -1;

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
	nfo->RootDirectory_offset = nfo->FAT_offset + ((uint32_t)nfo->FAT_tables * (uint32_t)nfo->FAT_table_size);
	nfo->Data_offset = nfo->RootDirectory_offset + nfo->RootDirectory_size;

	// how big is the data area?
	nfo->Data_size = nfo->TotalSectors;
	if (nfo->Data_size <= nfo->Data_offset) return -1; // at least one sector!
	nfo->Data_size -= nfo->Data_offset;

	// how many clusters? this is vital to determining FAT type.
	nfo->Total_data_clusters = nfo->Data_size / nfo->Sectors_Per_Cluster;
	if (nfo->Total_data_clusters == (uint32_t)0) return -1;

	// So, what's the FAT type? (based on official Microsoft docs)
	if (nfo->Total_data_clusters < (uint32_t)4085UL)
		nfo->FAT_size = 12;
	else if (nfo->Total_data_clusters < (uint32_t)65525UL)
		nfo->FAT_size = 16;
	else
		nfo->FAT_size = 32;

	if (nfo->FAT_size == 32) {
		if (nfo->RootDirectory_size != (uint32_t)0) return -1; // Hey!
		nfo->RootDirectory_offset = (uint32_t)0;
		nfo->fat32.RootDirectory_cluster = le32toh(p_bs->at36.BPB_FAT32.BPB_RootClus);
		nfo->fat32.BPB_FSInfo = le32toh(p_bs->at36.BPB_FAT32.BPB_FSInfo);
	}

	nfo->Max_possible_clusters = nfo->FAT_table_size * (uint32_t)nfo->BytesPerSector;
	if (nfo->FAT_size == 12) {
		nfo->Max_possible_clusters /= 3UL;
		nfo->Max_possible_clusters *= 2UL;
	}
	else {
		nfo->Max_possible_clusters /= (nfo->FAT_size / (uint32_t)8UL);
	}

	// remember that clusters with data start at cluster #2. that means
	// the total clusters is +2 the number we computed, because that's the
	// number of FAT entries needed to represent the total data clusters.
	nfo->Total_clusters = nfo->Total_data_clusters + (uint32_t)2UL;

	// what we computed from the FAT table is the maximum possible cluster.
	// not counting clusters 0 and 1, the max possible data cluster is max cluster - 2.
	if (nfo->Max_possible_clusters >= 2UL)
		nfo->Max_possible_data_clusters = nfo->Max_possible_clusters - (uint32_t)2UL;
	else
		nfo->Max_possible_data_clusters = (uint32_t)0UL;

	return 0;
}

// NTS: "cluster" is not range-checked from fatinfo Total Clusters value, in case you want to peep at the extra entries.
//      But this code does range-limit against the FAT table size.
// NTS: For FAT32, it is the caller's responsibility to strip off bits 28-31 before using as cluster number.
int libmsfat_context_read_FAT(struct libmsfat_context_t *r,libmsfat_FAT_entry_t *entry,const libmsfat_cluster_t cluster,unsigned int instance) {
	uint64_t offset;
	uint8_t rdsize;
	uint8_t buf[4];

	if (r == NULL || entry == NULL) return -1;
	if (!r->fatinfo_set) return -1;
	if (r->read == NULL) return -1;
	if (instance >= r->fatinfo.FAT_tables) return -1;
	// fatinfo_set means the FAT offset has been validated

	if (r->fatinfo.FAT_size == 12) {
		offset = (uint64_t)cluster + (uint64_t)(cluster / 2U); /* 1.5 bytes per entry -> 0,1,3,4,6,7,9,10... */
		rdsize = 2;
	}
	else {
		rdsize = (uint8_t)(r->fatinfo.FAT_size / 8U); /* FAT16/FAT32 -> 2/4 */
		offset = (uint64_t)cluster * (uint64_t)rdsize;
	}

	{//range check against the sector max
		uint64_t max = (uint64_t)r->fatinfo.FAT_table_size * (uint64_t)r->fatinfo.BytesPerSector;
		if ((offset+((uint64_t)rdsize)) > max) return -1;
	}

	// add the offset of the FAT table
	offset += (uint64_t)r->fatinfo.FAT_offset * (uint64_t)r->fatinfo.BytesPerSector;
	// and the partition offset
	offset += r->partition_byte_offset;
	// instance
	offset += (uint64_t)instance * (uint64_t)r->fatinfo.BytesPerSector * (uint64_t)r->fatinfo.FAT_table_size;

	if (r->read(r,buf,offset,(size_t)rdsize) != 0) return -1;

	if (r->fatinfo.FAT_size == 12) {
		uint16_t raw = *((uint16_t*)buf);
		uint16_t tmp = le16toh(raw);

		// if (cluster & 1)
		//   val = tmp >> 4
		// else
		//   val = tmp & 0xFFF;

		*entry = (libmsfat_FAT_entry_t)((tmp >> (((uint16_t)cluster & 1U) * 4U)) & 0xFFF);
	}
	else if (r->fatinfo.FAT_size == 16) {
		uint16_t raw = *((uint16_t*)buf);
		*entry = (libmsfat_FAT_entry_t)le16toh(raw);
	}
	else { // FAT_size == 32
		uint32_t raw = *((uint32_t*)buf);
		*entry = (libmsfat_FAT_entry_t)le32toh(raw);
	}

	return 0;
}

// NTS: "cluster" is not range-checked from fatinfo Total Clusters value, in case you want to peep at the extra entries.
//      But this code does range-limit against the FAT table size.
// NTS: For FAT32, it is the caller's responsibility to preserve bits 28-31 when modifying.
int libmsfat_context_write_FAT(struct libmsfat_context_t *r,libmsfat_FAT_entry_t entry,const libmsfat_cluster_t cluster,unsigned int instance) {
	uint64_t offset;
	uint8_t rdsize;
	uint8_t buf[4];

	if (r == NULL) return -1;
	if (!r->fatinfo_set) return -1;
	if (r->read == NULL) return -1;
	if (r->write == NULL) return -1;
	if (instance >= r->fatinfo.FAT_tables) return -1;
	// fatinfo_set means the FAT offset has been validated

	if (r->fatinfo.FAT_size == 12) {
		offset = (uint64_t)cluster + (uint64_t)(cluster / 2U); /* 1.5 bytes per entry -> 0,1,3,4,6,7,9,10... */
		rdsize = 2;
	}
	else {
		rdsize = (uint8_t)(r->fatinfo.FAT_size / 8U); /* FAT16/FAT32 -> 2/4 */
		offset = (uint64_t)cluster * (uint64_t)rdsize;
	}

	{//range check against the sector max
		uint64_t max = (uint64_t)r->fatinfo.FAT_table_size * (uint64_t)r->fatinfo.BytesPerSector;
		if ((offset+((uint64_t)rdsize)) > max) return -1;
	}

	// add the offset of the FAT table
	offset += (uint64_t)r->fatinfo.FAT_offset * (uint64_t)r->fatinfo.BytesPerSector;
	// and the partition offset
	offset += r->partition_byte_offset;
	// instance
	offset += (uint64_t)instance * (uint64_t)r->fatinfo.BytesPerSector * (uint64_t)r->fatinfo.FAT_table_size;

	if (r->fatinfo.FAT_size == 12) {
		uint16_t raw,tmp;

		// if (cluster & 1)
		//   val = tmp >> 4
		// else
		//   val = tmp & 0xFFF;

		if (r->read(r,buf,offset,(size_t)rdsize) != 0) return -1;

		raw = *((uint16_t*)buf);
		tmp = le16toh(raw);

		tmp &= ~((uint16_t)(0xFFF << (((uint16_t)cluster & 1U) * 4U)));
		tmp |=  ((uint16_t)entry & 0xFFF) << (((uint16_t)cluster & 1U) * 4U);

		raw = htole16(tmp);
		*((uint16_t*)buf) = raw;

		if (r->write(r,buf,offset,(size_t)rdsize) != 0) return -1;
	}
	else if (r->fatinfo.FAT_size == 16) {
		*((uint16_t*)buf) = htole16((uint16_t)entry);
		if (r->write(r,buf,offset,(size_t)rdsize) != 0) return -1;
	}
	else { // FAT_size == 32
		*((uint32_t*)buf) = htole32((uint32_t)entry);
		if (r->write(r,buf,offset,(size_t)rdsize) != 0) return -1;
	}

	return 0;
}

int libmsfat_context_set_fat_info(struct libmsfat_context_t *r,const struct libmsfat_disk_locations_and_info *nfo) {
	if (r == NULL || nfo == NULL) return -1;
	r->fatinfo_set = 0;

	if (!(nfo->FAT_size == 12 || nfo->FAT_size == 16 || nfo->FAT_size == 32)) return -1;
	if (nfo->FAT_tables == 0) return -1;
	if (nfo->BytesPerSector < 128) return -1;
	if (nfo->FAT_table_size == (uint32_t)0)  return -1;
	if (nfo->FAT_offset == (uint32_t)0) return -1;
	if (nfo->Data_offset == (uint32_t)0) return -1;
	if (nfo->Data_size == (uint32_t)0) return -1;
	if (nfo->Total_clusters == (uint32_t)0) return -1;
	if (nfo->TotalSectors == (uint32_t)0) return -1;
	if (nfo->Sectors_Per_Cluster == (uint32_t)0) return -1;
	r->fatinfo_set = 1;
	r->fatinfo = *nfo;
	return 0;
}

int libmsfat_context_def_fd_read(struct libmsfat_context_t *r,uint8_t *buffer,uint64_t offset,size_t len) {
	lseek_off_t ofs,res;
	int rd;

	if (r == NULL || buffer == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (r->user_fd < 0) {
		errno = EBADF;
		return -1;
	}
	if (len == (size_t)0) {
		return 0;
	}

	ofs = (lseek_off_t)offset;
	res = lseek64(r->user_fd,ofs,SEEK_SET);
	if (res < (lseek_off_t)0)
		return -1; // lseek() also sets errno
	else if (res != ofs) {
		errno = ERANGE;
		return -1;
	}

	rd = read(r->user_fd,buffer,(unsigned int)len);
	if (rd < 0)
		return -1; // read() also set errno
	else if ((size_t)rd != len) {
		errno = EIO;
		return -1;
	}

	return 0; // success
}

int libmsfat_context_def_fd_write(struct libmsfat_context_t *r,const uint8_t *buffer,uint64_t offset,size_t len) {
	lseek_off_t ofs,res;
	int rd;

	if (r == NULL || buffer == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (r->user_fd < 0) {
		errno = EBADF;
		return -1;
	}
	if (len == (size_t)0) {
		return 0;
	}

	ofs = (lseek_off_t)offset;
	res = lseek64(r->user_fd,ofs,SEEK_SET);
	if (res < (lseek_off_t)0)
		return -1; // lseek() also sets errno
	else if (res != ofs) {
		errno = ERANGE;
		return -1;
	}

	rd = write(r->user_fd,buffer,(unsigned int)len);
	if (rd < 0)
		return -1; // read() also set errno
	else if ((size_t)rd != len) {
		errno = EIO;
		return -1;
	}

	return 0; // success
}

int libmsfat_context_init(struct libmsfat_context_t *r) {
	if (r == NULL) return -1;
	memset(r,0,sizeof(*r));
	r->user_fd = -1;
#if defined(_WIN32)
	r->user_win32_handle = INVALID_HANDLE_VALUE;
#endif
	return 0;
}

void libmsfat_context_close_file(struct libmsfat_context_t *r) {
	if (r->user_free_cb != NULL) r->user_free_cb(r);
	r->user_ptr = NULL;
	if (r->user_fd >= 0) {
		close(r->user_fd);
		r->user_fd = -1;
	}
#if defined(_WIN32)
	if (r->user_win32_handle != INVALID_HANDLE_VALUE) {
		CloseHandle(r->user_win32_handle);
		r->user_win32_handle = INVALID_HANDLE_VALUE;
	}
#endif
}

void libmsfat_context_free(struct libmsfat_context_t *r) {
	if (r == NULL) return;
	libmsfat_context_close_file(r);
}

struct libmsfat_context_t *libmsfat_context_create() {
	struct libmsfat_context_t *r;

	r = malloc(sizeof(struct libmsfat_context_t));
	if (r != NULL && libmsfat_context_init(r)) {
		free(r);
		r = NULL;
	}

	return r;
}

struct libmsfat_context_t *libmsfat_context_destroy(struct libmsfat_context_t *r) {
	if (r != NULL) {
		libmsfat_context_free(r);
		free(r);
	}

	return NULL;
}

int libmsfat_context_assign_fd(struct libmsfat_context_t *r,const int fd) {
	if (fd < 0) return -1;
	if (r == NULL) return -1;

	libmsfat_context_close_file(r);
	r->user_fd = fd;

	/* if the user has not assigned read/write callbacks, then assign now */
	if (r->read == NULL)
		r->read = libmsfat_context_def_fd_read;
	if (r->write == NULL)
		r->write = libmsfat_context_def_fd_write;

	return 0;
}

int libmsfat_context_get_cluster_sector(struct libmsfat_context_t *ctx,uint64_t *sector,const libmsfat_cluster_t cluster) {
	uint64_t sct;

	if (ctx == NULL || sector == NULL) return -1;
	if (!ctx->fatinfo_set) return -1;

	// clusters 0 and 1 do not have corresponding data storage
	if (cluster < (libmsfat_cluster_t)2UL) return -1;

	sct = (uint64_t)(cluster - (libmsfat_cluster_t)2UL) * (uint64_t)ctx->fatinfo.Sectors_Per_Cluster;
	if (sct >= ctx->fatinfo.Data_size) return -1;
	sct += (uint64_t)ctx->fatinfo.Data_offset;

	*sector = sct;
	return 0;
}

int libmsfat_context_get_cluster_offset(struct libmsfat_context_t *ctx,uint64_t *offset,const libmsfat_cluster_t cluster) {
	uint64_t tmp;

	if (libmsfat_context_get_cluster_sector(ctx,&tmp,cluster)) return -1;
	*offset = ((uint64_t)tmp * (uint64_t)ctx->fatinfo.BytesPerSector) +
		(uint64_t)ctx->partition_byte_offset;
	return 0;
}

uint32_t libmsfat_context_get_cluster_size(struct libmsfat_context_t *ctx) {
	if (ctx == NULL) return (uint32_t)0;
	if (!ctx->fatinfo_set) return (uint32_t)0;
	return (uint32_t)ctx->fatinfo.BytesPerSector * (uint32_t)ctx->fatinfo.Sectors_Per_Cluster;
}

int libmsfat_context_read_disk(struct libmsfat_context_t *r,uint8_t *buf,const uint64_t offset,const size_t rdsz) {
	if (r == NULL || buf == NULL) return -1;
	if (r->read == NULL) return -1;
	return r->read(r,buf,offset,rdsz);
}

int libmsfat_context_fat_is_end_of_chain(const struct libmsfat_context_t *r,const libmsfat_cluster_t c) {
	if (c < (libmsfat_cluster_t)2UL) return 1;

	if (r->fatinfo.FAT_size == 12) {
		if (c >= (libmsfat_cluster_t)0xFF8UL)
			return 1;
	}
	else if (r->fatinfo.FAT_size == 16) {
		if (c >= (libmsfat_cluster_t)0xFFF8UL)
			return 1;
	}
	else if (r->fatinfo.FAT_size == 32) {
		if (libmsfat_FAT32_CLUSTER(c) >= (libmsfat_cluster_t)0x0FFFFFF8UL)
			return 1;
	}

	return 0;
}

uint8_t libmsfat_lfn_83_checksum_dirent(const struct libmsfat_dirent_t *dir) {
	uint8_t sum = 0;
	unsigned int i;

	for (i=0;i < 8;i++)
		sum = ((sum & 1) ? 0x80 : 0x00) + (sum >> 1) + dir->a.n.DIR_Name[i];
	for (i=0;i < 3;i++)
		sum = ((sum & 1) ? 0x80 : 0x00) + (sum >> 1) + dir->a.n.DIR_Ext[i];

	return sum;
}

libmsfat_cluster_t libmsfat_dirent_get_starting_cluster(const struct libmsfat_context_t *msfatctx,const struct libmsfat_dirent_t *dir) {
	libmsfat_cluster_t c;

	if (msfatctx->fatinfo.FAT_size == 32) {
		c = (libmsfat_cluster_t)le16toh(dir->a.n.DIR_FstClusLO);
		c += (libmsfat_cluster_t)le16toh(dir->a.n.DIR_FstClusHI) << (libmsfat_cluster_t)16UL;
	}
	else {
		c = (libmsfat_cluster_t)le16toh(dir->a.n.DIR_FstClusLO);
	}

	return c;
}

void libmsfat_lfn_assembly_init(struct libmsfat_lfn_assembly_t *l) {
	memset(l,0,sizeof(*l));
}

int libmsfat_lfn_dirent_complete(struct libmsfat_lfn_assembly_t *lfna,const struct libmsfat_dirent_t *dir) {
	uint8_t chksum;
	unsigned int i;

	if (lfna == NULL || dir == NULL) return -1;
	lfna->name_avail = 0;
	if (lfna->max == 0) return 0;
	lfna->err_missing = 0;
	lfna->err_checksum = 0;

	chksum = libmsfat_lfn_83_checksum_dirent(dir);

	for (i=0;i < lfna->max;i++) {
		if (lfna->present[i]) {
			if (lfna->chksum[i] != chksum)
				lfna->err_checksum = 1;
		}
		else {
			lfna->err_missing = 1;
		}
	}

	if (lfna->err_missing)
		lfna->err_str = "One or more segments are missing";
	else if (lfna->err_checksum)
		lfna->err_str = "Assembled LFN checksum mismatch against 8.3 name";

	lfna->name_avail = lfna->max;
	lfna->max = 0;
	return 0;
}

int libmsfat_lfn_dirent_assemble(struct libmsfat_lfn_assembly_t *lfna,const struct libmsfat_dirent_t *dir) {
	uint8_t ord;

	if (lfna == NULL || dir == NULL)
		return -1;

	if (lfna->max == 0) {
		if (!(dir->a.lfn.LDIR_Ord & 0x40)) { // must be marked as first
			lfna->err_str = "unexpected LFN entry that is not the start";
			lfna->max = 0;
			return -1;
		}

		lfna->err_missing = 0;
		lfna->err_checksum = 0;
		lfna->max = dir->a.lfn.LDIR_Ord & 0x3F;
		if (lfna->max == 0 || lfna->max > 32) {
			lfna->err_str = "first LFN entry has very large ordinal";
			lfna->max = 0;
			return -1;
		}

		assert(lfna->max <= sizeof(lfna->present));

		memset(lfna->present,0,lfna->max);
		memset(lfna->chksum,0,lfna->max);
		memset(lfna->assembly,0,sizeof(uint16_t)*lfna->max);
	}
	else {
		if (dir->a.lfn.LDIR_Ord & 0x40) { // another first entry??
			lfna->err_str = "unexpected LFN first entry while assembling an LFN";
			lfna->max = 0;
			return -1;
		}
		else if (dir->a.lfn.LDIR_Ord == 0 || dir->a.lfn.LDIR_Ord > lfna->max) {
			lfna->err_str = "LFN entry out of range";
			lfna->max = 0;
			return -1;
		}
	}

	ord = dir->a.lfn.LDIR_Ord & 0x3F;
	assert(ord > 0);
	ord--;
	assert(ord < lfna->max);

	if (lfna->present[ord]) {
		lfna->err_str = "duplicate entry";
		lfna->max = 0;
		return -1;
	}

	{
		uint16_t *d = lfna->assembly + (ord * (5+6+2));
		unsigned int i;

		assert(((uint8_t*)(d+(5+6+2))) <= ((uint8_t*)lfna->assembly + sizeof(lfna->assembly)));

		for (i=0;i < 5;i++)
			d[i+0] = dir->a.lfn.LDIR_Name1[i];
		for (i=0;i < 6;i++)
			d[i+5] = dir->a.lfn.LDIR_Name2[i];
		for (i=0;i < 2;i++)
			d[i+11] = dir->a.lfn.LDIR_Name3[i];
	}

	lfna->chksum[ord] = dir->a.lfn.LDIR_Chksum;
	lfna->present[ord] = 1;
	return 0;
}

int libmsfat_lfn_dirent_is_lfn(const struct libmsfat_dirent_t *dir) {
	if (dir == NULL) return 0;

	if (dir->a.n.DIR_Name[0] == 0x00 || dir->a.n.DIR_Name[0] == (char)0xE5)
		return 0;
	if ((dir->a.n.DIR_Attr & libmsfat_DIR_ATTR_MASK) == libmsfat_DIR_ATTR_LONG_NAME)
		return 1;

	return 0;
}

void libmsfat_file_io_ctx_init(struct libmsfat_file_io_ctx_t *c) {
	memset(c,0,sizeof(*c));
}

void libmsfat_file_io_ctx_free(struct libmsfat_file_io_ctx_t *c) {
	memset(c,0,sizeof(*c));
}

struct libmsfat_file_io_ctx_t *libmsfat_file_io_ctx_create() {
	struct libmsfat_file_io_ctx_t *ctx;

	ctx = (struct libmsfat_file_io_ctx_t*)malloc(sizeof(struct libmsfat_file_io_ctx_t));
	if (ctx == NULL) return NULL;
	libmsfat_file_io_ctx_init(ctx);
	return ctx;
}

struct libmsfat_file_io_ctx_t *libmsfat_file_io_ctx_destroy(struct libmsfat_file_io_ctx_t *c) {
	if (c != NULL) {
		libmsfat_file_io_ctx_free(c);
		free(c);
	}

	return NULL;
}

void libmsfat_file_io_ctx_close(struct libmsfat_file_io_ctx_t *c) {
	if (c == NULL) return;
	c->is_root_dir = 0;
	c->is_directory = 0;
	c->is_root_parent = 0;
	c->is_cluster_chain = 0;
	c->non_cluster_offset = 0;
	c->first_cluster = 0;
	c->position = 0;
}

int libmsfat_file_io_ctx_assign_cluster_chain(struct libmsfat_file_io_ctx_t *c,const struct libmsfat_context_t *msfatctx,libmsfat_cluster_t cluster) {
	if (c == NULL || msfatctx == NULL) return -1;
	libmsfat_file_io_ctx_close(c);
	if (!msfatctx->fatinfo_set) return -1;
	if (cluster < (libmsfat_cluster_t)2) cluster = (libmsfat_cluster_t)0;

	c->position = 0;
	c->file_size = 0;
	c->is_root_dir = 0;
	c->is_directory = 0;
	c->is_root_parent = 0;
	c->is_cluster_chain = 1;
	c->first_cluster = cluster;
	c->cluster_position_start = 0;
	c->cluster_position = c->first_cluster;
	c->cluster_size = (uint32_t)msfatctx->fatinfo.Sectors_Per_Cluster *
		(uint32_t)msfatctx->fatinfo.BytesPerSector;
	return 0;
}

uint32_t libmsfat_file_io_ctx_tell(struct libmsfat_file_io_ctx_t *c,const struct libmsfat_context_t *msfatctx) {
	if (c == NULL || msfatctx == NULL) return -1;
	return c->position;
}

libmsfat_cluster_t libmsfat_context_find_free_cluster(struct libmsfat_context_t *msfatctx) {
	libmsfat_FAT_entry_t entry;
	libmsfat_cluster_t i;

	if (msfatctx == NULL) return (libmsfat_cluster_t)0;

	/* scan */
	for (i=(libmsfat_cluster_t)2UL;i < msfatctx->fatinfo.Total_clusters;i++) {
		if (libmsfat_context_read_FAT(msfatctx,&entry,i,0)) break;
		if (msfatctx->fatinfo.FAT_size == 32) entry &= libmsfat_FAT32_CLUSTER_MASK;

		/* if the cluster is empty, then claim it */
		if (entry == (libmsfat_FAT_entry_t)0UL) return i;
	}

	/* didn't find anything */
	return (libmsfat_cluster_t)0;
}

int libmsfat_file_io_ctx_lseek(struct libmsfat_file_io_ctx_t *c,struct libmsfat_context_t *msfatctx,uint32_t offset,unsigned int flags) {
	libmsfat_FAT_entry_t next_cluster_fat;
	libmsfat_cluster_t next_cluster;
	uint32_t offset_cluster_offset;
	uint32_t offset_cluster_round;

	if (c == NULL || msfatctx == NULL) return -1;
	if (!msfatctx->fatinfo_set) return -1;

	if (c->is_root_dir && !c->is_cluster_chain) {
		if (offset > c->file_size) offset = c->file_size;
		c->position = offset;
	}
	else if (c->is_cluster_chain) {
		/* we need a cluster size */
		if (c->cluster_size == (uint32_t)0UL)
			return -1;

		/* NTS: directories tend not to indicate their size */
		if (offset > c->file_size && !c->is_directory &&
			(flags & (libmsfat_lseek_FLAG_IGNORE_FILE_SIZE | libmsfat_lseek_FLAG_EXTEND_CLUSTER_CHAIN)) == 0)
			offset = c->file_size;

		/* if allowed to extend the allocation chain, and no clusters allocated to the file, and the desired offset is nonzero, then try to start the chain */
		if ((flags & libmsfat_lseek_FLAG_EXTEND_CLUSTER_CHAIN) && c->first_cluster < (uint32_t)2UL && offset != (uint32_t)0UL) {
			next_cluster = libmsfat_context_find_free_cluster(msfatctx);
			if (next_cluster != (libmsfat_cluster_t)0) {
				/* rewrite the new cluster to be the end of the chain, and then update the dirent */
				if (libmsfat_context_write_FAT(msfatctx,(libmsfat_FAT_entry_t)0xFFFFFFFFUL,next_cluster,0) == 0) {
					c->first_cluster = next_cluster;
					c->cluster_position = c->first_cluster;
					c->cluster_position_start = 0;
					c->should_update_dirent = 1;
					c->position = 0;

					if (flags & libmsfat_lseek_FLAG_ZERO_CLUSTER_ON_ALLOC)
						libmsfat_file_io_ctx_zero_cluster(next_cluster,msfatctx);
				}
			}
		}

		/* files with no allocation chain cannot seek past zero */
		if (c->first_cluster < (uint32_t)2UL) offset = (uint32_t)0UL;

		/* if we go backwards or are explicitly seeking to 0, then reset position to zero to scan the FAT table again */
		if (offset == (uint32_t)0 || offset < c->cluster_position_start) {
			c->cluster_position = c->first_cluster;
			c->cluster_position_start = 0;
			c->position = 0;
		}

		/* what cluster to seek to? */
		offset_cluster_round = offset;
		offset_cluster_offset = offset_cluster_round % c->cluster_size;
		offset_cluster_round -= offset_cluster_offset;

		/* scan forward to desired position */
		c->position = c->cluster_position_start;
		while (c->cluster_position_start < offset_cluster_round) {
			/* this code should maintain the cluster position so that we're somewhere on the chain,
			 * and if we hit the end, we stay there. c->cluster_position == 0 should only happen IF
			 * the file has no cluster chain at all. */
			if (c->cluster_position < (uint32_t)2UL) break;

			/* update the position so that, if reading the FAT fails, or the FAT entry indicates we
			 * hit the end of the allocation chain, the current cluster remains on the last one and
			 * the offset reflects it (unchanged) but the position points to the END of the cluster. */
			c->position = c->cluster_position_start + c->cluster_size;
			if (libmsfat_context_read_FAT(msfatctx,&next_cluster_fat,c->cluster_position,0)) break;

			if (msfatctx->fatinfo.FAT_size >= 32)
				next_cluster = libmsfat_FAT32_CLUSTER((libmsfat_cluster_t)next_cluster_fat);
			else
				next_cluster = (libmsfat_cluster_t)next_cluster_fat;

			if (libmsfat_context_fat_is_end_of_chain(msfatctx,next_cluster)) {
				if (flags & libmsfat_lseek_FLAG_EXTEND_CLUSTER_CHAIN) {
					/* allow lseeking to the first byte past a cluster, but do not extend
					 * the allocation chain, IF, this is the last step to meet the caller's
					 * requirements and the seek operation puts it directly at the first
					 * byte of the next cluster. Most OSes do not allocate an extra cluster
					 * if you write up to the end of a cluster but not into the next. */
					if ((c->cluster_position_start + c->cluster_size) == offset_cluster_round && offset_cluster_offset == 0) break;
					/* the caller wants us to extend the allocation chain if necessary to
					 * satisfy the request. find a free cluster and take it, or give up.
					 * if for any reason we can't write the FAT table, then give up. */
					next_cluster = libmsfat_context_find_free_cluster(msfatctx);
					if (next_cluster == (libmsfat_cluster_t)0) break;
					/* rewrite the current cluster to point to the new cluster */
					if (libmsfat_context_write_FAT(msfatctx,next_cluster,c->cluster_position,0)) break;
					/* rewrite the new cluster to be the end of the chain */
					if (libmsfat_context_write_FAT(msfatctx,(libmsfat_FAT_entry_t)0xFFFFFFFFUL,next_cluster,0)) break;

					if (flags & libmsfat_lseek_FLAG_ZERO_CLUSTER_ON_ALLOC)
						libmsfat_file_io_ctx_zero_cluster(next_cluster,msfatctx);
				}
				else {
					break;
				}
			}

			/* the next link of the chain is valid. update position */
			c->cluster_position = next_cluster;
			c->cluster_position_start += c->cluster_size;
			c->position = c->cluster_position_start;
		}

		/* if we got to the cluster we wanted, then update the file pointer to cluster offset + offset within cluster */
		if (c->cluster_position_start == offset_cluster_round)
			c->position = c->cluster_position_start + offset_cluster_offset;

		/* if we extended the file past the original end, then the dirent needs updating */
		if ((flags & (libmsfat_lseek_FLAG_IGNORE_FILE_SIZE | libmsfat_lseek_FLAG_EXTEND_CLUSTER_CHAIN)) != 0) {
			if (c->file_size < c->position) {
				c->should_update_dirent = 1;
				c->file_size = c->position;
			}
		}
	}
	else {
		return -1;
	}

	return 0;
}

int libmsfat_file_io_ctx_assign_root_directory_with_parent(struct libmsfat_file_io_ctx_t *c,struct libmsfat_file_io_ctx_t *cp,struct libmsfat_context_t *msfatctx) {
	if (libmsfat_file_io_ctx_assign_root_directory(c,msfatctx))
		return -1;

	// parent at first represents a virtual "parent" of the root directory
	libmsfat_file_io_ctx_init(cp);
	cp->is_root_parent = 1;
	return 0;
}

int libmsfat_file_io_ctx_assign_root_directory(struct libmsfat_file_io_ctx_t *c,struct libmsfat_context_t *msfatctx) {
	if (c == NULL || msfatctx == NULL) return -1;
	libmsfat_file_io_ctx_close(c);
	if (!msfatctx->fatinfo_set) return -1;

	if (msfatctx->fatinfo.FAT_size == 32) {
		if (msfatctx->fatinfo.fat32.RootDirectory_cluster == (libmsfat_cluster_t)0UL)
			return -1;

		if (libmsfat_file_io_ctx_assign_cluster_chain(c,msfatctx,msfatctx->fatinfo.fat32.RootDirectory_cluster))
			return -1;

		c->is_root_parent = 0;
		c->is_directory = 1;
		c->is_root_dir = 1;
		c->file_size = 0;
	}
	else {
		if (msfatctx->fatinfo.RootDirectory_offset == (uint32_t)0UL)
			return -1;
		if (msfatctx->fatinfo.RootDirectory_size == (uint32_t)0UL)
			return -1;

		c->non_cluster_offset = (uint64_t)msfatctx->fatinfo.RootDirectory_offset * (uint64_t)msfatctx->fatinfo.BytesPerSector;
		c->cluster_size = msfatctx->fatinfo.RootDirectory_size * msfatctx->fatinfo.BytesPerSector;
		c->file_size = c->cluster_size;
		c->is_root_parent = 0;
		c->is_directory = 1;
		c->is_root_dir = 1;
	}

	c->zero_cluster_on_alloc = c->is_directory;
	if (libmsfat_file_io_ctx_lseek(c,msfatctx,(uint32_t)0UL,/*flags*/0) || libmsfat_file_io_ctx_tell(c,msfatctx) != (uint32_t)0UL) {
		libmsfat_file_io_ctx_close(c);
		return -1;
	}

	return 0;
}

/* TODO: If enabled, this code should also support allocating new clusters to write beyond the current end of the file.
 *       This includes allcating a cluster and modifying the dirent if the file is zero length. */
int libmsfat_file_io_ctx_write(struct libmsfat_file_io_ctx_t *c,struct libmsfat_context_t *msfatctx,const void *buffer,size_t len) {
	const uint8_t *d = buffer;
	unsigned int flags = 0;
	size_t canwrite;
	size_t dowrite;
	size_t wd = 0;
	uint32_t npos;
	uint64_t dofs;
	uint32_t ofs;

	if (c == NULL || msfatctx == NULL || buffer == NULL) return -1;
	if (!msfatctx->fatinfo_set) return -1;
	if (msfatctx->write == NULL) return -1;
	if (len == 0) return 0;

	if (c->is_root_dir && !c->is_cluster_chain) {
		if (c->position > c->file_size) return 0;
		canwrite = (size_t)(c->file_size - c->position);
		if (canwrite > len) canwrite = len;

		if (canwrite > (size_t)0) {
			if (msfatctx->write(msfatctx,d,msfatctx->partition_byte_offset+c->non_cluster_offset+(uint64_t)c->position,canwrite))
				return -1;
		}

		d += canwrite;
		wd = canwrite;
		c->position += (uint32_t)wd;
	}
	else if (c->is_cluster_chain) {
		if (c->cluster_size == (uint32_t)0) return 0;

		/* limit to file_size, unless directory, or file size limits are lifted */
		if (!c->is_directory && !c->allow_extend_to_cluster_tip && !c->allow_extend_allocation_chain) {
			if (c->position > c->file_size) return 0;
			canwrite = (size_t)(c->file_size - c->position);
			if (canwrite > len) canwrite = len;
		}
		else {
			canwrite = len;
		}

		/* if the fioctx indicates extending the file is permissible, then update flags to tell lseek */
		if (c->allow_extend_to_cluster_tip)
			flags |= libmsfat_lseek_FLAG_IGNORE_FILE_SIZE;
		if (c->allow_extend_allocation_chain)
			flags |= libmsfat_lseek_FLAG_EXTEND_CLUSTER_CHAIN;
		if (c->zero_cluster_on_alloc)
			flags |= libmsfat_lseek_FLAG_ZERO_CLUSTER_ON_ALLOC;

		while (canwrite > (size_t)0) {
			/* If the file has no allocation chain, then no writing happens.
			 * This should only happen if the file has no allocation chain.
			 * It should never happen if the file has any kind of allocation chain.
			 * Lseek keeps cluster_position somewhere within the chain, always. */
			if ((flags & libmsfat_lseek_FLAG_EXTEND_CLUSTER_CHAIN) && c->cluster_position < (uint32_t)2UL &&
				c->position == (uint32_t)0UL && c->file_size == (uint32_t)0UL) {
				/* lseek to start the allocation chain */
				if (libmsfat_file_io_ctx_lseek(c,msfatctx,c->cluster_size,flags))
					break;
				if (libmsfat_file_io_ctx_tell(c,msfatctx) != c->cluster_size)
					break;
				/* then lseek back to zero */
				if (libmsfat_file_io_ctx_lseek(c,msfatctx,(uint32_t)0,flags))
					break;
				/* the lseek() call changes the file size. set it back to zero */
				c->file_size = (uint32_t)0UL;
			}
			if (c->cluster_position < (uint32_t)2UL)
				break;

			/* how much more can we read within the cluster, before we need to
			 * move to the next cluster? this limits how much we can read in this round. */
			ofs = c->position % c->cluster_size;
			dowrite = c->cluster_size - ofs;
			if (dowrite > canwrite) dowrite = canwrite;
			assert(dowrite != (uint32_t)0);

			/* wait. if lseek hit the end of the chain, then the offset should be right on
			 * the start of the next cluster, and the file pointer should point exactly to
			 * the cluster byte offset plus cluster size. we can try lseeking to the current
			 * position if we're told we can extend the allocation chain, which might allow
			 * us to lseek to the target offset. */
			if ((flags & libmsfat_lseek_FLAG_EXTEND_CLUSTER_CHAIN) && ofs == (uint32_t)0UL &&
				c->position == (c->cluster_position_start + c->cluster_size)) {
				npos = (uint32_t)c->position + (uint32_t)dowrite;
				if (libmsfat_file_io_ctx_lseek(c,msfatctx,npos,flags))
					break;
				if (libmsfat_file_io_ctx_tell(c,msfatctx) != npos)
					break;

				npos -= (uint32_t)dowrite;
				if (libmsfat_file_io_ctx_lseek(c,msfatctx,npos,flags))
					break;
				if (libmsfat_file_io_ctx_tell(c,msfatctx) != npos)
					break;
			}
			if (ofs == (uint32_t)0UL && c->position == (c->cluster_position_start + c->cluster_size))
				break;

			/* OK. read! */
			if (libmsfat_context_get_cluster_offset(msfatctx,&dofs,c->cluster_position))
				return -1;
			if (msfatctx->write(msfatctx,d,dofs+(uint64_t)ofs,dowrite))
				return -1;

			/* advance the pointer, read count, and deduct from the amount we have yet to read. */
			d += dowrite;
			wd += dowrite;
			canwrite -= dowrite;

			/* advance the file pointer */
			npos = (uint32_t)c->position + (uint32_t)dowrite;
			if (libmsfat_file_io_ctx_lseek(c,msfatctx,npos,flags))
				break;
			if (libmsfat_file_io_ctx_tell(c,msfatctx) != npos)
				break;
		}
	}
	else {
		return -1;
	}

	return (int)wd;
}

int libmsfat_file_io_ctx_read(struct libmsfat_file_io_ctx_t *c,struct libmsfat_context_t *msfatctx,void *buffer,size_t len) {
	uint8_t *d = buffer;
	size_t canread;
	size_t doread;
	size_t rd = 0;
	uint32_t npos;
	uint64_t dofs;
	uint32_t ofs;

	if (c == NULL || msfatctx == NULL || buffer == NULL) return -1;
	if (!msfatctx->fatinfo_set) return -1;
	if (msfatctx->read == NULL) return -1;
	if (len == 0) return 0;

	if (c->is_root_dir && !c->is_cluster_chain) {
		if (c->position > c->file_size) return 0;
		canread = (size_t)(c->file_size - c->position);
		if (canread > len) canread = len;

		if (canread > (size_t)0) {
			if (msfatctx->read(msfatctx,d,msfatctx->partition_byte_offset+c->non_cluster_offset+(uint64_t)c->position,canread))
				return -1;
		}

		d += canread;
		rd = canread;
		c->position += (uint32_t)rd;
	}
	else if (c->is_cluster_chain) {
		if (c->cluster_size == (uint32_t)0) return 0;

		if (!c->is_directory) {
			if (c->position > c->file_size) return 0;
			canread = (size_t)(c->file_size - c->position);
			if (canread > len) canread = len;
		}
		else {
			canread = len;
		}

		while (canread > (size_t)0) {
			/* If the file has no allocation chain, then no reading happens.
			 * This should only happen if the file has no allocation chain.
			 * It should never happen if the file has any kind of allocation chain.
			 * Lseek keeps cluster_position somewhere within the chain, always. */
			if (c->cluster_position < (uint32_t)2UL)
				break;

			/* how much more can we read within the cluster, before we need to
			 * move to the next cluster? this limits how much we can read in this round. */
			ofs = c->position % c->cluster_size;
			doread = c->cluster_size - ofs;
			if (doread > canread) doread = canread;
			assert(doread != (uint32_t)0);

			/* wait. if lseek hit the end of the chain, then the offset should be right on
			 * the start of the next cluster, and the file pointer should point exactly to
			 * the cluster byte offset plus cluster size */
			if (ofs == (uint32_t)0UL && c->position == (c->cluster_position_start + c->cluster_size))
				break;

			/* OK. read! */
			if (libmsfat_context_get_cluster_offset(msfatctx,&dofs,c->cluster_position))
				return -1;
			if (msfatctx->read(msfatctx,d,dofs+(uint64_t)ofs,doread))
				return -1;

			/* advance the pointer, read count, and deduct from the amount we have yet to read. */
			d += doread;
			rd += doread;
			canread -= doread;

			/* advance the file pointer */
			npos = (uint32_t)c->position + (uint32_t)doread;
			if (libmsfat_file_io_ctx_lseek(c,msfatctx,npos,/*flags*/0))
				return -1;
			if (libmsfat_file_io_ctx_tell(c,msfatctx) != npos)
				break;
		}
	}
	else {
		return -1;
	}

	return (int)rd;
}

void libmsfat_dirent_volume_label_to_str(char *buf,size_t buflen,const struct libmsfat_dirent_t *dirent) {
	const char *s,*se;
	char *bufend,*d;

	if (buf == NULL || buflen <= (size_t)13) return;
	bufend = buf + buflen - 1;
	d = buf;

	if (dirent == NULL) {
		*d = 0;
		return;
	}

	/* DIR_Name is immediately followed by DIR_Ext */
	s = dirent->a.n.DIR_Name;
	se = s + sizeof(dirent->a.n.DIR_Name) + sizeof(dirent->a.n.DIR_Ext) - 1;
	while (se >= s && (*se == ' ' || *se == 0)) se--;
	se++;
	while (s < se && d < bufend) *d++ = *s++;

	*d = 0;
	assert(d <= bufend);
}

void libmsfat_dirent_filename_to_str(char *buf,size_t buflen,const struct libmsfat_dirent_t *dirent) {
	const char *s,*se;
	char *bufend,*d;

	if (buf == NULL || buflen <= (size_t)13) return;
	bufend = buf + buflen - 1;
	d = buf;

	if (dirent == NULL) {
		*d = 0;
		return;
	}

	s = dirent->a.n.DIR_Name;
	se = s + sizeof(dirent->a.n.DIR_Name) - 1;
	while (se >= s && (*se == ' ' || *se == 0)) se--;
	se++;
	while (s < se && d < bufend) *d++ = *s++;

	s = dirent->a.n.DIR_Ext;
	se = s + sizeof(dirent->a.n.DIR_Ext) - 1;
	while (se >= s && (*se == ' ' || *se == 0)) se--;
	se++;
	if (s < se) {
		*d++ = '.';
		while (s < se && d < bufend) *d++ = *s++;
	}

	*d = 0;
	assert(d <= bufend);
}

int libmsfat_file_io_ctx_rewinddir(struct libmsfat_file_io_ctx_t *fioctx,struct libmsfat_context_t *msfatctx,struct libmsfat_lfn_assembly_t *lfn_name) {
	if (fioctx == NULL || msfatctx == NULL) return -1;
	if (lfn_name != NULL) libmsfat_lfn_assembly_init(lfn_name);
	if (libmsfat_file_io_ctx_lseek(fioctx,msfatctx,(uint32_t)0,/*flags*/0)) return -1;
	if (libmsfat_file_io_ctx_tell(fioctx,msfatctx) != (uint32_t)0) return -1;
	return 0;
}

int libmsfat_file_io_ctx_readdir(struct libmsfat_file_io_ctx_t *fioctx,struct libmsfat_context_t *msfatctx,struct libmsfat_lfn_assembly_t *lfn_name,struct libmsfat_dirent_t *dirent) {
	if (fioctx == NULL || msfatctx == NULL || dirent == NULL) return -1;
	if (lfn_name != NULL) libmsfat_lfn_assembly_init(lfn_name);
	fioctx->dirent_lfn_start = (uint32_t)0xFFFFFFFFUL;
	fioctx->dirent_start = 0;

	assert(sizeof(*dirent) == 32);
	do {
		uint32_t pos = fioctx->position;

		if (libmsfat_file_io_ctx_read(fioctx,msfatctx,dirent,sizeof(*dirent)) != sizeof(*dirent))
			return -1;

		if (libmsfat_lfn_dirent_is_lfn(dirent)) {
			if (lfn_name != NULL) {
				if (lfn_name != NULL && lfn_name->max == 0)
					fioctx->dirent_lfn_start = pos;

				if (libmsfat_lfn_dirent_assemble(lfn_name,dirent))
					libmsfat_lfn_assembly_init(lfn_name);
			}
		}
		else {
			if (lfn_name != NULL) {
				if (libmsfat_lfn_dirent_complete(lfn_name,dirent))
					libmsfat_lfn_assembly_init(lfn_name);
			}

			/* 0x00, end of directory */
			if (dirent->a.n.DIR_Name[0] == 0x00)
				return -1;
			/* 0xE5, deleted entry */
			if (dirent->a.n.DIR_Name[0] == (char)0xE5)
				continue;

			/* found one! */
			fioctx->dirent_start = pos;
			break;
		}
	} while (1);

	if (lfn_name != NULL && !lfn_name->name_avail)
		fioctx->dirent_lfn_start = (uint32_t)0xFFFFFFFFUL;

	return 0;
}

int libmsfat_file_io_ctx_assign_from_dirent(struct libmsfat_file_io_ctx_t *fioctx,struct libmsfat_context_t *msfatctx,struct libmsfat_dirent_t *dirent) {
	libmsfat_cluster_t cluster;

	if (fioctx == NULL || msfatctx == NULL || dirent == NULL) return -1;

	cluster = libmsfat_dirent_get_starting_cluster(msfatctx,dirent);
	if (libmsfat_file_io_ctx_assign_cluster_chain(fioctx,msfatctx,cluster)) return -1;

	if (dirent->a.n.DIR_Attr & libmsfat_DIR_ATTR_DIRECTORY)
		fioctx->is_directory = 1;
	else
		fioctx->file_size = dirent->a.n.DIR_FileSize;

	fioctx->zero_cluster_on_alloc = fioctx->is_directory;
	return 0;
}

int libmsfat_file_io_ctx_write_dirent(struct libmsfat_file_io_ctx_t *fioctx,struct libmsfat_file_io_ctx_t *fioctx_parent,
	struct libmsfat_context_t *msfatctx,struct libmsfat_dirent_t *dirent,struct libmsfat_lfn_assembly_t *lfn_name) {
	/* sanity check */
	if (fioctx == NULL || fioctx_parent == NULL || msfatctx == NULL || dirent == NULL) return -1;
	if (!fioctx_parent->is_directory) return -1;

	/* we want to modify the entry in the parent directory ioctx */
	if (libmsfat_file_io_ctx_lseek(fioctx_parent,msfatctx,fioctx_parent->dirent_start,/*flags*/0)) return -1;
	if (libmsfat_file_io_ctx_tell(fioctx_parent,msfatctx) != fioctx_parent->dirent_start) return -1;
	if (libmsfat_file_io_ctx_write(fioctx_parent,msfatctx,dirent,sizeof(*dirent)) != sizeof(*dirent)) return -1;

	/* done */
	return 0;
}

/* WARNING: This code will also by design allow truncating directories to zero length. It does not regard the directory entries
 *          inside the directory and whether or not they have clusters allocated. This code assumes that if you truncate a directory
 *          to zero length that you took responsibility to delete all files and folders within the directory. If you did not do
 *          that, the clusters associated with the files and folders will not be freed and will show up as lost clusters.
 *
 *          I'm also not certain whether it is valid on FAT filesystems to have directories with no allocated clusters. To be
 *          safe, it is recommended that you do not truncate a directory to zero length unless you fully intend to delete the
 *          directory entry as well. */
int libmsfat_file_io_ctx_truncate_file(struct libmsfat_file_io_ctx_t *fioctx,struct libmsfat_file_io_ctx_t *fioctx_parent,
	struct libmsfat_context_t *msfatctx,struct libmsfat_dirent_t *dirent,struct libmsfat_lfn_assembly_t *lfn_name,uint32_t offset) {
	libmsfat_cluster_t current,next,eoc;
	libmsfat_FAT_entry_t fatent;
	unsigned char cut=0;
	uint32_t count=0;

	/* sanity check */
	if (fioctx == NULL || fioctx_parent == NULL || msfatctx == NULL) return -1;
	if (!fioctx->is_cluster_chain) return -1;

	/* parent fioctx must be a directory, unless it's the virtual parent of the root dir */
	if (!fioctx_parent->is_directory && !fioctx_parent->is_root_parent)
		return -1;

	/* do not permit the root directory to be truncated to zero (especially FAT32).
	 * FAT32 filesystem drivers don't like it when the root directory starts in a cluster marked free. */
	if (!fioctx_parent->is_directory && fioctx_parent->is_root_parent && offset == (uint32_t)0UL)
		offset = (uint32_t)1UL;

	/* at this point, dirent_start must be the byte offset within fioctx_parent of the short 8.3 dirent */

	/* end of chain */
	if (msfatctx->fatinfo.FAT_size == 32)
		eoc = (libmsfat_cluster_t)0x0FFFFFFFUL;
	else if (msfatctx->fatinfo.FAT_size == 16)
		eoc = (libmsfat_cluster_t)0xFFFFUL;
	else
		eoc = (libmsfat_cluster_t)0xFFFUL;

	/* can't truncate a file that's zero length */
	if (libmsfat_context_fat_is_end_of_chain(msfatctx,fioctx->first_cluster)) {
		fioctx->file_size = 0;
		if (dirent != NULL) {
			dirent->a.n.DIR_FileSize = 0;
			return libmsfat_file_io_ctx_write_dirent(fioctx,fioctx_parent,msfatctx,dirent,lfn_name);
		}
		else {
			return 0;
		}
	}

	/* first cluster */
	current = fioctx->first_cluster;

	do {
		if (libmsfat_context_fat_is_end_of_chain(msfatctx,current))
			break;
		if (libmsfat_context_read_FAT(msfatctx,&fatent,current,0))
			return -1;

		next = (libmsfat_cluster_t)fatent;
		if (msfatctx->fatinfo.FAT_size == 32) next &= libmsfat_FAT32_CLUSTER_MASK;

		count += fioctx->cluster_size;
		if (count >= offset || cut) {
			/* if trimming the file to zero length or having already decided to cut the file, write zeros into the FAT table */
			/* if we just started cutting the file, then the first FAT modification is to write an end of chain marker, then
			 * set the flag to reminder ourself the rest of the chain is to be overwritten by zeros. */
			if (offset == (uint32_t)0 || cut) {
				/* mark the cluster as empty. */
				if (libmsfat_context_write_FAT(msfatctx,(libmsfat_FAT_entry_t)0,current,0)) return -1;
				if (count == fioctx->cluster_size) {
					/* if this is the FIRST cluster we are removing, then the file is now zero length
					 * and the file's "cluster start" value should be set to zero to show that nothing
					 * is allocated to it. */
					if (dirent != NULL) {
						dirent->a.n.DIR_FstClusLO = 0;
						dirent->a.n.DIR_FstClusHI = 0;
					}
				}
			}
			else {
				/* mark the cluster as end of the chain, meaning that the current cluster holds data
				 * that appears as the end of the file. */
				if (libmsfat_context_write_FAT(msfatctx,(libmsfat_FAT_entry_t)eoc,current,0)) return -1;
			}

			if (!cut) {
				if (dirent != NULL) {
					if (dirent->a.n.DIR_Attr & libmsfat_DIR_ATTR_DIRECTORY)
						dirent->a.n.DIR_FileSize = (uint32_t)0UL;
					else
						dirent->a.n.DIR_FileSize = htole32(offset);
				}

				fioctx->file_size = offset;
				cut = 1;
			}
		}

		current = next;
	} while (1);

	/* root directory child doesn't have a dirent to update */
	if (!fioctx_parent->is_directory) {
		if (fioctx_parent->is_root_parent)
			return 0;
	}

	/* done */
	if (dirent != NULL)
		return libmsfat_file_io_ctx_write_dirent(fioctx,fioctx_parent,msfatctx,dirent,lfn_name);

	return 0;
}

int libmsfat_file_io_ctx_delete_dirent(struct libmsfat_file_io_ctx_t *fioctx,struct libmsfat_file_io_ctx_t *fioctx_parent,
	struct libmsfat_context_t *msfatctx,struct libmsfat_dirent_t *dirent,struct libmsfat_lfn_assembly_t *lfn_name) {
	/* sanity check */
	if (fioctx == NULL || fioctx_parent == NULL || msfatctx == NULL || dirent == NULL) return -1;
	if (!fioctx_parent->is_directory) return -1;

	/* mark the dirent deleted */
	dirent->a.n.DIR_Name[0] = (char)0xE5;

	/* write back */
	if (libmsfat_file_io_ctx_write_dirent(fioctx,fioctx_parent,msfatctx,dirent,lfn_name)) return -1;

	/* does the dirent have Long Filenames? They need to be erased too */
	if (fioctx_parent->dirent_lfn_start != (uint32_t)0xFFFFFFFFUL) {
		struct libmsfat_dirent_t lfnent;

		while (fioctx_parent->dirent_lfn_start < fioctx_parent->dirent_start) {
			if (libmsfat_file_io_ctx_lseek(fioctx_parent,msfatctx,fioctx_parent->dirent_lfn_start,/*flags*/0)) break;
			if (libmsfat_file_io_ctx_tell(fioctx_parent,msfatctx) != fioctx_parent->dirent_lfn_start) return -1;
			if (libmsfat_file_io_ctx_read(fioctx_parent,msfatctx,&lfnent,sizeof(lfnent)) != sizeof(lfnent)) return -1;

			lfnent.a.n.DIR_Name[0] = (char)0xE5;
			if (libmsfat_file_io_ctx_lseek(fioctx_parent,msfatctx,fioctx_parent->dirent_lfn_start,/*flags*/0)) break;
			if (libmsfat_file_io_ctx_tell(fioctx_parent,msfatctx) != fioctx_parent->dirent_lfn_start) return -1;
			if (libmsfat_file_io_ctx_write(fioctx_parent,msfatctx,&lfnent,sizeof(lfnent)) != sizeof(lfnent)) return -1;

			fioctx_parent->dirent_lfn_start += (uint32_t)32;
		}

		fioctx_parent->dirent_lfn_start = (uint32_t)0xFFFFFFFFUL;
	}

	/* done */
	return 0;
}

int libmsfat_dirent_is_dot_dir(struct libmsfat_dirent_t *dirent) {
	unsigned int i;

	if (dirent == NULL)
		return 0;

	/* we're looking for . or .. */
	if (dirent->a.n.DIR_Name[0] == '.') {
		if (dirent->a.n.DIR_Name[1] == '.' || dirent->a.n.DIR_Name[1] == ' ' || dirent->a.n.DIR_Name[1] == 0) {
			i = 2;
			while (i < 8) {
				if (dirent->a.n.DIR_Name[i] == ' ' || dirent->a.n.DIR_Name[i] == 0)
					i++;
				else
					return 0;
			}

			i = 0;
			while (i < 3) {
				if (dirent->a.n.DIR_Ext[i] == ' ' || dirent->a.n.DIR_Ext[i] == 0)
					i++;
				else
					return 0;
			}

			return 1;
		}
	}

	return 0;
}

int libmsfat_context_copy_FAT(struct libmsfat_context_t *msfatctx,unsigned int dst,unsigned int src) {
	unsigned char tmp[512];
	uint64_t ofs1,ofs2;
	uint32_t rd,len;

	if (msfatctx == NULL) return -1;
	if (src == dst) return 0;
	if (src >= msfatctx->fatinfo.FAT_tables) return -1;
	if (dst >= msfatctx->fatinfo.FAT_tables) return -1;
	if (msfatctx->read == NULL) return 0;
	if (msfatctx->write == NULL) return 0;

	ofs1 = (uint64_t)msfatctx->fatinfo.FAT_offset *
		(uint32_t)msfatctx->fatinfo.BytesPerSector;
	ofs1 += (uint64_t)msfatctx->fatinfo.FAT_table_size *
		(uint64_t)src * (uint64_t)msfatctx->fatinfo.BytesPerSector;
	ofs1 += (uint64_t)msfatctx->partition_byte_offset;

	ofs2 = (uint64_t)msfatctx->fatinfo.FAT_offset *
		(uint32_t)msfatctx->fatinfo.BytesPerSector;
	ofs2 += (uint64_t)msfatctx->fatinfo.FAT_table_size *
		(uint64_t)dst * (uint64_t)msfatctx->fatinfo.BytesPerSector;
	ofs2 += (uint64_t)msfatctx->partition_byte_offset;

	len = (uint32_t)msfatctx->fatinfo.FAT_table_size *
		(uint32_t)msfatctx->fatinfo.BytesPerSector;

	while (len > (uint32_t)0UL) {
		rd = len;
		if (rd > (uint32_t)sizeof(tmp)) rd = (uint32_t)sizeof(tmp);

		if (msfatctx->read(msfatctx,tmp,ofs1,rd)) return -1;
		if (msfatctx->write(msfatctx,tmp,ofs2,rd)) return -1;

		len -= rd;
		ofs1 += (uint64_t)rd;
		ofs2 += (uint64_t)rd;
	}

	return 0;
}

int libmsfat_context_load_fat32_fsinfo(struct libmsfat_context_t *msfatctx,struct libmsfat_fat32_fsinfo_t *fsinfo) {
	uint64_t ofs;

	if (msfatctx == NULL || fsinfo == NULL) return -1;
	if (msfatctx->read == NULL) return -1;
	if (!msfatctx->fatinfo_set) return -1;
	if (msfatctx->fatinfo.FAT_size != 32) return -1;
	if (msfatctx->fatinfo.fat32.BPB_FSInfo == (uint32_t)0UL) return -1;

	ofs = (uint64_t)msfatctx->fatinfo.fat32.BPB_FSInfo *
		(uint64_t)msfatctx->fatinfo.BytesPerSector;
	ofs += (uint64_t)msfatctx->partition_byte_offset;

	if (msfatctx->read(msfatctx,(uint8_t*)fsinfo,ofs,sizeof(*fsinfo)) != 0) return -1;
	if (le32toh(fsinfo->FSI_LeadSig) != 0x41615252UL) return -1;
	if (le32toh(fsinfo->FSI_StrucSig) != 0x61417272UL) return -1;
	if (le32toh(fsinfo->FSI_TrailSig) != 0xAA550000UL) return -1;

	return 0;
}

int libmsfat_context_write_fat32_fsinfo(struct libmsfat_context_t *msfatctx,struct libmsfat_fat32_fsinfo_t *fsinfo) {
	uint64_t ofs;

	if (msfatctx == NULL || fsinfo == NULL) return -1;
	if (msfatctx->write == NULL) return -1;
	if (!msfatctx->fatinfo_set) return -1;
	if (msfatctx->fatinfo.FAT_size != 32) return -1;
	if (msfatctx->fatinfo.fat32.BPB_FSInfo == (uint32_t)0UL) return -1;

	ofs = (uint64_t)msfatctx->fatinfo.fat32.BPB_FSInfo *
		(uint64_t)msfatctx->fatinfo.BytesPerSector;
	ofs += (uint64_t)msfatctx->partition_byte_offset;

	if (le32toh(fsinfo->FSI_LeadSig) != 0x41615252UL) return -1;
	if (le32toh(fsinfo->FSI_StrucSig) != 0x61417272UL) return -1;
	if (le32toh(fsinfo->FSI_TrailSig) != 0xAA550000UL) return -1;
	if (msfatctx->write(msfatctx,(uint8_t*)fsinfo,ofs,sizeof(*fsinfo)) != 0) return -1;

	return 0;
}

int libmsfat_context_update_fat32_free_cluster_count(struct libmsfat_context_t *msfatctx) {
	struct libmsfat_fat32_fsinfo_t fsinfo;
	libmsfat_FAT_entry_t entry;
	uint32_t free_count = 0;
	libmsfat_cluster_t c;

	if (msfatctx == NULL) return -1;
	if (!msfatctx->fatinfo_set) return -1;
	if (msfatctx->fatinfo.FAT_size != 32) return -1;
	if (libmsfat_context_load_fat32_fsinfo(msfatctx,&fsinfo)) return -1;

	for (c=(libmsfat_cluster_t)2UL;c < (libmsfat_cluster_t)msfatctx->fatinfo.Total_clusters;c++) {
		if (libmsfat_context_read_FAT(msfatctx,&entry,c,0))
			return -1;

		/* assume FAT32. we made sure of that, remember? */
		entry &= libmsfat_FAT32_CLUSTER_MASK;

		/* count */
		if (entry == (libmsfat_FAT_entry_t)0) free_count++;
	}

	fsinfo.FSI_Free_Count = htole32(free_count);
	if (libmsfat_context_write_fat32_fsinfo(msfatctx,&fsinfo)) return -1;
	return 0;
}

int libmsfat_file_io_ctx_update_dirent_from_context(struct libmsfat_dirent_t *dirent,struct libmsfat_file_io_ctx_t *fioctx,struct libmsfat_file_io_ctx_t *fioctx_parent,struct libmsfat_context_t *msfatctx) {
	if (dirent == NULL || fioctx == NULL || msfatctx == NULL) return -1;

	if (fioctx->is_directory)
		dirent->a.n.DIR_FileSize = (uint32_t)0;
	else
		dirent->a.n.DIR_FileSize = htole32(fioctx->file_size);

	dirent->a.n.DIR_FstClusLO = (uint16_t)(fioctx->first_cluster & 0xFFFF);
	if (msfatctx->fatinfo.FAT_size == 32) dirent->a.n.DIR_FstClusHI = (uint16_t)(fioctx->first_cluster >> 16);
	return 0;
}

int libmsfat_file_io_ctx_enable_write_extend(struct libmsfat_file_io_ctx_t *fioctx,struct libmsfat_file_io_ctx_t *fioctx_parent,struct libmsfat_context_t *msfatctx) {
	if (fioctx == NULL || fioctx_parent == NULL || msfatctx == NULL)
		return -1;

	fioctx->allow_extend_to_cluster_tip = 1;
	fioctx->allow_extend_allocation_chain = 1;
	return 0;
}

int libmsfat_dirent_str_to_filename(struct libmsfat_dirent_t *dirent,const char *name) {
	unsigned int i;

	if (dirent == NULL || name == NULL) return -1;

	/* init */
	memset(dirent,0,sizeof(*dirent));
	while (*name == '.' || *name == ' ') return -1;
	if (*name == 0) return -1;

	/* name */
	i = 0;
	while (*name != 0 && *name != '.') {
		if (i >= 8) return -1;
		if (*name < 32) return -1;
		dirent->a.n.DIR_Name[i++] = toupper(*name);
		name++;
	}
	while (i < 8) dirent->a.n.DIR_Name[i++] = ' ';

	/* extension */
	i = 0;
	if (*name == '.') {
		name++;
		while (*name != 0) {
			if (*name == '.') return -1;
			if (*name < 32) return -1;
			if (i >= 3) return -1;
			dirent->a.n.DIR_Ext[i++] = toupper(*name);
			name++;
		}
	}
	while (i < 3) dirent->a.n.DIR_Ext[i++] = ' ';

	return 0;
}

int libmsfat_dirent_lfn_to_dirent_piece(struct libmsfat_dirent_t *dirent,struct libmsfat_lfn_assembly_t *lfn_name,unsigned int segment) {
	unsigned int i;
	uint16_t *s;

	if (dirent == NULL || lfn_name == NULL) return -1;
	if (segment >= lfn_name->name_avail || segment >= 32U) return -1;

	memset(dirent,0,sizeof(*dirent));
	dirent->a.lfn.LDIR_Ord = segment + 1;
	if ((segment + 1) == lfn_name->name_avail) dirent->a.lfn.LDIR_Ord |= 0x40; /* last entry */

	dirent->a.lfn.LDIR_Attr = 0x0F;
	dirent->a.lfn.LDIR_Chksum = lfn_name->chksum[segment];

	s = lfn_name->assembly + (segment * 13);
	for (i=0;i < 5;i++) dirent->a.lfn.LDIR_Name1[i] = *s++;
	for (i=0;i < 6;i++) dirent->a.lfn.LDIR_Name2[i] = *s++;
	for (i=0;i < 2;i++) dirent->a.lfn.LDIR_Name3[i] = *s++;

	return 0;
}

int libmsfat_file_io_ctx_zero_cluster(libmsfat_cluster_t cluster,struct libmsfat_context_t *msfatctx) {
	uint8_t tmp[512];
	uint64_t offset;
	uint32_t sz,wr;

	if (msfatctx == NULL) return -1;
	if (cluster < (libmsfat_cluster_t)2UL) return -1;
	if (libmsfat_context_get_cluster_offset(msfatctx,&offset,cluster)) return -1;

	sz = libmsfat_context_get_cluster_size(msfatctx);
	memset(tmp,0,512);
	while (sz > 0) {
		wr = sz;
		if (wr > sizeof(tmp)) wr = sizeof(tmp);

		if (msfatctx->write(msfatctx,tmp,offset,wr))
			return -1;

		offset += wr;
		sz -= wr;
	}

	return 0;
}

