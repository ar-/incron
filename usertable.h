
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
#include <deque>

#include "inotify-cxx.h"
#include "incrontab.h"


class UserTable;

/// Callback for calling after a process finishes.
typedef void (*proc_done_cb)(InotifyWatch*);

/// Child process data
typedef struct
{
  pid_t pid;            ///< PID
  proc_done_cb onDone;  ///< function called after process finishes
  InotifyWatch* pWatch; ///< related watch
} ProcData_t;

/// Watch-to-usertable mapping
typedef std::map<InotifyWatch*, UserTable*> IWUT_MAP;

/// Watch-to-tableentry mapping
typedef std::map<InotifyWatch*, InCronTabEntry*> IWCE_MAP;

/// Child process list
typedef std::deque<ProcData_t> PROC_LIST;

/// Event dispatcher class.
/**
 * This class distributes inotify events to appropriate user tables.
 */
class EventDispatcher
{
public:
  /// Constructor.
  /**
   * \param[in] pIn inotify object
   */
  EventDispatcher(Inotify* pIn);
  
  /// Destructor.
  ~EventDispatcher() {}

  /// Dispatches an event.
  /**
   * \param[in] rEvt inotify event
   */
  void DispatchEvent(InotifyEvent& rEvt);
  
  /// Registers a watch for an user table.
  /**
   * \param[in] pWatch inotify watch
   * \param[in] pTab user table
   */
  void Register(InotifyWatch* pWatch, UserTable* pTab);
  
  /// Unregisters a watch.
  /**
   * \param[in] pWatch inotify watch
   */
  void Unregister(InotifyWatch* pWatch);
  
  /// Unregisters all watches for an user table.
  /**
   * \param[in] pTab user table
   */
  void UnregisterAll(UserTable* pTab);
  
private:
  Inotify* m_pIn;   ///< inotify object
  IWUT_MAP m_maps;  ///< watch-to-usertable mapping
  
  /// Finds an user table for a watch.
  /**
   * \param[in] pW inotify watch
   * \return pointer to the appropriate watch; NULL if no such watch exists
   */
  UserTable* FindTable(InotifyWatch* pW);
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
   * \param[in] pIn inotify object
   * \param[in] pEd event dispatcher
   * \param[in] rUser user name
   */
	UserTable(Inotify* pIn, EventDispatcher* pEd, const std::string& rUser);
  
  /// Destructor.
	virtual ~UserTable();
  
  /// Loads the table.
  /**
   * All loaded entries have their inotify watches and are
   * registered for event dispatching.
   * If loading fails the table remains empty.
   */
  void Load();
  
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
  
  /// Cleans-up all zombie child processes and enables disabled watches.
  /**
   * \attention This method must be called AFTER processing all events
   *            which has been caused by the processes.
   */
  static void FinishDone();
private:
  Inotify* m_pIn;         ///< inotify object
  EventDispatcher* m_pEd; ///< event dispatcher
  std::string m_user;     ///< user name
  InCronTab m_tab;        ///< incron table
  IWCE_MAP m_map;         ///< watch-to-entry mapping

  static PROC_LIST s_procList;  ///< child process list
  
  /// Finds an entry for a watch.
  /**
   * \param[in] pWatch inotify watch
   * \return pointer to the appropriate entry; NULL if no such entry exists
   */
  InCronTabEntry* FindEntry(InotifyWatch* pWatch);
  
  /// Prepares arguments for creating a child process.
  /**
   * \param[in] rCmd command string
   * \param[out] argc argument count
   * \param[out] argv argument array
   * \return true = success, false = failure
   */
  bool PrepareArgs(const std::string& rCmd, int& argc, char**& argv);
  
  /// Frees memory allocated for arguments.
  /**
   * \param[in] argc argument count
   * \param[in] argv argument array
   */
  void CleanupArgs(int argc, char** argv);
  
};

#endif //_USERTABLE_H_
