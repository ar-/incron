
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
#include <pwd.h>
#include <dirent.h>
#include <syslog.h>
#include <errno.h>
#include <sys/poll.h>

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


/// Handles a signal.
/**
 * For SIGTERM and SIGINT it sets the program finish variable.
 * 
 * \param[in] signo signal number
 */
void on_signal(int signo)
{
  if (signo == SIGTERM || signo == SIGINT)
    g_fFinish = true;
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
    if (pDe->d_type == DT_REG && un != "." && un != "..") {
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
    
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    signal(SIGCHLD, on_signal);
    
    if (DAEMON)
      daemon(0, 0);
    
    uint32_t wm = IN_CLOSE_WRITE | IN_DELETE | IN_MOVE | IN_DELETE_SELF | IN_UNMOUNT;
    InotifyWatch watch(INCRON_TABLE_BASE, wm);
    in.Add(watch);
    
    syslog(LOG_NOTICE, "ready to process filesystem events");
    
    InotifyEvent e;
    
    struct pollfd pfd;
    pfd.fd = in.GetDescriptor();
    pfd.events = (short) POLLIN;
    pfd.revents = (short) 0;
    
    while (!g_fFinish) {
      
      int res = poll(&pfd, 1, -1);
      if (res > 0) {
        in.WaitForEvents(true);
      }
      else if (res < 0) {
        if (errno != EINTR)
          throw InotifyException("polling failed", errno, NULL);
      }
      
      UserTable::FinishDone();
      
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
