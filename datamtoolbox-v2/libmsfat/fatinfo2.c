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
#include <datamtoolbox-v2/libmsfat/libmsfat.h>

/* oh come on Microsoft ... */
#if defined(_MSC_VER) && !defined(S_ISREG)
# define S_ISREG(x) (x & _S_IFREG)
#endif

#ifndef O_BINARY
# define O_BINARY 0
#endif

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
	struct libmsfat_disk_locations_and_info locinfo;
	struct libmsfat_context_t *msfatctx = NULL;
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
		int bs_struct_size;
		unsigned int i;

		assert(sizeof(*p_bs) <= 512);

		/* we can tell how much structure is there by the JMP instruction at the beginning of the boot sector.
		 * This is vital as earlier MS-DOS versions used shorter structures followed by data. Not taking length
		 * into account means misinterpreting code as data */
		bs_struct_size = libmsfat_bs_struct_length(p_bs);

		printf("Boot sector contents:\n");
		printf("    BS_jmpBoot:      0x%02x 0x%02x 0x%02x\n",
			p_bs->BS_header.BS_jmpBoot[0],
			p_bs->BS_header.BS_jmpBoot[1],
			p_bs->BS_header.BS_jmpBoot[2]);
		printf("    (structure size): %d bytes\n",
			bs_struct_size);

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
		if (libmsfat_bs_fat1216_BPB_TotSec32_present(p_bs))
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
		else if (libmsfat_bs_fat1216_BS_BootSig_present(p_bs)) {
			printf("    ---[FAT12/16 BPB continues]\n");

			printf("    BS_DrvNum:       0x%02x\n",(unsigned int)p_bs->at36.BPB_FAT.BS_DrvNum);
			printf("    BS_Reserved1:    0x%02x\n",(unsigned int)p_bs->at36.BPB_FAT.BS_Reserved1);
			printf("    BS_BootSig:      0x%02x\n",(unsigned int)p_bs->at36.BPB_FAT.BS_BootSig);

			if (libmsfat_bs_fat1216_bootsig_present(p_bs)) {
				if (libmsfat_bs_fat1216_BS_VolID_exists(p_bs))
					printf("    BS_VolID:        0x%08lx\n",(unsigned long)le32toh(p_bs->at36.BPB_FAT.BS_VolID));

				if (libmsfat_bs_fat1216_BS_VolLab_exists(p_bs)) {
					field2str(tmpstr,sizeof(tmpstr),p_bs->at36.BPB_FAT.BS_VolLab,sizeof(p_bs->at36.BPB_FAT.BS_VolLab));
					printf("    BS_VolLab:       '%s'\n",tmpstr);
				}

				if (libmsfat_bs_fat1216_BS_FilSysType_exists(p_bs)) {
					field2str(tmpstr,sizeof(tmpstr),p_bs->at36.BPB_FAT.BS_FilSysType,sizeof(p_bs->at36.BPB_FAT.BS_FilSysType));
					printf("    BS_FilSysType:   '%s'\n",tmpstr);
				}
			}
		}
	}

	{
		const char *err_str = NULL;

		if (!libmsfat_boot_sector_is_valid(sectorbuf,&err_str)) {
			printf("Boot sector is not valid: reason=%s\n",err_str);
			return 1;
		}
	}

	{
		struct libmsfat_bootsector *p_bs = (struct libmsfat_bootsector*)sectorbuf;
		libmsfat_FAT_entry_t fatent;
		int dfd;

		if (libmsfat_bs_compute_disk_locations(&locinfo,p_bs)) {
			printf("Unable to locate disk locations.\n");
			return 1;
		}

		printf("Disk location and FAT info:\n");
		printf("    FAT format:        FAT%u\n",
			locinfo.FAT_size);
		printf("    FAT tables:        %u\n",
			locinfo.FAT_tables);
		printf("    FAT table size:    %lu sectors\n",
			(unsigned long)locinfo.FAT_table_size);
		printf("    FAT offset:        %lu sectors\n",
			(unsigned long)locinfo.FAT_offset);
		printf("    Root directory:    %lu sectors\n",
			(unsigned long)locinfo.RootDirectory_offset);
		printf("    Root dir size:     %lu sectors\n",
			(unsigned long)locinfo.RootDirectory_size);
		printf("    Data offset:       %lu sectors\n",
			(unsigned long)locinfo.Data_offset);
		printf("    Data size:         %lu sectors\n",
			(unsigned long)locinfo.Data_size);
		printf("    Sectors/cluster:   %u\n",
			(unsigned int)locinfo.Sectors_Per_Cluster);
		printf("    Bytes per sector:  %u\n",
			(unsigned int)locinfo.BytesPerSector);
		printf("    Total clusters:    %lu\n",
			(unsigned long)locinfo.Total_clusters);
		printf("    Total sectors:     %lu sectors\n",
			(unsigned long)locinfo.TotalSectors);
		if (locinfo.FAT_size >= 32) {
			printf("    FAT32 FSInfo:      %lu sector\n",
				(unsigned long)locinfo.fat32.BPB_FSInfo);
			printf("    Root dir cluster:  %lu\n",
				(unsigned long)locinfo.fat32.RootDirectory_cluster);
		}

		msfatctx = libmsfat_context_create();
		if (msfatctx == NULL) {
			fprintf(stderr,"Failed to create msfat context\n");
			return -1;
		}
		dfd = dup(fd);
		if (libmsfat_context_assign_fd(msfatctx,dfd)) {
			fprintf(stderr,"Failed to assign file descriptor to msfat\n");
			close(dfd);
			return -1;
		}
		dfd = -1; /* takes ownership, drop it */

		msfatctx->partition_byte_offset = (uint64_t)first_lba * (uint64_t)512UL;
		if (libmsfat_context_set_fat_info(msfatctx,&locinfo)) {
			fprintf(stderr,"msfat library rejected disk location info\n");
			return -1;
		}

		// the first two entries in the FAT filesystem table are the media byte (with extended 1s set) and another end of chain with upper bits used by Win9x for FAT sanity flags
		if (libmsfat_context_read_FAT(msfatctx,&fatent,libmsfat_CLUSTER_0_MEDIA_TYPE)) {
			fprintf(stderr,"Failed to read FAT entry #0\n");
			return -1;
		}
		printf("    FAT entry #0:      0x%08lx\n",
			(unsigned long)fatent);

		if (locinfo.FAT_size == 32 && (fatent & (libmsfat_FAT_entry_t)0x0FFFFF00UL) != (libmsfat_FAT_entry_t)0x0FFFFF00UL)
			fprintf(stderr,"WARNING: upper bits of entry #0 are not all 1's\n");
		else if (locinfo.FAT_size == 16 && (fatent & (libmsfat_FAT_entry_t)0xFF00UL) != (libmsfat_FAT_entry_t)0xFF00UL)
			fprintf(stderr,"WARNING: upper bits of entry #0 are not all 1's\n");
		else if (locinfo.FAT_size == 12 && (fatent & (libmsfat_FAT_entry_t)0xF00UL) != (libmsfat_FAT_entry_t)0xF00UL)
			fprintf(stderr,"WARNING: upper bits of entry #0 are not all 1's\n");
		if (((uint8_t)(fatent&0xFFUL)) != p_bs->BPB_common.BPB_Media)
			fprintf(stderr,"WARNING: media byte in entry #0 does not match media byte type in BPB\n");

		if (libmsfat_context_read_FAT(msfatctx,&fatent,libmsfat_CLUSTER_1_DIRTY_FLAGS)) {
			fprintf(stderr,"Failed to read FAT entry #1\n");
			return -1;
		}
		printf("    FAT entry #1:      0x%08lx\n",
			(unsigned long)fatent);

		// FAT32 and FAT16: check most of the bits are 1, except for ones used by MS-DOS/Win9x for dirty flags
		if (locinfo.FAT_size == 32 && (fatent & (libmsfat_FAT_entry_t)0x03FFFFFFUL) != (libmsfat_FAT_entry_t)0x03FFFFFFUL) // bits 0-25 should be 1's
			fprintf(stderr,"WARNING: entry #1 has 0 bits where all should be 1's\n");
		else if (locinfo.FAT_size == 16 && (fatent & (libmsfat_FAT_entry_t)0x3FFFUL) != (libmsfat_FAT_entry_t)0x3FFFUL) // bits 0-13 should be 1's
			fprintf(stderr,"WARNING: entry #1 has 0 bits where all should be 1's\n");
		// FAT12: MS-DOS/Win9x does not implement dirty flags
		else if (locinfo.FAT_size == 12 && (fatent & (libmsfat_FAT_entry_t)0xFFFUL) != (libmsfat_FAT_entry_t)0xFFFUL) // all bits should be 1's
			fprintf(stderr,"WARNING: entry #1 has 0 bits where all should be 1's\n");
	}

	msfatctx = libmsfat_context_destroy(msfatctx);
	close(fd);
	fd = -1;
	return 0;
}

