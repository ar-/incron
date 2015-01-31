
/// inotify cron daemon user tables header
/**
 * \file usertable.h
 * 
 * inotify cron system
 * 
 * Copyright (C) 2006, 2007, 2008 Lukas Jelinek, <lukas@aiken.cz>
 * Copyright (C) 2014, 2015 Andreas Altair Redmer, <altair.ibn.la.ahad.sy@gmail.com>
 * 
 * This program is free software; you can use it, redistribute
 * it and/or modify it under the terms of the GNU General Public
 * License, version 2 (see LICENSE-GPL).
 *  
 */

#ifndef _USERTABLE_H_
#define _USERTABLE_H_

#include <map>
#include <deque>
#include <sys/poll.h>

#include "inotify-cxx.h"
#include "incrontab.h"


class UserTable;

/// User name to user table mapping definition
typedef std::map<std::string, UserTable*> SUT_MAP;

/// Callback for calling after a process finishes.
typedef void (*proc_done_cb)(InotifyWatch*);

/// Child process data
typedef struct
{
  proc_done_cb onDone;  ///< function called after process finishes
  InotifyWatch* pWatch; ///< related watch
} ProcData_t;

/// fd-to-usertable mapping
typedef std::map<int, UserTable*> FDUT_MAP;

/// Watch-to-tableentry mapping
typedef std::map<InotifyWatch*, IncronTabEntry*> IWCE_MAP;

/// Child process list
typedef std::map<pid_t, ProcData_t> PROC_MAP;

/// Event dispatcher class.
/**
 * This class processes events and distributes them as needed.
 */
class EventDispatcher
{
public:
  /// Constructor.
  /**
   * \param[in] iPipeFd pipe descriptor
   * \param[in] pIn inotify object for table management
   * \param[in] pSys watch for system tables
   * \param[in] pUser watch for user tables
   */
  EventDispatcher(int iPipeFd, Inotify* pIn, InotifyWatch* pSys, InotifyWatch* pUser);
  
  /// Destructor.
  ~EventDispatcher();

  /// Processes events.
  /**
   * \return pipe event occurred yes/no
   */
  bool ProcessEvents();
  
  /// Registers an user table.
  /**
   * \param[in] pTab user table
   */
  void Register(UserTable* pTab);
  
  /// Unregisters an user table.
  /**
   * \param[in] pTab user table
   */
  void Unregister(UserTable* pTab);
  
  /// Returns the poll data size.
  /**
   * \return poll data size
   */
  inline size_t GetSize() const
  {
    return m_size;
  }
  
  /// Returns the poll data.
  /**
   * \return poll data
   */
  inline struct pollfd* GetPollData()
  {
    return m_pPoll;
  }
  
  /// Rebuilds the poll array data.
  void Rebuild();
  
  /// Removes all registered user tables.
  /**
   * It doesn't cause poll data rebuilding.
   */
  inline void Clear()
  {
    m_maps.clear();
  }
  
private:
  int m_iPipeFd;    ///< pipe file descriptor
  int m_iMgmtFd;    ///< table management file descriptor
  Inotify* m_pIn;   ///< table management inotify object 
  InotifyWatch* m_pSys;   ///< watch for system tables
  InotifyWatch* m_pUser;  ///< watch for user tables 
  FDUT_MAP m_maps;  ///< watch-to-usertable mapping
  size_t m_size;    ///< poll data size
  struct pollfd* m_pPoll; ///< poll data array
  
  /// Processes events on the table management inotify object. 
  void ProcessMgmtEvents();
};


/// User table class.
/**
 * This class processes inotify events for an user. It creates
 * child processes which do appropriate actions as defined
 * in the user table file.
 */
class UserTable
{
public:
  /// Constructor.
  /**
   * \param[in] pEd event dispatcher
   * \param[in] rUser user name
   * \param[in] fSysTable system table yes/no
   */
	UserTable(EventDispatcher* pEd, const std::string& rUser, bool fSysTable);
  
  /// Destructor.
	virtual ~UserTable();
  
  /// Loads the table.
  /**
   * All loaded entries have their inotify watches and are
   * registered for event dispatching.
   * If loading fails the table remains empty.
   */
  void Load();
  
  void AddTabEntry(IncronTabEntry& rE);

  /// Removes all entries from the table.
  /**
   * All entries are unregistered from the event dispatcher and
   * their watches are destroyed.
   */
  void Dispose();
  
  /// Processes an inotify event.
  /**
   * \param[in] rEvt inotify event
   */
  void OnEvent(InotifyEvent& rEvt);

  /// Checks whether the user may access a file.
  /**
   * Any access right (RWX) is sufficient.
   * 
   * \param[in] rPath absolute file path
   * \param[in] fNoFollow don't follow a symbolic link 
   * \return true = access granted, false = otherwise
   */
  bool MayAccess(const std::string& rPath, bool fNoFollow) const;
  
  /// Checks whether it is a system table.
  /**
   * \return true = system table, false = user table
   */
  bool IsSystem() const;
  
  /// Returns the related inotify object.
  /**
   * \return related inotify object
   */
  Inotify* GetInotify()
  {
    return &m_in;
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
  inline static bool CheckUser(const char* user)
  {
    struct passwd* pw = getpwnam(user);
    if (pw == NULL)
      return false;
      
    return IncronTab::CheckUser(user);
  }
  
  /// Runs a program as the table's user.
  /**
   * \attention Don't call from the main process (before forking)!
   */
  void RunAsUser(std::string cmd) const;
  
private:
  Inotify m_in;           ///< inotify object
  EventDispatcher* m_pEd; ///< event dispatcher
  std::string m_user;     ///< user name
  bool m_fSysTable;       ///< system table yes/no
  IncronTab m_tab;        ///< incron table
  IWCE_MAP m_map;         ///< watch-to-entry mapping

  static PROC_MAP s_procMap;  ///< child process mapping
  
  /// Finds an entry for a watch.
  /**
   * \param[in] pWatch inotify watch
   * \return pointer to the appropriate entry; NULL if no such entry exists
   */
  IncronTabEntry* FindEntry(InotifyWatch* pWatch);
 
};

#endif //_USERTABLE_H_
