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
#include <xapi_workitemactivity.h>
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
#define ConvertLowercase(A)   transform(A.begin(), A.end(), A.begin(), ::tolower)


////////////////////////////////////////////////////////////////////////////////
XAPI::WorkitemActivityEntry::WorkitemActivityEntry()
{

}
////////////////////////////////////////////////////////////////////////////////
XAPI::WorkitemActivityEntry::~WorkitemActivityEntry()
{

}
void
XAPI::WorkitemActivityEntry::ParseTimestamp(const std::string & strtime)
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
XAPI::WorkitemActivityEntry::Parse( const nlohmann::json & wi) 
{
  // bug, feature, epic, task etc.
  context = wi["fields"]["System.WorkItemType"];
  std::string date= wi["fields"]["System.ChangedDate"];
  ParseTimestamp(date);
  description = wi["fields"]["System.Reason"];

  username =  wi["fields"]["System.ChangedBy"]["displayName"];
  userid = wi["fields"]["System.ChangedBy"]["uniqueName"];

  size_t containsAssignment = (wi["fields"].count("System.AssignedTo") > 0) ;
  if ( containsAssignment )
  {
    // assigned to, if it exists
    related_username = wi["fields"]["System.AssignedTo"]["displayName"];
    related_userid = wi["fields"]["System.AssignedTo"]["uniqueName"];
  }
  
  event =  wi["url"]; // remove revisions/* from end
  
  #pragma omp critical
  {
    UserIDToEmail[userid] = anonymizer(userid);
  }
  #pragma omp critical
  {
    UserIDToUserName[userid] = anonymizer(username); 
  }
  component = wi["fields"]["System.State"];

}
////////////////////////////////////////////////////////////////////////////////
std::string
XAPI::WorkitemActivityEntry::ToXapiStatement()
{
  json statement;
  json actor;
  json verb;
  json object;
  json result;
  json extensions;

  ConvertLowercase( component );
  ConvertLowercase( context );
  ConvertLowercase( event );
  ConvertLowercase( description );
  
  // for some readability
  string & type = context;
  string & state = component;
  string & url = event;
  string & reason = description;
  string activity_type_id;
  string activity_type;
  
  string object_id = url.substr(0,url.rfind("revisions")-1);
  if ( type == "epic" )
  {
    activity_type = "Epic";
    activity_type_id = "http://id.tincanapi.com/activitytype/epic";
  }
  else if ( type == "task" )
  {
    activity_type = "Task";
    activity_type_id = "http://id.tincanapi.com/activitytype/task";
  }
  else if ( type == "product backlog item")
  {
    activity_type = "Product Backlog Item";
    activity_type_id = "http://id.tincanapi.com/activitytype/product-backlog-item";
  }
  else if ( type == "feature")
  {
    activity_type = "Feature";
    activity_type_id = "http://id.tincanapi.com/activitytype/feature";
  }
  else if ( type == "bug" )
  {
    activity_type = "Bug";
    activity_type_id = "http://id.tincanapi.com/activitytype/bug";    
  } 
  else if ( type == "test case" )
  {
    activity_type = "Test Case";
    activity_type_id = "http://id.tincanapi.com/activitytype/test-case";    
  } 
  else if ( type == "test suite" )
  {
    activity_type = "Test Suite";
    activity_type_id = "http://id.tincanapi.com/activitytype/test-suite";        
  } 
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
  if ( state == "new" && reason.rfind("new") )
  {
    verbname = "added";
    verb_xapi_id = "http://activitystrea.ms/schema/1.0/add";
  }
  else if ( state == "new" && reason.rfind("additional work") )
  {
    verbname = "updated";
    verb_xapi_id = "http://activitystrea.ms/schema/1.0/update";
  }
  else if ( state == "committed" )
  {
    // team agreed on backlog item or bug
    verbname ="agreed";
    verb_xapi_id = "http://activitystrea.ms/schema/1.0/agree";
  }
  else if ( state == "done" )
  {
    verbname = "completed";
    verb_xapi_id = "http://adlnet.gov/expapi/verbs/completed";
  }
  else if ( state == "in progress" )
  {
    // started work on task
    verbname = "started";
    verb_xapi_id = "http://activitystrea.ms/schema/1.0/start";   
  }
  else if ( state == "to do" )
  {
    // added tasks
    verbname = "added";
    verb_xapi_id = "http://activitystrea.ms/schema/1.0/add";   
  }
  else if ( state == "design" )
  {
    throw xapi_parsing_error("state design not supported");
  }
  
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
    { "id",  object_id},
    { "definition",
      {
       { "description",
         {
          { "en-GB", activity_type}
         }
       },
       { "type" , activity_type_id }
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
