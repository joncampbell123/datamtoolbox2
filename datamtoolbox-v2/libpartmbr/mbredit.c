#include <stdlib.h>
#if !defined(_MSC_VER)
# include <unistd.h>
#endif
#include <sys/stat.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <endian.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include <datamtoolbox-v2/libint13chs/int13chs.h>
#include <datamtoolbox-v2/libpartmbr/partmbr.h>

#ifndef O_BINARY
# define O_BINARY 0
#endif

enum {
	LIBPARTMBR_TYPE_EMPTY=0x00,				// empty
	LIBPARTMBR_TYPE_FAT12_32MB=0x01,			// FAT12 within the first 32MB, or anywhere in logical drivve
	LIBPARTMBR_TYPE_XENIX_ROOT=0x02,			// XENIX root
	LIBPARTMBR_TYPE_XENIX_USR=0x03,				// XENIX usr
	LIBPARTMBR_TYPE_FAT16_32MB=0x04,			// FAT16 within the first 32MB, less than 65536 sectors, or anywhere in logical drive
	LIBPARTMBR_TYPE_EXTENDED_CHS=0x05,			// extended partition (CHS mapping)
	LIBPARTMBR_TYPE_FAT16B_8GB=0x06,			// FAT16B (CHS) within the first 8GB, 65536 or more sectors, or FAT12/FAT16 outside first 32MB, or in type 0x05 extended part
	LIBPARTMBR_TYPE_NTFS_HPFS=0x07,				// OS/2 IFS/HPFS, Windows NT NTFS, Windows CE exFAT
	LIBPARTMBR_TYPE_LOGSECT_FAT16=0x08,			// Logically sectored FAT12/FAT16 (larger sectors to overcome limits)

	LIBPARTMBR_TYPE_FAT32_CHS=0x0B,				// FAT32 (CHS)
	LIBPARTMBR_TYPE_FAT32_LBA=0x0C,				// FAT32 (LBA)

	LIBPARTMBR_TYPE_FAT16B_LBA=0x0E,			// FAT16B (LBA)
	LIBPARTMBR_TYPE_EXTENDED_LBA=0x0F,			// extended partition (LBA mapping)
};

const char *libpartmbr_partition_type_to_str(const uint8_t t) {
	switch (t) {
		case 0x00: return "empty";
		case 0x01: return "FAT12 (first 32MB)";
		case 0x02: return "XENIX root";
		case 0x03: return "XENIX usr";
		case 0x04: return "FAT16 (first 32MB, <65536)";
		case 0x05: return "Extended (CHS)";
		case 0x06: return "FAT16B (CHS) (first 8GB, >=65536)";
		case 0x07: return "IFS/HPFS/NTFS/exFAT";
		case 0x08: return "Logical sectored FAT12/FAT16";

		case 0x0B: return "FAT32 (CHS)";
		case 0x0C: return "FAT32 (LBA)";

		case 0x0E: return "FAT16B (LBA)";
		case 0x0F: return "Extended (LBA)";
	};

	return "";
}

enum libpartmbr_type_t {
	LIBPARTMBR_TYPE_CLASSIC=0,
	LIBPARTMBR_TYPE_MODERN,
	LIBPARTMBR_TYPE_AAP,
	LIBPARTMBR_TYPE_NEWLDR,
	LIBPARTMBR_TYPE_AST_SPEEDSTOR,
	LIBPARTMBR_TYPE_DISK_MANAGER,

	LIBPARTMBR_TYPE_MAX
};

const char *libpartmbr_type_str[LIBPARTMBR_TYPE_MAX] = {
	"classic",
	"modern",
	"AAP",
	"NEWLDR",
	"AST/SPEEDSTOR",
	"Disk manager"
};

static inline const char *libpartmbr_type_to_string(enum libpartmbr_type_t x) {
	if (x >= LIBPARTMBR_TYPE_MAX) return "";
	return libpartmbr_type_str[x];
}

struct libpartmbr_state_t {
	enum libpartmbr_type_t	type;
	unsigned char		entries;
};

typedef uint8_t libpartmbr_sector_t[LIBPARTMBR_SECTOR_SIZE];

void libpartmbr_state_zero(struct libpartmbr_state_t *x) {
	x->type = LIBPARTMBR_TYPE_CLASSIC;
	x->entries = 4;
}

int libpartmbr_state_probe(struct libpartmbr_state_t *x,libpartmbr_sector_t sct) {
	/* the sector must end in 0x55 0xAA */
	if (memcmp(sct+0x1FE,"\x55\xAA",2)) return 1;
	x->type = LIBPARTMBR_TYPE_CLASSIC;
	x->entries = 4;
	return 0;
}

#pragma pack(push,1)
struct libpartmbr_entry_t {
	uint8_t				bootable_flag;		/* +0 or "status" or "physical flag". 0x80 for bootable, 0x00 for not */
	struct int13h_packed_geometry_t	first_chs_sector;	/* +1 first absolute sector */
	uint8_t				partition_type;		/* +4 */
	struct int13h_packed_geometry_t	last_chs_sector;	/* +5 last absolute sector */
	uint32_t			first_lba_sector;	/* +8 CONVERTED TO/FROM HOST BYTE ORDER */
	uint32_t			number_lba_sectors;	/* +12 CONVERTED TO/FROM HOST BYTE ORDER */
};								/* =16 */
#pragma pack(pop)

int libpartmbr_read_entry(struct libpartmbr_entry_t *ent,struct libpartmbr_state_t *state,libpartmbr_sector_t sector,unsigned int entry) {
	if (entry >= state->entries) return 1;

	/* classic MBR reading, one of 4 entries */
	unsigned char *s = sector + 0x1BE + (entry * 0x10);
	assert(sizeof(*ent) == 0x10);
	memcpy(ent,s,0x10);

	/* next, convert byte order for the caller */
	ent->number_lba_sectors = le32toh(ent->number_lba_sectors);
	ent->first_lba_sector = le32toh(ent->first_lba_sector);
	return 0;
}

int libpartmbr_sanity_check() {
	if (sizeof(struct libpartmbr_entry_t) != 16) return 0;
	if (sizeof(struct int13h_packed_geometry_t) != 3) return 0;
	return -1;
}




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

