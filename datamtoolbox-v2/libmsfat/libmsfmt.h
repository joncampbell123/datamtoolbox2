#ifndef __DATAMTOOLBOX_LIBMSFAT_LIBMSFMT_H
#define __DATAMTOOLBOX_LIBMSFAT_LIBMSFMT_H

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
	uint8_t						set_fat_tables;
	uint8_t						set_boot_sector_bpb_size;
	uint16_t					cluster_size;
	uint32_t					backup_boot_sector;
	uint32_t					root_cluster;
	uint32_t					set_root_directory_entries;
	uint32_t					root_directory_entries;
	uint32_t					set_fsinfo_sector;
	uint32_t					reserved_sectors;
	uint32_t					set_reserved_sectors;
	char*						volume_label;
	unsigned int					lba_mode:1;
	unsigned int					chs_mode:1;
	unsigned int					volume_id_set:1;
	unsigned int					root_cluster_set:1;
	unsigned int					partition_type_set:1;
	unsigned int					disk_size_bytes_set:1;
	unsigned int					backup_boot_sector_set:1;
	unsigned int					disk_media_type_byte_set:1;
	unsigned int					disk_bytes_per_sector_set:1;
	unsigned int					allow_non_power_of_2_cluster_size:1;
	unsigned int					allow_64kb_or_larger_clusters:1;
	unsigned int					dont_partition_track_align:1;
	unsigned int					make_partition:1;
	unsigned int					allow_fat32:1;
	unsigned int					allow_fat16:1;
	unsigned int					allow_fat12:1;
};

int libmsfat_formatting_params_set_volume_label(struct libmsfat_formatting_params *f,const char *n);
void libmsfat_formatting_params_free(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_init(struct libmsfat_formatting_params *f);
struct libmsfat_formatting_params *libmsfat_formatting_params_destroy(struct libmsfat_formatting_params *f);
struct libmsfat_formatting_params *libmsfat_formatting_params_create();
int libmsfat_formatting_params_set_FAT_width(struct libmsfat_formatting_params *f,unsigned int width);
int libmsfat_formatting_params_update_disk_sectors(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_set_media_type_byte(struct libmsfat_formatting_params *f,unsigned int b);
int libmsfat_formatting_params_set_sector_size(struct libmsfat_formatting_params *f,unsigned int sz);
int libmsfat_formatting_params_set_disk_size_bytes(struct libmsfat_formatting_params *f,uint64_t byte_count);
int libmsfat_formatting_params_autofill_geometry(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_auto_choose_lba_chs_from_geometry(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_auto_choose_lba_chs_from_disk_size(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_enlarge_sectors_4GB_count_workaround(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_update_size_from_geometry(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_update_geometry_from_size_bios_xlat(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_update_geometry_from_size(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_set_partition_offset(struct libmsfat_formatting_params *f,uint64_t byte_offset);
int libmsfat_formatting_params_set_partition_size(struct libmsfat_formatting_params *f,uint64_t byte_offset);
int libmsfat_formatting_params_partition_autofill_and_align(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_update_total_sectors(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_auto_choose_FAT_size(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_auto_choose_FAT_tables(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_auto_choose_root_directory_size(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_auto_choose_cluster_size(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_auto_choose_reserved_sectors(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_auto_choose_fat32_bpb_fsinfo_location(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_auto_choose_fat32_backup_boot_sector(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_compute_cluster_count(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_choose_partition_table(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_set_partition_type(struct libmsfat_formatting_params *f,unsigned int t);
int libmsfat_formatting_params_auto_choose_media_type_byte(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_set_volume_id(struct libmsfat_formatting_params *f,uint32_t vol_id);
int libmsfat_formatting_params_set_backup_boot_sector(struct libmsfat_formatting_params *f,uint32_t sector);
int libmsfat_formatting_params_set_root_cluster(struct libmsfat_formatting_params *f,uint32_t cluster);
int libmsfat_formatting_params_auto_choose_root_cluster(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_auto_choose_volume_id(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_is_valid(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_create_partition_table_and_write_entry(struct libmsfat_formatting_params *f,struct libmsfat_context_t *msfatctx);
unsigned int libmsfat_formatting_params_get_bpb_size(struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_generate_boot_sector(unsigned char *sector512,struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_write_fat32_fsinfo(unsigned char *sector512,struct libmsfat_context_t *msfatctx,struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_write_boot_sector(unsigned char *sector512,struct libmsfat_context_t *msfatctx,struct libmsfat_formatting_params *f);
int libmsfat_formatting_params_reinit_final_info(struct libmsfat_formatting_params *f,struct libmsfat_context_t *msfatctx,unsigned char *sector512);
int libmsfat_formatting_params_write_fat_clusters01(struct libmsfat_formatting_params *f,struct libmsfat_context_t *msfatctx);
int libmsfat_formatting_params_init_fat32_root_cluster(struct libmsfat_formatting_params *f,struct libmsfat_context_t *msfatctx,unsigned char *sector512);
int libmsfat_formatting_params_init_root_directory_volume_label(struct libmsfat_formatting_params *f,struct libmsfat_context_t *msfatctx);
int libmsfat_formatting_params_zero_fat_tables(struct libmsfat_formatting_params *f,struct libmsfat_context_t *msfatctx);

#endif /* __DATAMTOOLBOX_LIBMSFAT_LIBMSFMT_H */

