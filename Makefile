prefix	= /usr
bindir	= $(prefix)/bin
mandir	= $(prefix)/man
man1dir	= $(mandir)/man1

install	= /usr/bin/install
installdata = $(install) -m 644
installbin = $(install) -m 755
installdir = $(install) -d

progs	= qmail-qfilter

version	= 1.0

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
	rm -rf qmail-qfilter-$(version)
	mkdir qmail-qfilter-$(version)
	cp *.c *.1 *.spec COPYING README Makefile \
		qmail-qfilter-$(version)
	tar -czvf qmail-qfilter-$(version).tar.gz qmail-qfilter-$(version)
	rm -rf qmail-qfilter-$(version)

rpms: dist
	rpm -ta --clean qmail-qfilter-$(version).tar.gz
	mv $(rpmdir)/RPMS/i386/qmail-qfilter-$(version)-?.i386.rpm .
	mv $(rpmdir)/SRPMS/qmail-qfilter-$(version)-?.src.rpm .

www: dist rpms
	install -m 444 qmail-qfilter-$(version).tar.gz historical
	scp README \
		qmail-qfilter-$(version).tar.gz \
		qmail-qfilter-$(version)-?.*.rpm \
		bruceg@em.ca:www/qmail-qfilter

clean:
	$(RM) core *.o $(progs)
	$(RM) qmail-qfilter-$(version).tar.gz
	$(RM) qmail-qfilter-$(version)-?.*.rpm
