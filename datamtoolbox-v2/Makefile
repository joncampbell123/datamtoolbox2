SUBDIR=libint13chs libpartmbr libmsfat

all:
	for i in $(SUBDIR); do make -C "$$i" all || break; done

exe:
	for i in $(SUBDIR); do make -C "$$i" exe || break; done

lib:
	for i in $(SUBDIR); do make -C "$$i" lib || break; done

clean:
	for i in $(SUBDIR); do make -C "$$i" clean || break; done

