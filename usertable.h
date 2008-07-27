
/// inotify cron daemon user tables header
/**
 * \file usertable.h
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

#ifndef _USERTABLE_H_
#define _USERTABLE_H_

#include <map>

#include "inotify-cxx.h"
#include "incrontab.h"


class UserTable;


typedef std::map<InotifyWatch*, UserTable*> IWUT_MAP;
typedef std::map<InotifyWatch*, InCronTabEntry*> IWCE_MAP;

class EventDispatcher
{
public:
  EventDispatcher(Inotify* pIn);
  
  ~EventDispatcher() {}

  void DispatchEvent(InotifyEvent& rEvt);
  
  void Register(InotifyWatch* pWatch, UserTable* pTab);
  
  void Unregister(InotifyWatch* pWatch);
  
  void UnregisterAll(UserTable* pTab);
  
private:
  Inotify* m_pIn;
  IWUT_MAP m_maps;
  
  UserTable* FindTable(InotifyWatch* pW);
};


class UserTable
{
public:
	UserTable(Inotify* pIn, EventDispatcher* pEd, const std::string& rUser);
	virtual ~UserTable();
  
  void Load();
  
  void Dispose();
  
  void OnEvent(InotifyEvent& rEvt);
private:
  Inotify* m_pIn;
  EventDispatcher* m_pEd;
  std::string m_user;
  InCronTab m_tab;
  IWCE_MAP m_map;
  
  InCronTabEntry* FindEntry(InotifyWatch* pWatch);
  
  bool PrepareArgs(const std::string& rCmd, int& argc, char**& argv);
};

#endif //_USERTABLE_H_
