#ifndef __DATAMTOOLBOX_POLYFILL_ENDIAN_H
#define __DATAMTOOLBOX_POLYFILL_ENDIAN_H

#if defined(_MSC_VER) /* Microsoft C++ polyfill */
// TODO
#elif defined(_DARWIN)
// Mac OS X
# include <machine/endian.h>
# include <datamtoolbox-v2/polyfill/yinyin-endian-macosx.h>
#else
// Linux
# include <endian.h>
#endif

#endif /* __DATAMTOOLBOX_POLYFILL_ENDIAN_H */
