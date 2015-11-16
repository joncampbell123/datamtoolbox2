
#include <stdint.h>

#pragma pack(push,1)
/* Microsoft FAT32 File System Specification v1.03 December 6, 2000 - Boot Sector and BPB Structure (common to FAT12/16/32) */
/* "NOTE: Fields that start with BS_ are part of the boot sector, BPB_ is part of the BPB" */
struct libmsfat_BS_bootsector_header { /* Boot sector, starting at byte offset +0 of the boot sector up until +11 where the BPB starts */
	uint8_t				BS_jmpBoot[3];			/* struct  +0 + 0 -> boot sector  +0 */
	uint8_t				BS_OEMName[8];			/* struct  +3 + 0 -> boot secotr  +3 */
};									/*=struct +11 + 0 -> boot sector +11 */
#pragma pack(pop)

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

int libmsfat_sanity_check();
int libmsfat_bs_is_fat32(struct libmsfat_bootsector *p_bs);

