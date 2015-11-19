SUBDIR=
TARGET_EXE=fatinfo fatinfo2
TARGET_LIB=libmsfat.a

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

exe: lib fatinfo fatinfo2
	for i in $(SUBDIR); do make -C "$$i" exe || break; done

lib: $(LIBINT13CHS) $(LIBPARTMBR) libmsfat.a
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
# DEPENDENCY
LIBPARTMBR=../libpartmbr/libpartmbr.a
$(LIBPARTMBR):
	make -C ../libpartmbr lib

#--------------------------------------
libmsfat.a: libmsfat.o
	rm -f $@
	$(AR) r $@ $^

libmsfat.o: libmsfat.c
	$(CC) $(CFLAGS) -c -o $@ $^

#--------------------------------------
fatinfo: fatinfo.o libmsfat.a $(LIBINT13CHS) $(LIBPARTMBR)
	$(CC) -o $@ $^ libmsfat.a $(LIBINT13CHS) $(LIBPARTMBR) $(LDFLAGS)

fatinfo.o: fatinfo.c
	$(CC) $(CFLAGS) -c -o $@ $^

#--------------------------------------
fatinfo2: fatinfo2.o libmsfat.a $(LIBINT13CHS) $(LIBPARTMBR)
	$(CC) -o $@ $^ libmsfat.a $(LIBINT13CHS) $(LIBPARTMBR) $(LDFLAGS)

fatinfo2.o: fatinfo2.c
	$(CC) $(CFLAGS) -c -o $@ $^
