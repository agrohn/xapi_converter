#include <xapi_activityentry.h>
#include <xapi_errors.h>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
typedef boost::tokenizer< boost::escaped_list_separator<char> > Tokenizer;
////////////////////////////////////////////////////////////////////////////////
using namespace boost;
using json = nlohmann::json;
using namespace std;
////////////////////////////////////////////////////////////////////////////////
void
XAPI::ActivityEntry::ParseTimestamp(const string & strtime)
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
////////////////////////////////////////////////////////////////////////////////
void
XAPI::ActivityEntry::Parse(const std::string & line ) 
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
    
std::string
XAPI::ActivityEntry::ToXapiStatement()
{
  // Actor is starting point for our parsing. 
  json actor;
    
  regex re("[Tt]he user with id '([[:digit:]]+)'( has)*( had)* (manually )*([[:alnum:]]+)(.*)");
  smatch match;
  //////////
  // seek user info
  if ( regex_search(description, match, re) )
  {
    userid = match[1];
    //cerr << "matches:\n";
    /*int c=0;
      for(auto & m : match )
      {
      cerr << c++ << ":" << m << "\n";
      }*/
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

  if ( username.empty() ) throw xapi_parsing_error("no username found for: " + description);
  //cerr << "setting username-id pair: " << username << " -> " << userid << "\n";
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
  //cerr << "appending task '" << context << "' -> " << object_id << "\n";
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
  if ( GetTimestamp().length() > 17 )
    throw xapi_parsing_error("Timestamp length is off: " + GetTimestamp());
  return statement.dump();
}