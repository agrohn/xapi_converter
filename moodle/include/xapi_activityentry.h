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
#include <xapi_entry.h>
#include <xapi_anonymizer.h>
#include <iomanip>
#include <ctime>
#include <regex>
#include <nlohmann/json.hpp>
////////////////////////////////////////////////////////////////////////////////
extern const std::map<std::string, std::string> supportedVerbs;
extern std::map<std::string, std::string> activityTypes;
extern const std::map<std::string, std::string> moodleXapiActivity;
extern const std::map<std::string, std::string> contextModuleLocaleToActivityType;
extern std::map<std::string, std::string> TaskNameToTaskID;
extern std::map<std::string, std::string> UserNameToUserID;
extern std::map<std::string, std::string> UserIDToUserName;
extern std::map<std::string, std::string> UserIDToEmail;
extern std::map<std::string, std::string> GroupNameToGroupID;
extern std::map<std::string, std::string> GroupIDToGroupName;
extern XAPI::Anonymizer  anonymizer;
////////////////////////////////////////////////////////////////////////////////
namespace XAPI
{
  class ActivityEntry : public XAPI::Entry
  {
  public:

    // parses time structure from string.
    void ParseTimestamp(const std::string & strtime) override;
    void Parse(const std::string & line );
    void Parse(const std::vector<std::string> & lineAsVector );
    std::string ToXapiStatement() override;
    // updates user name and id mappings so they are found in actual run.
    void UpdateUserData();
  };
}
