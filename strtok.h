
/// string tokenizer header
/**
 * \file strtok.h
 * 
 * string tokenizer
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


#ifndef _STRTOK_H_
#define _STRTOK_H_


#include <string>


class StringTokenizer
{
public:
  StringTokenizer(const std::string& rStr, char cDelim = ',');
  
  ~StringTokenizer() {}
  
  inline bool HasMoreTokens() const
  {
    return m_pos < m_len;
  }
  
  std::string GetNextToken();
  
  inline void SetDelimiter(char cDelim)
  {
    m_cDelim = cDelim;
  }
  
  inline char GetDelimiter() const
  {
    return m_cDelim;
  }
  
  inline void Reset()
  {
    m_pos = 0;
  }
  
private:
  std::string m_str;
  char m_cDelim;
  std::string::size_type m_pos;
  std::string::size_type m_len;
};


#endif //_STRTOK_H_
