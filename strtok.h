
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

/// Simple string tokenizer class.
/**
 * This class implements a string tokenizer. It splits a string
 * by a character to a number of elements (tokens) which are
 * provided sequentially.
 * 
 * All operations are made on the original string itself.
 * The implementation is not ready to handle any changes of the
 * string.
 * 
 * The original string is left unchanged. All tokens are returned
 * as newly created strings.
 */
class StringTokenizer
{
public:
  /// Constructor.
  /**
   * Creates a ready-to-use tokenizer.
   * 
   * \param[in] rStr string for tokenizing
   * \param[in] cDelim delimiter (separator) character
   */
  StringTokenizer(const std::string& rStr, char cDelim = ',');
  
  /// Destructor.
  ~StringTokenizer() {}
  
  /// Checks whether the tokenizer can provide more tokens.
  /**
   * \return true = more tokens available, false = otherwise
   */
  inline bool HasMoreTokens() const
  {
    return m_pos < m_len;
  }
  
  /// Returns the next token.
  /**
   * \return next token or "" if no more tokens available
   */
  std::string GetNextToken();
  
  /// Sets a delimiter (separator) character.
  /**
   * The new delimiter has effect only to tokens returned later;
   * the position in the string is not affected.
   * 
   * \param[in] cDelim delimiter character
   */
  inline void SetDelimiter(char cDelim)
  {
    m_cDelim = cDelim;
  }
  
  /// Returns the delimiter (separator) character.
  /**
   * \return delimiter character
   */
  inline char GetDelimiter() const
  {
    return m_cDelim;
  }
  
  /// Resets the tokenizer.
  /**
   * Re-initializes tokenizing to the start of the string.
   */
  inline void Reset()
  {
    m_pos = 0;
  }
  
private:
  std::string m_str;            ///< tokenized string
  char m_cDelim;                ///< delimiter character
  std::string::size_type m_pos; ///< current position
  std::string::size_type m_len; ///< string length
};


#endif //_STRTOK_H_
