/*
  This file is part of xapi_converter.  
  Copyright (C) 2018-2019 Anssi Gröhn
  
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#pragma once
#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <regex>
#include <json.hpp>
////////////////////////////////////////////////////////////////////////////////
extern const std::map<std::string, std::string> supportedVerbs;
extern std::map<std::string, std::string> activityTypes;
extern std::map<std::string, std::string> TaskNameToTaskID;
extern std::map<std::string, std::string> UserNameToUserID;
extern std::map<std::string, std::string> UserIDToUserName;
////////////////////////////////////////////////////////////////////////////////
namespace XAPI
{

  class Entry
  {
  protected:
    Entry();
  public:
    struct tm when {0};
    nlohmann::json statement;
    
    // do we need these?
    std::string username;
    std::string related_username;
    std::string userid;
    std::string related_userid;
    std::string context; // generally the course
    std::string context_module_locale_specific; ///< prefix for context field, changes with locale settings. 
    std::string component;
    std::string event;
    std::string verb; // xapi-related
    std::string description; // moodle event id description
    std::string ip_address;

    std::string course_name; ///< Used to identify course (human-readable)
    std::string course_id; ///< Used to identify course (unique url).
  public:    
    virtual ~Entry();
    // parses time structure from string.
    virtual void ParseTimestamp(const std::string & strtime) = 0;

    /// Returns string as yyyy-mm-ddThh:mmZ
    std::string GetTimestamp() const;

    virtual std::string ToXapiStatement() = 0;

  };
}