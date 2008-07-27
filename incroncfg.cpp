
/// inotify cron configuration implementation
/**
 * \file incroncfg.cpp
 * 
 * incron configuration
 * 
 * Copyright (C) 2007 Lukas Jelinek, <lukas@aiken.cz>
 * 
 * This program is free software; you can use it, redistribute
 * it and/or modify it under the terms of the GNU General Public
 * License, version 2 (see LICENSE-GPL).
 *  
 */


#include <fstream>
#include <sstream>

#include "incroncfg.h"


#define INCRON_CFG_DEFAULT "/etc/incron.conf"


typedef std::map<std::string, std::string> CFG_MAP;
typedef CFG_MAP::iterator CFG_ITER;


CFG_MAP IncronCfg::m_values;
CFG_MAP IncronCfg::m_defaults;


void IncronCfg::Init()
{
  m_defaults.insert(CFG_MAP::value_type("system_table_dir", "/etc/incron.d"));
  m_defaults.insert(CFG_MAP::value_type("user_table_dir", "/var/spool/incron"));
  m_defaults.insert(CFG_MAP::value_type("allowed_users", "/etc/incron.allow"));
  m_defaults.insert(CFG_MAP::value_type("denied_users", "/etc/incron.deny"));
  m_defaults.insert(CFG_MAP::value_type("lockfile_dir", "/var/run"));
  m_defaults.insert(CFG_MAP::value_type("lockfile_name", "incrond"));
  m_defaults.insert(CFG_MAP::value_type("editor", ""));
}

void IncronCfg::Load(const std::string& rPath)
{
  char s[1024];
  
  std::ifstream is(rPath.c_str());
  if (is.is_open()) {
    while (!is.eof() && !is.fail()) {
      is.getline(s, 1023);
      std::string key, val;
      if (ParseLine(s, key, val)) {
        m_values.insert(CFG_MAP::value_type(key, val));
      }
    }
    is.close();
    return;
  }
  
  if (rPath == INCRON_CFG_DEFAULT)
    return;
  
  is.open(INCRON_CFG_DEFAULT);
  if (is.is_open()) {
    while (!is.eof() && !is.fail()) {
      is.getline(s, 1023);
      std::string key, val;
      if (ParseLine(s, key, val)) {
        m_values.insert(CFG_MAP::value_type(key, val));
      }
    }
    is.close();
  }
}

bool IncronCfg::GetValue(const std::string& rKey, std::string& rVal)
{
  CFG_ITER it = m_values.find(rKey);
  if (it != m_values.end()) {
    rVal = (*it).second;
    return true;  
  }
  
  it = m_defaults.find(rKey);
  if (it != m_defaults.end()) {
    rVal = (*it).second;
    return true;  
  }
  
  return false;
}

bool IncronCfg::GetValue(const std::string& rKey, int& rVal)
{
  std::string s;
  if (GetValue(rKey, s)) {
    if (sscanf(s.c_str(), "%i", &rVal) == 1)
      return true; 
  }
  
  return false;
}

bool IncronCfg::GetValue(const std::string& rKey, unsigned& rVal)
{
  std::string s;
  if (GetValue(rKey, s)) {
    if (sscanf(s.c_str(), "%u", &rVal) == 1)
      return true; 
  }
  
  return false;
}

bool IncronCfg::GetValue(const std::string& rKey, bool& rVal)
{
  std::string s;
  if (GetValue(rKey, s)) {
    size_t len = (size_t) s.length();
    for (size_t i = 0; i < len; i++) {
      s[i] = (char) tolower(s[i]);
    }
    
    rVal = (s == "1" || s == "true" || s == "yes" || s == "on" || s == "enable" || s == "enabled");
    return true;
  }
  
  return false;
}

std::string IncronCfg::BuildPath(const std::string& rPath, const std::string& rName)
{
  if (rPath.rfind('/') == rPath.length() - 1)
    return rPath + rName;
    
  return rPath + "/" + rName;
}

bool IncronCfg::ParseLine(const char* s, std::string& rKey, std::string& rVal)
{
  // CAUTION: This code hasn't been optimized. It may be slow.
  
  char key[1024], val[1024];
  
  if (IsComment(s))
    return false;
  
  std::istringstream ss(s);
  ss.get(key, 1023, '=');
  if (ss.fail())
    return false;
    
  ss.get(val, 1023);
  if (ss.fail())
    return false;
    
  rKey = key;
  rVal = val;
    
  std::string::size_type a = rKey.find_first_not_of(" \t");
  std::string::size_type b = rKey.find_last_not_of(" \t");
  if (a == std::string::npos || b == std::string::npos)
    return false;
    
  rKey = rKey.substr(a, b-a+1);
  
  a = rVal.find_first_not_of(" \t=");
  b = rVal.find_last_not_of(" \t");
  if (a == std::string::npos || b == std::string::npos) {
    rVal = "";
  }
  else {
    rVal = rVal.substr(a, b-a+1);
  }
    
  return true;
}

bool IncronCfg::IsComment(const char* s)
{
  char* sx = strchr(s, '#');
  if (sx == NULL)
    return false;
    
  size_t len = sx - s;
  for (size_t i = 0; i < len; i++) {
    if (!(s[i] == ' ' || s[i] == '\t'))
      return false;
  }
  
  return true;
}

