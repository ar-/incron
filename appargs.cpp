
/// application arguments processor implementation
/**
 * \file appargs.cpp
 * 
 * application arguments processor
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


#include "strtok.h"

#include "appargs.h"


size_t AppArgs::s_minCnt = 0;
size_t AppArgs::s_maxCnt = APPARGS_NOLIMIT;

AA_LONG_MAP AppArgs::s_longMap;
AA_SHORT_MAP AppArgs::s_shortMap;
AA_VAL_LIST AppArgs::s_valList;


void AppArgs::Init(size_t valMinCnt, size_t valMaxCnt)
{
  s_minCnt = valMinCnt;
  s_maxCnt = valMaxCnt;
}

void AppArgs::Destroy()
{
  AA_LONG_MAP::iterator it = s_longMap.begin();
  while (it != s_longMap.end()) {
    delete (*it).second;
    it++;
  }
  
  s_longMap.clear();
  s_shortMap.clear();
  s_valList.clear();
}

void AppArgs::Parse(int argc, const char* const* argv)
{
  for (int i=1; i<argc; i++) {
    // this shouldn't occur
    if (argv[i] == NULL)
      return;
      
    if (IsOption(argv[i])) {
      if (IsLongOption(argv[i])) {
        std::string name, val;
        bool hasVal;
        if (ParseLong(argv[i], name, val, hasVal)) {
          AA_LONG_MAP::iterator it = s_longMap.find(name);
          if (it != s_longMap.end()) {
            AppArgOption_t* pOpt = (*it).second;
            pOpt->found = true;
            pOpt->hasVal = hasVal;
            pOpt->val = val;
          }
        }
      }
      else {
        char name;
        std::string val;
        bool hasVal;
        ParseShort(argv[i], name, val, hasVal);
        AA_SHORT_MAP::iterator it = s_shortMap.find(name);
        if (it != s_shortMap.end()) {
          AppArgOption_t* pOpt = (*it).second;
          pOpt->found = true;
          if (hasVal) {
            pOpt->hasVal = true;
            pOpt->val = val;
          }
          else {
            if (i+1 < argc && !IsOption(argv[i+1])) {
              pOpt->hasVal = true;
              pOpt->val = argv[i+1];
              i++;
            }
            else {
              pOpt->hasVal = false;
            }
          }
        }
      }
    }
    else {
      s_valList.push_back(argv[i]);
    }
  }
}

bool AppArgs::IsValid()
{
  size_t size = s_valList.size();
  if (size < s_minCnt || size > s_maxCnt)
    return false;
    
  AA_LONG_MAP::iterator it = s_longMap.begin();
  while (it != s_longMap.end()) {
    AppArgOption_t* pOpt = (*it).second;
    if (pOpt->mand && !(pOpt->found))
      return false;
    if (pOpt->type == AAT_MANDATORY_VALUE && pOpt->found && !(pOpt->hasVal))
      return false;
    it++;
  }
  
  return true;
}

bool AppArgs::ExistsOption(const std::string& rArg)
{
  AA_LONG_MAP::iterator it = s_longMap.find(rArg);
  return it != s_longMap.end() && (*it).second->found;
}

bool AppArgs::GetOption(const std::string& rArg, std::string& rVal)
{
  AA_LONG_MAP::iterator it = s_longMap.find(rArg);
  if (it == s_longMap.end())
    return false;
    
  AppArgOption_t* pOpt = (*it).second; 
  if (!(pOpt->found) || !(pOpt->hasVal))
    return false;
    
  rVal = pOpt->val;
  return true;
}

bool AppArgs::AddOption(const std::string& rName, char cShort, AppArgType_t type, bool fMandatory)
{
  if (s_longMap.find(rName) != s_longMap.end() || s_shortMap.find(cShort) != s_shortMap.end())
    return false;
  
  AppArgOption_t* pOpt = new AppArgOption_t();
  pOpt->found = false;
  pOpt->hasVal = false;
  pOpt->mand = fMandatory;
  pOpt->type = type;
  
  s_longMap.insert(AA_LONG_MAP::value_type(rName, pOpt));
  s_shortMap.insert(AA_SHORT_MAP::value_type(cShort, pOpt));
  return true;
}

size_t AppArgs::GetValueCount()
{
  return s_valList.size();
}

bool AppArgs::GetValue(size_t index, std::string& rVal)
{
  if (index > s_valList.size())
    return false;
    
  rVal = s_valList[index];
  return true;
}

void AppArgs::Dump()
{
  fprintf(stderr, "Options:\n");
  AA_LONG_MAP::iterator it = s_longMap.begin();
  while (it != s_longMap.end()) {
    AppArgOption_t* pOpt = (*it).second;
    AA_SHORT_MAP::iterator it2 = s_shortMap.begin();
    while (it2 != s_shortMap.end()) {
      if ((*it2).second == pOpt) {
        DumpOption((*it).first, (*it2).first, pOpt);
      }
      it2++;
    }      
    it++;
  }
  
  fprintf(stderr, "Values:\n");
  
  AA_VAL_LIST::iterator it3 = s_valList.begin();
  while (it3 != s_valList.end()) {
    fprintf(stderr, "%s\n", (*it3).c_str());
    it3++;
  }  
}



bool AppArgs::IsOption(const char* pchStr)
{
  if (strlen(pchStr) < 2)
    return false;
    
  return pchStr[0] == '-';
}

bool AppArgs::IsLongOption(const char* pchStr)
{
  return pchStr[1] == '-';
}

bool AppArgs::ParseLong(const char* pchStr, std::string& rName, std::string& rVal, bool& rfHasVal)
{
  StringTokenizer tok(pchStr+2, '=');
  if (!tok.HasMoreTokens())
    return false;
  
  rName = tok.GetNextToken();
  if (!tok.HasMoreTokens()) {
    rfHasVal = false;
    return true;
  }
    
  rVal = tok.GetRemainder();
  rfHasVal = true;
  return true;
}

void AppArgs::ParseShort(const char* pchStr, char& rcName, std::string& rVal, bool& rfHasVal)
{
  rcName = pchStr[1];
  size_t len = strlen(pchStr);
  if (len == 2) {
    rfHasVal = false;
    return;
  }
  
  rVal = pchStr + 2;
  rfHasVal = true;  
}

void AppArgs::DumpOption(const std::string& rName, char cShort, AppArgOption_t* pOpt)
{
  const char* s;
  switch (pOpt->type) {
    case AAT_NO_VALUE: s = "no value";
      break;
    case AAT_OPTIONAL_VALUE: s = "optional value";
      break;
    case AAT_MANDATORY_VALUE: s = "mandatory value";
      break;
    default:;
      s = "unknown";
  }
  
  fprintf(stderr, "long='%s', short='%c', type='%s', found=%s, has_value=%s, value='%s'\n",
      rName.c_str(),
      cShort,
      s,
      pOpt->found ? "YES" : "NO",
      pOpt->hasVal ? "YES" : "NO",
      pOpt->val.c_str()
  );
}


