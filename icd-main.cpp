
/// inotify cron daemon main file
/**
 * \file icd-main.cpp
 * 
 * inotify cron system
 * 
 * Copyright (C) 2006 Lukas Jelinek, <lukas@aiken.cz>
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

/// Daemon yes/no
#define DAEMON true

/// Logging options (console as fallback, log PID)
#define INCRON_LOG_OPTS (LOG_CONS | LOG_PID)

/// Logging facility (use CRON)
#define INCRON_LOG_FACIL LOG_CRON


/// User name to user table mapping definition
typedef std::map<std::string, UserTable*> SUT_MAP;

/// User name to user table mapping table
SUT_MAP g_ut;

/// Finish program yes/no
volatile bool g_fFinish = false;

/// Pipe for notifying about dead children
int g_cldPipe[2];

// Buffer for emptying child pipe
#define CHILD_PIPE_BUF_LEN 32
char g_cldPipeBuf[CHILD_PIPE_BUF_LEN];


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

/// Checks whether an user exists and has permission to use incron.
/**
 * It searches for the given user name in the user database.
 * If it failes it returns 'false'. Otherwise it checks
 * permission files for this user (see InCronTab::CheckUser()).
 * 
 * \param[in] user user name
 * \return true = user has permission to use incron, false = otherwise
 * 
 * \sa InCronTab::CheckUser()
 */
bool check_user(const char* user)
{
  struct passwd* pw = getpwnam(user);
  if (pw == NULL)
    return false;
    
  return InCronTab::CheckUser(user);
}

/// Attempts to load all user incron tables.
/**
 * Loaded tables are registered for processing events.
 * 
 * \param[in] pIn inotify object
 * \param[in] pEd inotify event dispatcher
 * 
 * \throw InotifyException thrown if base table directory cannot be read
 */
void load_tables(Inotify* pIn, EventDispatcher* pEd) throw (InotifyException)
{
  DIR* d = opendir(INCRON_TABLE_BASE);
  if (d == NULL)
    throw InotifyException("cannot open table directory", errno);
  
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
      if (check_user(pDe->d_name)) {
        syslog(LOG_INFO, "loading table for user %s", pDe->d_name);
        UserTable* pUt = new UserTable(pIn, pEd, un);
        g_ut.insert(SUT_MAP::value_type(un, pUt));
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
  }
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
  openlog(INCRON_DAEMON_NAME, INCRON_LOG_OPTS, INCRON_LOG_FACIL);
  
  syslog(LOG_NOTICE, "starting service (version %s, built on %s %s)", INCRON_VERSION, __DATE__, __TIME__);
  
  try {
    Inotify in;
    in.SetNonBlock(true);
    
    EventDispatcher ed(&in);
    
    try {
      load_tables(&in, &ed);
    } catch (InotifyException e) {
      int err = e.GetErrorNumber();
      syslog(LOG_CRIT, "%s: (%i) %s", e.GetMessage().c_str(), err, strerror(err));
      syslog(LOG_NOTICE, "stopping service");
      closelog();
      return 1;
    }
    
    if (DAEMON)
      daemon(0, 0);
    
    prepare_pipe();
    
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    signal(SIGCHLD, on_signal);
    
    uint32_t wm = IN_CLOSE_WRITE | IN_DELETE | IN_MOVE | IN_DELETE_SELF | IN_UNMOUNT;
    InotifyWatch watch(INCRON_TABLE_BASE, wm);
    in.Add(watch);
    
    syslog(LOG_NOTICE, "ready to process filesystem events");
    
    InotifyEvent e;
    
    struct pollfd pfd[2];
    pfd[0].fd = in.GetDescriptor();
    pfd[0].events = (short) POLLIN;
    pfd[0].revents = (short) 0;
    pfd[1].fd = g_cldPipe[0];
    pfd[1].events = (short) POLLIN;
    pfd[1].revents = (short) 0;
    
    while (!g_fFinish) {
      
      int res = poll(pfd, 2, -1);
      
      if (res > 0) {
        if (pfd[1].revents & ((short) POLLIN)) {
          char c;
          while (read(pfd[1].fd, &c, 1) > 0) {}
          UserTable::FinishDone();
        }
        else {
          in.WaitForEvents(true);
        }
      }
      else if (res < 0) {
        if (errno != EINTR)
          throw InotifyException("polling failed", errno, NULL);
      }
      
      while (in.GetEvent(e)) {
        
        if (e.GetWatch() == &watch) {
          if (e.IsType(IN_DELETE_SELF) || e.IsType(IN_UNMOUNT)) {
            syslog(LOG_CRIT, "base directory destroyed, exitting");
            g_fFinish = true;
          }
          else if (!e.GetName().empty()) {
            SUT_MAP::iterator it = g_ut.find(e.GetName());
            if (it != g_ut.end()) {
              UserTable* pUt = (*it).second;
              if (e.IsType(IN_CLOSE_WRITE) || e.IsType(IN_MOVED_TO)) {
                syslog(LOG_INFO, "table for user %s changed, reloading", e.GetName().c_str());
                pUt->Dispose();
                pUt->Load();
              }
              else if (e.IsType(IN_MOVED_FROM) || e.IsType(IN_DELETE)) {
                syslog(LOG_INFO, "table for user %s destroyed, removing", e.GetName().c_str());
                delete pUt;
                g_ut.erase(it);
              }
            }
            else if (e.IsType(IN_CLOSE_WRITE) || e.IsType(IN_MOVED_TO)) {
              if (check_user(e.GetName().c_str())) {
                syslog(LOG_INFO, "table for user %s created, loading", e.GetName().c_str());
                UserTable* pUt = new UserTable(&in, &ed, e.GetName());
                g_ut.insert(SUT_MAP::value_type(e.GetName(), pUt));
                pUt->Load();
              }
            }
          }
        }
        else {
          ed.DispatchEvent(e);
        }
        
      }
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
