#if defined(_MSC_VER)
/*hack: Microsoft C++ does not have le32toh() and friends, but Windows is Little Endian anyway*/
# define le16toh(x) (x)
# define le32toh(x) (x)
# define htole32(x) (x)
# define htole16(x) (x)
#endif
#if defined(_MSC_VER)
/* shut up Microsoft. how the fuck is strerror() unsafe? */
# define _CRT_SECURE_NO_WARNINGS
# include <io.h>
/* shut up Microsoft. what the hell is your problem with POSIX functions? */
# define open _open
# define read _read
# define write _write
# define close _close
# define lseek _lseeki64
# define lseek_off_t __int64
#else
# define lseek_off_t off_t
#endif
#include <stdlib.h>
#if !defined(_MSC_VER)
# include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>

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
int libmsfat_context_read_FAT(struct libmsfat_context_t *r,libmsfat_FAT_entry_t *entry,const libmsfat_cluster_t cluster) {
	uint64_t offset;
	uint8_t rdsize;
	uint8_t buf[4];

	if (r == NULL || entry == NULL) return -1;
	if (!r->fatinfo_set) return -1;
	if (r->read == NULL) return -1;
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

	if (r->read(r,buf,offset,(size_t)rdsize) != 0) return -1;

	if (r->fatinfo.FAT_size == 12) {
		uint16_t raw = *((uint16_t*)buf);
		uint16_t tmp = le16toh(raw);

		// if (cluster & 1)
		//   val = tmp >> 4
		// else
		//   val = tmp & 0xFFF;

		*entry = (libmsfat_FAT_entry_t)((tmp >> (((uint8_t)cluster & 1U) * 4U)) & 0xFFF);
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
	res = lseek(r->user_fd,ofs,SEEK_SET);
	if (res < (lseek_off_t)0)
		return -1; // lseek() also sets errno
	else if (res != ofs) {
		errno = ERANGE;
		return -1;
	}

	rd = read(r->user_fd,buffer,len);
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
	res = lseek(r->user_fd,ofs,SEEK_SET);
	if (res < (lseek_off_t)0)
		return -1; // lseek() also sets errno
	else if (res != ofs) {
		errno = ERANGE;
		return -1;
	}

	rd = write(r->user_fd,buffer,len);
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
#if defined(WIN32)
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
#if defined(WIN32)
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

