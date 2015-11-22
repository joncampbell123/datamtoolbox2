#ifndef __DATAMTOOLBOX_LIBMSFAT_LIBMSFAT_H
#define __DATAMTOOLBOX_LIBMSFAT_LIBMSFAT_H

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
#pragma pack()
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
#pragma pack()
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
#pragma pack()

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
#pragma pack()

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
#pragma pack()

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
	uint32_t			Total_clusters;		// total cluster count, including non-data clusters 0 and 1.
								// this is also the size of the FAT table related to the disk, not including the extra entries in the FAT table.
	uint32_t			Total_data_clusters;	// total data cluster count (clusters 0 and 1 do not have data storage, counted as if 0-1 did not exist)
	uint32_t			Max_possible_clusters;	// highest possible total cluster count given the size of the FAT table (compared to the portion actually used for the clusters on disk)
								// this is the number of clusters that would exist if the entire FAT table (including extra space) were used.
	uint32_t			Max_possible_data_clusters;// highest possible total data cluster count, excluding clusters 0 and 1.
								// this is the number of clusters that would exist if the entire FAT table (including extra space) were used.
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

// context for assembling long filenames while reading directories
struct libmsfat_lfn_assembly_t {
	uint16_t			assembly[(5+6+2)*32];
	uint8_t				present[32];
	uint8_t				chksum[32];
	uint8_t				name_avail;
	uint8_t				err_missing;
	uint8_t				err_checksum;
	uint8_t				max;
	const char*			err_str;
};

// context for reading files and directories
struct libmsfat_file_io_ctx_t {
	unsigned int			is_root_dir:1;		// FAT12/FAT16 root dir, not cluster chain
	unsigned int			is_directory:1;		// contents are that of a directory
	unsigned int			is_cluster_chain:1;	// is cluster chain
	unsigned int			_padding_:28;
	uint64_t			non_cluster_offset;	// if root dir (non-cluster), byte offset
	libmsfat_cluster_t		first_cluster;		// if not root dir, then starting cluster
	uint32_t			file_size;		// file size, in bytes
	uint32_t			position;		// file position (pointer) a la lseek
	uint32_t			cluster_position;	// current cluster representing file position
	uint32_t			cluster_position_start;	// byte offset in file representing start of current cluster
	uint32_t			cluster_size;		// size of cluster unit. if FAT12/16 root dir, then size of root dir.
	// directory reading, last dirent read by readdir
	uint32_t			dirent_lfn_start;
	uint32_t			dirent_start;
};

#pragma pack(push,1)
# if defined(_MSC_VER) /* Microsoft C++ treats unsigned int bitfields properly except the struct becomes 32-bit wide, which is WRONG */
#  define bitfield_t					unsigned short int
# else /* GCC will bitch about anything other than "unsigned int" used in bitfield definitions, but will make the struct 16 bits wide total */
#  define bitfield_t					unsigned int
# endif
struct libmsfat_msdos_time_t {
	union {
		uint16_t				raw;
		struct {
			bitfield_t			seconds2:5;		/* [4:0] seconds, in units of 2 seconds, 0-29 inclusive */
			bitfield_t			minutes:6;		/* [10:5] minutes, 0-59 inclusive */
			bitfield_t			hours:5;		/* [15:11] hours, 0-23 inclusive */
		} f;
	} a;
};
# undef bitfield_t
#pragma pack(pop)
#pragma pack()

#pragma pack(push,1)
# if defined(_MSC_VER) /* Microsoft C++ treats unsigned int bitfields properly except the struct becomes 32-bit wide, which is WRONG */
#  define bitfield_t					unsigned short int
# else /* GCC will bitch about anything other than "unsigned int" used in bitfield definitions, but will make the struct 16 bits wide total */
#  define bitfield_t					unsigned int
# endif
struct libmsfat_msdos_date_t {
	union {
		uint16_t				raw;
		struct {
			bitfield_t			day_of_month:5;		/* [4:0] day of month, 1-31 inclusive */
			bitfield_t			month_of_year:4;	/* [8:5] month of year, 1-12 inclusive */
			bitfield_t			years_since_1980:7;	/* [15:9] years since 1980 */
		} f;
	} a;
};
# undef bitfield_t
#pragma pack(pop)
#pragma pack()

#pragma pack(push,1)
/* directory entry */
struct libmsfat_dirent_t {
	union {
		struct { /* normal dirent */
			char				DIR_Name[8];	/* offset +0 */
									/* 8.3 file name (not extension). if less than 8 chars, ascii space ' ' is used to fill out 8 chars */
									/* NTS: we break from Microsoft specification by separating name from extension */
			char				DIR_Ext[3];	/* offset +8 */
									/* 8.3 file extension. if less than 3 chars, ascii space ' ' is used to fill out 3 chars */
			uint8_t				DIR_Attr;	/* offset +11 */
									/* file attributes */
			uint8_t				DIR_NTRes;	/* offset +12 */
									/* Windows NT reserved */
			uint8_t				DIR_CrtTimeTenth; /* offset +13 */
									/* NTS: Microsoft... did you make a mistake? Did you mean 100ths of a second? */
			struct libmsfat_msdos_date_t	DIR_CrtTime;	/* offset +14 */
									/* Packed structure, time file was created */
			struct libmsfat_msdos_time_t	DIR_CrtDate;	/* offset +16 */
									/* Packed structure, date file was created */
			struct libmsfat_msdos_date_t	DIR_LstAccDate;	/* offset +18 */
									/* Last accessed date (no time field) */
			uint16_t			DIR_FstClusHI;	/* offset +20 */
									/* hi 16 bits of cluster field (FAT32 only) */
			struct libmsfat_msdos_time_t	DIR_WrtTime;	/* offset +22 */
									/* time of last write */
			struct libmsfat_msdos_date_t	DIR_WrtDate;	/* offset +24 */
									/* date of last write */
			uint16_t			DIR_FstClusLO;	/* offset +26 */
									/* low 16 bits of cluster field */
			uint32_t			DIR_FileSize;	/* offset +28 */
									/* file size in bytes */
		} n;							/*=offset +32 */
		struct { /* long filename dirent */
			uint8_t				LDIR_Ord;	/* offset +0 */
									/* order of the entry in the sequence of long dir names.
									 * may be masked with LAST_LONG_ENTRY (0x40) to signal that it's the last entry */
			uint16_t			LDIR_Name1[5];	/* offset +1 */
									/* Characters 1-5 of the long filename (UCS-16 unicode) */
			uint8_t				LDIR_Attr;	/* offset +11 */
									/* Attributes. must be ATTR_LONG_NAME */
			uint8_t				LDIR_Type;	/* offset +12 */
									/* set to 0 to signify a long file name component. may be nonzero for other types. */
			uint8_t				LDIR_Chksum;	/* offset +13 */
									/* Checksum of name in the short dir entry that follows the LFN entries */
			uint16_t			LDIR_Name2[6];	/* offset +14 */
									/* Characters 6-11 of the long filename (UCS-16 unicode) */
			uint16_t			LDIR_FstClusLO;	/* offset +26 */
									/* upper 16 bits of cluster field. must be zero. */
			uint16_t			LDIR_Name3[2];	/* offset +28 */
									/* Characters 12-13 of the long filename (UCS-16 unicode) */
		} lfn;							/*=offset +32 */
	} a;
};									/*=offset +32 */
#pragma pack(pop)
#pragma pack()

#define libmsfat_DIR_ATTR_READ_ONLY			(1U << 0U)
#define libmsfat_DIR_ATTR_HIDDEN			(1U << 1U)
#define libmsfat_DIR_ATTR_SYSTEM			(1U << 2U)
#define libmsfat_DIR_ATTR_VOLUME_ID			(1U << 3U)
#define libmsfat_DIR_ATTR_DIRECTORY			(1U << 4U)
#define libmsfat_DIR_ATTR_ARCHIVE			(1U << 5U)

#define libmsfat_DIR_ATTR_LONG_NAME			(0x0F)

#define libmsfat_DIR_ATTR_MASK				(0x3F)

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
int libmsfat_context_write_FAT(struct libmsfat_context_t *r,libmsfat_FAT_entry_t entry,const libmsfat_cluster_t cluster);
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
uint32_t libmsfat_context_get_cluster_size(struct libmsfat_context_t *ctx);
int libmsfat_context_read_disk(struct libmsfat_context_t *r,uint8_t *buf,const uint64_t offset,const size_t rdsz);
uint8_t libmsfat_lfn_83_checksum_dirent(const struct libmsfat_dirent_t *dir);
libmsfat_cluster_t libmsfat_dirent_get_starting_cluster(const struct libmsfat_context_t *msfatctx,const struct libmsfat_dirent_t *dir);

int libmsfat_context_fat_is_end_of_chain(const struct libmsfat_context_t *r,const libmsfat_cluster_t c);

void libmsfat_lfn_assembly_init(struct libmsfat_lfn_assembly_t *l);
int libmsfat_lfn_dirent_complete(struct libmsfat_lfn_assembly_t *lfna,const struct libmsfat_dirent_t *dir);
int libmsfat_lfn_dirent_assemble(struct libmsfat_lfn_assembly_t *lfna,const struct libmsfat_dirent_t *dir);
int libmsfat_lfn_dirent_is_lfn(const struct libmsfat_dirent_t *dir);

struct libmsfat_file_io_ctx_t *libmsfat_file_io_ctx_create();
void libmsfat_file_io_ctx_init(struct libmsfat_file_io_ctx_t *c);
void libmsfat_file_io_ctx_free(struct libmsfat_file_io_ctx_t *c);
void libmsfat_file_io_ctx_close(struct libmsfat_file_io_ctx_t *c);
struct libmsfat_file_io_ctx_t *libmsfat_file_io_ctx_destroy(struct libmsfat_file_io_ctx_t *c);
uint32_t libmsfat_file_io_ctx_tell(struct libmsfat_file_io_ctx_t *c,const struct libmsfat_context_t *msfatctx);
int libmsfat_file_io_ctx_lseek(struct libmsfat_file_io_ctx_t *c,struct libmsfat_context_t *msfatctx,uint32_t offset);
int libmsfat_file_io_ctx_assign_root_directory(struct libmsfat_file_io_ctx_t *c,struct libmsfat_context_t *msfatctx);
int libmsfat_file_io_ctx_read(struct libmsfat_file_io_ctx_t *c,struct libmsfat_context_t *msfatctx,void *buffer,size_t len);
int libmsfat_file_io_ctx_write(struct libmsfat_file_io_ctx_t *c,struct libmsfat_context_t *msfatctx,const void *buffer,size_t len);
int libmsfat_file_io_ctx_assign_cluster_chain(struct libmsfat_file_io_ctx_t *c,const struct libmsfat_context_t *msfatctx,libmsfat_cluster_t cluster);
int libmsfat_file_io_ctx_assign_from_dirent(struct libmsfat_file_io_ctx_t *fioctx,struct libmsfat_context_t *msfatctx,struct libmsfat_dirent_t *dirent);

void libmsfat_dirent_filename_to_str(char *buf,size_t buflen,const struct libmsfat_dirent_t *dirent);
void libmsfat_dirent_volume_label_to_str(char *buf,size_t buflen,const struct libmsfat_dirent_t *dirent);
int libmsfat_file_io_ctx_rewinddir(struct libmsfat_file_io_ctx_t *fioctx,struct libmsfat_context_t *msfatctx,struct libmsfat_lfn_assembly_t *lfn_name);
int libmsfat_file_io_ctx_readdir(struct libmsfat_file_io_ctx_t *fioctx,struct libmsfat_context_t *msfatctx,struct libmsfat_lfn_assembly_t *lfn_name,struct libmsfat_dirent_t *dirent);

int libmsfat_file_io_ctx_write_dirent(struct libmsfat_file_io_ctx_t *fioctx,struct libmsfat_file_io_ctx_t *fioctx_parent,
	struct libmsfat_context_t *msfatctx,struct libmsfat_dirent_t *dirent,struct libmsfat_lfn_assembly_t *lfn_name);

int libmsfat_file_io_ctx_truncate_file(struct libmsfat_file_io_ctx_t *fioctx,struct libmsfat_file_io_ctx_t *fioctx_parent,
	struct libmsfat_context_t *msfatctx,struct libmsfat_dirent_t *dirent,struct libmsfat_lfn_assembly_t *lfn_name,uint32_t offset);

int libmsfat_file_io_ctx_delete_dirent(struct libmsfat_file_io_ctx_t *fioctx,struct libmsfat_file_io_ctx_t *fioctx_parent,
	struct libmsfat_context_t *msfatctx,struct libmsfat_dirent_t *dirent,struct libmsfat_lfn_assembly_t *lfn_name);

int libmsfat_dirent_is_dot_dir(struct libmsfat_dirent_t *dirent);

#endif // __DATAMTOOLBOX_LIBMSFAT_LIBMSFAT_H

