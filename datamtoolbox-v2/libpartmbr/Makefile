SUBDIR=
TARGET_EXE=mbredit mbredit2
TARGET_LIB=libpartmbr.a

AR := ar
CC := gcc
CPP := cpp
CXX := g++

# Linux: ensure lseek64
CFLAGS_LSEEK64 = -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE

# I know GCC 4.8 has -std=gnu++11 but I want a wider target base
CXXFLAGS := -D_FILE_OFFSET_BITS=64 -Wall -pedantic -std=gnu++0x -I../.. $(CFLAGS_LSEEK64)
LDFLAGS := -L.. -L../..
CFLAGS := -D_FILE_OFFSET_BITS=64 -Wall -pedantic -std=gnu99 -I../../ $(CFLAGS_LSEEK64)

# identify the OS. Note the uname -o || uname, since Darwin (Mac OS) uname does not support -o
include ../system.ident.makefile

all: lib exe
	for i in $(SUBDIR); do make -C "$$i" all || break; done

exe: lib mbredit mbredit2
	for i in $(SUBDIR); do make -C "$$i" exe || break; done

lib: $(LIBINT13CHS) libpartmbr.a
	for i in $(SUBDIR); do make -C "$$i" lib || break; done

clean:
	for i in $(SUBDIR); do make -C "$$i" clean || break; done
	rm -f *.o *.a *.la *.lib *.obj $(TARGET_EXE) $(TARGET_LIB)

#--------------------------------------
# DEPENDENCY
LIBINT13CHS=../libint13chs/libint13chs.a
$(LIBINT13CHS):
	make -C ../libint13chs lib

#--------------------------------------
libpartmbr.a: partmbr.o mbrctx.o
	rm -f $@
	$(AR) r $@ $^

partmbr.o: partmbr.c
	$(CC) $(CFLAGS) -c -o $@ $^

mbrctx.o: mbrctx.c
	$(CC) $(CFLAGS) -c -o $@ $^

#--------------------------------------
mbredit: mbredit.o libpartmbr.a $(LIBINT13CHS)
	$(CC) -o $@ $^ libpartmbr.a $(LIBINT13CHS) $(LDFLAGS)

mbredit.o: mbredit.c
	$(CC) $(CFLAGS) -c -o $@ $^

#--------------------------------------
mbredit2: mbredit2.o libpartmbr.a $(LIBINT13CHS)
	$(CC) -o $@ $^ libpartmbr.a $(LIBINT13CHS) $(LDFLAGS)

mbredit2.o: mbredit2.c
	$(CC) $(CFLAGS) -c -o $@ $^

