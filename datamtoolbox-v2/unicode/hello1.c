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
#include <datamtoolbox-v2/polyfill/unix.h>

#include <datamtoolbox-v2/unicode/utf8.h>

const unicode_char_t msg[] = {
	// BOM
	UNICODE_BOM,
	// english
	'h','e','l','l','o',' ',
	// greek letters that look like "HELLO" spelled like "HEIIO" where the "O" is the Theta
	0x397U,0x395U,0x399U,0x399U,0x398U,' ',
	// black double arrow right, then *left, then *up, then *down
	0x23E9U,0x23EAU,0x23EB,0x23ECU,
	// end
	0
};

// NTS: This code assumes modern Linux configuration where the terminal uses UTF-8 encoding.
//      The console in Windows won't show this properly, BUT if you pipe to a text file and open in Notepad it should appear properly.
int main(int argc,char **argv) {
	const unicode_char_t *s = msg;
	unicode_char_t res;
	char tmp[32],*d;

	while (*s != (unicode_char_t)0UL) {
		d = tmp;
		res = utf8_encode(&d,tmp+sizeof(tmp),*s);
		if (res < UTFERR_FIRST_ERR) {
			*d = 0;
			printf("%s",tmp);
		}
		else {
			if (res == UTFERR_NO_ROOM)
				fprintf(stderr,"Out of room\n");
			else if (res == UTFERR_INVALID)
				fprintf(stderr,"Invalid (code 0x%08lx)\n",(unsigned long)(*s));

			putc('?',stdout);
		}

		s++;
	}
	printf("\n");
	return 0;
}

