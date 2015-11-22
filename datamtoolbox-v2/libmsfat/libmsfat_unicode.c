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

