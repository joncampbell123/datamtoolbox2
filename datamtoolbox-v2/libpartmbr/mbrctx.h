
#ifndef DATAMTOOLBOXV2_LIBPARTMBR_MBRCTX_H
#define DATAMTOOLBOXV2_LIBPARTMBR_MBRCTX_H

#if defined(WIN32)
# include <windows.h>
#endif

#include <stdint.h>
#include <string.h>

#include <datamtoolbox-v2/libint13chs/int13chs.h>
#include <datamtoolbox-v2/libpartmbr/partmbr.h>

struct libpartmbr_context_entry_t {
	unsigned int			is_extended:1;	// parititon in the extended partition
	unsigned int			is_primary:1;	// partition in the master boot record
	unsigned int			is_empty:1;	// partition entry is empty
	unsigned int			start_lba_overflow:1; // extended partition start sector overflow
	unsigned int			__reserved__:4;	// padding

	uint32_t			extended_mbr_sector; // sector number of the extended MBR record, if is_extended
	unsigned int			index;		// index in the partition list. MBR: index in the table  Extended: counter from the start of linked list
	int				parent_entry;	// MBR: -1  Extended: index in master MBR of the extended partition this entry came from
	struct libpartmbr_entry_t	entry;		// partition entry
};

struct libpartmbr_context_t {
	struct chs_geometry_t		geometry;
	struct libpartmbr_state_t	state;
	union {
		uint16_t		val16;
		uint32_t		val32;
		uint64_t		val64;
		int			intval;
		long			longval;
	} user;
	void				(*user_free_cb)(struct libpartmbr_context_t *r);	// callback to free/deallocate user_ptr/user_id
	void*				user_ptr;
	uint64_t			user_id;
	int				user_fd;
#if defined(WIN32)
	HANDLE				user_win32_handle;
#endif
	const char*			err_str;	// error string
	int				(*read_sector)(struct libpartmbr_context_t *r,uint8_t *buffer,uint32_t sector_number); // 0=success   -1=error (see errno)
	int				(*write_sector)(struct libpartmbr_context_t *r,const uint8_t *buffer,uint32_t sector_number);
	// the partition list
	struct libpartmbr_context_entry_t*		list;
	size_t						list_alloc,list_count;
	// sector buffer
	libpartmbr_sector_t		sector;
};

int libpartmbr_context_def_fd_read_sector(struct libpartmbr_context_t *r,uint8_t *buffer,uint32_t sector_number);
int libpartmbr_context_def_fd_write_sector(struct libpartmbr_context_t *r,const uint8_t *buffer,uint32_t sector_number);
int libpartmbr_context_init(struct libpartmbr_context_t *r);
void libpartmbr_context_close_file(struct libpartmbr_context_t *r);
int libpartmbr_context_alloc_list(struct libpartmbr_context_t *r,size_t count);
void libpartmbr_context_free_list(struct libpartmbr_context_t *r);
void libpartmbr_context_free(struct libpartmbr_context_t *r);
struct libpartmbr_context_t *libpartmbr_context_create();
struct libpartmbr_context_t *libpartmbr_context_destroy(struct libpartmbr_context_t *r);
int libpartmbr_context_assign_fd(struct libpartmbr_context_t *r,const int fd);
int libpartmbr_context_read_partition_table(struct libpartmbr_context_t *r);
int libpartmbr_context_write_partition_table(struct libpartmbr_context_t *r);

#endif // DATAMTOOLBOXV2_LIBPARTMBR_MBRCTX_H

