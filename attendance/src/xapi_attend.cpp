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
#include <xapi_attend.h>
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
XAPI::AttendanceEntry::AttendanceEntry()
{

}
////////////////////////////////////////////////////////////////////////////////
XAPI::AttendanceEntry::~AttendanceEntry()
{

}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::AttendanceEntry::ParseTimestamp(const string & strtime) 
{
  std::vector<string> tokens;
  boost::split( tokens, strtime, boost::is_any_of("-"));
  if ( tokens.size() != 3 )
    throw runtime_error("invalid date format : '"+strtime+string("'"));

  {
    stringstream ss;
    ss << tokens.at(2);
    if ( !(ss >> when.tm_mday) ) throw runtime_error("Conversion error, day");
  }
  {
    stringstream ss;
    ss << tokens.at(1);
      
    if ( !(ss >> when.tm_mon)  ) throw runtime_error("Conversion error, mon");
    // sanity check for month value
    if ( when.tm_mon < 1 || when.tm_mon > 12 )
    {
      stringstream smsg;
      smsg << "Conversion error in attendance, month value out of range 1-12.";
      smsg << "Got: '" << tokens.at(3) << "', value is : " << when.tm_mon;
      throw runtime_error(smsg.str());
    }
  }
  {
    stringstream ss;
    ss << tokens.at(0);
    if ( !(ss >> when.tm_year)  ) throw runtime_error("Conversion error, year");
  }
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::AttendanceEntry::Parse( const std::vector<std::string> & vec ) 
{
  
  // break line into comma-separated parts.
  if ( vec.at(2) != "NULL" && !vec.at(2).empty() )
    ParseTimestamp(vec.at(2));
  else
    throw runtime_error(string("Invalid timestamp ")+vec.at(2));
  
  userid = vec.at(1); // gradee student email
  // convert to lower-case 
  for ( auto & c : userid ) c = tolower(c);


  #pragma omp critical
  {
    UserIDToEmail[userid] = anonymizer(userid);
  }
  #pragma omp critical
  {
    UserIDToUserName[userid] = anonymizer(vec.at(0)); 
  }
  
  event = vec.at(4); // lecture/class that was attended
  component = vec.at(3); // attendance type
  //course_name = vec.at(5);
  //course_id = vec.at(3);
}
////////////////////////////////////////////////////////////////////////////////
std::string
XAPI::AttendanceEntry::ToXapiStatement()
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
  string lecture_id = "http://online-lecture/id="+event;
  //event = tolower(event);

  verbname = "attended";
  verb_xapi_id = "http://activitystrea.ms/schema/1.0/attend";

  // convert component string to lowercase 
  transform(component.begin(), component.end(), component.begin(), ::tolower);

  // duration needs to be encoded using ISO8601
  // https://en.wikipedia.org/wiki/ISO_8601
  if( component == "x" )
  {
    extensions["http://id.tincanapi.com/extension/severity"] = "live";
    extensions["http://id.tincanapi.com/extension/duration"] = "PT90M";
  }
  else if ( component == "v" )
  {
    extensions["http://id.tincanapi.com/extension/severity"] = "online";
    extensions["http://id.tincanapi.com/extension/duration"] = "PT90M";
  }
  else if ( component == "vo" )
  {
    extensions["http://id.tincanapi.com/extension/severity"] = "online";
    extensions["http://id.tincanapi.com/extension/duration"] = "PT45M";
  }
  else if ( component == "voo" )
  {
    extensions["http://id.tincanapi.com/extension/severity"] = "online";
    extensions["http://id.tincanapi.com/extension/duration"] = "PT60M";
  }
  else
  {
    verbname = "skipped";
    extensions["http://id.tincanapi.com/extension/severity"] = "skipped";
    extensions["http://id.tincanapi.com/extension/duration"] = "PT0M";
    verb_xapi_id = "http://id.tincanapi.com/verb/skipped";
  }; 
  
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
          { "en-GB", "Lecture"}
         }
       },
       { "type" , "http://activitystrea.ms/schema/1.0/event"},
       { "extensions", extensions }
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
