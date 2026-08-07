# Makefile for installwatch

# Well, the only configurable part is the following variable.
# Make sure the directory you specify exists.

PREFIX=/usr/local

# End of configurable part

VERSION=0.7.0beta7

BINDIR=$(PREFIX)/bin
LIBDIR=$(PREFIX)/lib

LIBGCC=`gcc -print-libgcc-file-name`

all: installwatch.so

installwatch.so: installwatch.o
	ld $(LDFLAGS) -znow -shared -o installwatch.so installwatch.o -ldl -lc $(LIBGCC)

installwatch.o: installwatch.c localdecls.h
	gcc $(CPPFLAGS) $(CFLAGS) -Wall -c -D_GNU_SOURCE -DPIC -fPIC -D_REENTRANT -DVERSION=\"$(VERSION)\" installwatch.c

localdecls.h: create-localdecls
	./create-localdecls

install: all
	mkdir -p $(LIBDIR)
	mkdir -p $(BINDIR)
	if [ -r $(LIBDIR)/installwatch.so ]; then \
		rm -f  $(LIBDIR)/installwatch.so; \
	fi
	install installwatch.so $(LIBDIR)
	
	sed -e "s|#PREFIX#|$(PREFIX)|" < installwatch > $(BINDIR)/installwatch
	chmod 755 $(BINDIR)/installwatch

uninstall:
	rm -f $(LIBDIR)/installwatch.so
	rm -f $(BINDIR)/installwatch
	
clean:
	rm -f *~ *.bak *.o installwatch.so core localdecls.h libcfiletest libctest test-installwatch

tarball: clean
	tar -czvC .. -f ../installwatch-$(VERSION).tar.gz installwatch-$(VERSION)

test: install
	gcc -Wall -DVERSION=\"$(VERSION)\" -DLIBDIR=\"$(LIBDIR)\" -o test-installwatch test-installwatch.c -ldl
	PREFIX=$(PREFIX) $(PREFIX)/bin/installwatch ./test-installwatch
