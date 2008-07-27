
/// inotify C++ interface implementation
/**
 * \file inotify-cxx.cpp
 * 
 * inotify C++ interface
 * 
 * Copyright (C) 2006 Lukas Jelinek <lukas@aiken.cz>
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
 

#include <errno.h>
#include <unistd.h>

#include "inotify-cxx.h"

#define DUMP_SEP \
  ({ \
    if (!rStr.empty()) { \
      rStr.append(","); \
    } \
  })

uint32_t InotifyEvent::GetMaskByName(const std::string& rName)
{
  if (rName == "IN_ACCESS")
    return IN_ACCESS;
  else if (rName == "IN_MODIFY")
    return IN_MODIFY;
  else if (rName == "IN_ATTRIB")
    return IN_ATTRIB;
  else if (rName == "IN_CLOSE_WRITE")
    return IN_CLOSE_WRITE;
  else if (rName == "IN_CLOSE_NOWRITE")
    return IN_CLOSE_NOWRITE;
  else if (rName == "IN_MOVED_FROM")
    return IN_MOVED_FROM;
  else if (rName == "IN_MOVED_TO")
    return IN_MOVED_TO;
  else if (rName == "IN_CREATE")
    return IN_CREATE;
  else if (rName == "IN_DELETE")
    return IN_DELETE;
  else if (rName == "IN_DELETE_SELF")
    return IN_DELETE_SELF;
  else if (rName == "IN_UNMOUNT")
    return IN_UNMOUNT;
  else if (rName == "IN_Q_OVERFLOW")
    return IN_Q_OVERFLOW;
  else if (rName == "IN_IGNORED")
    return IN_IGNORED;
  else if (rName == "IN_CLOSE")
    return IN_CLOSE;
  else if (rName == "IN_MOVE")
    return IN_MOVE;
  else if (rName == "IN_ISDIR")
    return IN_ISDIR;
  else if (rName == "IN_ONESHOT")
    return IN_ONESHOT;
  else if (rName == "IN_ALL_EVENTS")
    return IN_ALL_EVENTS;
    
  return (uint32_t) 0;
}

void InotifyEvent::DumpTypes(uint32_t uValue, std::string& rStr)
{
  rStr = "";
  
  if (IsType(uValue, IN_ALL_EVENTS)) {
    rStr.append("IN_ALL_EVENTS");
  }
  else {
    if (IsType(uValue, IN_ACCESS)) {
      DUMP_SEP;
      rStr.append("IN_ACCESS");    
    }
    if (IsType(uValue, IN_MODIFY)) {
      DUMP_SEP;
      rStr.append("IN_MODIFY");
    }
    if (IsType(uValue, IN_ATTRIB)) {
      DUMP_SEP;
      rStr.append("IN_ATTRIB");
    }
    if (IsType(uValue, IN_CREATE)) {
      DUMP_SEP;
      rStr.append("IN_CREATE");
    }
    if (IsType(uValue, IN_DELETE)) {
      DUMP_SEP;
      rStr.append("IN_DELETE");
    }
    if (IsType(uValue, IN_DELETE_SELF)) {
      DUMP_SEP;
      rStr.append("IN_DELETE_SELF");
    }
    if (IsType(uValue, IN_OPEN)) {
      DUMP_SEP;
      rStr.append("IN_OPEN");
    }
    if (IsType(uValue, IN_CLOSE)) {
      DUMP_SEP;
      rStr.append("IN_CLOSE");
    }
    else {
      if (IsType(uValue, IN_CLOSE_WRITE)) {
        DUMP_SEP;
        rStr.append("IN_CLOSE_WRITE");
      }
      if (IsType(uValue, IN_CLOSE_NOWRITE)) {
        DUMP_SEP;
        rStr.append("IN_CLOSE_NOWRITE");
      }
    }
    if (IsType(uValue, IN_MOVE)) {
      DUMP_SEP;
      rStr.append("IN_MOVE");
    }
    else {
      if (IsType(uValue, IN_MOVED_FROM)) {
        DUMP_SEP;
        rStr.append("IN_MOVED_FROM");
      }
      if (IsType(uValue, IN_MOVED_TO)) {
        DUMP_SEP;
        rStr.append("IN_MOVED_TO");
      }
    }
  }
  if (IsType(uValue, IN_UNMOUNT)) {
    DUMP_SEP;
    rStr.append("IN_UNMOUNT");
  }
  if (IsType(uValue, IN_Q_OVERFLOW)) {
    DUMP_SEP;
    rStr.append("IN_Q_OVERFLOW");
  }
  if (IsType(uValue, IN_IGNORED)) {
    DUMP_SEP;
    rStr.append("IN_IGNORED");
  }
  if (IsType(uValue, IN_ISDIR)) {
    DUMP_SEP;
    rStr.append("IN_ISDIR");
  }
  if (IsType(uValue, IN_ONESHOT)) {
    DUMP_SEP;
    rStr.append("IN_ONESHOT");
  }
}

void InotifyEvent::DumpTypes(std::string& rStr) const
{
  DumpTypes((uint32_t) m_evt.mask, rStr);
}


Inotify::Inotify()
{
  m_fd = inotify_init();  
}
  
Inotify::~Inotify()
{
  Close();
}

void Inotify::Close()
{
  if (m_fd != -1) {
    RemoveAll();
    close(m_fd);
    m_fd = -1;
  }
}

bool Inotify::Add(InotifyWatch* pWatch)
{
  if (m_fd == -1)
    return false;
    
  pWatch->m_wd = inotify_add_watch(m_fd, pWatch->GetPath().c_str(), pWatch->GetMask());
  if (pWatch->m_wd != -1) {
    m_watches.insert(IN_WATCH_MAP::value_type(pWatch->m_wd, pWatch));
    pWatch->m_pInotify = this;
    return true;
  }
  
  return false;
}

void Inotify::Remove(InotifyWatch* pWatch)
{
  if (m_fd == -1)
    return;
    
  if (inotify_rm_watch(m_fd, pWatch->GetMask()) != -1) {
    m_watches.erase(pWatch->m_wd);
    pWatch->m_wd = -1;
    pWatch->m_pInotify = NULL;
  }
}

void Inotify::RemoveAll()
{
  IN_WATCH_MAP::iterator it = m_watches.begin();
  while (it != m_watches.end()) {
    InotifyWatch* pW = (*it).second;
    inotify_rm_watch(m_fd, pW->GetMask());
    pW->m_wd = -1;
    pW->m_pInotify = NULL;
    it++;
  }
  
  m_watches.clear();
}

bool Inotify::WaitForEvents(bool fNoIntr)
{
  ssize_t len = 0;
  
  do {
    len = read(m_fd, m_buf, INOTIFY_BUFLEN);
  } while (fNoIntr && len == -1 && errno == EINTR);
  
  if (len <= 0) {
    return false;
  }
  
  ssize_t i = 0;
  while (i < len) {
    InotifyWatch* pW = FindWatch(((struct inotify_event *) &m_buf[i])->wd);
    InotifyEvent evt((struct inotify_event *) &m_buf[i], pW);
    m_events.push_back(evt);
    i += INOTIFY_EVENT_SIZE + (int) evt.GetLength();
  }
  
  return true;
}
  
int Inotify::GetEventCount()
{
  return m_events.size();
}
  
bool Inotify::GetEvent(InotifyEvent* pEvt)
{
  bool b = PeekEvent(pEvt); 
  if (b)
    m_events.pop_front();
  return b;
}
  
bool Inotify::PeekEvent(InotifyEvent* pEvt)
{
  if (pEvt == NULL || m_events.empty())
    return false;
    
  *pEvt = m_events.front();
  
  return true;
}

InotifyWatch* Inotify::FindWatch(int iDescriptor)
{
  IN_WATCH_MAP::iterator it = m_watches.find(iDescriptor);
  if (it == m_watches.end())
    return NULL;
    
  return (*it).second;
}
  
