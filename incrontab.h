
/// inotify cron table manipulator classes header
/**
 * \file incrontab.h
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


#ifndef _INCRONTAB_H_
#define _INCRONTAB_H_

#include <string>
#include <deque>

#include "strtok.h"

/// Incron table entry class.
class IncronTabEntry
{
public:
  /// Constructor.
  /**
   * Creates an empty entry for later use with Parse().
   * 
   * \sa Parse()
   */
  IncronTabEntry();

  /// Constructor.
  /**
   * Creates an entry based on defined parameters.
   * 
   * \param[in] rPath watched filesystem path
   * \param[in] uMask event mask
   * \param[in] rCmd command string
   */
  IncronTabEntry(const std::string& rPath, uint32_t uMask, const std::string& rCmd);
  
  /// Destructor.
  ~IncronTabEntry() {}
  
  /// Converts the entry to string representation.
  /**
   * This method creates a string for use in a table file.
   * 
   * \return string representation
   */
  std::string ToString() const;
  
  /// Parses a string and attempts to extract entry parameters.
  /**
   * \param[in] rStr parsed string
   * \param[out] rEntry parametrized entry
   * \return true = success, false = failure
   */
  static bool Parse(const std::string& rStr, IncronTabEntry& rEntry);
  
  /// Returns the watch filesystem path.
  /**
   * \return watch path
   */
  inline const std::string& GetPath() const
  {
    return m_path;
  }
  
  /// Returns the event mask.
  /**
   * \return event mask
   */
  inline int32_t GetMask() const
  {
    return m_uMask;
  }
  
  /// Returns the command string.
  /**
   * \return command string
   */
  inline const std::string& GetCmd() const
  {
    return m_cmd;
  }
  
  /// Checks whether this entry has set loop-avoidance.
  /**
   * \return true = no loop, false = loop allowed
   */
  inline bool IsNoLoop() const
  {
    return m_fNoLoop;
  }
  
  /// Add backslashes before spaces in the source path.
  /**
   * It also adds backslashes before all original backslashes
   * of course.
   * 
   * The source string is not modified and a copy is returned
   * instead.
   * 
   * This method is intended to be used for paths in user tables.
   * 
   * \param[in] rPath path to be modified
   * \return modified path
   */
  static std::string GetSafePath(const std::string& rPath);
  
protected:
  std::string m_path; ///< watch path
  uint32_t m_uMask;   ///< event mask
  std::string m_cmd;  ///< command string
  bool m_fNoLoop;     ///< no loop yes/no
};


/// Incron table class.
class IncronTab
{
public:
  /// Constructor.
  IncronTab() {}
  
  /// Destructor.
  ~IncronTab() {}
  
  /// Add an entry to the table.
  /**
   * \param[in] rEntry table entry
   */
  inline void Add(const IncronTabEntry& rEntry)
  {
    m_tab.push_back(rEntry);
  }
  
  /// Removes all entries.
  inline void Clear()
  {
    m_tab.clear();
  }
  
  /// Checks whether the table is empty.
  /**
   * \return true = empty, false = otherwise
   */
  inline bool IsEmpty() const
  {
    return m_tab.empty();
  }
  
  /// Returns the count of entries.
  /**
   * \return count of entries
   */
  inline int GetCount() const
  {
    return (int) m_tab.size();
  }
  
  /// Returns an entry.
  /**
   * \return reference to the entry for the given index
   * 
   * \attention This method doesn't test index bounds. If you
   *            pass an invalid value the program may crash
   *            and/or behave unpredictible way!
   */
  inline IncronTabEntry& GetEntry(int index)
  {
    return m_tab[index];
  }
  
  /// Loads the table.
  /**
   * \param[in] rPath path to a source table file
   * \return true = success, false = failure
   */
  bool Load(const std::string& rPath);
  
  /// Saves the table.
  /**
   * \param[in] rPath path to a destination table file
   * \return true = success, false = failure
   */
  bool Save(const std::string& rPath);
  
  /// Checks whether an user has permission to use incron.
  /**
   * \param[in] rUser user name
   * \return true = permission OK, false = otherwise
   */
  static bool CheckUser(const std::string& rUser);
  
  /// Composes a path to an user incron table file.
  /**
   * \param[in] rUser user name
   * \return path to the table file
   * 
   * \attention No tests (existence, permission etc.) are done.
   */
  static std::string GetUserTablePath(const std::string& rUser);
  
  /// Composes a path to a system incron table file.
  /**
   * \param[in] rName table name (pseudouser)
   * \return path to the table file
   * 
   * \attention No tests (existence, permission etc.) are done.
   */
  static std::string GetSystemTablePath(const std::string& rName);

protected:
  std::deque<IncronTabEntry> m_tab; ///< incron table
};


#endif //_INCRONTAB_H_
