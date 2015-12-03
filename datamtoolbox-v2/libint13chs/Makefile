SUBDIR=
TARGET_EXE=int13cnv
TARGET_LIB=libint13chs.a

AR := ar
CC := gcc
CPP := cpp
CXX := g++

# Linux: ensure lseek64
CFLAGS_LSEEK64 = -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE

# I know GCC 4.8 has -std=gnu++11 but I want a wider target base
CXXFLAGS := -Wall -pedantic -std=gnu++0x -I../.. $(CFLAGS_LSEEK64)
LDFLAGS := -L.. -L../..
CFLAGS := -Wall -pedantic -std=gnu99 -I../../ $(CFLAGS_LSEEK64)

# identify the OS. Note the uname -o || uname, since Darwin (Mac OS) uname does not support -o
include ../system.ident.makefile

all: lib exe
	for i in $(SUBDIR); do make -C "$$i" all || break; done

exe: lib int13cnv
	for i in $(SUBDIR); do make -C "$$i" exe || break; done

lib: libint13chs.a
	for i in $(SUBDIR); do make -C "$$i" lib || break; done

clean:
	for i in $(SUBDIR); do make -C "$$i" clean || break; done
	rm -f *.o *.a *.la *.lib *.obj $(TARGET_EXE) $(TARGET_LIB)

#--------------------------------------
libint13chs.a: int13chs.o
	rm -f $@
	$(AR) r $@ $^

int13chs.o: int13chs.c
	$(CC) $(CFLAGS) -c -o $@ $^

#--------------------------------------
int13cnv: int13cnv.o libint13chs.a
	$(CC) -o $@ $^ libint13chs.a $(LDFLAGS)

int13cnv.o: int13cnv.c
	$(CC) $(CFLAGS) -c -o $@ $^

