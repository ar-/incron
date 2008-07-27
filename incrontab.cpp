
/// inotify cron table manipulator classes implementation
/**
 * \file incrontab.cpp
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


#include <sstream>
#include <stdio.h>
#include <errno.h>

#include "inotify-cxx.h"

#include "incrontab.h"


/// Allowed users
#define INCRON_ALLOW_PATH "/etc/incron.allow"

/// Denied users
#define INCRON_DENY_PATH "/etc/incron.deny"




InCronTabEntry::InCronTabEntry()
: m_uMask(0),
  m_fNoLoop(false)
{
  
}

InCronTabEntry::InCronTabEntry(const std::string& rPath, uint32_t uMask, const std::string& rCmd)
: m_path(rPath),
  m_uMask(uMask),
  m_cmd(rCmd)
{
  
}

std::string InCronTabEntry::ToString() const
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
  
  ss << m_path << " " << m << " " << m_cmd;
  return ss.str();
}

bool InCronTabEntry::Parse(const std::string& rStr, InCronTabEntry& rEntry)
{
  char s1[1000], s2[1000], s3[1000];
  unsigned long u;
  
  if (sscanf(rStr.c_str(), "%s %s %[^\n]", s1, s2, s3) != 3)
    return false;
  
  rEntry.m_path = s1;
  rEntry.m_cmd = s3;
  rEntry.m_uMask = 0;  
  
  if (sscanf(s2, "%lu", &u) == 1) {
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

bool InCronTab::Load(const std::string& rPath)
{
  m_tab.clear();
  
  FILE* f = fopen(rPath.c_str(), "r");
  if (f == NULL)
    return false;
  
  char s[1000];
  InCronTabEntry e;
  while (fgets(s, 1000, f) != NULL) {
    if (InCronTabEntry::Parse(s, e)) {
      m_tab.push_back(e);
    }
  }
  
  fclose(f);
  
  return true;
}

bool InCronTab::Save(const std::string& rPath)
{
  FILE* f = fopen(rPath.c_str(), "w");
  if (f == NULL)
    return false;
  
  std::deque<InCronTabEntry>::iterator it = m_tab.begin();
  while (it != m_tab.end()) {
    fputs((*it).ToString().c_str(), f);
    fputs("\n", f);
    it++;
  }
  
  fclose(f);
  
  return true;
}

bool InCronTab::CheckUser(const std::string& rUser)
{
  char s[100], u[100];
  
  FILE* f = fopen(INCRON_ALLOW_PATH, "r");
  if (f == NULL) {
    if (errno == ENOENT) {
      f = fopen(INCRON_DENY_PATH, "r");
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

std::string InCronTab::GetUserTablePath(const std::string& rUser)
{
  std::string s(INCRON_TABLE_BASE);
  s.append(rUser);
  return s;
}

