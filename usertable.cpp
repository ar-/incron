
/// inotify cron daemon user tables implementation
/**
 * \file usertable.cpp
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


#include <pwd.h>
#include <syslog.h>
#include <errno.h>
#include <sys/wait.h>

#include "usertable.h"


PROC_LIST UserTable::s_procList;


void on_proc_done(InotifyWatch* pW)
{
  pW->SetEnabled(true);
}


EventDispatcher::EventDispatcher(Inotify* pIn)
{
 m_pIn = pIn; 
}

void EventDispatcher::DispatchEvent(InotifyEvent& rEvt)
{
  if (m_pIn == NULL)
    return;
    
  InotifyWatch* pW = rEvt.GetWatch();
  if (pW == NULL)
    return;
    
  UserTable* pT = FindTable(pW);
  if (pT == NULL)
    return;
    
  pT->OnEvent(rEvt);
}
  
void EventDispatcher::Register(InotifyWatch* pWatch, UserTable* pTab)
{
  if (pWatch != NULL && pTab != NULL)
    m_maps.insert(IWUT_MAP::value_type(pWatch, pTab));
}
  
void EventDispatcher::Unregister(InotifyWatch* pWatch)
{
  IWUT_MAP::iterator it = m_maps.find(pWatch);
  if (it == m_maps.end())
    return;
    
  m_maps.erase(it);
}
  
void EventDispatcher::UnregisterAll(UserTable* pTab)
{
  IWUT_MAP::iterator it = m_maps.begin();
  while (it != m_maps.end()) {
    if ((*it).second == pTab) {
      IWUT_MAP::iterator it2 = it;
      it++;
      m_maps.erase(it2);
    }
    else {
      it++;
    }
  }
}

UserTable* EventDispatcher::FindTable(InotifyWatch* pW)
{
  IWUT_MAP::iterator it = m_maps.find(pW);
  if (it == m_maps.end())
    return NULL;
    
  return (*it).second;
}




UserTable::UserTable(Inotify* pIn, EventDispatcher* pEd, const std::string& rUser)
: m_user(rUser)
{
  m_pIn = pIn;
  m_pEd = pEd;
}

UserTable::~UserTable()
{
  Dispose();
}
  
void UserTable::Load()
{
  m_tab.Load(InCronTab::GetUserTablePath(m_user));
  
  int cnt = m_tab.GetCount();
  for (int i=0; i<cnt; i++) {
    InCronTabEntry& rE = m_tab.GetEntry(i);
    InotifyWatch* pW = new InotifyWatch(rE.GetPath(), rE.GetMask());
    try {
      m_pIn->Add(pW);
      m_pEd->Register(pW, this);
      m_map.insert(IWCE_MAP::value_type(pW, &rE));
    } catch (InotifyException e) {
      syslog(LOG_ERR, "cannot create watch for user %s", m_user.c_str());
      delete pW;
    }
  }
}

void UserTable::Dispose()
{
  IWCE_MAP::iterator it = m_map.begin();
  while (it != m_map.end()) {
    InotifyWatch* pW = (*it).first;
    m_pEd->Unregister(pW);
    m_pIn->Remove(pW);
    delete pW;
    it++;
  }
  
  m_map.clear();
}
  
void UserTable::OnEvent(InotifyEvent& rEvt)
{
  InotifyWatch* pW = rEvt.GetWatch();
  InCronTabEntry* pE = FindEntry(pW);
  
  if (pE == NULL)
    return;
  
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
      else if (cs[px] == '@') {
        cmd.append(cs.substr(oldpos, pos-oldpos));
        cmd.append(pW->GetPath());
        oldpos = pos + 2;
      }
      else if (cs[px] == '#') {
        cmd.append(cs.substr(oldpos, pos-oldpos));
        cmd.append(rEvt.GetName());
        oldpos = pos + 2;
      }
      else {
        cmd.append(cs.substr(oldpos, pos-oldpos));
        oldpos = pos + 1;
      }
    }
    else {
      cmd.append(cs.substr(oldpos, pos-oldpos));
      oldpos = pos + 1;
    }
  }    
  cmd.append(cs.substr(oldpos));
  
  int argc;
  char** argv;
  if (!PrepareArgs(cmd, argc, argv)) {
    syslog(LOG_ERR, "cannot prepare command arguments");
    return;
  }
  
  syslog(LOG_INFO, "(%s) CMD (%s)", m_user.c_str(), cmd.c_str());
  
  if (pE->IsNoLoop())
    pW->SetEnabled(false);
  
  ProcData_t pd;
  pd.pid = fork();
  if (pd.pid == 0) {
    
    struct passwd* pwd = getpwnam(m_user.c_str());
    if (pwd == NULL)
      _exit(1);
      
    if (setuid(pwd->pw_uid) != 0)
      _exit(1);
    
    if (execvp(argv[0], argv) != 0) {
      _exit(1);
    }
  }
  else if (pd.pid > 0) {
    if (pE->IsNoLoop()) {
      pd.onDone = on_proc_done;
      pd.pWatch = pW;
    }
    else {
      pd.onDone = NULL;
      pd.pWatch = NULL;
    }
    
    s_procList.push_back(pd);
  }
  else {
    if (pE->IsNoLoop())
      pW->SetEnabled(true);
      
    syslog(LOG_ERR, "cannot fork process: %s", strerror(errno));
  }
}

InCronTabEntry* UserTable::FindEntry(InotifyWatch* pWatch)
{
  IWCE_MAP::iterator it = m_map.find(pWatch);
  if (it == m_map.end())
    return NULL;
    
  return (*it).second;
}

bool UserTable::PrepareArgs(const std::string& rCmd, int& argc, char**& argv)
{
  if (rCmd.empty())
    return false;
    
  StringTokenizer tok(rCmd, ' ');
  std::deque<std::string> args;
  while (tok.HasMoreTokens()) {
    args.push_back(tok.GetNextToken());
  }
  
  if (args.empty())
    return false;
  
  argc = (int) args.size();
  argv = new char*[argc+1];
  argv[argc] = NULL;
  
  for (int i=0; i<argc; i++) {
    const std::string& s = args[i];
    size_t len = s.length();
    argv[i] = new char[len+1];
    strcpy(argv[i], s.c_str());
  }
  
  return true;
}

void UserTable::FinishDone()
{
  PROC_LIST::iterator it = s_procList.begin();
  while (it != s_procList.end()) {
    ProcData_t& pd = *it;
    int status = 0;
    int res = waitpid(pd.pid, &status, WNOHANG);
    if (res == pd.pid && (WIFEXITED(status) || WIFSIGNALED(status))) {
      if (pd.onDone != NULL)
        (*pd.onDone)(pd.pWatch);
      it = s_procList.erase(it);
    }
    else {
      it++;
    }
  }  
}


