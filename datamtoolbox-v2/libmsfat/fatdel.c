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
static unsigned char			buffer[800];

int main(int argc,char **argv) {
	struct libmsfat_disk_locations_and_info locinfo;
	struct libmsfat_file_io_ctx_t *fioctx_parent = NULL;
	struct libmsfat_file_io_ctx_t *fioctx = NULL;
	struct libmsfat_context_t *msfatctx = NULL;
	struct libmsfat_lfn_assembly_t lfn_name;
	struct libmsfat_dirent_t dirent;
	const char *s_partition = NULL;
	const char *s_trunc = NULL;
	const char *s_image = NULL;
	const char *s_path = NULL;
	const char *s_out = NULL;
	uint32_t first_lba=0;
	int i,fd,out_fd=-1;
	uint32_t trunc=0;

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
			else if (!strcmp(a,"o") || !strcmp(a,"out")) {
				s_out = argv[i++];
			}
			else if (!strcmp(a,"t")) {
				s_trunc = argv[i++];
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

	if (s_image == NULL || s_path == NULL) {
		fprintf(stderr,"fatinfo --image <image> ...\n");
		fprintf(stderr,"\n");
		fprintf(stderr,"--partition <n>          Hard disk image, use partition N from the MBR\n");
		fprintf(stderr,"--cluster <n>            Which cluster to start from (if not root dir)\n");
		fprintf(stderr,"--path <x>               File/dir to look up\n");
		fprintf(stderr,"-t <n>                   Truncate file to N bytes, don't delete\n");
		fprintf(stderr,"-o <file>                Dump directory to file\n");
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

	if (s_trunc != NULL)
		trunc = (uint32_t)strtoul(s_trunc,NULL,0);

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

#if defined(_MSC_VER)
		// Microsoft Visual Studio 2015: Strange pointer corruption here in x64 builds?!?!?!?!
		// ctx = (valid pointer)
		// ctx->list = (valid pointer)
		// ent = &ctx->list[...] = 0x0000000000000008   (WTF??)
		assert((uintptr_t)ent >= (uintptr_t)0x1000UL);
#endif

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
		lseek(fd,0,SEEK_SET);
	}

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

	/* look up the path. this should give us two fioctx's, one for the parent dir, and one for the file itself */
	if (libmsfat_file_io_ctx_path_lookup(fioctx,fioctx_parent,msfatctx,&dirent,&lfn_name,s_path,0/*default*/)) {
		fprintf(stderr,"Path lookup failed\n");
		return 1;
	}

	/* dump file contents before deleting, if instructed */
	if (s_out != NULL) {
		int rd;

		fprintf(stderr,"Prior to deleting file, dumping to %s\n",s_out);

		out_fd = open(s_out,O_CREAT|O_TRUNC|O_BINARY|O_WRONLY,0644);
		if (out_fd < 0) {
			fprintf(stderr,"Failed to create dump file, %s\n",strerror(errno));
			return 1;
		}

		while ((rd=libmsfat_file_io_ctx_read(fioctx,msfatctx,buffer,sizeof(buffer))) > 0)
			write(out_fd,buffer,rd);

		close(out_fd);
		out_fd = -1;
	}

	if (fioctx->is_directory) {
		int files = 0;

		if (libmsfat_dirent_is_dot_dir(&dirent)) {
			printf("I refuse to modify . and .. directory entries\n");
			return 1;
		}

		printf("Path refers to a directory. Scanning directory to make sure no files remain.\n");

		while (libmsfat_file_io_ctx_readdir(fioctx,msfatctx,&lfn_name,&dirent) == 0) {
			if (!libmsfat_dirent_is_dot_dir(&dirent)) {
				if (!(dirent.a.n.DIR_Attr & libmsfat_DIR_ATTR_VOLUME_ID))
					files++;
			}
		}

		if (files != 0) {
			printf("%u files remain. Will not delete or truncate directory\n",files);
			return 1;
		}

		printf("Directory is empty. Proceeding to delete.\n");
	}

	if (s_trunc != NULL) {
		if (libmsfat_file_io_ctx_truncate_file(fioctx,fioctx_parent,msfatctx,&dirent,&lfn_name,trunc)) {
			fprintf(stderr,"Failed to truncate file\n");
			return 1;
		}

		printf("File has been truncated to %lu bytes (asked for %lu)\n",
			(unsigned long)fioctx->file_size,
			(unsigned long)trunc);
	}
	else {
		/* now delete it.
		 * at this level we first call to truncate the file to zero, then delete the dirent */
		if (libmsfat_file_io_ctx_truncate_file(fioctx,fioctx_parent,msfatctx,&dirent,&lfn_name,(uint32_t)0) == 0) {
			if (libmsfat_file_io_ctx_delete_dirent(fioctx,fioctx_parent,msfatctx,&dirent,&lfn_name) == 0) {
				/* OK */
			}
			else {
				fprintf(stderr,"Failed to delete file\n");
			}
		}
		else {
			fprintf(stderr,"Failed to truncate file\n");
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

