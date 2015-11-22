#if defined(_MSC_VER) /* Microsoft C++ polyfill */
# define lseek_off_t __int64
#else
# include <sys/types.h>
# include <unistd.h>
# define lseek_off_t off_t
#endif
