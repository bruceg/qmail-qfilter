PACKAGE = qmail-qfilter
VERSION	= 1.4

prefix	= /usr
bindir	= $(prefix)/bin
mandir	= $(prefix)/man
man1dir	= $(mandir)/man1

install	= /usr/bin/install
installdata = $(install) -m 644
installbin = $(install) -m 755
installdir = $(install) -d

CC	= gcc
# Choose TMPDIR carefully.  See README for details.
DEFINES = -DTMPDIR=\"/tmp\" -DBUFSIZE=4096 \
	-DQMAIL_QUEUE=\"/var/qmail/bin/qmail-queue\"
CFLAGS	= -O -Wall -g $(DEFINES)
LD	= $(CC)
LDFLAGS	= -g
LIBS	=
RM	= rm -f

PROGS	= qmail-qfilter
SCRIPTS	=
MAN1S	= qmail-qfilter.1

all: $(PROGS)

qmail-qfilter: qmail-qfilter.o
	$(LD) $(LDFLAGS) -o $@ qmail-qfilter.o $(LIBS)

qmail-qfilter.o: qmail-qfilter.c
	$(CC) $(CFLAGS) $(DEFINES) -o $@ -c $<

install: install.bin install.man

install.bin: $(PROGS)
	$(installdir) $(bindir)
	$(installbin) $(PROGS) $(bindir)

install.man: $(MAN1S)
	$(installdir) $(man1dir)
	$(installdata) $(MAN1S) $(man1dir)

clean:
	$(RM) core *.o $(PROGS)
	$(RM) qmail-qfilter-$(VERSION).tar.gz
	$(RM) qmail-qfilter-$(VERSION)-?.*.rpm
