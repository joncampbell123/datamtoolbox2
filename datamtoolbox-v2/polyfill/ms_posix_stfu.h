#if defined(_MSC_VER) /* Microsoft C++ polyfill */
/* Microsoft has some sort of phobia about POSIX functions and some need of underscores */
# ifndef _CRT_SECURE_NO_WARNINGS
#  define _CRT_SECURE_NO_WARNINGS
# endif
#endif
