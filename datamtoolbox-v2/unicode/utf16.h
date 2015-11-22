#ifndef ____UNICODE_UTF16_H
#define ____UNICODE_UTF16_H

#include <datamtoolbox-v2/unicode/consts.h>

unicode_char_t utf16le_encode(char **ptr,char *fence,unicode_char_t code);
unicode_char_t utf16le_decode(const char **ptr,const char *fence);

#endif

