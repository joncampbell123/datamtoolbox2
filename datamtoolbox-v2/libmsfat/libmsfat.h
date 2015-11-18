
#include <stdint.h>

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

