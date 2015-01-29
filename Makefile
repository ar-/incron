
PREFIX = /usr/local
USERDATADIR = /var/spool/incron
SYSDATADIR = /etc/incron.d
CFGDIR = /etc
MANPATH = $(PREFIX)/share/man
RELEASE = incron-`cat VERSION`
RELEASEDIR = /tmp/$(RELEASE)
DOCDIR = $(PREFIX)/share/doc/$(RELEASE)/

USER = root

CXX ?= g++
INSTALL = install

#OPTIMIZE = -O2 -pedantic -std=c++11
OPTIMIZE = -O2 -pedantic -std=c++11
DEBUG = -g0
#WARNINGS = -Wall -W -Wshadow -Wpointer-arith -Wwrite-strings -ffor-scope
WARNINGS = -Wall -W 
CXXAUX = -pipe

CXXFLAGS ?= $(OPTIMIZE) $(DEBUG) $(CXXAUX)
CXXFLAGS += $(WARNINGS)

PROGRAMS = incrond incrontab

INCROND_OBJ = icd-main.o incrontab.o inotify-cxx.o usertable.o strtok.o appinst.o incroncfg.o appargs.o
INCRONTAB_OBJ = ict-main.o incrontab.o inotify-cxx.o strtok.o incroncfg.o appargs.o


all:	$(PROGRAMS)

incrond:	$(INCROND_OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(INCROND_OBJ)

incrontab:	$(INCRONTAB_OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(INCRONTAB_OBJ)

.cpp.o:
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(PROGRAMS)
	rm -f *.o

distclean: clean

install:	all install-man
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PREFIX)/bin/
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PREFIX)/sbin/
	$(INSTALL) -m 0755 -d $(DESTDIR)$(DOCDIR)/
	$(INSTALL) -m 04755 -o $(USER) incrontab $(DESTDIR)$(PREFIX)/bin/
	$(INSTALL) -m 0755 incrond $(DESTDIR)$(PREFIX)/sbin/
	$(INSTALL) -m 0755 -o $(USER) -d $(DESTDIR)$(USERDATADIR)
	$(INSTALL) -m 0755 -o $(USER) -d $(DESTDIR)$(SYSDATADIR)
	$(INSTALL) -m 0644 incron.conf.example $(DESTDIR)$(DOCDIR)/

install-man:	incrontab.1 incrontab.5 incrond.8 incron.conf.5
	$(INSTALL) -m 0755 -d $(DESTDIR)$(MANPATH)/man1
	$(INSTALL) -m 0755 -d $(DESTDIR)$(MANPATH)/man5
	$(INSTALL) -m 0755 -d $(DESTDIR)$(MANPATH)/man8
	$(INSTALL) -m 0644 incrontab.1 $(DESTDIR)$(MANPATH)/man1
	$(INSTALL) -m 0644 incrontab.5 $(DESTDIR)$(MANPATH)/man5
	$(INSTALL) -m 0644 incrond.8 $(DESTDIR)$(MANPATH)/man8
	$(INSTALL) -m 0644 incron.conf.5 $(DESTDIR)$(MANPATH)/man5

uninstall:	uninstall-man
	rm -f $(DESTDIR)$(PREFIX)/bin/incrontab
	rm -f $(DESTDIR)$(PREFIX)/sbin/incrond
	rm -rf $(DESTDIR)$(DOCDIR)/

uninstall-man:
	rm -f $(DESTDIR)$(MANPATH)/man1/incrontab.1
	rm -f $(DESTDIR)$(MANPATH)/man5/incrontab.5
	rm -f $(DESTDIR)$(MANPATH)/man8/incrond.8
	rm -f $(DESTDIR)$(MANPATH)/man5/incron.conf.5

update:		uninstall install

release:
	doxygen
	mkdir -p $(RELEASEDIR)
	cp -r doc $(RELEASEDIR)
	cp *.h $(RELEASEDIR)
	cp *.cpp $(RELEASEDIR)
	cp incron.conf.example $(RELEASEDIR)
	cp Makefile CHANGELOG COPYING LICENSE-GPL LICENSE-LGPL LICENSE-X11 README TODO VERSION $(RELEASEDIR)
	cp incrond.8 incrontab.1 incrontab.5 incron.conf.5 $(RELEASEDIR)
	tar -c -f $(RELEASE).tar -C $(RELEASEDIR)/.. $(RELEASE)
	bzip2 -9 $(RELEASE).tar
	tar -c -f $(RELEASE).tar -C $(RELEASEDIR)/.. $(RELEASE)
	gzip --best $(RELEASE).tar
	echo #!/bin/sh > myzip
	echo cd $(RELEASEDIR)/.. >> myzip
	echo zip -r -9 `pwd`/$(RELEASE).zip $(RELEASE) >> myzip
	chmod 0700 myzip
	./myzip
	rm -f myzip
	sha1sum $(RELEASE).tar.bz2 > sha1.txt
	sha1sum $(RELEASE).tar.gz >> sha1.txt
	sha1sum $(RELEASE).zip >> sha1.txt
	rm -rf $(RELEASEDIR)

release-clean:
	rm -rf doc
	rm -f $(RELEASE).tar.bz2
	rm -f $(RELEASE).tar.gz
	rm -f $(RELEASE).zip
	rm -f sha1.txt

.PHONY:	all clean distclean install install-man uninstall uninstall-man release release-clean update

.POSIX:

icd-main.o:	icd-main.cpp inotify-cxx.h incrontab.h usertable.h incron.h appinst.h incroncfg.h appargs.h
incrontab.o:	incrontab.cpp incrontab.h inotify-cxx.h strtok.h
inotify-cxx.o:	inotify-cxx.cpp inotify-cxx.h
usertable.o:	usertable.cpp usertable.h strtok.h
ict-main.o:	ict-main.cpp incrontab.h incron.h incroncfg.h appargs.h
strtok.o:	strtok.cpp strtok.h
appinst.o:	appinst.cpp appinst.h
incroncfg.o:	incroncfg.cpp incroncfg.h
appargs.o:	appargs.cpp appargs.h
