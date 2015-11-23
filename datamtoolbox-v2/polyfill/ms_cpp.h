#if defined(_MSC_VER) /* Microsoft C++ polyfill */

/* Microsoft has some sort of phobia about POSIX functions and some need of underscores */
# ifndef _CRT_SECURE_NO_WARNINGS
#  define _CRT_SECURE_NO_WARNINGS
# endif

# include <sys/types.h>
# include <sys/stat.h>
# include <stdlib.h>
# include <stdint.h>
# include <string.h>
# include <assert.h>
# include <malloc.h>
# include <fcntl.h>
# include <stdio.h>
# include <ctype.h>
# include <errno.h>
# include <io.h>

# define dup _dup
# define open _open
# define read _read
# define write _write
# define close _close
# define isatty _isatty
# define lseek _lseeki64

/* Microsoft C++ does not have GNU/Linux's naming scheme for strcmpi vs strcasecmp. Oh, and their underscored POSIX fetish too. */
# define strcasecmp strcmpi
# define strcmpi _strcmpi

/* Microsoft C++ does not have le32toh() and friends, but Windows is Little Endian anyway*/
# ifndef le16toh
#  define le16toh(x) (x)
# endif
# ifndef le32toh
#  define le32toh(x) (x)
# endif
# ifndef htole16
#  define htole16(x) (x)
# endif
# ifndef htole32
#  define htole32(x) (x)
# endif

#endif
