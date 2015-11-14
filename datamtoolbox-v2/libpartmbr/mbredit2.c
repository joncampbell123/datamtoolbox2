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

struct libpartmbr_context_entry_t {
	unsigned int			is_extended:1;	// parititon in the extended partition
	unsigned int			is_primary:1;	// partition in the master boot record
	unsigned int			is_empty:1;	// partition entry is empty
	unsigned int			__reserved__:5;	// padding

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

int libpartmbr_context_def_fd_read_sector(struct libpartmbr_context_t *r,uint8_t *buffer,uint32_t sector_number) {
	off_t ofs,res;
	int rd;

	if (r == NULL || buffer == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (r->user_fd < 0) {
		errno = EBADF;
		return -1;
	}

	ofs = (off_t)sector_number * (off_t)LIBPARTMBR_SECTOR_SIZE;
	res = lseek(r->user_fd,ofs,SEEK_SET);
	if (res < (off_t)0)
		return -1; // lseek() also sets errno
	else if (res != ofs) {
		errno = ERANGE;
		return -1;
	}

	rd = read(r->user_fd,buffer,LIBPARTMBR_SECTOR_SIZE);
	if (rd < 0)
		return -1; // read() also set errno
	else if (rd != LIBPARTMBR_SECTOR_SIZE) {
		errno = EIO;
		return -1;
	}

	return 0; // success
}

int libpartmbr_context_def_fd_write_sector(struct libpartmbr_context_t *r,const uint8_t *buffer,uint32_t sector_number) {
	off_t ofs,res;
	int rd;

	if (r == NULL || buffer == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (r->user_fd < 0) {
		errno = EBADF;
		return -1;
	}

	ofs = (off_t)sector_number * (off_t)LIBPARTMBR_SECTOR_SIZE;
	res = lseek(r->user_fd,ofs,SEEK_SET);
	if (res < (off_t)0)
		return -1; // lseek() also sets errno
	else if (res != ofs) {
		errno = ERANGE;
		return -1;
	}

	rd = write(r->user_fd,buffer,LIBPARTMBR_SECTOR_SIZE);
	if (rd < 0)
		return -1; // read() also set errno
	else if (rd != LIBPARTMBR_SECTOR_SIZE) {
		errno = EIO;
		return -1;
	}

	return 0; // success
}

int libpartmbr_context_init(struct libpartmbr_context_t *r) {
	if (r == NULL) return -1;
	memset(r,0,sizeof(*r));
	r->user_fd = -1;
#if defined(WIN32)
	r->user_win32_handle = INVALID_HANDLE_VALUE;
#endif
	return 0;
}

void libpartmbr_context_close_file(struct libpartmbr_context_t *r) {
	if (r->user_free_cb != NULL) r->user_free_cb(r);
	r->user_ptr = NULL;
	if (r->user_fd >= 0) {
		close(r->user_fd);
		r->user_fd = -1;
	}
#if defined(WIN32)
	if (r->user_win32_handle != INVALID_HANDLE_VALUE) {
		CloseHandle(r->user_win32_handle);
		r->user_win32_handle = INVALID_HANDLE_VALUE;
	}
#endif
}

int libpartmbr_context_alloc_list(struct libpartmbr_context_t *r,size_t count) {
	if (r == NULL) return -1;

	// 1024 partitions is WAY more than a normal hard drive would use!
	if (count > 1024) return -1;

	if (r->list != NULL) {
		// if the caller asks for less or same, then don't bother
		if (count <= r->list_alloc) return 0;

		// round up the count some in case the caller may ask for more again later
		count = (count | 3) + 1; // round up (even if multiple) to next multiple of 4

		// realloc. failure should mean the list allocation length is not changed.
		// success should mean the list becomes at least <count> entries long.
		{
			void *np = realloc((void*)(r->list),sizeof(struct libpartmbr_context_entry_t) * count);
			if (np == NULL) return -1;

			// it worked. take the new pointer. the old pointer is now invalid.
			r->list_alloc = count;
			r->list = np;
		}
	}
	else {
		if (count == 0) return -1; // ask for no entries? fail.

		if (count > 8) // don't let the caller ask for too little
			r->list_alloc = count;
		else
			r->list_alloc = 8;

		r->list = malloc(sizeof(struct libpartmbr_context_entry_t) * r->list_alloc);
		if (r->list == NULL) {
			r->list_count = 0;
			r->list_alloc = 0;
			return -1;
		}
	}

	return 0;
}

void libpartmbr_context_free_list(struct libpartmbr_context_t *r) {
	if (r == NULL) return;

	if (r->list != NULL) {
		free(r->list);
		r->list = NULL;
	}
	r->list_alloc = 0;
	r->list_count = 0;
}

void libpartmbr_context_free(struct libpartmbr_context_t *r) {
	if (r == NULL) return;
	libpartmbr_context_close_file(r);
	libpartmbr_context_free_list(r);
}

struct libpartmbr_context_t *libpartmbr_context_create() {
	struct libpartmbr_context_t *r;

	r = malloc(sizeof(struct libpartmbr_context_t));
	if (r != NULL && libpartmbr_context_init(r)) {
		free(r);
		r = NULL;
	}

	return r;
}

struct libpartmbr_context_t *libpartmbr_context_destroy(struct libpartmbr_context_t *r) {
	if (r != NULL) {
		libpartmbr_context_free(r);
		free(r);
	}

	return NULL;
}

int libpartmbr_context_assign_fd(struct libpartmbr_context_t *r,const int fd) {
	if (fd < 0) return -1;
	if (r == NULL) return -1;

	libpartmbr_context_close_file(r);
	r->user_fd = fd;

	/* if the user has not assigned read/write callbacks, then assign now */
	if (r->read_sector == NULL)
		r->read_sector = libpartmbr_context_def_fd_read_sector;
	if (r->write_sector == NULL)
		r->write_sector = libpartmbr_context_def_fd_write_sector;

	return 0;
}

int libpartmbr_context_read_partition_table(struct libpartmbr_context_t *r) {
	struct libpartmbr_entry_t mbrent;
	unsigned int i,primary_count;

	if (r == NULL) return -1;

	r->err_str = NULL;
	if (r->read_sector == NULL) {
		r->err_str = "Read sector function not assigned";
		return -1; /* read sector function must exist */
	}

	/* dump existing list */
	libpartmbr_context_free_list(r);

	/* read sector zero. is there an MBR there? failure to read  */
	if (r->read_sector(r,r->sector,(uint32_t)0UL) < 0) {
		r->err_str = "Unable to read sector 0";
		return -1;
	}
	if (libpartmbr_state_probe(&r->state,r->sector) || r->state.entries == 0) {
		r->err_str = "Master boot record not found";
		return -1;
	}

	/* allocate the list in prepraration for reading the MBR */
	if (libpartmbr_context_alloc_list(r,r->state.entries)) {
		r->err_str = "Unable to allocate list for master boot record";
		return -1;
	}
	r->list_count = 0;
	for (i=0;i < r->state.entries;i++) {
		struct libpartmbr_context_entry_t *ent = &r->list[r->list_count++];
		assert(r->list_count <= r->list_alloc); // sanity check!
		memset(ent,0,sizeof(*ent));
		ent->parent_entry = -1;
		ent->is_primary = 1;
		ent->index = i;

		if (libpartmbr_read_entry(&mbrent,&r->state,r->sector,i) || mbrent.partition_type == 0) {
			// failed/empty
			ent->is_empty = 1;
		}
		else {
			ent->entry = mbrent;
		}
	}

	/* if any partitions are extended in the list, and at the primary level, then
	 * step into extended partitions now. we do not support extended partitions inside
	 * extended partitions. we DO support multiple extended partitions even though
	 * Linux fdisk complains about it and ignores all but the first */
	primary_count = r->list_count;
	for (i=0;i < primary_count;i++) {
		struct libpartmbr_context_entry_t *ent = &r->list[i];
		uint32_t scan_first_lba,scan_lba,new_lba;
		struct libpartmbr_entry_t ex1,ex2;
		struct libpartmbr_state_t exstate;
		unsigned int ext_count = 0; // count the number of entries, in case of runaway extended partitions

		// primary MBR entries ONLY
		assert(i < r->list_count);
		if (ent->is_empty || !ent->is_primary)
			continue;

		// extended partitions (CHS or LBA) only
		if (!(ent->entry.partition_type == LIBPARTMBR_TYPE_EXTENDED_CHS ||
			ent->entry.partition_type == LIBPARTMBR_TYPE_EXTENDED_LBA))
			continue;

		// NOTE: it is said that the starting sector number is considered, but most implementations
		//       do not take into consideration the "number of sectors" count of the extended partition entry.
		if (ent->entry.first_lba_sector == (uint32_t)0UL)
			continue;

		scan_lba = scan_first_lba = ent->entry.first_lba_sector;
		ent = NULL; // !!! alloc_list() will reallocate if necessary, do not use *ent
		do {
			if (r->read_sector(r,r->sector,scan_lba) < 0)
				break;
			if (libpartmbr_state_probe(&exstate,r->sector) || r->state.entries == 0)
				break;

			// make an entry
			if (libpartmbr_context_alloc_list(r,r->list_count + 8)) {
				r->err_str = "Unable to extend list for master boot record";
				return -1;
			}
			ent = &r->list[r->list_count++];
			assert(r->list_count <= r->list_alloc);
			memset(ent,0,sizeof(*ent));
			ent->parent_entry = (int)i;
			ent->is_extended = 1;
			ent->index = ext_count;

			if (libpartmbr_read_entry(&ex1,&r->state,r->sector,0) || ex1.partition_type == 0) {
				// failed/empty
				ent->is_empty = 1;
			}
			else {
				ent->entry = ex1;
			}

			// read second entry, which points to the next partition in the list
			if (libpartmbr_read_entry(&ex2,&exstate,r->sector,1))
				break;
			// second entry MUST be a pointer to the next entry
			if (!(ex2.partition_type == LIBPARTMBR_TYPE_EXTENDED_CHS || ex2.partition_type == LIBPARTMBR_TYPE_EXTENDED_LBA))
				break;
			if (ex2.first_lba_sector == 0 || ex2.number_lba_sectors == 0)
				break;

			new_lba = ex2.first_lba_sector + scan_first_lba;
			if (new_lba <= scan_lba) break; // advance or stop reading
			scan_lba = new_lba;

			// most systems do not allow the linked list to grow too long
			if ((++ext_count) >= 256) break;
		} while (1);
	}

	return 0;
}





static struct libpartmbr_context_t*		diskimage_ctx = NULL;

static int do_list() {
	struct chs_geometry_t chs;
	int i;

	assert(diskimage_ctx != NULL);
	assert(diskimage_ctx->list != NULL);

	printf("MBR list (%u entries):\n",(unsigned int)diskimage_ctx->list_count);
	for (i=0;i < diskimage_ctx->list_count;i++) {
		struct libpartmbr_context_entry_t *ent = &diskimage_ctx->list[i];

		printf("    ");
		// if multiple bits set that should NOT be set, deliberate grammer errors will happen, by design
		if (ent->is_primary) printf("Primary ");
		if (ent->is_extended) printf("Extended ");
		if (ent->is_empty) printf("empty ");
		if (ent->parent_entry >= 0) printf("[child of entry #%d] ",ent->parent_entry);
		printf("entry #%u: (index=%u)\n",(unsigned int)i,(unsigned int)ent->index);

		if (!ent->is_empty) {
			printf("        Type: 0x%02x %s\n",(unsigned int)ent->entry.partition_type,libpartmbr_partition_type_to_str(ent->entry.partition_type));
			printf("        Bootable: 0x%02x\n",(unsigned int)ent->entry.bootable_flag);
			printf("        Start (LBA): %lx\n",(unsigned long)ent->entry.first_lba_sector);
			printf("        Size (LBA): %lx\n",(unsigned long)ent->entry.number_lba_sectors);

			if (int13cnv_int13_to_chs(&chs,&ent->entry.first_chs_sector) == 0)
				printf("        First sector (CHS): %u/%u/%u\n",(unsigned int)chs.cylinders,(unsigned int)chs.heads,(unsigned int)chs.sectors);

			if (int13cnv_int13_to_chs(&chs,&ent->entry.last_chs_sector) == 0)
				printf("        Last sector (CHS): %u/%u/%u\n",(unsigned int)chs.cylinders,(unsigned int)chs.heads,(unsigned int)chs.sectors);
		}
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
	int i,ret=1,entry=-1,type=-1,fd=-1;

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

		fprintf(stderr,"-c list                     Print the contents of the partition table\n");
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

	assert(libpartmbr_sanity_check());

	diskimage_ctx = libpartmbr_context_create();
	if (diskimage_ctx == NULL) {
		fprintf(stderr,"Cannot allocate libpartmbr context\n");
		return 1;
	}

	fd = open(s_image,O_RDONLY|O_BINARY);
	if (fd < 0) {
		fprintf(stderr,"Failed to open disk image, error=%s\n",strerror(errno));
		return 1;
	}
	{
		/* make sure it's a file */
		struct stat st;
		if (fstat(fd,&st) || !S_ISREG(st.st_mode)) {
			fprintf(stderr,"Image is not a file\n");
			return 1;
		}
	}

	/* good! hand it off to the context. it takes ownership, so we forget our copy. */
	if (libpartmbr_context_assign_fd(diskimage_ctx,fd)) {
		fprintf(stderr,"libpartmbr did not accept file descriptor %d\n",fd);
		return 1;
	}
	fd = -1;

	if (s_geometry != NULL) {
		if (int13cnv_parse_chs_geometry(&diskimage_ctx->geometry,s_geometry)) {
			fprintf(stderr,"Failed to parse C/H/S geometry\n");
			return 1;
		}
	}
	else {
		fprintf(stderr,"WARNING: Geometry not specified. Assuming CHS 16384/255/63\n");
		diskimage_ctx->geometry.cylinders = 16384;
		diskimage_ctx->geometry.sectors = 63;
		diskimage_ctx->geometry.heads = 255;
	}

	/* load the partition table! */
	if (libpartmbr_context_read_partition_table(diskimage_ctx)) {
		fprintf(stderr,"Failed to read partition table\n");
		return 1;
	}

	if (!strcmp(s_command,"list"))
		ret = do_list();
	else
		fprintf(stderr,"Unknown command '%s'\n",s_command);

	diskimage_ctx = libpartmbr_context_destroy(diskimage_ctx);
	return ret;
}

