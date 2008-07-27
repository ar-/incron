
/// inotify C++ interface header
/**
 * \file inotify-cxx.h
 * 
 * inotify C++ interface
 * 
 * Copyright (C) 2006 Lukas Jelinek, <lukas@aiken.cz>
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





#ifndef _INOTIFYCXX_H_
#define _INOTIFYCXX_H_

#include <string>
#include <deque>
#include <map>

// Please ensure that the following headers take the right place.
#include <sys/inotify.h>
#include <sys/inotify-syscalls.h>

/// Event struct size
#define INOTIFY_EVENT_SIZE (sizeof(struct inotify_event))

/// Event buffer length
#define INOTIFY_BUFLEN (1024 * (INOTIFY_EVENT_SIZE + 16))


// forward declaration
class InotifyWatch;
class Inotify;


/// inotify event class
/**
 * It holds all information about inotify event and provides
 * access to its particular values.
 */
class InotifyEvent
{
public:
  /// Constructor.
  /**
   * Creates a plain event.
   */
  InotifyEvent()
  {
    memset(&m_evt, 0, sizeof(m_evt));
    m_evt.wd = (int32_t) -1;
    m_pWatch = NULL;
  }
  
  /// Constructor.
  /**
   * Creates an event based on inotify event data.
   * For NULL pointers it works the same way as InotifyEvent().
   * 
   * \param[in] pEvt event data
   * \param[in] pWatch inotify watch
   */
  InotifyEvent(const struct inotify_event* pEvt, InotifyWatch* pWatch)
  {
    if (pEvt != NULL) {
      memcpy(&m_evt, pEvt, sizeof(m_evt));
      if (pEvt->name != NULL)
        m_name = pEvt->name;
      m_pWatch = pWatch;
    }
    else {
      memset(&m_evt, 0, sizeof(m_evt));
      m_evt.wd = (int32_t) -1;
      m_pWatch = NULL;
    }
  }
  
  /// Destructor.
  ~InotifyEvent() {}
  
  /// Returns the event watch descriptor.
  /**
   * \return watch descriptor
   * 
   * \sa InotifyWatch::GetDescriptor()
   */
  inline int32_t GetDescriptor() const
  {
    return (int32_t) m_evt.wd;
  }
  
  /// Returns the event mask.
  /**
   * \return event mask
   * 
   * \sa InotifyWatch::GetMask()
   */
  inline uint32_t GetMask() const
  {
    return (uint32_t) m_evt.mask;
  }
  
  /// Checks a value for the event type.
  /**
   * \param[in] uValue checked value
   * \param[in] uType type which is checked for
   * \return true = the value contains the given type, false = otherwise
   */
  inline static bool IsType(uint32_t uValue, uint32_t uType)
  {
    return ((uValue & uType) != 0) && ((~uValue & uType) == 0);
  }
  
  /// Checks for the event type.
  /**
   * \param[in] uType type which is checked for
   * \return true = event mask contains the given type, false = otherwise
   */
  inline bool IsType(uint32_t uType) const
  {
    return IsType((uint32_t) m_evt.mask, uType);
  }
  
  /// Returns the event cookie.
  /**
   * \return event cookie
   */
  inline uint32_t GetCookie() const
  {
    return (uint32_t) m_evt.cookie;
  }
  
  /// Returns the event name length.
  /**
   * \return event name length
   */
  inline uint32_t GetLength() const
  {
    return (uint32_t) m_evt.len;
  }
  
  /// Returns the event name.
  /**
   * \return event name
   */
  inline const std::string& GetName() const
  {
    return m_name;
  }
  
  /// Extracts the event name.
  /**
   * \param[out] rName event name
   */
  inline void GetName(std::string& rName) const
  {
    rName = GetName();
  }
  
  /// Returns the source watch.
  /**
   * \return source watch
   */
  inline InotifyWatch* GetWatch()
  {
    return m_pWatch;
  }
  
  /// Returns the event raw data.
  /**
   * For NULL pointer it does nothing.
   * 
   * \param[in,out] pEvt event data
   */
  inline void GetData(struct inotify_event* pEvt)
  {
    if (pEvt != NULL)
      memcpy(pEvt, &m_evt, sizeof(m_evt));
  }
  
  /// Returns the event raw data.
  /**
   * \param[in,out] rEvt event data
   */
  inline void GetData(struct inotify_event& rEvt)
  {
    memcpy(&rEvt, &m_evt, sizeof(m_evt));
  }
  
  /// Finds the appropriate mask for a name.
  /**
   * \param[in] rName mask name
   * \return mask for name; 0 on failure
   */
  static uint32_t GetMaskByName(const std::string& rName);
  
  /// Fills the string with all types contained in an event mask value.
  /**
   * \param[in] uValue event mask value
   * \param[out] rStr dumped event types
   */
  static void DumpTypes(uint32_t uValue, std::string& rStr);
  
  /// Fills the string with all types contained in the event mask.
  /**
   * \param[out] rStr dumped event types
   */
  void DumpTypes(std::string& rStr) const;
  
private:
  struct inotify_event m_evt; ///< event structure
  std::string m_name;         ///< event name
  InotifyWatch* m_pWatch;     ///< source watch
};



/// inotify watch class
class InotifyWatch
{
public:
  /// Constructor.
  /**
   * Creates an inotify watch. Because this watch is
   * inactive it has an invalid descriptor (-1).
   * 
   * \param[in] rPath watched file path
   * \param[in] uMask mask for events
   */
  InotifyWatch(const std::string& rPath, int32_t uMask)
  {
    m_path = rPath;
    m_uMask = uMask;
    m_wd = (int32_t) -1;
  }
  
  /// Destructor.
  ~InotifyWatch() {}
  
  /// Returns the watch descriptor.
  /**
   * \return watch descriptor; -1 for inactive watch
   */
  inline int32_t GetDescriptor() const
  {
    return m_wd;
  }
  
  /// Returns the watched file path.
  /**
   * \return file path
   */
  inline const std::string& GetPath() const
  {
    return m_path;
  }
  
  /// Returns the watch event mask.
  /**
   * \return event mask
   */
  inline uint32_t GetMask() const
  {
    return (uint32_t) m_uMask;
  }
  
  /// Returns the appropriate inotify class instance.
  /**
   * \return inotify instance
   */
  inline Inotify* GetInotify()
  {
    return m_pInotify;
  }
  
private:
  friend class Inotify;

  std::string m_path;   ///< watched file path
  uint32_t m_uMask;     ///< event mask
  int32_t m_wd;         ///< watch descriptor
  Inotify* m_pInotify;  ///< inotify object
};


/// Mapping from watch descriptors to watch objects.
typedef std::map<int32_t, InotifyWatch*> IN_WATCH_MAP;


/// inotify class
class Inotify
{
public:
  /// Constructor.
  /**
   * Creates and initializes an instance of inotify communication
   * object (opens the inotify device).
   */
  Inotify();
  
  /// Destructor.
  /**
   * Calls Close() due for clean-up.
   */
  ~Inotify();
  
  /// Removes all watches and closes the inotify device.
  void Close();
  
  /// Checks whether the inotify is ready.
  /**
   * \return true = initialized properly, false = something failed
   */
  inline bool IsReady() const
  {
    return m_fd != -1;
  }
  
  /// Adds a new watch.
  /**
   * \param[in] pWatch inotify watch
   * \return true = success, false = failure
   */
  bool Add(InotifyWatch* pWatch);
  
  /// Adds a new watch.
  /**
   * \param[in] rWatch inotify watch
   * \return true = success, false = failure
   */
  inline bool Add(InotifyWatch& rWatch)
  {
    return Add(&rWatch);
  }
  
  /// Removes a watch.
  /**
   * If the given watch is not present it does nothing.
   * 
   * \param[in] pWatch inotify watch
   */
  void Remove(InotifyWatch* pWatch);
  
  /// Removes a watch.
  /**
   * If the given watch is not present it does nothing.
   * 
   * \param[in] rWatch inotify watch
   */
  inline void Remove(InotifyWatch& rWatch)
  {
    Remove(&rWatch);
  }
  
  /// Removes all watches.
  void RemoveAll();
  
  /// Returns the count of watches.
  /**
   * \return count of watches
   */
  inline size_t GetWatchCount() const
  {
    return (size_t) m_watches.size();
  }
  
  /// Waits for inotify events.
  /**
   * It waits until one or more events occur.
   * 
   * \param[in] fNoIntr if true it re-calls the system call after a handled signal
   * \return true = event(s) occurred, false = failure
   */
  bool WaitForEvents(bool fNoIntr = false);
  
  /// Returns the count of received and queued events.
  /**
   * This number is related to the events in the queue inside
   * this object, not to the events pending in the kernel.
   * 
   * \return count of events
   */
  int GetEventCount();
  
  /// Extracts a queued inotify event.
  /**
   * The extracted event is removed from the queue.
   * If the pointer is NULL it does nothing.
   * 
   * \param[in,out] pEvt event object
   * \return true = success, false = failure
   */
  bool GetEvent(InotifyEvent* pEvt);
  
  /// Extracts a queued inotify event.
  /**
   * The extracted event is removed from the queue.
   * 
   * \param[in,out] rEvt event object
   * \return true = success, false = failure
   */
  bool GetEvent(InotifyEvent& rEvt)
  {
    return GetEvent(&rEvt);
  }
  
  /// Extracts a queued inotify event (without removing).
  /**
   * The extracted event stays in the queue.
   * If the pointer is NULL it does nothing.
   * 
   * \param[in,out] pEvt event object
   * \return true = success, false = failure
   */
  bool PeekEvent(InotifyEvent* pEvt);
  
  /// Extracts a queued inotify event (without removing).
  /**
   * The extracted event stays in the queue.
   * 
   * \param[in,out] rEvt event object
   * \return true = success, false = failure
   */
  bool PeekEvent(InotifyEvent& rEvt)
  {
    return PeekEvent(&rEvt);
  }
  
  /// Searches for a watch.
  /**
   * It tries to find a watch by the given descriptor.
   * 
   * \param[in] iDescriptor watch descriptor
   * \return found descriptor; NULL if no such watch exists
   */
  InotifyWatch* FindWatch(int iDescriptor);  

private: 
  int m_fd;                             ///< file descriptor
  IN_WATCH_MAP m_watches;               ///< watches
  unsigned char m_buf[INOTIFY_BUFLEN];  ///< buffer for events
  std::deque<InotifyEvent> m_events;    ///< event queue
};


#endif //_INOTIFYCXX_H_
