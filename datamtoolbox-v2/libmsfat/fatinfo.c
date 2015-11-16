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
#include <string.h>
#include <stddef.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>

#include <datamtoolbox-v2/libpartmbr/mbrctx.h>

/* oh come on Microsoft ... */
#if defined(_MSC_VER) && !defined(S_ISREG)
# define S_ISREG(x) (x & _S_IFREG)
#endif

#ifndef O_BINARY
# define O_BINARY 0
#endif

//////////////////////////////////////////////////////////////////////////
#pragma pack(push,1)
/* Microsoft FAT32 File System Specification v1.03 December 6, 2000 - Boot Sector and BPB Structure (common to FAT12/16/32) */
/* "NOTE: Fields that start with BS_ are part of the boot sector, BPB_ is part of the BPB" */
struct libmsfat_BS_bootsector_header { /* Boot sector, starting at byte offset +0 of the boot sector up until +11 where the BPB starts */
	uint8_t				BS_jmpBoot[3];			/* struct  +0 + 0 -> boot sector  +0 */
	uint8_t				BS_OEMName[8];			/* struct  +3 + 0 -> boot secotr  +3 */
};									/*=struct +11 + 0 -> boot sector +11 */
#pragma pack(pop)
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
int libmsfat_sanity_check() {
	{
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
	}

	return 0;
}

int libmsfat_bs_is_fat32(struct libmsfat_bootsector *p_bs) {
	if (p_bs == NULL) return 0;

	/* NTS: No byte swapping is needed to compare against zero.
	 * We're checking for values that, if zero, would render FAT12/FAT16 unreadable to systems that do not understand FAT32.
	 * Especially important is the FATSz16 field. */
	if (p_bs->BPB_common.BPB_RootEntCnt == 0U && p_bs->BPB_common.BPB_TotSec16 == 0U && p_bs->BPB_common.BPB_FATSz16 == 0U)
		return 1;

	return 0;
}
//////////////////////////////////////////////////////////////////////////

static unsigned char			sectorbuf[512];
static char				tmpstr[512];

static void field2str(char *dst,size_t dstlen,const uint8_t *src,const size_t srclen) {
	size_t cpy;

	if (dstlen == (size_t)0) return;
	dstlen--; // dst including NUL

	cpy = srclen;
	if (cpy > dstlen) cpy = dstlen;
	if (cpy != (size_t)0) memcpy(dst,src,cpy);
	dst[cpy] = 0;
}

int main(int argc,char **argv) {
	uint32_t first_lba=0,size_lba=0;
	const char *s_partition = NULL;
	const char *s_image = NULL;
	int i,fd;

	for (i=1;i < argc;) {
		const char *a = argv[i++];

		if (*a == '-') {
			do { a++; } while (*a == '-');

			if (!strcmp(a,"image")) {
				s_image = argv[i++];
			}
			else if (!strcmp(a,"partition")) {
				s_partition = argv[i++];
			}
			else {
				fprintf(stderr,"Unknown switch '%s'\n",a);
				return 1;
			}
		}
		else {
			fprintf(stderr,"Unexpected arg '%s'\n",a);
			return 1;
		}
	}

	if (libmsfat_sanity_check() != 0) {
		fprintf(stderr,"libmsfat sanity check fail\n");
		return 1;
	}

	if (s_image == NULL) {
		fprintf(stderr,"fatinfo --image <image> ...\n");
		fprintf(stderr,"\n");
		fprintf(stderr,"--partition <n>          Hard disk image, use partition N from the MBR\n");
		return 1;
	}

	fd = open(s_image,O_RDONLY|O_BINARY);
	if (fd < 0) {
		fprintf(stderr,"Unable to open disk image, %s\n",strerror(errno));
		return 1;
	}
	{
		/* make sure it's a file */
		struct stat st;
		if (fstat(fd,&st) || !S_ISREG(st.st_mode)) {
			fprintf(stderr,"Image is not a file\n");
			return 1;
		}
	}

	if (s_partition != NULL) {
		struct libpartmbr_context_t *ctx = NULL;
		int index = atoi(s_partition);
		int dfd = -1;

		if (index < 0) {
			fprintf(stderr,"Invalid index\n");
			return 1;
		}

		ctx = libpartmbr_context_create();
		if (ctx == NULL) {
			fprintf(stderr,"Cannot allocate libpartmbr context\n");
			return 1;
		}

		/* good! hand it off to the context. use dup() because it takes ownership */
		dfd = dup(fd);
		if (libpartmbr_context_assign_fd(ctx,dfd)) {
			fprintf(stderr,"libpartmbr did not accept file descriptor %d\n",fd);
			close(dfd); // dispose of it
			return 1;
		}
		dfd = -1;
		ctx->geometry.cylinders = 16384;
		ctx->geometry.sectors = 63;
		ctx->geometry.heads = 255;

		/* load the partition table! */
		if (libpartmbr_context_read_partition_table(ctx)) {
			fprintf(stderr,"Failed to read partition table\n");
			return 1;
		}

		/* index */
		if ((size_t)index >= ctx->list_count) {
			fprintf(stderr,"Index too large\n");
			return 1;
		}

		/* valid? */
		struct libpartmbr_context_entry_t *ent = &ctx->list[(size_t)index];

		if (ent->is_empty) {
			fprintf(stderr,"You chose an empty MBR partition\n");
			return 1;
		}

		/* show */
		printf("Chosen partition:\n");
		printf("    Type: 0x%02x %s\n",
			(unsigned int)ent->entry.partition_type,
			libpartmbr_partition_type_to_str(ent->entry.partition_type));

		if (!(ent->entry.partition_type == LIBPARTMBR_TYPE_FAT12_32MB ||
			ent->entry.partition_type == LIBPARTMBR_TYPE_FAT16_32MB ||
			ent->entry.partition_type == LIBPARTMBR_TYPE_FAT16B_8GB ||
			ent->entry.partition_type == LIBPARTMBR_TYPE_FAT32_CHS ||
			ent->entry.partition_type == LIBPARTMBR_TYPE_FAT32_LBA ||
			ent->entry.partition_type == LIBPARTMBR_TYPE_FAT16B_LBA ||
			ent->entry.partition_type == LIBPARTMBR_TYPE_FAT12_32MB_HIDDEN ||
			ent->entry.partition_type == LIBPARTMBR_TYPE_FAT16_32MB_HIDDEN ||
			ent->entry.partition_type == LIBPARTMBR_TYPE_FAT16B_8GB_HIDDEN ||
			ent->entry.partition_type == LIBPARTMBR_TYPE_FAT32_CHS_HIDDEN ||
			ent->entry.partition_type == LIBPARTMBR_TYPE_FAT32_LBA_HIDDEN ||
			ent->entry.partition_type == LIBPARTMBR_TYPE_FAT16B_LBA_HIDDEN)) {
			fprintf(stderr,"MBR type does not suggest a FAT filesystem\n");
			return 1;
		}
		first_lba = ent->entry.first_lba_sector;
		size_lba = ent->entry.number_lba_sectors;

		/* done */
		ctx = libpartmbr_context_destroy(ctx);
	}
	else {
		lseek_off_t x;

		first_lba = 0;

		x = lseek(fd,0,SEEK_END);
		if (x < (lseek_off_t)0) x = 0;
		x /= (lseek_off_t)512UL;
		if (x > (lseek_off_t)0xFFFFFFFFUL) x = (lseek_off_t)0xFFFFFFFFUL;
		size_lba = (uint32_t)x;
		lseek(fd,0,SEEK_SET);
	}

	printf("Reading from disk sectors %lu-%lu (%lu sectors)\n",
		(unsigned long)first_lba,
		(unsigned long)first_lba+(unsigned long)size_lba-(unsigned long)1UL,
		(unsigned long)size_lba);

	{
		lseek_off_t x = (lseek_off_t)first_lba * (lseek_off_t)512UL;

		if (lseek(fd,x,SEEK_SET) != x || read(fd,sectorbuf,512) != 512) {
			fprintf(stderr,"Unable to read boot sector\n");
			return 1;
		}
	}

	{
		struct libmsfat_bootsector *p_bs = (struct libmsfat_bootsector*)sectorbuf;
		unsigned int i;

		assert(sizeof(*p_bs) <= 512);

		printf("Boot sector contents:\n");
		printf("    BS_jmpBoot:      0x%02x 0x%02x %02x\n",
			p_bs->BS_header.BS_jmpBoot[0],
			p_bs->BS_header.BS_jmpBoot[1],
			p_bs->BS_header.BS_jmpBoot[2]);

		field2str(tmpstr,sizeof(tmpstr),p_bs->BS_header.BS_OEMName,sizeof(p_bs->BS_header.BS_OEMName));
		printf("    BS_OEMName:      '%s'\n",tmpstr);

		printf("    BPB_BytsPerSec:  %u\n",(unsigned int)le16toh(p_bs->BPB_common.BPB_BytsPerSec));
		printf("    BPB_SecPerClus:  %u\n",(unsigned int)p_bs->BPB_common.BPB_SecPerClus);
		printf("    BPB_RsvdSecCnt:  %u\n",(unsigned int)le16toh(p_bs->BPB_common.BPB_RsvdSecCnt));
		printf("    BPB_NumFATs:     %u\n",(unsigned int)p_bs->BPB_common.BPB_NumFATs);
		printf("    BPB_RootEntCnt:  %u\n",(unsigned int)le16toh(p_bs->BPB_common.BPB_RootEntCnt));
		printf("    BPB_TotSec16:    %u\n",(unsigned int)le16toh(p_bs->BPB_common.BPB_TotSec16));
		printf("    BPB_Media:       0x%02x\n",(unsigned int)p_bs->BPB_common.BPB_Media);
		printf("    BPB_FATSz16:     %u\n",(unsigned int)le16toh(p_bs->BPB_common.BPB_FATSz16));
		printf("    BPB_SecPerTrk:   %u\n",(unsigned int)le16toh(p_bs->BPB_common.BPB_SecPerTrk));
		printf("    BPB_NumHeads:    %u\n",(unsigned int)le16toh(p_bs->BPB_common.BPB_NumHeads));
		printf("    BPB_HiddSec:     %lu\n",(unsigned long)le32toh(p_bs->BPB_common.BPB_HiddSec));
		printf("    BPB_TotSec32:    %lu\n",(unsigned long)le32toh(p_bs->BPB_common.BPB_TotSec32));

		if (libmsfat_bs_is_fat32(p_bs)) {
			printf("    ---[FAT32 BPB continues]\n");

			printf("    BPB_FATSz32:     %lu\n",(unsigned long)le32toh(p_bs->at36.BPB_FAT32.BPB_FATSz32));
			printf("    BPB_ExtFlags:    %u\n",(unsigned int)le16toh(p_bs->at36.BPB_FAT32.BPB_ExtFlags));
			printf("    BPB_FSVer:       0x%04x\n",(unsigned int)le16toh(p_bs->at36.BPB_FAT32.BPB_FSVer));
			printf("    BPB_RootClus:    %lu\n",(unsigned long)le32toh(p_bs->at36.BPB_FAT32.BPB_RootClus));
			printf("    BPB_FSInfo:      0x%04x\n",(unsigned int)le16toh(p_bs->at36.BPB_FAT32.BPB_FSInfo));
			printf("    BPB_BkBootSec:   %u\n",(unsigned int)le16toh(p_bs->at36.BPB_FAT32.BPB_BkBootSec));

			printf("    BPB_Reserved:    ");
			for (i=0;i < 12;i++) printf("0x%02x ",p_bs->at36.BPB_FAT32.BPB_Reserved[i]);
			printf("\n");

			printf("    BS_DrvNum:       0x%02x\n",p_bs->at36.BPB_FAT32.BS_DrvNum);
			printf("    BS_Reserved1:    0x%02x\n",p_bs->at36.BPB_FAT32.BS_Reserved1);
			printf("    BS_BootSig:      0x%02x\n",p_bs->at36.BPB_FAT32.BS_BootSig);
			printf("    BS_VolID:        0x%08lx\n",(unsigned long)le32toh(p_bs->at36.BPB_FAT32.BS_VolID));

			field2str(tmpstr,sizeof(tmpstr),p_bs->at36.BPB_FAT32.BS_VolLab,sizeof(p_bs->at36.BPB_FAT32.BS_VolLab));
			printf("    BS_VolLab:       '%s'\n",tmpstr);

			field2str(tmpstr,sizeof(tmpstr),p_bs->at36.BPB_FAT32.BS_FilSysType,sizeof(p_bs->at36.BPB_FAT32.BS_FilSysType));
			printf("    BS_FilSysType:   '%s'\n",tmpstr);
		}
		else {
			printf("    ---[FAT12/16 BPB continues]\n");

			printf("    BS_DrvNum:       0x%02x\n",(unsigned int)p_bs->at36.BPB_FAT.BS_DrvNum);
			printf("    BS_Reserved1:    0x%02x\n",(unsigned int)p_bs->at36.BPB_FAT.BS_Reserved1);
			printf("    BS_BootSig:      0x%02x\n",(unsigned int)p_bs->at36.BPB_FAT.BS_BootSig);
			printf("    BS_VolID:        0x%08lx\n",(unsigned long)le32toh(p_bs->at36.BPB_FAT.BS_VolID));

			field2str(tmpstr,sizeof(tmpstr),p_bs->at36.BPB_FAT.BS_VolLab,sizeof(p_bs->at36.BPB_FAT.BS_VolLab));
			printf("    BS_VolLab:       '%s'\n",tmpstr);

			field2str(tmpstr,sizeof(tmpstr),p_bs->at36.BPB_FAT.BS_FilSysType,sizeof(p_bs->at36.BPB_FAT.BS_FilSysType));
			printf("    BS_FilSysType:   '%s'\n",tmpstr);
		}
	}

	close(fd);
	fd = -1;
	return 0;
}

