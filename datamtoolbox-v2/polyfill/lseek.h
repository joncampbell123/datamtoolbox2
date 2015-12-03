#ifndef __DATAMTOOLBOX_POLYFILL_LSEEK_H
#define __DATAMTOOLBOX_POLYFILL_LSEEK_H

#if defined(_MSC_VER) /* Microsoft C++ polyfill */
# define lseek_off_t __int64

/* Map Linux-like lseek64 to Microsoft's _lseeki64 */
static lseek_off_t _polyfill_lseek(int fd, lseek_off_t ofs, int origin) {
	return (lseek_off_t)_lseeki64(fd, (__int64)ofs, origin);
}
#else
# ifndef _FILE_OFFSET_BITS
#  pragma message("you need to define _FILE_OFFSET_BITS")
# elif _FILE_OFFSET_BITS != 64
#  pragma message("you need to set _FILE_OFFSET_BITS == 64")
# endif
# ifndef _LARGEFILE64_SOURCE
#  pragma message("you need to define _LARGEFILE64_SOURCE")
# endif

# include <sys/types.h>
# include <unistd.h>
/* Linux: we set _FILE_OFFSET_BITS == 64, off_t should be 64-bit and lseek() should invoke 64-bit version */
# define lseek_off_t off_t
# define _polyfill_lseek lseek
#endif

#endif /* __DATAMTOOLBOX_POLYFILL_LSEEK_H */
