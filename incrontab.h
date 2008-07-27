
/// inotify cron table manipulator classes header
/**
 * \file incrontab.h
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


#ifndef _INCRONTAB_H_
#define _INCRONTAB_H_

#include <string>
#include <deque>

#include "strtok.h"


#define INCRON_TABLE_BASE "/var/spool/incron/"



class InCronTabEntry
{
public:
  InCronTabEntry();

  InCronTabEntry(const std::string& rPath, uint32_t uMask, const std::string& rCmd);
  
  ~InCronTabEntry() {}
  
  std::string ToString() const;
  
  static bool Parse(const std::string& rStr, InCronTabEntry& rEntry);
  
  inline const std::string& GetPath() const
  {
    return m_path;
  }
  
  inline int32_t GetMask() const
  {
    return m_uMask;
  }
  
  inline const std::string& GetCmd() const
  {
    return m_cmd;
  }
  
protected:
  std::string m_path;
  uint32_t m_uMask;
  std::string m_cmd;
};



class InCronTab
{
public:
  InCronTab() {}
  
  ~InCronTab() {}
  
  inline void Add(const InCronTabEntry& rEntry)
  {
    m_tab.push_back(rEntry);
  }
  
  inline void Clear()
  {
    m_tab.clear();
  }
  
  inline bool IsEmpty() const
  {
    return m_tab.empty();
  }
  
  inline int GetCount() const
  {
    return (int) m_tab.size();
  }
  
  inline InCronTabEntry& GetEntry(int index)
  {
    return m_tab[index];
  }
  
  bool Load(const std::string& rPath);
  
  bool Save(const std::string& rPath);
  
  static bool CheckUser(const std::string& rUser);
  
  static std::string GetUserTablePath(const std::string& rUser);

protected:
  std::deque<InCronTabEntry> m_tab;
};


#endif //_INCRONTAB_H_
