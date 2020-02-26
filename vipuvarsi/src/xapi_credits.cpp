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
#include <xapi_credits.h>
#include <algorithm>
#include <xapi_errors.h>
#include <xapi_anonymizer.h>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <locale>
////////////////////////////////////////////////////////////////////////////////
using namespace boost;
using namespace std;
using json = nlohmann::json;
extern XAPI::Anonymizer anonymizer;
////////////////////////////////////////////////////////////////////////////////
XAPI::CreditsEntry::CreditsEntry()
{

}
////////////////////////////////////////////////////////////////////////////////
XAPI::CreditsEntry::~CreditsEntry()
{

}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::CreditsEntry::ParseTimestamp(const string & strtime) 
{
  std::vector<string> tokens;
  boost::split( tokens, strtime, boost::is_any_of("-"));
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
      smsg << "Conversion error in grade, month value out of range 1-12.";
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
XAPI::CreditsEntry::Parse( const std::vector<std::string> & vec ) 
{
  // break line into comma-separated parts.
  if ( vec.at(7) != "NULL" && !vec.at(7).empty() )
    ParseTimestamp(vec.at(7));
  else
    ParseTimestamp(vec.at(8));
   
  userid = anonymizer(vec.at(0)); // gradee student id
  event = vec.at(5); // grade
  course_name = vec.at(3);
  course_id = vec.at(1);
}
////////////////////////////////////////////////////////////////////////////////
std::string
XAPI::CreditsEntry::ToXapiStatement()
{
  json statement;
  json actor;
  json verb;
  json object;
  json result;
  
  /*if ( UserNameToUserID.find(username) == UserNameToUserID.end())
  {
    throw xapi_cached_user_not_found_error(username);
    }*/
  
  //string userid = UserNameToUserID[username];
  // define user receiving a score
  actor = CreateActorJson(userid);
  // construct scoring verb
  string verbname = "received";

  // http://activitystrea.ms/schema/1.0/receive
  // passed | failed | received
  // http://adlnet.gov/expapi/verbs/failed
  // http://adlnet.gov/expapi/verbs/passed
  //auto it = supportedVerbs.find(verbname);
  string verb_xapi_id = "http://activitystrea.ms/schema/1.0/receive";
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
    { "id", event },
    { "definition",
      {
	/*{ "description", { "en-GB", ""}},*/
	{ "type" , "http://www.tincanapi.co.uk/activitytypes/grade_classification"},
	{ "interactionType", "other" }
      }
    }
  };
    
  object["definition"]["name"] =  {
    {"en-GB", context}
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
nlohmann::json
XAPI::CreditsEntry::CreateActorJson( const std::string & userid )
{

  json user;
  string email;
  string username;

  email = userid + std::string("@karelia.fi");
  username = userid;
  // in case some thread is modifying a map - it should not happen anymore, but still.

  // build json 
  user = {
            { "objectType", "Agent"},
            { "name", username },
            { "mbox", email }
  };
  
  return user;
}
