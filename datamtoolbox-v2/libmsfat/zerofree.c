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

static unsigned char			zero_by_holepunch = 0;

static unsigned char			sectorbuf[512];
static unsigned char			buffer[4096];

static int holepunch(struct libmsfat_context_t *msfatctx,uint64_t offset,uint64_t sz) {
#if defined(_LINUX) && defined(FALLOC_FL_PUNCH_HOLE)
	if (msfatctx->user_fd < 0) return -1;
	if (fallocate(msfatctx->user_fd,FALLOC_FL_KEEP_SIZE | FALLOC_FL_PUNCH_HOLE,(off_t)offset,(off_t)sz)) {
		if (errno == ENOSYS)
			fprintf(stderr,"Filesystem does not support punching holes\n");
		else
			fprintf(stderr,"holepunch failed, offset=%llu sz=%llu %s\n",(unsigned long long)offset,(unsigned long long)sz,strerror(errno));

		zero_by_holepunch = 0;
		return -1;
	}

	return 0;
#elif defined(_WIN32)
	/* Windows NT sparse file support */
	FILE_ZERO_DATA_INFORMATION fzdi;
	DWORD dwTemp,err;
	HANDLE h;

	if (msfatctx->user_win32_handle != INVALID_HANDLE_VALUE)
		h = msfatctx->user_win32_handle;
	else if (msfatctx->user_fd >= 0)
		h = (HANDLE)_get_osfhandle(msfatctx->user_fd);
	else
		return -1;

	if (h == INVALID_HANDLE_VALUE)
		return -1;

	if (DeviceIoControl(h,FSCTL_SET_SPARSE,NULL,0,NULL,0,&dwTemp,NULL)) {
		err = GetLastError();

		fprintf(stderr,"FSCTL_SET_SPARSE error code 0x%08lx\n",(unsigned long)err);
		zero_by_holepunch = 0;
		return -1;
	}

	memset(&fzdi,0,sizeof(fzdi));
	fzdi.FileOffset.QuadPart = offset;
	fzdi.BeyondFinalZero.QuadPart = offset + sz;
	if (DeviceIoControl(h,FSCTL_SET_ZERO_DATA,&fzdi,sizeof(fzdi),NULL,0,&dwTemp,NULL)) {
		err = GetLastError();

		fprintf(stderr,"FSCTL_SET_ZERO_DATA error code 0x%08lx\n",(unsigned long)err);
		zero_by_holepunch = 0;
		return -1;
	}

	return 0;
#else
	fprintf(stderr, "Sorry, there's no support on your platform for sparse files\n");
	zero_by_holepunch = 0;
	return -1;
#endif
}

static void zero_write(struct libmsfat_context_t *msfatctx,uint64_t offset,uint64_t sz) {
	size_t wr;

	memset(buffer,0,sizeof(buffer));
	while (sz != (uint64_t)0UL) {
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

static void do_zero(struct libmsfat_context_t *msfatctx,uint64_t offset,uint64_t sz) {
	if (zero_by_holepunch && holepunch(msfatctx,offset,sz) == 0) return; /* if hole punching worked.. */
	zero_write(msfatctx,offset,sz);
}

static void clean_file_cluster_tip(struct libmsfat_file_io_ctx_t *fioctx,struct libmsfat_file_io_ctx_t *fioctx_parent,struct libmsfat_context_t *msfatctx,struct libmsfat_dirent_t *dir_dirent) {
	uint32_t bytes_per_cluster;

	/* files by definition are a cluster chain */
	if (!fioctx->is_cluster_chain) return;
	if (fioctx->is_directory) return;

	/* we need the cluster size for this */
	bytes_per_cluster = libmsfat_context_get_cluster_size(msfatctx);
	if (bytes_per_cluster == (uint32_t)0) return;

	/* first, make sure the file is truncated to exactly what it's size dictates */
	if (libmsfat_file_io_ctx_truncate_file(fioctx,fioctx_parent,msfatctx,dir_dirent,NULL,fioctx->file_size)) {
		fprintf(stderr,"ERROR: failed to truncate file\n");
		return;
	}

	/* if the file size is not a multiple of the cluster size, then we need to zero the last cluster beyond
	 * the end of the file. */
	if (fioctx->file_size % bytes_per_cluster) {
		uint32_t partial = fioctx->file_size % bytes_per_cluster;
		uint32_t sz = bytes_per_cluster - partial;
		uint64_t offset;

		/* locate the cluster */
		if (libmsfat_file_io_ctx_lseek(fioctx,msfatctx,fioctx->file_size,/*flags*/0))
			return;
		if (libmsfat_file_io_ctx_tell(fioctx,msfatctx) != fioctx->file_size)
			return;
		if (libmsfat_context_get_cluster_offset(msfatctx,&offset,fioctx->cluster_position))
			return;

		printf("....clearing %lu bytes in the cluster tip\n",(unsigned long)sz);
		do_zero(msfatctx,offset+partial,sz);
	}
}

static void clean_directory(struct libmsfat_file_io_ctx_t *fioctx,struct libmsfat_file_io_ctx_t *fioctx_parent,struct libmsfat_context_t *msfatctx,struct libmsfat_dirent_t *dir_dirent) {
	struct libmsfat_dirent_t dirent;
	uint32_t last_ro_end=0;
	uint32_t truncate=0;
	uint32_t ro=0,wo=0;

	/* scan the directory. we need to recurse into subdirectories. */
	if (libmsfat_file_io_ctx_rewinddir(fioctx,msfatctx,NULL) == 0) {
		while (libmsfat_file_io_ctx_readdir(fioctx,msfatctx,NULL,&dirent) == 0) {
			struct libmsfat_file_io_ctx_t *subfioctx;

			if (dirent.a.n.DIR_Attr & libmsfat_DIR_ATTR_VOLUME_ID)
				continue;
			if ((dirent.a.n.DIR_Attr & libmsfat_DIR_ATTR_DIRECTORY) && libmsfat_dirent_is_dot_dir(&dirent))
				continue;

			subfioctx = libmsfat_file_io_ctx_create();
			if (subfioctx != NULL && libmsfat_file_io_ctx_assign_from_dirent(subfioctx,msfatctx,&dirent) == 0) {
				{
					char tmp[64];

					tmp[0] = 0; libmsfat_dirent_filename_to_str(tmp,sizeof(tmp),&dirent);
					printf("...cleaning ");
					if (dirent.a.n.DIR_Attr & libmsfat_DIR_ATTR_DIRECTORY)
						printf("subdirectory ");
					else
						printf("file cluster tip in ");

					printf("'%s'\n",tmp);
				}

				if (dirent.a.n.DIR_Attr & libmsfat_DIR_ATTR_DIRECTORY)
					clean_directory(subfioctx,fioctx,msfatctx,&dirent);
				else
					clean_file_cluster_tip(subfioctx,fioctx,msfatctx,&dirent);

				subfioctx = libmsfat_file_io_ctx_destroy(subfioctx);
			}
		}
	}

	/* scan the directory again, copying items back to fill in deleted and empty entries */
	while (1) {
		if (libmsfat_file_io_ctx_lseek(fioctx,msfatctx,ro,/*flags*/0))
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
			if (libmsfat_file_io_ctx_lseek(fioctx,msfatctx,wo,/*flags*/0))
				break;
			if (libmsfat_file_io_ctx_tell(fioctx,msfatctx) != wo)
				break;
			if (libmsfat_file_io_ctx_write(fioctx,msfatctx,&dirent,sizeof(dirent)) != sizeof(dirent))
				break;
		}

		ro += (uint32_t)sizeof(dirent);
		wo += (uint32_t)sizeof(dirent);
		last_ro_end = ro;
	}

	/* if we removed anything, say so (NTS: the above code should always cause wo < ro) */
	if (last_ro_end != wo)
		printf("Removed %lu empty/deleted directory entries\n",
			((unsigned long)(last_ro_end-wo)) / (unsigned long)sizeof(dirent));

	/* take note where the write pointer is. we will truncate there after zeroing the directory.
	 * but, do not truncate the directory to zero! it is not specified in Microsoft's standards
	 * document whether an empty directory is given at minimum one cluster, or whether it is not
	 * given any clusters and the starting cluster is zero (like a file). to be safe, we do not
	 * allow truncating to zero. */
	truncate = wo;
	if (truncate == (uint32_t)0) truncate = (uint32_t)sizeof(dirent);

	/* fill out the rest of the directory with zeros */
	memset(&dirent,0,sizeof(dirent));
	if (libmsfat_file_io_ctx_lseek(fioctx,msfatctx,wo,/*flags*/0))
		return;
	if (libmsfat_file_io_ctx_tell(fioctx,msfatctx) != wo)
		return;
	while (libmsfat_file_io_ctx_write(fioctx,msfatctx,&dirent,sizeof(dirent)) == sizeof(dirent))
		{ };

	/* truncate, if possible (if the directory is based on a cluster chain) */
	if (fioctx->is_cluster_chain) {
		if (libmsfat_file_io_ctx_truncate_file(fioctx,fioctx_parent,msfatctx,dir_dirent,NULL,truncate))
			fprintf(stderr,"ERROR: failed to truncate directory (truncate point=%lu first_cluster=%lu)\n",
				(unsigned long)truncate,
				(unsigned long)fioctx->first_cluster);
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
	uint32_t zeroed_clusters=0;
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
			else if (!strcmp(a,"hole-punch")) {
				zero_by_holepunch = 1;
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
		fprintf(stderr,"--hole-punch             Zero unused spaces by hole punching (making the file sparse).\n");
		fprintf(stderr,"                         This can allow zeroing the empty spaces a LOT faster, only if\n");
		fprintf(stderr,"                         supported by the filesystem.\n");
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

	printf("Scanning directory structure, to clean and repack diretories and zero file cluster tips\n");
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

		if (msfatctx->fatinfo.FAT_size == 32)
			fatent &= libmsfat_FAT32_CLUSTER_MASK;

		percent = (int)(((uint64_t)cluster * (uint64_t)100UL) / msfatctx->fatinfo.Total_clusters);

		if (((uint32_t)cluster & 0xFF) == (uint32_t)0) {
			printf("\x0D" "Reading cluster %lu / %lu (%%%u) (%lu cleared) ",
				(unsigned long)cluster,
				(unsigned long)msfatctx->fatinfo.Total_clusters,
				percent,
				(unsigned long)zeroed_clusters);
			fflush(stdout);
		}

		if (fatent == (libmsfat_FAT_entry_t)0UL) {
			uint64_t offset;

			/* cluster is free! zero it! */
			if (libmsfat_context_get_cluster_offset(msfatctx,&offset,cluster)) {
				fprintf(stderr,"\nERROR: unable to locate cluster #%lu\n",
					(unsigned long)cluster);
				continue;
			}

			do_zero(msfatctx,offset,bytes_per_cluster);
			zeroed_clusters++;
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

