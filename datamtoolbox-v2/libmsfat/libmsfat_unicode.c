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
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#if defined(_MSC_VER)
# include <datamtoolbox-v2/polyfill/ms_cpp.h>
#endif
#include <datamtoolbox-v2/polyfill/lseek.h>

#include <datamtoolbox-v2/libmsfat/libmsfat_unicode.h>

#include <datamtoolbox-v2/unicode/utf8.h>
#include <datamtoolbox-v2/unicode/utf16.h>

void libmsfat_dirent_lfn_to_str_utf8(char *buf,size_t buflen,const struct libmsfat_lfn_assembly_t *lfn_name) {
	const uint16_t *s,*se;
	uint16_t uc;
	char *d,*de;

	if (buf == NULL || buflen == (size_t)0) return;

	d = buf;
	de = d + buflen - 1;
	if (lfn_name == NULL) {
		*d = 0;
	}
	else if (!lfn_name->name_avail) {
		*d = 0;
	}
	else {
		s = lfn_name->assembly;
		se = s + ((5U+6U+2U)*lfn_name->name_avail);
		assert((char*)se <= (((char*)lfn_name->assembly) + sizeof(lfn_name->assembly)));

		while (s < se && d < de) {
			if (*s == 0) break;
			uc = le16toh(*s); s++;

			if (utf16_is_surrogate_pair_start(uc)) {
				unicode_char_t chr;
				uint16_t lo;

				if (s >= se) break;
				lo = le16toh(*s); s++;
				if (lo == 0) break;

				chr = utf16_decode_surrogate_pair(/*hi*/uc,/*lo*/lo);
				if (chr != (unicode_char_t)0UL) utf8_encode(&d,de,chr);
			}
			else if (utf16_is_surrogate_pair_end(uc)) {
				// skip
			}
			else {
				utf8_encode(&d,de,(unicode_char_t)uc);
			}
		}
	}

	*d = 0;
	assert(d <= de);
}

void libmsfat_dirent_lfn_to_str_utf16le(char *buf,size_t buflen,const struct libmsfat_lfn_assembly_t *lfn_name) {
	const uint16_t *s,*se;
	uint16_t *d,*de;

	if (buf == NULL || buflen <= (size_t)1U) return;

	d = (uint16_t*)buf;
	de = d + (buflen >> (size_t)1U) - 1U;
	if (lfn_name == NULL) {
		*d = 0U;
	}
	else if (!lfn_name->name_avail) {
		*d = 0U;
	}
	else {
		s = lfn_name->assembly;
		se = s + ((5U+6U+2U)*lfn_name->name_avail);
		assert((char*)se <= (((char*)lfn_name->assembly) + sizeof(lfn_name->assembly)));

		while (s < se && d < de) {
			if (*s == 0) break;
			*d++ = *s++; // we're copying UTF16LE to UTF16LE, no conversion needed
		}
	}

	*d = 0;
	assert(d <= de);
}

int libmsfat_dirent_utf8_str_to_lfn(struct libmsfat_dirent_t *dirent,struct libmsfat_lfn_assembly_t *lfn_name,const char *name,unsigned int hash) {
	unicode_char_t uc;
	unsigned int o;

	if (lfn_name == NULL || name == NULL) return -1;
	lfn_name->name_avail = 0;
	lfn_name->max = 0;
	o = 0;

	if (*name == 0) return -1;

	while (*name != 0) {
		uc = utf8_decode(&name,name+16);
		if (UTF_IS_ERR(uc)) return -1;
		if ((o+1U) >= (sizeof(lfn_name->assembly)/sizeof(lfn_name->assembly[0]))) return -1;

		// TODO: If encoding needs a surrogate pair...

		lfn_name->assembly[o++] = (uint16_t)htole16(uc);
	}

	/* NUL */
	assert(o < (sizeof(lfn_name->assembly)/sizeof(lfn_name->assembly[0])));
	lfn_name->assembly[o++] = 0;

	/* how many dirents needed? */
	lfn_name->name_avail = (o + 12) / 13;
	if (lfn_name->name_avail > 32) return -1;

	/* pad fill 0xFFFF (noted Win9x behavior) */
	while (o < (13U * lfn_name->name_avail)) lfn_name->assembly[o++] = 0xFFFF;
	assert(o < (sizeof(lfn_name->assembly)/sizeof(lfn_name->assembly[0])));

	/* next we need to generate a 8.3 name, checksum it, and assign the checksum to the LFN */
	{
		uint16_t *sa = lfn_name->assembly;
		uint16_t *sf = sa + o;
		char *d,*df;

		d = dirent->a.n.DIR_Name;
		df = d + sizeof(dirent->a.n.DIR_Name) - 4;

		/* name */
		while (d < df && sa < sf) {
			uint16_t c = le16toh(*sa);

			if (c == '.' || c == 0)
				break;
			if (c > 32 && c < 127 &&
				!(c == ':' || c == '\\' || c == '/' || c == '*' || c == ';'))
				*d++ = (char)(c);

			sa++;
		}
		while (d < df) *d++ = '_';
		*d++ = '~';
		*d++ = (char)(((hash / 26 / 26) % 26) + 'A');
		*d++ = (char)(((hash / 26) % 26) + 'A');
		*d++ = (char)((hash % 26) + 'A');
		while (sa < sf && *sa != 0 && le16toh(*sa) != '.') sa++;

		d = dirent->a.n.DIR_Ext;
		df = d + sizeof(dirent->a.n.DIR_Ext);

		/* extension */
		if (le16toh(*sa) == '.') {
			sa++;
			while (d < df && sa < sf) {
				uint16_t c = le16toh(*sa);

				if (c == 0)
					break;
				if (c > 32 && c < 127 &&
					!(c == ':' || c == '\\' || c == '/' || c == '*' || c == ';'))
					*d++ = (char)(c);

				sa++;
			}
		}
		while (d < df) *d++ = ' ';
	}

	/* checksum */
	lfn_name->chksum[0] = libmsfat_lfn_83_checksum_dirent(dirent);
	for (o=1;o < lfn_name->name_avail;o++) lfn_name->chksum[o] = lfn_name->chksum[0];
	return 0;
}

int libmsfat_name_needs_lfn_utf8(const char *name) {
	unicode_char_t uc;
	int namelen = 0;
	int extlen = 0;

	while (*name != 0) {
		uc = utf8_decode(&name,name+16);
		if (UTF_IS_ERR(uc)) return 1;
		if (uc < (unicode_char_t)32UL || uc > (unicode_char_t)127UL) return 1;
		if (uc == (unicode_char_t)' ' || uc == (unicode_char_t)':' || uc == (unicode_char_t)'/' || uc == (unicode_char_t)'\\' ||
			uc == (unicode_char_t)'\"' || uc == (unicode_char_t)'\'') return 1;
		if (uc == (unicode_char_t)'.') break;
		if (uc >= (unicode_char_t)'a' && uc <= (unicode_char_t)'z') return 1;

		namelen++;
	}

	while (*name != 0) {
		uc = utf8_decode(&name,name+16);
		if (UTF_IS_ERR(uc)) return 1;
		if (uc < (unicode_char_t)32UL || uc > (unicode_char_t)127UL) return 1;
		if (uc == (unicode_char_t)' ' || uc == (unicode_char_t)':' || uc == (unicode_char_t)'/' || uc == (unicode_char_t)'\\' ||
			uc == (unicode_char_t)'\"' || uc == (unicode_char_t)'\'' || uc == (unicode_char_t)'.') return 1;
		if (uc >= (unicode_char_t)'a' && uc <= (unicode_char_t)'z') return 1;

		extlen++;
	}

	return (namelen > 8 || extlen > 3)?1:0;
}

int libmsfat_file_io_ctx_find_in_dir(struct libmsfat_file_io_ctx_t *fioctx,struct libmsfat_context_t *msfatctx,struct libmsfat_dirent_t *dirent,struct libmsfat_lfn_assembly_t *lfn_name,const char *name,unsigned int flags) {
	char tmp[512];

	if (fioctx == NULL || msfatctx == NULL || dirent == NULL || name == NULL) // lfn_name CAN be NULL
		return -1;
	if (!fioctx->is_directory)
		return -1;
	if (libmsfat_file_io_ctx_rewinddir(fioctx,msfatctx,lfn_name))
		return -1;

	while (libmsfat_file_io_ctx_readdir(fioctx,msfatctx,lfn_name,dirent) == 0) {
		if (dirent->a.n.DIR_Attr & libmsfat_DIR_ATTR_VOLUME_ID)
			continue;

		// first, match 8.3 filename
		libmsfat_dirent_filename_to_str(tmp,sizeof(tmp),dirent);
		if (!strcasecmp(tmp,name)) {
			// MATCH
			return 0;
		}

		// then match long filename
		if (lfn_name != NULL && lfn_name->name_avail) {
			libmsfat_dirent_lfn_to_str_utf8(tmp,sizeof(tmp),lfn_name);
			if (!strcasecmp(tmp,name)) {
				// MATCH
				return 0;
			}
		}
	}

	/* then create one */
	if (flags & libmsfat_path_lookup_CREATE) {
		unsigned char found = 0;
		uint32_t last_read;
		uint32_t ent_start;
		uint32_t empty = 0;
		size_t est_entries;
		int rd;

		if (libmsfat_file_io_ctx_lseek(fioctx,msfatctx,(uint32_t)0,/*flags*/0))
			return -1;
		if (libmsfat_file_io_ctx_tell(fioctx,msfatctx) != (uint32_t)0)
			return -1;

		/* form the file name, 8.3 and LFN. this is also where we determine the number of dirents
		 * we need to hold the LFN and the 8.3 name. */
		memset(dirent,0,sizeof(*dirent));
		if (lfn_name != NULL && libmsfat_name_needs_lfn_utf8(name)) {
			if (libmsfat_dirent_utf8_str_to_lfn(dirent,lfn_name,name,ent_start / (uint32_t)32UL)/*failed*/ || lfn_name->name_avail == 0) {
				if (libmsfat_dirent_str_to_filename(dirent,name))/*give up and make 8.3 version */
					return -1;
			}
		}
		else {
			if (libmsfat_dirent_str_to_filename(dirent,name))
				return -1;
		}

		if (flags & libmsfat_path_lookup_DIRECTORY)
			dirent->a.n.DIR_Attr |= libmsfat_DIR_ATTR_DIRECTORY;

		/* look for an empty spot large enough to fit it */
		{
			struct libmsfat_dirent_t scandirent;

			if (lfn_name != NULL && lfn_name->name_avail != 0)
				est_entries = 1 + lfn_name->name_avail; // number of entries we need for the LFN
			else
				est_entries = 1;

			rd = 0;
			do {
				last_read = libmsfat_file_io_ctx_tell(fioctx,msfatctx);
				rd = libmsfat_file_io_ctx_read(fioctx,msfatctx,&scandirent,sizeof(scandirent));
				if (rd == (int)sizeof(scandirent)) {
					if (scandirent.a.n.DIR_Name[0] == 0x00 || scandirent.a.n.DIR_Name[0] == (char)0xE5) {
						if (empty == 0) ent_start = last_read;
						empty++;

						if (empty == est_entries) {
							found = 1;
							break;
						}
					}
					else {
						ent_start = 0;
						empty = 0;
					}
				}
				else {
					break;
				}
			} while (1);
		}

		if (!found && rd == 0) {
			/* empty slot big enough not found, we'll have to append to the directory.
			 * note that we depend on the write being all or nothing. partial writes are
			 * something to avoid. start either at the end of the directory or the start
			 * of the last empty space we found but never finished scanning. */
			if (ent_start == 0) ent_start = fioctx->position;
			empty = (uint32_t)est_entries;
			found = 1;

			/* we need to enable extending the allocation chain on write */
			fioctx->allow_extend_allocation_chain = 1;
		}
		if (found) {
			uint32_t written_ent = 0;

			/* go and write it. LFN first, 8.3 name last. */
			if (libmsfat_file_io_ctx_lseek(fioctx,msfatctx,ent_start,libmsfat_lseek_FLAG_IGNORE_FILE_SIZE|libmsfat_lseek_FLAG_EXTEND_CLUSTER_CHAIN) == 0 &&
				libmsfat_file_io_ctx_tell(fioctx,msfatctx) == ent_start) {

				/* LFN name first */
				if (lfn_name != NULL && lfn_name->name_avail != 0) {
					struct libmsfat_dirent_t lfndirent;

					for (rd=lfn_name->name_avail-1;rd >= 0;rd--) {
						written_ent++;
						if (libmsfat_dirent_lfn_to_dirent_piece(&lfndirent,lfn_name,(unsigned int)rd))
							return -1;
						if (libmsfat_file_io_ctx_write(fioctx,msfatctx,&lfndirent,sizeof(lfndirent)) != sizeof(lfndirent))
							return -1;
					}
				}

				/* 8.3 entry next */
				written_ent++;
				if (libmsfat_file_io_ctx_write(fioctx,msfatctx,dirent,sizeof(*dirent)) != sizeof(*dirent))
					return -1;

				/* check */
				assert(written_ent <= est_entries);

				/* okay. now give the fioctx everything needed to update the dirent */
				if (lfn_name != NULL && lfn_name->name_avail != 0)
					fioctx->dirent_lfn_start = ent_start;
				else
					fioctx->dirent_lfn_start = (uint32_t)0xFFFFFFFFUL;

				fioctx->dirent_start = ent_start + (written_ent * (uint32_t)32UL) - (uint32_t)32UL;
				return 0;
			}
		}
	}

	return -1;
}

int libmsfat_file_io_ctx_path_lookup(struct libmsfat_file_io_ctx_t *fioctx,struct libmsfat_file_io_ctx_t *fioctx_parent,
	struct libmsfat_context_t *msfatctx,struct libmsfat_dirent_t *dirent,struct libmsfat_lfn_assembly_t *lfn_name,
	const char *path,unsigned int flags) {
	unsigned int tmp_flags;
	char tmp[320],*d,*df;

	// requires fioctx to set up target file/dir
	if (fioctx == NULL) return -1;
	// requires fioctx for the directory containing file/dir
	if (fioctx_parent == NULL) return -1;
	// other requirements
	if (msfatctx == NULL || dirent == NULL || path == NULL) return -1;
	// lfn_name == NULL is OK

	// parent at first represents a virtual "parent" of the root directory
	libmsfat_file_io_ctx_init(fioctx_parent);
	fioctx_parent->is_root_parent = 1;

	// start from root directory
	if (libmsfat_file_io_ctx_assign_root_directory(fioctx,msfatctx) || libmsfat_file_io_ctx_rewinddir(fioctx,msfatctx,lfn_name))
		return -1;
	if (!fioctx->is_directory)
		return -1;

	/* MS-DOS \ path separator or Linux / path separator */
	while (*path == '\\' || *path == '/') path++;

	while (*path != 0) {
		d = tmp; df = tmp+sizeof(tmp)-1;
		while (*path != 0 && d < df) {
			if (*path == '/' || *path == '\\') {
				while (*path == '\\' || *path == '/') path++; /* MS-DOS \ path separator or Linux / path separator */
				break;
			}

			*d++ = *path++;
		}
		*d = 0;
		if (d >= df) return -1;

		/* only allow CREATE if last element */
		tmp_flags = flags;
		if (*path != 0) tmp_flags &= ~libmsfat_path_lookup_CREATE;

		/* use parent fioctx to scan for path element */
		if (libmsfat_file_io_ctx_find_in_dir(fioctx,msfatctx,dirent,lfn_name,tmp,tmp_flags))
			return -1;

		/* fioctx becomes parent, start a new one */
		*fioctx_parent = *fioctx;
		memset(fioctx,0,sizeof(*fioctx));

		/* got it. assign to fioctx (child) */
		if (libmsfat_file_io_ctx_assign_from_dirent(fioctx,msfatctx,dirent) ||
			libmsfat_file_io_ctx_rewinddir(fioctx,msfatctx,lfn_name))
			return -1;

		/* if we created a directory, then we need to create . and .. directories */
		if ((flags & (libmsfat_path_lookup_DIRECTORY|libmsfat_path_lookup_CREATE)) == (libmsfat_path_lookup_DIRECTORY|libmsfat_path_lookup_CREATE)) {
			if (fioctx->first_cluster < (uint32_t)2UL) {
				struct libmsfat_dirent_t dotdirent;
				unsigned int count;

				memset(&dotdirent,0,sizeof(dotdirent));
				if (libmsfat_file_io_ctx_lseek(fioctx,msfatctx,fioctx->cluster_size,
					libmsfat_lseek_FLAG_IGNORE_FILE_SIZE|libmsfat_lseek_FLAG_EXTEND_CLUSTER_CHAIN))
					return -1;
				if (libmsfat_file_io_ctx_tell(fioctx,msfatctx) != fioctx->cluster_size)
					return -1;

				/* need to fill the directory with zeros */
				if (libmsfat_file_io_ctx_lseek(fioctx,msfatctx,0,0))
					return -1;
				if (libmsfat_file_io_ctx_tell(fioctx,msfatctx) != (uint32_t)0)
					return -1;
				count = (unsigned int)(fioctx->cluster_size / (uint32_t)32UL);
				while (count > 0) {
					if (libmsfat_file_io_ctx_write(fioctx,msfatctx,&dotdirent,sizeof(dotdirent)) != sizeof(dotdirent))
						return -1;

					count--;
				}
				if (libmsfat_file_io_ctx_lseek(fioctx,msfatctx,0,0))
					return -1;
				if (libmsfat_file_io_ctx_tell(fioctx,msfatctx) != (uint32_t)0)
					return -1;

				/* if we still don't have a starting cluster, then bail */
				if (fioctx->first_cluster < (uint32_t)2UL)
					return -1;

				/* prepare dirent */
				memset(dotdirent.a.n.DIR_Name,' ',8);
				memset(dotdirent.a.n.DIR_Ext,' ',3);

				/* . directory refers to this directory */
				dotdirent.a.n.DIR_Name[0] = '.';
				dotdirent.a.n.DIR_Attr = libmsfat_DIR_ATTR_DIRECTORY;
				dotdirent.a.n.DIR_FstClusLO = (uint16_t)(fioctx->first_cluster & 0xFFFF);
				if (msfatctx->fatinfo.FAT_size == 32)
					dotdirent.a.n.DIR_FstClusHI = (uint16_t)(fioctx->first_cluster >> 16);
				if (libmsfat_file_io_ctx_write(fioctx,msfatctx,&dotdirent,sizeof(dotdirent)) != sizeof(dotdirent))
					return -1;

				/* .. directory refers to parent directory. cluster == 0 is valid for root directory */
				dotdirent.a.n.DIR_Name[1] = '.';
				dotdirent.a.n.DIR_FstClusLO = (uint16_t)(fioctx_parent->first_cluster & 0xFFFF);
				if (msfatctx->fatinfo.FAT_size == 32)
					dotdirent.a.n.DIR_FstClusHI = (uint16_t)(fioctx_parent->first_cluster >> 16);
				if (libmsfat_file_io_ctx_write(fioctx,msfatctx,&dotdirent,sizeof(dotdirent)) != sizeof(dotdirent))
					return -1;

				/* update parent dirent */
				libmsfat_file_io_ctx_update_dirent_from_context(dirent,fioctx,fioctx_parent,msfatctx);
				if (libmsfat_file_io_ctx_write_dirent(fioctx,fioctx_parent,msfatctx,dirent,lfn_name))
					return -1;
			}
		}
	}

	return 0;
}

