#ifndef ____UNICODE_CONSTS_H
#define ____UNICODE_CONSTS_H

#include <datamtoolbox-v2/unicode/consts.h>

#include <stdint.h>

typedef uint16_t utf16_char_t;
typedef uint32_t unicode_char_t;

#ifndef UNICODE_BOM
#define UNICODE_BOM	((unicode_char_t)0xFEFFUL)
#endif

#ifndef UTFERR_INVALID
#define UTFERR_INVALID	((unicode_char_t)0xFFFFFFFFUL)
#endif

#ifndef UTFERR_NO_ROOM
#define UTFERR_NO_ROOM	((unicode_char_t)0xFFFFFFFEUL)
#endif

#ifndef UTFERR_FIRST_ERR
#define UTFERR_FIRST_ERR UTFERR_NO_ROOM
#endif

#define UTF_IS_ERR(x)	((x) >= UTFERR_NO_ROOM)

#endif

