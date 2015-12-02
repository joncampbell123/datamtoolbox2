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

#include <datamtoolbox-v2/libint13chs/int13chs.h>
#include <datamtoolbox-v2/libpartmbr/mbrctx.h>
#include <datamtoolbox-v2/libmsfat/libmsfat.h>
#include <datamtoolbox-v2/libmsfat/libmsfat_unicode.h>

static struct chs_geometry_t			disk_geo;
static uint64_t					disk_sectors = 0;
static unsigned int				disk_bytes_per_sector = 0;
static uint64_t					disk_size_bytes = 0;
static uint8_t					disk_media_type_byte = 0;

uint8_t guess_from_geometry(struct chs_geometry_t *g) {
	if (g->cylinders == 40) {
		if (g->sectors == 8) {
			if (g->heads == 2)
				return 0xFF;	// 5.25" 320KB MFM    C/H/S 40/2/8
			else if (g->heads == 1)
				return 0xFE;	// 5.25" 160KB MFM    C/H/S 40/1/8
		}
		else if (g->sectors == 9) {
			if (g->heads == 2)
				return 0xFD;	// 5.25" 360KB MFM    C/H/S 40/2/9
			else if (g->heads == 1)
				return 0xFC;	// 5.25" 180KB MFM    C/H/S 40/1/9
		}
	}
	else if (g->cylinders == 80) {
		if (g->sectors == 8) {
			if (g->heads == 2)
				return 0xFB;	// 5.25"/3.5" 640KB MFM    C/H/S 80/2/8
			else if (g->heads == 1)
				return 0xFA;	// 5.25"/3.5" 320KB MFM    C/H/S 80/1/8
		}
		else if (g->sectors == 9) {
			if (g->heads == 2)	// NTS: This also matches a 5.25" 720KB format (0xF8) but somehow that seems unlikely to be used in practice
				return 0xF9;	// 3.5" 720KB MFM     C/H/S 80/2/9
			else if (g->heads == 1)
				return 0xF8;	// 5.25"/3.5" 360KB MFM    C/H/S 80/1/9
		}
		else if (g->sectors == 15) {
			if (g->heads == 2)
				return 0xF9;	// 5.25" 1.2MB        C/H/S 80/2/15
		}
		else if (g->sectors == 18) {
			if (g->heads == 2)	// NTS: This also matches a 3.5" 1.44MB format (0xF9) which is... what exactly?
				return 0xF0;	// 3.5" 1.44MB        C/H/S 80/2/18
		}
		else if (g->sectors == 36) {
			if (g->heads == 2)	// 3.5" 2.88MB        C/H/S 80/2/36
				return 0xF0;
		}
	}

	return 0xF8;
}

int main(int argc,char **argv) {
	const char *s_sectorsize = NULL;
	const char *s_media_type = NULL;
	const char *s_geometry = NULL;
	const char *s_image = NULL;
	const char *s_size = NULL;
	int i,fd;

	memset(&disk_geo,0,sizeof(disk_geo));

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
			else if (!strcmp(a,"size")) {
				s_size = argv[i++];
			}
			else if (!strcmp(a,"sectorsize")) {
				s_sectorsize = argv[i++];
			}
			else if (!strcmp(a,"media-type")) {
				s_media_type = argv[i++];
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

	if (s_image == NULL || (s_geometry == NULL && s_size == NULL)) {
		fprintf(stderr,"mkfatfs --image <image> ...\n");
		fprintf(stderr,"Generate a new filesystem. You must specify either --geometry or --size.\n");
		fprintf(stderr,"\n");
		fprintf(stderr,"--geometry <C/H/S>          Specify geometry of the disk image\n");
		fprintf(stderr,"--size <n>                  Disk image is N bytes long. Use suffixes K, M, G to specify KB, MB, GB\n");
		fprintf(stderr,"--sectorsize <n>            Disk image uses N bytes/sector (default 512)\n");
		fprintf(stderr,"--media-type <x>            Force media type byte X\n");
		return 1;
	}

	if (s_sectorsize != NULL)
		disk_bytes_per_sector = (unsigned int)strtoul(s_sectorsize,NULL,0);
	else
		disk_bytes_per_sector = 512U;

	if ((disk_bytes_per_sector % 32) != 0 || disk_bytes_per_sector < 128 || disk_bytes_per_sector > 4096) return 1;

	if (s_geometry != NULL) {
		if (int13cnv_parse_chs_geometry(&disk_geo,s_geometry)) {
			fprintf(stderr,"Failed to parse geometry\n");
			return 1;
		}
	}

	if (s_size != NULL) {
		uint64_t cyl;
		char *s=NULL;

		disk_size_bytes = (uint64_t)strtoull(s_size,&s,0);
		if (disk_size_bytes == 0) return 1;

		// suffix
		if (s && *s) {
			if (tolower(*s) == 'k')
				disk_size_bytes <<= (uint64_t)10;	// KB
			else if (tolower(*s) == 'm')
				disk_size_bytes <<= (uint64_t)20;	// MB
			else if (tolower(*s) == 'g')
				disk_size_bytes <<= (uint64_t)30;	// GB
			else if (tolower(*s) == 't')
				disk_size_bytes <<= (uint64_t)40;	// TB
		}

		if (disk_geo.heads == 0)
			disk_geo.heads = 16;

		if (disk_size_bytes > (uint64_t)(512ULL * 1024ULL)) { // 512MB
			if (disk_geo.sectors == 0)
				disk_geo.sectors = 63;
		}
		else if (disk_size_bytes > (uint64_t)(32ULL * 1024ULL)) { // 32MB
			if (disk_geo.sectors == 0)
				disk_geo.sectors = 32;
		}
		else {
			if (disk_geo.sectors == 0)
				disk_geo.sectors = 15;
		}

		disk_sectors = disk_size_bytes / (uint64_t)disk_bytes_per_sector;

		cyl = disk_sectors / ((uint64_t)disk_geo.heads * (uint64_t)disk_geo.sectors);
		if (cyl > (uint64_t)16384UL) cyl = (uint64_t)16384UL;

		disk_geo.cylinders = (uint16_t)cyl;
	}
	else {
		if (disk_geo.cylinders == 0 || disk_geo.heads == 0 || disk_geo.sectors == 0) return 1;

		disk_sectors = (uint64_t)disk_geo.heads * (uint64_t)disk_geo.sectors *
			(uint64_t)disk_geo.cylinders;
		disk_size_bytes = disk_sectors * (uint64_t)disk_bytes_per_sector;
	}

	if (s_media_type != NULL) {
		disk_media_type_byte = (uint8_t)strtoul(s_media_type,NULL,0);
		if (disk_media_type_byte < 0xF0) return 1;
	}
	else {
		disk_media_type_byte = guess_from_geometry(&disk_geo);
	}

	printf("Formatting: %llu sectors x %u bytes per sector = %llu bytes (C/H/S %u/%u/%u) media type 0x%02x\n",
		(unsigned long long)disk_sectors,
		(unsigned int)disk_bytes_per_sector,
		(unsigned long long)disk_sectors * (unsigned long long)disk_bytes_per_sector,
		(unsigned int)disk_geo.cylinders,
		(unsigned int)disk_geo.heads,
		(unsigned int)disk_geo.sectors,
		(unsigned int)disk_media_type_byte);

	fd = open(s_image,O_RDWR|O_BINARY|O_CREAT|O_TRUNC,0644);
	if (fd < 0) {
		fprintf(stderr,"Unable to open disk image, %s\n",strerror(errno));
		return 1;
	}

	/* extend the file out to the size we want. */
#if defined(_LINUX)
	/* Linux: we can easily make the file sparse and quickly generate what we want with ftruncate.
	 * On ext3/ext4 volumes this is a very fast way to create a disk image of any size AND to make
	 * it sparse so that disk space is allocated only to what we write data to. */
	if (ftruncate(fd,(off_t)disk_size_bytes)) {
		fprintf(stderr,"ftruncate failed\n");
		return 1;
	}
#elif defined(_WIN32)
	/* TODO: Make the file NTFS sparse (if possible), then set file size */
#else
	/* try to make it sparse using lseek then a write.
	 * this is the most portable way I know to make a file of the size we want.
	 * it may cause lseek+write to take a long time in the system kernel to
	 * satisfy the request especially if the filesystem is not treating it as sparse i.e. FAT. */
	{
		char c = 0;

		if (lseek(fd,(off_t)disk_size_bytes - (off_t)1U,SEEK_SET) != ((off_t)disk_size_bytes - (off_t)1U)) {
			fprintf(stderr,"lseek failed\n");
			return 1;
		}
		if (write(fd,&c,1) != 1) {
			fprintf(stderr,"write failed\n");
			return 1;
		}
	}
#endif

	close(fd);
	fd = -1;
	return 0;
}

