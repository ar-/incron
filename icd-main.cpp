
/// inotify cron daemon main file
/**
 * \file icd-main.cpp
 * 
 * inotify cron system
 * 
 * Copyright (C) 2006, 2007 Lukas Jelinek, <lukas@aiken.cz>
 * 
 * This program is free software; you can use it, redistribute
 * it and/or modify it under the terms of the GNU General Public
 * License, version 2 (see LICENSE-GPL).
 *  
 */

#include <map>
#include <signal.h>
#include <wait.h>
#include <fcntl.h>
#include <pwd.h>
#include <dirent.h>
#include <syslog.h>
#include <errno.h>
#include <sys/poll.h>
#include <sys/stat.h>

#include "inotify-cxx.h"

#include "incron.h"
#include "incrontab.h"
#include "usertable.h"


/// Logging options (console as fallback, log PID)
#define INCRON_LOG_OPTS (LOG_CONS | LOG_PID)

/// Logging facility (use CRON)
#define INCRON_LOG_FACIL LOG_CRON

/// Help text
#define INCROND_HELP_TEXT "Usage: incrond [option]\n" \
                          "incrond - inotify cron daemon\n" \
                          "(c) Lukas Jelinek, 2006, 2007\n\n" \
                          "-h, --help             Give this help list\n" \
                          "-n, --foreground       Run on foreground (don't daemonize)\n" \
                          "-k, --kill             Terminate running instance of incrond\n" \
                          "-V, --version          Print program version\n\n" \
                          "Report bugs to <bugs@aiken.cz>"

#define INCROND_VERSION_TEXT INCRON_DAEMON_NAME " " INCRON_VERSION


/// User name to user table mapping table
SUT_MAP g_ut;

/// Finish program yes/no
volatile bool g_fFinish = false;

/// Pipe for notifying about dead children
int g_cldPipe[2];

// Buffer for emptying child pipe
#define CHILD_PIPE_BUF_LEN 32
char g_cldPipeBuf[CHILD_PIPE_BUF_LEN];

/// Daemonize true/false
bool g_daemon = true;

/// Handles a signal.
/**
 * For SIGTERM and SIGINT it sets the program finish variable.
 * For SIGCHLD it writes a character into the notification pipe
 * (this is a workaround made due to disability to reliably
 * wait for dead children).
 * 
 * \param[in] signo signal number
 */
void on_signal(int signo)
{
  switch (signo) {
    case SIGTERM:
    case SIGINT:
      g_fFinish = true;
      break;
    case SIGCHLD:
      // first empty pipe (to prevent internal buffer overflow)
      do {} while (read(g_cldPipe[0], g_cldPipeBuf, CHILD_PIPE_BUF_LEN) > 0);
      
      // now write one character
      write(g_cldPipe[1], "X", 1);
      break;
    default:;
  }
}


/// Attempts to load all user incron tables.
/**
 * Loaded tables are registered for processing events.
 * 
 * \param[in] pEd inotify event dispatcher
 * 
 * \throw InotifyException thrown if base table directory cannot be read
 */
void load_tables(EventDispatcher* pEd) throw (InotifyException)
{
  // WARNING - this function has not been optimized!!!
  
  DIR* d = opendir(INCRON_SYS_TABLE_BASE);
  if (d != NULL) {
    syslog(LOG_NOTICE, "loading system tables");
      
    struct dirent* pDe = NULL;
    while ((pDe = readdir(d)) != NULL) {
      std::string un(pDe->d_name);
      
      bool ok = pDe->d_type == DT_REG;
      if (pDe->d_type == DT_UNKNOWN) {
        struct stat st;
        if (stat(pDe->d_name, &st) == 0)
          ok = S_ISREG(st.st_mode);
      }
      
      if (ok) {
        syslog(LOG_INFO, "loading table %s", pDe->d_name);
        UserTable* pUt = new UserTable(pEd, un, true);
        g_ut.insert(SUT_MAP::value_type(std::string(INCRON_SYS_TABLE_BASE) + un, pUt));
        pUt->Load();
      }
    }
    
    closedir(d);
  }
  else {
    syslog(LOG_WARNING, "cannot open system table directory (ignoring)");
  }
  
  
  d = opendir(INCRON_USER_TABLE_BASE);
  if (d == NULL)
    throw InotifyException("cannot open user table directory", errno);
  
  syslog(LOG_NOTICE, "loading user tables");
    
  struct dirent* pDe = NULL;
  while ((pDe = readdir(d)) != NULL) {
    std::string un(pDe->d_name);
    
    bool ok = pDe->d_type == DT_REG;
    if (pDe->d_type == DT_UNKNOWN) {
      struct stat st;
      if (stat(pDe->d_name, &st) == 0)
        ok = S_ISREG(st.st_mode);
    }
    
    if (ok) {
      if (UserTable::CheckUser(pDe->d_name)) {
        syslog(LOG_INFO, "loading table for user %s", pDe->d_name);
        UserTable* pUt = new UserTable(pEd, un, false);
        g_ut.insert(SUT_MAP::value_type(std::string(INCRON_USER_TABLE_BASE) + un, pUt));
        pUt->Load();
      }
      else {
        syslog(LOG_WARNING, "table for invalid user %s found (ignored)", pDe->d_name);
      }
    }
  }
  
  closedir(d);
}

/// Prepares a 'dead/done child' notification pipe.
/**
 * This function returns no value at all and on error it
 * throws an exception.
 */
void prepare_pipe()
{
  g_cldPipe[0] = -1;
  g_cldPipe[1] = -1;
  
  if (pipe(g_cldPipe) != 0)
      throw InotifyException("cannot create notification pipe", errno, NULL);
  
  for (int i=0; i<2; i++) {
    int res = fcntl(g_cldPipe[i], F_GETFL);
    if (res == -1)
      throw InotifyException("cannot get pipe flags", errno, NULL);
    
    res |= O_NONBLOCK;
        
    if (fcntl(g_cldPipe[i], F_SETFL, res) == -1)
      throw InotifyException("cannot set pipe flags", errno, NULL);
      
    res = fcntl(g_cldPipe[i], F_GETFD);
    if (res == -1)
      throw InotifyException("cannot get pipe flags", errno, NULL);
    
    res |= FD_CLOEXEC;
        
    if (fcntl(g_cldPipe[i], F_SETFD, res) == -1)
      throw InotifyException("cannot set pipe flags", errno, NULL);
  }
}

/// Checks whether a parameter string is a specific command.
/**
 * The string is accepted if it equals either the short or long
 * form of the command.
 * 
 * \param[in] s checked string
 * \param[in] shortCmd short form of command
 * \param[in] longCmd long form of command
 * \return true = string accepted, false = otherwise
 */  
bool check_parameter(const char* s, const char* shortCmd, const char* longCmd)
{
  return strcmp(s, shortCmd)  == 0
      || strcmp(s, longCmd)   == 0;
}

/// Attempts to kill all running instances of incrond.
/**
 * It kills only instances which use the same executable image
 * as the currently running one.
 * 
 * \return true = at least one instance killed, false = otherwise
 * \attention More than one instance may be currently run simultaneously.
 */ 
bool kill_incrond()
{
  unsigned pid_self = (unsigned) getpid(); // self PID
  
  char s[PATH_MAX];
  snprintf(s, PATH_MAX, "/proc/%u/exe", pid_self);
  
  char path_self[PATH_MAX];
  ssize_t len = readlink(s, path_self, PATH_MAX-1);
  if (len <= 0)
    return false;
  path_self[len] = '\0';
  
  DIR* d = opendir("/proc");
  if (d == NULL)
    return false;
    
  bool ok = false;
    
  char path[PATH_MAX];
  struct dirent* de = NULL;
  while ((de = readdir(d)) != NULL) {
    bool to = false;
    if (de->d_type == DT_DIR)
      to = true;
    else if (de->d_type == DT_UNKNOWN) {
      // fallback way
      snprintf(s, PATH_MAX, "/proc/%s", de->d_name);
      struct stat st;
      if (stat(s, &st) == 0 && S_ISDIR(st.st_mode))
        to = true;
    }
    
    if (to) {
      unsigned pid;
      if (sscanf(de->d_name, "%u", &pid) == 1   // PID successfully retrieved
          && pid != pid_self)                   // don't do suicide!
      {
        snprintf(s, PATH_MAX, "/proc/%u/exe", pid);
        len = readlink(s, path, PATH_MAX-1);
        if (len > 0) {
          path[len] = '\0';
          if (    strcmp(path, path_self) == 0
              &&  kill((pid_t) pid, SIGTERM) == 0)
            ok = true;
        }
      }
    }
  }
  
  closedir(d);
  
  return ok;
}

/// Initializes a poll array.
/**
 * \param[in] pipefd pipe file descriptor
 * \param[in] infd inotify infrastructure file descriptor
 */
void init_poll_array(struct pollfd pfd[], int pipefd, int infd)
{
  pfd[0].fd = pipefd;
  pfd[0].events = (short) POLLIN;
  pfd[0].revents = (short) 0;
  pfd[1].fd = infd;
  pfd[1].events = (short) POLLIN;
  pfd[1].revents = (short) 0;
}


/// Main application function.
/**
 * \param[in] argc argument count
 * \param[in] argv argument array
 * \return 0 on success, 1 on error
 * 
 * \attention In daemon mode, it finishes immediately.
 */
int main(int argc, char** argv)
{
  if (argc > 2) {
    fprintf(stderr, "error: too many parameters\n");
    fprintf(stderr, "give --help or -h for more information\n");
    return 1;
  }
  
  if (argc == 2) {
    if (check_parameter(argv[1], "-h", "--help")) {
      printf("%s\n", INCROND_HELP_TEXT);
      return 0;
    }
    else if (check_parameter(argv[1], "-n", "--foreground")) {
      g_daemon = false;
    }
    else if (check_parameter(argv[1], "-k", "--kill")) {
      fprintf(stderr, "attempting to terminate a running instance of incrond...\n");
      if (kill_incrond()) {
        fprintf(stderr, "instance(s) notified, going down\n");
        return 0;
      }
      else { 
        fprintf(stderr, "error - incrond probably not running\n");
        return 1;
      }
    }
    else if (check_parameter(argv[1], "-V", "--version")) {
      printf("%s\n", INCROND_VERSION_TEXT);
      return 0;
    }
    else {
      fprintf(stderr, "error - unrecognized parameter: %s\n", argv[1]);
      return 1;
    }
  }
  
  openlog(INCRON_DAEMON_NAME, INCRON_LOG_OPTS, INCRON_LOG_FACIL);
  
  syslog(LOG_NOTICE, "starting service (version %s, built on %s %s)", INCRON_VERSION, __DATE__, __TIME__);
  
  try {
    if (g_daemon)
      daemon(0, 0);
    
    prepare_pipe();
    
    Inotify in;
    in.SetNonBlock(true);
    in.SetCloseOnExec(true);
    
    uint32_t wm = IN_CLOSE_WRITE | IN_DELETE | IN_MOVE | IN_DELETE_SELF | IN_UNMOUNT;
    InotifyWatch stw(INCRON_SYS_TABLE_BASE, wm);
    in.Add(stw);
    InotifyWatch utw(INCRON_USER_TABLE_BASE, wm);
    in.Add(utw);
    
    EventDispatcher ed(g_cldPipe[0], &in, &stw, &utw);
    
    try {
      load_tables(&ed);
    } catch (InotifyException e) {
      int err = e.GetErrorNumber();
      syslog(LOG_CRIT, "%s: (%i) %s", e.GetMessage().c_str(), err, strerror(err));
      syslog(LOG_NOTICE, "stopping service");
      closelog();
      return 1;
    }
    
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    signal(SIGCHLD, on_signal);
    
    syslog(LOG_NOTICE, "ready to process filesystem events");
    
    while (!g_fFinish) {
      
      int res = poll(ed.GetPollData(), ed.GetSize(), -1);
      
      if (res > 0) {
        if (ed.ProcessEvents())
          UserTable::FinishDone();
      }
      else if (res < 0) {
        if (errno != EINTR)
          throw InotifyException("polling failed", errno, NULL);
      }
      /*
      
      while (in.GetEvent(e)) {
        
        if (e.GetWatch() == &stw) {
          if (e.IsType(IN_DELETE_SELF) || e.IsType(IN_UNMOUNT)) {
            syslog(LOG_CRIT, "base directory destroyed, exitting");
            g_fFinish = true;
          }
          else if (!e.GetName().empty()) {
            SUT_MAP::iterator it = g_ut.find(stw.GetPath() + e.GetName());
            if (it != g_ut.end()) {
              UserTable* pUt = (*it).second;
              if (e.IsType(IN_CLOSE_WRITE) || e.IsType(IN_MOVED_TO)) {
                syslog(LOG_INFO, "system table %s changed, reloading", e.GetName().c_str());
                pUt->Dispose();
                pUt->Load();
              }
              else if (e.IsType(IN_MOVED_FROM) || e.IsType(IN_DELETE)) {
                syslog(LOG_INFO, "system table %s destroyed, removing", e.GetName().c_str());
                delete pUt;
                g_ut.erase(it);
              }
            }
            else if (e.IsType(IN_CLOSE_WRITE) || e.IsType(IN_MOVED_TO)) {
              syslog(LOG_INFO, "system table %s created, loading", e.GetName().c_str());
              UserTable* pUt = new UserTable(&in, &ed, e.GetName(), true);
              g_ut.insert(SUT_MAP::value_type(std::string(INCRON_SYS_TABLE_BASE) + e.GetName(), pUt));
              pUt->Load();
            }
          }
        }
        else if (e.GetWatch() == &utw) {
          if (e.IsType(IN_DELETE_SELF) || e.IsType(IN_UNMOUNT)) {
            syslog(LOG_CRIT, "base directory destroyed, exitting");
            g_fFinish = true;
          }
          else if (!e.GetName().empty()) {
            SUT_MAP::iterator it = g_ut.find(e.GetWatch()->GetPath() + e.GetName());
            if (it != g_ut.end()) {
              UserTable* pUt = (*it).second;
              if (e.IsType(IN_CLOSE_WRITE) || e.IsType(IN_MOVED_TO)) {
                syslog(LOG_INFO, "table for user %s changed, reloading", e.GetName().c_str());
                pUt->Dispose();
                pUt->Load();
              }
              else if (e.IsType(IN_MOVED_FROM) || e.IsType(IN_DELETE)) {
                syslog(LOG_INFO, "table for user %s destroyed, removing",  e.GetName().c_str());
                delete pUt;
                g_ut.erase(it);
              }
            }
            else if (e.IsType(IN_CLOSE_WRITE) || e.IsType(IN_MOVED_TO)) {
              if (check_user(e.GetName().c_str())) {
                syslog(LOG_INFO, "table for user %s created, loading", e.GetName().c_str());
                UserTable* pUt = new UserTable(&in, &ed, e.GetName(), false);
                g_ut.insert(SUT_MAP::value_type(std::string(INCRON_USER_TABLE_BASE) + e.GetName(), pUt));
                pUt->Load();
              }
            }
          }
        }
        else {
          ed.DispatchEvent(e);
        }
        
      }
      
      */
    }
    
    if (g_cldPipe[0] != -1)
      close(g_cldPipe[0]);
    if (g_cldPipe[1] != -1)
      close(g_cldPipe[1]);
  } catch (InotifyException e) {
    int err = e.GetErrorNumber();
    syslog(LOG_CRIT, "*** unhandled exception occurred ***");
    syslog(LOG_CRIT, "  %s", e.GetMessage().c_str());
    syslog(LOG_CRIT, "  error: (%i) %s", err, strerror(err));
  }

  syslog(LOG_NOTICE, "stopping service");
  
  closelog();
  
  return 0;
}
