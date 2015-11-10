#if defined(_MSC_VER)
/* shut up Microsoft. how the fuck is strerror() unsafe? */
# define _CRT_SECURE_NO_WARNINGS
# include <io.h>
/* shut up Microsoft. what the hell is your problem with POSIX functions? */
# define open _open
# define read _read
# define write _write
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

static int do_editloc(int entry,uint32_t start,uint32_t num,int type) {
	struct libpartmbr_entry_t ent;
	struct chs_geometry_t chs;

	if (type <= 0 || type > 255) {
		fprintf(stderr,"Type %d is not valid. Please use --type to specify nonzero type code.\n",type);
		return 1;
	}
	if (entry < 0) {
		fprintf(stderr,"You must specify a partition entry to remove\n");
		return 1;
	}
	if (entry >= diskimage_state.entries) {
		fprintf(stderr,"Entry number out of range (%u >= %u)\n",(unsigned int)entry,(unsigned int)diskimage_state.entries);
		return 1;
	}
	if (diskimage_use_chs == 0) {
		fprintf(stderr,"You must specify a disk geometry in order for correct values to be written.\n");
		fprintf(stderr,"If unsure, use '--geometry 1024/16/63' or '--geometry 16384/255/63'\n");
		return 1;
	}
	if (num == 0) {
		fprintf(stderr,"A partition entry cannot have zero sectors\n");
		return 1;
	}
	if (start == 0) {
		fprintf(stderr,"A partition entry cannot start at sector zero because the partition table sits there\n");
		return 1;
	}

	if (libpartmbr_read_entry(&ent,&diskimage_state,diskimage_sector,entry)) {
		fprintf(stderr,"Unable to read entry\n");
		return 1;
	}

	ent.partition_type = (uint8_t)type;
	ent.first_lba_sector = start; // write function will convert this field to little endian
	ent.number_lba_sectors = num; // ...and this one too

	/* update start C/H/S */
	if (int13cnv_lba_to_chs(&chs,&diskimage_chs,start)) {
		fprintf(stderr,"Failed to convert start LBA -> CHS\n");
		return 1;
	}
	int13cnv_chs_int13_cliprange(&chs,&diskimage_chs);
	if (int13cnv_chs_to_int13(&ent.first_chs_sector,&chs)) {
		fprintf(stderr,"Failed to convert start CHS -> INT13\n");
		return 1;
	}

	/* update end C/H/S */
	if (int13cnv_lba_to_chs(&chs,&diskimage_chs,start + num - (uint32_t)1UL)) {
		fprintf(stderr,"Failed to convert end LBA -> CHS\n");
		return 1;
	}
	int13cnv_chs_int13_cliprange(&chs,&diskimage_chs);
	if (int13cnv_chs_to_int13(&ent.last_chs_sector,&chs)) {
		fprintf(stderr,"Failed to convert end CHS -> INT13\n");
		return 1;
	}

	/* write back to disk */
	if (libpartmbr_write_entry(diskimage_sector,&ent,&diskimage_state,entry)) {
		fprintf(stderr,"Unable to write entry %u\n",(unsigned int)entry);
		return 1;
	}

	assert(diskimage_fd >= 0);
	if (lseek(diskimage_fd,0,SEEK_SET) != 0 || write(diskimage_fd,diskimage_sector,LIBPARTMBR_SECTOR_SIZE) != LIBPARTMBR_SECTOR_SIZE) {
		fprintf(stderr,"Failed to write MBR back\n");
		return 1;
	}

	return 0;
}

static int do_remove(int entry) {
	if (entry < 0) {
		fprintf(stderr,"You must specify a partition entry to remove\n");
		return 1;
	}
	if (entry >= diskimage_state.entries) {
		fprintf(stderr,"Entry number out of range (%u >= %u)\n",(unsigned int)entry,(unsigned int)diskimage_state.entries);
		return 1;
	}

	{
		struct libpartmbr_entry_t ent;

		memset(&ent,0,sizeof(ent));
		if (libpartmbr_write_entry(diskimage_sector,&ent,&diskimage_state,entry)) {
			fprintf(stderr,"Unable to write entry %u\n",(unsigned int)entry);
			return 1;
		}
	}

	assert(diskimage_fd >= 0);
	if (lseek(diskimage_fd,0,SEEK_SET) != 0 || write(diskimage_fd,diskimage_sector,LIBPARTMBR_SECTOR_SIZE) != LIBPARTMBR_SECTOR_SIZE) {
		fprintf(stderr,"Failed to write MBR back\n");
		return 1;
	}

	return 0;
}

static int do_create() {
	libpartmbr_state_zero(&diskimage_state);

	if (libpartmbr_create_partition_table(diskimage_sector,&diskimage_state)) {
		fprintf(stderr,"Failed to create partition table sector\n");
		return 1;
	}

	assert(diskimage_fd >= 0);
	if (lseek(diskimage_fd,0,SEEK_SET) != 0 || write(diskimage_fd,diskimage_sector,LIBPARTMBR_SECTOR_SIZE) != LIBPARTMBR_SECTOR_SIZE) {
		fprintf(stderr,"Failed to write MBR back\n");
		return 1;
	}

	return 0;
}

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

/* In the extended MBR partition scheme, the extended partition contains a "singly linked list"
 * of MBR partition tables. In each node, the first partition entry contains the partition of
 * interest, and the second partition entry refers to the next MBR partition table. To read
 * all partitions in the linked list, just read each node and follow to the next until the
 * second entry is not an extended partition type or the LBA address is zero.
 *
 * the third and fourth entries are (apparently) not used.
 *
 * LBA starting numbers in each entry (first & second) are relative to the extended partition.
 * Inside the linked list, an MBR entry with start sector 2048 would mean the physical disk
 * location is 2048 + LBA of the extended partition containing the linked list. So if the
 * main MBR lists an extended partition starting at sector 1024, the linked list entry's true
 * starting sector would be 2048 + 1024.
 *
 * I'm not sure if this is true, but the C/H/S values appear to be relative to the extended
 * partition as well, at least the way Linux fdisk generates the entries. */
static int do_ext_list(uint32_t first_lba) {
	struct libpartmbr_entry_t ent,ent2;
	struct libpartmbr_state_t state;
	libpartmbr_sector_t sector;
	struct chs_geometry_t chs;
	uint32_t scan_lba,new_lba;
	off_t seekofs;

	printf("-->MBR extended partitions:\n");
	scan_lba = first_lba;
	while (1) {
		seekofs = (off_t)512ULL * (off_t)scan_lba;
		if (lseek(diskimage_fd,seekofs,SEEK_SET) != seekofs || read(diskimage_fd,sector,LIBPARTMBR_SECTOR_SIZE) != LIBPARTMBR_SECTOR_SIZE)
			break;
		if (libpartmbr_state_probe(&state,sector))
			break;

		/* first and second entries have different meaning.
		 * first entry is the offset (relative to the EBR) of the partition and size.
		 * second entry is the offset of the next EBR (relative to this partition) */
		if (libpartmbr_read_entry(&ent,&state,sector,0))
			break;

		printf("-->ext at %10lu: ",(unsigned long)scan_lba);
		if (ent.partition_type != LIBPARTMBR_TYPE_EMPTY) {
			printf("    type=%s(0x%02x) ",
				libpartmbr_partition_type_to_str(ent.partition_type),
				ent.partition_type);

			printf("bootable=0x%02x ",
				ent.bootable_flag);

			printf("first_lba=%lu(+%lu)=%lu number_lba=%lu ",
				(unsigned long)ent.first_lba_sector,
				(unsigned long)scan_lba,
				(unsigned long)ent.first_lba_sector + (unsigned long)scan_lba,
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

		/* just so you know... */
		if (ent.partition_type == 0x05 || ent.partition_type == 0x0F)
			printf("* WARNING: Extended partition INSIDE an extended partition is not supported\n");

		/* next one...? */
		if (libpartmbr_read_entry(&ent2,&state,sector,1))
			break;
		if (!(ent2.partition_type == 0x05 || ent2.partition_type == 0x0F))
			break;
		if (ent2.first_lba_sector == 0)
			break;

		new_lba = ent2.first_lba_sector + first_lba;
		if (scan_lba >= new_lba) break;
		scan_lba = new_lba;
	}

	return 0;
}

static int do_list(unsigned int view_ext) {
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

		/* Extended MBR partitions have a linked list of partitions within to allow holding more than the main 4 in the MBR */
		if (ent.partition_type == LIBPARTMBR_TYPE_EXTENDED_CHS || ent.partition_type == LIBPARTMBR_TYPE_EXTENDED_LBA)
			do_ext_list(ent.first_lba_sector);
	}

	return 0;
}

int main(int argc,char **argv) {
	const char *s_geometry = NULL;
	const char *s_command = NULL;
	const char *s_image = NULL;
	const char *s_entry = NULL;
	const char *s_start = NULL;
	const char *s_type = NULL;
	const char *s_num = NULL;
	uint32_t start=0,num=0;
	int i,ret=1,entry=-1,type=-1;

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
			else if (!strcmp(a,"entry")) {
				s_entry = argv[i++];
			}
			else if (!strcmp(a,"c")) {
				s_command = argv[i++];
			}
			else if (!strcmp(a,"start")) {
				s_start = argv[i++];
			}
			else if (!strcmp(a,"type")) {
				s_type = argv[i++];
			}
			else if (!strcmp(a,"num")) {
				s_num = argv[i++];
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

		fprintf(stderr,"-c dump                     Dump the sector containing the MBR\n");
		fprintf(stderr,"\n");

		fprintf(stderr,"-c list                     Print the contents of the partition table\n");
		fprintf(stderr,"\n");

		fprintf(stderr,"-c create                   Create a new partition table\n");
		fprintf(stderr,"\n");

		fprintf(stderr,"-c remove                   Remove entry\n");
		fprintf(stderr,"\n");

		fprintf(stderr,"-c editloc                  Edit entry, change location and type\n");
		fprintf(stderr,"\n");
		return 1;
	}

	if (s_image == NULL) {
		fprintf(stderr,"No image provided\n");
		return 1;
	}

	if (s_entry != NULL)
		entry = atoi(s_entry);

	if (s_start != NULL)
		start = (uint32_t)strtoul(s_start,NULL,0);
	if (s_num != NULL)
		num = (uint32_t)strtoul(s_num,NULL,0);
	if (s_type != NULL)
		type = (int)strtol(s_type,NULL,0);

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

	if (!strcmp(s_command,"create") || !strcmp(s_command,"remove") ||
		!strcmp(s_command,"editloc"))
		diskimage_fd = open(s_image,O_RDWR|O_BINARY|O_CREAT,0644);
	else
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

	if (!strcmp(s_command,"create"))
		return do_create();

	if (!strcmp(s_command,"dump"))
		return do_dump();

	if (libpartmbr_state_probe(&diskimage_state,diskimage_sector)) {
		fprintf(stderr,"Doesn't look like MBR\n");
		return 1;
	}
	printf("MBR partition type: %s\n",libpartmbr_type_to_string(diskimage_state.type));

	if (!strcmp(s_command,"list"))
		ret = do_list(0);
	else if (!strcmp(s_command,"elist"))
		ret = do_list(1);
	else if (!strcmp(s_command,"remove"))
		ret = do_remove(entry);
	else if (!strcmp(s_command,"editloc"))
		ret = do_editloc(entry,start,num,type);
	else
		fprintf(stderr,"Unknown command '%s'\n",s_command);

	close(diskimage_fd);
	diskimage_fd = -1;
	return ret;
}

