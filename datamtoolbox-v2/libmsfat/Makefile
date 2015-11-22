SUBDIR=
TARGET_EXE=fatinfo fatinfo2 tabldump dcluster dumpdir fio1 fio2 fio3 fio4 fio5
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

exe: lib fatinfo fatinfo2 tabldump dcluster dumpdir fio1 fio2 fio3 fio4 fio5
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
# DEPENDENCY
LIBUNICODE=../unicode/libunicode.a
$(LIBUNICODE):
	make -C ../unicode lib

#--------------------------------------
libmsfat.a: libmsfat.o
	rm -f $@
	$(AR) r $@ $^

libmsfat.o: libmsfat.c
	$(CC) $(CFLAGS) -c -o $@ $^

#--------------------------------------
libmsfat_unicode.a: libmsfat_unicode.o
	rm -f $@
	$(AR) r $@ $^

libmsfat_unicode.o: libmsfat_unicode.c
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

#--------------------------------------
tabldump: tabldump.o libmsfat.a $(LIBINT13CHS) $(LIBPARTMBR)
	$(CC) -o $@ $^ libmsfat.a $(LIBINT13CHS) $(LIBPARTMBR) $(LDFLAGS)

tabldump.o: tabldump.c
	$(CC) $(CFLAGS) -c -o $@ $^

#--------------------------------------
dcluster: dcluster.o libmsfat.a $(LIBINT13CHS) $(LIBPARTMBR)
	$(CC) -o $@ $^ libmsfat.a $(LIBINT13CHS) $(LIBPARTMBR) $(LDFLAGS)

dcluster.o: dcluster.c
	$(CC) $(CFLAGS) -c -o $@ $^

#--------------------------------------
dumpdir: dumpdir.o libmsfat.a $(LIBINT13CHS) $(LIBPARTMBR)
	$(CC) -o $@ $^ libmsfat.a $(LIBINT13CHS) $(LIBPARTMBR) $(LDFLAGS)

dumpdir.o: dumpdir.c
	$(CC) $(CFLAGS) -c -o $@ $^

#--------------------------------------
fio1: fio1.o libmsfat.a $(LIBINT13CHS) $(LIBPARTMBR)
	$(CC) -o $@ $^ libmsfat.a $(LIBINT13CHS) $(LIBPARTMBR) $(LDFLAGS)

fio1.o: fio1.c
	$(CC) $(CFLAGS) -c -o $@ $^

#--------------------------------------
fio2: fio2.o libmsfat.a $(LIBINT13CHS) $(LIBPARTMBR)
	$(CC) -o $@ $^ libmsfat.a $(LIBINT13CHS) $(LIBPARTMBR) $(LDFLAGS)

fio2.o: fio2.c
	$(CC) $(CFLAGS) -c -o $@ $^

#--------------------------------------
fio3: fio3.o libmsfat.a libmsfat_unicode.a $(LIBINT13CHS) $(LIBPARTMBR) $(LIBUNICODE)
	$(CC) -o $@ $^ libmsfat.a libmsfat_unicode.a $(LIBINT13CHS) $(LIBPARTMBR) $(LIBUNICODE) $(LDFLAGS)

fio3.o: fio3.c
	$(CC) $(CFLAGS) -c -o $@ $^

#--------------------------------------
fio4: fio4.o libmsfat.a libmsfat_unicode.a $(LIBINT13CHS) $(LIBPARTMBR) $(LIBUNICODE)
	$(CC) -o $@ $^ libmsfat.a libmsfat_unicode.a $(LIBINT13CHS) $(LIBPARTMBR) $(LIBUNICODE) $(LDFLAGS)

fio4.o: fio4.c
	$(CC) $(CFLAGS) -c -o $@ $^

#--------------------------------------
fio5: fio5.o libmsfat.a libmsfat_unicode.a $(LIBINT13CHS) $(LIBPARTMBR) $(LIBUNICODE)
	$(CC) -o $@ $^ libmsfat.a libmsfat_unicode.a $(LIBINT13CHS) $(LIBPARTMBR) $(LIBUNICODE) $(LDFLAGS)

fio5.o: fio5.c
	$(CC) $(CFLAGS) -c -o $@ $^

