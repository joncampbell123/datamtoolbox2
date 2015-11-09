#include <stdlib.h>
#if !defined(_MSC_VER)
# include <unistd.h>
#endif
#include <sys/stat.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include <datamtoolbox-v2/libint13chs/int13chs.h>
#include <datamtoolbox-v2/libpartmbr/partmbr.h>

#ifndef O_BINARY
# define O_BINARY 0
#endif

static uint8_t			diskimage_sector[LIBPARTMBR_SECTOR_SIZE];

static struct chs_geometry_t	diskimage_chs;
static int			diskimage_fd = -1;
static unsigned char		diskimage_use_chs = 0;

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
		ret = do_dump();
	else if (!strcmp(s_command,"list"))
		ret = do_list();
	else
		fprintf(stderr,"Unknown command '%s'\n",s_command);

	return ret;
}

