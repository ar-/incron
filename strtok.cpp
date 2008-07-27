
/// string tokenizer implementation
/**
 * \file strtok.cpp
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


#include "strtok.h"

typedef std::string::size_type SIZE;

StringTokenizer::StringTokenizer(const std::string& rStr, char cDelim)
{
  m_str = rStr;
  m_cDelim = cDelim;
  m_pos = 0;
  m_len = rStr.length();
}
    
std::string StringTokenizer::GetNextToken()
{
  std::string s;
  
  for (SIZE i=m_pos; i<m_len; i++) {
    if (m_str[i] == m_cDelim) {
      s = m_str.substr(m_pos, i - m_pos);
      m_pos = i + 1;
      return s;
    }    
  }
  
  s = m_str.substr(m_pos);
  m_pos = m_len;
  return s;
}
