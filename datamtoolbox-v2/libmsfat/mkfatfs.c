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
#include <time.h>
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
static uint16_t					set_cluster_size = 0;
static uint8_t					allow_non_power_of_2_cluster_size = 0;
static uint8_t					allow_64kb_or_larger_clusters = 0;
static uint32_t					set_root_directory_entries = 0;
static uint32_t					root_directory_entries = 0;
static uint32_t					set_fsinfo_sector = 0;
static uint32_t					reserved_sectors = 0;
static uint8_t					set_fat_tables = 0;
static uint32_t					set_reserved_sectors = 0;
static uint32_t					volume_id = 0;
static const char*				volume_label = NULL;

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
	struct libmsfat_context_t *msfatctx = NULL;
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
			else if (!strcmp(a,"cluster-size")) {
				set_cluster_size = (uint16_t)strtoul(argv[i++],NULL,0);
			}
			else if (!strcmp(a,"cluster-non-power-of-2")) {
				allow_non_power_of_2_cluster_size = 1;
			}
			else if (!strcmp(a,"large-clusters")) {
				allow_64kb_or_larger_clusters = 1;
			}
			else if (!strcmp(a,"fat-tables")) {
				set_fat_tables = (uint8_t)strtoul(argv[i++],NULL,0);
			}
			else if (!strcmp(a,"root-directories")) {
				set_root_directory_entries = (uint32_t)strtoul(argv[i++],NULL,0);
			}
			else if (!strcmp(a,"fsinfo")) {
				set_fsinfo_sector = (uint32_t)strtoul(argv[i++],NULL,0);
			}
			else if (!strcmp(a,"reserved-sectors")) {
				set_reserved_sectors = (uint32_t)strtoul(argv[i++],NULL,0);
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
		fprintf(stderr,"--cluster-size <x>          Cluster size in bytes\n");
		fprintf(stderr,"--cluster-non-power-of-2    Allow cluster size that is not a power of 2 (FAT specification violation!)\n");
		fprintf(stderr,"--large-clusters            Allow clusters to be 64KB or larger (FAT specification violation!)\n");
		fprintf(stderr,"--fat-tables <x>            Number of FAT tables\n");
		fprintf(stderr,"--root-directories <x>      Number of root directory entries\n");
		fprintf(stderr,"--fsinfo <x>                What sector to put the FAT32 FSInfo at\n");
		fprintf(stderr,"--reserved-sectors <x>      Number of reserved sectors\n");
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
	if (base_info.FAT_size == 0) {
		fprintf(stderr,"FAT size not determinated\n");
		return 1;
	}

	if (set_fat_tables)
		base_info.FAT_tables = set_fat_tables;
	else if (base_info.FAT_tables == 0)
		base_info.FAT_tables = 2;

	if (set_cluster_size != 0) {
		unsigned long x;

		x = ((unsigned long)set_cluster_size + ((unsigned long)base_info.BytesPerSector / 2UL)) / (unsigned long)base_info.BytesPerSector;
		if (x == 0) x = base_info.BytesPerSector;
		if (x > 255UL) x = 255UL;
		base_info.Sectors_Per_Cluster = (uint8_t)x;
	}
	else {
		uint32_t cluslimit;

		if (base_info.FAT_size == 32) {
			/* it's better to keep the FAT table small. try not to do one cluster per sector */
			if (base_info.TotalSectors >= (uint32_t)0x3FFFF5UL) {
				base_info.Sectors_Per_Cluster = 16;
				if (base_info.TotalSectors >= (uint32_t)0x00FFFFF5UL)
					cluslimit = (uint32_t)0x03FFFFF5UL;
				else
					cluslimit = (uint32_t)0x00FFFFF5UL;
			}
			else {
				cluslimit = (uint32_t)0x0007FFF5UL;
				base_info.Sectors_Per_Cluster = 1;
			}
		}
		else {
			base_info.Sectors_Per_Cluster = 1;
			if (base_info.FAT_size == 16)
				cluslimit = (uint32_t)0x0000FFF5UL;
			else
				cluslimit = (uint32_t)0x00000FF5UL;
		}

		while (base_info.Sectors_Per_Cluster < 0xFF && (base_info.TotalSectors / (uint32_t)base_info.Sectors_Per_Cluster) >= cluslimit)
			base_info.Sectors_Per_Cluster++;
	}

	if (base_info.FAT_size == 32) {
		base_info.RootDirectory_size = 0;
		root_directory_entries = 0; // FAT32 makes the root directory an allocation chain
		if (set_reserved_sectors >= (uint32_t)2U)
			reserved_sectors = set_reserved_sectors;
		else
			reserved_sectors = 32; // typical FAT32 value, allowing FSInfo at sector 6
	}
	else {
		if (set_root_directory_entries != 0)
			root_directory_entries = set_root_directory_entries;
		else if (disk_size_bytes >= (uint64_t)(100ULL << 20ULL)) // 100MB
			root_directory_entries = 512; // 16KB
		else if (disk_size_bytes >= (uint64_t)(60ULL << 20ULL)) // 60MB
			root_directory_entries = 256; // 8KB
		else if (disk_size_bytes >= (uint64_t)(32ULL << 20ULL)) // 32MB
			root_directory_entries = 128; // 4KB
		else if (disk_size_bytes >= (uint64_t)(26ULL << 20ULL)) // 26MB
			root_directory_entries = 64; // 2KB
		else
			root_directory_entries = 32; // 1KB

		base_info.RootDirectory_size = ((root_directory_entries * 32) + 511) / 512;

		if (set_reserved_sectors != (uint32_t)0U)
			reserved_sectors = set_reserved_sectors;
		else
			reserved_sectors = 1;
	}

	if (base_info.FAT_size == 32) {
		if (set_fsinfo_sector != 0)
			base_info.fat32.BPB_FSInfo = set_fsinfo_sector;
		else
			base_info.fat32.BPB_FSInfo = 6;
	}
	else {
		base_info.fat32.BPB_FSInfo = 0;
	}

	if (base_info.fat32.BPB_FSInfo >= reserved_sectors)
		base_info.fat32.BPB_FSInfo = reserved_sectors - 1;

	if (!allow_non_power_of_2_cluster_size) {
		/* need to round to a power of 2 */
		if (base_info.Sectors_Per_Cluster >= (64+1)/*65*/)
			base_info.Sectors_Per_Cluster = 128;
		else if (base_info.Sectors_Per_Cluster >= (32+1)/*33*/)
			base_info.Sectors_Per_Cluster = 64;
		else if (base_info.Sectors_Per_Cluster >= (16+1)/*17*/)
			base_info.Sectors_Per_Cluster = 32;
		else if (base_info.Sectors_Per_Cluster >= (8+1)/*9*/)
			base_info.Sectors_Per_Cluster = 16;
		else if (base_info.Sectors_Per_Cluster >= (4+1)/*5*/)
			base_info.Sectors_Per_Cluster = 8;
		else if (base_info.Sectors_Per_Cluster >= (2+1)/*3*/)
			base_info.Sectors_Per_Cluster = 4;
		else if (base_info.Sectors_Per_Cluster >= 2)
			base_info.Sectors_Per_Cluster = 2;
		else
			base_info.Sectors_Per_Cluster = 1;
	}

	if (!allow_64kb_or_larger_clusters) {
		uint32_t sz;

		sz = (uint32_t)base_info.Sectors_Per_Cluster * (uint32_t)base_info.BytesPerSector;
		if (sz >= (uint32_t)0x10000UL) {
			fprintf(stderr,"WARNING: cluster size choice means cluster is 64KB or larger. choosing smaller cluster size.\n");

			if (allow_non_power_of_2_cluster_size)
				base_info.Sectors_Per_Cluster = (uint32_t)0xFFFFUL / (uint32_t)base_info.BytesPerSector;
			else {
				while (sz >= (uint32_t)0x10000UL && base_info.Sectors_Per_Cluster >= 2U) {
					base_info.Sectors_Per_Cluster /= 2U;
					sz = (uint32_t)base_info.Sectors_Per_Cluster * (uint32_t)base_info.BytesPerSector;
				}
			}
		}
	}

	// now we need to figure out number of clusters.
	{
		// initial estimate. doesn't factor in root directory, root directory, boot sector...
		// estimate will be a little large compared to the final value.
		uint32_t cluslimit;
		uint32_t x,t;

		if (base_info.FAT_size == 32)
			cluslimit = (uint32_t)0x0FFFFFF5UL;
		else if (base_info.FAT_size == 16)
			cluslimit = (uint32_t)0x0000FFF5UL;
		else
			cluslimit = (uint32_t)0x00000FF5UL;

		x = base_info.TotalSectors;

		if (x > (uint32_t)reserved_sectors) x -= (uint32_t)reserved_sectors;
		else x = (uint32_t)0;

		if (x > (uint32_t)base_info.RootDirectory_size) x -= (uint32_t)base_info.RootDirectory_size;
		else x = (uint32_t)0;

		x /= (uint32_t)base_info.Sectors_Per_Cluster;
		if (x == (uint32_t)0UL) {
			fprintf(stderr,"Too small for any clusters\n");
			return 1;
		}

		/* use it to calculate size of the FAT table */
		if (base_info.FAT_size == 12)
			t = ((x + (uint32_t)1UL) / (uint32_t)2UL) * (uint32_t)3UL; // 1/2x * 3 = number of 24-bit doubles
		else
			t = x * (base_info.FAT_size / (uint32_t)8UL);
		t = (t + (uint32_t)base_info.BytesPerSector - (uint32_t)1UL) / (uint32_t)base_info.BytesPerSector;
		base_info.FAT_table_size = t;

		/* do it again */
		x = base_info.TotalSectors;

		if (x > (uint32_t)reserved_sectors) x -= (uint32_t)reserved_sectors;
		else x = (uint32_t)0;

		if (x > (uint32_t)base_info.RootDirectory_size) x -= (uint32_t)base_info.RootDirectory_size;
		else x = (uint32_t)0;

		t = (uint32_t)base_info.FAT_table_size * (uint32_t)base_info.FAT_tables;
		if (x > t) x -= t;
		else x = (uint32_t)0;

		x /= (uint32_t)base_info.Sectors_Per_Cluster;
		if (x == (uint32_t)0UL) {
			fprintf(stderr,"Too small for any clusters\n");
			return 1;
		}

		/* clip the cluster count (and therefore the sector count) if we have to */
		if (x >= cluslimit) {
			fprintf(stderr,"WARNING: total sector and cluster count exceeds highest valid count for FAT width\n");
			fprintf(stderr,"          sectors=%lu data_clusters=%lu limit=%lu\n",
				(unsigned long)base_info.TotalSectors,
				(unsigned long)x,
				(unsigned long)cluslimit);

			assert(cluslimit != (uint32_t)0UL);
			x = cluslimit - (uint32_t)1UL;

			if (base_info.FAT_size == 12)
				t = ((x + (uint32_t)1UL) / (uint32_t)2UL) * (uint32_t)3UL; // 1/2x * 3 = number of 24-bit doubles
			else
				t = x * (base_info.FAT_size / (uint32_t)8UL);
			t = (t + (uint32_t)base_info.BytesPerSector - (uint32_t)1UL) / (uint32_t)base_info.BytesPerSector;
			base_info.FAT_table_size = t;

			base_info.TotalSectors =
				(x * (uint32_t)base_info.Sectors_Per_Cluster) +
				(uint32_t)base_info.FAT_table_size +
				(uint32_t)base_info.RootDirectory_size +
				(uint32_t)reserved_sectors;
		}

		base_info.Total_data_clusters = x;
		base_info.Total_clusters = x + (uint32_t)2UL;
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

	// TODO: allow user override
	volume_label = "NO LABEL";

	// TODO: allow user override
	volume_id = (uint32_t)time(NULL);

	printf("   FAT filesystem FAT%u. %lu x %lu (%lu bytes) per cluster. %lu sectors => %lu clusters (%lu)\n",
		base_info.FAT_size,
		(unsigned long)base_info.Sectors_Per_Cluster,
		(unsigned long)base_info.BytesPerSector,
		(unsigned long)base_info.Sectors_Per_Cluster * (unsigned long)base_info.BytesPerSector,
		(unsigned long)base_info.TotalSectors,
		(unsigned long)base_info.Total_data_clusters,
		(unsigned long)base_info.Total_clusters);
	printf("   Reserved sectors: %lu\n",
		(unsigned long)reserved_sectors);
	printf("   Root directory entries: %lu (%lu sectors)\n",
		(unsigned long)root_directory_entries,
		(unsigned long)base_info.RootDirectory_size);
	printf("   %u FAT tables: %lu sectors per table\n",
		(unsigned int)base_info.FAT_tables,
		(unsigned long)base_info.FAT_table_size);
	printf("   Volume ID: 0x%08lx\n",
		(unsigned long)volume_id);

	if (base_info.FAT_size == 32)
		printf("   FAT32 FSInfo sector: %lu\n",
			(unsigned long)base_info.fat32.BPB_FSInfo);

	if (base_info.FAT_size == 32 && base_info.fat32.BPB_FSInfo == 0) {
		fprintf(stderr,"FAT32 requires a sector set aside for FSInfo\n");
		return 1;
	}

	if (base_info.FAT_size == 0) {
		fprintf(stderr,"Unable to decide on a FAT bit width\n");
		return 1;
	}
	if (base_info.Sectors_Per_Cluster == 0) {
		fprintf(stderr,"Unable to determine cluster size\n");
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
		ent.first_lba_sector = (uint32_t)partition_offset;
		ent.number_lba_sectors = (uint32_t)partition_size;
		if (int13cnv_lba_to_chs(&chs,&disk_geo,(uint32_t)partition_offset)) {
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

	/* generate the boot sector! */
	{
		uint64_t offset;
		unsigned char sector[512];
		struct libmsfat_bootsector *bs = (struct libmsfat_bootsector*)sector;
		unsigned int bs_sz;
		int dfd;

		if (make_partition)
			offset = partition_offset;
		else
			offset = 0;
		offset *= (uint64_t)disk_bytes_per_sector;

		// TODO: options to emulate various DOS versions of the BPB
		if (base_info.FAT_size == 32)
			bs_sz = 90;
		else
			bs_sz = 62;

		// BEGIN
		memset(sector,0,sizeof(sector));

		// JMP instruction
		bs->BS_header.BS_jmpBoot[0] = 0xEB;
		bs->BS_header.BS_jmpBoot[1] = bs_sz - 2;
		bs->BS_header.BS_jmpBoot[2] = 0x90;
		memcpy(bs->BS_header.BS_OEMName,"DATATBOX",8);

		// trap in case of booting. trigger bootstrap
		sector[bs_sz] = 0xCD;
		sector[bs_sz+1] = 0x19;
		strcpy((char*)sector+bs_sz+2,"Not bootable, data only");

		// common BPB
		bs->BPB_common.BPB_BytsPerSec = htole16(disk_bytes_per_sector);
		bs->BPB_common.BPB_SecPerClus = base_info.Sectors_Per_Cluster;
		bs->BPB_common.BPB_RsvdSecCnt = htole16(reserved_sectors);
		bs->BPB_common.BPB_NumFATs = base_info.FAT_tables;
		if (base_info.FAT_size == 32)
			bs->BPB_common.BPB_RootEntCnt = 0;
		else
			bs->BPB_common.BPB_RootEntCnt = htole16(root_directory_entries);
		if (base_info.TotalSectors >= (uint32_t)65536 || base_info.FAT_size == 32)
			bs->BPB_common.BPB_TotSec16 = 0;
		else
			bs->BPB_common.BPB_TotSec16 = (uint16_t)htole16(base_info.TotalSectors);
		bs->BPB_common.BPB_Media = disk_media_type_byte;
		if (base_info.FAT_size == 32)
			bs->BPB_common.BPB_FATSz16 = 0;
		else
			bs->BPB_common.BPB_FATSz16 = htole16(base_info.FAT_table_size);
		bs->BPB_common.BPB_SecPerTrk = disk_geo.sectors;
		bs->BPB_common.BPB_NumHeads = disk_geo.heads;
		bs->BPB_common.BPB_TotSec32 = htole32(base_info.TotalSectors);

		if (base_info.FAT_size == 32) {
			bs->at36.BPB_FAT32.BPB_FATSz32 = htole32(base_info.FAT_table_size);
			bs->at36.BPB_FAT32.BPB_ExtFlags = 0;
			bs->at36.BPB_FAT32.BPB_FSVer = 0;
			bs->at36.BPB_FAT32.BPB_RootClus = 2; // FIXME: allow user override
			bs->at36.BPB_FAT32.BPB_FSInfo = htole16(base_info.fat32.BPB_FSInfo);
			bs->at36.BPB_FAT32.BPB_BkBootSec = htole16(1); // FIXME: allow user override. Also, what does MS-DOS normally do?
			bs->at36.BPB_FAT32.BS_DrvNum = (make_partition ? 0x80 : 0x00);
			bs->at36.BPB_FAT32.BS_BootSig = 0x29;
			bs->at36.BPB_FAT32.BS_VolID = htole32(volume_id);

			{
				const char *s = volume_label;
				uint8_t *d = bs->at36.BPB_FAT32.BS_VolLab;
				uint8_t *df = d + 11;

				while (*s && d < df) *d++ = *s++;
				while (d < df) *d++ = ' ';
			}

			memcpy(bs->at36.BPB_FAT32.BS_FilSysType,"FAT32   ",8);
		}
		else {
			bs->at36.BPB_FAT.BS_DrvNum = (make_partition ? 0x80 : 0x00);
			bs->at36.BPB_FAT.BS_BootSig = 0x29;
			bs->at36.BPB_FAT.BS_VolID = htole32(volume_id);

			{
				const char *s = volume_label;
				uint8_t *d = bs->at36.BPB_FAT.BS_VolLab;
				uint8_t *df = d + 11;

				while (*s && d < df) *d++ = *s++;
				while (d < df) *d++ = ' ';
			}

			if (base_info.FAT_size == 16)
				memcpy(bs->at36.BPB_FAT.BS_FilSysType,"FAT16   ",8);
			else
				memcpy(bs->at36.BPB_FAT.BS_FilSysType,"FAT12   ",8);
		}

		// signature
		sector[510] = 0x55;
		sector[511] = 0xAA;

		// write it
		if (lseek(fd,(off_t)offset,SEEK_SET) != (off_t)offset || write(fd,sector,512) != 512) {
			fprintf(stderr,"Failed to write BPB back\n");
			return 1;
		}

		// backup copy too
		if (base_info.FAT_size == 32) {
			if (make_partition)
				offset = partition_offset;
			else
				offset = 0;

			offset += (uint64_t)le16toh(bs->at36.BPB_FAT32.BPB_BkBootSec);
			offset *= (uint64_t)disk_bytes_per_sector;

			if (lseek(fd,(off_t)offset,SEEK_SET) != (off_t)offset || write(fd,sector,512) != 512) {
				fprintf(stderr,"Failed to write BPB back\n");
				return 1;
			}
		}

		// FSInfo
		if (base_info.FAT_size == 32) {
			struct libmsfat_fat32_fsinfo_t fsinfo;

			if (make_partition)
				offset = partition_offset;
			else
				offset = 0;

			offset += (uint64_t)le16toh(bs->at36.BPB_FAT32.BPB_FSInfo);
			offset *= (uint64_t)disk_bytes_per_sector;

			memset(&fsinfo,0,sizeof(fsinfo));
			fsinfo.FSI_LeadSig = htole32((uint32_t)0x41615252UL);
			fsinfo.FSI_StrucSig = htole32((uint32_t)0x61417272UL);
			fsinfo.FSI_Free_Count = (uint32_t)0xFFFFFFFFUL;
			fsinfo.FSI_Nxt_Free = (uint32_t)0xFFFFFFFFUL;
			fsinfo.FSI_TrailSig = htole32((uint32_t)0xAA550000UL);

			if (lseek(fd,(off_t)offset,SEEK_SET) != (off_t)offset || write(fd,&fsinfo,512) != 512) {
				fprintf(stderr,"Failed to write BPB back\n");
				return 1;
			}
		}

		// good. we need to work on it a a bit.
		msfatctx = libmsfat_context_create();
		if (msfatctx == NULL) {
			fprintf(stderr,"Failed to create msfat context\n");
			return 1;
		}
		dfd = dup(fd);
		if (libmsfat_context_assign_fd(msfatctx,dfd)) {
			fprintf(stderr,"Failed to assign file descriptor to msfat\n");
			close(dfd);
			return 1;
		}
		dfd = -1; /* takes ownership, drop it */

		msfatctx->partition_byte_offset = (uint64_t)partition_offset * (uint64_t)disk_bytes_per_sector;
		if (libmsfat_bs_compute_disk_locations(&final_info,bs)) {
			printf("Unable to locate disk locations.\n");
			return 1;
		}
		if (libmsfat_context_set_fat_info(msfatctx,&final_info)) {
			fprintf(stderr,"msfat library rejected disk location info\n");
			return 1;
		}

		printf("Disk location and FAT info:\n");
		printf("    FAT format:        FAT%u\n",
			final_info.FAT_size);
		printf("    FAT tables:        %u\n",
			final_info.FAT_tables);
		printf("    FAT table size:    %lu sectors\n",
			(unsigned long)final_info.FAT_table_size);
		printf("    FAT offset:        %lu sectors\n",
			(unsigned long)final_info.FAT_offset);
		printf("    Root directory:    %lu sectors\n",
			(unsigned long)final_info.RootDirectory_offset);
		printf("    Root dir size:     %lu sectors\n",
			(unsigned long)final_info.RootDirectory_size);
		printf("    Data offset:       %lu sectors\n",
			(unsigned long)final_info.Data_offset);
		printf("    Data size:         %lu sectors\n",
			(unsigned long)final_info.Data_size);
		printf("    Sectors/cluster:   %u\n",
			(unsigned int)final_info.Sectors_Per_Cluster);
		printf("    Bytes per sector:  %u\n",
			(unsigned int)final_info.BytesPerSector);
		printf("    Total clusters:    %lu\n",
			(unsigned long)final_info.Total_clusters);
		printf("    Total data clusts: %lu\n",
			(unsigned long)final_info.Total_data_clusters);
		printf("    Max clusters psbl: %lu\n",
			(unsigned long)final_info.Max_possible_clusters);
		printf("    Max data clus psbl:%lu\n",
			(unsigned long)final_info.Max_possible_data_clusters);
		printf("    Total sectors:     %lu sectors\n",
			(unsigned long)final_info.TotalSectors);
		if (final_info.FAT_size >= 32) {
			printf("    FAT32 FSInfo:      %lu sector\n",
				(unsigned long)final_info.fat32.BPB_FSInfo);
			printf("    Root dir cluster:  %lu\n",
				(unsigned long)final_info.fat32.RootDirectory_cluster);
		}

		for (i=0;i < final_info.FAT_tables;i++) {
			if (libmsfat_context_write_FAT(msfatctx,(uint32_t)0xFFFFFF00UL + (uint32_t)disk_media_type_byte,libmsfat_CLUSTER_0_MEDIA_TYPE,i)) {
				fprintf(stderr,"Cannot write cluster 1\n");
				return 1;
			}
			if (libmsfat_context_write_FAT(msfatctx,(uint32_t)0xFFFFFFFFUL,libmsfat_CLUSTER_1_DIRTY_FLAGS,i)) {
				fprintf(stderr,"Cannot write cluster 1\n");
				return 1;
			}
		}
	}

	msfatctx = libmsfat_context_destroy(msfatctx);
	close(fd);
	fd = -1;
	return 0;
}

