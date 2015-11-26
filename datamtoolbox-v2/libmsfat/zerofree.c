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
# include <windows.h>
#endif
#include <datamtoolbox-v2/polyfill/lseek.h>
#include <datamtoolbox-v2/polyfill/unix.h>

#include <datamtoolbox-v2/libpartmbr/mbrctx.h>
#include <datamtoolbox-v2/libmsfat/libmsfat.h>
#include <datamtoolbox-v2/libmsfat/libmsfat_unicode.h>

static unsigned char			sectorbuf[512];
static unsigned char			buffer[4096];

void clean_directory(struct libmsfat_file_io_ctx_t *fioctx,struct libmsfat_file_io_ctx_t *fioctx_parent,struct libmsfat_context_t *msfatctx,struct libmsfat_dirent_t *dir_dirent) {
	struct libmsfat_dirent_t dirent;
	uint32_t truncate=0;
	uint32_t ro=0,wo=0;

	while (1) {
		if (libmsfat_file_io_ctx_lseek(fioctx,msfatctx,ro))
			break;
		if (libmsfat_file_io_ctx_tell(fioctx,msfatctx) != ro)
			break;
		if (libmsfat_file_io_ctx_read(fioctx,msfatctx,&dirent,sizeof(dirent)) != sizeof(dirent))
			break;

		/* skip empty/deleted dirent entries */
		if (dirent.a.n.DIR_Name[0] == 0x00 || dirent.a.n.DIR_Name[0] == (char)0xE5) {
			ro += (uint32_t)sizeof(dirent);
			continue;
		}

		/* if ro != wo, move the dirent back */
		if (ro != wo) {
			if (libmsfat_file_io_ctx_lseek(fioctx,msfatctx,wo))
				break;
			if (libmsfat_file_io_ctx_tell(fioctx,msfatctx) != wo)
				break;
			if (libmsfat_file_io_ctx_write(fioctx,msfatctx,&dirent,sizeof(dirent)) != sizeof(dirent))
				break;
		}

		ro += (uint32_t)sizeof(dirent);
		wo += (uint32_t)sizeof(dirent);
	}

	/* take note where the write pointer is. we will truncate there after zeroing the directory.
	 * but, do not truncate the directory to zero! */
	truncate = wo;
	if (truncate == (uint32_t)0) truncate = (uint32_t)sizeof(dirent);

	/* fill out the rest of the directory with zeros */
	memset(&dirent,0,sizeof(dirent));
	if (libmsfat_file_io_ctx_lseek(fioctx,msfatctx,wo))
		return;
	if (libmsfat_file_io_ctx_tell(fioctx,msfatctx) != wo)
		return;
	while (libmsfat_file_io_ctx_write(fioctx,msfatctx,&dirent,sizeof(dirent)) == sizeof(dirent))
		{ };

	/* truncate, if possible (if the directory is based on a cluster chain) */
	if (fioctx->is_cluster_chain) {
		if (libmsfat_file_io_ctx_truncate_file(fioctx,fioctx_parent,msfatctx,dir_dirent,NULL,truncate))
			fprintf(stderr,"ERROR: failed to truncate directory\n");
	}
}

int main(int argc,char **argv) {
	struct libmsfat_disk_locations_and_info locinfo;
	struct libmsfat_file_io_ctx_t *fioctx_parent = NULL;
	struct libmsfat_file_io_ctx_t *fioctx = NULL;
	struct libmsfat_context_t *msfatctx = NULL;
	uint32_t first_lba=0,size_lba=0;
	const char *s_partition = NULL;
	libmsfat_FAT_entry_t fatent;
	uint32_t bytes_per_cluster;
	const char *s_image = NULL;
	libmsfat_cluster_t cluster;
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
		fprintf(stderr,"zerofree --image <image> ...\n");
		fprintf(stderr,"zero out all areas of the disk/partition that are not allocated by the filesystem.\n");
		fprintf(stderr,"\n");
		fprintf(stderr,"--partition <n>          Hard disk image, use partition N from the MBR\n");
		return 1;
	}

	fd = open(s_image,O_RDWR|O_BINARY);
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
		if (ent->entry.number_lba_sectors == (uint32_t)0) {
			fprintf(stderr,"Partition has no size\n");
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

		if (lseek64(fd,x,SEEK_SET) != x || read(fd,sectorbuf,512) != 512) {
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

	fioctx_parent = libmsfat_file_io_ctx_create();
	if (fioctx_parent == NULL) {
		fprintf(stderr,"Cannot alloc FIO ctx\n");
		return 1;
	}

	fioctx = libmsfat_file_io_ctx_create();
	if (fioctx == NULL) {
		fprintf(stderr,"Cannot alloc FIO ctx\n");
		return 1;
	}

	printf("Scanning directory structure, to clean and repack diretories\n");
	if (libmsfat_file_io_ctx_assign_root_directory_with_parent(fioctx,fioctx_parent,msfatctx) == 0)
		clean_directory(fioctx,fioctx_parent,msfatctx,NULL);
	else
		fprintf(stderr,"Unable to assign root directory\n");

	bytes_per_cluster = libmsfat_context_get_cluster_size(msfatctx);
	if (bytes_per_cluster == (uint32_t)0) {
		fprintf(stderr,"Unable to determine bytes per cluster\n");
		return 1;
	}

	printf("Scanning FAT table and zeroing unallocated clusters.\n");
	for (cluster=(libmsfat_cluster_t)2;cluster < msfatctx->fatinfo.Total_clusters;cluster++) {
		int percent;

		if (libmsfat_context_read_FAT(msfatctx,&fatent,cluster,0)) {
			fprintf(stderr,"\nERROR: unable to read FAT table entry #%lu\n",
				(unsigned long)cluster);
			continue;
		}

		percent = (int)(((uint64_t)cluster * (uint64_t)100UL) / msfatctx->fatinfo.Total_clusters);

		if (((uint32_t)cluster & 0xFF) == (uint32_t)0) {
			printf("\x0D" "Reading cluster %lu / %lu (%%%u) ",
				(unsigned long)cluster,
				(unsigned long)msfatctx->fatinfo.Total_clusters,
				percent);
			fflush(stdout);
		}

		if (cluster < (libmsfat_cluster_t)2UL) {
			uint32_t sz = bytes_per_cluster,wr;
			uint64_t offset;

			/* cluster is free! zero it! */
			if (libmsfat_context_get_cluster_offset(msfatctx,&offset,cluster)) {
				fprintf(stderr,"\nERROR: unable to locate cluster #%lu\n",
					(unsigned long)cluster);
				continue;
			}

			memset(buffer,0,sizeof(buffer));
			while (sz != (uint32_t)0) {
				if (sz > sizeof(buffer))
					wr = sizeof(buffer);
				else
					wr = sz;

				if (msfatctx->write(msfatctx,buffer,offset,(size_t)wr)) {
					fprintf(stderr,"\nERROR: write error to offset %llu\n",(unsigned long long)offset);
					break;
				}

				offset += (uint64_t)wr;
				sz -= wr;
			}
		}
	}
	printf("\nDone!\n");

	/* this code focuses on FAT table #0. make sure to copy FAT 0 to FAT 1/2/3 etc. */
	for (i=1;i < (int)msfatctx->fatinfo.FAT_tables;i++) {
		if (libmsfat_context_copy_FAT(msfatctx,/*dst*/i,/*src*/0))
			fprintf(stderr,"Problem copying FAT table\n");
	}

	/* FAT32: Need to update free cluster count */
	if (msfatctx->fatinfo.FAT_size == 32) {
		if (libmsfat_context_update_fat32_free_cluster_count(msfatctx))
			fprintf(stderr,"Problem updating FSInfo free cluster count\n");
	}

	/* done */
	fioctx_parent = libmsfat_file_io_ctx_destroy(fioctx_parent);
	fioctx = libmsfat_file_io_ctx_destroy(fioctx);
	msfatctx = libmsfat_context_destroy(msfatctx);
	close(fd);
	fd = -1;
	return 0;
}

