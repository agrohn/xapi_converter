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
#include <xapi_memo.h>
#include <algorithm>
#include <xapi_errors.h>
#include <xapi_anonymizer.h>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <locale>
#include <string>
////////////////////////////////////////////////////////////////////////////////
using namespace boost;
using namespace std;
using json = nlohmann::json;
extern XAPI::Anonymizer anonymizer;
extern std::map<std::string, std::string> UserNameToUserID;
extern std::map<std::string, std::string> UserIDToUserName;
extern std::map<std::string, std::string> UserIDToEmail;
////////////////////////////////////////////////////////////////////////////////
XAPI::MemoEntry::MemoEntry()
{

}
////////////////////////////////////////////////////////////////////////////////
XAPI::MemoEntry::~MemoEntry()
{

}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::MemoEntry::ParseTimestamp(const string & strtime) 
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
      smsg << "Conversion error in memo, month value out of range 1-12.";
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
std::string
XAPI::MemoEntry::ToXapiStatement()
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
  // convert component string to lowercase 
  transform(component.begin(), component.end(), component.begin(), ::tolower);
  //event = tolower(event);
  if ( context == "meeting" )
  {
    if( component == "x" )
    {
      verbname = "attended";
      verb_xapi_id = "http://activitystrea.ms/schema/1.0/attend";    
    }
    else if ( component == "-" )
    {

      verbname = "skipped";
      extensions["http://id.tincanapi.com/extension/severity"] = "known";
      verb_xapi_id = "http://id.tincanapi.com/verb/skipped";

    }
    else if ( component == "?" )
    {
      verbname = "skipped";
      extensions["http://id.tincanapi.com/extension/severity"] = "unknown";
      verb_xapi_id = "http://id.tincanapi.com/verb/skipped";
      
    }
    /*    extensions[ "http://id.tincanapi.com/extension/tags"] = {
     
	  }*/
    
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


  }
  else if ( context == "role" )
  {
    verbname = "received";
    verb_xapi_id = "http://activitystrea.ms/schema/1.0/receive";
    object = {
      { "objectType", "Activity"},
      { "id", "https://karelia.fi/role" },
      { "definition",
	{
	  { "description",
	    {
	      { "en-GB", context}
	    }
	  },
	  { "type" , "http://id.tincanapi.com/activitytype/security-role"},
	  { "extensions", extensions }
	}
      }
    };

  }
  else if ( context == "devblog" )
  {
    verbname = "updated";
    verb_xapi_id = "http://activitystrea.ms/schema/1.0/update";
    object = {
      { "objectType", "Activity"},
      { "id",  "https://karelia.fi/devblog"},
      { "definition",
	{
	  { "description",
	    {
	      { "en-GB", "Lecture"}
	    }
	  },
	  { "type" , "http://id.tincanapi.com/activitytype/blog"},
	  { "extensions", extensions }
	}
      }
  };
    
  }
  
  
  verb = {
    {"id", verb_xapi_id },
    {"display",
     {
       {"en-GB", verbname} // this will probably depend on subtype (see object)
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
