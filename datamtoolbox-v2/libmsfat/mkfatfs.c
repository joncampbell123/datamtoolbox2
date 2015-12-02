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

static libpartmbr_sector_t			diskimage_sector;

static struct chs_geometry_t			disk_geo;
static uint64_t					disk_sectors = 0;
static unsigned int				disk_bytes_per_sector = 0;
static uint64_t					disk_size_bytes = 0;
static uint8_t					disk_media_type_byte = 0;
static uint8_t					make_partition = 0;
static uint64_t					partition_offset = 0;
static uint64_t					partition_size = 0;
static uint8_t					partition_type = 0;
static uint8_t					lba_mode = 0;
static uint8_t					chs_mode = 0;
static uint8_t					allow_fat32 = 1;
static uint8_t					allow_fat16 = 1;
static uint8_t					allow_fat12 = 1;
static uint8_t					force_fat = 0;

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
	struct libmsfat_disk_locations_and_info base_info,final_info;
	const char *s_partition_offset = NULL;
	const char *s_partition_size = NULL;
	const char *s_partition_type = NULL;
	const char *s_sectorsize = NULL;
	const char *s_media_type = NULL;
	const char *s_geometry = NULL;
	const char *s_image = NULL;
	const char *s_size = NULL;
	int i,fd;

	memset(&final_info,0,sizeof(final_info));
	memset(&base_info,0,sizeof(base_info));
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
			else if (!strcmp(a,"partition")) {
				make_partition = 1;
			}
			else if (!strcmp(a,"partition-offset")) {
				s_partition_offset = argv[i++];
			}
			else if (!strcmp(a,"partition-size")) {
				s_partition_size = argv[i++];
			}
			else if (!strcmp(a,"partition-type")) {
				s_partition_type = argv[i++];
			}
			else if (!strcmp(a,"lba")) {
				lba_mode = 1;
			}
			else if (!strcmp(a,"chs")) {
				chs_mode = 1;
			}
			else if (!strcmp(a,"no-fat32")) {
				allow_fat32 = 0;
			}
			else if (!strcmp(a,"fat32")) {
				allow_fat32 = 1;
			}
			else if (!strcmp(a,"no-fat16")) {
				allow_fat16 = 0;
			}
			else if (!strcmp(a,"fat16")) {
				allow_fat16 = 1;
			}
			else if (!strcmp(a,"no-fat12")) {
				allow_fat12 = 0;
			}
			else if (!strcmp(a,"fat12")) {
				allow_fat12 = 1;
			}
			else if (!strcmp(a,"fat")) {
				force_fat = (int)strtoul(argv[i++],NULL,0);
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
		fprintf(stderr,"--partition                 Make partition table (as hard drive)\n");
		fprintf(stderr,"--partition-offset <x>      Starting sector number of the partition\n");
		fprintf(stderr,"--partition-size <n>        Make partition within image N bytes (with suffixes)\n");
		fprintf(stderr,"--partition-type <x>        Force partition type X\n");
		fprintf(stderr,"--fat32 / --no-fat32        Allow / Don't allow FAT32\n");
		fprintf(stderr,"--fat16 / --no-fat16        Allow / Don't allow FAT16\n");
		fprintf(stderr,"--fat12 / --no-fat12        Allow / Don't allow FAT12\n");
		fprintf(stderr,"--fat <x>                   Force FATx (x can be 12, 16, or 32)\n");
		fprintf(stderr,"--lba                       LBA mode\n");
		fprintf(stderr,"--chs                       CHS mode\n");
		return 1;
	}

	if (force_fat != 0 && !(force_fat == 12 || force_fat == 16 || force_fat == 32)) {
		fprintf(stderr,"Invalid FAT bit width\n");
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

		if (lba_mode || disk_size_bytes > (uint64_t)(128ULL << 20ULL)) { // 64MB
			if (disk_geo.heads == 0)
				disk_geo.heads = 16;
		}
		else {
			if (disk_geo.heads == 0)
				disk_geo.heads = 8;
		}

		if (lba_mode || disk_size_bytes > (uint64_t)(96ULL << 20ULL)) { // 96MB
			if (disk_geo.sectors == 0)
				disk_geo.sectors = 63;
		}
		else if (disk_size_bytes > (uint64_t)(32ULL << 20ULL)) { // 32MB
			if (disk_geo.sectors == 0)
				disk_geo.sectors = 32;
		}
		else {
			if (disk_geo.sectors == 0)
				disk_geo.sectors = 15;
		}

		if (!lba_mode && !chs_mode) {
			if (disk_size_bytes >= (uint64_t)(8192ULL << 20ULL))
				lba_mode = 1;
			else
				chs_mode = 1;
		}

		disk_sectors = disk_size_bytes / (uint64_t)disk_bytes_per_sector;
		if (s_sectorsize == NULL) {
			while (disk_sectors >= (uint64_t)0xFFFFFFF0UL) {
				disk_bytes_per_sector *= (uint64_t)2UL;
				disk_sectors = disk_size_bytes / (uint64_t)disk_bytes_per_sector;
			}
		}

		/* how many cylinders? */
		cyl = disk_sectors / ((uint64_t)disk_geo.heads * (uint64_t)disk_geo.sectors);

		/* BIOS CHS translations to exceed 512MB limit */
		while (cyl > 1023 && disk_geo.heads < 128) {
			disk_geo.heads *= 2;
			cyl = disk_sectors / ((uint64_t)disk_geo.heads * (uint64_t)disk_geo.sectors);
		}
		if (cyl > 1023 && disk_geo.heads < 255) {
			disk_geo.heads = 255;
			cyl = disk_sectors / ((uint64_t)disk_geo.heads * (uint64_t)disk_geo.sectors);
		}

		/* final limit (16383 at IDE, 1024 at BIOS) */
		if (cyl > (uint64_t)1024UL) cyl = (uint64_t)1024UL;

		/* that's the number of cylinders! */
		disk_geo.cylinders = (uint16_t)cyl;

		if (chs_mode) {
			disk_sectors = (uint64_t)disk_geo.heads * (uint64_t)disk_geo.sectors *
				(uint64_t)disk_geo.cylinders;
			disk_size_bytes = disk_sectors * (uint64_t)disk_bytes_per_sector;
		}
	}
	else {
		if (disk_geo.cylinders == 0 || disk_geo.heads == 0 || disk_geo.sectors == 0) return 1;

		disk_sectors = (uint64_t)disk_geo.heads * (uint64_t)disk_geo.sectors *
			(uint64_t)disk_geo.cylinders;
		disk_size_bytes = disk_sectors * (uint64_t)disk_bytes_per_sector;
	}

	if (!lba_mode && !chs_mode) {
		if (disk_geo.sectors >= 63 && disk_geo.cylinders >= 4096)
			lba_mode = 1;
		else
			chs_mode = 1;
	}

	if (s_partition_offset != NULL)
		partition_offset = (uint64_t)strtoull(s_partition_offset,NULL,0);

	if (s_partition_size != NULL) {
		char *s=NULL;

		partition_size = (uint64_t)strtoull(s_partition_size,&s,0);

		if (s && *s) {
			if (tolower(*s) == 'k')
				partition_size <<= (uint64_t)10;	// KB
			else if (tolower(*s) == 'm')
				partition_size <<= (uint64_t)20;	// MB
			else if (tolower(*s) == 'g')
				partition_size <<= (uint64_t)30;	// GB
			else if (tolower(*s) == 't')
				partition_size <<= (uint64_t)40;	// TB
		}

		partition_size /= (uint64_t)disk_bytes_per_sector;
	}

	// automatically default to a partition starting on a track boundary.
	// MS-DOS, especially 6.x and earlier, require this. MS-DOS 7 and higher
	// demand this IF the partition was made in CHS mode.
	if (partition_offset == (uint64_t)0UL)
		partition_offset = disk_geo.sectors;

	if (partition_offset >= disk_sectors) {
		fprintf(stderr,"Partition offset >= disk sectors\n");
		return 1;
	}

	if (partition_size == (uint64_t)0UL) {
		uint64_t diskrnd = disk_sectors;
		diskrnd -= diskrnd % (uint64_t)disk_geo.sectors;
		if (partition_offset >= diskrnd) {
			fprintf(stderr,"Partition offset >= disk sectors (rounded down to track)\n");
			return 1;
		}

		partition_size = diskrnd - partition_offset;
	}

	base_info.BytesPerSector = disk_bytes_per_sector;
	if (make_partition)
		base_info.TotalSectors = (uint32_t)partition_size;
	else
		base_info.TotalSectors = (uint32_t)disk_sectors;

	if (force_fat != 0) {
		if (force_fat == 12 && !allow_fat12) {
			fprintf(stderr,"--no-fat12 and FAT12 forced does not make sense\n");
			return 1;
		}
		else if (force_fat == 16 && !allow_fat16) {
			fprintf(stderr,"--no-fat16 and FAT16 forced does not make sense\n");
			return 1;
		}
		else if (force_fat == 32 && !allow_fat32) {
			fprintf(stderr,"--no-fat32 and FAT32 forced does not make sense\n");
			return 1;
		}

		base_info.FAT_size = force_fat;
	}
	if (base_info.FAT_size == 0) {
		// if FAT32 allowed, and 2GB or larger, then do FAT32
		if (allow_fat32 && ((uint64_t)base_info.TotalSectors * (uint64_t)base_info.BytesPerSector) >= ((uint64_t)(2000ULL << 20ULL)))
			base_info.FAT_size = 32;
		// if FAT16 allowed, and 32MB or larger, then do FAT16
		else if (allow_fat16 && ((uint64_t)base_info.TotalSectors * (uint64_t)base_info.BytesPerSector) >= ((uint64_t)(30ULL << 20ULL)))
			base_info.FAT_size = 16;
		// if FAT12 allowed, then do it
		else if (allow_fat12 && ((uint64_t)base_info.TotalSectors * (uint64_t)base_info.BytesPerSector) < ((uint64_t)(31ULL << 20ULL)))
			base_info.FAT_size = 12;
		// maybe we can do FAT32?
		else if (allow_fat32 && ((uint64_t)base_info.TotalSectors * (uint64_t)base_info.BytesPerSector) >= ((uint64_t)(200ULL << 20ULL)))
			base_info.FAT_size = 32;
		else if (allow_fat16)
			base_info.FAT_size = 16;
		else if (allow_fat12)
			base_info.FAT_size = 12;
		else if (allow_fat32)
			base_info.FAT_size = 32;
	}

	if (s_partition_type != NULL) {
		partition_type = (uint8_t)strtoul(s_partition_type,NULL,0);
		if (partition_type == 0) return 1;
	}
	else {
		// resides in the first 32MB. this is the only case that disregards LBA mode
		if ((partition_offset+partition_size)*((uint64_t)disk_bytes_per_sector) <= ((uint64_t)32UL << (uint64_t)20UL)) {
			if (base_info.FAT_size == 16)
				partition_type = 0x04; // FAT16 with less than 65536 sectors
			else
				partition_type = 0x01; // FAT12 as primary partition in the first 32MB
		}
		// resides in the first 8GB.
		else if ((partition_offset+partition_size)*((uint64_t)disk_bytes_per_sector) <= ((uint64_t)8192UL << (uint64_t)20UL)) {
			if (base_info.FAT_size == 32)
				partition_type = lba_mode ? 0x0C : 0x0B;	// FAT32 with (lba_mode ? LBA : CHS) mode
			else if (base_info.FAT_size == 16)
				partition_type = lba_mode ? 0x0E : 0x06;	// FAT16B with (lba_mode ? LBA : CHS) mode
			else
				partition_type = 0x01; // FAT12
		}
		// resides past 8GB. this is the only case that disregards CHS mode
		else {
			if (base_info.FAT_size == 32)
				partition_type = 0x0C;	// FAT32 with LBA mode
			else if (base_info.FAT_size == 16)
				partition_type = 0x0E;	// FAT16B with LBA mode
			else
				partition_type = 0x01; // FAT12
		}
	}

	if (s_media_type != NULL) {
		disk_media_type_byte = (uint8_t)strtoul(s_media_type,NULL,0);
		if (disk_media_type_byte < 0xF0) return 1;
	}
	else {
		disk_media_type_byte = guess_from_geometry(&disk_geo);
	}

	assert(lba_mode || chs_mode);
	printf("Formatting: %llu sectors x %u bytes per sector = %llu bytes (C/H/S %u/%u/%u) media type 0x%02x %s\n",
		(unsigned long long)disk_sectors,
		(unsigned int)disk_bytes_per_sector,
		(unsigned long long)disk_sectors * (unsigned long long)disk_bytes_per_sector,
		(unsigned int)disk_geo.cylinders,
		(unsigned int)disk_geo.heads,
		(unsigned int)disk_geo.sectors,
		(unsigned int)disk_media_type_byte,
		lba_mode ? "LBA" : "CHS");

	if (make_partition) {
		printf("   Partition: type=0x%02x sectors %llu-%llu\n",
			(unsigned int)partition_type,
			(unsigned long long)partition_offset,
			(unsigned long long)(partition_offset + partition_size - (uint64_t)1UL));

		if (chs_mode && (partition_offset % (uint64_t)disk_geo.sectors) != 0)
			printf("    WARNING: Partition does not start on track boundary (CHS mode warning). MS-DOS may have issues with it.\n");
		if (chs_mode && ((partition_offset + partition_size) % (uint64_t)disk_geo.sectors) != 0)
			printf("    WARNING: Partition does not end on track boundary (CHS mode warning). MS-DOS may have issues with it.\n");

		if (partition_offset == 0 || partition_size == 0) return 1;
	}

	printf("   FAT filesystem FAT%u\n",
		base_info.FAT_size);

	if (base_info.FAT_size == 0) {
		fprintf(stderr,"Unable to decide on a FAT bit width\n");
		return 1;
	}

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
	/* Windows: Assuming NTFS and NT kernel (XP/Vista/7/8/10), mark the file as sparse, then set the file size. */
	{
		DWORD dwTemp;
		LONG lo,hi;
		HANDLE h;

		h = (HANDLE)_get_osfhandle(fd);
		if (h == INVALID_HANDLE_VALUE) return 1; // <- what?
		DeviceIoControl(h,FSCTL_SET_SPARSE,NULL,0,NULL,0,&dwTemp,NULL); // <- don't care if it fails.

		/* FIXME: "LONG" is 32-bit wide even in Win64, right? Does SetFilePointer() work the same in Win64? */
		lo = (LONG)(disk_size_bytes & 0xFFFFFFFFUL);
		hi = (LONG)(disk_size_bytes >> (uint64_t)32UL);
		if (SetFilePointer(h,lo,&hi,FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
			fprintf(stderr,"SetFilePointer failed\n");
			return 1;
		}

		/* and then make that the end of the file */
		if (SetEndOfFile(h) == 0) {
			fprintf(stderr,"SetEndOfFile failed\n");
			return 1;
		}
	}
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

	if (make_partition) {
		struct libpartmbr_state_t diskimage_state;
		struct libpartmbr_entry_t ent;
		struct chs_geometry_t chs;

		libpartmbr_state_zero(&diskimage_state);

		if (libpartmbr_create_partition_table(diskimage_sector,&diskimage_state)) {
			fprintf(stderr,"Failed to create partition table sector\n");
			return 1;
		}

		ent.partition_type = partition_type;
		ent.first_lba_sector = partition_offset;
		ent.number_lba_sectors = partition_size;
		if (int13cnv_lba_to_chs(&chs,&disk_geo,partition_offset)) {
			fprintf(stderr,"Failed to convert start LBA -> CHS\n");
			return 1;
		}
		int13cnv_chs_int13_cliprange(&chs,&disk_geo);
		if (int13cnv_chs_to_int13(&ent.first_chs_sector,&chs)) {
			fprintf(stderr,"Failed to convert start CHS -> INT13\n");
			return 1;
		}

		if (int13cnv_lba_to_chs(&chs,&disk_geo,(uint32_t)partition_offset + (uint32_t)partition_size - (uint32_t)1UL)) {
			fprintf(stderr,"Failed to convert end LBA -> CHS\n");
			return 1;
		}
		int13cnv_chs_int13_cliprange(&chs,&disk_geo);
		if (int13cnv_chs_to_int13(&ent.last_chs_sector,&chs)) {
			fprintf(stderr,"Failed to convert end CHS -> INT13\n");
			return 1;
		}

		if (libpartmbr_write_entry(diskimage_sector,&ent,&diskimage_state,0)) {
			fprintf(stderr,"Unable to write entry\n");
			return 1;
		}

		if (lseek(fd,0,SEEK_SET) != 0 || write(fd,diskimage_sector,LIBPARTMBR_SECTOR_SIZE) != LIBPARTMBR_SECTOR_SIZE) {
			fprintf(stderr,"Failed to write MBR back\n");
			return 1;
		}
	}

	close(fd);
	fd = -1;
	return 0;
}

