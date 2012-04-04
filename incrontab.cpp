
/// inotify cron table manipulator classes implementation
/**
 * \file incrontab.cpp
 * 
 * inotify cron system
 * 
 * Copyright (C) 2006, 2007, 2008, 2012 Lukas Jelinek, <lukas@aiken.cz>
 * 
 * This program is free software; you can use it, redistribute
 * it and/or modify it under the terms of the GNU General Public
 * License, version 2 (see LICENSE-GPL).
 *  
 * Credits:
 *   Christian Ruppert (new include to build with GCC 4.4+)
 * 
 */


#include <sstream>
#include <cstdio>
#include <errno.h>

#include "inotify-cxx.h"

#include "incrontab.h"
#include "incroncfg.h"

/*
/// Allowed users
#define INCRON_ALLOW_PATH "/etc/incron.allow"

/// Denied users
#define INCRON_DENY_PATH "/etc/incron.deny"
*/

/*
 * ALLOW/DENY SEMANTICS
 * 
 * If /etc/incron.allow exists ONLY users contained here
 * are allowed to use incron.
 * 
 * Otherwise, if /etc/incron.deny exists only user NOT
 * contained here are allowed to use incron.
 * 
 * Otherwise all users may use incron.
 * 
 */



IncronTabEntry::IncronTabEntry()
: m_uMask(0),
  m_fNoLoop(false)
{
  
}

IncronTabEntry::IncronTabEntry(const std::string& rPath, uint32_t uMask, const std::string& rCmd)
: m_path(rPath),
  m_uMask(uMask),
  m_cmd(rCmd)
{
  
}

std::string IncronTabEntry::ToString() const
{
  std::ostringstream ss;
  
  std::string m;
  
  InotifyEvent::DumpTypes(m_uMask, m);
  if (m.empty()) {
    m = m_fNoLoop ? "IN_NO_LOOP" : "0";
  }
  else {
    if (m_fNoLoop)
      m.append(",IN_NO_LOOP");
  }
  
  ss << GetSafePath(m_path) << " " << m << " " << m_cmd;
  return ss.str();
}

bool IncronTabEntry::Parse(const std::string& rStr, IncronTabEntry& rEntry)
{
  unsigned long u;
  std::string s1, s2, s3;
  
  StringTokenizer tok(rStr, ' ', '\\');
  if (!tok.HasMoreTokens())
    return false;
    
  s1 = tok.GetNextToken(true);
  if (!tok.HasMoreTokens())
    return false;
    
  s2 = tok.GetNextToken(true);
  if (!tok.HasMoreTokens())
    return false;
  
  tok.SetNoPrefix();
  s3 = tok.GetRemainder();
  SIZE len = s3.length();
  if (len > 0 && s3[len-1] == '\n')
    s3.resize(len-1);
  
  rEntry.m_path = s1;
  rEntry.m_cmd = s3;
  rEntry.m_uMask = 0;
  rEntry.m_fNoLoop = false;
  
  if (sscanf(s2.c_str(), "%lu", &u) == 1) {
    rEntry.m_uMask = (uint32_t) u;
  }
  else {
    StringTokenizer tok(s2);
    while (tok.HasMoreTokens()) {
      std::string s(tok.GetNextToken());
      if (s == "IN_NO_LOOP")
        rEntry.m_fNoLoop = true;
      else
        rEntry.m_uMask |= InotifyEvent::GetMaskByName(s);
    }
  }
  
  return true;
}

std::string IncronTabEntry::GetSafePath(const std::string& rPath)
{
  std::ostringstream stream;
  
  SIZE len = rPath.length();
  for (SIZE i = 0; i < len; i++) {
    if (rPath[i] == ' ') {
      stream << "\\ ";
    }
    else if (rPath[i] == '\\') {
      stream << "\\\\";
    }
    else {
      stream << rPath[i];
    }
  }
  
  return stream.str();
}

bool IncronTab::Load(const std::string& rPath)
{
  m_tab.clear();
  
  FILE* f = fopen(rPath.c_str(), "r");
  if (f == NULL)
    return false;
  
  char s[1000];
  IncronTabEntry e;
  while (fgets(s, 1000, f) != NULL) {
    if (IncronTabEntry::Parse(s, e)) {
      m_tab.push_back(e);
    }
  }
  
  fclose(f);
  
  return true;
}

bool IncronTab::Save(const std::string& rPath)
{
  FILE* f = fopen(rPath.c_str(), "w");
  if (f == NULL)
    return false;
  
  std::deque<IncronTabEntry>::iterator it = m_tab.begin();
  while (it != m_tab.end()) {
    fputs((*it).ToString().c_str(), f);
    fputs("\n", f);
    it++;
  }
  
  fclose(f);
  
  return true;
}

bool IncronTab::CheckUser(const std::string& rUser)
{
  char s[100], u[100];
  
  std::string path;
  if (!IncronCfg::GetValue("allowed_users", path))
    throw InotifyException("configuration is corrupted", EINVAL);
  
  FILE* f = fopen(path.c_str(), "r");
  if (f == NULL) {
    if (errno == ENOENT) {
      if (!IncronCfg::GetValue("denied_users", path))
        throw InotifyException("configuration is corrupted", EINVAL);
      
      f = fopen(path.c_str(), "r");
      if (f == NULL) {
        return errno == ENOENT;
      }
      while (fgets(s, 100, f) != NULL) {
        if (sscanf(s, "%s", u) == 1) {
          if (rUser == u) {
            fclose(f);
            return false;
          }
        }
      }
      fclose(f);
      return true;
    }
    
    return false;
  }

  while (fgets(s, 100, f) != NULL) {
    if (sscanf(s, "%s", u) == 1) {
      if (rUser == u) {
        fclose(f);
        return true;
      }
    }
  }
  
  fclose(f);
  return false;
}

std::string IncronTab::GetUserTablePath(const std::string& rUser)
{
  std::string s;
  if (!IncronCfg::GetValue("user_table_dir", s))
    throw InotifyException("configuration is corrupted", EINVAL);
    
  return IncronCfg::BuildPath(s, rUser);
}

std::string IncronTab::GetSystemTablePath(const std::string& rName)
{
  std::string s;
  if (!IncronCfg::GetValue("system_table_dir", s))
    throw InotifyException("configuration is corrupted", EINVAL);
    
  return IncronCfg::BuildPath(s, rName);
}



