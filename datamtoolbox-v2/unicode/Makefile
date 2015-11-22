SUBDIR=
TARGET_EXE=hello1 hello2
TARGET_LIB=libunicode.a

AR := ar
CC := gcc
CPP := cpp
CXX := g++

# I know GCC 4.8 has -std=gnu++11 but I want a wider 
CXXFLAGS := -Wall -pedantic -std=gnu++0x -I../..
LDFLAGS := -L.. -L../..
CFLAGS := -Wall -pedantic -std=gnu99 -I../../

all: lib exe
	for i in $(SUBDIR); do make -C "$$i" all || break; done

exe: lib hello1 hello2
	for i in $(SUBDIR); do make -C "$$i" exe || break; done

lib: $(LIBINT13CHS) $(LIBPARTMBR) libunicode.a
	for i in $(SUBDIR); do make -C "$$i" lib || break; done

clean:
	for i in $(SUBDIR); do make -C "$$i" clean || break; done
	rm -f *.o *.a *.la *.lib *.obj $(TARGET_EXE) $(TARGET_LIB)

#--------------------------------------
libunicode.a: utf8.o utf16.o
	rm -f $@
	$(AR) r $@ $^

utf16.o: utf16.c
	$(CC) $(CFLAGS) -c -o $@ $^

utf8.o: utf8.c
	$(CC) $(CFLAGS) -c -o $@ $^

#--------------------------------------
hello1: hello1.o libunicode.a $(LIBINT13CHS) $(LIBPARTMBR)
	$(CC) -o $@ $^ libunicode.a $(LIBINT13CHS) $(LIBPARTMBR) $(LDFLAGS)

hello1.o: hello1.c
	$(CC) $(CFLAGS) -c -o $@ $^

#--------------------------------------
hello2: hello2.o libunicode.a $(LIBINT13CHS) $(LIBPARTMBR)
	$(CC) -o $@ $^ libunicode.a $(LIBINT13CHS) $(LIBPARTMBR) $(LDFLAGS)

hello2.o: hello2.c
	$(CC) $(CFLAGS) -c -o $@ $^

