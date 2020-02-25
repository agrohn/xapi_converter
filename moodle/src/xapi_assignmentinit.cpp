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
#include <xapi_assignmentinit.h>
#include <algorithm>
#include <xapi_errors.h>
#include <xapi_anonymizer.h>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
////////////////////////////////////////////////////////////////////////////////
using namespace boost;
using namespace std;
using json = nlohmann::json;
////////////////////////////////////////////////////////////////////////////////
void
XAPI::AssignmentInitEntry::ParseTimestamp(const std::string & strtime)
{
  std::vector<string> tokens;
  boost::split( tokens, strtime, boost::is_any_of("-"));
  {
    stringstream ss;
    ss << tokens.at(0);
    if ( !(ss >> when.tm_year)  ) throw runtime_error("Conversion error, year");
  }
  {
    stringstream ss;
    ss << tokens.at(1);
    if ( !(ss >> when.tm_mon)  ) throw runtime_error("Conversion error, mon");
    // sanity check for month value
    if ( when.tm_mon < 1 || when.tm_mon > 12 )
    {
      stringstream smsg;
      smsg << "Conversion error, month value out of range 1-12.";
      smsg << "Got: '" << tokens.at(1) << "', value is : " << when.tm_mon;
      throw runtime_error(smsg.str());
    }
  }
  {
    stringstream ss;
    ss << tokens.at(2);
    if ( !(ss >> when.tm_mday) ) throw runtime_error("Conversion error, day");
  }
}
////////////////////////////////////////////////////////////////////////////////
std::string
XAPI::AssignmentInitEntry::ToXapiStatement()
{
  json statement;
  
  json actor;
  json verb;
  json object;

  
  // fake user id
  string userid = "0";

  // define user receiving a score
  actor = CreateActorJson(userid);

  // construct verb
  string verbname = "authorized";
  auto it = supportedVerbs.find(verbname);
  string verb_xapi_id = it->second;
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
    { "id", id },
    { "definition",
      {
	/*{ "description", { "en-GB", ""}},*/
	{ "type" , "http://id.tincanapi.com/activitytype/school-assignment"},
	{ "interactionType", "other" }
      }
    }
  };
    
  object["definition"]["name"] =  {
    {"en-GB", name}
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
