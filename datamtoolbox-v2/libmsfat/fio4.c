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
	struct libmsfat_file_io_ctx_t *fioctx = NULL;
	struct libmsfat_context_t *msfatctx = NULL;
	struct libmsfat_lfn_assembly_t lfn_name;
	struct libmsfat_dirent_t dirent;
	const char *s_partition = NULL;
	const char *s_image = NULL;
	const char *s_path = NULL;
	const char *s_out = NULL;
	uint32_t first_lba=0;
	int i,fd,out_fd=-1;
	char tmp[512];

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
		fprintf(stderr,"-o <file>                Dump directory to file\n");
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

	fioctx = libmsfat_file_io_ctx_create();
	if (fioctx == NULL) {
		fprintf(stderr,"Cannot alloc FIO ctx\n");
		return 1;
	}

	/* start at the root dir */
	if (libmsfat_file_io_ctx_assign_root_directory(fioctx,msfatctx) || libmsfat_file_io_ctx_rewinddir(fioctx,msfatctx,&lfn_name)) {
		fprintf(stderr,"Cannot assign root dir\n");
		return 1;
	}

	/* we allow --path '' to refer to the root directory */
	while (*s_path == '\\' || *s_path == '/') s_path++; /* MS-DOS \ path separator or Linux / path separator */
	{
		const char *s = s_path;
		char tmp[320],*d,*df;

		while (*s != 0) {
			d = tmp;
			df = tmp+sizeof(tmp)-1;

			while (*s != 0 && d < df) {
				if (*s == '/' || *s == '\\') {
					while (*s == '\\' || *s == '/') s++; /* MS-DOS \ path separator or Linux / path separator */
					break;
				}

				*d++ = *s++;
			}
			*d = 0;
			if (d >= df) {
				fprintf(stderr,"Path elem too long\n");
				return 1;
			}

			if (libmsfat_file_io_ctx_find_in_dir(fioctx,msfatctx,&dirent,&lfn_name,tmp,0/*default*/)) {
				fprintf(stderr,"%s not found\n",tmp);
				return 1;
			}

			printf("%s: found ",tmp);
			printf("dirent=%lu lfn=%lu\n",
				(unsigned long)fioctx->dirent_start,
				(unsigned long)fioctx->dirent_lfn_start);
			libmsfat_dirent_filename_to_str(tmp,sizeof(tmp),&dirent);
			if (dirent.a.n.DIR_Attr & libmsfat_DIR_ATTR_DIRECTORY)
				printf("directory '%s'",tmp);
			else
				printf("file '%s'",tmp);
			if (lfn_name.name_avail) {
				libmsfat_dirent_lfn_to_str_utf8(tmp,sizeof(tmp),&lfn_name);
				printf(" lfn '%s'",tmp);
			}
			printf("\n");

			if (libmsfat_file_io_ctx_assign_from_dirent(fioctx,msfatctx,&dirent) ||
				libmsfat_file_io_ctx_rewinddir(fioctx,msfatctx,&lfn_name)) {
				fprintf(stderr,"%s: Cannot assign cluster\n",tmp);
				return 1;
			}
			if (dirent.a.n.DIR_Attr & libmsfat_DIR_ATTR_DIRECTORY)
				fioctx->is_directory = 1;
			else
				fioctx->file_size = dirent.a.n.DIR_FileSize;
		}
	}

	if (fioctx->is_directory) {
		printf("Directory contents:\n");

		while (libmsfat_file_io_ctx_readdir(fioctx,msfatctx,&lfn_name,&dirent) == 0) {
			if (dirent.a.n.DIR_Attr & libmsfat_DIR_ATTR_VOLUME_ID) {
				libmsfat_dirent_volume_label_to_str(tmp,sizeof(tmp),&dirent);
				printf("    <VOL>  '%s'",tmp);
			}
			else {
				libmsfat_dirent_filename_to_str(tmp,sizeof(tmp),&dirent);
				if (dirent.a.n.DIR_Attr & libmsfat_DIR_ATTR_DIRECTORY)
					printf("    <DIR>  '%s'",tmp);
				else
					printf("    <FIL>  '%s'",tmp);
			}
			printf("\n");

			if (lfn_name.name_avail) {
#if defined(_WIN32) && defined(_MSC_VER) /* Windows + Microsoft C++ */
				// use widechar printf in Windows to show the name properly
				if (isatty(1/*STDOUT*/)) {
					if (sizeof(wchar_t) == 2) { /* Microsoft C runtime sets wchar_t == uint16_t aka WORD */
						size_t wl;

						libmsfat_dirent_lfn_to_str_utf16le(tmp,sizeof(tmp),&lfn_name);
						wl = wcslen((const wchar_t*)tmp);

						printf("        Long name:          '");
						fflush(stdout);
						{
							DWORD written = 0;
							WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), (const void*)tmp, (DWORD)wl, &written, NULL);
						}
						printf("'\n");
					}
				}
				else {
					libmsfat_dirent_lfn_to_str_utf8(tmp,sizeof(tmp),&lfn_name);
					printf("        Long name:          '%s'\n",tmp);
				}
#else
				// Linux terminals nowaways use UTF-8 encoding
				libmsfat_dirent_lfn_to_str_utf8(tmp,sizeof(tmp),&lfn_name);
				printf("        Long name:          '%s'\n",tmp);
#endif
			}

			printf("        File size:          %lu bytes\n",
				(unsigned long)le32toh(dirent.a.n.DIR_FileSize));
			printf("        Starting cluster:   %lu\n",
				(unsigned long)libmsfat_dirent_get_starting_cluster(msfatctx,&dirent));
		}
	}
	else {
		uint64_t count=0;
		int rd;

		if (s_out != NULL) {
			out_fd = open(s_out,O_CREAT|O_TRUNC|O_BINARY|O_WRONLY,0644);
			if (out_fd < 0) {
				fprintf(stderr,"Failed to create dump file, %s\n",strerror(errno));
				return 1;
			}
		}

		printf("File found (%lu bytes)\n",
			(unsigned long)fioctx->file_size);
		if (out_fd < 0)
			printf("-------------------------------[ contents follow ]-------------------------\n"); fflush(stdout);

		while ((rd=libmsfat_file_io_ctx_read(fioctx,msfatctx,buffer,sizeof(buffer))) > 0) {
			if (out_fd >= 0) write(out_fd,buffer,rd);
			else write(1/*STDOUT*/,buffer,rd);
			count += (uint64_t)rd;
		}
		if (rd < 0) fprintf(stderr,"Read error\n");
		if (count != (uint64_t)fioctx->file_size)
			fprintf(stderr,"File size mismatch. Read %llu, actual size %lu\n",
				(unsigned long long)count,(unsigned long)fioctx->file_size);

		if (out_fd >= 0 && rd > 0)
			write(out_fd,buffer,rd);
	}

	fioctx = libmsfat_file_io_ctx_destroy(fioctx);
	msfatctx = libmsfat_context_destroy(msfatctx);
	close(fd);
	fd = -1;
	return 0;
}

