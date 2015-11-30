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

///////////////////////////////////////////

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

		lfn_name->assembly[o++] = (uint16_t)uc;
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
			if (*sa == '.' || *sa == 0)
				break;
			if (*sa > 32 && *sa < 127 &&
				!(*sa == ':' || *sa == '\\' || *sa == '/' || *sa == '*' || *sa == ';'))
				*d++ = (char)(*sa);

			sa++;
		}
		while (d < df) *d++ = '_';
		*d++ = '~';
		*d++ = (char)(((hash / 26 / 26) % 26) + 'A');
		*d++ = (char)(((hash / 26) % 26) + 'A');
		*d++ = (char)((hash % 26) + 'A');
		while (sa < sf && *sa != 0 && *sa != '.') sa++;

		d = dirent->a.n.DIR_Ext;
		df = d + sizeof(dirent->a.n.DIR_Ext);

		/* extension */
		if (*sa == '.') {
			sa++;
			while (d < df && sa < sf) {
				if (*sa == 0)
					break;
				if (*sa > 32 && *sa < 127 &&
					!(*sa == ':' || *sa == '\\' || *sa == '/' || *sa == '*' || *sa == ';'))
					*d++ = (char)(*sa);

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

int libmsfat_dirent_str_to_filename(struct libmsfat_dirent_t *dirent,const char *name) {
	unsigned int i;

	if (dirent == NULL || name == NULL) return -1;

	/* init */
	memset(dirent,0,sizeof(*dirent));
	while (*name == '.' || *name == ' ') return -1;
	if (*name == 0) return -1;

	/* name */
	i = 0;
	while (*name != 0 && *name != '.') {
		if (i >= 8) return -1;
		if (*name < 32) return -1;
		dirent->a.n.DIR_Name[i++] = toupper(*name);
		name++;
	}
	while (i < 8) dirent->a.n.DIR_Name[i++] = ' ';

	/* extension */
	if (*name == '.') {
		i = 0;
		name++;
		while (*name != 0) {
			if (*name == '.') return -1;
			if (*name < 32) return -1;
			if (i >= 3) return -1;
			dirent->a.n.DIR_Ext[i++] = toupper(*name);
			name++;
		}
		while (i < 3) dirent->a.n.DIR_Ext[i++] = ' ';
	}

	return 0;
}

int libmsfat_dirent_lfn_to_dirent_piece(struct libmsfat_dirent_t *dirent,struct libmsfat_lfn_assembly_t *lfn_name,unsigned int segment) {
	unsigned int i;
	uint16_t *s;

	if (dirent == NULL || lfn_name == NULL) return -1;
	if (segment >= lfn_name->name_avail || segment >= 32U) return -1;

	memset(dirent,0,sizeof(*dirent));
	dirent->a.lfn.LDIR_Ord = segment + 1;
	if ((segment + 1) == lfn_name->name_avail) dirent->a.lfn.LDIR_Ord |= 0x40; /* last entry */

	dirent->a.lfn.LDIR_Attr = 0x0F;
	dirent->a.lfn.LDIR_Chksum = lfn_name->chksum[segment];

	s = lfn_name->assembly + (segment * 13);
	for (i=0;i < 5;i++) dirent->a.lfn.LDIR_Name1[i] = *s++;
	for (i=0;i < 6;i++) dirent->a.lfn.LDIR_Name2[i] = *s++;
	for (i=0;i < 2;i++) dirent->a.lfn.LDIR_Name3[i] = *s++;

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

///////////////////////////////////////////

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
		struct libmsfat_dirent_t lfndirent;
		size_t name_len,est_entries;
		unsigned char found = 0;
		uint32_t last_read;
		uint32_t ent_start;
		uint32_t empty = 0;
		int rd;

		name_len = strlen(name);
		if (lfn_name != NULL)
			est_entries = 1 + ((name_len + 1 + 12) / 13); // number of entries we need for the LFN
		else
			est_entries = 1;

		if (libmsfat_file_io_ctx_lseek(fioctx,msfatctx,(uint32_t)0,/*flags*/0))
			return -1;
		if (libmsfat_file_io_ctx_tell(fioctx,msfatctx) != (uint32_t)0)
			return -1;

		/* look for an empty spot large enough to fit it */
		rd = 0;
		do {
			last_read = libmsfat_file_io_ctx_tell(fioctx,msfatctx);
			rd = libmsfat_file_io_ctx_read(fioctx,msfatctx,dirent,sizeof(*dirent));
			if (rd == (int)sizeof(*dirent)) {
				if (dirent->a.n.DIR_Name[0] == 0x00 || dirent->a.n.DIR_Name[0] == (char)0xE5) {
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

		if (!found && rd == 0) {
			/* empty slot big enough not found, we'll have to append to the directory.
			 * note that we depend on the write being all or nothing. partial writes are
			 * something to avoid. */
			ent_start = fioctx->position;
			empty = est_entries;
			found = 1;
		}
		if (found) {
			uint32_t written_ent = 0;

			/* we either found an empty slot big enough or we decided to append to the
			 * directory to make the entry */
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

			/* go and write it. LFN first, 8.3 name last. */
			if (libmsfat_file_io_ctx_lseek(fioctx,msfatctx,ent_start,libmsfat_lseek_FLAG_IGNORE_FILE_SIZE|libmsfat_lseek_FLAG_EXTEND_CLUSTER_CHAIN) == 0 &&
				libmsfat_file_io_ctx_tell(fioctx,msfatctx) == ent_start) {

				/* LFN name first */
				if (lfn_name != NULL && lfn_name->name_avail != 0) {
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

		/* use parent fioctx to scan for path element */
		if (libmsfat_file_io_ctx_find_in_dir(fioctx,msfatctx,dirent,lfn_name,tmp,flags))
			return -1;

		/* fioctx becomes parent, start a new one */
		*fioctx_parent = *fioctx;
		memset(fioctx,0,sizeof(*fioctx));

		/* got it. assign to fioctx (child) */
		if (libmsfat_file_io_ctx_assign_from_dirent(fioctx,msfatctx,dirent) ||
			libmsfat_file_io_ctx_rewinddir(fioctx,msfatctx,lfn_name))
			return -1;
	}

	return 0;
}

