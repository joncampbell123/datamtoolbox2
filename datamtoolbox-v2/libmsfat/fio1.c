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

#include <datamtoolbox-v2/libpartmbr/mbrctx.h>
#include <datamtoolbox-v2/libmsfat/libmsfat.h>

static unsigned char			sectorbuf[512];
static struct libmsfat_lfn_assembly_t	lfn_name;

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
};

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

int libmsfat_file_io_ctx_lseek(struct libmsfat_file_io_ctx_t *c,struct libmsfat_context_t *msfatctx,uint32_t offset) {
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
		/* NTS: directories tend not to indicate their size */
		if (offset > c->file_size && !c->is_directory) offset = c->file_size;

		/* if we go backwards, then we have to re-scan the FAT table from the start */
		if (offset < c->cluster_position_start) {
			c->cluster_position = c->first_cluster;
			c->cluster_position_start = 0;
			c->position = 0;
		}

		/* we need a cluster size */
		if (c->cluster_size == (uint32_t)0UL)
			return -1;

		/* what cluster to seek to? */
		offset_cluster_round = offset;
		offset_cluster_offset = offset_cluster_round % c->cluster_size;
		offset_cluster_round -= offset_cluster_offset;

		/* scan forward to desired position */
		while (c->cluster_position_start < offset_cluster_round) {
			if (c->cluster_position < (uint32_t)2UL)
				break;

			c->cluster_position_start += c->cluster_size;
			if (libmsfat_context_read_FAT(msfatctx,&next_cluster_fat,c->cluster_position)) {
				c->cluster_position = 0;
				break;
			}

			if (msfatctx->fatinfo.FAT_size >= 32)
				next_cluster = libmsfat_FAT32_CLUSTER((libmsfat_cluster_t)next_cluster_fat);
			else
				next_cluster = (libmsfat_cluster_t)next_cluster_fat;

			if (libmsfat_context_fat_is_end_of_chain(msfatctx,next_cluster)) {
				c->cluster_position = 0;
				break;
			}

			c->cluster_position = next_cluster;
		}

		/* if we're not in a cluster then we cannot allow an offset */
		if (c->cluster_position < (uint32_t)2UL)
			offset_cluster_offset = 0;

		/* final... */
		c->position = offset_cluster_round + offset_cluster_offset;
	}
	else {
		return -1;
	}

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
		c->is_directory = 1;
		c->is_root_dir = 1;
	}

	if (libmsfat_file_io_ctx_lseek(c,msfatctx,(uint32_t)0UL) || libmsfat_file_io_ctx_tell(c,msfatctx) != (uint32_t)0UL) {
		libmsfat_file_io_ctx_close(c);
		return -1;
	}

	return 0;
}

int libmsfat_file_io_ctx_read(struct libmsfat_file_io_ctx_t *c,struct libmsfat_context_t *msfatctx,void *buffer,size_t len) {
	uint8_t *d = buffer;
	size_t canread;
	size_t doread;
	uint64_t dofs;
	uint32_t ofs;
	int rd = 0;

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
		c->position += rd;
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
			uint32_t npos;

			if (c->position == (uint32_t)0xFFFFFFFFUL)
				break;
			if (c->cluster_position < (uint32_t)2UL)
				break;

			ofs = c->position % c->cluster_size;
			doread = c->cluster_size - ofs;
			if (doread > canread) doread = canread;

			if (libmsfat_context_get_cluster_offset(msfatctx,&dofs,c->cluster_position))
				return -1;

			assert(doread != (uint32_t)0);
			if (msfatctx->read(msfatctx,d,dofs+(uint64_t)ofs,doread))
				return -1;

			npos = c->position+doread;
			if (npos < c->position) { /* integer overflow */
				npos = (uint32_t)0xFFFFFFFFUL;
				doread = npos - c->position;
			}

			d += doread;
			rd += doread;
			canread -= doread;
			if (libmsfat_file_io_ctx_lseek(c,msfatctx,npos))
				return -1;
			if (libmsfat_file_io_ctx_tell(c,msfatctx) != npos)
				break;
		}
	}
	else {
		return -1;
	}

	return rd;
}

////////////////////////////////////////

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
		if (!nohex) {
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
		printf("            Starting cluster:     %lu\n",(unsigned long)libmsfat_dirent_get_starting_cluster(msfatctx,dir));

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
	struct libmsfat_file_io_ctx_t *fioctx = NULL;
	struct libmsfat_context_t *msfatctx = NULL;
	uint32_t first_lba=0,size_lba=0;
	const char *s_partition = NULL;
	const char *s_cluster = NULL;
	libmsfat_cluster_t cluster=0;
	const char *s_image = NULL;
	const char *s_out = NULL;
	unsigned char nohex = 0;
	uint32_t fiooffset = 0;
	int i,fd,out_fd=-1;
	char tmp[256];

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
	if (locinfo.FAT_size >= 32) {
	}
	else if (locinfo.FAT_size < 32 && (locinfo.RootDirectory_offset == (uint32_t)0UL || locinfo.RootDirectory_size == (uint32_t)0UL)) {
		fprintf(stderr,"ERROR: FAT12/FAT16 root directory area does not exist\n");
		return 1;
	}

	libmsfat_lfn_assembly_init(&lfn_name);

	fioctx = libmsfat_file_io_ctx_create();
	if (fioctx == NULL) {
		fprintf(stderr,"Cannot alloc FIO ctx\n");
		return 1;
	}

	if (s_cluster != NULL || cluster != (libmsfat_cluster_t)0UL) {
		if (libmsfat_file_io_ctx_assign_cluster_chain(fioctx,msfatctx,cluster)) {
			fprintf(stderr,"Cannot assign cluster\n");
			return 1;
		}
		fioctx->is_root_dir = 1;
		fioctx->is_directory = 1;
	}
	else {
		if (libmsfat_file_io_ctx_assign_root_directory(fioctx,msfatctx)) {
			fprintf(stderr,"Cannot assign root dir\n");
			return 1;
		}
	}

	printf("***** lseek test ******\n");

	printf("FIO: file_size=%lu position=%lu cluster=%lu cluster_size=%lu first_cluster=%lu cluster_pos=%lu root=%u dir=%u chain=%u\n",
		(unsigned long)fioctx->file_size,
		(unsigned long)fioctx->position,
		(unsigned long)fioctx->cluster_position,
		(unsigned long)fioctx->cluster_size,
		(unsigned long)fioctx->first_cluster,
		(unsigned long)fioctx->cluster_position_start,
		(unsigned int)fioctx->is_root_dir,
		(unsigned int)fioctx->is_directory,
		(unsigned int)fioctx->is_cluster_chain);

	fiooffset = (uint32_t)0;
	do {
		uint32_t pos;

		if (libmsfat_file_io_ctx_lseek(fioctx,msfatctx,fiooffset)) {
			fprintf(stderr,"Failed to lseek to %lu\n",(unsigned long)fiooffset);
			break;
		}
		if ((pos=libmsfat_file_io_ctx_tell(fioctx,msfatctx)) != fiooffset) {
			fprintf(stderr,"Lseek did not reach %lu (got to %lu)\n",(unsigned long)fiooffset,(unsigned long)pos);
			break;
		}

		printf("FIO(%lu): file_size=%lu position=%lu cluster=%lu cluster_size=%lu first_cluster=%lu cluster_pos=%lu root=%u dir=%u chain=%u\n",
			(unsigned long)fiooffset,
			(unsigned long)fioctx->file_size,
			(unsigned long)fioctx->position,
			(unsigned long)fioctx->cluster_position,
			(unsigned long)fioctx->cluster_size,
			(unsigned long)fioctx->first_cluster,
			(unsigned long)fioctx->cluster_position_start,
			(unsigned int)fioctx->is_root_dir,
			(unsigned int)fioctx->is_directory,
			(unsigned int)fioctx->is_cluster_chain);

		fiooffset += (uint32_t)32;
	} while (1);

	if (fiooffset != (uint32_t)0)
		fiooffset -= (uint32_t)32;

	do {
		uint32_t pos;

		if (libmsfat_file_io_ctx_lseek(fioctx,msfatctx,fiooffset)) {
			fprintf(stderr,"Failed to lseek to %lu\n",(unsigned long)fiooffset);
			break;
		}
		if ((pos=libmsfat_file_io_ctx_tell(fioctx,msfatctx)) != fiooffset) {
			fprintf(stderr,"Lseek did not reach %lu (got to %lu)\n",(unsigned long)fiooffset,(unsigned long)pos);
			break;
		}

		printf("FIO2(%lu): file_size=%lu position=%lu cluster=%lu cluster_size=%lu first_cluster=%lu cluster_pos=%lu root=%u dir=%u chain=%u\n",
			(unsigned long)fiooffset,
			(unsigned long)fioctx->file_size,
			(unsigned long)fioctx->position,
			(unsigned long)fioctx->cluster_position,
			(unsigned long)fioctx->cluster_size,
			(unsigned long)fioctx->first_cluster,
			(unsigned long)fioctx->cluster_position_start,
			(unsigned int)fioctx->is_root_dir,
			(unsigned int)fioctx->is_directory,
			(unsigned int)fioctx->is_cluster_chain);

		if (fiooffset == (uint32_t)0) break;
		fiooffset -= (uint32_t)32;
	} while (1);

	printf("***** lseek+read test ******\n");

	printf("FIO: file_size=%lu position=%lu cluster=%lu cluster_size=%lu first_cluster=%lu cluster_pos=%lu root=%u dir=%u chain=%u\n",
		(unsigned long)fioctx->file_size,
		(unsigned long)fioctx->position,
		(unsigned long)fioctx->cluster_position,
		(unsigned long)fioctx->cluster_size,
		(unsigned long)fioctx->first_cluster,
		(unsigned long)fioctx->cluster_position_start,
		(unsigned int)fioctx->is_root_dir,
		(unsigned int)fioctx->is_directory,
		(unsigned int)fioctx->is_cluster_chain);

	fiooffset = (uint32_t)0;
	do {
		uint32_t pos;
		int rd;

		if (libmsfat_file_io_ctx_lseek(fioctx,msfatctx,fiooffset)) {
			fprintf(stderr,"Failed to lseek to %lu\n",(unsigned long)fiooffset);
			break;
		}
		if ((pos=libmsfat_file_io_ctx_tell(fioctx,msfatctx)) != fiooffset) {
			fprintf(stderr,"Lseek did not reach %lu (got to %lu)\n",(unsigned long)fiooffset,(unsigned long)pos);
			break;
		}
		if ((rd=libmsfat_file_io_ctx_read(fioctx,msfatctx,tmp,32)) != 32) {
			fprintf(stderr,"read did not get 32 bytes (rd=%d)\n",rd);
			break;
		}

		printf("FIO(%lu): file_size=%lu position=%lu cluster=%lu cluster_size=%lu first_cluster=%lu cluster_pos=%lu root=%u dir=%u chain=%u\n",
			(unsigned long)fiooffset,
			(unsigned long)fioctx->file_size,
			(unsigned long)fioctx->position,
			(unsigned long)fioctx->cluster_position,
			(unsigned long)fioctx->cluster_size,
			(unsigned long)fioctx->first_cluster,
			(unsigned long)fioctx->cluster_position_start,
			(unsigned int)fioctx->is_root_dir,
			(unsigned int)fioctx->is_directory,
			(unsigned int)fioctx->is_cluster_chain);

		if (!nohex) {
			printf("   GOT: ");
			for (i=0;i < 32;i++) printf("%02x ",(uint8_t)tmp[i]);
			printf("\n");
		}

		print_dirent(msfatctx,(const struct libmsfat_dirent_t*)tmp,nohex);

		fiooffset += (uint32_t)32;
	} while (1);

	if (fiooffset != (uint32_t)0)
		fiooffset -= (uint32_t)32;

	do {
		uint32_t pos;
		int rd;

		if (libmsfat_file_io_ctx_lseek(fioctx,msfatctx,fiooffset)) {
			fprintf(stderr,"Failed to lseek to %lu\n",(unsigned long)fiooffset);
			break;
		}
		if ((pos=libmsfat_file_io_ctx_tell(fioctx,msfatctx)) != fiooffset) {
			fprintf(stderr,"Lseek did not reach %lu (got to %lu)\n",(unsigned long)fiooffset,(unsigned long)pos);
			break;
		}
		if ((rd=libmsfat_file_io_ctx_read(fioctx,msfatctx,tmp,32)) != 32) {
			fprintf(stderr,"read did not get 32 bytes (rd=%d)\n",rd);
			break;
		}

		printf("FIO2(%lu): file_size=%lu position=%lu cluster=%lu cluster_size=%lu first_cluster=%lu cluster_pos=%lu root=%u dir=%u chain=%u\n",
			(unsigned long)fiooffset,
			(unsigned long)fioctx->file_size,
			(unsigned long)fioctx->position,
			(unsigned long)fioctx->cluster_position,
			(unsigned long)fioctx->cluster_size,
			(unsigned long)fioctx->first_cluster,
			(unsigned long)fioctx->cluster_position_start,
			(unsigned int)fioctx->is_root_dir,
			(unsigned int)fioctx->is_directory,
			(unsigned int)fioctx->is_cluster_chain);

		if (!nohex) {
			printf("   GOT: ");
			for (i=0;i < 32;i++) printf("%02x ",(uint8_t)tmp[i]);
			printf("\n");
		}

		print_dirent(msfatctx,(const struct libmsfat_dirent_t*)tmp,nohex);

		if (fiooffset == (uint32_t)0) break;
		fiooffset -= (uint32_t)32;
	} while (1);

	if (out_fd >= 0) close(out_fd);
	out_fd = -1;

	fioctx = libmsfat_file_io_ctx_destroy(fioctx);
	msfatctx = libmsfat_context_destroy(msfatctx);
	close(fd);
	fd = -1;
	return 0;
}

