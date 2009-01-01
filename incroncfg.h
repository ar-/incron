
/// inotify cron configuration header
/**
 * \file incroncfg.h
 * 
 * incron configuration
 * 
 * Copyright (C) 2007, 2008 Lukas Jelinek, <lukas@aiken.cz>
 * 
 * This program is free software; you can use it, redistribute
 * it and/or modify it under the terms of the GNU General Public
 * License, version 2 (see LICENSE-GPL).
 *  
 */


#ifndef INCRONCFG_H_
#define INCRONCFG_H_


#include <cstring>
#include <map>

/// Configuration class.
/**
 * This class provides access to values loaded from
 * a configuration file (either a explicitly specified one
 * or the default one).
 */
class IncronCfg
{
public:

  /// Initializes default configuration values.
  static void Init();

  /// Loads configuration values.
  /**
   * This method attempts to load configuration values
   * from the specified file. If it fails (e.g. the file
   * doesn't exist) the default file is read. As the last
   * resort (for the case the default file can't be loaded)
   * the hard-wired values are used.
   * 
   * \param[in] rPath configuration file path
   */
  static void Load(const std::string& rPath);

  /// Retrieves a configuration value.
  /**
   * This method attempts to find the appropriate configuration
   * value for the given key and stores it into the given
   * variable.
   * 
   * \param[in] rKey value key
   * \param[out] rVal retrieved value
   * \return true = success, false = failure (invalid key)
   */ 
  static bool GetValue(const std::string& rKey, std::string& rVal);
  
  /// Retrieves a configuration value.
  /**
   * This method attempts to find the appropriate configuration
   * value for the given key and stores it into the given
   * variable.
   * 
   * \param[in] rKey value key
   * \param[out] rVal retrieved value
   * \return true = success, false = failure (invalid key)
   */
  static bool GetValue(const std::string& rKey, int& rVal);
  
  /// Retrieves a configuration value.
  /**
   * This method attempts to find the appropriate configuration
   * value for the given key and stores it into the given
   * variable.
   * 
   * \param[in] rKey value key
   * \param[out] rVal retrieved value
   * \return true = success, false = failure (invalid key)
   */
  static bool GetValue(const std::string& rKey, unsigned& rVal);
  
  /// Retrieves a configuration value.
  /**
   * This method attempts to find the appropriate configuration
   * value for the given key and stores it into the given
   * variable.
   * 
   * \param[in] rKey value key
   * \param[out] rVal retrieved value
   * \return true = success, false = failure (invalid key)
   */
  static bool GetValue(const std::string& rKey, bool& rVal);
  
  /// Builds a file path.
  /**
   * This function composes a path from a base path and a file name.
   * 
   * \param[in] rPath base path
   * \param[in] rName file name
   * \return full path
   */
  static std::string BuildPath(const std::string& rPath, const std::string& rName);
  
protected:
  /// Parses a line a attempts to get a key and a value.
  /**
   * \param[in] s text line
   * \param[out] rKey key
   * \param[out] rVal value
   * \return true = success, false = failure
   */
  static bool ParseLine(const char* s, std::string& rKey, std::string& rVal);
  
  /// Checks whether a line is a comment.
  /**
   * \param[in] s text line
   * \return true = comment, false = otherwise
   */
  static bool IsComment(const char* s);

private:
  static std::map<std::string, std::string> m_values;
  static std::map<std::string, std::string> m_defaults;
};

#endif /*INCRONCFG_H_*/
