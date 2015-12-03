
#if defined(_MSC_VER)
# include <datamtoolbox-v2/polyfill/ms_cpp.h>
#endif
#include <datamtoolbox-v2/polyfill/endian.h>
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
		unicode_char_t rcode = code - (unicode_char_t)0x10000UL;
		uint16_t lo = (uint16_t)(( rcode                          & (unicode_char_t)0x3FFUL) + (unicode_char_t)0xDC00UL);
		uint16_t hi = (uint16_t)(((rcode >> (unicode_char_t)10UL) & (unicode_char_t)0x3FFUL) + (unicode_char_t)0xD800UL);
		if ((p+2+2) > fence) return UTFERR_NO_ROOM;
		*((uint16_t*)p) = htole16(hi); p += 2;
		*((uint16_t*)p) = htole16(lo); p += 2;
	}
	else if ((code & (unicode_char_t)0xF800UL) == (unicode_char_t)0xD800UL) { /* do not allow accidental surrogate pairs (0xD800-0xDFFF) */
		return UTFERR_INVALID;
	}
	else {
		if ((p+2) > fence) return UTFERR_NO_ROOM;
		*((uint16_t*)p) = htole16((uint16_t)code); p += 2;
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

	if (!p) return UTFERR_NO_ROOM;
	if ((p+2) > fence) return UTFERR_NO_ROOM;

	ret = (unicode_char_t)le16toh(*((uint16_t*)p)); p += 2;
	if (ret >= (unicode_char_t)0xD800UL && ret <= (unicode_char_t)0xDBFFUL) {
		uint16_t hi,lo;

		if ((p+2) > fence) return UTFERR_NO_ROOM;
		hi = (uint16_t)(ret & (unicode_char_t)0x3FFUL);
		lo = le16toh(*((uint16_t*)p)); p += 2;
		if (lo < (unicode_char_t)0xDC00UL || lo > (unicode_char_t)0xDFFFUL) return UTFERR_INVALID;
		lo &= (unicode_char_t)0x3FFUL;
		ret = (((unicode_char_t)hi << (unicode_char_t)10UL) + (unicode_char_t)lo) + (unicode_char_t)0x10000UL;
	}
	else if (ret >= (unicode_char_t)0xDC00UL && ret <= (unicode_char_t)0xDFFFUL) {
		return UTFERR_INVALID;
	}

	*ptr = p;
	return ret;
}

unicode_char_t utf16_decode_surrogate_pair(uint16_t hi,uint16_t lo) {
	if (!utf16_is_surrogate_pair_start((unicode_char_t)hi) || !utf16_is_surrogate_pair_end((unicode_char_t)lo))
		return (unicode_char_t)0UL;

	return	(((unicode_char_t)(hi & (uint16_t)0x3FFU)) << (unicode_char_t)10UL) +
		 ((unicode_char_t)(lo & (uint16_t)0x3FFU)) +
		 ((unicode_char_t)0x10000UL);
}

int utf16_is_surrogate_pair_start(const uint16_t c) {
	return ((c & (uint16_t)0xFC00U) == (uint16_t)0xD800U);
}

int utf16_is_surrogate_pair_end(const uint16_t c) {
	return ((c & (uint16_t)0xFC00U) == (uint16_t)0xDC00U);
}

