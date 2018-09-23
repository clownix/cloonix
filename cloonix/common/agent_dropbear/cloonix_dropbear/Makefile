
LIB_CLOWNIX=../../lib_io_clownix
GLOB_INCLUDE=../../glob_include
BLKD=../../lib_blkd
RPCT=../../lib_rpct


LIB_IO_CLOWNIX=$(LIB_CLOWNIX)
LIBS=-L${BLKD} -lblkd \
     -L${RPCT} -lrpct \
     -lutil

LIBDBCLIENT=-L${LIB_IO_CLOWNIX} -lio_clownix -lssl -lcrypto -lrt
CFLAGS+=-I${LIB_IO_CLOWNIX}/include 
CFLAGS+=-I${GLOB_INCLUDE} 
CFLAGS+=-I../cloonix_agent/src 

COMMONOBJS=dbutil.o buffer.o \
 	   queue.o atomicio.o compat.o
 


SVROBJS= sshpty.o svr-session.o svr-service.o \
	svr-chansession.o svr-main.o cloonix_serv.o cloonix_timer.o

CLIOBJS=cli-main.o \
		cli-session.o cli-runopts.o cli-chansession.o \
		cli-channel.o \
		list.o cloonix_cli.o cloonix_x11.o

CLISVROBJS=common-session.o packet.o \
	   common-channel.o common-chansession.o \
           termcodes.o listener.o process-packet.o \
	   common-runopts.o circbuffer.o

SCPOBJS=scp.o progressmeter.o atomicio.o scpmisc.o compat.o

HEADERS=options.h dbutil.h session.h packet.h algo.h ssh.h buffer.h \
		service.h \
		channel.h chansession.h queue.h sshpty.h \
		termcodes.h runopts.h includes.h \
		atomicio.h compat.h listener.h 

dropbearobjs=$(COMMONOBJS) $(CLISVROBJS) $(SVROBJS)
dbclientobjs=$(COMMONOBJS) $(CLISVROBJS) $(CLIOBJS)
scpobjs=$(SCPOBJS)


srcdir=.

prefix=/usr/local
exec_prefix=${prefix}
datarootdir = ${prefix}/share
bindir=${exec_prefix}/bin
sbindir=${exec_prefix}/sbin
mandir=${datarootdir}/man

CC?=gcc
AR?=ar
RANLIB?=ranlib
STRIP=strip
INSTALL=install
CPPFLAGS=
CFLAGS+=-I. -I$(srcdir) -Wall

EXEEXT=

space:= $(empty) $(empty)


export CC
export CFLAGS
export RANLIB AR STRIP


all: dropbear dbclient scp

strip: $(TARGETS)
	$(STRIP) $(addsuffix $(EXEEXT), $(TARGETS))

install: $(addprefix inst_, $(TARGETS))

insmultidropbear: dropbearmulti
	$(INSTALL) -d $(DESTDIR)$(sbindir)
	-rm -f $(DESTDIR)$(sbindir)/dropbear$(EXEEXT)
	-ln -s $(bindir)/dropbearmulti$(EXEEXT) $(DESTDIR)$(sbindir)/dropbear$(EXEEXT) 
	$(INSTALL) -d $(DESTDIR)$(mandir)/man8
	$(INSTALL) -m 644 $(srcdir)/dropbear.8  $(DESTDIR)$(mandir)/man8/dropbear.8

insmulti%: dropbearmulti
	$(INSTALL) -d $(DESTDIR)$(bindir)
	-rm -f $(DESTDIR)$(bindir)/$*$(EXEEXT) 
	-ln -s $(bindir)/dropbearmulti$(EXEEXT) $(DESTDIR)$(bindir)/$*$(EXEEXT) 
	$(INSTALL) -d $(DESTDIR)$(mandir)/man1
	if test -e $*.1; then $(INSTALL) -m 644 $*.1 $(DESTDIR)$(mandir)/man1/$*.1; fi

# dropbear should go in sbin, so it needs a seperate rule
inst_dropbear: dropbear
	$(INSTALL) -d $(DESTDIR)$(sbindir)
	$(INSTALL) dropbear$(EXEEXT) $(DESTDIR)$(sbindir)
	$(INSTALL) -d $(DESTDIR)$(mandir)/man8
	$(INSTALL) -m 644 $(srcdir)/dropbear.8 $(DESTDIR)$(mandir)/man8/dropbear.8

inst_%: %
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) $*$(EXEEXT) $(DESTDIR)$(bindir)
	$(INSTALL) -d $(DESTDIR)$(mandir)/man1
	if test -e $*.1; then $(INSTALL) -m 644 $*.1 $(DESTDIR)$(mandir)/man1/$*.1; fi


# for some reason the rule further down doesn't like $($@objs) as a prereq.
dropbear: $(dropbearobjs)
dbclient: $(dbclientobjs)

dropbear: $(HEADERS) $(LIBTOM_DEPS) Makefile
	$(CC) $(LDFLAGS) -o $@$(EXEEXT) $($@objs) $(LIBS)

dbclient: $(HEADERS) $(LIBTOM_DEPS) Makefile
	$(CC) $(LDFLAGS) -o $@$(EXEEXT) $($@objs) $(LIBDBCLIENT) $(LIBS)


scp: $(SCPOBJS)  $(HEADERS) Makefile
	$(CC) $(LDFLAGS) -o $@$(EXEEXT) $(SCPOBJS)


link%:
	-rm -f $*$(EXEEXT)
	-ln -s dropbearmulti$(EXEEXT) $*$(EXEEXT)

clean:
	@-rm -f dropbear dbclient scp *.o  
