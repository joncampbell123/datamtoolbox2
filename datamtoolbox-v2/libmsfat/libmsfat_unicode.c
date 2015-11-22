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
	char *bufend,*d;

	if (buf == NULL || buflen <= (size_t)13) return;
	bufend = buf + buflen - 1;
	d = buf;

	if (lfn_name == NULL) {
		*d = 0;
		return;
	}
	if (!lfn_name->name_avail) {
		*d = 0;
		return;
	}

	s = lfn_name->assembly;
	se = s + ((5U+6U+2U)*lfn_name->name_avail);
	assert((char*)se <= (((char*)lfn_name->assembly) + sizeof(lfn_name->assembly)));

	while (s < se && d < bufend) {
		if (*s == 0) break;

		/* TODO: does the FAT Long Filename scheme support UTF16 surrogate pairs? */
		utf8_encode(&d,bufend,(unicode_char_t)(*s++));
	}

	*d = 0;
	assert(d <= bufend);
}

void libmsfat_dirent_lfn_to_str_utf16le(char *buf,size_t buflen,const struct libmsfat_lfn_assembly_t *lfn_name) {
	const uint16_t *s,*se;
	char *bufend,*d;

	if (buf == NULL || buflen <= (size_t)13) return;
	bufend = buf + buflen - 2;
	d = buf;

	if (lfn_name == NULL) {
		*d = 0;
		return;
	}
	if (!lfn_name->name_avail) {
		*d = 0;
		return;
	}

	s = lfn_name->assembly;
	se = s + ((5U+6U+2U)*lfn_name->name_avail);
	assert((char*)se <= (((char*)lfn_name->assembly) + sizeof(lfn_name->assembly)));

	while (s < se && d < bufend) {
		if (*s == 0) break;

		/* TODO: does the FAT Long Filename scheme support UTF16 surrogate pairs? */
		utf16le_encode(&d,bufend,(unicode_char_t)(*s++));
	}

	*d++ = 0;
	*d = 0;
	assert(d <= bufend);
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

	return -1;
}

