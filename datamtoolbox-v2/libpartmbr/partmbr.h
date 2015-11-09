
#define LIBPARTMBR_SECTOR_SIZE	512

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

enum libpartmbr_type_t {
	LIBPARTMBR_TYPE_CLASSIC=0,
	LIBPARTMBR_TYPE_MODERN,
	LIBPARTMBR_TYPE_AAP,
	LIBPARTMBR_TYPE_NEWLDR,
	LIBPARTMBR_TYPE_AST_SPEEDSTOR,
	LIBPARTMBR_TYPE_DISK_MANAGER,

	LIBPARTMBR_TYPE_MAX
};

struct libpartmbr_state_t {
	enum libpartmbr_type_t	type;
	unsigned char		entries;
};

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

typedef uint8_t libpartmbr_sector_t[LIBPARTMBR_SECTOR_SIZE];

void libpartmbr_state_zero(struct libpartmbr_state_t *x);
int libpartmbr_state_probe(struct libpartmbr_state_t *x,libpartmbr_sector_t sct);
int libpartmbr_read_entry(struct libpartmbr_entry_t *ent,const struct libpartmbr_state_t *state,const libpartmbr_sector_t sector,const unsigned int entry);
int libpartmbr_write_entry(libpartmbr_sector_t sector,const struct libpartmbr_entry_t *ent,const struct libpartmbr_state_t *state,const unsigned int entry);
const char *libpartmbr_partition_type_to_str(const uint8_t t);
int libpartmbr_sanity_check();

extern const char *libpartmbr_type_str[LIBPARTMBR_TYPE_MAX];

static inline const char *libpartmbr_type_to_string(enum libpartmbr_type_t x) {
	if (x >= LIBPARTMBR_TYPE_MAX) return "";
	return libpartmbr_type_str[x];
}

