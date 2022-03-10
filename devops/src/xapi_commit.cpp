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
#include <xapi_commit.h>
#include <algorithm>
#include <xapi_errors.h>
#include <xapi_anonymizer.h>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <locale>
////////////////////////////////////////////////////////////////////////////////
using namespace boost;
using namespace std;
using json = nlohmann::json;
extern XAPI::Anonymizer anonymizer;
extern std::map<std::string, std::string> UserNameToUserID;
extern std::map<std::string, std::string> UserIDToUserName;
extern std::map<std::string, std::string> UserIDToEmail;
////////////////////////////////////////////////////////////////////////////////
XAPI::CommitEntry::CommitEntry()
{

}
////////////////////////////////////////////////////////////////////////////////
XAPI::CommitEntry::~CommitEntry()
{

}
void
XAPI::CommitEntry::ParseTimestamp(const std::string & strtime)
{
  //2021-03-25T09:21:20Z
  // 
  {
    stringstream ss;
    ss << strtime.substr(0,4);
    if ( !(ss >> when.tm_year)  ) throw runtime_error("Conversion error, year");
  }
  // 
  {
    stringstream ss;
    ss << strtime.substr(5,2);
    if ( !(ss >> when.tm_mon)  ) throw runtime_error("Conversion error, month");
  }
  // 
  {
    stringstream ss;
    ss << strtime.substr(8,2);
    if ( !(ss >> when.tm_mday)  ) throw runtime_error("Conversion error, day");
  }

  {
    stringstream ss;
    ss << strtime.substr(11,2);
    if ( !(ss >> when.tm_hour)  ) throw runtime_error("Conversion error, hour");
  }
  
  {
    stringstream ss;
    ss << strtime.substr(14,2);
    if ( !(ss >> when.tm_min)  ) throw runtime_error("Conversion error, minute");
  }
  {
    stringstream ss;
    ss << strtime.substr(17,2);
    if ( !(ss >> when.tm_sec)  ) throw runtime_error("Conversion error, second");
  }
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::CommitEntry::Parse( const nlohmann::json & devopsCommitItem) 
{
  std::string date= devopsCommitItem["committer"]["date"];
  ParseTimestamp(date);
  
  username = devopsCommitItem["committer"]["name"];
  userid = devopsCommitItem["committer"]["email"];
  event = devopsCommitItem["remoteUrl"];

#pragma omp critical
  {
    UserIDToEmail[userid] = anonymizer(userid);
  }
  #pragma omp critical
  {
    UserIDToUserName[userid] = anonymizer(username); 
  }
  //component = vec.at(3); // memo type
  //course_name = vec.at(5);
  //course_id = vec.at(3);
}
////////////////////////////////////////////////////////////////////////////////
std::string
XAPI::CommitEntry::ToXapiStatement()
{
  json statement;
  json actor;
  json verb;
  json object;
  json result;
  json extensions;
  /*if ( UserNameToUserID.find(username) == UserNameToUserID.end())
  {
    throw xapi_cached_user_not_found_error(username);
    }*/
  
  //string userid = UserNameToUserID[username];
  // define user 
  actor = CreateActorJson(userid);
  
  // construct attend verb
  string verbname;
  string verb_xapi_id;

  //event = tolower(event);

  verbname = "created";
  verb_xapi_id = "http://activitystrea.ms/schema/1.0/create";

  // convert component string to lowercase 
  transform(component.begin(), component.end(), component.begin(), ::tolower);

  // duration needs to be encoded using ISO8601
  // https://en.wikipedia.org/wiki/ISO_8601
  
  verb = {
    {"id", verb_xapi_id },
    {"display",
     {
       {"en-GB", verbname} // this will probably depend on subtype (see object)
     }
    }
  };
  
  // define object result relates to
  object = {
    { "objectType", "Activity"},
    { "id",  event},
    { "definition",
      {
       { "description",
         {
          { "en-GB", "Commit"}
         }
       },
       { "type" , "http://id.tincanapi.com/activitytype/code-commit"}
      }
    }
  };
    
  json activityContext;
  json grouping = {
    { "grouping", {
	{
	  { "objectType", "Activity" },
	  { "id", course_id },
	  { "definition",
	    {
	      { "description",
		{
		  { "en-GB", course_name}
		}
	      },
	      { "type", "http://adlnet.gov/expapi/activities/course"}
	    }
	  }
	}
      }
    }
  };
  
  activityContext =  {
    { "contextActivities", grouping }
  };
  
  statement["actor"] = actor;
  statement["verb"] = verb;
  statement["timestamp"] = GetTimestamp();
  statement["object"] = object;
  statement["context"] = activityContext;

  return statement.dump();
}
////////////////////////////////////////////////////////////////////////////////
