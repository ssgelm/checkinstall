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
	# --build-id: linking with ld rather than gcc means nothing adds the
	# note otherwise, and without it there are no separate debug symbols.
	ld --build-id -znow -shared -o installwatch.so installwatch.o -ldl -lc $(LIBGCC)

# An interposition library has to define every ABI variant of a call itself:
# open() and open64(), stat() and stat64() and __stat64_time64(), and so on.
# _FILE_OFFSET_BITS=64 and _TIME_BITS=64 make glibc's headers rename those
# definitions, so the plain names disappear from the library and, worse, the
# true_* pointers get bound by dlsym() to a symbol whose struct layout no
# longer matches the one this file was compiled against. Distributions pass
# both by default on 32-bit, so undo them here rather than trusting the
# caller. The explicit 64-bit entry points below cover what they were for.
ABI_UNDEFS=-U_FILE_OFFSET_BITS -U_TIME_BITS

installwatch.o: installwatch.c localdecls.h
	gcc $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(ABI_UNDEFS) -Wall -c -g -D_GNU_SOURCE -DPIC -fPIC -D_REENTRANT -DVERSION=\"$(VERSION)\" installwatch.c

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
