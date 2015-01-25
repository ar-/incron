
/// string tokenizer header
/**
 * \file strtok.h
 * 
 * string tokenizer
 * 
 * Copyright (C) 2006, 2007, 2008 Lukas Jelinek, <lukas@aiken.cz>
 * Copyright (C) 2014, 2015 Andreas Altair Redmer, <altair.ibn.la.ahad.sy@gmail.com>
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

typedef std::string::size_type SIZE;

/// Simple string tokenizer class.
/**
 * This class implements a string tokenizer. It splits a string
 * by a character to a number of elements (tokens) which are
 * provided sequentially.
 * 
 * All operations are made on a copy of the original string
 * (which may be in fact a copy-on-write instance).
 * 
 * The original string is left unchanged. All tokens are returned
 * as newly created strings.
 * 
 * There is possibility to specify a prefix character which
 * causes the consecutive character is not considered as
 * a delimiter. If you don't specify this character (or specify
 * the NUL character, 0x00) this feature is disabled. The mostly
 * used prefix is a backslash ('\').
 * 
 * This class is not thread-safe.
 * 
 * Performance note: This class is currently not intended
 * to be very fast. Speed optimizations will be done later.
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
   * \param[in] cPrefix character which is prepended if a
   *            character must not separate tokens
   */
  StringTokenizer(const std::string& rStr, const std::string& cDelim = ",", char cPrefix = '\0');
  
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
   * If a prefix is defined it is stripped from the returned
   * string (e.g. 'abc\ def' is transformed to 'abc def'
   * while the prefix is '\').
   * 
   * \param[in] fSkipEmpty skip empty strings (more consecutive delimiters)
   * \return next token or "" if no more tokens available
   * 
   * \sa GetNextTokenRaw()
   */
  std::string GetNextToken(bool fSkipEmpty = false);
  
  /// Returns the next token.
  /**
   * This method always returns an unmodified string even
   * if it contains prefix characters.
   * 
   * \param[in] fSkipEmpty skip empty strings (more consecutive delimiters)
   * \return next token or "" if no more tokens available
   * 
   * \sa GetNextToken()
   */
  std::string GetNextTokenRaw(bool fSkipEmpty = false);
  
  /// Returns the remainder of the source string.
  /**
   * This method returns everything what has not been
   * processed (tokenized) yet and moves the current
   * position to the end of the string.
   * 
   * If a prefix is defined it is stripped from
   * the returned string.
   * 
   * \return remainder string
   */
  std::string GetRemainder();
    
  /// Sets a delimiter (separator) character.
  /**
   * The new delimiter has effect only to tokens returned later;
   * the position in the string is not affected.
   * 
   * If you specify a NUL character (0x00) here the prefix
   * will not be used.
   * 
   * \param[in] cDelim delimiter character
   */
  inline void SetDelimiter(std::string cDelim)
  {
    m_cDelim = cDelim;
  }
  
  /// Returns the delimiter (separator) character.
  /**
   * \return delimiter character
   */
  inline std::string GetDelimiter() const
  {
    return m_cDelim;
  }
  
  /// Sets a prefix character.
  /**
   * The new prefix has effect only to tokens returned later;
   * the position in the string is not affected.
   * 
   * \param[in] cPrefix prefix character
   * 
   * \sa SetNoPrefix()
   */
  inline void SetPrefix(char cPrefix)
  {
    m_cPrefix = cPrefix;
  }
  
  /// Returns the prefix character.
  /**
   * \return prefix character
   */
  inline char GetPrefix() const
  {
    return m_cPrefix;
  }
  
  /// Sets the prefix to 'no prefix'.
  /**
   * Calling this method is equivalent to SetPrefix((char) 0).
   * 
   * \sa SetPrefix()
   */
  inline void SetNoPrefix()
  {
    SetPrefix('\0');
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
  std::string m_cDelim;                ///< delimiter character
  char m_cPrefix;               ///< prefix character
  std::string::size_type m_pos; ///< current position
  std::string::size_type m_len; ///< string length
  
  /// Strips all prefix characters.
  /**
   * \param[in] s source string
   * \param[in] cnt string length
   * \return modified string
   */
  std::string StripPrefix(const char* s, SIZE cnt);
  
  /// Extracts the next token (internal method).
  /**
   * The extracted token may be empty.
   * 
   * \param[out] rToken extracted token
   * \param[in] fStripPrefix strip prefix characters yes/no
   */
  void _GetNextToken(std::string& rToken, bool fStripPrefix);
  
  /// Extracts the next token (internal method).
  /**
   * This method does no checking about the prefix character.
   * 
   * The extracted token may be empty.
   * 
   * \param[out] rToken extracted token
   */
  void _GetNextTokenNoPrefix(std::string& rToken);
  
  /// Extracts the next token (internal method).
  /**
   * This method does checking about the prefix character.
   * 
   * The extracted token may be empty.
   * 
   * \param[out] rToken extracted token
   */
  void _GetNextTokenWithPrefix(std::string& rToken);
};


#endif //_STRTOK_H_
