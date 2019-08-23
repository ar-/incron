
/// inotify cron daemon user tables implementation
/**
 * \file usertable.cpp
 *
 * inotify cron system
 *
 * Copyright (C) 2006, 2007, 2008, 2012 Lukas Jelinek, <lukas@aiken.cz>
 * Copyright (C) 2014, 2015 Andreas Altair Redmer, <altair.ibn.la.ahad.sy@gmail.com>
 *
 * This program is free software; you can use it, redistribute
 * it and/or modify it under the terms of the GNU General Public
 * License, version 2 (see LICENSE-GPL).
 *
 * Credits:
 *   David Santinoli (supplementary groups)
 *   Boris Lechner (spaces in event-related file names)
 *   Christian Ruppert (new include to build with GCC 4.4+)
 *
 */


#include <pwd.h>
#include <syslog.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
#include <grp.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>
#include <algorithm>

#include "usertable.h"
#include "incroncfg.h"
#include "incrontab.h"
#include "executor.h"

#ifdef IN_DONT_FOLLOW
#define DONT_FOLLOW(mask) InotifyEvent::IsType(mask, IN_DONT_FOLLOW)
#else // IN_DONT_FOLLOW
#define DONT_FOLLOW(mask) (false)
#endif // IN_DONT_FOLLOW

// this is not enough, but...
#define DEFAULT_PATH "/usr/local/bin:/usr/bin:/bin:/usr/X11R6/bin"


PROC_MAP UserTable::s_procMap;

extern volatile bool g_fFinish;
extern SUT_MAP g_ut;


#ifdef LOOPER
void on_proc_done(InotifyWatch* pW)
{
  pW->SetEnabled(true);
}
#endif


EventDispatcher::EventDispatcher(int iPipeFd, Inotify* pIn, InotifyWatch* pSys, InotifyWatch* pUser)
{
  m_iPipeFd = iPipeFd;
  m_iMgmtFd = pIn->GetDescriptor();
  m_pIn = pIn;
  m_pSys = pSys;
  m_pUser = pUser;
  m_size = 0;
  m_pPoll = NULL;
}

EventDispatcher::~EventDispatcher()
{
  if (m_pPoll != NULL)
    delete[] m_pPoll;
}

bool EventDispatcher::ProcessEvents()
{
  // consume pipe events if any (and report back)
  bool pipe = (m_pPoll[0].revents & POLLIN);
  if (pipe) {
    char c;
    while (read(m_pPoll[0].fd, &c, 1) > 0) {}
    m_pPoll[0].revents = 0;
  }

  // process table management events if any
  if (m_pPoll[1].revents & POLLIN) {
    ProcessMgmtEvents();
    m_pPoll[1].revents = 0;
  }

  InotifyEvent evt;

  for (size_t i=2; i<m_size; i++) {

    // process events if occurred
    if (m_pPoll[i].revents & POLLIN) {
      FDUT_MAP::iterator it = m_maps.find(m_pPoll[i].fd);
      if (it != m_maps.end()) {
        Inotify* pIn = ((*it).second)->GetInotify();
        pIn->WaitForEvents(true);

        // process events for this object
        while (pIn->GetEvent(evt)) {
          ((*it).second)->OnEvent(evt);
        }
      }
      m_pPoll[i].revents = 0;
    }
  }

  return pipe;
}

void EventDispatcher::Register(UserTable* pTab)
{
  if (pTab != NULL) {
    Inotify* pIn = pTab->GetInotify();
    if (pIn != NULL) {
      int fd = pIn->GetDescriptor();
      if (fd != -1) {
        m_maps.insert(FDUT_MAP::value_type(fd, pTab));
        Rebuild();
      }
    }
  }
}

void EventDispatcher::Unregister(UserTable* pTab)
{
  FDUT_MAP::iterator it = m_maps.find(pTab->GetInotify()->GetDescriptor());
  if (it != m_maps.end()) {
    m_maps.erase(it);
    Rebuild();
  }
}

void EventDispatcher::Rebuild()
{
  // delete old data if exists
  if (m_pPoll != NULL)
    delete[] m_pPoll;

  // allocate memory
  m_size = m_maps.size() + 2;
  m_pPoll = new struct pollfd[m_size];

  // add pipe descriptor
  m_pPoll[0].fd = m_iPipeFd;
  m_pPoll[0].events = POLLIN;
  m_pPoll[0].revents = 0;

  // add table management descriptor
  m_pPoll[1].fd = m_iMgmtFd;
  m_pPoll[1].events = POLLIN;
  m_pPoll[1].revents = 0;

  // add all inotify descriptors
  FDUT_MAP::iterator it = m_maps.begin();
  for (size_t i=2; i<m_size; i++, it++) {
    m_pPoll[i].fd = (*it).first;
    m_pPoll[i].events = POLLIN;
    m_pPoll[i].revents = 0;
  }
}

void EventDispatcher::ProcessMgmtEvents()
{
  m_pIn->WaitForEvents(true);

  InotifyEvent e;

  while (m_pIn->GetEvent(e)) {
    if (e.GetWatch() == m_pSys) {
      if (e.IsType(IN_DELETE_SELF) || e.IsType(IN_UNMOUNT)) {
        syslog(LOG_CRIT, "base directory destroyed, exitting");
        g_fFinish = true;
      }
      else if (!e.GetName().empty()) {
		//ignore all names that start with a dot
		const std::string eventName = e.GetName();
		if (eventName[0] == '.') 
			continue;
		  
		//start the actual process here
        SUT_MAP::iterator it = g_ut.find(IncronCfg::BuildPath(m_pSys->GetPath(), e.GetName()));
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
          UserTable* pUt = new UserTable(this, e.GetName(), true);
          g_ut.insert(SUT_MAP::value_type(IncronTab::GetSystemTablePath(e.GetName()), pUt));
          pUt->Load();
        }
      }
    }
    else if (e.GetWatch() == m_pUser) {
      if (e.IsType(IN_DELETE_SELF) || e.IsType(IN_UNMOUNT)) {
        syslog(LOG_CRIT, "base directory destroyed, exitting");
        g_fFinish = true;
      }
      else if (!e.GetName().empty()) {
        SUT_MAP::iterator it = g_ut.find(IncronCfg::BuildPath(m_pUser->GetPath(), e.GetName()));
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
          if (UserTable::CheckUser(e.GetName().c_str())) {
            syslog(LOG_INFO, "table for user %s created, loading", e.GetName().c_str());
            UserTable* pUt = new UserTable(this, e.GetName(), false);
            g_ut.insert(SUT_MAP::value_type(IncronTab::GetUserTablePath(e.GetName()), pUt));
            pUt->Load();
          }
        }
      }
    }
  }
}




UserTable::UserTable(EventDispatcher* pEd, const std::string& rUser, bool fSysTable)
: m_user(rUser),
  m_fSysTable(fSysTable)
{
  m_pEd = pEd;

  m_in.SetNonBlock(true);
  m_in.SetCloseOnExec(true);
}

UserTable::~UserTable()
{
  Dispose();
}

void UserTable::Load()
{
  m_tab.Load(m_fSysTable
      ? IncronTab::GetSystemTablePath(m_user)
      : IncronTab::GetUserTablePath(m_user));

  int cnt = m_tab.GetCount();
  
  // add all subdirectories (recursively) as new tab entries with same events
  for (int i=0; i<cnt; i++) {
    IncronTabEntry& rE = m_tab.GetEntry(i);
    
    // skip if recursion is not wanted by user
    if (rE.IsNoRecursion())
		continue;
    
    std::vector<std::string> ssvec = Executor::getSubDirVec (rE.GetPath(),rE.IsDotDirs());
	if (rE.GetPath().find("*") != std::string::npos) 
	{
		std::vector<std::string> allfilesvec = Executor::getAllFilesByDescriptor (rE.GetPath(),rE.IsDotDirs());
		
		// add to ssvec but without duplicates - in case user defined a dirctory with subdirectories via star descriptor
		for (unsigned int j=0; j<allfilesvec.size(); j++) 
		{
			if(std::find(ssvec.begin(), ssvec.end(), allfilesvec[j]) == ssvec.end()) 
			{
				ssvec.push_back(allfilesvec[j]);
			}
		}
	}    
    for (unsigned int j=0; j<ssvec.size(); j++) {
	  std::string subDir = ssvec[j];
	  
	  // skip if the subdir is the dir itself
	  if (rE.GetPath() == subDir )
		continue;
		
	  IncronTabEntry ite = IncronTabEntry(subDir, rE.GetMask(), rE.GetCmd());
	  m_tab.Add(ite);
    }
  }
  
  // recount table elements too iterate over all of them
  cnt = m_tab.GetCount();
  for (int i=0; i<cnt; i++) {
    IncronTabEntry& rE = m_tab.GetEntry(i);
    // skip the wildcard selector, as they have been replace by the actual files
	if (rE.GetPath().find("*") != std::string::npos) 
		continue;
	AddTabEntry(rE);
  }
  
  m_pEd->Register(this);
}

void UserTable::AddTabEntry(IncronTabEntry& rE)
{
    //syslog(LOG_INFO, "registering inotify for (%s)", rE.GetPath().c_str()); // TODO is this log spamming too much ?
    InotifyWatch* pW = new InotifyWatch(rE.GetPath(), rE.GetMask());

    // warning only - permissions may change later
    if (!(m_fSysTable || MayAccess(rE.GetPath(), DONT_FOLLOW(rE.GetMask()))))
      syslog(LOG_WARNING, "access denied on %s - events will be discarded silently", rE.GetPath().c_str());

    try {
      m_in.Add(pW);
      m_map.insert(IWCE_MAP::value_type(pW, &rE));
    } catch (InotifyException e) {
      if (m_fSysTable)
        syslog(LOG_ERR, "cannot create watch for system table %s: (%i) %s", m_user.c_str(), e.GetErrorNumber(), strerror(e.GetErrorNumber()));
      else
        syslog(LOG_ERR, "cannot create watch for user %s: (%i) %s", m_user.c_str(), e.GetErrorNumber(), strerror(e.GetErrorNumber()));
      delete pW;
    }	
}


void UserTable::Dispose()
{
  m_pEd->Unregister(this);

  IWCE_MAP::iterator it = m_map.begin();
  while (it != m_map.end()) {
    InotifyWatch* pW = (*it).first;
    m_in.Remove(pW);

    PROC_MAP::iterator it2 = s_procMap.begin();
    while (it2 != s_procMap.end()) {
      if ((*it2).second.pWatch == pW) {
        PROC_MAP::iterator it3 = it2;
        it2++;
        s_procMap.erase(it3);
      }
      else {
        it2++;
      }
    }

    delete pW;
    it++;
  }

  m_map.clear();
}

void UserTable::OnEvent(InotifyEvent& rEvt)
{
  InotifyWatch* pW = rEvt.GetWatch();
  IncronTabEntry* pE = FindEntry(pW);

  // no entry found - this shouldn't occur
  if (pE == NULL)
    return;

  // discard event if user has no access rights to watch path
  if (!(m_fSysTable || MayAccess(pW->GetPath(), DONT_FOLLOW(rEvt.GetMask()))))
    return;
    
  //#if 0
  // log output for each dir + file + event
  std::string events;
  rEvt.DumpTypes(events);
  syslog(LOG_INFO, "PATH (%s) FILE (%s) EVENT (%s)", pW->GetPath().c_str() , IncronTabEntry::GetSafePath(rEvt.GetName()).c_str() , events.c_str());
  //#endif
  
  // add new watch for newly created subdirs
  if ( rEvt.IsType(IN_ISDIR) && (rEvt.IsType(IN_CREATE) || rEvt.IsType(IN_MOVED_TO)) )
  {
	Dispose();
	sleep (1);
	Load();
  // this is the fast way of registering new subsirs, but it 
  // misses new sub-sub dirs if they are created too fast in a row
  // eg by : mkdir -p /tmp/a/b/c/d/e
  // so the reload is for now the better way to go
  // the complete reload also happens if the incrontab file changes
  /*
  m_pEd->Unregister(this);
	std::string * pECmd = new std::string(pE->GetCmd().c_str());
	IncronTabEntry * newEntry = new IncronTabEntry(completeFile, pE->GetMask(),  *pECmd);
	m_tab.Add(*newEntry);
	AddTabEntry(*newEntry);
  m_pEd->Register(this);
  */ 
  }

  std::string cmd;
  const std::string& cs = pE->GetCmd();
  size_t pos = 0;
  size_t oldpos = 0;
  size_t len = cs.length();
  while ((pos = cs.find('$', oldpos)) != std::string::npos) {
    if (pos < len - 1) {
      size_t px = pos + 1;
      if (cs[px] == '$') {
        cmd.append(cs.substr(oldpos, pos-oldpos+1));
        oldpos = pos + 2;
      }
      else {
        cmd.append(cs.substr(oldpos, pos-oldpos));
        if (cs[px] == '@') {          // base path
          cmd.append(IncronTabEntry::GetSafePath(pW->GetPath()));
          oldpos = pos + 2;
        }
        else if (cs[px] == '#') {     // file name
          cmd.append(IncronTabEntry::GetSafePath(rEvt.GetName()));
          oldpos = pos + 2;
        }
        else if (cs[px] == '%') {     // mask symbols
          std::string s;
          rEvt.DumpTypes(s);
          cmd.append(s);
          oldpos = pos + 2;
        }
        else if (cs[px] == '&') {     // numeric mask
          char* s;
#pragma GCC diagnostic ignored "-Wunused-result"  
          asprintf(&s, "%u", (unsigned) rEvt.GetMask());
#pragma GCC diagnostic warning "-Wunused-result"
          cmd.append(s);
          free(s);
          oldpos = pos + 2;
        }
        else {
          oldpos = pos + 1;
        }
      }
    }
    else {
      cmd.append(cs.substr(oldpos, pos-oldpos));
      oldpos = pos + 1;
    }
  }
  cmd.append(cs.substr(oldpos));

  if (m_fSysTable)
    syslog(LOG_INFO, "(system::%s) CMD (%s)", m_user.c_str(), cmd.c_str());
  else
    syslog(LOG_INFO, "(%s) CMD (%s)", m_user.c_str(), cmd.c_str());
    
#ifdef LOOPER
  if (pE->IsNoLoop())
    pW->SetEnabled(false);
#endif

  pid_t pid = fork();
  if (pid == 0) {

    // for system table
    if (m_fSysTable) {
      execl("/bin/sh", "sh", "-c", cmd.c_str(), (char *) NULL);
      syslog(LOG_ERR, "cannot exec process: %s", strerror(errno));
      _exit(1);
    }
    else {
      // for user table
      RunAsUser(cmd);
#ifdef LOOPER
	  if (pE->IsNoLoop())
		pW->SetEnabled(true);
#endif
    }
  }
  else if (pid > 0) {
#ifdef LOOPER
    ProcData_t pd;
    if (pE->IsNoLoop()) {
      pd.onDone = on_proc_done;
      pd.pWatch = pW;
    }
    else {
      pd.onDone = NULL;
      pd.pWatch = NULL;
    }

    s_procMap.insert(PROC_MAP::value_type(pid, pd));
#endif
  }
  else {
#ifdef LOOPER
    if (pE->IsNoLoop())
      pW->SetEnabled(true);
#endif

    syslog(LOG_ERR, "cannot fork process: %s", strerror(errno));
  }

}

IncronTabEntry* UserTable::FindEntry(InotifyWatch* pWatch)
{
  IWCE_MAP::iterator it = m_map.find(pWatch);
  if (it == m_map.end())
    return NULL;

  return (*it).second;
}

bool UserTable::MayAccess(const std::string& rPath, bool fNoFollow) const
{
  // first, retrieve file permissions
  struct stat st;
  int res = fNoFollow
      ? lstat(rPath.c_str(), &st) // don't follow symlink
      : stat(rPath.c_str(), &st);
  if (res != 0)
    return false; // retrieving permissions failed

  // file accessible to everyone
  if (st.st_mode & S_IRWXO)
    return true;

  // retrieve user data
  struct passwd* pwd = getpwnam(m_user.c_str());

  // root may always access
  if (pwd->pw_uid == 0)
    return true;

  // file accessible to group
  if (st.st_mode & S_IRWXG) {

    // user's primary group
    if (pwd != NULL && pwd->pw_gid == st.st_gid)
        return true;

    // now check group database
    struct group *gr = getgrgid(st.st_gid);
    if (gr != NULL) {
      int pos = 0;
      const char* un;
      while ((un = gr->gr_mem[pos]) != NULL) {
        if (strcmp(un, m_user.c_str()) == 0)
          return true;
        pos++;
      }
    }
  }

  // file accessible to owner
  if (st.st_mode & S_IRWXU) {
    if (pwd != NULL && pwd->pw_uid == st.st_uid)
      return true;
  }

  return false; // no access right found
}

#ifndef __linux__
static int
clearenv(void)
{
  extern char **environ;

  environ[0] = NULL;
  return 0;
}
#endif

void UserTable::RunAsUser(std::string cmd) const
{
  struct passwd* pwd = getpwnam(m_user.c_str());
  if (    pwd == NULL                 // check query result
      ||  setgid(pwd->pw_gid) != 0    // check GID
      ||  initgroups(m_user.c_str(),pwd->pw_gid) != 0 // check supplementary groups
      ||  setuid(pwd->pw_uid) != 0)    // check UID
  {
    goto failed;
  }

  if (pwd->pw_uid != 0) {
    if (clearenv() != 0)
      goto failed;

    if (    setenv("LOGNAME",   pwd->pw_name,   1) != 0
        ||  setenv("USER",      pwd->pw_name,   1) != 0
        ||  setenv("USERNAME",  pwd->pw_name,   1) != 0
        ||  setenv("HOME",      pwd->pw_dir,    1) != 0
        ||  setenv("SHELL",     pwd->pw_shell,  1) != 0
        ||  setenv("PATH",      DEFAULT_PATH,   1) != 0)
    {
      goto failed;
    }
  }
  
  execlp("/bin/bash","/bin/bash", "-c", cmd.c_str(), (char *)NULL);

failed:

  syslog(LOG_ERR, "cannot exec process: %s", strerror(errno));
  _exit(1);
}

