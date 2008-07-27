
PREFIX = /usr/local
DATADIR = /var/spool/incron
MANPATH = /usr/share/man
RELEASE = incron-`cat VERSION`
RELEASEDIR = /tmp/$(RELEASE)

USER = root

CXX = g++
INSTALL = install

OPTIMIZE = -O2
DEBUG = -g0
WARNINGS = -Wall
CXXAUX = -pipe

CPPFLAGS = 
CXXFLAGS = $(OPTIMIZE) $(DEBUG) $(WARNINGS) $(CXXAUX)
LDFLAGS = $(WARNINGS)

PROGRAMS = incrond incrontab

INCROND_OBJ = icd-main.o incrontab.o inotify-cxx.o usertable.o strtok.o
INCRONTAB_OBJ = ict-main.o incrontab.o inotify-cxx.o strtok.o


all:	$(PROGRAMS)

incrond:	$(INCROND_OBJ)
	$(CXX) $(LDFLAGS) -o $@ $(INCROND_OBJ)

incrontab:	$(INCRONTAB_OBJ)
	$(CXX) $(LDFLAGS) -o $@ $(INCRONTAB_OBJ)

.cpp.o:
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(PROGRAMS)
	rm -f *.o

distclean: clean

install:	all install-man
	[ -d $(PREFIX) ]
	$(INSTALL) -m 04755 -o $(USER) incrontab $(PREFIX)/bin/
	$(INSTALL) -m 0755 incrond $(PREFIX)/sbin/
	$(INSTALL) -m 0755 -o $(USER) -d $(DATADIR)

install-man:	incrontab.1 incrontab.5 incrond.8
	$(INSTALL) -m 0755 -d $(MANPATH)/man1
	$(INSTALL) -m 0755 -d $(MANPATH)/man5
	$(INSTALL) -m 0755 -d $(MANPATH)/man8
	$(INSTALL) -m 0644 incrontab.1 $(MANPATH)/man1
	$(INSTALL) -m 0644 incrontab.5 $(MANPATH)/man5
	$(INSTALL) -m 0644 incrond.8 $(MANPATH)/man8

uninstall:	uninstall-man
	[ -d $(PREFIX) ]
	rm -f $(PREFIX)/bin/incrontab
	rm -f $(PREFIX)/sbin/incrond

uninstall-man:
	rm -f $(MANPATH)/man1/incrontab.1
	rm -f $(MANPATH)/man5/incrontab.5
	rm -f $(MANPATH)/man8/incrontab.8

release:
	mkdir -p $(RELEASEDIR)
	cp -r doc $(RELEASEDIR)
	cp *.h $(RELEASEDIR)
	cp *.cpp $(RELEASEDIR)
	cp Makefile CHANGELOG COPYING LICENSE-GPL LICENSE-LGPL LICENSE-X11 README TODO VERSION $(RELEASEDIR)
	cp incrond.8 incrontab.1 incrontab.5 $(RELEASEDIR)
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
	rm -f $(RELEASE).tar.bz2
	rm -f $(RELEASE).tar.gz
	rm -f $(RELEASE).zip
	rm -f sha1.txt

.PHONY:	all clean distclean install install-man uninstall uninstall-man release release-clean

.POSIX:

icd-main.o:	icd-main.cpp inotify-cxx.h incrontab.h usertable.h incron.h
incrontab.o:	incrontab.cpp incrontab.h inotify-cxx.h strtok.h
inotify-cxx.o:	inotify-cxx.cpp inotify-cxx.h
usertable.o:	usertable.cpp usertable.h strtok.h
ict-main.o:	ict-main.cpp incrontab.h incron.h
strtok.o:	strtok.cpp strtok.h

