#include <xapi_activityentry.h>
#include <xapi_errors.h>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <string>
#include <vector>
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
XAPI::ActivityEntry::Parse(const std::vector<std::string> & vec ) 
{
	
  ParseTimestamp(vec.at(0));
  username = vec.at(1);
  related_username = vec.at(2);
      
  context = vec.at(3);
  // remove existing type-prefixing since it messes up name caching
  size_t pos = context.find_first_of(": ");
  if ( pos != std::string::npos)
  {
    context_module_locale_specific = context.substr(0,pos);
    context = context.substr(pos+2);
  }
      
  component = vec.at(4);
  event = vec.at(5);
  description = vec.at(6);
  // vec.at(7) = origin
  ip_address = vec.at(8);
}
void
XAPI::ActivityEntry::Parse(const std::string & line ) 
{
  vector< string > vec;
      
  // break line into comma-separated parts.
  Tokenizer tok(line);
  vec.assign(tok.begin(),tok.end());
      
  Parse(vec);
}
    
std::string
XAPI::ActivityEntry::ToXapiStatement()
{
  // Actor is starting point for our parsing. 
  json actor;

  // construct context for activity (course, and additionally others.)
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

 
  
  smatch match;
  string verbname;
  
  //////////
  // seek user info
  if ( regex_search(description, match,
		    regex("The user with id '[[:digit:]]+' has had their attempt with id '[[:digit:]]+' ([[:alnum:]]+) by the user with id '[[:digit:]]+' for the quiz with course module id '[[:digit:]]+'\\.")) )
  {
    // this will be ignored...
    verbname = match[1]; 
  }
  else if ( regex_search(description, match,
		    regex("[Tt]he user with id '(-*[[:alnum:]]+)'( has)*( had)*( manually)* ([[:alnum:]]+) (.*)")) )
  {
    // actual user completing a task
    userid = match[1];
    stringstream	homepage;
    homepage << HOMEPAGE_URL_PREFIX << userid;
    verbname = match[5];
    //cerr << "matches:\n";
    /*int c=0;
      for(auto & m : match )
      {
      cerr << c++ << ":" << m << "\n";
      }*/

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

      
  }
  else if ( regex_search(description, match, regex("User ([[:digit:]]+) updated the question ([[:digit:]]+)\\.")))
  {
    // we cannot create proper log entry using this kind of statement moodle logs give us.
    throw xapi_activity_ignored_error("updated:question (question id without proper course module id)");
  }
  else
  {
    throw xapi_parsing_error("could not extract xapi statement for actor: " + description);
  }

  if ( username.empty() ) throw xapi_parsing_error("no username found for: " + description);
  //cerr << "setting username-id pair: " << username << " -> " << userid << "\n";
  UserNameToUserID[username] = userid;
  UserIDToUserName[userid] = username;



  //////////
  // create verb jsonseek verb
  
  // Find verb from description
  json verb;

  
  {
    auto it = supportedVerbs.find(verbname);
    if ( it == supportedVerbs.end())
      throw xapi_verb_not_supported_error(verbname+"' in '"+description);

    string verb_xapi_id = it->second;
    if ( verb_xapi_id.length() == 0 )
      throw xapi_verb_not_supported_error(verbname+"(ignored)");

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
  string sectionNumber; // required only for course sections
  string chapterNumber; // required only for book chapters
	string postNumber;    // required for discussion posts
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
  else if ( verbname == "updated" )
  {
    /* special case when assignment status is updated by system */
    if ( regex_search(details, match_details,
		      regex("the completion state for the course module with id '([[:digit:]]+)' for the user with id '([[:digit:]]+)'\\.") ))
    {
      // This can be almost anything for the completion state. It happens to be locale-specific, so it needs additional parsing.
      // Currently we support only finnish.
      auto it = contextModuleLocaleToActivityType.find(context_module_locale_specific);
      if ( it != contextModuleLocaleToActivityType.end())
      {
	activityType = it->second;
      }
      else
      {
	// we never should get here, but better safe than sorry.
	throw xapi_activity_type_not_supported_error("(Localized: "+context_module_locale_specific+")");
      }
      
      // We also assume that you update completion status only to "completed" one.
	
    }
    else if ( regex_search(details, match_details,
			   regex("(section number) '([[:digit:]]+)' for the course with id '([[:digit:]]+)'")) )
    {
      activityType = "section";
      tmp_id = match_details[2];

      
    }
    else if ( regex_search(details, match_details,
			   regex("the chapter with id '([[:digit:]]+)' for the book with course module id '([[:digit:]]+)'")) )
    {
      activityType = "chapter";
      tmp_id = match_details[2];
      chapterNumber = match_details[1];
     
    }
    else if ( regex_search(details, match_details,
			   regex("the '([[:alnum:]]+)' activity with course module id '([[:digit:]]+)'.*")) )
    {
      activityType = match_details[1];
      tmp_id = match_details[2];
      
    }
    else if ( regex_search(details, match_details,
			   regex("the course with id '([[:digit:]]+)'.*")) )
    {
      activityType = "course";
      tmp_id = match_details[1];
      
    }
    else if ( regex_search(details, match_details,
			   regex("the post with id '([[:digit:]]+)' in the discussion with id '([[:digit:]]+)'.*")) )
    {
      activityType = "post";
      tmp_id = match_details[2];
			postNumber = match_details[1];
      
    }
    else if ( regex_search(details, match_details,
			   regex("the grade with id '([[:digit:]]+)' for the user with id '([[:digit:]]+)' for the grade item with id '([[:digit:]]+)'") ))
    {

      activityType = "grade";
      tmp_id = match_details[1];
      throw xapi_activity_ignored_error("updated:grade");
    }
    else if ( regex_search(details, match_details,
			   regex("the (event|instance of enrolment method) '(.*)' with id '([[:digit:]]+)'") ))
    {

      activityType = "event";
      tmp_id = match_details[1];
      throw xapi_activity_ignored_error("updated:event/instance");
    }
  }
  else if ( verbname == "started" ) // quiz attempts are started
  {
		regex re_details("the ([[:alnum:]]+) with id '([[:digit:]]+)' for the quiz with course module id '([[:digit:]]+)'.*");
    if ( regex_search(details, match_details, re_details) )
    {
      activityType = "quiz";
      tmp_id = match_details[3];
    }
    else throw xapi_parsing_error("Cannot make sense of: " + details);
  }
	else if ( verbname == "created" ) 
  {
		if ( regex_search(details, match_details,
											regex("(the|a)* (backup|event|instance of enrolment method).*")))
		{
			throw xapi_activity_ignored_error("created:backup/event/instance of enrolment");
    }
		else if ( regex_search(details, match_details,
													 regex("the post with id '([[:digit:]]+)' in the discussion with id '([[:digit:]]+)' in the forum with course module id '([[:digit:]]+)\\.")))
    {
      activityType = "post";
			tmp_id = match_details[2];
			postNumber = match_details[1];
    }
		else if ( regex_search(details, match_details,
													 regex("the discussion with id '([[:digit:]]+)' in the forum .*")))
    {
      activityType = "discussion";
			tmp_id = match_details[1];
    }
		else if ( regex_search(details, match_details,
													 regex("section number '([[:digit:]]+)' for the course with id '([[:digit:]]+)'")))
    {
      activityType = "section";
			tmp_id = match_details[2];
			sectionNumber = match_details[1];
    }
		else if ( regex_search(details, match_details,
													 regex("the '([[:alnum:]]+)' activity with course module id '([[:digit:]]+)'\\.")))
    {
      activityType = match_details[1];
			tmp_id = match_details[2];
    }
    else throw xapi_parsing_error("Cannot make sense of: " + details);
  }
	else if ( verbname == "deleted" )
	{
		if (regex_search(details, match_details,
										 regex("the grade with id .*"))){
			throw xapi_activity_ignored_error("delete:grade");
		}
		else throw xapi_parsing_error("Cannot make sense of: " + details);
	}
  else
  {
    if ( regex_search(details, match_details,
		      regex("the attempt with id '([[:digit:]]+)' belonging to the user with id '([[:digit:]]+)' for the quiz with course module id '([[:digit:]]+)'.*")) )
    {
      activityType = "attempt";
      tmp_id = match_details[1];
      string quiz_id = match_details[3];
      string target_userid = match_details[2];
      stringstream target_user_homepage;
      target_user_homepage << HOMEPAGE_URL_PREFIX << target_userid;

      // add quiz to statement related context
      auto quizType = moodleXapiActivity.find("quiz");
      json quiz = {
	{ "objectType", "Activity"},
	{ "id", quiz_id },
	{ "definition", {
	    { "description",
	      {
		{ "en-GB", context}
	      }
	    },
	    { "type", quizType->second }
	  }
	}
      };
      
      // add user to statement related context
      json user = {
	{ "objectType", "Agent"},
	{ "name", UserIDToUserName[target_userid]}, // todo fix username here as well
	{ "account",{}},
      };
      user["account"] = {
	{"name", target_userid }, 
	{"homePage", target_user_homepage.str()}
      };

      // add to related context
      activityContext["contextActivities"]["grouping"].push_back(quiz);
      activityContext["contextActivities"]["grouping"].push_back(user);
    }
    else if ( regex_search(details, match_details,
			   regex("the discussion with id '([[:digit:]]+)' in the forum with course module id '([[:digit:]]+)'\\." )))
    {
      activityType = "discussion";
      tmp_id = match_details[1];
      /// \TODO add forum as related context?
    }
    else if ( regex_search(details, match_details,
			   regex("the profile for the user with id '([[:digit:]]+)' in the course with id '([[:digit:]]+)'\\." )))
    {
      activityType = "user";
      tmp_id = match_details[1];
    }
    else if ( regex_search(details, match_details,
			   regex("the (user|grader|history|overview)( log| statistics)* report .*")))
    {
      throw xapi_activity_ignored_error("viewed:(log/statistics/history/overview/grade)report");
    }
    else if ( regex_search(details, match_details,
			   regex("the list of available badges .*")))
    {
      throw xapi_activity_ignored_error("viewed:available badge list");
    }
    else if ( regex_search(details, match_details,
			   regex("the user with id '([[:digit:]]+)' using enrolment method '(self|manual)' in the course with id '([[:digit:]]+)'\\.")))
    {
      //enrolled the user with id '356' using the enrolment method 'manual' in the course with id '2070'.
      //enrolled the user with id '2199' using the enrolment method 'self' in the course with id '2070'.
      activityType = "course";
      tmp_id = match_details[1];
      stringstream related_user_homepage;
      related_user_homepage << HOMEPAGE_URL_PREFIX << tmp_id;
      if ( tmp_id != userid )
      {
	// actor was the (un)enroller, actual user that was (un)enrolled needs to be set as actor.
	json related_user = actor;
	actor = {
	  { "objectType", "Agent"},
	  { "name", UserIDToUserName[tmp_id]}, // todo fix username here as well
	  { "account",{}},
	};
	actor["account"] = {
	  {"name", userid }, 
	  {"homePage", related_user_homepage.str()}
	};
	
	// add (un)enroller to related context
	activityContext["contextActivities"]["grouping"].push_back(related_user);
	
      }
    }
    else if ( regex_search(details, match_details,     // course, page, collaborate, etc.    
			   regex("the '*([[:alnum:]]+)'*( activity)* with (course module id|id) '([[:digit:]]+)'.*")) )
    {
      activityType = match_details[1];
      tmp_id = match_details[4];
    }
    else throw xapi_parsing_error("Cannot make sense of: " + details);
    
  }
  /* construct object id */
  auto it = activityTypes.find(activityType);
  if ( it == activityTypes.end()) throw xapi_activity_type_not_supported_error(verbname+":"+activityType+"' with statement '"+details);
  string object_id = it->second + tmp_id;
  
  // handle special cases
  if ( it->first == "section" )
  {
    stringstream ss;
    ss << object_id << "#section-" << sectionNumber;
    object_id = ss.str();
  }
  else if ( it->first == "chapter" )
  {
    stringstream ss;
    ss << object_id << "&chapterid=" << chapterNumber;
    object_id = ss.str();
  }
  /* find proper Xapi activity type */ 
  string moodleActivity = it->first;
  auto activityIt = moodleXapiActivity.find(moodleActivity);
  if ( activityIt == moodleXapiActivity.end()) throw xapi_activity_type_not_supported_error(moodleActivity);
  
  string xapiActivity = activityIt->second;
  // assign mapping for later use in grading log parsing.
  //cerr << "appending task '" << context << "' -> " << object_id << "\n";
  TaskNameToTaskID[context] = object_id;
    
  object = {
    { "objectType", "Activity"},
    { "id", object_id },
    { "definition",
      {
	/*{ "description", { "en-GB", ""}},*/
	{ "type" , xapiActivity },
	{ "interactionType", "other" }
      }
    }
  };
  /* http://id.tincanapi.com/activitytype/document */
  /* http://adlnet.gov/expapi/activities/media */
  
  object["definition"]["name"] =  {
    {"en-GB", context}
  };   

  
  // construct full xapi statement
  statement["actor"] = actor;

  statement["verb"] = verb;
  statement["timestamp"] = GetTimestamp();
  statement["object"] = object;
  statement["context"] = activityContext;


  
  if ( GetTimestamp().length() > 17 )
    throw xapi_parsing_error("Timestamp length is off: " + GetTimestamp());
  return statement.dump();
}
