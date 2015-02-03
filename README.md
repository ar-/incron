inotify cron system
![alt tag](https://travis-ci.org/ar-/incron.svg)

(c) Andreas Altair Redmer, 2014, 2015
    Lukas Jelinek, 2006, 2007, 2008, 2009, 2012

1. About
2. Install a binary version
3. Obtain the source code
4. Requirements
5. How to use
6. Bugs, suggestions
7. Licensing
8. Documentation


========================================================================


1. About
This program is the "inotify cron" system. It consist of a daemon and
a table manipulator. You can use it a similar way as the regular cron.
The difference is that the inotify cron handles filesystem events
rather than time periods.

This project was kicked off by Lukas Jelinek in 2006 and then 
unfortunatally abandoned in 2012. Upstream development and 
bug-tracking/fixing continued in 2014 on GitHub:
https://github.com/ar-/incron .

2. Install a binary version
On Debian and Ubuntu based systems you can install this software with
sudo apt-get install incron

On all other Linux based systems you have to build it from source.

3. Obtain the source code
You can download the latest stable version from 
https://github.com/ar-/incron/archive/master.tar.gz

You can download older versions from 
https://github.com/ar-/incron/releases

4. Requirements
* Linux kernel 2.6.13 or later (with inotify compiled in)
* inotify headers (inotify.h, sometimes inotify-syscalls.h) installed in
  <INCLUDE_DIR>/sys. The most common place is /usr/include/sys.
* GCC 4.x compiler (probably works also with GCC 3.4, possibly with
  older versions too)

  
5. How to build
Short:

make -j8 && sudo make install

Long:
This software does not contain a standard
portable build mechanism. There is only a Makefile which may be
modified manually. On many Linux systems you need not to change
anything.

Please review the Makefile BEFORE you type 'make'. Especially
check the PREFIX and other common variables. If done you can
now build the files ('make').

The binaries must be of course installed as root.

If you want to use (after editing) the example configuration
file simply rename it from /etc/incron.conf.example to
/etc/incron.conf (you can also use -f <config> for one-time
use of a custom configuration file).

Making a release of the source tree relies on the 'VERSION' file.
The file should contain only a simple version string such as '0.5.9'
or (if you wish) something more comlex (e.g. '0.5.9-improved').
The doxygen program must be installed and its control file 'Doxygen'
created for generating the API documentation.


6. How to use
The incron daemon (incrond) must be run under root (typically from
runlevel script etc.). It loads the current user tables and hooks
them for later changes.

The incron table manipulator may be run under any regular user
since it SUIDs. For manipulation with the tables use basically
the same syntax as for the crontab program. You can import a table,
remove and edit the current table.

The user table rows have the following syntax:
 <path> <mask> <command>

Where:

  <path> is a filesystem path (currently avoid whitespaces!)
  <mask> is a symbolic (see inotify.h; use commas for separating
         symbols) or numeric mask for events
  <command> is an application or script to run on the events

The command may contain these wildcards:

  $$ - a dollar sign
  $@ - the watched filesystem path (see above)
  $# - the event-related file name
  $% - the event flags (textually)
  $& - the event flags (numerically)

The mask may additionaly contain a special symbol loopable=true which
disables events occurred during the event handling (to avoid loops).
It also may contain recursive=false to ignore sub-directories.
The mask can also be extended by dotdirs=true which will include 
dotdirectories (hidden directories and hidden files) into the search.

Example 1: You need to run program 'abc' with the full file path as
an argument every time a file is changed in /var/mail. One of
the solutions follows:

/var/mail IN_CLOSE_WRITE abc $@/$#

Example 2: You need to run program 'efg' with the full file path as
the first argument and the numeric event flags as the second one.
It have to monitor all events on files in /tmp. Here is it:

/tmp IN_ALL_EVENTS efg $@/$# $&

Since 0.4.0 also system tables are supported. They are located in
/etc/incron.d and their commands use root privileges. System tables
are intended to be changed directly (without incrontab).

Some parameters of both incrontab and incrond can be changed by
the configuration. See the example file for more information.

Example 3: You need to run program 'playmp3' with the full file path as
an argument every time a MP3 file is moved to in /home/u1/Music. One of
the solutions follows:

/home/u1/Music/*.mp3 IN_MOVED_TO playmp3 $@/$#

Example 4: You need to observe the directory /etc/ 
recursively and report every change in the syslog. One of
the solutions follows:

/etc/ IN_CLOSE_WRITE echo $@/$# | logger

Example 5: You need to observe the directory /etc/apache
but exclude the sub-directories and report every change in the syslog.
One of the solutions follows:

/etc/apache IN_CLOSE_WRITE,recursive=false echo $@/$# | logger

Example 6: You need to observe the directory /home/user1
recursively, including all the hidden sub-directories and hidden files
(dotfiles/dotdirectories) and report every change in the syslog.
One of the solutions follows:

/home/user1 IN_CLOSE_WRITE,dotdirs=true echo $@/$# | logger


7. Bugs, suggestions
incrond is currently not resistent against looping.

If you find a bug or have a suggestion how to improve the program,
please use the bug tracking system at 
https://github.com/ar-/incron/issues.


8. Licensing
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License,
version 2  (see LICENSE-GPL).

Some parts may be also covered by other licenses.
Please look into the source files for detailed information.

