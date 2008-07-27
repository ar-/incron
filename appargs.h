
/// application arguments processor header
/**
 * \file appargs.h
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
 

#ifndef APPARGS_H_
#define APPARGS_H_

#include <string>
#include <map>
#include <deque>



/// Option argument type
typedef enum
{
  AAT_NO_VALUE,       ///< no value needed
  AAT_OPTIONAL_VALUE, ///< optional value
  AAT_MANDATORY_VALUE ///< mandatory value
} AppArgType_t;


#define APPARGS_NOLIMIT 0x7fffffff ///< value count has no limit

/// Argument option type
typedef struct
{
  AppArgType_t type;  ///< argument type
  bool mand;          ///< mandatory yes/no
  bool found;         ///< found in argument vector
  std::string val;    ///< value
  bool hasVal;        ///< value is set 
} AppArgOption_t;


/// Mapping from long option name to option data
typedef std::map<std::string, AppArgOption_t*> AA_LONG_MAP;

/// Mapping from short option name to option data
typedef std::map<char, AppArgOption_t*> AA_SHORT_MAP;

/// Value list type
typedef std::deque<std::string> AA_VAL_LIST;


/// Application arguments
/**
 * This class is set-up for processing command line arguments.
 * Then it parses these arguments and builds data which
 * can be queried later.
 * 
 * There are two categories of arguments:
 * \li options (a.k.a. switches)
 * \li values
 * 
 * Options represent changeable parameters of the application.
 * Values are a kind of input data.
 * 
 * Each option has one of the following types:
 * \li no value (two-state logic, e.g. running on foreground/background)
 * \li optional value (e.g. for logging: another file than default can be specified)
 * \li mandatory value (e.g. custom configuration file)
 * 
 * Each option always have two forms - long one (introcuded by
 * two hyphens, e.g. --edit) and short one (introduced by one
 * hyphen, e.g. -e). These forms are functionally equivalent.
 * 
 * Unknown options are silently ignored.
 */
class AppArgs
{
public:
  /// Initializes the processor.
  /**
   * \param[in] valMinCnt minimum count of values
   * \param[in] valMaxCnt maximum number of values (no effect if lower than valMinCnt)
   */ 
  static void Init(size_t valMinCnt = 0, size_t valMaxCnt = APPARGS_NOLIMIT);

  /// Releases resources allocated by the processor.
  /**
   * This method should be called if the argument values are
   * no longer needed.
   */ 
  static void Destroy();

  /// Parses arguments and builds the appropriate structure.
  /**
   * \param[in] argc argument count
   * \param[in] argv argument vector
   * 
   * \attention All errors are silently ignored.
   */ 
  static void Parse(int argc, const char* const* argv);
  
  /// Checks whether the arguments have valid form.
  /**
   * Arguments are valid if:
   * \li all mandatory options are present
   * \li all options with mandatory values have their values
   * \li value count is between its minimum and maximum
   * \li there are no unknown options (if unknown options are not accepted)
   * 
   * \return true = arguments valid, false = otherwise
   */ 
  static bool IsValid();
  
  /// Checks whether an option exists.
  /**
   * \param[in] rArg long option name
   * \return true = option exists, false = otherwise
   */
  static bool ExistsOption(const std::string& rArg);
  
  /// Extracts an option value.
  /**
   * \param[in] rArg long option name
   * \param[out] rVal option value
   * \return true = value extracted, false = option not found or has no value
   */
  static bool GetOption(const std::string& rArg, std::string& rVal);

  /// Adds an option.
  /**
   * This method is intended to be called between initilization
   * and parsing. It adds an option which may (or must) occur
   * inside the argument vector.
   * 
   * \param[in] rName long option name
   * \param[in] cShort short (one-character) option name
   * \param[in] type argument type
   * \param[in] fMandatory option is mandatory yes/no
   * \return true = success, false = failure (e.g. option already exists)
   */  
  static bool AddOption(const std::string& rName, char cShort, AppArgType_t type, bool fMandatory);
  
  /// Returns the count of values.
  /**
   * \return count of values
   */
  static size_t GetValueCount();
  
  /// Extracts a value.
  /**
   * \param[in] index value index
   * \param[out] rVal extracted value
   * \return true = value extracted, false = otherwise
   */
  static bool GetValue(size_t index, std::string& rVal);
  
  /// Dumps information about options and value to STDERR.
  /**
   * \attention This method may be very slow.
   */
  static void Dump();  
  
protected:
  /// Checks whether a string is an option.
  /**
   * \param[in] pchStr text string
   * \return true = option, false = otherwise
   */
  static bool IsOption(const char* pchStr);
  
  /// Checks whether a string is a long option.
  /**
   * This methos assumes the string is an option
   * (if not the behavior is undefined).
   * 
   * \param[in] pchStr text string
   * \return true = option, false = otherwise
   */
  static bool IsLongOption(const char* pchStr);
  
  /// Parses a string and attempts to treat it as a long option.
  /**
   * \param[in] pchStr text string
   * \param[out] rName option name
   * \param[out] rVal value string
   * \param[out] rfHasVal option has value yes/no
   * \return true = success, false = failure
   */
  static bool ParseLong(const char* pchStr, std::string& rName, std::string& rVal, bool& rfHasVal);
  
  /// Parses a string and attempts to treat it as a short option.
  /**
   * \param[in] pchStr text string
   * \param[out] rcName option name
   * \param[out] rVal value string
   * \param[out] rfHasVal option has value yes/no
   * 
   * \attention This method assumes the string is a valid short option.
   */
  static void ParseShort(const char* pchStr, char& rcName, std::string& rVal, bool& rfHasVal);

  /// Dumps an option to STDERR.
  /**
   * \param[in] rName long option name
   * \param[in] cShort short option name
   * \param[in] pOpt option data
   */
  static void DumpOption(const std::string& rName, char cShort, AppArgOption_t* pOpt);
  
  
private:
  static size_t s_minCnt;         ///< minimum value count
  static size_t s_maxCnt;         ///< maximum value count

  static AA_LONG_MAP s_longMap;   ///< mapping from long names to data
  static AA_SHORT_MAP s_shortMap; ///< mapping from short names to data
  static AA_VAL_LIST s_valList;   ///< value list
  
};


#endif /*APPARGS_H_*/
