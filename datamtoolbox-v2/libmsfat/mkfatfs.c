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

static unsigned long long strtoull_with_unit_suffixes(const char *s,char **r,unsigned int base) {
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

static int extend_sparse_file_to_size(int fd,uint64_t size) {
	if (fd < 0) return -1;

#if defined(_LINUX)
	{
		struct stat st;

		/* we support giving a block device for the image.
		 * ftruncate() can't work with those, so we need to check first */
		if (fstat(fd,&st)) {
			fprintf(stderr,"Cannot identify file info\n");
			return -1;
		}

		if (S_ISREG(st.st_mode)) {
			/* Linux: we can easily make the file sparse and quickly generate what we want with ftruncate.
			 * On ext3/ext4 volumes this is a very fast way to create a disk image of any size AND to make
			 * it sparse so that disk space is allocated only to what we write data to. */
			if (ftruncate(fd,(off_t)size)) {
				fprintf(stderr,"ftruncate failed\n");
				return -1;
			}
		}
		else if (S_ISBLK(st.st_mode)) {
			/* check to make sure the device is that large */
			if (lseek(fd,(off_t)size,SEEK_SET) != (off_t)size) {
				fprintf(stderr,"Block device is not that large\n");
				return -1;
			}
		}
	}
#elif defined(_WIN32)
	/* Windows: Assuming NTFS and NT kernel (XP/Vista/7/8/10), mark the file as sparse, then set the file size. */
	{
		DWORD dwTemp;
		LONG lo,hi;
		HANDLE h;

		h = (HANDLE)_get_osfhandle(fd);
		if (h == INVALID_HANDLE_VALUE) return -1; // <- what?
		DeviceIoControl(h,FSCTL_SET_SPARSE,NULL,0,NULL,0,&dwTemp,NULL); // <- don't care if it fails.

		/* FIXME: "LONG" is 32-bit wide even in Win64, right? Does SetFilePointer() work the same in Win64? */
		lo = (LONG)(size & 0xFFFFFFFFUL);
		hi = (LONG)(size >> (uint64_t)32UL);
		if (SetFilePointer(h,lo,&hi,FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
			fprintf(stderr,"SetFilePointer failed\n");
			return -1;
		}

		/* and then make that the end of the file */
		if (SetEndOfFile(h) == 0) {
			fprintf(stderr,"SetEndOfFile failed\n");
			return -1;
		}
	}
#else
	/* try to make it sparse using lseek then a write.
	 * this is the most portable way I know to make a file of the size we want.
	 * it may cause lseek+write to take a long time in the system kernel to
	 * satisfy the request especially if the filesystem is not treating it as sparse i.e. FAT. */
	{
		char c = 0;

		if (lseek(fd,(off_t)size - (off_t)1U,SEEK_SET) != ((off_t)size - (off_t)1U)) {
			fprintf(stderr,"lseek failed\n");
			return -1;
		}
		if (write(fd,&c,1) != 1) {
			fprintf(stderr,"write failed\n");
			return -1;
		}
	}
#endif

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
				fmtparam->make_partition = 1;
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
				fmtparam->allow_fat32 = 0;
			}
			else if (!strcmp(a,"fat32")) {
				fmtparam->allow_fat32 = 1;
			}
			else if (!strcmp(a,"no-fat16")) {
				fmtparam->allow_fat16 = 0;
			}
			else if (!strcmp(a,"fat16")) {
				fmtparam->allow_fat16 = 1;
			}
			else if (!strcmp(a,"no-fat12")) {
				fmtparam->allow_fat12 = 0;
			}
			else if (!strcmp(a,"fat12")) {
				fmtparam->allow_fat12 = 1;
			}
			else if (!strcmp(a,"fat")) {
				if (libmsfat_formatting_params_set_FAT_width(fmtparam,(unsigned int)strtoul(argv[i++],NULL,0))) {
					fprintf(stderr,"--fat: Invalid FAT bit width\n");
					return 1;
				}
			}
			else if (!strcmp(a,"cluster-size")) {
				fmtparam->cluster_size = (uint16_t)strtoul(argv[i++],NULL,0);
			}
			else if (!strcmp(a,"cluster-non-power-of-2")) {
				fmtparam->allow_non_power_of_2_cluster_size = 1;
			}
			else if (!strcmp(a,"large-clusters")) {
				fmtparam->allow_64kb_or_larger_clusters = 1;
			}
			else if (!strcmp(a,"fat-tables")) {
				fmtparam->set_fat_tables = (uint8_t)strtoul(argv[i++],NULL,0);
			}
			else if (!strcmp(a,"root-directories")) {
				fmtparam->set_root_directory_entries = (uint32_t)strtoul(argv[i++],NULL,0);
			}
			else if (!strcmp(a,"fsinfo")) {
				fmtparam->set_fsinfo_sector = (uint32_t)strtoul(argv[i++],NULL,0);
			}
			else if (!strcmp(a,"reserved-sectors")) {
				fmtparam->set_reserved_sectors = (uint32_t)strtoul(argv[i++],NULL,0);
			}
			else if (!strcmp(a,"volume-id")) {
				if (libmsfat_formatting_params_set_volume_id(fmtparam,(unsigned int)strtoul(argv[i++],NULL,0))) {
					fprintf(stderr,"--volume-id: failed\n");
					return 1;
				}
			}
			else if (!strcmp(a,"volume-label")) {
				if (libmsfat_formatting_params_set_volume_label(fmtparam,argv[i++])) {
					fprintf(stderr,"--volume-label: rejected\n");
					return 1;
				}
			}
			else if (!strcmp(a,"root-cluster")) {
				if (libmsfat_formatting_params_set_root_cluster(fmtparam,(unsigned int)strtoul(argv[i++],NULL,0))) {
					fprintf(stderr,"--root-cluster: failed\n");
					return 1;
				}
			}
			else if (!strcmp(a,"backup-boot-sector")) {
				if (libmsfat_formatting_params_set_backup_boot_sector(fmtparam,(unsigned int)strtoul(argv[i++],NULL,0))) {
					fprintf(stderr,"--backup-boot-sector: failed\n");
					return 1;
				}
			}
			else if (!strcmp(a,"bpb-size")) {
				fmtparam->set_boot_sector_bpb_size = (uint8_t)strtoul(argv[i++],NULL,0);
			}
			else if (!strcmp(a,"no-partition-track-align")) {
				fmtparam->dont_partition_track_align = 1;
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

	if (fmtparam->make_partition) {
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
	if (fmtparam->make_partition && libmsfat_formatting_params_choose_partition_table(fmtparam)) return 1;
	if (libmsfat_formatting_params_auto_choose_media_type_byte(fmtparam)) return 1;
	if (libmsfat_formatting_params_auto_choose_volume_id(fmtparam)) return 1;
	if (libmsfat_formatting_params_auto_choose_root_cluster(fmtparam)) return 1;

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

	if (fmtparam->make_partition) {
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
		(unsigned long)fmtparam->reserved_sectors);
	printf("   Root directory entries: %lu (%lu sectors)\n",
		(unsigned long)fmtparam->root_directory_entries,
		(unsigned long)fmtparam->base_info.RootDirectory_size);
	printf("   %u FAT tables: %lu sectors per table\n",
		(unsigned int)fmtparam->base_info.FAT_tables,
		(unsigned long)fmtparam->base_info.FAT_table_size);
	printf("   Volume ID: 0x%08lx\n",
		(unsigned long)fmtparam->volume_id);

	if (fmtparam->base_info.FAT_size == 32)
		printf("   FAT32 FSInfo sector: %lu\n",
			(unsigned long)fmtparam->base_info.fat32.BPB_FSInfo);

	if (libmsfat_formatting_params_is_valid(fmtparam)) return 1;

	/* create the file, then extend out to the desired size */
	fd = open(s_image,O_RDWR|O_BINARY|O_CREAT|O_TRUNC,0644);
	if (fd < 0) {
		fprintf(stderr,"Unable to open disk image, %s\n",strerror(errno));
		return 1;
	}

	/* extend the file out to the size we want. */
	if (extend_sparse_file_to_size(fd,fmtparam->disk_size_bytes))
		return 1;

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

	if (fmtparam->make_partition) {
		msfatctx->partition_byte_offset = (uint64_t)fmtparam->partition_offset * (uint64_t)fmtparam->disk_bytes_per_sector;
		if (libmsfat_formatting_params_create_partition_table_and_write_entry(fmtparam,msfatctx)) return 1;
	}

	/* generate the boot sector! */
	{
		unsigned char sector[512];
		unsigned int bs_sz = libmsfat_formatting_params_get_bpb_size(fmtparam);
		struct libmsfat_bootsector *bs = (struct libmsfat_bootsector*)sector;

		memset(sector,0,sizeof(sector));
		memcpy(bs->BS_header.BS_OEMName,"DATATBOX",8);

		// trap in case of booting. trigger bootstrap
		sector[bs_sz] = 0xCD;
		sector[bs_sz+1] = 0x19;
		strcpy((char*)sector+bs_sz+2,"Not bootable, data only");

		// generate the rest
		if (libmsfat_formatting_params_generate_boot_sector(sector,fmtparam)) return 1;
		if (libmsfat_formatting_params_write_boot_sector(sector,msfatctx,fmtparam)) return 1;
		if (libmsfat_formatting_params_write_fat32_fsinfo(sector,msfatctx,fmtparam)) return 1;
		if (libmsfat_formatting_params_reinit_final_info(fmtparam,msfatctx,sector)) return 1;
		if (libmsfat_formatting_params_zero_fat_tables(fmtparam,msfatctx)) return 1;
		if (libmsfat_formatting_params_write_fat_clusters01(fmtparam,msfatctx)) return 1;
		if (libmsfat_formatting_params_init_fat32_root_cluster(fmtparam,msfatctx,sector)) return 1;
		if (libmsfat_formatting_params_init_root_directory_volume_label(fmtparam,msfatctx)) return 1;
	}

	fmtparam = libmsfat_formatting_params_destroy(fmtparam);
	msfatctx = libmsfat_context_destroy(msfatctx);
	close(fd);
	fd = -1;
	return 0;
}

