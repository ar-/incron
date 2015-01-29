#ifndef _EXECUTOR_H_
#define _EXECUTOR_H_
/**
 * \file executor.h
 * 
 * Copyright (C) 2015 Andreas Altair Redmer, <altair.ibn.la.ahad.sy@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of one of the following licenses:
 *
 * \li 1. GNU Lesser General Public License, version 2.1 (see LICENSE-LGPL)
 * \li 2. GNU General Public License, version 2  (see LICENSE-GPL)
 *
 * If you want to help with choosing the best license for you,
 * please visit http://www.gnu.org/licenses/license-list.html.
 * 
 */

#include <iostream>
#include <string>
#include <vector>
#include <sstream>

class Executor
{
	private:
	
		inline static std::string trim_right_copy(
		  const std::string& s,
		  const std::string& delimiters = " \f\n\r\t\v" )
		{
			//cout << "trc" << endl;
			if (s.length()==0)
				return "";
			//cout << "trc" <<s.find_last_not_of( delimiters ) + 1 << endl;
			return s.substr( 0, s.find_last_not_of( delimiters ) + 1 );
		}

		inline static std::string trim_left_copy(
		  const std::string& s,
		  const std::string& delimiters = " \f\n\r\t\v" )
		{
			//cout << "tlc" << endl;
			if (s.length()==0)
				return "";
			//cout << "tlc" << s.find_first_not_of( delimiters ) << " -- "<< delimiters << endl;
			return s.substr( s.find_first_not_of( delimiters ) );
		}

		inline static std::string trim_copy(
		  const std::string& s,
		  const std::string& delimiters = " \f\n\r\t\v\'\"-" )
		{
			//cout << "tc" << endl;
			if (s.length()==0)
				return "";
			return trim_left_copy( trim_right_copy( s, delimiters ), delimiters );
		}
		
	public:
//		static std::string plain_exec(std::string cmd) 
//		{
//			return plain_exec(cmd.c_str());
//		}
		
		static std::string plain_exec(char* cmd) 
		{
			//cout << "plain_exec " <<cmd<< endl;
			FILE* pipe = popen(cmd, "r");
			if (!pipe) return "EXECPIPEERROR";
			char buffer[128];
			std::string result = "";
			while(!feof(pipe)) {
				if(fgets(buffer, 128, pipe) != NULL)
					result += buffer;
			}
			pclose(pipe);
			//cout << "plain_exec end" << endl;
			//return result;
			return trim_copy(result);
		}
				
		
//		static const vector<string> execBashVec (string script);
//		static const string execBash (string script);

		static const std::vector<std::string> execBashVec (std::string script)
		{
			std::string s = execBash (script);
			// from split
			std::vector<std::string> elems;
				std::stringstream ss(s);
				std::string item;
				while (std::getline(ss, item, '\n')) {
					elems.push_back(item);
				}
			return elems;
		}

		static const std::string execBash (std::string script)
		{
			//cout << "execBash: " << script << endl;
			std::string ret = plain_exec ((char*) script.c_str());
			return ret;
		}
	
};

#endif //_EXECUTOR_H_
