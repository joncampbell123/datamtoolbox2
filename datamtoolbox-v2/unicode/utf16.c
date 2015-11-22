
#include <datamtoolbox-v2/unicode/utf16.h>

/* [doc] utf16le_encode
 *
 * Encode unicode character 'code' as UTF-16 (little endian)
 *
 * Parameters:
 *
 *    p = pointer to a char* pointer where the output will be written.
 *        on return the pointer will have been updated.
 *
 *    fence = first byte past the end of the buffer. the function will not
 *            write the output and update the pointer if doing so would bring
 *            it past this point, in order to prevent buffer overrun and
 *            possible memory corruption issues
 *
 *    c = unicode character to encode
 *
 * Warning:
 *
 *    UTF-16 output is generally 2 bytes long, but it can be 4 bytes long if
 *    a surrogate pair is needed to encode the unicode char.
 * 
 */
unicode_char_t utf16le_encode(char **ptr,char *fence,unicode_char_t code) {
	char *p = *ptr;

	if (!p) return UTFERR_NO_ROOM;
	if (code > (unicode_char_t)0x10FFFFUL) return UTFERR_INVALID;
	if (code > (unicode_char_t)0xFFFFUL) { /* UTF-16 surrogate pair */
		uint32_t lo =  (code - (unicode_char_t)0x10000UL)                          & (unicode_char_t)0x3FFUL;
		uint32_t hi = ((code - (unicode_char_t)0x10000UL) >> (unicode_char_t)10UL) & (unicode_char_t)0x3FFUL;
		if ((p+2+2) > fence) return UTFERR_NO_ROOM;
		*p++ = (char)( (hi+(unicode_char_t)0xD800UL)                       & (unicode_char_t)0xFFUL);
		*p++ = (char)(((hi+(unicode_char_t)0xD800UL) >> (unicode_char_t)8) & (unicode_char_t)0xFFUL);
		*p++ = (char)( (lo+(unicode_char_t)0xDC00UL)                       & (unicode_char_t)0xFFUL);
		*p++ = (char)(((lo+(unicode_char_t)0xDC00UL) >> (unicode_char_t)8) & (unicode_char_t)0xFFUL);
	}
	else if ((code & (unicode_char_t)0xF800UL) == (unicode_char_t)0xD800UL) { /* do not allow accidental surrogate pairs (0xD800-0xDFFF) */
		return UTFERR_INVALID;
	}
	else {
		if ((p+2) > fence) return UTFERR_NO_ROOM;
		*p++ = (char)( code                       & (unicode_char_t)0xFFUL);
		*p++ = (char)((code >> (unicode_char_t)8) & (unicode_char_t)0xFFUL);
	}

	*ptr = p;
	return 0;
}

/* [doc] utf16le_decode
 *
 * Decode one UTF-16 (little endian) unicode char from a UTF-16 string
 *
 * Parameters:
 *
 *    p = pointer to a char* pointer where the input will be read from.
 *        on return the pointer will have been updated.
 *
 *    fence = first byte past the end of the buffer. the function will not
 *            read from the pointer and update the pointer if doing so would
 *            bring it past this point, in order to prevent buffer overrun
 *            and memory access violation issues.
 *
 * Warning:
 *
 *    UTF-16 characters are generally 2 bytes long, but they can be 4
 *    bytes long if stored in a surrogate pair. This function will not
 *    decode the char if doing so would reach beyond the end of the buffer
 * 
 */
unicode_char_t utf16le_decode(const char **ptr,const char *fence) {
	const char *p = *ptr;
	unicode_char_t ret;
	int b=2;

	if (!p) return UTFERR_NO_ROOM;
	if ((p+1) >= fence) return UTFERR_NO_ROOM;

	ret = (unsigned char)p[0];
	ret |= ((unicode_char_t)((unsigned char)p[1])) << (unicode_char_t)8UL;
	if (ret >= (unicode_char_t)0xD800UL && ret <= (unicode_char_t)0xDBFFUL)
		b=4;
	else if (ret >= (unicode_char_t)0xDC00UL && ret <= (unicode_char_t)0xDFFFUL)
		{ p++; return UTFERR_INVALID; }

	if ((p+b) > fence)
		return UTFERR_NO_ROOM;

	p += 2;
	if (ret >= (unicode_char_t)0xD800UL && ret <= (unicode_char_t)0xDBFFUL) {
		/* decode surrogate pair */
		unicode_char_t hi = ret & (unicode_char_t)0x3FFUL;
		unicode_char_t lo = (unsigned char)p[0];
		lo |= ((unicode_char_t)((unsigned char)p[1])) << (unicode_char_t)8UL;
		p += 2;
		if (lo < (unicode_char_t)0xDC00UL || lo > (unicode_char_t)0xDFFFUL) return UTFERR_INVALID;
		lo &= (unicode_char_t)0x3FFUL;
		ret = ((hi << (unicode_char_t)10UL) | lo) + (unicode_char_t)0x10000UL;
	}

	*ptr = p;
	return ret;
}

