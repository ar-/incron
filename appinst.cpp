
/// Application instance class implementation
/**
 * \file appinst.cpp
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
 

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>

#include "appinst.h"

/// Lockfile permissions (currently 0644)
#define APPLOCK_PERM (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
  

AppInstance::AppInstance(const std::string& rName, const std::string& rBase)
: m_fLocked(false)
{
  std::string base(rBase);
  if (base.empty())
    base = APPLOCK_BASEDIR;
  
  if (base[base.length()-1] == '/')
    m_path = base + rName + ".pid";
  else
    m_path = base + "/" + rName + ".pid";
}

AppInstance::~AppInstance()
{
  try {
    Unlock();
  } catch (AppInstException e) {}
}

bool AppInstance::DoLock()
{
  int fd = open(m_path.c_str(), O_WRONLY | O_CREAT | O_EXCL, APPLOCK_PERM);
  if (fd != -1) {
    FILE* f = fdopen(fd, "w");
    if (f == NULL) {
      AppInstException e(errno);
      close(fd);
      throw e;
    }
    
    if (fprintf(f, "%u", (unsigned) getpid()) <= 0) {
      AppInstException e(errno);
      fclose(f);
      throw e;
    }
    
    if (fclose(f) != 0)
      throw AppInstException(errno);
    
    m_fLocked = true;  
    return true;
  }
  
  if (errno != EEXIST)
    throw AppInstException(errno);
  
  return false;
}

bool AppInstance::Lock()
{
  for (int i=0; i<100; i++) {
    if (DoLock())
      return true;
    
    FILE* f = fopen(m_path.c_str(), "r");
    if (f == NULL) {
      if (errno != ENOENT)
        throw AppInstException(errno);
    }
    else {
      unsigned pid;
      ssize_t len = fscanf(f, "%u", &pid);
      if (len == -1) {
        AppInstException e(errno);
        fclose(f);
        throw e;
      }
      else if (len == 0) {
        AppInstException e(EIO);
        fclose(f);
        throw e;
      }
      
      fclose(f);
      
      int res = kill((pid_t) pid, 0);
      if (res == 0)
        return false;
        
      if (errno != ESRCH)
        throw AppInstException(errno);
        
      res = unlink(m_path.c_str());
      if (res != 0 && errno != ENOENT)
        throw AppInstException(errno);
    }
  }
  
  return false;
}

void AppInstance::Unlock()
{
  if (!m_fLocked)
    return;
    
  if (unlink(m_path.c_str()) != 0 && errno != ENOENT)
    throw AppInstException(errno);
    
  m_fLocked = false;
}

bool AppInstance::Exists() const
{
  if (m_fLocked)
    return true;
  
  FILE* f = fopen(m_path.c_str(), "r");
  if (f == NULL) {
    if (errno == ENOENT)
      return false;
    else
      throw AppInstException(errno);
  }
    
  bool ok = false;
    
  unsigned pid;
  if (fscanf(f, "%u", &pid) == 1) {
    if (kill((pid_t) pid, 0) == 0)
      ok = true;
    else if (errno != ESRCH) {
      AppInstException e(errno);
      fclose(f);
      throw e;
    }
  }
  
  fclose(f);
  
  return ok;
}

bool AppInstance::SendSignal(int iSigNo) const
{
  FILE* f = fopen(m_path.c_str(), "r");
  if (f == NULL) {
    if (errno == ENOENT)
      return false;
    else
      throw AppInstException(errno);
  }
    
  bool ok = false;
    
  unsigned pid;
  if (fscanf(f, "%u", &pid) == 1) {
    if (pid != (unsigned) getpid()) {
      if (kill((pid_t) pid, iSigNo) == 0) {
        ok = true;
      }
      else if (errno != ESRCH) {
        AppInstException e(errno);
        fclose(f);
        throw e;
      }
    }
  }
  
  fclose(f);
  
  return ok;
}

