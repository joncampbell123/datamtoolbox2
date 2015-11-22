#ifndef ____UNICODE_UTF8_H
#define ____UNICODE_UTF8_H

#include <datamtoolbox-v2/unicode/consts.h>

unicode_char_t utf8_encode(char **ptr,char *fence,unicode_char_t code);
unicode_char_t utf8_decode(const char **ptr,const char *fence);

#endif

