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
#include <datamtoolbox-v2/polyfill/stat.h>
#include <datamtoolbox-v2/polyfill/unix.h>

#include <datamtoolbox-v2/libpartmbr/mbrctx.h>
#include <datamtoolbox-v2/libmsfat/libmsfat.h>
#include <datamtoolbox-v2/libmsfat/libmsfat_unicode.h>

static int				do_mkdir = 0;
static unsigned char			sectorbuf[512];
static const char*			msg_add = NULL;
static int				msg_rep = 1;

int main(int argc,char **argv) {
	struct libmsfat_disk_locations_and_info locinfo;
	struct libmsfat_file_io_ctx_t *fioctx_parent = NULL;
	struct libmsfat_file_io_ctx_t *fioctx = NULL;
	struct libmsfat_context_t *msfatctx = NULL;
	struct libmsfat_lfn_assembly_t lfn_name;
	struct libmsfat_dirent_t dirent;
	uint32_t first_lba=0,size_lba=0;
	const char *s_partition = NULL;
	const char *s_image = NULL;
	const char *s_path = NULL;
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
			else if (!strcmp(a,"path")) {
				s_path = argv[i++];
			}
			else if (!strcmp(a,"m")) {
				msg_add = argv[i++];
			}
			else if (!strcmp(a,"c")) {
				msg_rep = atoi(argv[i++]);
			}
			else if (!strcmp(a,"dir")) {
				do_mkdir = 1;
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

	if (s_image == NULL || s_path == NULL || msg_add == NULL) {
		fprintf(stderr,"fappend --image <image> -m <message> ...\n");
		fprintf(stderr,"zero out all areas of the disk/partition that are not allocated by the filesystem.\n");
		fprintf(stderr,"\n");
		fprintf(stderr,"--dir                    Create a directory, not a file\n");
		fprintf(stderr,"--partition <n>          Hard disk image, use partition N from the MBR\n");
		fprintf(stderr,"-c <N>                   Number of times to write the message\n");
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
		if (_polyfill_fstat(fd,&st) || (!S_ISREG(st.st_mode) && !S_ISBLK(st.st_mode))) {
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

		x = _polyfill_lseek(fd,0,SEEK_END);
		if (x < (lseek_off_t)0) x = 0;
		x /= (lseek_off_t)512UL;
		if (x > (lseek_off_t)0xFFFFFFFFUL) x = (lseek_off_t)0xFFFFFFFFUL;
		size_lba = (uint32_t)x;
		_polyfill_lseek(fd,0,SEEK_SET);
	}

	printf("Reading from disk sectors %lu-%lu (%lu sectors)\n",
		(unsigned long)first_lba,
		(unsigned long)first_lba+(unsigned long)size_lba-(unsigned long)1UL,
		(unsigned long)size_lba);

	{
		lseek_off_t x = (lseek_off_t)first_lba * (lseek_off_t)512UL;

		if (_polyfill_lseek(fd,x,SEEK_SET) != x || read(fd,sectorbuf,512) != 512) {
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

	/* do it */
	if (libmsfat_file_io_ctx_path_lookup(fioctx,fioctx_parent,msfatctx,&dirent,&lfn_name,s_path,
		libmsfat_path_lookup_CREATE | (do_mkdir?libmsfat_path_lookup_DIRECTORY:0))) {
		fprintf(stderr,"Path lookup failed\n");
		return 1;
	}

	if (do_mkdir) {
		if (!fioctx->is_directory || fioctx->is_root_dir || fioctx->is_root_parent) {
			fprintf(stderr,"Not a directory\n");
			return 1;
		}
	}
	else {
		/* no directories! */
		if (fioctx->is_directory || fioctx->is_root_parent) {
			fprintf(stderr,"Cannot append to directories\n");
			return 1;
		}

		/* please allow extending the file (TODO: there should be library functions to enable this!) */
		if (libmsfat_file_io_ctx_enable_write_extend(fioctx,fioctx_parent,msfatctx)) {
			fprintf(stderr,"Cannot make file write-extendable\n");
			return 1;
		}

		/* seek to the end */
		if (libmsfat_file_io_ctx_lseek(fioctx,msfatctx,fioctx->file_size,/*flags*/0)) {
			fprintf(stderr,"Cannot lseek to end\n");
			return 1;
		}
		if (libmsfat_file_io_ctx_tell(fioctx,msfatctx) != fioctx->file_size) {
			fprintf(stderr,"lseek didn't reach end\n");
			return 1;
		}

		/* SANITY CHECK: the libmsfat library currently clears the cluster position to zero when hitting the end of the chain */
		if (fioctx->position != (uint32_t)0UL && fioctx->cluster_position == (uint32_t)0UL) {
			fprintf(stderr,"lseek hit the end of the cluster, position lost (BUG)\n");
			return 1;
		}

		/* write it */
		{
			size_t len = strlen(msg_add);
			int wr;

			while (msg_rep > 0) {
				wr = libmsfat_file_io_ctx_write(fioctx,msfatctx,msg_add,len);
				if (wr < 0) {
					fprintf(stderr,"Write failed\n");
					break;
				}
				else if (wr == 0) {
					fprintf(stderr,"Write hit EOF\n");
					break;
				}
				else if (wr < (int)len) {
					fprintf(stderr,"Write was incomplete (%d < %d)\n",
							wr,(int)len);
					break;
				}

				msg_rep--;
			}
		}

		/* if we need to update the dirent, then do so */
		if (fioctx->should_update_dirent) {
			/* write it back to disk */
			libmsfat_file_io_ctx_update_dirent_from_context(&dirent,fioctx,fioctx_parent,msfatctx);
			if (libmsfat_file_io_ctx_write_dirent(fioctx,fioctx_parent,msfatctx,&dirent,&lfn_name))
				fprintf(stderr,"Failed to update dirent\n");
		}
	}

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

