#if defined(_MSC_VER)
# include <datamtoolbox-v2/polyfill/ms_posix_stfu.h>
#endif
#if !defined(_MSC_VER)
# include <unistd.h>
# include <endian.h>
#endif
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#if defined(_MSC_VER)
# include <datamtoolbox-v2/polyfill/ms_cpp.h>
#endif
#include <datamtoolbox-v2/polyfill/lseek.h>
#include <datamtoolbox-v2/polyfill/stat.h>
#include <datamtoolbox-v2/polyfill/unix.h>

#include <datamtoolbox-v2/libint13chs/int13chs.h>
#include <datamtoolbox-v2/libpartmbr/partmbr.h>
#include <datamtoolbox-v2/libpartmbr/mbrctx.h>

static struct libpartmbr_context_t*		diskimage_ctx = NULL;

static int do_list() {
	struct chs_geometry_t chs;
	unsigned int i;

	assert(diskimage_ctx != NULL);
	assert(diskimage_ctx->list != NULL);

	printf("MBR list (%u entries):\n",(unsigned int)diskimage_ctx->list_count);
	for (i=0;i < diskimage_ctx->list_count;i++) {
		struct libpartmbr_context_entry_t *ent = &diskimage_ctx->list[i];

		printf("    ");
		// if multiple bits set that should NOT be set, deliberate grammer errors will happen, by design
		if (ent->is_primary) printf("Primary ");
		if (ent->is_extended) printf("Extended ");
		if (ent->is_empty) printf("empty ");
		if (ent->parent_entry >= 0) printf("[child of entry #%d] ",ent->parent_entry);
		if (ent->start_lba_overflow) printf("[START SECTOR OVERFLOW!] ");
		printf("entry #%u: (index=%u)\n",(unsigned int)i,(unsigned int)ent->index);

		if (ent->is_extended)
			printf("        Extended MBR at sector (LBA): %lu\n",(unsigned long)ent->extended_mbr_sector);

		if (!ent->is_empty) {
			printf("        Type: 0x%02x %s\n",(unsigned int)ent->entry.partition_type,libpartmbr_partition_type_to_str(ent->entry.partition_type));
			printf("        Bootable: 0x%02x\n",(unsigned int)ent->entry.bootable_flag);
			printf("        Start (LBA): %lu\n",(unsigned long)ent->entry.first_lba_sector);
			printf("        Size (LBA): %lu\n",(unsigned long)ent->entry.number_lba_sectors);

			if (int13cnv_int13_to_chs(&chs,&ent->entry.first_chs_sector) == 0)
				printf("        First sector (CHS): %u/%u/%u\n",(unsigned int)chs.cylinders,(unsigned int)chs.heads,(unsigned int)chs.sectors);

			if (int13cnv_int13_to_chs(&chs,&ent->entry.last_chs_sector) == 0)
				printf("        Last sector (CHS): %u/%u/%u\n",(unsigned int)chs.cylinders,(unsigned int)chs.heads,(unsigned int)chs.sectors);
		}
	}

	return 0;
}

static int do_rewrite() {
	assert(diskimage_ctx != NULL);
	assert(diskimage_ctx->list != NULL);

	if (libpartmbr_context_write_partition_table(diskimage_ctx)) {
		fprintf(stderr,"Failed to write partition table. err=%s\n",diskimage_ctx->err_str);
		return 1;
	}

	return 0;
}

int main(int argc,char **argv) {
	const char *s_geometry = NULL;
	const char *s_command = NULL;
	const char *s_image = NULL;
	int i,ret=1,fd=-1;

	for (i=1;i < argc;) {
		const char *a = argv[i++];

		if (*a == '-') {
			do { a++; } while (*a == '-');

			if (!strcmp(a,"geometry")) {
				s_geometry = argv[i++];
			}
			else if (!strcmp(a,"image")) {
				s_image = argv[i++];
			}
			else if (!strcmp(a,"c")) {
				s_command = argv[i++];
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

	if (s_command == NULL) {
		fprintf(stderr,"mbredit -c <command> [options] ...\n");
		fprintf(stderr,"\n");

		fprintf(stderr,"--geometry <C/H/S>          Specify geometry of the disk image\n");
		fprintf(stderr,"--image <path>              Disk image path\n");
		fprintf(stderr,"--entry <x>                 Partition entry\n");
		fprintf(stderr,"--start <x>                 Starting (LBA) sector\n");
		fprintf(stderr,"--num <x>                   Number of (LBA) sectors\n");
		fprintf(stderr,"--type <x>                  Partition type code\n");
		fprintf(stderr,"\n");

		fprintf(stderr,"-c list                     Print the contents of the partition table\n");
		fprintf(stderr,"\n");

		fprintf(stderr,"-c rewrite                  Load and rewrite partition table\n");
		fprintf(stderr,"\n");
		return 1;
	}

	if (s_image == NULL) {
		fprintf(stderr,"No image provided\n");
		return 1;
	}

	assert(libpartmbr_sanity_check());

	diskimage_ctx = libpartmbr_context_create();
	if (diskimage_ctx == NULL) {
		fprintf(stderr,"Cannot allocate libpartmbr context\n");
		return 1;
	}

	if (!strcmp(s_command,"rewrite"))
		fd = open(s_image,O_RDWR|O_BINARY|O_CREAT,0644);
	else
		fd = open(s_image,O_RDONLY|O_BINARY);

	if (fd < 0) {
		fprintf(stderr,"Failed to open disk image, error=%s\n",strerror(errno));
		return 1;
	}
	{
		/* make sure it's a file */
		struct stat st;
		if (_polyfill_fstat(fd,&st) || !S_ISREG(st.st_mode)) {
			fprintf(stderr,"Image is not a file\n");
			return 1;
		}
	}

	/* good! hand it off to the context. it takes ownership, so we forget our copy. */
	if (libpartmbr_context_assign_fd(diskimage_ctx,fd)) {
		fprintf(stderr,"libpartmbr did not accept file descriptor %d\n",fd);
		return 1;
	}
	fd = -1;

	if (s_geometry != NULL) {
		if (int13cnv_parse_chs_geometry(&diskimage_ctx->geometry,s_geometry)) {
			fprintf(stderr,"Failed to parse C/H/S geometry\n");
			return 1;
		}
	}
	else {
		fprintf(stderr,"WARNING: Geometry not specified. Assuming CHS 16384/255/63\n");
		diskimage_ctx->geometry.cylinders = 16384;
		diskimage_ctx->geometry.sectors = 63;
		diskimage_ctx->geometry.heads = 255;
	}

	/* load the partition table! */
	if (libpartmbr_context_read_partition_table(diskimage_ctx)) {
		fprintf(stderr,"Failed to read partition table\n");
		return 1;
	}

	if (!strcmp(s_command,"list"))
		ret = do_list();
	else if (!strcmp(s_command,"rewrite"))
		ret = do_rewrite();
	else
		fprintf(stderr,"Unknown command '%s'\n",s_command);

	diskimage_ctx = libpartmbr_context_destroy(diskimage_ctx);
	return ret;
}

