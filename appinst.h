
/// Application instance class header
/**
 * \file appinst.h
 * 
 * Copyright (C) 2007 Lukas Jelinek, <lukas@aiken.cz>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of one of the following licenses:
 *
 * \li 1. X11-style license (see LICENSE-X11)
 * \li 2. GNU Lesser General Public License, version 2.1 (see LICENSE-LGPL)
 * \li 3. GNU General Public License, version 2  (see LICENSE-GPL)
 *
 * If you want to help with choosing the best license for you,
 * please visit http://www.gnu.org/licenses/license-list.html.
 * 
 */


#ifndef APPINST_H_
#define APPINST_H_


#include <string>


/// Exception class.
/**
 * This class provides information about occurred errors.
 */
class AppInstException
{
public:
  /// Constructor.
  /**
   * \param[in] iErr error number
   */
  AppInstException(int iErr) : m_iErr(iErr) {}
  
  /// Returns the error number.
  /**
   * \return error number
   */
  inline int GetErrorNumber() const
  {
    return m_iErr;
  }
  
private:
  int m_iErr; ///< error number
    
};

/// Application instance management class.
/**
 * This class is intended for application which require to
 * be running only once (one instance only). It provides some
 * methods for simple locking, signaling etc.
 */
class AppInstance
{
public:
  /// Constructor.
  /**
   * \param[in] rName application name
   */
  AppInstance(const std::string& rName);
  
  /// Destructor.
  ~AppInstance();
  
  /// Attempts to lock the instance.
  /**
   * This method attempts to create a lockfile. If the file
   * already exists it checks whether its owner is still living.
   * If it does this method fails. Otherwise it unlinks this file
   * and re-attempts to create it.
   * 
   * \return true = instance locked, false = otherwise
   */
  bool Lock();
  
  /// Unlocks the instance.
  /**
   * This method removes (unlinks) the appropriate lockfile.
   * If the instance hasn't been locked this method has no
   * effect.
   */
  void Unlock();
  
  /// Checks whether an instance of this application exists.
  /**
   * If this instance has acquired the lockfile the call will
   * be successful. Otherwise it checks for existence of
   * another running instance.
   * 
   * \return true = instance exists, false = otherwise
   */
  bool Exists() const;
  
  /// Sends a signal to an instance of this application.
  /**
   * This method doesn't signal the current instance.
   * 
   * \param[in] iSigNo signal number
   * \return true = success, false = otherwise
   */ 
  bool SendSignal(int iSigNo) const;
  
protected:
  bool DoLock(const char* path);

  std::string GetLockfile() const;
  
private:
  std::string m_name;
  bool m_fLocked;
};

#endif /*APPINST_H_*/
