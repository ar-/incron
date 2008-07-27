
PREFIX = /usr/local
DATADIR = /var/spool/incron

USER = incron

CXX = g++
INSTALL = install

OPTIMIZE = -O2
DEBUG = -g0
WARNINGS = -Wall

CPPFLAGS = 
CXXFLAGS = $(OPTIMIZE) $(DEBUG) $(WARNINGS)
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

install:	all
	[ -d $(PREFIX) ]
	useradd -M -s /sbin/nologin $(USER)
	$(INSTALL) -m 04755 -o $(USER) incrontab $(PREFIX)/bin/
	$(INSTALL) -m 0755 incrond $(PREFIX)/sbin/
	$(INSTALL) -m 0755 -o $(USER) -d $(DATADIR)

uninstall:
	[ -d $(PREFIX) ]
	rm -f $(PREFIX)/bin/incrontab
	rm -f $(PREFIX)/sbin/incrond
	userdel $(USER)


.PHONY:	all clean distclean install uninstall

.POSIX:

icd-main.o:	icd-main.cpp inotify-cxx.h incrontab.h usertable.h
incrontab.o:	incrontab.cpp incrontab.h inotify-cxx.h strtok.h
inotify-cxx.o:	inotify-cxx.cpp inotify-cxx.h
usertable.o:	usertable.cpp usertable.h strtok.h
ict-main.o:	ict-main.cpp incrontab.h
strtok.o:	strtok.cpp strtok.h

