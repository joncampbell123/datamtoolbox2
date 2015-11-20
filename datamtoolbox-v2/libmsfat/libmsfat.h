
#include <stdint.h>

#include <datamtoolbox-v2/libint13chs/int13chs.h>

typedef uint32_t libmsfat_cluster_t;
typedef uint32_t libmsfat_FAT_entry_t;

#pragma pack(push,1)
/* Microsoft FAT32 File System Specification v1.03 December 6, 2000 - Boot Sector and BPB Structure (common to FAT12/16/32) */
/* "NOTE: Fields that start with BS_ are part of the boot sector, BPB_ is part of the BPB" */
struct libmsfat_BS_bootsector_header { /* Boot sector, starting at byte offset +0 of the boot sector up until +11 where the BPB starts */
	uint8_t				BS_jmpBoot[3];			/* struct  +0 + 0 -> boot sector  +0 */
	uint8_t				BS_OEMName[8];			/* struct  +3 + 0 -> boot secotr  +3 */
};									/*=struct +11 + 0 -> boot sector +11 */
#pragma pack(pop)
/* Fields:
 *
 *      BS_jmpBoot
 *
 *      Jump instruction to boot code. This field has two allowed forms:
 *      jmpBoot[0] = 0xEB, jmpBoot[1] = 0x??, jmpBoot[2] = 0x90 and
 *      jmpBoot[0] = 0xE9, jmpBoot[1] = 0x??, jmpBoot[2] = 0x??
 *      This code typically occupies the rest of sector 0 of the volume
 *      following the BPB and possibly other sectors. Either of these forms
 *      is acceptable. JmpBoot[0] = 0xEB is the more frequently used format.
 *
 *      BS_OEMName
 *
 *      "MSWIN4.1",  Microsoft operating systems don't pay any attention to
 *      this field. Some FAT drivers do.
 */

#pragma pack(push,1)
/* Microsoft FAT32 File System Specification v1.03 December 6, 2000 - Boot Sector and BPB Structure (common to FAT12/16/32) */
/* "NOTE: Fields that start with BS_ are part of the boot sector, BPB_ is part of the BPB" */
struct libmsfat_BPB_common_header { /* BPB structure, starting with BPB_BytsPerSec at byte offset +11 of the sector immediately after BS_OEMName. All fields little endian. */
	uint16_t			BPB_BytsPerSec;			/* struct  +0 + 11 -> boot sector +11 */
	uint8_t				BPB_SecPerClus;			/* struct  +2 + 11 -> boot sector +13 */
	uint16_t			BPB_RsvdSecCnt;			/* struct  +3 + 11 -> boot sector +14 */
	uint8_t				BPB_NumFATs;			/* struct  +5 + 11 -> boot sector +16 */
	uint16_t			BPB_RootEntCnt;			/* struct  +6 + 11 -> boot sector +17 */
	uint16_t			BPB_TotSec16;			/* struct  +8 + 11 -> boot sector +19 */
	uint8_t				BPB_Media;			/* struct +10 + 11 -> boot sector +21 */
	uint16_t			BPB_FATSz16;			/* struct +11 + 11 -> boot sector +22 */
	uint16_t			BPB_SecPerTrk;			/* struct +13 + 11 -> boot sector +24 */
	uint16_t			BPB_NumHeads;			/* struct +15 + 11 -> boot sector +26 */
	uint32_t			BPB_HiddSec;			/* struct +17 + 11 -> boot sector +28 */
	uint32_t			BPB_TotSec32;			/* struct +21 + 11 -> boot sector +32 */
};									/*=struct +25 + 11 -> boot sector +36 */
#pragma pack(pop)
/* Fields:
 *
 *      BPB_BytsPerSec
 *
 *      Count of bytes per sector. This value may take on only the following values: 512, 1024, 2048 or 4096.
 *      If maximum compatibility with old implementations is desired, only the value 512 should be used.
 *      There is a lot of FAT code in the world that is basically “hard wired” to 512 bytes per sector and
 *      doesn’t bother to check this field to make sure it is 512. Microsoft operating systems will properly
 *      support 1024, 2048, and 4096. If the media being recorded has a physical sector size N, you must use
 *      N and this must still be less than or equal to 4096.
 *
 *      BPB_SecPerClus
 *
 *      Number of sectors per allocation unit. This value must be a power of 2 that
 *      is greater than 0. The legal values are 1, 2, 4, 8, 16, 32, 64, and 128.
 *      Note however, that a value should never be used that results in a “bytes per
 *      cluster” value (BPB_BytsPerSec * BPB_SecPerClus) greater than 32K (32 * 1024).
 *      Values that cause a cluster size greater than 32K bytes do not work properly;
 *      do not try to define one. Some versions of some systems allow 64K bytes per
 *      cluster value. Many application setup programs will not work correctly on
 *      such a FAT volume.
 *
 *      BPB_RsvdSecCnt
 *
 *      Number of reserved sectors in the Reserved region of the volume starting at the
 *      first sector of the volume. This field must not be 0. For FAT12 and FAT16 volumes,
 *      this value should never be anything other than 1. For FAT32 volumes, this value is
 *      typically 32. Microsoft operating systems will properly support any non-zero value
 *      in this field.
 *
 */

#pragma pack(push,1)
/* Microsoft FAT32 File System Specification v1.03 December 6, 2000 - Fat12 and Fat16 Structure Starting at Offset 36 */
struct libmsfat_BPBat36_FAT1216 { /* BPB structure, starting with BPB_DrvNum at byte offset +36 of the sector immediately after BPB_TotSec32. All fields little endian. */
	uint8_t				BS_DrvNum;			/* struct  +0 + 36 -> boot sector +36 */
	uint8_t				BS_Reserved1;			/* struct  +1 + 36 -> boot sector +37 */
	uint8_t				BS_BootSig;			/* struct  +2 + 36 -> boot sector +38 */
	uint32_t			BS_VolID;			/* struct  +3 + 36 -> boot sector +39 */
	uint8_t				BS_VolLab[11];			/* struct  +7 + 36 -> boot sector +43 */
	uint8_t				BS_FilSysType[8];		/* struct +18 + 36 -> boot sector +54 */
};									/*=struct +26 + 36 -> boot sector +62 */
#pragma pack(pop)

#pragma pack(push,1)
/* Microsoft FAT32 File System Specification v1.03 December 6, 2000 - Fat32 Structure Starting at Offset 36 */
struct libmsfat_BPBat36_FAT32 { /* BPB structure, starting with BPB_DrvNum at byte offset +36 of the sector immediately after BPB_TotSec32. All fields little endian. */
	uint32_t			BPB_FATSz32;			/* struct  +0 + 36 -> boot sector +36 */
	uint16_t			BPB_ExtFlags;			/* struct  +4 + 36 -> boot sector +40 */
	uint16_t			BPB_FSVer;			/* struct  +6 + 36 -> boot sector +42 */
	uint32_t			BPB_RootClus;			/* struct  +8 + 36 -> boot sector +44 */
	uint16_t			BPB_FSInfo;			/* struct +12 + 36 -> boot sector +48 */
	uint16_t			BPB_BkBootSec;			/* struct +14 + 36 -> boot sector +50 */
	uint8_t				BPB_Reserved[12];		/* struct +16 + 36 -> boot sector +52 */
	uint8_t				BS_DrvNum;			/* struct +28 + 36 -> boot sector +64 */
	uint8_t				BS_Reserved1;			/* struct +29 + 36 -> boot sector +65 */
	uint8_t				BS_BootSig;			/* struct +30 + 36 -> boot sector +66 */
	uint32_t			BS_VolID;			/* struct +31 + 36 -> boot sector +67 */
	uint8_t				BS_VolLab[11];			/* struct +35 + 36 -> boot sector +71 */
	uint8_t				BS_FilSysType[8];		/* struct +46 + 36 -> boot sector +82 */
};									/*=struct +54 + 36 -> boot sector +90 */
#pragma pack(pop)

#pragma pack(push,1)
/* combined structure, to typedef against an MS-DOS FAT boot sector */
struct libmsfat_bootsector { /* boot sector, from byte offset +0 */
	struct libmsfat_BS_bootsector_header		BS_header;	/* boot sector  +0 */
	struct libmsfat_BPB_common_header		BPB_common;	/* boot sector +11 */
	union {
		struct libmsfat_BPBat36_FAT1216		BPB_FAT;	/* boot sector +36 (FAT12/FAT16) */
		struct libmsfat_BPBat36_FAT32		BPB_FAT32;	/* boot sector +36 (FAT32) */
	} at36;								/*=boot sector +90 (union) */
};									/*=boot sector +90 */
#pragma pack(pop)

/* struct to hold computed disk locations */
struct libmsfat_disk_locations_and_info {
	uint8_t				FAT_size;		// 12, 16, or 32
	uint8_t				FAT_tables;		// copy of BPB_NumFATs
	uint16_t			BytesPerSector;		// BPB_BytsPerSec
	uint32_t			FAT_table_size;		// size of FAT table in sectors
	uint32_t			FAT_offset;		// offset of first FAT table (sectors)
	uint32_t			RootDirectory_offset;	// offset of root directory (FAT12/FAT16 only), zero for FAT32 (sectors)
	uint32_t			RootDirectory_size;	// size of the root directory in sectors (FAT12/FAT16)
	uint32_t			Data_offset;		// offset of first data cluster (cluster #2) (sectors)
	uint32_t			Data_size;		// size of the data area
	uint32_t			Total_clusters;		// total cluster count
	uint32_t			Total_data_clusters;	// total data cluster count (clusters 0 and 1 do not have data storage, counted as if 0-1 did not exist)
	uint32_t			Max_possible_clusters;	// highest possible cluster count, considering sector boundaries
	uint32_t			Max_possible_data_clusters;// highest possible data cluster count (clusters 0 and 1 do not have data storage, counted as if 0-1 did not exist)
	uint32_t			TotalSectors;		// total sector count
	uint8_t				Sectors_Per_Cluster;	// sectors per cluster
	struct {
		uint32_t		BPB_FSInfo;		// copy of BPB FSInfo, sector of this structure
		uint32_t		RootDirectory_cluster;	// first cluster of root directory
	} fat32;
};

// context structure for FAT I/O
struct libmsfat_context_t {
	struct chs_geometry_t				geometry;
	union {
		uint16_t				val16;
		uint32_t				val32;
		uint64_t				val64;
		int					intval;
		long					longval;
	} user;
	void						(*user_free_cb)(struct libmsfat_context_t *r);	// callback to free/deallocate user_ptr/user_id
	void*						user_ptr;
	uint64_t					user_id;
	int						user_fd;
#if defined(WIN32)
	HANDLE						user_win32_handle;
#endif
	struct libmsfat_disk_locations_and_info		fatinfo;
	const char*					err_str;	// error string
	unsigned char					chs_mode;	// if set, use C/H/S addressing (geometry must be set)
	unsigned char					fatinfo_set;
	uint64_t					partition_byte_offset; // if within a partition table scheme, the BYTE offset of the partition, added to all read/write I/O
	int						(*read)(struct libmsfat_context_t *r,uint8_t *buffer,uint64_t offset,size_t len); // 0=success   -1=error (see errno)
	int						(*write)(struct libmsfat_context_t *r,const uint8_t *buffer,uint64_t offset,size_t len);
};

/* cluster zero does not represent data, but instead, holds the media type ID in the lowest 8 bits */
#define libmsfat_CLUSTER_0_MEDIA_TYPE			((libmsfat_cluster_t)0UL)

/* cluster one is generally 0xFFF/0xFFFF/0xxFFFFFFF but MS-DOS 6.22/Win9x/ME uses the upper 2 bits for volume health status */
#define libmsfat_CLUSTER_1_DIRTY_FLAGS			((libmsfat_cluster_t)1UL)

/* these are the bits used by MS-DOS/Win9x/ME for volume health */
/* FAT16 versions */
/*   _CLEAN: if set, the volume is clean. if not set, volume is "dirty", meaning the filesystem driver did not dismount the volume properly, check the filesystem */
#define libmsfat_FAT16_DIRTYFLAG_CLEAN			((libmsfat_FAT_entry_t)0x8000UL)
/*   _NOIOERROR: if set, no disk i/o errors were encountered. if not set, the filesystem driver encountered a disk I/O error and you should check the filesystem */
#define libmsfat_FAT16_DIRTYFLAG_NOIOERROR		((libmsfat_FAT_entry_t)0x4000UL)

/* FAT32 versions (remember bits 28-31 are undefined) */
/*   _CLEAN: if set, the volume is clean. if not set, volume is "dirty", meaning the filesystem driver did not dismount the volume properly, check the filesystem */
#define libmsfat_FAT32_DIRTYFLAG_CLEAN			((libmsfat_FAT_entry_t)0x08000000UL)
/*   _NOIOERROR: if set, no disk i/o errors were encountered. if not set, the filesystem driver encountered a disk I/O error and you should check the filesystem */
#define libmsfat_FAT32_DIRTYFLAG_NOIOERROR		((libmsfat_FAT_entry_t)0x04000000UL)

/* use this macro for separating out FAT32 cluster number vs reserved bits */
#define libmsfat_FAT32_CLUSTER_MASK			((uint32_t)0x0FFFFFFFUL)
#define libmsfat_FAT32_RESERVED_MASK			((uint32_t)0xF0000000UL)
#define libmsfat_FAT32_CLUSTER(x)			(((uint32_t)(x)) & libmsfat_FAT32_CLUSTER_MASK)
#define libmsfat_FAT32_RESERVED(x)			(((uint32_t)(x)) & libmsfat_FAT32_RESERVED_MASK)

/* sanity check: self-test structures and functions to ensure everything compiled OK */
int libmsfat_sanity_check();

/* take bootsector struct and determine whether to read the BPB using FAT32 or FAT12/16 structure definition (at36 union).
 *
 * WARNING: This tells you which BPB struct to use, it does *not* mean the filesystem itself is FAT32.
 * To determine that, you must take the disk/partition size into account and compute the number of clusters
 * on the disk, then use the cluster count to determine FAT12/FAT16/FAT32.
 *
 * [Microsoft FAT32 File System Specification v1.03 December 6, 2000, Page 14-15, "FAT Type Determination"] */
int libmsfat_bs_is_fat32(const struct libmsfat_bootsector *p_bs);

int libmsfat_bs_is_valid(const struct libmsfat_bootsector *p_bs,const char **err_str);
int libmsfat_boot_sector_is_valid(const unsigned char *sector/*512 bytes*/,const char **err_str);

/* take boot sector struct, analyze beginning JMP instruction and use it to determine the
 * length of the structure, and therefore, which fields are valid. This is *VITAL* if we
 * are to successfully parse older MS-DOS formats (anything prior to MS-DOS 3.3). */
int libmsfat_bs_struct_length(const struct libmsfat_bootsector *p_bs);
/* Common structure lengths:

   MS-DOS 7.1 / Windows 98 FAT32: 90 bytes
   MS-DOS 6.22:                   62 bytes
   MS-DOS 5.0:                    62 bytes
   MS-DOS 3.31 (Compaq):          42 bytes
   MS-DOS 3.3:                    54 bytes
   MS-DOS 3.21:                   54 bytes
   MS-DOS 2.1:                    46 bytes (fields past "hidden sectors" appear to be code, not data)
   MS-DOS 1.01:                   41 bytes (and all the fields are zero) Boot signature is not present.
   MS-DOS 1.0:                    49 bytes (and all fields are zero, except for ASCII date where BPB would start). Boot signature is not present.

   Also consistent is the opcode 0xFA where the CPU would jump to in MS-DOS boot sectors.

 */

/* report whether the Boot Signature is present. If so, the fields BS_VolID, BS_VolLab, and BS_FilSysType are valid */
int libmsfat_bs_fat1216_bootsig_present(const struct libmsfat_bootsector *p_bs);
int libmsfat_bs_fat1216_BS_VolID_exists(const struct libmsfat_bootsector *p_bs);
int libmsfat_bs_fat1216_BS_VolLab_exists(const struct libmsfat_bootsector *p_bs);
int libmsfat_bs_fat1216_BS_FilSysType_exists(const struct libmsfat_bootsector *p_bs);
int libmsfat_bs_fat1216_BS_BootSig_present(const struct libmsfat_bootsector *p_bs);
int libmsfat_bs_fat1216_BPB_TotSec32_present(const struct libmsfat_bootsector *p_bs);
int libmsfat_bs_compute_disk_locations(struct libmsfat_disk_locations_and_info *nfo,const struct libmsfat_bootsector *p_bs);
int libmsfat_context_read_FAT(struct libmsfat_context_t *r,libmsfat_FAT_entry_t *entry,const libmsfat_cluster_t cluster);
int libmsfat_context_set_fat_info(struct libmsfat_context_t *r,const struct libmsfat_disk_locations_and_info *nfo);
int libmsfat_context_def_fd_read(struct libmsfat_context_t *r,uint8_t *buffer,uint64_t offset,size_t len);
int libmsfat_context_def_fd_write(struct libmsfat_context_t *r,const uint8_t *buffer,uint64_t offset,size_t len);
int libmsfat_context_init(struct libmsfat_context_t *r);
void libmsfat_context_close_file(struct libmsfat_context_t *r);
void libmsfat_context_free(struct libmsfat_context_t *r);
struct libmsfat_context_t *libmsfat_context_create();
struct libmsfat_context_t *libmsfat_context_destroy(struct libmsfat_context_t *r);
int libmsfat_context_assign_fd(struct libmsfat_context_t *r,const int fd);
int libmsfat_context_get_cluster_sector(struct libmsfat_context_t *ctx,uint64_t *sector,const libmsfat_cluster_t cluster);
int libmsfat_context_get_cluster_offset(struct libmsfat_context_t *ctx,uint64_t *offset,const libmsfat_cluster_t cluster);

