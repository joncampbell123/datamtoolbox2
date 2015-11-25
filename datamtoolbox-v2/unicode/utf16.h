#ifndef ____UNICODE_UTF16_H
#define ____UNICODE_UTF16_H

#include <datamtoolbox-v2/unicode/consts.h>

unicode_char_t utf16le_encode(char **ptr,char *fence,unicode_char_t code);
unicode_char_t utf16le_decode(const char **ptr,const char *fence);

// caller is expected to byte-swap the UTF-16 to host order before passing to these functions
unicode_char_t utf16_decode_surrogate_pair(uint16_t hi,uint16_t lo);
int utf16_is_surrogate_pair_start(const uint16_t c);
int utf16_is_surrogate_pair_end(const uint16_t c);

#endif

