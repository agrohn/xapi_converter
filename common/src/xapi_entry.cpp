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
#include <xapi_errors.h>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
extern std::map<std::string, std::string> UserIDToUserName;
extern std::map<std::string, std::string> UserIDToEmail;
using namespace boost;
using json = nlohmann::json;
using namespace std;
////////////////////////////////////////////////////////////////////////////////
XAPI::Entry::Entry() {}
////////////////////////////////////////////////////////////////////////////////
XAPI::Entry::~Entry() {}
////////////////////////////////////////////////////////////////////////////////
/// Returns string as yyyy-mm-ddThh:mmZ
std::string
XAPI::Entry::GetTimestamp() const
{
  stringstream ss;
  ss << when.tm_year ;
  ss << "-" ;
  ss << std::setfill('0') << std::setw(2) << when.tm_mon;
  ss << "-";
  ss << std::setfill('0') << std::setw(2) << when.tm_mday;
  ss << "T";
  ss << std::setfill('0') << std::setw(2) << when.tm_hour;
  ss << ":";
  ss << std::setfill('0') << std::setw(2) << when.tm_sec;
  ss << "Z";
  return ss.str();
}
////////////////////////////////////////////////////////////////////////////////
nlohmann::json
XAPI::Entry::CreateActorJson( const std::string & userid )
{

  json user;
  string email;
  string username;
  // okay, since exceptions thrown within openmp constructs should be caught
  // within that same section, we need workaround for this situation.
  // https://stackoverflow.com/questions/13663231/throwing-a-c-exception-inside-an-omp-critical-section
  bool errorState = false;
  // in case some thread is modifying a map - it should not happen anymore, but still.
  #pragma omp critical
  {
    if ( UserIDToEmail.find(userid) == UserIDToEmail.end()) errorState = true;
    if ( UserIDToUserName.find(userid) == UserIDToUserName.end()) errorState = true;
    if ( errorState == false )
    {
	email = string("mailto:")+UserIDToEmail[userid];
	username = UserIDToUserName[userid];
    }
  }
  if ( errorState ) throw xapi_cached_user_not_found_error(userid);
  // build json 
  user = {
            { "objectType", "Agent"},
            { "name", username },
            { "mbox", email }
  };
  
  return user;
}
