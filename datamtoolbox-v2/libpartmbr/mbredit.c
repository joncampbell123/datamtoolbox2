#if defined(_MSC_VER)
/* shut up Microsoft. how the fuck is strerror() unsafe? */
# define _CRT_SECURE_NO_WARNINGS
# include <io.h>
/* shut up Microsoft. what the hell is your problem with POSIX functions? */
# define open _open
# define read _read
# define close _close
# define lseek _lseeki64
#endif
#if !defined(_MSC_VER)
# include <unistd.h>
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

/* oh come on Microsoft ... */
#if defined(_MSC_VER) && !defined(S_ISREG)
# define S_ISREG(x) (x & _S_IFREG)
#endif

#include <datamtoolbox-v2/libint13chs/int13chs.h>
#include <datamtoolbox-v2/libpartmbr/partmbr.h>

#ifndef O_BINARY
# define O_BINARY 0
#endif

static struct libpartmbr_state_t	diskimage_state;
static libpartmbr_sector_t		diskimage_sector;
static struct chs_geometry_t		diskimage_chs;
static int				diskimage_fd = -1;
static unsigned char			diskimage_use_chs = 0;

static int do_dump() {
	unsigned int i;

	printf("MBR sector contents:\n");
	for (i=0;i < LIBPARTMBR_SECTOR_SIZE;i++) {
		if ((i&15) == 0) printf("   ");
		printf("%02x ",diskimage_sector[i]);
		if ((i&15) == 15) printf("\n");
	}

	return 0;
}

static int do_list() {
	struct libpartmbr_entry_t ent;
	struct chs_geometry_t chs;
	unsigned int entry;

	printf("MBR partition list:\n");
	for (entry=0;entry < diskimage_state.entries;entry++) {
		if (libpartmbr_read_entry(&ent,&diskimage_state,diskimage_sector,entry))
			continue;

		printf("#%d: ",entry+1);
		if (ent.partition_type != LIBPARTMBR_TYPE_EMPTY) {
			printf("type=%s(0x%02x) ",
				libpartmbr_partition_type_to_str(ent.partition_type),
				ent.partition_type);

			printf("bootable=0x%02x ",
				ent.bootable_flag);

			printf("first_lba=%lu number_lba=%lu ",
				(unsigned long)ent.first_lba_sector,
				(unsigned long)ent.number_lba_sectors);

			printf("first_chs=");
			if (int13cnv_int13_to_chs(&chs,&ent.first_chs_sector) == 0)
				printf("%u/%u/%u",
					(unsigned int)chs.cylinders,
					(unsigned int)chs.heads,
					(unsigned int)chs.sectors);
			else
				printf("N/A");
			printf(" ");

			printf("last_chs=");
			if (int13cnv_int13_to_chs(&chs,&ent.last_chs_sector) == 0)
				printf("%u/%u/%u",
					(unsigned int)chs.cylinders,
					(unsigned int)chs.heads,
					(unsigned int)chs.sectors);
			else
				printf("N/A");
		}
		else {
			printf("(empty)");
		}
		printf("\n");
	}

	return 0;
}

int main(int argc,char **argv) {
	const char *s_geometry = NULL;
	const char *s_command = NULL;
	const char *s_image = NULL;
	int i,ret=1;

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
		fprintf(stderr,"\n");

		fprintf(stderr,"-c dump                     Dump the sector containing the MBR\n");
		fprintf(stderr,"\n");

		fprintf(stderr,"-c list                     Print the contents of the partition table\n");
		fprintf(stderr,"\n");
		return 1;
	}

	if (s_image == NULL) {
		fprintf(stderr,"No image provided\n");
		return 1;
	}

	if (s_geometry != NULL) {
		diskimage_use_chs = 1;
		if (int13cnv_parse_chs_geometry(&diskimage_chs,s_geometry)) {
			fprintf(stderr,"Failed to parse C/H/S geometry\n");
			return 1;
		}
	}
	else {
		diskimage_chs.cylinders = 16384;
		diskimage_chs.sectors = 63;
		diskimage_chs.heads = 255;
		diskimage_use_chs = 0;
	}

	assert(sizeof(diskimage_sector) >= LIBPARTMBR_SECTOR_SIZE);
	assert(libpartmbr_sanity_check());

	diskimage_fd = open(s_image,O_RDONLY|O_BINARY);
	if (diskimage_fd < 0) {
		fprintf(stderr,"Failed to open disk image, error=%s\n",strerror(errno));
		return 1;
	}
	{
		/* make sure it's a file */
		struct stat st;
		if (fstat(diskimage_fd,&st) || !S_ISREG(st.st_mode)) {
			fprintf(stderr,"Image is not a file\n");
			return 1;
		}
	}
	if (lseek(diskimage_fd,0,SEEK_SET) != 0 || read(diskimage_fd,diskimage_sector,LIBPARTMBR_SECTOR_SIZE) != LIBPARTMBR_SECTOR_SIZE) {
		fprintf(stderr,"Failed to read MBR\n");
		return 1;
	}
	close(diskimage_fd);
	diskimage_fd = -1;

	if (!strcmp(s_command,"dump"))
		return do_dump();

	if (libpartmbr_state_probe(&diskimage_state,diskimage_sector)) {
		fprintf(stderr,"Doesn't look like MBR\n");
		return 1;
	}
	printf("MBR partition type: %s\n",libpartmbr_type_to_string(diskimage_state.type));

	if (!strcmp(s_command,"list"))
		ret = do_list();
	else
		fprintf(stderr,"Unknown command '%s'\n",s_command);

	return ret;
}

