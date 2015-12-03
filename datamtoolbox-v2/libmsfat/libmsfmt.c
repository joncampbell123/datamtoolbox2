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
#include <datamtoolbox-v2/libmsfat/libmsfmt.h>

int libmsfat_formatting_params_set_volume_label(struct libmsfat_formatting_params *f,const char *n) {
	if (f == NULL) return -1;

	if (f->volume_label != NULL) {
		free(f->volume_label);
		f->volume_label = NULL;
	}

	if (n != NULL) f->volume_label = strdup(n);
	else f->volume_label = strdup("NO LABEL");
	return 0;
}

void libmsfat_formatting_params_free(struct libmsfat_formatting_params *f) {
	if (f->volume_label != NULL) {
		free(f->volume_label);
		f->volume_label = NULL;
	}
}

int libmsfat_formatting_params_init(struct libmsfat_formatting_params *f) {
	memset(f,0,sizeof(*f));
	f->disk_bytes_per_sector = 512U;
	libmsfat_formatting_params_set_volume_label(f,NULL);
	f->allow_fat32 = 1;
	f->allow_fat16 = 1;
	f->allow_fat12 = 1;
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

	if (!f->dont_partition_track_align) {
		if (f->partition_offset != (uint64_t)0UL) {
			f->partition_offset -= f->partition_offset % (uint64_t)f->disk_geo.sectors;
			if (f->partition_offset == (uint64_t)0UL) f->partition_offset += (uint64_t)f->disk_geo.sectors;
		}
	}

	if (f->partition_size == (uint64_t)0UL && f->partition_offset != (uint64_t)0UL)
		f->partition_size = f->disk_sectors - f->partition_offset;

	if (!f->dont_partition_track_align) {
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

	if (f->make_partition)
		f->base_info.TotalSectors = (uint32_t)f->partition_size;
	else
		f->base_info.TotalSectors = (uint32_t)f->disk_sectors;

	return 0;
}

int libmsfat_formatting_params_auto_choose_FAT_size(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	if (f->force_fat != 0) {
		if (f->force_fat == 12 && !f->allow_fat12)
			return -1;
		else if (f->force_fat == 16 && !f->allow_fat16)
			return -1;
		else if (f->force_fat == 32 && !f->allow_fat32)
			return -1;

		f->base_info.FAT_size = f->force_fat;
	}
	if (f->base_info.FAT_size == 0) {
		// if FAT32 allowed, and 2GB or larger, then do FAT32
		if (f->allow_fat32 && ((uint64_t)f->base_info.TotalSectors * (uint64_t)f->disk_bytes_per_sector) >= ((uint64_t)(2000ULL << 20ULL)))
			f->base_info.FAT_size = 32;
		// if FAT16 allowed, and 32MB or larger, then do FAT16
		else if (f->allow_fat16 && ((uint64_t)f->base_info.TotalSectors * (uint64_t)f->disk_bytes_per_sector) >= ((uint64_t)(30ULL << 20ULL)))
			f->base_info.FAT_size = 16;
		// if FAT12 allowed, then do it
		else if (f->allow_fat12 && ((uint64_t)f->base_info.TotalSectors * (uint64_t)f->disk_bytes_per_sector) < ((uint64_t)(31ULL << 20ULL)))
			f->base_info.FAT_size = 12;
		// maybe we can do FAT32?
		else if (f->allow_fat32 && ((uint64_t)f->base_info.TotalSectors * (uint64_t)f->disk_bytes_per_sector) >= ((uint64_t)(200ULL << 20ULL)))
			f->base_info.FAT_size = 32;
		else if (f->allow_fat16)
			f->base_info.FAT_size = 16;
		else if (f->allow_fat12)
			f->base_info.FAT_size = 12;
		else if (f->allow_fat32)
			f->base_info.FAT_size = 32;
	}
	if (f->base_info.FAT_size == 0)
		return -1;

	return 0;
}

int libmsfat_formatting_params_auto_choose_FAT_tables(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	if (f->set_fat_tables)
		f->base_info.FAT_tables = f->set_fat_tables;
	else if (f->base_info.FAT_tables == 0)
		f->base_info.FAT_tables = 2;

	return 0;
}

int libmsfat_formatting_params_auto_choose_root_directory_size(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	if (f->base_info.FAT_size == 32) {
		f->base_info.RootDirectory_size = 0;
		f->root_directory_entries = 0; // FAT32 makes the root directory an allocation chain
	}
	else {
		if (f->set_root_directory_entries != 0)
			f->root_directory_entries = f->set_root_directory_entries;
		else if (f->disk_size_bytes >= (uint64_t)(100ULL << 20ULL)) // 100MB
			f->root_directory_entries = 512; // 16KB
		else if (f->disk_size_bytes >= (uint64_t)(60ULL << 20ULL)) // 60MB
			f->root_directory_entries = 256; // 8KB
		else if (f->disk_size_bytes >= (uint64_t)(32ULL << 20ULL)) // 32MB
			f->root_directory_entries = 128; // 4KB
		else if (f->disk_size_bytes >= (uint64_t)(26ULL << 20ULL)) // 26MB
			f->root_directory_entries = 64; // 2KB
		else
			f->root_directory_entries = 32; // 1KB

		f->base_info.RootDirectory_size = ((f->root_directory_entries * 32) + 511) / 512;
	}

	return 0;
}

int libmsfat_formatting_params_auto_choose_cluster_size(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	if (f->cluster_size != 0) {
		unsigned long x;

		x = ((unsigned long)f->cluster_size + ((unsigned long)f->disk_bytes_per_sector / 2UL)) / (unsigned long)f->disk_bytes_per_sector;
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

	if (!f->allow_non_power_of_2_cluster_size) {
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

	if (!f->allow_64kb_or_larger_clusters) {
		uint32_t sz;

		sz = (uint32_t)f->base_info.Sectors_Per_Cluster * (uint32_t)f->disk_bytes_per_sector;
		if (sz >= (uint32_t)0x10000UL) {
			fprintf(stderr,"WARNING: cluster size choice means cluster is 64KB or larger. choosing smaller cluster size.\n");

			if (f->allow_non_power_of_2_cluster_size)
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
		if (f->set_reserved_sectors != (uint32_t)0U)
			f->reserved_sectors = f->set_reserved_sectors;
		else
			f->reserved_sectors = 32; // typical FAT32 value, allowing FSInfo at sector 6

		if (f->reserved_sectors < 3U) // BPB + backup BPB + FSInfo
			f->reserved_sectors = 3U;
	}
	else {
		if (f->set_reserved_sectors != (uint32_t)0U)
			f->reserved_sectors = f->set_reserved_sectors;
		else
			f->reserved_sectors = 1;
	}

	return 0;
}

int libmsfat_formatting_params_auto_choose_fat32_bpb_fsinfo_location(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	if (f->base_info.FAT_size == 32) {
		if (f->set_fsinfo_sector != 0)
			f->base_info.fat32.BPB_FSInfo = f->set_fsinfo_sector;
		else
			f->base_info.fat32.BPB_FSInfo = 6;

		if (f->base_info.fat32.BPB_FSInfo >= f->reserved_sectors)
			f->base_info.fat32.BPB_FSInfo = f->reserved_sectors - 1;
	}
	else {
		f->base_info.fat32.BPB_FSInfo = 0;
	}

	return 0;
}

int libmsfat_formatting_params_auto_choose_fat32_backup_boot_sector(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	if (f->base_info.FAT_size == 32) {
		if (!f->backup_boot_sector_set) f->backup_boot_sector = 1;

		if (f->backup_boot_sector >= f->reserved_sectors)
			f->backup_boot_sector = f->reserved_sectors - 1;
		if (f->backup_boot_sector > 1 && f->backup_boot_sector == f->base_info.fat32.BPB_FSInfo)
			f->backup_boot_sector--;
		if (f->backup_boot_sector == f->base_info.fat32.BPB_FSInfo)
			return 1;
	}
	else {
		f->backup_boot_sector = 0;
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

	if (x > (uint32_t)f->reserved_sectors) x -= (uint32_t)f->reserved_sectors;
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

	if (x > (uint32_t)f->reserved_sectors) x -= (uint32_t)f->reserved_sectors;
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
			(uint32_t)f->reserved_sectors;
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

int libmsfat_formatting_params_set_backup_boot_sector(struct libmsfat_formatting_params *f,uint32_t sector) {
	if (f == NULL) return -1;
	f->backup_boot_sector = sector;
	f->backup_boot_sector_set = 1;
	return 0;
}

int libmsfat_formatting_params_set_root_cluster(struct libmsfat_formatting_params *f,uint32_t cluster) {
	if (f == NULL) return -1;
	f->root_cluster = cluster;
	f->root_cluster_set = 1;
	return 0;
}

int libmsfat_formatting_params_auto_choose_root_cluster(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;
	if (!f->root_cluster_set) f->root_cluster = 2;
	return 0;
}

int libmsfat_formatting_params_auto_choose_volume_id(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	if (!f->volume_id_set)
		f->volume_id = (uint32_t)time(NULL);

	return 0;
}

int libmsfat_formatting_params_is_valid(struct libmsfat_formatting_params *f) {
	if (f == NULL) return -1;

	if (f->base_info.FAT_size == 32 && f->base_info.fat32.BPB_FSInfo == 0)
		return -1;
	if (f->base_info.FAT_size == 0)
		return -1;
	if (f->base_info.Sectors_Per_Cluster == 0)
		return -1;

	return 0;
}

int libmsfat_formatting_params_create_partition_table_and_write_entry(struct libmsfat_formatting_params *f,struct libmsfat_context_t *msfatctx) {
	struct libpartmbr_state_t diskimage_state;
	struct libpartmbr_entry_t ent;
	struct chs_geometry_t chs;

	if (f == NULL || msfatctx == NULL)
		return -1;
	if (msfatctx->write == NULL)
		return -1;

	memset(f->diskimage_sector,0,sizeof(f->diskimage_sector));
	libpartmbr_state_zero(&diskimage_state);
	if (libpartmbr_create_partition_table(f->diskimage_sector,&diskimage_state))
		return -1;

	ent.partition_type = f->partition_type;
	ent.first_lba_sector = (uint32_t)f->partition_offset;
	ent.number_lba_sectors = (uint32_t)f->partition_size;
	if (int13cnv_lba_to_chs(&chs,&f->disk_geo,(uint32_t)f->partition_offset))
		return -1;

	int13cnv_chs_int13_cliprange(&chs,&f->disk_geo);
	if (int13cnv_chs_to_int13(&ent.first_chs_sector,&chs))
		return -1;

	if (int13cnv_lba_to_chs(&chs,&f->disk_geo,(uint32_t)f->partition_offset + (uint32_t)f->partition_size - (uint32_t)1UL))
		return -1;

	int13cnv_chs_int13_cliprange(&chs,&f->disk_geo);
	if (int13cnv_chs_to_int13(&ent.last_chs_sector,&chs))
		return -1;

	if (libpartmbr_write_entry(f->diskimage_sector,&ent,&diskimage_state,0))
		return -1;

	if (msfatctx->write(msfatctx,f->diskimage_sector,0,LIBPARTMBR_SECTOR_SIZE))
		return -1;

	return 0;
}

unsigned int libmsfat_formatting_params_get_bpb_size(struct libmsfat_formatting_params *f) {
	unsigned int bs_sz;

	if (f == NULL) return 0;

	// TODO: options to emulate various DOS versions of the BPB
	if (f->set_boot_sector_bpb_size != 0)
		bs_sz = f->set_boot_sector_bpb_size;
	else if (f->base_info.FAT_size == 32)
		bs_sz = 90;	// MS-DOS 7.x / Windows 9x FAT32
	else
		bs_sz = 62;	// MS-DOS 6.x and earlier, FAT12/FAT16

	if (bs_sz < 40 || bs_sz > (f->disk_bytes_per_sector - 2U)) return 0;
	if (bs_sz < 90 && f->base_info.FAT_size == 32) return 0;
	return bs_sz;
}

int libmsfat_formatting_params_generate_boot_sector(unsigned char *sector512,struct libmsfat_formatting_params *f) {
	struct libmsfat_bootsector *bs = (struct libmsfat_bootsector*)sector512;
	unsigned int bs_sz;

	if (sector512 == NULL || f == NULL) return -1;
	bs_sz = libmsfat_formatting_params_get_bpb_size(f);

	// JMP instruction
	bs->BS_header.BS_jmpBoot[0] = 0xEB;
	bs->BS_header.BS_jmpBoot[1] = bs_sz - 2;
	bs->BS_header.BS_jmpBoot[2] = 0x90;

	// common BPB
	bs->BPB_common.BPB_BytsPerSec = htole16(f->disk_bytes_per_sector);
	bs->BPB_common.BPB_SecPerClus = f->base_info.Sectors_Per_Cluster;
	bs->BPB_common.BPB_RsvdSecCnt = htole16(f->reserved_sectors);
	bs->BPB_common.BPB_NumFATs = f->base_info.FAT_tables;
	if (f->base_info.FAT_size == 32)
		bs->BPB_common.BPB_RootEntCnt = 0;
	else
		bs->BPB_common.BPB_RootEntCnt = htole16(f->root_directory_entries);
	if (f->base_info.TotalSectors >= (uint32_t)65536 || f->base_info.FAT_size == 32)
		bs->BPB_common.BPB_TotSec16 = 0;
	else
		bs->BPB_common.BPB_TotSec16 = (uint16_t)htole16(f->base_info.TotalSectors);
	bs->BPB_common.BPB_Media = f->disk_media_type_byte;
	if (f->base_info.FAT_size == 32)
		bs->BPB_common.BPB_FATSz16 = 0;
	else
		bs->BPB_common.BPB_FATSz16 = htole16(f->base_info.FAT_table_size);
	bs->BPB_common.BPB_SecPerTrk = f->disk_geo.sectors;
	bs->BPB_common.BPB_NumHeads = f->disk_geo.heads;
	bs->BPB_common.BPB_TotSec32 = htole32(f->base_info.TotalSectors);

	if (f->base_info.FAT_size == 32) {
		bs->at36.BPB_FAT32.BPB_FATSz32 = htole32(f->base_info.FAT_table_size);
		bs->at36.BPB_FAT32.BPB_ExtFlags = 0;
		bs->at36.BPB_FAT32.BPB_FSVer = 0;
		bs->at36.BPB_FAT32.BPB_RootClus = htole32(f->root_cluster);
		bs->at36.BPB_FAT32.BPB_FSInfo = htole16(f->base_info.fat32.BPB_FSInfo);
		bs->at36.BPB_FAT32.BPB_BkBootSec = htole16(f->backup_boot_sector);
		bs->at36.BPB_FAT32.BS_DrvNum = (f->make_partition ? 0x80 : 0x00);
		bs->at36.BPB_FAT32.BS_BootSig = 0x29;
		bs->at36.BPB_FAT32.BS_VolID = htole32(f->volume_id);

		{
			char *s = f->volume_label;
			uint8_t *d = bs->at36.BPB_FAT32.BS_VolLab;
			uint8_t *df = d + 11;

			while (*s && d < df) *d++ = *s++;
			while (d < df) *d++ = ' ';
		}

		memcpy(bs->at36.BPB_FAT32.BS_FilSysType,"FAT32   ",8);
	}
	else {
		bs->at36.BPB_FAT.BS_DrvNum = (f->make_partition ? 0x80 : 0x00);
		bs->at36.BPB_FAT.BS_BootSig = 0x29;
		bs->at36.BPB_FAT.BS_VolID = htole32(f->volume_id);

		{
			char *s = f->volume_label;
			uint8_t *d = bs->at36.BPB_FAT.BS_VolLab;
			uint8_t *df = d + 11;

			while (*s && d < df) *d++ = *s++;
			while (d < df) *d++ = ' ';
		}

		if (f->base_info.FAT_size == 16)
			memcpy(bs->at36.BPB_FAT.BS_FilSysType,"FAT16   ",8);
		else
			memcpy(bs->at36.BPB_FAT.BS_FilSysType,"FAT12   ",8);
	}

	// signature
	sector512[510] = 0x55;
	sector512[511] = 0xAA;

	return 0;
}

int libmsfat_formatting_params_write_fat32_fsinfo(unsigned char *sector512,struct libmsfat_context_t *msfatctx,struct libmsfat_formatting_params *f) {
	struct libmsfat_bootsector *bs = (struct libmsfat_bootsector*)sector512;
	struct libmsfat_fat32_fsinfo_t fsinfo;
	uint64_t offset;

	if (sector512 == NULL || msfatctx == NULL || f == NULL)
		return -1;
	if (f->base_info.FAT_size != 32)
		return 0;

	memset(&fsinfo,0,sizeof(fsinfo));
	if (f->make_partition) offset = f->partition_offset * (uint64_t)f->disk_bytes_per_sector;
	else offset = 0;
	offset += (uint64_t)le16toh(bs->at36.BPB_FAT32.BPB_FSInfo) * (uint64_t)f->disk_bytes_per_sector;

	fsinfo.FSI_LeadSig = htole32((uint32_t)0x41615252UL);
	fsinfo.FSI_StrucSig = htole32((uint32_t)0x61417272UL);
	fsinfo.FSI_Free_Count = (uint32_t)0xFFFFFFFFUL;
	fsinfo.FSI_Nxt_Free = (uint32_t)0xFFFFFFFFUL;
	fsinfo.FSI_TrailSig = htole32((uint32_t)0xAA550000UL);

	if (msfatctx->write(msfatctx,(uint8_t*)(&fsinfo),offset,512))
		return -1;

	return 0;
}

int libmsfat_formatting_params_write_boot_sector(unsigned char *sector512,struct libmsfat_context_t *msfatctx,struct libmsfat_formatting_params *f) {
	struct libmsfat_bootsector *bs = (struct libmsfat_bootsector*)sector512;
	uint64_t offset;

	if (sector512 == NULL || msfatctx == NULL || f == NULL)
		return -1;
	if (msfatctx->write == NULL)
		return -1;

	if (f->make_partition) offset = f->partition_offset * (uint64_t)f->disk_bytes_per_sector;
	else offset = 0;

	if (msfatctx->write(msfatctx,sector512,offset,512))
		return -1;

	// backup copy too
	if (f->base_info.FAT_size == 32) {
		if (f->make_partition) offset = f->partition_offset * (uint64_t)f->disk_bytes_per_sector;
		else offset = 0;
		offset += (uint64_t)le16toh(bs->at36.BPB_FAT32.BPB_BkBootSec) * (uint64_t)f->disk_bytes_per_sector;
		if (msfatctx->write(msfatctx,sector512,offset,512))
			return -1;
	}

	return 0;
}

int libmsfat_formatting_params_reinit_final_info(struct libmsfat_formatting_params *f,struct libmsfat_context_t *msfatctx,unsigned char *sector512) {
	struct libmsfat_bootsector *bs = (struct libmsfat_bootsector*)sector512;

	if (f == NULL || msfatctx == NULL || sector512 == NULL)
		return -1;
	if (libmsfat_bs_compute_disk_locations(&f->final_info,bs))
		return -1;
	if (libmsfat_context_set_fat_info(msfatctx,&f->final_info))
		return -1;

	return 0;
}

int libmsfat_formatting_params_write_fat_clusters01(struct libmsfat_formatting_params *f,struct libmsfat_context_t *msfatctx) {
	unsigned int i;

	if (f == NULL || msfatctx == NULL)
		return -1;

	/* cluster dirty flags and media type byte */
	for (i=0;i < f->final_info.FAT_tables;i++) {
		if (libmsfat_context_write_FAT(msfatctx,(uint32_t)0xFFFFFF00UL + (uint32_t)f->disk_media_type_byte,libmsfat_CLUSTER_0_MEDIA_TYPE,i))
			return -1;
		if (libmsfat_context_write_FAT(msfatctx,(uint32_t)0xFFFFFFFFUL,libmsfat_CLUSTER_1_DIRTY_FLAGS,i))
			return -1;
	}

	return 0;
}

int libmsfat_formatting_params_init_root_directory(struct libmsfat_formatting_params *f,struct libmsfat_context_t *msfatctx,unsigned char *sector512) {
	struct libmsfat_bootsector *bs = (struct libmsfat_bootsector*)sector512;
	unsigned int i;

	if (f == NULL || msfatctx == NULL)
		return -1;

	if (f->final_info.FAT_size == 32) {
		/* remember the root cluster we chose? we need to mark it allocated, and
		 * then zero the cluster out */
		libmsfat_cluster_t rootclus = (libmsfat_cluster_t)le32toh(bs->at36.BPB_FAT32.BPB_RootClus);

		if (rootclus < (libmsfat_cluster_t)2UL)
			return -1;

		for (i=0;i < f->final_info.FAT_tables;i++) {
			if (libmsfat_context_write_FAT(msfatctx,(uint32_t)0xFFFFFFFFUL,rootclus,i))
				return -1;
		}

		// make sure the cluster corresponding to the root dir is zeroed out
		libmsfat_file_io_ctx_zero_cluster(rootclus,msfatctx);
	}
	else {
		unsigned char buf[512];
		uint64_t offset;
		uint32_t sz,rd;

		memset(buf,0x00,sizeof(buf));
		offset  = (uint64_t)msfatctx->fatinfo.RootDirectory_offset * (uint64_t)msfatctx->fatinfo.BytesPerSector;
		offset += msfatctx->partition_byte_offset;
		sz = (uint64_t)msfatctx->fatinfo.BytesPerSector * (uint64_t)msfatctx->fatinfo.RootDirectory_size;
		while (sz != (uint32_t)0) {
			if (sz > (uint32_t)sizeof(buf))
				rd = sizeof(buf);
			else
				rd = (size_t)sz;

			if (msfatctx->write(msfatctx,buf,offset,sizeof(buf)))
				return -1;

			offset += (uint64_t)rd;
			sz -= rd;
		}
	}

	return 0;
}

int libmsfat_formatting_params_init_root_directory_volume_label(struct libmsfat_formatting_params *f,struct libmsfat_context_t *msfatctx) {
	struct libmsfat_file_io_ctx_t *fioctx_parent = NULL;
	struct libmsfat_file_io_ctx_t *fioctx = NULL;

	if (f == NULL || msfatctx == NULL)
		return -1;

	fioctx_parent = libmsfat_file_io_ctx_create();
	if (fioctx_parent == NULL) goto cleanup_err;

	fioctx = libmsfat_file_io_ctx_create();
	if (fioctx == NULL) goto cleanup_err;

	if (libmsfat_file_io_ctx_assign_root_directory_with_parent(fioctx,fioctx_parent,msfatctx))
		goto cleanup_err;

	if (*f->volume_label != 0) {
		struct libmsfat_dirent_t dirent;

		memset(&dirent,0,sizeof(dirent));

		{
			const char *s = f->volume_label;
			char *d = dirent.a.n.DIR_Name;
			char *df = d + 11;

			while (*s && d < df) *d++ = *s++;
			while (d < df) *d++ = ' ';
		}
		dirent.a.n.DIR_Attr = libmsfat_DIR_ATTR_VOLUME_ID;

		if (libmsfat_file_io_ctx_write(fioctx,msfatctx,&dirent,sizeof(dirent)) != sizeof(dirent))
			goto cleanup_err;
	}

	fioctx_parent = libmsfat_file_io_ctx_destroy(fioctx_parent);
	fioctx = libmsfat_file_io_ctx_destroy(fioctx);
	return 0;
cleanup_err:
	fioctx_parent = libmsfat_file_io_ctx_destroy(fioctx_parent);
	fioctx = libmsfat_file_io_ctx_destroy(fioctx);
	return -1;
}

int libmsfat_formatting_params_zero_fat_tables(struct libmsfat_formatting_params *f,struct libmsfat_context_t *msfatctx) {
	unsigned char buf[512];
	unsigned int instance;
	uint64_t offset;
	uint64_t sz;
	size_t rd;

	if (f == NULL || msfatctx == NULL) return -1;
	if (msfatctx->write == NULL) return -1;

	for (instance=0;instance < msfatctx->fatinfo.FAT_tables;instance++) {
		offset  = (uint64_t)msfatctx->fatinfo.FAT_offset * (uint64_t)msfatctx->fatinfo.BytesPerSector;
		// and the partition offset
		offset += msfatctx->partition_byte_offset;
		// instance
		offset += (uint64_t)instance * (uint64_t)msfatctx->fatinfo.BytesPerSector * (uint64_t)msfatctx->fatinfo.FAT_table_size;

		// zero the FAT table
		memset(buf,0x00,sizeof(buf));
		sz = (uint64_t)msfatctx->fatinfo.BytesPerSector * (uint64_t)msfatctx->fatinfo.FAT_table_size;
		while (sz != (uint64_t)0) {
			if (sz > (uint64_t)sizeof(buf))
				rd = sizeof(buf);
			else
				rd = (size_t)sz;

			if (msfatctx->write(msfatctx,buf,offset,sizeof(buf)))
				return -1;

			offset += (uint64_t)rd;
			sz -= (uint64_t)rd;
		}
	}

	return 0;
}

