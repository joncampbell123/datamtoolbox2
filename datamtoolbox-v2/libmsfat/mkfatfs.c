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

unsigned long long strtoull_with_unit_suffixes(const char *s,char **r,unsigned int base) {
	unsigned long long res = 0;
	char *l_r = NULL;

	if (s == NULL) return 0ULL;
	res = strtoull(s,&l_r,base);

	// suffix parsing
	if (l_r && *l_r) {
		if (tolower(*l_r) == 'k')
			res <<= (uint64_t)10;	// KB
		else if (tolower(*l_r) == 'm')
			res <<= (uint64_t)20;	// MB
		else if (tolower(*l_r) == 'g')
			res <<= (uint64_t)30;	// GB
		else if (tolower(*l_r) == 't')
			res <<= (uint64_t)40;	// TB
	}

	if (r != NULL) *r = l_r;
	return res;
}

static uint8_t					make_partition = 0;
static uint8_t					allow_fat32 = 1;
static uint8_t					allow_fat16 = 1;
static uint8_t					allow_fat12 = 1;
static uint16_t					set_cluster_size = 0;
static uint8_t					allow_non_power_of_2_cluster_size = 0;
static uint8_t					allow_64kb_or_larger_clusters = 0;
static uint32_t					set_root_directory_entries = 0;
static uint32_t					root_directory_entries = 0;
static uint32_t					set_fsinfo_sector = 0;
static uint32_t					reserved_sectors = 0;
static uint8_t					set_fat_tables = 0;
static uint32_t					set_reserved_sectors = 0;
static const char*				set_volume_label = NULL;
static uint32_t					set_root_cluster = 0;
static uint32_t					set_backup_boot_sector = 0;
static uint8_t					set_boot_sector_bpb_size = 0;
static uint8_t					dont_partition_track_align = 0;
static uint32_t					backup_boot_sector = 0;

struct libmsfat_formatting_params {
	uint8_t						force_fat;
	struct libmsfat_disk_locations_and_info		base_info;
	struct libmsfat_disk_locations_and_info		final_info;
	struct chs_geometry_t				disk_geo;
	libpartmbr_sector_t				diskimage_sector;
	unsigned int					disk_bytes_per_sector;
	uint64_t					partition_offset;
	uint64_t					disk_size_bytes;
	uint64_t					partition_size;
	uint64_t					disk_sectors;
	uint32_t					volume_id;
	uint8_t						partition_type;
	uint8_t						disk_media_type_byte;
	const char*					volume_label;

	unsigned int					lba_mode:1;
	unsigned int					chs_mode:1;
	unsigned int					volume_id_set:1;
	unsigned int					partition_type_set:1;
	unsigned int					disk_size_bytes_set:1;
	unsigned int					disk_media_type_byte_set:1;
	unsigned int					disk_bytes_per_sector_set:1;
};

int libmsfat_formatting_params_set_volume_label(struct libmsfat_formatting_params *f,const char *n) {
	if (f == NULL) return -1;

	if (n != NULL) f->volume_label = n;
	else f->volume_label = "NO LABEL";
	return 0;
}

void libmsfat_formatting_params_free(struct libmsfat_formatting_params *f) {
}

int libmsfat_formatting_params_init(struct libmsfat_formatting_params *f) {
	memset(f,0,sizeof(*f));
	f->disk_bytes_per_sector = 512U;
	libmsfat_formatting_params_set_volume_label(f,NULL);
	return 0;
}

struct libmsfat_formatting_params *libmsfat_formatting_params_destroy(struct libmsfat_formatting_params *f) {
	if (f != NULL) {
		libmsfat_formatting_params_free(f);
		free(f);
	}

	return NULL;
}

struct libmsfat_formatting_params *libmsfat_formatting_params_create() {
	struct libmsfat_formatting_params *f;

	f = malloc(sizeof(struct libmsfat_formatting_params));
	if (f == NULL) return NULL;
	if (libmsfat_formatting_params_init(f)) {
		libmsfat_formatting_params_destroy(f);
		free(f);
		return NULL;
	}

	return f;
}

int libmsfat_formatting_params_set_FAT_width(struct libmsfat_formatting_params *f,unsigned int width) {
	if (f == NULL) return -1;

	if (width == 0 || width == 12 || width == 16 || width == 32)
		f->force_fat = (uint8_t)width;
	else
		return -1;

	return 0;
}

int libmsfat_formatting_params_update_disk_sectors(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;
	if (f->disk_bytes_per_sector == 0) return -1;
	f->disk_sectors = (uint64_t)f->disk_size_bytes / (uint64_t)f->disk_bytes_per_sector;
	return 0;
}

int libmsfat_formatting_params_set_media_type_byte(struct libmsfat_formatting_params *f,unsigned int b) {
	if (f == NULL) return -1;

	if (b != 0) {
		if (b < 0xF0) return -1;
		f->disk_media_type_byte_set = 1;
		f->disk_media_type_byte = (uint8_t)b;
	}
	else {
		f->disk_media_type_byte_set = 0;
		f->disk_media_type_byte = 0;
	}

	return 0;
}

int libmsfat_formatting_params_set_sector_size(struct libmsfat_formatting_params *f,unsigned int sz) {
	if (f == NULL) return -1;

	if (sz == 0) {
		f->disk_bytes_per_sector = 512U;
		f->disk_bytes_per_sector_set = 0;
	}
	else {
		if (sz < 128U || sz > 16384U || (sz % 32U) != 0U) return -1;
		f->disk_bytes_per_sector = sz;
		f->disk_bytes_per_sector_set = 1;
	}

	return 0;
}

int libmsfat_formatting_params_set_disk_size_bytes(struct libmsfat_formatting_params *f,uint64_t byte_count) {
	if (f == NULL) return -1;

	if (byte_count == (uint64_t)0ULL) {
		f->disk_size_bytes_set = 0;
		f->disk_size_bytes = 0;
	}
	else {
		f->disk_size_bytes_set = 1;
		f->disk_size_bytes = byte_count;
	}

	return 0;
}

int libmsfat_formatting_params_autofill_geometry(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;
	if (f->disk_size_bytes == 0) return 1;

	if (f->lba_mode || f->disk_size_bytes > (uint64_t)(128ULL << 20ULL)) { // 64MB
		if (f->disk_geo.heads == 0)
			f->disk_geo.heads = 16;
	}
	else {
		if (f->disk_geo.heads == 0)
			f->disk_geo.heads = 8;
	}

	if (f->lba_mode || f->disk_size_bytes > (uint64_t)(96ULL << 20ULL)) { // 96MB
		if (f->disk_geo.sectors == 0)
			f->disk_geo.sectors = 63;
	}
	else if (f->disk_size_bytes > (uint64_t)(32ULL << 20ULL)) { // 32MB
		if (f->disk_geo.sectors == 0)
			f->disk_geo.sectors = 32;
	}
	else {
		if (f->disk_geo.sectors == 0)
			f->disk_geo.sectors = 15;
	}

	return 0;
}

int libmsfat_formatting_params_auto_choose_lba_chs_from_geometry(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	if (!f->lba_mode && !f->chs_mode) {
		if (f->disk_geo.sectors >= 63 && f->disk_geo.cylinders >= 1024)
			f->lba_mode = 1;
		else
			f->chs_mode = 1;
	}

	return 0;
}

int libmsfat_formatting_params_auto_choose_lba_chs_from_disk_size(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	if (!f->lba_mode && !f->chs_mode) {
		if (f->disk_size_bytes >= (uint64_t)(8192ULL << 20ULL))
			f->lba_mode = 1;
		else
			f->chs_mode = 1;
	}

	return 0;
}

int libmsfat_formatting_params_enlarge_sectors_4GB_count_workaround(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	if (!f->disk_bytes_per_sector_set) {
		while (f->disk_sectors >= (uint64_t)0xFFFFFFF0UL) {
			f->disk_bytes_per_sector *= (uint64_t)2UL;
			f->disk_sectors = (uint64_t)f->disk_size_bytes / (uint64_t)f->disk_bytes_per_sector;
		}
	}

	return 0;
}

int libmsfat_formatting_params_update_size_from_geometry(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	f->disk_sectors = (uint64_t)f->disk_geo.heads *
		(uint64_t)f->disk_geo.sectors *
		(uint64_t)f->disk_geo.cylinders;
	f->disk_size_bytes = f->disk_sectors *
		(uint64_t)f->disk_bytes_per_sector;

	return 0;
}

int libmsfat_formatting_params_update_geometry_from_size_bios_xlat(struct libmsfat_formatting_params *f) {
	uint64_t cyl;

	if (f == NULL) return -1;
	if (f->disk_geo.heads == 0 || f->disk_geo.sectors == 0) return -1;

	/* how many cylinders? */
	cyl = f->disk_sectors / ((uint64_t)f->disk_geo.heads * (uint64_t)f->disk_geo.sectors);

	/* BIOS CHS translations to exceed 512MB limit */
	while (cyl > 1023 && f->disk_geo.heads < 128) {
		f->disk_geo.heads *= 2;
		cyl = f->disk_sectors / ((uint64_t)f->disk_geo.heads * (uint64_t)f->disk_geo.sectors);
	}
	if (cyl > 1023 && f->disk_geo.heads < 255) {
		f->disk_geo.heads = 255;
		cyl = f->disk_sectors / ((uint64_t)f->disk_geo.heads * (uint64_t)f->disk_geo.sectors);
	}

	/* final limit (16383 at IDE, 1024 at BIOS) */
	if (cyl > (uint64_t)1024UL) cyl = (uint64_t)1024UL;

	/* that's the number of cylinders! */
	f->disk_geo.cylinders = (uint16_t)cyl;

	return 0;
}

int libmsfat_formatting_params_update_geometry_from_size(struct libmsfat_formatting_params *f) {
	uint64_t cyl;

	if (f == NULL) return -1;
	if (f->disk_geo.heads == 0 || f->disk_geo.sectors == 0) return -1;

	/* how many cylinders? */
	cyl = f->disk_sectors / ((uint64_t)f->disk_geo.heads * (uint64_t)f->disk_geo.sectors);

	/* final limit (16383 at IDE, 1024 at BIOS) */
	if (cyl > (uint64_t)1024UL) cyl = (uint64_t)1024UL;

	/* that's the number of cylinders! */
	f->disk_geo.cylinders = (uint16_t)cyl;

	return 0;
}

int libmsfat_formatting_params_set_partition_offset(struct libmsfat_formatting_params *f,uint64_t byte_offset) {
	if (f == NULL) return -1;
	if (f->disk_bytes_per_sector == 0) return -1;
	f->partition_offset = byte_offset / (uint64_t)f->disk_bytes_per_sector;
	return 0;
}

int libmsfat_formatting_params_set_partition_size(struct libmsfat_formatting_params *f,uint64_t byte_offset) {
	if (f == NULL) return -1;
	if (f->disk_bytes_per_sector == 0) return -1;
	f->partition_size = byte_offset / (uint64_t)f->disk_bytes_per_sector;
	return 0;
}

int libmsfat_formatting_params_partition_autofill_and_align(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	// automatically default to a partition starting on a track boundary.
	// MS-DOS, especially 6.x and earlier, require this. MS-DOS 7 and higher
	// demand this IF the partition was made in CHS mode.
	if (f->partition_offset == (uint64_t)0UL)
		f->partition_offset = f->disk_geo.sectors;

	if (f->partition_offset >= f->disk_sectors)
		return -1;

	if (!dont_partition_track_align) {
		if (f->partition_offset != (uint64_t)0UL) {
			f->partition_offset -= f->partition_offset % (uint64_t)f->disk_geo.sectors;
			if (f->partition_offset == (uint64_t)0UL) f->partition_offset += (uint64_t)f->disk_geo.sectors;
		}
	}

	if (f->partition_size == (uint64_t)0UL && f->partition_offset != (uint64_t)0UL)
		f->partition_size = f->disk_sectors - f->partition_offset;

	if (!dont_partition_track_align) {
		uint64_t sum = f->partition_offset + f->partition_size;

		sum -= sum % (uint64_t)f->disk_geo.sectors;
		if (sum <= f->partition_offset) return -1;

		sum -= f->partition_offset;
		f->partition_size = sum;
	}

	return 0;
}

int libmsfat_formatting_params_update_total_sectors(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	if (make_partition)
		f->base_info.TotalSectors = (uint32_t)f->partition_size;
	else
		f->base_info.TotalSectors = (uint32_t)f->disk_sectors;

	return 0;
}

int libmsfat_formatting_params_auto_choose_FAT_size(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	if (f->force_fat != 0) {
		if (f->force_fat == 12 && !allow_fat12)
			return -1;
		else if (f->force_fat == 16 && !allow_fat16)
			return -1;
		else if (f->force_fat == 32 && !allow_fat32)
			return -1;

		f->base_info.FAT_size = f->force_fat;
	}
	if (f->base_info.FAT_size == 0) {
		// if FAT32 allowed, and 2GB or larger, then do FAT32
		if (allow_fat32 && ((uint64_t)f->base_info.TotalSectors * (uint64_t)f->disk_bytes_per_sector) >= ((uint64_t)(2000ULL << 20ULL)))
			f->base_info.FAT_size = 32;
		// if FAT16 allowed, and 32MB or larger, then do FAT16
		else if (allow_fat16 && ((uint64_t)f->base_info.TotalSectors * (uint64_t)f->disk_bytes_per_sector) >= ((uint64_t)(30ULL << 20ULL)))
			f->base_info.FAT_size = 16;
		// if FAT12 allowed, then do it
		else if (allow_fat12 && ((uint64_t)f->base_info.TotalSectors * (uint64_t)f->disk_bytes_per_sector) < ((uint64_t)(31ULL << 20ULL)))
			f->base_info.FAT_size = 12;
		// maybe we can do FAT32?
		else if (allow_fat32 && ((uint64_t)f->base_info.TotalSectors * (uint64_t)f->disk_bytes_per_sector) >= ((uint64_t)(200ULL << 20ULL)))
			f->base_info.FAT_size = 32;
		else if (allow_fat16)
			f->base_info.FAT_size = 16;
		else if (allow_fat12)
			f->base_info.FAT_size = 12;
		else if (allow_fat32)
			f->base_info.FAT_size = 32;
	}
	if (f->base_info.FAT_size == 0)
		return -1;

	return 0;
}

int libmsfat_formatting_params_auto_choose_FAT_tables(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	if (set_fat_tables)
		f->base_info.FAT_tables = set_fat_tables;
	else if (f->base_info.FAT_tables == 0)
		f->base_info.FAT_tables = 2;

	return 0;
}

int libmsfat_formatting_params_auto_choose_root_directory_size(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	if (f->base_info.FAT_size == 32) {
		f->base_info.RootDirectory_size = 0;
		root_directory_entries = 0; // FAT32 makes the root directory an allocation chain
	}
	else {
		if (set_root_directory_entries != 0)
			root_directory_entries = set_root_directory_entries;
		else if (f->disk_size_bytes >= (uint64_t)(100ULL << 20ULL)) // 100MB
			root_directory_entries = 512; // 16KB
		else if (f->disk_size_bytes >= (uint64_t)(60ULL << 20ULL)) // 60MB
			root_directory_entries = 256; // 8KB
		else if (f->disk_size_bytes >= (uint64_t)(32ULL << 20ULL)) // 32MB
			root_directory_entries = 128; // 4KB
		else if (f->disk_size_bytes >= (uint64_t)(26ULL << 20ULL)) // 26MB
			root_directory_entries = 64; // 2KB
		else
			root_directory_entries = 32; // 1KB

		f->base_info.RootDirectory_size = ((root_directory_entries * 32) + 511) / 512;
	}

	return 0;
}

int libmsfat_formatting_params_auto_choose_cluster_size(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	if (set_cluster_size != 0) {
		unsigned long x;

		x = ((unsigned long)set_cluster_size + ((unsigned long)f->disk_bytes_per_sector / 2UL)) / (unsigned long)f->disk_bytes_per_sector;
		if (x == 0) x = f->disk_bytes_per_sector;
		if (x > 255UL) x = 255UL;
		f->base_info.Sectors_Per_Cluster = (uint8_t)x;
	}
	else {
		uint32_t cluslimit;

		if (f->base_info.FAT_size == 32) {
			/* it's better to keep the FAT table small. try not to do one cluster per sector */
			if (f->base_info.TotalSectors >= (uint32_t)0x3FFFF5UL) {
				f->base_info.Sectors_Per_Cluster = 16;
				if (f->base_info.TotalSectors >= (uint32_t)0x00FFFFF5UL)
					cluslimit = (uint32_t)0x03FFFFF5UL;
				else
					cluslimit = (uint32_t)0x00FFFFF5UL;
			}
			else {
				cluslimit = (uint32_t)0x0007FFF5UL;
				f->base_info.Sectors_Per_Cluster = 1;
			}
		}
		else {
			f->base_info.Sectors_Per_Cluster = 1;
			if (f->base_info.FAT_size == 16)
				cluslimit = (uint32_t)0x0000FFF5UL;
			else
				cluslimit = (uint32_t)0x00000FF5UL;
		}

		while (f->base_info.Sectors_Per_Cluster < 0xFF && (f->base_info.TotalSectors / (uint32_t)f->base_info.Sectors_Per_Cluster) >= cluslimit)
			f->base_info.Sectors_Per_Cluster++;
	}

	if (!allow_non_power_of_2_cluster_size) {
		/* need to round to a power of 2 */
		if (f->base_info.Sectors_Per_Cluster >= (64+1)/*65*/)
			f->base_info.Sectors_Per_Cluster = 128;
		else if (f->base_info.Sectors_Per_Cluster >= (32+1)/*33*/)
			f->base_info.Sectors_Per_Cluster = 64;
		else if (f->base_info.Sectors_Per_Cluster >= (16+1)/*17*/)
			f->base_info.Sectors_Per_Cluster = 32;
		else if (f->base_info.Sectors_Per_Cluster >= (8+1)/*9*/)
			f->base_info.Sectors_Per_Cluster = 16;
		else if (f->base_info.Sectors_Per_Cluster >= (4+1)/*5*/)
			f->base_info.Sectors_Per_Cluster = 8;
		else if (f->base_info.Sectors_Per_Cluster >= (2+1)/*3*/)
			f->base_info.Sectors_Per_Cluster = 4;
		else if (f->base_info.Sectors_Per_Cluster >= 2)
			f->base_info.Sectors_Per_Cluster = 2;
		else
			f->base_info.Sectors_Per_Cluster = 1;
	}

	if (!allow_64kb_or_larger_clusters) {
		uint32_t sz;

		sz = (uint32_t)f->base_info.Sectors_Per_Cluster * (uint32_t)f->disk_bytes_per_sector;
		if (sz >= (uint32_t)0x10000UL) {
			fprintf(stderr,"WARNING: cluster size choice means cluster is 64KB or larger. choosing smaller cluster size.\n");

			if (allow_non_power_of_2_cluster_size)
				f->base_info.Sectors_Per_Cluster = (uint32_t)0xFFFFUL / (uint32_t)f->disk_bytes_per_sector;
			else {
				while (sz >= (uint32_t)0x10000UL && f->base_info.Sectors_Per_Cluster >= 2U) {
					f->base_info.Sectors_Per_Cluster /= 2U;
					sz = (uint32_t)f->base_info.Sectors_Per_Cluster * (uint32_t)f->disk_bytes_per_sector;
				}
			}
		}
	}

	return 0;
}

int libmsfat_formatting_params_auto_choose_reserved_sectors(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	if (f->base_info.FAT_size == 32) {
		if (set_reserved_sectors != (uint32_t)0U)
			reserved_sectors = set_reserved_sectors;
		else
			reserved_sectors = 32; // typical FAT32 value, allowing FSInfo at sector 6

		if (reserved_sectors < 3U) // BPB + backup BPB + FSInfo
			reserved_sectors = 3U;
	}
	else {
		if (set_reserved_sectors != (uint32_t)0U)
			reserved_sectors = set_reserved_sectors;
		else
			reserved_sectors = 1;
	}

	return 0;
}

int libmsfat_formatting_params_auto_choose_fat32_bpb_fsinfo_location(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	if (f->base_info.FAT_size == 32) {
		if (set_fsinfo_sector != 0)
			f->base_info.fat32.BPB_FSInfo = set_fsinfo_sector;
		else
			f->base_info.fat32.BPB_FSInfo = 6;

		if (f->base_info.fat32.BPB_FSInfo >= reserved_sectors)
			f->base_info.fat32.BPB_FSInfo = reserved_sectors - 1;
	}
	else {
		f->base_info.fat32.BPB_FSInfo = 0;
	}

	return 0;
}

int libmsfat_formatting_params_auto_choose_fat32_backup_boot_sector(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	if (f->base_info.FAT_size == 32) {
		if (set_backup_boot_sector != 0) backup_boot_sector = set_backup_boot_sector;
		else backup_boot_sector = 1;

		if (backup_boot_sector >= reserved_sectors)
			backup_boot_sector = reserved_sectors - 1;
		if (backup_boot_sector > 1 && backup_boot_sector == f->base_info.fat32.BPB_FSInfo)
			backup_boot_sector--;
		if (backup_boot_sector == f->base_info.fat32.BPB_FSInfo)
			return 1;
	}
	else {
		backup_boot_sector = 0;
	}

	return 0;
}

int libmsfat_formatting_params_compute_cluster_count(struct libmsfat_formatting_params *f) {
	uint32_t cluslimit,x,t;

	if (f == NULL) return -1;

	// initial estimate. doesn't factor in root directory, root directory, boot sector...
	// estimate will be a little large compared to the final value.
	if (f->base_info.FAT_size == 32)
		cluslimit = (uint32_t)0x0FFFFFF5UL;
	else if (f->base_info.FAT_size == 16)
		cluslimit = (uint32_t)0x0000FFF5UL;
	else
		cluslimit = (uint32_t)0x00000FF5UL;

	x = f->base_info.TotalSectors;

	if (x > (uint32_t)reserved_sectors) x -= (uint32_t)reserved_sectors;
	else x = (uint32_t)0;

	if (x > (uint32_t)f->base_info.RootDirectory_size) x -= (uint32_t)f->base_info.RootDirectory_size;
	else x = (uint32_t)0;

	x /= (uint32_t)f->base_info.Sectors_Per_Cluster;
	if (x == (uint32_t)0UL) return -1;

	/* use it to calculate size of the FAT table */
	if (f->base_info.FAT_size == 12)
		t = ((x + (uint32_t)1UL) / (uint32_t)2UL) * (uint32_t)3UL; // 1/2x * 3 = number of 24-bit doubles
	else
		t = x * (f->base_info.FAT_size / (uint32_t)8UL);
	t = (t + (uint32_t)f->disk_bytes_per_sector - (uint32_t)1UL) / (uint32_t)f->disk_bytes_per_sector;
	f->base_info.FAT_table_size = t;

	/* do it again */
	x = f->base_info.TotalSectors;

	if (x > (uint32_t)reserved_sectors) x -= (uint32_t)reserved_sectors;
	else x = (uint32_t)0;

	if (x > (uint32_t)f->base_info.RootDirectory_size) x -= (uint32_t)f->base_info.RootDirectory_size;
	else x = (uint32_t)0;

	t = (uint32_t)f->base_info.FAT_table_size * (uint32_t)f->base_info.FAT_tables;
	if (x > t) x -= t;
	else x = (uint32_t)0;

	x /= (uint32_t)f->base_info.Sectors_Per_Cluster;
	if (x == (uint32_t)0UL) return -1;

	/* clip the cluster count (and therefore the sector count) if we have to */
	if (x >= cluslimit) {
		assert(cluslimit != (uint32_t)0UL);
		x = cluslimit - (uint32_t)1UL;

		if (f->base_info.FAT_size == 12)
			t = ((x + (uint32_t)1UL) / (uint32_t)2UL) * (uint32_t)3UL; // 1/2x * 3 = number of 24-bit doubles
		else
			t = x * (f->base_info.FAT_size / (uint32_t)8UL);
		t = (t + (uint32_t)f->disk_bytes_per_sector - (uint32_t)1UL) / (uint32_t)f->disk_bytes_per_sector;
		f->base_info.FAT_table_size = t;

		f->base_info.TotalSectors =
			(x * (uint32_t)f->base_info.Sectors_Per_Cluster) +
			(uint32_t)f->base_info.FAT_table_size +
			(uint32_t)f->base_info.RootDirectory_size +
			(uint32_t)reserved_sectors;
	}

	f->base_info.Total_data_clusters = x;
	f->base_info.Total_clusters = x + (uint32_t)2UL;
	return 0;
}

int libmsfat_formatting_params_choose_partition_table(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	if (!f->partition_type_set) {
		// resides in the first 32MB. this is the only case that disregards LBA mode
		if ((f->partition_offset+f->partition_size)*((uint64_t)f->disk_bytes_per_sector) <= ((uint64_t)32UL << (uint64_t)20UL)) {
			if (f->base_info.FAT_size == 16)
				f->partition_type = 0x04; // FAT16 with less than 65536 sectors
			else
				f->partition_type = 0x01; // FAT12 as primary partition in the first 32MB
		}
		// resides in the first 8GB.
		else if ((f->partition_offset+f->partition_size)*((uint64_t)f->disk_bytes_per_sector) <= ((uint64_t)8192UL << (uint64_t)20UL)) {
			if (f->base_info.FAT_size == 32)
				f->partition_type = f->lba_mode ? 0x0C : 0x0B;	// FAT32 with (lba_mode ? LBA : CHS) mode
			else if (f->base_info.FAT_size == 16)
				f->partition_type = f->lba_mode ? 0x0E : 0x06;	// FAT16B with (lba_mode ? LBA : CHS) mode
			else
				f->partition_type = 0x01; // FAT12
		}
		// resides past 8GB. this is the only case that disregards CHS mode
		else {
			if (f->base_info.FAT_size == 32)
				f->partition_type = 0x0C;	// FAT32 with LBA mode
			else if (f->base_info.FAT_size == 16)
				f->partition_type = 0x0E;	// FAT16B with LBA mode
			else
				f->partition_type = 0x01; // FAT12
		}
	}

	return 0;
}

int libmsfat_formatting_params_set_partition_type(struct libmsfat_formatting_params *f,unsigned int t) {
	if (f == NULL) return -1;
	f->partition_type = (uint8_t)t;
	f->partition_type_set = 1;
	return 0;
}

int libmsfat_formatting_params_auto_choose_media_type_byte(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	if (!f->disk_media_type_byte_set)
		f->disk_media_type_byte = libmsfat_guess_from_geometry(&f->disk_geo);

	if (f->disk_media_type_byte < 0xF0) return -1;
	return 0;
}

int libmsfat_formatting_params_set_volume_id(struct libmsfat_formatting_params *f,uint32_t vol_id) {
	if (f == NULL) return -1;
	f->volume_id = vol_id;
	f->volume_id_set = 1;
	return 0;
}

int libmsfat_formatting_params_auto_choose_volume_id(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	if (!f->volume_id_set)
		f->volume_id = (uint32_t)time(NULL);

	return 0;
}

int main(int argc,char **argv) {
	struct libmsfat_formatting_params *fmtparam;
	struct libmsfat_context_t *msfatctx = NULL;
	const char *s_partition_offset = NULL;
	const char *s_partition_size = NULL;
	const char *s_geometry = NULL;
	const char *s_image = NULL;
	int i,fd,dfd;

	if ((fmtparam=libmsfat_formatting_params_create()) == NULL) {
		fprintf(stderr,"Cannot alloc format params\n");
		return 1;
	}

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
				if (libmsfat_formatting_params_set_disk_size_bytes(fmtparam,(uint64_t)strtoull_with_unit_suffixes(argv[i++],NULL,0))) {
					fprintf(stderr,"--size: Size setting failed\n");
					return 1;
				}
			}
			else if (!strcmp(a,"sectorsize")) {
				if (libmsfat_formatting_params_set_sector_size(fmtparam,(unsigned int)strtoull_with_unit_suffixes(argv[i++],NULL,0))) {
					fprintf(stderr,"--fat: Invalid FAT bit width\n");
					return 1;
				}
			}
			else if (!strcmp(a,"media-type")) {
				if (libmsfat_formatting_params_set_media_type_byte(fmtparam,(unsigned int)strtoul(argv[i++],NULL,0))) {
					fprintf(stderr,"--fat: Invalid FAT bit width\n");
					return 1;
				}
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
				if (libmsfat_formatting_params_set_partition_type(fmtparam,(unsigned int)strtoull_with_unit_suffixes(argv[i++],NULL,0))) {
					fprintf(stderr,"--fat: Invalid FAT bit width\n");
					return 1;
				}
			}
			else if (!strcmp(a,"lba")) {
				fmtparam->lba_mode = 1;
			}
			else if (!strcmp(a,"chs")) {
				fmtparam->chs_mode = 1;
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
				if (libmsfat_formatting_params_set_FAT_width(fmtparam,(unsigned int)strtoul(argv[i++],NULL,0))) {
					fprintf(stderr,"--fat: Invalid FAT bit width\n");
					return 1;
				}
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
			else if (!strcmp(a,"volume-id")) {
				if (libmsfat_formatting_params_set_volume_id(fmtparam,(unsigned int)strtoul(argv[i++],NULL,0))) {
					fprintf(stderr,"--volume-id: failed\n");
					return 1;
				}
			}
			else if (!strcmp(a,"volume-label")) {
				set_volume_label = argv[i++];
			}
			else if (!strcmp(a,"root-cluster")) {
				set_root_cluster = (uint32_t)strtoul(argv[i++],NULL,0);
			}
			else if (!strcmp(a,"backup-boot-sector")) {
				set_backup_boot_sector = (uint32_t)strtoul(argv[i++],NULL,0);
			}
			else if (!strcmp(a,"bpb-size")) {
				set_boot_sector_bpb_size = (uint8_t)strtoul(argv[i++],NULL,0);
			}
			else if (!strcmp(a,"no-partition-track-align")) {
				dont_partition_track_align = 1;
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

	if (s_image == NULL || (s_geometry == NULL && !fmtparam->disk_size_bytes_set)) {
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
		fprintf(stderr,"--no-partition-track-align  Don't align partitions to C/H/S track boundaries\n");
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
		fprintf(stderr,"--volume-id <x>             Set volume id to <x> (X is a decimal or hex value 32-bit wide)\n");
		fprintf(stderr,"--volume-label <x>          Set volume label to string <x> (max 11 chars)\n");
		fprintf(stderr,"--root-cluster <x>          Root directory starts at cluster <x> (FAT32 only)\n");
		fprintf(stderr,"--backup-boot-sector <x>    FAT32 backup boot sector location\n");
		fprintf(stderr,"--bpb-size <x>              Set the size of the BPB (!!)\n");
		return 1;
	}

	if (s_geometry != NULL && int13cnv_parse_chs_geometry(&fmtparam->disk_geo,s_geometry)) {
		fprintf(stderr,"Failed to parse geometry\n");
		return 1;
	}

	if (fmtparam->disk_size_bytes_set && libmsfat_formatting_params_autofill_geometry(fmtparam)) {
		fprintf(stderr,"Failed to auto-fill geometry given disk size\n");
		return 1;
	}

	if (fmtparam->disk_size_bytes_set) {
		if (libmsfat_formatting_params_update_disk_sectors(fmtparam)) return 1;
		if (libmsfat_formatting_params_auto_choose_lba_chs_from_disk_size(fmtparam)) return 1;
		if (libmsfat_formatting_params_enlarge_sectors_4GB_count_workaround(fmtparam)) return 1;
		if (libmsfat_formatting_params_update_geometry_from_size_bios_xlat(fmtparam)) return 1;
		if (fmtparam->chs_mode && libmsfat_formatting_params_update_size_from_geometry(fmtparam)) return 1;
	}
	else {
		if (libmsfat_formatting_params_update_size_from_geometry(fmtparam)) return 1;
		if (libmsfat_formatting_params_auto_choose_lba_chs_from_geometry(fmtparam)) return 1;
	}

	if (make_partition) {
		if (s_partition_offset != NULL && libmsfat_formatting_params_set_partition_offset(fmtparam,(uint64_t)strtoull_with_unit_suffixes(s_partition_offset,NULL,0)))
			return 1;
		if (s_partition_size != NULL && libmsfat_formatting_params_set_partition_size(fmtparam,(uint64_t)strtoull_with_unit_suffixes(s_partition_size,NULL,0)))
			return 1;
		if (libmsfat_formatting_params_partition_autofill_and_align(fmtparam))
			return 1;
	}

	if (libmsfat_formatting_params_update_total_sectors(fmtparam)) return 1;
	if (libmsfat_formatting_params_auto_choose_FAT_size(fmtparam)) return 1;
	if (libmsfat_formatting_params_auto_choose_FAT_tables(fmtparam)) return 1;
	if (libmsfat_formatting_params_auto_choose_cluster_size(fmtparam)) return 1;
	if (libmsfat_formatting_params_auto_choose_root_directory_size(fmtparam)) return 1;
	if (libmsfat_formatting_params_auto_choose_reserved_sectors(fmtparam)) return 1;
	if (libmsfat_formatting_params_auto_choose_fat32_bpb_fsinfo_location(fmtparam)) return 1;
	if (libmsfat_formatting_params_auto_choose_fat32_backup_boot_sector(fmtparam)) return 1;
	if (libmsfat_formatting_params_compute_cluster_count(fmtparam)) return 1;
	if (make_partition && libmsfat_formatting_params_choose_partition_table(fmtparam)) return 1;
	if (libmsfat_formatting_params_auto_choose_media_type_byte(fmtparam)) return 1;
	if (libmsfat_formatting_params_set_volume_label(fmtparam,set_volume_label)) return 1;
	if (libmsfat_formatting_params_auto_choose_volume_id(fmtparam)) return 1;

	assert(fmtparam->lba_mode || fmtparam->chs_mode);
	printf("Formatting: %llu sectors x %u bytes per sector = %llu bytes (C/H/S %u/%u/%u) media type 0x%02x %s\n",
		(unsigned long long)fmtparam->disk_sectors,
		(unsigned int)fmtparam->disk_bytes_per_sector,
		(unsigned long long)fmtparam->disk_sectors * (unsigned long long)fmtparam->disk_bytes_per_sector,
		(unsigned int)fmtparam->disk_geo.cylinders,
		(unsigned int)fmtparam->disk_geo.heads,
		(unsigned int)fmtparam->disk_geo.sectors,
		(unsigned int)fmtparam->disk_media_type_byte,
		fmtparam->lba_mode ? "LBA" : "CHS");

	if (make_partition) {
		printf("   Partition: type=0x%02x sectors %llu-%llu\n",
			(unsigned int)fmtparam->partition_type,
			(unsigned long long)fmtparam->partition_offset,
			(unsigned long long)(fmtparam->partition_offset + fmtparam->partition_size - (uint64_t)1UL));

		if (fmtparam->chs_mode && (fmtparam->partition_offset % (uint64_t)fmtparam->disk_geo.sectors) != 0)
			printf("    WARNING: Partition does not start on track boundary (CHS mode warning). MS-DOS may have issues with it.\n");
		if (fmtparam->chs_mode && ((fmtparam->partition_offset + fmtparam->partition_size) % (uint64_t)fmtparam->disk_geo.sectors) != 0)
			printf("    WARNING: Partition does not end on track boundary (CHS mode warning). MS-DOS may have issues with it.\n");

		if (fmtparam->partition_offset == 0 || fmtparam->partition_size == 0) return 1;
	}

	printf("   FAT filesystem FAT%u. %lu x %lu (%lu bytes) per cluster. %lu sectors => %lu clusters (%lu)\n",
		fmtparam->base_info.FAT_size,
		(unsigned long)fmtparam->base_info.Sectors_Per_Cluster,
		(unsigned long)fmtparam->disk_bytes_per_sector,
		(unsigned long)fmtparam->base_info.Sectors_Per_Cluster * (unsigned long)fmtparam->disk_bytes_per_sector,
		(unsigned long)fmtparam->base_info.TotalSectors,
		(unsigned long)fmtparam->base_info.Total_data_clusters,
		(unsigned long)fmtparam->base_info.Total_clusters);
	printf("   Reserved sectors: %lu\n",
		(unsigned long)reserved_sectors);
	printf("   Root directory entries: %lu (%lu sectors)\n",
		(unsigned long)root_directory_entries,
		(unsigned long)fmtparam->base_info.RootDirectory_size);
	printf("   %u FAT tables: %lu sectors per table\n",
		(unsigned int)fmtparam->base_info.FAT_tables,
		(unsigned long)fmtparam->base_info.FAT_table_size);
	printf("   Volume ID: 0x%08lx\n",
		(unsigned long)fmtparam->volume_id);

	if (fmtparam->base_info.FAT_size == 32)
		printf("   FAT32 FSInfo sector: %lu\n",
			(unsigned long)fmtparam->base_info.fat32.BPB_FSInfo);

	if (fmtparam->base_info.FAT_size == 32 && fmtparam->base_info.fat32.BPB_FSInfo == 0) {
		fprintf(stderr,"FAT32 requires a sector set aside for FSInfo\n");
		return 1;
	}

	if (fmtparam->base_info.FAT_size == 0) {
		fprintf(stderr,"Unable to decide on a FAT bit width\n");
		return 1;
	}
	if (fmtparam->base_info.Sectors_Per_Cluster == 0) {
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
	if (ftruncate(fd,(off_t)fmtparam->disk_size_bytes)) {
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

	if (make_partition)
		msfatctx->partition_byte_offset = (uint64_t)fmtparam->partition_offset * (uint64_t)fmtparam->disk_bytes_per_sector;

	if (make_partition) {
		struct libpartmbr_state_t diskimage_state;
		struct libpartmbr_entry_t ent;
		struct chs_geometry_t chs;

		libpartmbr_state_zero(&diskimage_state);

		if (libpartmbr_create_partition_table(fmtparam->diskimage_sector,&diskimage_state)) {
			fprintf(stderr,"Failed to create partition table sector\n");
			return 1;
		}

		ent.partition_type = fmtparam->partition_type;
		ent.first_lba_sector = (uint32_t)fmtparam->partition_offset;
		ent.number_lba_sectors = (uint32_t)fmtparam->partition_size;
		if (int13cnv_lba_to_chs(&chs,&fmtparam->disk_geo,(uint32_t)fmtparam->partition_offset)) {
			fprintf(stderr,"Failed to convert start LBA -> CHS\n");
			return 1;
		}
		int13cnv_chs_int13_cliprange(&chs,&fmtparam->disk_geo);
		if (int13cnv_chs_to_int13(&ent.first_chs_sector,&chs)) {
			fprintf(stderr,"Failed to convert start CHS -> INT13\n");
			return 1;
		}

		if (int13cnv_lba_to_chs(&chs,&fmtparam->disk_geo,(uint32_t)fmtparam->partition_offset + (uint32_t)fmtparam->partition_size - (uint32_t)1UL)) {
			fprintf(stderr,"Failed to convert end LBA -> CHS\n");
			return 1;
		}
		int13cnv_chs_int13_cliprange(&chs,&fmtparam->disk_geo);
		if (int13cnv_chs_to_int13(&ent.last_chs_sector,&chs)) {
			fprintf(stderr,"Failed to convert end CHS -> INT13\n");
			return 1;
		}

		if (libpartmbr_write_entry(fmtparam->diskimage_sector,&ent,&diskimage_state,0)) {
			fprintf(stderr,"Unable to write entry\n");
			return 1;
		}

		if (lseek(fd,0,SEEK_SET) != 0 || write(fd,fmtparam->diskimage_sector,LIBPARTMBR_SECTOR_SIZE) != LIBPARTMBR_SECTOR_SIZE) {
			fprintf(stderr,"Failed to write MBR back\n");
			return 1;
		}
	}

	/* generate the boot sector! */
	{
		uint64_t offset;
		unsigned char sector[512];
		struct libmsfat_bootsector *bs = (struct libmsfat_bootsector*)sector;
		struct libmsfat_file_io_ctx_t *fioctx_parent = NULL;
		struct libmsfat_file_io_ctx_t *fioctx = NULL;
		unsigned int bs_sz;

		if (make_partition)
			offset = fmtparam->partition_offset;
		else
			offset = 0;
		offset *= (uint64_t)fmtparam->disk_bytes_per_sector;

		// TODO: options to emulate various DOS versions of the BPB
		if (set_boot_sector_bpb_size != 0)
			bs_sz = set_boot_sector_bpb_size;
		else if (fmtparam->base_info.FAT_size == 32)
			bs_sz = 90;	// MS-DOS 7.x / Windows 9x FAT32
		else
			bs_sz = 62;	// MS-DOS 6.x and earlier, FAT12/FAT16

		if (bs_sz < 40) {
			fprintf(stderr,"BPB too small\n");
			return 1;
		}
		if (bs_sz < 90 && fmtparam->base_info.FAT_size == 32) {
			fprintf(stderr,"BPB too small for FAT32\n");
			return 1;
		}

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
		bs->BPB_common.BPB_BytsPerSec = htole16(fmtparam->disk_bytes_per_sector);
		bs->BPB_common.BPB_SecPerClus = fmtparam->base_info.Sectors_Per_Cluster;
		bs->BPB_common.BPB_RsvdSecCnt = htole16(reserved_sectors);
		bs->BPB_common.BPB_NumFATs = fmtparam->base_info.FAT_tables;
		if (fmtparam->base_info.FAT_size == 32)
			bs->BPB_common.BPB_RootEntCnt = 0;
		else
			bs->BPB_common.BPB_RootEntCnt = htole16(root_directory_entries);
		if (fmtparam->base_info.TotalSectors >= (uint32_t)65536 || fmtparam->base_info.FAT_size == 32)
			bs->BPB_common.BPB_TotSec16 = 0;
		else
			bs->BPB_common.BPB_TotSec16 = (uint16_t)htole16(fmtparam->base_info.TotalSectors);
		bs->BPB_common.BPB_Media = fmtparam->disk_media_type_byte;
		if (fmtparam->base_info.FAT_size == 32)
			bs->BPB_common.BPB_FATSz16 = 0;
		else
			bs->BPB_common.BPB_FATSz16 = htole16(fmtparam->base_info.FAT_table_size);
		bs->BPB_common.BPB_SecPerTrk = fmtparam->disk_geo.sectors;
		bs->BPB_common.BPB_NumHeads = fmtparam->disk_geo.heads;
		bs->BPB_common.BPB_TotSec32 = htole32(fmtparam->base_info.TotalSectors);

		if (fmtparam->base_info.FAT_size == 32) {
			bs->at36.BPB_FAT32.BPB_FATSz32 = htole32(fmtparam->base_info.FAT_table_size);
			bs->at36.BPB_FAT32.BPB_ExtFlags = 0;
			bs->at36.BPB_FAT32.BPB_FSVer = 0;
			if (set_root_cluster != 0)
				bs->at36.BPB_FAT32.BPB_RootClus = htole32(set_root_cluster);
			else
				bs->at36.BPB_FAT32.BPB_RootClus = htole32(2);
			bs->at36.BPB_FAT32.BPB_FSInfo = htole16(fmtparam->base_info.fat32.BPB_FSInfo);
			bs->at36.BPB_FAT32.BPB_BkBootSec = htole16(backup_boot_sector);
			bs->at36.BPB_FAT32.BS_DrvNum = (make_partition ? 0x80 : 0x00);
			bs->at36.BPB_FAT32.BS_BootSig = 0x29;
			bs->at36.BPB_FAT32.BS_VolID = htole32(fmtparam->volume_id);

			{
				const char *s = fmtparam->volume_label;
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
			bs->at36.BPB_FAT.BS_VolID = htole32(fmtparam->volume_id);

			{
				const char *s = fmtparam->volume_label;
				uint8_t *d = bs->at36.BPB_FAT.BS_VolLab;
				uint8_t *df = d + 11;

				while (*s && d < df) *d++ = *s++;
				while (d < df) *d++ = ' ';
			}

			if (fmtparam->base_info.FAT_size == 16)
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
		if (fmtparam->base_info.FAT_size == 32) {
			if (make_partition)
				offset = fmtparam->partition_offset;
			else
				offset = 0;

			offset += (uint64_t)le16toh(bs->at36.BPB_FAT32.BPB_BkBootSec);
			offset *= (uint64_t)fmtparam->disk_bytes_per_sector;

			if (lseek(fd,(off_t)offset,SEEK_SET) != (off_t)offset || write(fd,sector,512) != 512) {
				fprintf(stderr,"Failed to write BPB back\n");
				return 1;
			}
		}

		// FSInfo
		if (fmtparam->base_info.FAT_size == 32) {
			struct libmsfat_fat32_fsinfo_t fsinfo;

			if (make_partition)
				offset = fmtparam->partition_offset;
			else
				offset = 0;

			offset += (uint64_t)le16toh(bs->at36.BPB_FAT32.BPB_FSInfo);
			offset *= (uint64_t)fmtparam->disk_bytes_per_sector;

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

		if (libmsfat_bs_compute_disk_locations(&fmtparam->final_info,bs)) {
			printf("Unable to locate disk locations.\n");
			return 1;
		}
		if (libmsfat_context_set_fat_info(msfatctx,&fmtparam->final_info)) {
			fprintf(stderr,"msfat library rejected disk location info\n");
			return 1;
		}

		printf("Disk location and FAT info:\n");
		printf("    FAT format:        FAT%u\n",
			fmtparam->final_info.FAT_size);
		printf("    FAT tables:        %u\n",
			fmtparam->final_info.FAT_tables);
		printf("    FAT table size:    %lu sectors\n",
			(unsigned long)fmtparam->final_info.FAT_table_size);
		printf("    FAT offset:        %lu sectors\n",
			(unsigned long)fmtparam->final_info.FAT_offset);
		printf("    Root directory:    %lu sectors\n",
			(unsigned long)fmtparam->final_info.RootDirectory_offset);
		printf("    Root dir size:     %lu sectors\n",
			(unsigned long)fmtparam->final_info.RootDirectory_size);
		printf("    Data offset:       %lu sectors\n",
			(unsigned long)fmtparam->final_info.Data_offset);
		printf("    Data size:         %lu sectors\n",
			(unsigned long)fmtparam->final_info.Data_size);
		printf("    Sectors/cluster:   %u\n",
			(unsigned int)fmtparam->final_info.Sectors_Per_Cluster);
		printf("    Bytes per sector:  %u\n",
			(unsigned int)fmtparam->final_info.BytesPerSector);
		printf("    Total clusters:    %lu\n",
			(unsigned long)fmtparam->final_info.Total_clusters);
		printf("    Total data clusts: %lu\n",
			(unsigned long)fmtparam->final_info.Total_data_clusters);
		printf("    Max clusters psbl: %lu\n",
			(unsigned long)fmtparam->final_info.Max_possible_clusters);
		printf("    Max data clus psbl:%lu\n",
			(unsigned long)fmtparam->final_info.Max_possible_data_clusters);
		printf("    Total sectors:     %lu sectors\n",
			(unsigned long)fmtparam->final_info.TotalSectors);
		if (fmtparam->final_info.FAT_size >= 32) {
			printf("    FAT32 FSInfo:      %lu sector\n",
				(unsigned long)fmtparam->final_info.fat32.BPB_FSInfo);
			printf("    Root dir cluster:  %lu\n",
				(unsigned long)fmtparam->final_info.fat32.RootDirectory_cluster);
		}

		for (i=0;i < fmtparam->final_info.FAT_tables;i++) {
			if (libmsfat_context_write_FAT(msfatctx,(uint32_t)0xFFFFFF00UL + (uint32_t)fmtparam->disk_media_type_byte,libmsfat_CLUSTER_0_MEDIA_TYPE,i)) {
				fprintf(stderr,"Cannot write cluster 1\n");
				return 1;
			}
			if (libmsfat_context_write_FAT(msfatctx,(uint32_t)0xFFFFFFFFUL,libmsfat_CLUSTER_1_DIRTY_FLAGS,i)) {
				fprintf(stderr,"Cannot write cluster 1\n");
				return 1;
			}
		}

		if (fmtparam->final_info.FAT_size == 32) {
			/* remember the root cluster we chose? we need to mark it allocated, and
			 * then zero the cluster out */
			libmsfat_cluster_t rootclus = (libmsfat_cluster_t)le32toh(bs->at36.BPB_FAT32.BPB_RootClus);

			if (rootclus < (libmsfat_cluster_t)2UL) {
				fprintf(stderr,"Invalid root cluster\n");
				return 1;
			}
			for (i=0;i < fmtparam->final_info.FAT_tables;i++) {
				if (libmsfat_context_write_FAT(msfatctx,(uint32_t)0xFFFFFFFFUL,rootclus,i)) {
					fprintf(stderr,"Cannot write cluster %lu, to mark as EOC\n",(unsigned long)rootclus);
					return 1;
				}
			}
			// make sure the cluster corresponding to the root dir is zeroed out
			if (libmsfat_file_io_ctx_zero_cluster(rootclus,msfatctx))
				fprintf(stderr,"WARNING: failed to zero root cluster\n");
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

		if (libmsfat_file_io_ctx_assign_root_directory_with_parent(fioctx,fioctx_parent,msfatctx)) {
			fprintf(stderr,"Cannot assign root dir\n");
			return 1;
		}

		if (*fmtparam->volume_label != 0) {
			struct libmsfat_dirent_t dirent;

			memset(&dirent,0,sizeof(dirent));

			{
				const char *s = fmtparam->volume_label;
				char *d = dirent.a.n.DIR_Name;
				char *df = d + 11;

				while (*s && d < df) *d++ = *s++;
				while (d < df) *d++ = ' ';
			}
			dirent.a.n.DIR_Attr = libmsfat_DIR_ATTR_VOLUME_ID;

			if (libmsfat_file_io_ctx_write(fioctx,msfatctx,&dirent,sizeof(dirent)) != sizeof(dirent)) {
				fprintf(stderr,"Cannot write root dir volume label\n");
				return 1;
			}
		}

		fioctx_parent = libmsfat_file_io_ctx_destroy(fioctx_parent);
		fioctx = libmsfat_file_io_ctx_destroy(fioctx);
	}

	fmtparam = libmsfat_formatting_params_destroy(fmtparam);
	msfatctx = libmsfat_context_destroy(msfatctx);
	close(fd);
	fd = -1;
	return 0;
}

