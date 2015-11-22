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
# define dup _dup
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

static uint16_t				lfn_name_assembly[(5+6+2)*32];
static uint8_t				lfn_name_present[32];
static uint8_t				lfn_name_chksum[32];
static uint8_t				lfn_name_max = 0;

static void lfn_discontinuity() {
	if (lfn_name_max == 0) return;
	printf("LFN error: failed to assemble name\n");
	lfn_name_max = 0;
}

static void lfn_complete(const struct libmsfat_dirent_t *dir) { // non LFN entry
	uint8_t cksumerr=0,missing=0;
	uint8_t chksum;
	unsigned int i;
	uint16_t uc;

	if (lfn_name_max == 0) return;
	chksum = libmsfat_lfn_83_checksum_dirent(dir);

	for (i=0;i < lfn_name_max;i++) {
		if (lfn_name_present[i]) {
			if (lfn_name_chksum[i] != chksum) {
				fprintf(stderr,"LFN checksum mismatch: 0x%02x != 0x%02x\n",
					(unsigned int)lfn_name_chksum[i],
					(unsigned int)chksum);
				cksumerr=1;
			}
		}
		else {
			missing=1;
		}
	}

	if (missing)
		fprintf(stderr,"LFN error: One or more segments are missing\n");
	else if (cksumerr)
		fprintf(stderr,"LFN error: Assembled LFN checksum mismatch against 8.3 name\n");
	else {
		uint8_t tmp[((5U+6U+2U)*32U)+1U];

		for (i=0;i < ((5U+6U+2U)*lfn_name_max);i++) {
			uc = lfn_name_assembly[i];
			if (uc == 0) break;

			if (uc >= 0x80U) tmp[i] = '?';
			else tmp[i] = (char)uc;
		}
		assert(i < sizeof(tmp));
		tmp[i] = 0;

		printf("            >> LFN name: '%s'\n",tmp);
	}

	lfn_name_max = 0;
}

static void lfn_assemble(const struct libmsfat_dirent_t *dir) {
	uint8_t ord;

	if (lfn_name_max == 0) {
		if (!(dir->a.lfn.LDIR_Ord & 0x40)) { // must be marked as first
			fprintf(stderr,"LFN error: unexpected LFN entry #%u that is not the start\n",
				(unsigned int)(dir->a.lfn.LDIR_Ord & 0x3F));
			return;
		}

		lfn_name_max = dir->a.lfn.LDIR_Ord & 0x3F;
		if (lfn_name_max == 0 || lfn_name_max > 32) {
			fprintf(stderr,"LFN error: first LFN entry has very large ordinal\n");
			lfn_name_max = 0;
			return;
		}

		memset(lfn_name_present,0,lfn_name_max);
		memset(lfn_name_chksum,0,lfn_name_max);
		memset(lfn_name_assembly,0,sizeof(uint16_t)*lfn_name_max);
	}
	else {
		if (dir->a.lfn.LDIR_Ord & 0x40) { // another first entry??
			fprintf(stderr,"LFN error: unexpected LFN first entry #%u while assembling an LFN\n",
				(unsigned int)(dir->a.lfn.LDIR_Ord & 0x3F));
			lfn_name_max = 0;
			return;
		}
		else if (dir->a.lfn.LDIR_Ord == 0 || dir->a.lfn.LDIR_Ord > lfn_name_max) {
			fprintf(stderr,"LFN error: LFN entry #%u out of range\n",
				(unsigned int)(dir->a.lfn.LDIR_Ord & 0x3F));
			lfn_name_max = 0;
			return;
		}
	}

	ord = dir->a.lfn.LDIR_Ord & 0x3F;
	assert(ord > 0);
	ord--;
	assert(ord < lfn_name_max);

	if (lfn_name_present[ord]) {
		fprintf(stderr,"LFN error: duplicate entry #%u\n",
			(unsigned int)ord);
		lfn_name_max = 0;
		return;
	}

	{
		uint16_t *d = lfn_name_assembly + (ord * (5+6+2));
		unsigned int i;

		assert(((uint8_t*)(d+(5+6+2))) <= ((uint8_t*)lfn_name_assembly + sizeof(lfn_name_assembly)));

		for (i=0;i < 5;i++)
			d[i+0] = dir->a.lfn.LDIR_Name1[i];
		for (i=0;i < 6;i++)
			d[i+5] = dir->a.lfn.LDIR_Name2[i];
		for (i=0;i < 2;i++)
			d[i+11] = dir->a.lfn.LDIR_Name3[i];
	}

	lfn_name_chksum[ord] = dir->a.lfn.LDIR_Chksum;
	lfn_name_present[ord] = 1;
}

static void dirent_assemble_lfn(const struct libmsfat_context_t *msfatctx,const struct libmsfat_dirent_t *dir) {
	if (dir->a.n.DIR_Name[0] == 0x00 || dir->a.n.DIR_Name[0] == 0xE5)
		lfn_discontinuity();
	else if ((dir->a.n.DIR_Attr & libmsfat_DIR_ATTR_MASK) == libmsfat_DIR_ATTR_LONG_NAME)
		lfn_assemble(dir);
	else if ((dir->a.n.DIR_Attr & libmsfat_DIR_ATTR_VOLUME_ID) != 0)
		lfn_discontinuity();
	else
		lfn_complete(dir);
}

static void print_dirent(const struct libmsfat_context_t *msfatctx,const struct libmsfat_dirent_t *dir,const uint8_t nohex) {
	if (dir->a.n.DIR_Name[0] == 0) {
		if (!nohex)
			printf("    <EMPTY, end of dir>\n");
	}
	else if (dir->a.n.DIR_Name[0] == 0xE5) {
		if (!nohex)
			printf("    <DELETED>\n");
	}
	else if ((dir->a.n.DIR_Attr & libmsfat_DIR_ATTR_MASK) == libmsfat_DIR_ATTR_LONG_NAME) {
		printf("    <LFN> checksum 0x%02x type %u entry %u ",
			(unsigned int)dir->a.lfn.LDIR_Chksum,
			(unsigned int)dir->a.lfn.LDIR_Type,
			(unsigned int)dir->a.lfn.LDIR_Ord & 0x3F);
		if (dir->a.lfn.LDIR_Ord & 0x40) printf(" LAST ENTRY");

		if (dir->a.lfn.LDIR_Type == 0x00) {
			const uint16_t *s;
			char name[5+6+2+1+1];
			char *d = name;
			char *df = name + sizeof(name);
			unsigned int i,end=0;

			s = dir->a.lfn.LDIR_Name1;
			for (i=0;i < 5 && !end;i++) {
				uint16_t uc = s[i];
				if (uc == 0) {
					end = 1;
					break;
				}

				if (uc >= 0x80U) *d++ = '?';
				else *d++ = (char)(uc & 0xFF);
			}
			assert(d < df);

			s = dir->a.lfn.LDIR_Name2;
			for (i=0;i < 6 && !end;i++) {
				uint16_t uc = s[i];
				if (uc == 0) {
					end = 1;
					break;
				}

				if (uc >= 0x80U) *d++ = '?';
				else *d++ = (char)(uc & 0xFF);
			}
			assert(d < df);

			s = dir->a.lfn.LDIR_Name3;
			for (i=0;i < 2 && !end;i++) {
				uint16_t uc = s[i];
				if (uc == 0) {
					end = 1;
					break;
				}

				if (uc >= 0x80U) *d++ = '?';
				else *d++ = (char)(uc & 0xFF);
			}
			assert(d < df);
			*d = 0;

			printf(" fragment: '%s'\n",name);
		}
	}
	else if (dir->a.n.DIR_Attr & libmsfat_DIR_ATTR_VOLUME_ID) {
		const char *s = dir->a.n.DIR_Name,*se;
		char name[12];
		char *df = name + sizeof(name);
		char *d = name;

		se = dir->a.n.DIR_Ext+2;
		while (se >= s && (*se == ' ' || *se == 0)) se--; /* scan backwards past trailing spaces */
		se++;
		while (s < se) *d++ = *s++;
		assert(d < df);
		*d = 0;

		printf("    <VOLUME LABEL>   '%s'\n",name);
	}
	else {
		const char *s = dir->a.n.DIR_Name,*se;
		char name[8+1+3+1+1]; /* <NAME>.<EXT><NUL> */
		char *df = name + sizeof(name);
		char *d = name;

		/* if the first byte is 0x05, the actual char is 0xE5. Japanese SHIFT-JIS hack, apparently */
		if (*s == 0x05) {
			*d++ = (char)0xE5;
			s++;
		}
		se = dir->a.n.DIR_Name+7;
		while (se >= s && (*se == ' ' || *se == 0)) se--; /* scan backwards past trailing spaces */
		se++;
		while (s < se) *d++ = *s++;
		assert(d < df);

		s = dir->a.n.DIR_Ext;
		if (!(*s == ' ' || *s == 0)) {
			*d++ = '.';
			se = dir->a.n.DIR_Ext+2;
			while (se >= s && (*se == ' ' || *se == 0)) se--; /* scan backwards past trailing spaces */
			se++;
			while (s < se) *d++ = *s++;
			assert(d < df);
		}
		*d = 0;
		assert(d < df);

		if (dir->a.n.DIR_Attr & libmsfat_DIR_ATTR_DIRECTORY)
			printf("    <DIR>   ");
		else
			printf("    <FILE>  ");

		printf("%c",(dir->a.n.DIR_Attr & libmsfat_DIR_ATTR_READ_ONLY)?'R':'-');
		printf("%c",(dir->a.n.DIR_Attr & libmsfat_DIR_ATTR_HIDDEN)?'H':'-');
		printf("%c",(dir->a.n.DIR_Attr & libmsfat_DIR_ATTR_SYSTEM)?'S':'-');
		printf("%c",(dir->a.n.DIR_Attr & libmsfat_DIR_ATTR_ARCHIVE)?'A':'-');

		printf("  '%s'\n",name);

		printf("            File size:            %lu bytes\n",(unsigned long)le32toh(dir->a.n.DIR_FileSize));

		if (msfatctx->fatinfo.FAT_size == 32) {
			libmsfat_cluster_t c;

			c = (libmsfat_cluster_t)le16toh(dir->a.n.DIR_FstClusLO);
			c += (libmsfat_cluster_t)le16toh(dir->a.n.DIR_FstClusHI) << (libmsfat_cluster_t)16UL;
			printf("            Starting cluster:     %lu\n",(unsigned long)c);
		}
		else {
			printf("            Starting cluster:     %lu\n",(unsigned long)le16toh(dir->a.n.DIR_FstClusLO));
		}

		{
			struct libmsfat_msdos_date_t d;
			struct libmsfat_msdos_time_t t;

			d.a.raw = le16toh(dir->a.n.DIR_CrtDate.a.raw);
			t.a.raw = le16toh(dir->a.n.DIR_CrtTime.a.raw);

			if (d.a.raw != 0 || t.a.raw != 0) {
				printf("            File creation:        %04u-%02u-%02u %02u:%02u:%02u.%02u\n",
					1980 + d.a.f.years_since_1980,
					d.a.f.month_of_year,
					d.a.f.day_of_month,
					t.a.f.hours,
					t.a.f.minutes,
					(t.a.f.seconds2 * 2U) + (dir->a.n.DIR_CrtTimeTenth / 100),
					dir->a.n.DIR_CrtTimeTenth % 100);
			}
		}

		{
			struct libmsfat_msdos_date_t d;
			struct libmsfat_msdos_time_t t;

			d.a.raw = le16toh(dir->a.n.DIR_WrtDate.a.raw);
			t.a.raw = le16toh(dir->a.n.DIR_WrtTime.a.raw);

			if (d.a.raw != 0 || t.a.raw != 0) {
				printf("            File last modified:   %04u-%02u-%02u %02u:%02u:%02u\n",
					1980 + d.a.f.years_since_1980,
					d.a.f.month_of_year,
					d.a.f.day_of_month,
					t.a.f.hours,
					t.a.f.minutes,
					t.a.f.seconds2 * 2U);
			}
		}

		{
			struct libmsfat_msdos_date_t d;

			d.a.raw = le16toh(dir->a.n.DIR_LstAccDate.a.raw);

			if (d.a.raw != 0) {
				printf("            File last accessed:   %04u-%02u-%02u\n",
					1980 + d.a.f.years_since_1980,
					d.a.f.month_of_year,
					d.a.f.day_of_month);
			}
		}
	}
}

int main(int argc,char **argv) {
	struct libmsfat_disk_locations_and_info locinfo;
	struct libmsfat_context_t *msfatctx = NULL;
	uint32_t first_lba=0,size_lba=0;
	const char *s_partition = NULL;
	const char *s_cluster = NULL;
	libmsfat_cluster_t cluster=0;
	const char *s_image = NULL;
	const char *s_out = NULL;
	unsigned char nohex = 0;
	int i,fd,out_fd=-1;

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
			else if (!strcmp(a,"cluster")) {
				s_cluster = argv[i++];
			}
			else if (!strcmp(a,"nohex")) {
				nohex = 1;
			}
			else if (!strcmp(a,"o") || !strcmp(a,"out")) {
				s_out = argv[i++];
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
		fprintf(stderr,"--cluster <n>            Which cluster to start from (if not root dir)\n");
		fprintf(stderr,"-o <file>                Dump directory to file\n");
		fprintf(stderr,"--nohex                  Don't hex dump to STDOUT\n");
		return 1;
	}
	if (s_cluster != NULL)
		cluster = (libmsfat_cluster_t)strtoul(s_cluster,NULL,0);
	else
		cluster = 0;

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
		const char *err_str = NULL;

		if (!libmsfat_boot_sector_is_valid(sectorbuf,&err_str)) {
			printf("Boot sector is not valid: reason=%s\n",err_str);
			return 1;
		}
	}

	{
		struct libmsfat_bootsector *p_bs = (struct libmsfat_bootsector*)sectorbuf;
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
		printf("    Total data clusts: %lu\n",
			(unsigned long)locinfo.Total_data_clusters);
		printf("    Max clusters psbl: %lu\n",
			(unsigned long)locinfo.Max_possible_clusters);
		printf("    Max data clus psbl:%lu\n",
			(unsigned long)locinfo.Max_possible_data_clusters);
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
	}

	/* FAT32 always starts from a cluster.
	 * FAT12/FAT16 have a dedicated area for the root directory */
	if (locinfo.FAT_size == 32) {
		if (s_cluster == NULL)
			cluster = locinfo.fat32.RootDirectory_cluster;
	}

	if (cluster >= locinfo.Max_possible_clusters)
		fprintf(stderr,"WARNING: cluster %lu >= max possible clusters %lu\n",
			(unsigned long)cluster,
			(unsigned long)locinfo.Max_possible_clusters);
	else if (cluster >= locinfo.Total_clusters)
		fprintf(stderr,"WARNING: cluster %lu >= total clusters %lu\n",
			(unsigned long)cluster,
			(unsigned long)locinfo.Total_clusters);
	else if ((s_cluster != NULL || cluster != (libmsfat_cluster_t)0UL) && cluster < (libmsfat_cluster_t)2UL)
		fprintf(stderr,"WARNING: cluster %lu is known not to have corresponding data storage on disk\n",
			(unsigned long)cluster);

	if (s_cluster != NULL || cluster != (libmsfat_cluster_t)0UL)
		printf("Reading directory starting from cluster %lu\n",
			(unsigned long)cluster);
	else
		printf("Reading directory from FAT12/FAT16 root directory area\n");

	if (s_out != NULL) {
		out_fd = open(s_out,O_CREAT|O_TRUNC|O_BINARY|O_WRONLY,0644);
		if (out_fd < 0) {
			fprintf(stderr,"Failed to create dump file\n");
			return 1;
		}
	}

	/* sanity check */
	if (locinfo.FAT_size >= 32 && cluster < (libmsfat_cluster_t)2UL) {
		fprintf(stderr,"ERROR: FAT32 requires a root directory cluster to start from. There is no\n");
		fprintf(stderr,"       dedicated root directory area like what FAT12/FAT16 has.\n");
		return 1;
	}
	else if (locinfo.FAT_size < 32 && (locinfo.RootDirectory_offset == (uint32_t)0UL || locinfo.RootDirectory_size == (uint32_t)0UL)) {
		fprintf(stderr,"ERROR: FAT12/FAT16 root directory area does not exist\n");
		return 1;
	}

	if (cluster >= (libmsfat_cluster_t)2UL) {
		libmsfat_cluster_t next_cluster;
		uint32_t rd,cnt,scan;
		uint64_t offset;
		uint64_t sector;
		uint32_t clsz;
		uint8_t col;

		clsz = libmsfat_context_get_cluster_size(msfatctx);
		if (clsz == (uint32_t)0) {
			fprintf(stderr,"Unable to get cluster size\n");
			return 1;
		}
		printf("    Cluster size:      %lu bytes\n",
			(unsigned long)clsz);
		printf("\n");

		do {
			if (libmsfat_context_get_cluster_sector(msfatctx,&sector,cluster)) {
				fprintf(stderr,"Failed to map sector from cluster number %lu\n",(unsigned long)cluster);
				return 1;
			}
			if (libmsfat_context_get_cluster_offset(msfatctx,&offset,cluster)) {
				fprintf(stderr,"Failed to map offset from cluster number %lu\n",(unsigned long)cluster);
				return 1;
			}
			if (libmsfat_context_read_FAT(msfatctx,&next_cluster,cluster)) {
				fprintf(stderr,"WARNING: unable to read FAT entry for cluster #%lu\n",(unsigned long)cluster);
				next_cluster = libmsfat_FAT32_CLUSTER_MASK;
			}
			if (msfatctx->fatinfo.FAT_size == 32)
				next_cluster &= libmsfat_FAT32_CLUSTER_MASK;

			printf("    Cluster:           #%lu starts at sector %llu (%llu bytes)",
				(unsigned long)cluster,
				(unsigned long long)sector,
				(unsigned long long)offset);
			if (!libmsfat_context_fat_is_end_of_chain(msfatctx,next_cluster))
				printf(" -> next cluster is %lu",(unsigned long)next_cluster);
			printf("\n");

			col = 0;
			cnt = 0;
			rd = clsz;
			while (rd != 0) {
				uint32_t rdsz;

				rdsz = rd;
				if (rdsz > (uint32_t)sizeof(sectorbuf)) rdsz = (uint32_t)sizeof(sectorbuf);

				if (libmsfat_context_read_disk(msfatctx,sectorbuf,offset,rdsz)) {
					fprintf(stderr,"Failed to read from disk offset %llu\n",(unsigned long long)offset);
					break;
				}

				if (out_fd >= 0) {
					if (write(out_fd,sectorbuf,(size_t)rdsz) != (int)rdsz) {
						fprintf(stderr,"Failed to write to dump file\n");
						break;
					}
				}

				if (!nohex) {
					for (scan=0;scan < rdsz;scan++) {
						if (col == 0) printf("    0x%08lx: ",(unsigned long)cnt);

						printf("%02x ",sectorbuf[scan]);

						cnt++;
						if ((++col) >= 32) {
							printf("\n");
							if ((unsigned int)scan >= (unsigned int)31) {
								unsigned char *p = sectorbuf + scan - 31;
								assert((scan % 32) == 31);
								print_dirent(msfatctx,(struct libmsfat_dirent_t*)p,nohex);
								dirent_assemble_lfn(msfatctx,(struct libmsfat_dirent_t*)p);
							}
							col = 0;
						}
					}
				}
				else {
					for (scan=0;scan < rdsz;scan++) {
						cnt++;
						if ((++col) >= 32) {
							if ((unsigned int)scan >= (unsigned int)31) {
								unsigned char *p = sectorbuf + scan - 31;
								assert((scan % 32) == 31);
								print_dirent(msfatctx,(struct libmsfat_dirent_t*)p,nohex);
								dirent_assemble_lfn(msfatctx,(struct libmsfat_dirent_t*)p);
							}
							col = 0;
						}
					}
				}

				offset += (uint64_t)rdsz;
				rd -= rdsz;
			}

			if (col != 0 && !nohex) printf("\n");

			if (next_cluster == (libmsfat_cluster_t)1UL) {
				fprintf(stderr,"Next cluster is not a valid data cluster. Stopping now.\n");
				break;
			}
			else if (next_cluster == cluster) {
				fprintf(stderr,"Next cluster is the same as the current cluster. Stopping now.\n");
				break;
			}
			else if (libmsfat_context_fat_is_end_of_chain(msfatctx,next_cluster)) {
				break;
			}

			cluster = next_cluster;
		} while (1);
	}
	else {
		uint32_t rd,cnt,scan;
		uint64_t offset;
		uint64_t sector;
		uint32_t clsz;
		uint8_t col;

		sector  = (uint64_t)msfatctx->fatinfo.RootDirectory_offset;
		offset  = (uint64_t)sector * msfatctx->fatinfo.BytesPerSector;
		offset += (uint64_t)msfatctx->partition_byte_offset;

		printf("    Root directory starts at sector %llu (%llu bytes)\n",
			(unsigned long long)sector,
			(unsigned long long)offset);

		clsz = (uint32_t)msfatctx->fatinfo.BytesPerSector * (uint32_t)msfatctx->fatinfo.RootDirectory_size;;
		if (clsz == (uint32_t)0) {
			fprintf(stderr,"Unable to get cluster size\n");
			return 1;
		}
		printf("    Root dir size:     %lu bytes\n",
			(unsigned long)clsz);
		printf("\n");

		col = 0;
		cnt = 0;
		rd = clsz;
		while (rd != 0) {
			uint32_t rdsz;

			rdsz = rd;
			if (rdsz > (uint32_t)sizeof(sectorbuf)) rdsz = (uint32_t)sizeof(sectorbuf);

			if (libmsfat_context_read_disk(msfatctx,sectorbuf,offset,rdsz)) {
				fprintf(stderr,"Failed to read from disk offset %llu\n",(unsigned long long)offset);
				break;
			}

			if (out_fd >= 0) {
				if (write(out_fd,sectorbuf,(size_t)rdsz) != (int)rdsz) {
					fprintf(stderr,"Failed to write to dump file\n");
					break;
				}
			}

			if (!nohex) {
				for (scan=0;scan < rdsz;scan++) {
					if (col == 0) printf("    0x%08lx: ",(unsigned long)cnt);

					printf("%02x ",sectorbuf[scan]);

					cnt++;
					if ((++col) >= 32) {
						printf("\n");
						if ((unsigned int)scan >= (unsigned int)31) {
							unsigned char *p = sectorbuf + scan - 31;
							assert((scan % 32) == 31);
							print_dirent(msfatctx,(struct libmsfat_dirent_t*)p,nohex);
							dirent_assemble_lfn(msfatctx,(struct libmsfat_dirent_t*)p);
						}
						col = 0;
					}
				}
			}
			else {
				for (scan=0;scan < rdsz;scan++) {
					cnt++;
					if ((++col) >= 32) {
						if ((unsigned int)scan >= (unsigned int)31) {
							unsigned char *p = sectorbuf + scan - 31;
							assert((scan % 32) == 31);
							print_dirent(msfatctx,(struct libmsfat_dirent_t*)p,nohex);
							dirent_assemble_lfn(msfatctx,(struct libmsfat_dirent_t*)p);
						}
						col = 0;
					}
				}
			}

			offset += (uint64_t)rdsz;
			rd -= rdsz;
		}

		if (col != 0 && !nohex) printf("\n");

	}

	if (out_fd >= 0) close(out_fd);
	out_fd = -1;

	msfatctx = libmsfat_context_destroy(msfatctx);
	close(fd);
	fd = -1;
	return 0;
}

