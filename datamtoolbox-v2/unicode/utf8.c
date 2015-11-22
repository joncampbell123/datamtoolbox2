
#include <datamtoolbox-v2/unicode/utf8.h>

/* [doc] utf8_encode
 *
 * Encode unicode character 'code' as UTF-8 ASCII string
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
 *    Remember that one encoded UTF-8 character can occupy anywhere between
 *    1 to 4 bytes. Do not assume one byte = one char. Use the fence pointer
 *    to prevent buffer overruns and to know when the buffer should be
 *    emptied and refilled.
 * 
 */
unicode_char_t utf8_encode(char **ptr,char *fence,unicode_char_t code) {
	unsigned int uchar_size = 1;
	char *p = *ptr;

	if (!p) return UTFERR_NO_ROOM;
	if (code >= (unicode_char_t)0x80000000UL) return UTFERR_INVALID;
	if (p >= fence) return UTFERR_NO_ROOM;

	if (code >=      (unicode_char_t)0x4000000UL) uchar_size = 6;
	else if (code >= (unicode_char_t)0x200000UL) uchar_size = 5;
	else if (code >= (unicode_char_t)0x10000UL) uchar_size = 4;
	else if (code >= (unicode_char_t)0x800UL) uchar_size = 3;
	else if (code >= (unicode_char_t)0x80UL) uchar_size = 2;

	if ((p+uchar_size) > fence) return UTFERR_NO_ROOM;

	switch (uchar_size) {
		case 1:	*p++ = (char)(         code                          & 0x7FU);
			break;
		case 2:	*p++ = (char)(0xC0 | ((code >>  (unicode_char_t)6UL) & 0x1FU));
			*p++ = (char)(0x80 |  (code                          & 0x3FU));
			break;
		case 3:	*p++ = (char)(0xE0 | ((code >> (unicode_char_t)12UL) & 0x0FU));
			*p++ = (char)(0x80 | ((code >>  (unicode_char_t)6UL) & 0x3FU));
			*p++ = (char)(0x80 |  (code                          & 0x3FU));
			break;
		case 4:	*p++ = (char)(0xF0 | ((code >> (unicode_char_t)18UL) & 0x07U));
			*p++ = (char)(0x80 | ((code >> (unicode_char_t)12UL) & 0x3FU));
			*p++ = (char)(0x80 | ((code >>  (unicode_char_t)6UL) & 0x3FU));
			*p++ = (char)(0x80 |  (code                          & 0x3FU));
			break;
		case 5:	*p++ = (char)(0xF8 | ((code >> (unicode_char_t)24UL) & 0x03U));
			*p++ = (char)(0x80 | ((code >> (unicode_char_t)18UL) & 0x3FU));
			*p++ = (char)(0x80 | ((code >> (unicode_char_t)12UL) & 0x3FU));
			*p++ = (char)(0x80 | ((code >>  (unicode_char_t)6UL) & 0x3FU));
			*p++ = (char)(0x80 |  (code                          & 0x3FU));
			break;
		case 6:	*p++ = (char)(0xFC | ((code >> (unicode_char_t)30UL) & 0x01U));
			*p++ = (char)(0x80 | ((code >> (unicode_char_t)24UL) & 0x3FU));
			*p++ = (char)(0x80 | ((code >> (unicode_char_t)18UL) & 0x3FU));
			*p++ = (char)(0x80 | ((code >> (unicode_char_t)12UL) & 0x3FU));
			*p++ = (char)(0x80 | ((code >>  (unicode_char_t)6UL) & 0x3FU));
			*p++ = (char)(0x80 |  (code                          & 0x3FU));
			break;
	};

	*ptr = p;
	return (unicode_char_t)0;
}

/* [doc] utf8_decode
 *
 * Decode one UTF-8 unicode char from an ASCII string
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
 *    One UTF-8 character may be between 1 to 4 bytes long. This function will
 *    not decode a character if doing so will reach past the end of the buffer
 * 
 */
unicode_char_t utf8_decode(const char **ptr,const char *fence) {
	unsigned int uchar_size = 1;
	unicode_char_t ret = 0;
	const char *p = *ptr;
	char c;

	if (!p) return UTFERR_NO_ROOM;
	if (p >= fence) return UTFERR_NO_ROOM;

	ret = (unsigned char)(*p);
	if (ret >= (unicode_char_t)0xFEUL) { p++; return UTFERR_INVALID; }
	else if (ret >= (unicode_char_t)0xFCUL) uchar_size=6;
	else if (ret >= (unicode_char_t)0xF8UL) uchar_size=5;
	else if (ret >= (unicode_char_t)0xF0UL) uchar_size=4;
	else if (ret >= (unicode_char_t)0xE0UL) uchar_size=3;
	else if (ret >= (unicode_char_t)0xC0UL) uchar_size=2;
	else if (ret >= (unicode_char_t)0x80UL) { p++; return UTFERR_INVALID; }

	if ((p+uchar_size) > fence)
		return UTFERR_NO_ROOM;

	switch (uchar_size) {
		case 1:	p++;
			break;
		case 2:	ret = (unicode_char_t)(ret&0x1FU) << (unicode_char_t)6UL; p++;
			c = (unsigned char)(*p++); if ((c&0xC0U) != 0x80U) return UTFERR_INVALID;
			ret |= (unicode_char_t)(c&0x3FU);
			break;
		case 3:	ret = (unicode_char_t)(ret&0xFU) << (unicode_char_t)12UL; p++;
			c = (unsigned char)(*p++); if ((c&0xC0U) != 0x80U) return UTFERR_INVALID;
			ret |= (unicode_char_t)(c&0x3FU) << (unicode_char_t)6UL;
			c = (unsigned char)(*p++); if ((c&0xC0U) != 0x80U) return UTFERR_INVALID;
			ret |= (unicode_char_t)(c&0x3FU);
			break;
		case 4:	ret = (unicode_char_t)(ret&0x7U) << (unicode_char_t)18UL; p++;
			c = (unsigned char)(*p++); if ((c&0xC0U) != 0x80U) return UTFERR_INVALID;
			ret |= (unicode_char_t)(c&0x3FU) << (unicode_char_t)12UL;
			c = (unsigned char)(*p++); if ((c&0xC0U) != 0x80U) return UTFERR_INVALID;
			ret |= (unicode_char_t)(c&0x3FU) << (unicode_char_t)6UL;
			c = (unsigned char)(*p++); if ((c&0xC0U) != 0x80U) return UTFERR_INVALID;
			ret |= (unicode_char_t)(c&0x3FU);
			break;
		case 5:	ret = (unicode_char_t)(ret&0x3U) << (unicode_char_t)24UL; p++;
			c = (unsigned char)(*p++); if ((c&0xC0U) != 0x80U) return UTFERR_INVALID;
			ret |= (unicode_char_t)(c&0x3FU) << (unicode_char_t)18UL;
			c = (unsigned char)(*p++); if ((c&0xC0U) != 0x80U) return UTFERR_INVALID;
			ret |= (unicode_char_t)(c&0x3FU) << (unicode_char_t)12UL;
			c = (unsigned char)(*p++); if ((c&0xC0U) != 0x80U) return UTFERR_INVALID;
			ret |= (unicode_char_t)(c&0x3FU) << (unicode_char_t)6UL;
			c = (unsigned char)(*p++); if ((c&0xC0U) != 0x80U) return UTFERR_INVALID;
			ret |= (unicode_char_t)(c&0x3FU);
			break;
		case 6:	ret = (unicode_char_t)(ret&0x1U) << (unicode_char_t)30UL; p++;
			c = (unsigned char)(*p++); if ((c&0xC0U) != 0x80U) return UTFERR_INVALID;
			ret |= (unicode_char_t)(c&0x3FU) << (unicode_char_t)24UL;
			c = (unsigned char)(*p++); if ((c&0xC0U) != 0x80U) return UTFERR_INVALID;
			ret |= (unicode_char_t)(c&0x3FU) << (unicode_char_t)18UL;
			c = (unsigned char)(*p++); if ((c&0xC0U) != 0x80U) return UTFERR_INVALID;
			ret |= (unicode_char_t)(c&0x3FU) << (unicode_char_t)12UL;
			c = (unsigned char)(*p++); if ((c&0xC0U) != 0x80U) return UTFERR_INVALID;
			ret |= (unicode_char_t)(c&0x3FU) << (unicode_char_t)6UL;
			c = (unsigned char)(*p++); if ((c&0xC0U) != 0x80U) return UTFERR_INVALID;
			ret |= (unicode_char_t)(c&0x3FU);
			break;
	};

	*ptr = p;
	return ret;
}

