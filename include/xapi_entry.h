#pragma once

#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <regex>
#include <json.hpp>
////////////////////////////////////////////////////////////////////////////////
using namespace std;
using namespace boost;
using json = nlohmann::json;
////////////////////////////////////////////////////////////////////////////////
extern const std::map<std::string, std::string> supportedVerbs;
extern const std::map<std::string, std::string> activityTypes;
extern std::map<std::string, std::string> TaskNameToTaskID;
extern std::map<std::string, std::string> UserNameToUserID;
////////////////////////////////////////////////////////////////////////////////
class Entry
{
protected:
  const string HOMEPAGE_URL_PREFIX = "https://moodle.karelia.fi/user/profile.php?id=";
public:
  struct tm when {0};
  json statement;

  // do we need these?
  string username;
  string related_username;
  string userid;
  string related_userid;
  string context; // generally the course
  string component;
  string event;
  string verb; // xapi-related
  string description; // moodle event id description
  string ip_address;

  virtual ~Entry() {}
  // parses time structure from string.
  virtual void ParseTimestamp(const string & strtime)
  {
    // this would be better, but strftime does not support single-digit days without some kind of padding.
    //stringstream ss(strtime);
    //ss >> std::get_time(&when, "%d.%m.%Y %H:%M:%S");

    std::vector<string> tokens;
    boost::split( tokens, strtime, boost::is_any_of(". :"));
    {
      stringstream ss;
      ss << tokens.at(0);
      if ( !(ss >> when.tm_mday) ) throw runtime_error("Conversion error, day");
    }
    {
      stringstream ss;
      ss << tokens.at(1);
      if ( !(ss >> when.tm_mon)  ) throw runtime_error("Conversion error, mon");
    }
    {
      stringstream ss;
      ss << tokens.at(2);
      if ( !(ss >> when.tm_year)  ) throw runtime_error("Conversion error, year");
    }

    {
      stringstream ss;
      ss << tokens.at(3);
      if ( !(ss >> when.tm_hour)  ) throw runtime_error("Conversion error, hour");
    }

    {
      stringstream ss;
      ss << tokens.at(4);
      if ( !(ss >> when.tm_sec)  ) throw runtime_error("Conversion error, second");
    }

  }
  /// Returns string as yyyy-mm-ddThh:mmZ
  string GetTimestamp() const
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
  virtual void Parse(const std::string & line ) 
  {
    vector< string > vec;
    // break line into comma-separated parts.
    Tokenizer tok(line);
    vec.assign(tok.begin(),tok.end());

    ParseTimestamp(vec.at(0));
    username = vec.at(1);
    related_username = vec.at(2);
    
    context = vec.at(3);
    // remove existing type-prefixing since it messes up name caching
    size_t pos = context.find_first_of(": ");
    if ( pos != std::string::npos)
    {
      context = context.substr(pos+2);
    }
    
    component = vec.at(4);
    event = vec.at(5);
    description = vec.at(6);
    // vec.at(7) = origin
    ip_address = vec.at(8);
  }
  
  virtual string ToXapiStatement()
  {
// Actor is starting point for our parsing. 
    json actor;
    
    regex re("[Tt]he user with id '([[:digit:]]+)'( has)*( had)* (manually )*([[:alnum:]]+)(.*)");
    smatch match;
    //////////
    // seek user info
    if ( regex_search(description, match, re) )
    {
      string		userid = match[1];
      stringstream	homepage;
      homepage << HOMEPAGE_URL_PREFIX << userid;
      // construct fake email for testing
      
      actor = {
	{"objectType", "Agent"},
	{"name", username},
	{"account",{}},

      };
      actor["account"] = {
	{"name", userid }, 
	{"homePage", homepage.str()}
      };
      // Create username to id mapping for grade parsing phase

    }
    else
    {
      throw xapi_parsing_error("could not extract xapi statement for actor: " + description);
    }
    UserNameToUserID[username] = userid;
    
    //////////
    // create verb jsonseek verb

    // Find verb from description
    json verb;
    string verbname = match[5];
    {
      auto it = supportedVerbs.find(verbname);
      if ( it == supportedVerbs.end())
	throw xapi_verb_not_supported_error(verbname);
      
      string verb_xapi_id = it->second;
      verb = {
	{"id", verb_xapi_id },
	{"display",
	 {
	   {"en-GB", verbname} // this will probably depend on subtype (see object)
	 }
	}
      };
    }
    
    /////////
    // Object
    // construct object (activity)
    
    json object;
    string details = match[6];
    smatch match_details;
    string tmp_id;
    string activityType;
    
    if ( verbname == "submitted"  )
    {
      regex re_details("the (submission|attempt) with id '([[:digit:]]+)' for the (assignment|quiz) with course module id '([[:digit:]]+)'.*");
      if ( regex_search(details, match_details, re_details) )
      {
	//verb["display"]["en-GB"] = attempted?
	activityType = match_details[3]; // assignment / quiz
	tmp_id       = match_details[4];
	
      } else throw xapi_parsing_error("submitted/attempted: " + details);
    }
    else
    {
      // course, page, collaborate, etc.
      regex re_details("the '*([[:alnum:]]+)'*( activity)* with (course module id|id) '([[:digit:]]+)'.*");
      if ( regex_search(details, match_details, re_details) )
      {
	activityType = match_details[1];
	tmp_id = match_details[4];
      } else throw xapi_parsing_error("Cannot make sense of: " + details);
    }
    
    auto it = activityTypes.find(activityType);
    if ( it == activityTypes.end()) throw xapi_activity_type_not_supported_error(activityType);

    string object_id = it->second + tmp_id;

    // assign mapping for later use in grading log parsing.
    //cerr << "appending task '" << context << "'\n";
    TaskNameToTaskID[context] = object_id;
    
    object = {
      { "objectType", "Activity"},
      { "id", object_id },
      { "definition",
	{
	  /*{ "description", { "en-GB", ""}},*/
	  { "type" , "http://id.tincanapi.com/activitytype/school-assignment"},
	  { "interactionType", "other" }
	}
      }
    };

    object["definition"]["name"] =  {
      {"en-GB", context}
    };   
    
    // construct full xapi statement
    statement["actor"] = actor;

    statement["verb"] = verb;
    statement["timestamp"] = GetTimestamp();
    statement["object"] = object;
    return statement.dump();
  }
};
