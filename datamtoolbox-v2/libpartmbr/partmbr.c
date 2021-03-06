#if defined(_MSC_VER)
# include <datamtoolbox-v2/polyfill/ms_posix_stfu.h>
#endif
#if !defined(_MSC_VER)
# include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#if defined(_MSC_VER)
# include <datamtoolbox-v2/polyfill/ms_cpp.h>
#endif
#include <datamtoolbox-v2/polyfill/endian.h>
#include <datamtoolbox-v2/polyfill/lseek.h>
#include <datamtoolbox-v2/polyfill/unix.h>

#include <datamtoolbox-v2/libint13chs/int13chs.h>
#include <datamtoolbox-v2/libpartmbr/partmbr.h>

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

		case 0x11: return "FAT12 (first 32MB) (HIDDEN)";

		case 0x14: return "FAT16 (first 32MB, <65536) (HIDDEN)";

		case 0x16: return "FAT16B (CHS) (first 8GB, >=65536) (HIDDEN)";
		case 0x17: return "IFS/HPFS/NTFS/exFAT (HIDDEN)";

		case 0x1B: return "FAT32 (CHS) (HIDDEN)";
		case 0x1C: return "FAT32 (LBA) (HIDDEN)";

		case 0x1E: return "FAT16B (LBA) (HIDDEN)";

		case 0x82: return "Linux swap";
		case 0x83: return "Linux native";

		case 0xFD: return "Linux RAID";
	};

	return "";
}

const char *libpartmbr_type_str[LIBPARTMBR_TYPE_MAX] = {
	"classic",
	"modern",
	"AAP",
	"NEWLDR",
	"AST/SPEEDSTOR",
	"Disk manager"
};

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

int libpartmbr_read_entry(struct libpartmbr_entry_t *ent,const struct libpartmbr_state_t *state,const libpartmbr_sector_t sector,const unsigned int entry) {
	if (entry >= state->entries) return 1;

	/* classic MBR reading, one of 4 entries */
	if (entry >= 4) return 1;
	{
		const struct libpartmbr_entry_t *s = (const struct libpartmbr_entry_t*)((const unsigned char*)sector + 0x1BE + (entry * 0x10));
		assert(sizeof(*ent) == 0x10);
		*ent = *s;

		/* next, convert byte order for the caller */
		ent->number_lba_sectors = le32toh(ent->number_lba_sectors);
		ent->first_lba_sector = le32toh(ent->first_lba_sector);
	}

	return 0;
}

int libpartmbr_write_entry(libpartmbr_sector_t sector,const struct libpartmbr_entry_t *ent,const struct libpartmbr_state_t *state,const unsigned int entry) {
	if (entry >= state->entries) return 1;

	/* classic MBR reading, one of 4 entries */
	if (entry >= 4) return 1;
	{
		struct libpartmbr_entry_t *d = (struct libpartmbr_entry_t*)((unsigned char*)sector + 0x1BE + (entry * 0x10));
		assert(sizeof(*ent) == 0x10);
		*d = *ent;

		/* next, convert byte order for the caller */
		d->number_lba_sectors = htole32(d->number_lba_sectors);
		d->first_lba_sector = htole32(d->first_lba_sector);
	}

	return 0;
}

int libpartmbr_sanity_check() {
	if (sizeof(struct libpartmbr_entry_t) != 16) return 0;
	if (sizeof(struct int13h_packed_geometry_t) != 3) return 0;
	return -1;
}

int libpartmbr_create_partition_table(libpartmbr_sector_t sect,struct libpartmbr_state_t *state) {
	if (sect == NULL || state == NULL) return -1;

	if (state->type == LIBPARTMBR_TYPE_CLASSIC) {
		memset((unsigned char*)sect + 0x1BE,0,0x10 * 4);
	}
	else {
		return -1;
	}

	memcpy((unsigned char*)sect + 0x1FE,"\x55\xAA",2);
	return 0;
}

const char *libpartmbr_type_to_string(enum libpartmbr_type_t x) {
	if (x >= LIBPARTMBR_TYPE_MAX) return "";
	return libpartmbr_type_str[x];
}

void int13cnv_chs_int13_cliprange(struct chs_geometry_t *chs,struct chs_geometry_t *geo) {
	if (chs->cylinders > 1023) {
		chs->cylinders = 1023;
		chs->heads = geo->heads - 1;
		chs->sectors = geo->sectors;
	}
}

