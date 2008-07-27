
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

#include "inotify-cxx.h"
#include "incrontab.h"

#include "usertable.h"

#define DAEMON false

#define INCRON_APP_NAME "incrond"
#define INCRON_LOG_OPTS (LOG_CONS | LOG_PID)
#define INCRON_LOG_FACIL LOG_CRON


typedef std::map<std::string, UserTable*> SUT_MAP;


SUT_MAP g_ut;

bool g_fFinish = false;


void on_signal(int signo)
{
  g_fFinish = true;
}

void on_child(int signo)
{
  wait(NULL);
}

bool check_user(const char* user)
{
  struct passwd* pw = getpwnam(user);
  if (pw == NULL)
    return false;
    
  return InCronTab::CheckUser(user);
}

bool load_tables(Inotify* pIn, EventDispatcher* pEd)
{
  DIR* d = opendir(INCRON_TABLE_BASE);
  if (d == NULL) {
    syslog(LOG_CRIT, "cannot open table directory: %s", strerror(errno));
    return false;
  }
  
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
  return true;
}

int main(int argc, char** argv)
{
  openlog(INCRON_APP_NAME, INCRON_LOG_OPTS, INCRON_LOG_FACIL);
  
  syslog(LOG_NOTICE, "starting service");
  
  Inotify in;
  EventDispatcher ed(&in);
  
  if (!load_tables(&in, &ed)) {
    closelog();
    return 1;
  }
  
  signal(SIGTERM, on_signal);
  signal(SIGINT, on_signal);
  
  signal(SIGCHLD, on_child);
  
  if (DAEMON)
    daemon(0, 0);
  
  uint32_t wm = IN_CLOSE_WRITE | IN_DELETE | IN_MOVE | IN_DELETE_SELF | IN_UNMOUNT;
  InotifyWatch watch(INCRON_TABLE_BASE, wm);
  in.Add(watch);
  
  syslog(LOG_NOTICE, "ready to process filesystem events");
  
  InotifyEvent e;
  
  while (!g_fFinish) {
    if (in.WaitForEvents()) {
      while (in.GetEvent(e)) {
        
        std::string s;
        e.DumpTypes(s);
        //syslog(LOG_DEBUG, "EVENT: wd=%i, cookie=%u, name=%s, mask: %s", (int) e.GetDescriptor(), (unsigned) e.GetCookie(), e.GetName().c_str(), s.c_str());
        
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
  }

  syslog(LOG_NOTICE, "stopping service");
  
  closelog();
  
  return 0;
}
