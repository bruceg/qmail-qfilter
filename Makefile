prefix	= /usr
bindir	= $(prefix)/bin
mandir	= $(prefix)/man
man1dir	= $(mandir)/man1

install	= /usr/bin/install
installdata = $(install) -m 644
installbin = $(install) -m 755
installdir = $(install) -d

progs	= qmail-qfilter

PACKAGE = qmail-qfilter
VERSION	= 1.2

CC	= gcc
CFLAGS	= -O -Wall
LD	= $(CC)
LDFLAGS	= -g
LIBS	=
RM	= rm -f

all: $(progs)

qmail-qfilter: qmail-qfilter.o
	$(LD) $(LDFLAGS) -o $@ qmail-qfilter.o $(LIBS)

install: install.bin install.man

install.bin: $(progs)
	$(installdir) $(bindir)
	$(installbin) $(progs) $(bindir)

install.man:
	$(installdir) $(man1dir)
	$(installdata) *.1 $(man1dir)

rpmdir	= $(HOME)/redhat

dist:
	rm -rf qmail-qfilter-$(VERSION)
	mkdir qmail-qfilter-$(VERSION)
	cp *.c *.1 *.spec COPYING README Makefile \
		qmail-qfilter-$(VERSION)
	tar -czvf qmail-qfilter-$(VERSION).tar.gz qmail-qfilter-$(VERSION)
	rm -rf qmail-qfilter-$(VERSION)

rpms: dist
	rpm -ta --clean qmail-qfilter-$(VERSION).tar.gz
	mv $(rpmdir)/RPMS/i386/qmail-qfilter-$(VERSION)-?.i386.rpm .
	mv $(rpmdir)/SRPMS/qmail-qfilter-$(VERSION)-?.src.rpm .

www: dist rpms
	install -m 444 qmail-qfilter-$(VERSION).tar.gz historical
	scp README \
		qmail-qfilter-$(VERSION).tar.gz \
		qmail-qfilter-$(VERSION)-?.*.rpm \
		bruceg@em.ca:www/qmail-qfilter

clean:
	$(RM) core *.o $(progs)
	$(RM) qmail-qfilter-$(VERSION).tar.gz
	$(RM) qmail-qfilter-$(VERSION)-?.*.rpm
