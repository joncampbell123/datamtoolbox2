#if defined(_MSC_VER) /* Microsoft C++ polyfill */
# define lseek_off_t __int64

/* Map Linux-like lseek64 to Microsoft's _lseeki64 */
static lseek_off_t lseek64(int fd, lseek_off_t ofs, int origin) {
	return (lseek_off_t)_lseeki64(fd, (__int64)ofs, origin);
}
#else
# include <sys/types.h>
# include <unistd.h>
# define lseek_off_t off_t
/* Linux has lseek64() */
#endif
