
/* Microsoft C++ does not have S_ISREG */
#if defined(_MSC_VER) && !defined(S_ISREG) && defined(_S_IFREG)
# define S_ISREG(x) (x & _S_IFREG)
#endif

/* Microsoft C++ does not have S_ISBLK */
#if defined(_MSC_VER) && !defined(S_ISBLK) && defined(_S_IFBLK)
# define S_ISBLK(x) (x & _S_IFBLK)
#endif
#if defined(_MSC_VER) && !defined(S_ISBLK)
# define S_ISBLK(x) (0)
#endif

/* Linux does not have O_BINARY. That's primarily a DOS/Windows thing nowadays */
#ifndef O_BINARY
# define O_BINARY 0
#endif

