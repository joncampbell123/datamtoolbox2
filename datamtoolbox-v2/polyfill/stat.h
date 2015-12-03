#ifndef __DATAMTOOLBOX_POLYFILL_STAT_H
#define __DATAMTOOLBOX_POLYFILL_STAT_H

#if defined(_MSC_VER) /* Microsoft C++ polyfill */
# define _polyfill_struct_stat struct stati64

/* Map Linux-like stat64 to Microsoft's _stati64 */
static int _polyfill_stat(int fd, _polyfill_struct_stat *buffer) {
	return _stati64(fd,buffer);
}

/* Map Linux-like fstat64 to Microsoft's _fstati64 */
static int _polyfill_fstat(int fd, _polyfill_struct_stat *buffer) {
	return _fstati64(fd,buffer);
}

/* Microsoft does not support "symlinks", _polyfill_lstat is the same as _polyfill_stat */
# define _polyfill_lstat _polyfill_stat
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
# include <sys/stat.h>
# include <unistd.h>
/* Linux: we set _FILE_OFFSET_BITS == 64, struct stat should be 64-bit and stat() should invoke 64-bit version */
# define _polyfill_struct_stat struct stat
# define _polyfill_fstat fstat
# define _polyfill_lstat lstat
# define _polyfill_stat stat
#endif

#endif /* __DATAMTOOLBOX_POLYFILL_STAT_H */
