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
VERSION	= 1.3

CC	= gcc
# Choose TMPDIR carefully.  See README for details.
DEFINES = -DTMPDIR=\"/tmp\" -DBUFSIZE=4096 \
	-DQMAIL_QUEUE=\"/var/qmail/bin/qmail-queue\"
CFLAGS	= -O -Wall -g
LD	= $(CC)
LDFLAGS	= -g
LIBS	=
RM	= rm -f

all: $(progs)

qmail-qfilter: qmail-qfilter.o
	$(LD) $(LDFLAGS) -o $@ qmail-qfilter.o $(LIBS)

.c.o: $<
	$(CC) $(CFLAGS) $(DEFINES) -c $<

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
	cp *.c *.1 *.spec COPYING README Makefile deny-filetypes \
		qmail-qfilter-$(VERSION)
	tar -czvf qmail-qfilter-$(VERSION).tar.gz qmail-qfilter-$(VERSION)
	rm -rf qmail-qfilter-$(VERSION)

clean:
	$(RM) core *.o $(progs)
	$(RM) qmail-qfilter-$(VERSION).tar.gz
	$(RM) qmail-qfilter-$(VERSION)-?.*.rpm
