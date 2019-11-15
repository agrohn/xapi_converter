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
  boost::split( tokens, strtime, boost::is_any_of(". :/,"));
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
    // for english locale representation
    if ( tokens.at(2).size() == 2 )
      ss << "20";
    ss << tokens.at(2);
    if ( !(ss >> when.tm_year)  ) throw runtime_error("Conversion error, year");
  }
  int currentToken = 3;
  {
    // this works around with english locale (dd/mm/yy, hh:mm) since we define ',' as a delimeter.
    stringstream ss;
    if ( tokens.at(currentToken).empty())
      currentToken++;
    ss << tokens.at(currentToken);
    if ( !(ss >> when.tm_hour)  ) throw runtime_error("Conversion error, hour");
  }
      
  {
    currentToken++;
    stringstream ss;
    ss << tokens.at(currentToken);
    if ( !(ss >> when.tm_sec)  ) throw runtime_error("Conversion error, second");
  }
      
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::ActivityEntry::Parse(const std::vector<std::string> & vec ) 
{
  
  ParseTimestamp(vec.at(0));
#pragma omp critical
  {
    username =  anonymizer(vec.at(1));
    related_username =   anonymizer(vec.at(2));
  }
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
void
XAPI::ActivityEntry::UpdateUserData()
{
  smatch match;
  if ( username.empty() ) throw xapi_parsing_error("no username found for: " + description);
  if ( regex_search(description, match, regex("[Tt]he user with id '([[:digit:]]+)' .*")))
  {
#pragma omp critical
    {
      string userid = anonymizer(match[1]);
      UserNameToUserID[anonymizer(username)] = userid;
      UserIDToUserName[userid] = anonymizer(username);
    }
  }
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
  json extensions;

  activityContext =  {
    { "contextActivities", grouping }
  };

  smatch match;
  string verbname;


  string activity_id;
  string activityType;
  string sectionNumber; // required only for course sections
  string chapterNumber; // required only for book chapters
  string postNumber;    // required for discussion posts
  string discussionNumber; // required for discussion posts
  string attemptNumber; // for submissions and quiz attempts (though they are separate things)
  string userWhoIsProcessed; // target user


  

  if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' (has )?([[:alnum:]]+) (a |the )?(backup|grades|old course|course participation report|grading form|grading table|live log|singleview report|tour|question category|user report).*")))
  {
    string tmp = match[3];
    throw xapi_activity_ignored_error(tmp+":(backup/grades/old course/submission status page)");
  }
  /*
    "The user with id '' created the '' activity with course module id ''."
  */
    
  if ( regex_search(description, match,
                    regex("[Tt]he user with id '([[:digit:]]+)' ([[:alnum:]]+) the '(quiz|page|collaborate|resource|url|forum|hsuforum|lti|hvp|book|label|questionnaire|assign|chat|feedback)' activity with course module id '([[:digit:]]+)'\\.")) )
  {

    #pragma omp critical
    userid = anonymizer(match[1]);

    verbname = match[2];
    activityType = match[3];
    if ( activityType == "assign" ) activityType = "assignment";
    activity_id = match[4];
    
  }
  /*
    "The user with id '' has sent a message in the chat with course module id ''."
  */
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' has sent a message in the chat with course module id[[:space:]]*'([[:digit:]]+)'\\.")) )
  {

    #pragma omp critical
    userid = anonymizer(match[1]);

    verbname = "commented";
    activityType = "chat";
    activity_id = match[2];
    
  }
  /*
    "The user with id '' created section number '' for the course with id ''"
    "The user with id '' updated section number '' for the course with id ''"
    "The user with id '' viewed the section number '' of the course with id ''."
    "The user with id '' deleted section number '' (section name 'Siirretyt materiaalit ') for the course with id ''"
  */
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' ([[:alnum:]]+) (the )?section number '(-?[[:digit:]]+)' (\\(section name '.+'\\) )?(for|of) the course with id '[[:digit:]]+'.*")))
  {
    #pragma omp critical
    userid = anonymizer(match[1]);

    verbname = match[2];
    activityType = "section";
    sectionNumber = match[4];
    activity_id = match[7];
  }
  // 'The user with id '' viewed the profile for the user with id '' in the course with id ''.
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' viewed the profile for the user with id '([[:digit:]]+)' in the course with id '([[:digit:]]+)'\\.")) )
  {
    #pragma omp critical
    userid = anonymizer(match[1]);
    verbname = "viewed";
    activityType = "user";
#pragma omp critical
    activity_id = anonymizer(match[2]);
  }
  /*
    "The user with id '' has searched the course with id '' for forum posts containing """"."
    "The user with id '' updated the course with id ''.
    "The user with id '' viewed the course information for the course with id ''."
    "The user with id '' viewed the course with id ''."
  */
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)'( has)? ([[:alnum:]]+) the course (information for the course )?with id '([[:digit:]]+)'.*")) )
  {
    #pragma omp critical
    userid = anonymizer(match[1]);
    verbname = match[3];
    activityType = "course";
    activity_id = match[5];
  }
  /*
    "The user with id '' added the comment with id '' to the submission with id '' for the assignment with course module id ''."
    "The user with id '' deleted the comment with id '' from the submission with id '' for the assignment with course module id ''."
  */
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' ([[:alnum:]]+) the comment with id '([[:digit:]]+)' (to|from) the submission with id '([[:digit:]]+)' for the assignment with course module id '([[:digit:]]+)'\\.")) )
  {
    #pragma omp critical
    userid = anonymizer(match[1]);
    verbname = "commented"; //match[2];
    activityType = "submission";
    activity_id = match[6];
    attemptNumber = match[5];
#pragma omp critical
    userWhoIsProcessed = anonymizer(match[1]);
  }

    /* 
     The user with id '' has graded the submission '' for the user with id '' for the assignment with course module id ''. 
     
  */
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' has graded the submission '([[:digit:]]+)' for "
                               "the user with id '([[:digit:]]+)' for the assignment with course module id '([[:digit:]]+)'\\.")))
  {
    #pragma omp critical
    userid = anonymizer(match[1]);
    activityType = "submission";
    verbname = "evaluated";
    activity_id = match[4];
    attemptNumber = match[2];
    userWhoIsProcessed = match[3];
    extensions["http://id.tincanapi.com/extension/attempt-id"] = attemptNumber;
  }
  /* "The user with id '' submitted response for 'feedback' activity with course module id ''."*/
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' submitted response for 'feedback' activity with course module id '([[:digit:]]+)'\\.")))
  {
    #pragma omp critical
    userid = anonymizer(match[1]);
    activityType = "feedback";
    // although this does not match the actual verb, feedback is represented
    // as a 'survey' which is 'completed'.
    verbname = "completed"; 
    activity_id = match[2];
  }


  /*
    "The user with id '' has submitted the submission with id '' for the assignment with course module id ''."
    "The user with id '' has uploaded a file to the submission with id '' in the assignment activity with course module id ''."     
  */
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)'( has)? ([[:alnum:]]+) (the submission|a file to the submission) with id '([[:digit:]]+)' (for|in) the assignment( activity)? with course module id '([[:digit:]]+)'\\.")) )
  {
    // assignment submission
    #pragma omp critical
    userid = anonymizer(match[1]);
    
    verbname = match[3];

    activityType = "submission";
    attemptNumber = match[5];
    activity_id = match[8];

   #pragma omp critical
    userWhoIsProcessed = anonymizer(match[1]);
    
    auto it = activityTypes.find("submission");
    extensions["http://id.tincanapi.com/extension/attempt-id"] = it->second + attemptNumber;
    
  }
  /*
    "The user with id '' viewed their submission for the assignment with course module id ''."
    "The user with id '' created a file submission and uploaded '' file/s in the assignment with course module id ''."
    "The user with id '' updated a file submission and uploaded '' file/s in the assignment with course module id ''."
    "The user with id '' has viewed the submission status page for the assignment with course module id ''."
  
    "The user with id '' created an online text submission with '' words in the assignment with course module id ''."
    "The user with id '' updated an online text submission with '' words in the assignment with course module id ''."
    "The user with id '' has saved an online text submission with id '' in the assignment activity with course module id ''."

  */
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' (has )*([[:alnum:]]+) (their|a file|the|an online text) submission (.*) the assignment (activity )?with course module id '([[:digit:]]+)'\\.")) )
  {

#pragma omp critical
    userid = anonymizer(match[1]);

    verbname = match[3];
    if ( verbname != "viewed" )
      verbname = "answered";
    activityType = "assignment";
    activity_id = match[7];
  
  }
  /*
    "The user with id '' has had their attempt with id '' marked as abandoned for the quiz with course module id ''."
    "The user with id '' has started the attempt with id '' for the quiz with course module id ''."
    "The user with id '' has submitted the attempt with id '' for the quiz with course module id ''."
  */
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' has ([[:alnum:]]+) the(ir)? attempt with id '([[:digit:]]+)' (marked as abandoned )?for the quiz with course module id '([[:digit:]]+)'\\.")) )
  {
    #pragma omp critical
    userid = anonymizer(match[1]);
    verbname = match[2];
    if ( verbname == "had" ) verbname = "abandoned";
     
    activityType = "attempt";
    attemptNumber = match[4]; 
    activity_id = match[6]; // quiz
  }
  /*
    
    "The user with id '' has had their attempt with id '' previewed by the user with id '' for the quiz with course module id ''."
    "The user with id '' has had their attempt with id '' reviewed by the user with id '' for the quiz with course module id ''."
    "The user with id '' has viewed the attempt with id '' belonging to the user with id '' for the quiz with course module id ''."
    "The user with id '' has viewed the summary for the attempt with id '' belonging to the user with id '' for the quiz with course module id ''."
  */
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' has ([[:alnum:]]+) the(ir)? (summary for the )?attempt with id '([[:digit:]]+)' ([[:alnum:]]+) (by|to) the user with id '([[:digit:]]+)' for the quiz with course module id '([[:digit:]]+)'\\.")) )
  {
    // preview, review, view attempt (summary)
    #pragma omp critical
    userid = anonymizer(match[1]);
    verbname = match[2];
    if ( verbname == "had" )
    {
      verbname = match[6];
      #pragma omp critical
      userid = anonymizer(match[8]);
      #pragma omp critical
      userWhoIsProcessed = anonymizer(match[1]);
    }
    else
    {
      #pragma omp critical
      userWhoIsProcessed = anonymizer(match[8]);
    }
     
    activityType = "quiz";
    #pragma omp critical
    activity_id = anonymizer(match[9]);
    attemptNumber = match[5]; // quiz

     
    stringstream processed_user_homepage;
    processed_user_homepage << activityTypes["homepage"] << userWhoIsProcessed;
     
    string tmp_username;
    #pragma omp critical
    {
      tmp_username = UserIDToUserName[userWhoIsProcessed];
    }
    // add user to statement related context
    json user = {
      { "objectType", "Agent"},
      { "name", tmp_username}, // todo fix username here as well
      { "account",{}},
    };
    user["account"] = {
      {"name", userWhoIsProcessed }, 
      {"homePage", processed_user_homepage.str()}
    };

    auto it = activityTypes.find("attempt");
     
    // add to related context
    extensions["http://id.tincanapi.com/extension/target"] = user;
    extensions["http://id.tincanapi.com/extension/attempt-id"] = it->second + attemptNumber;
  }
  /*
    'The user with id '' manually graded the question with id '' for the attempt with id '' for the quiz with course module id ''
  */
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' manually graded the question with id '([[:digit:]]+)' for the attempt with id '([[:digit:]]+)' for the quiz with course module id '([[:digit:]]+)'\\.")) )
  {
    #pragma omp critical
    userid = anonymizer(match[1]);
    verbname = "evaluated";
    activityType = "submission";
    attemptNumber = match[3];
    activity_id = match[4];
    string questionId = match[2];
    auto it = activityTypes.find("quiz");
    extensions["http://id.tincanapi.com/extension/attempt-id"] = it->second + attemptNumber;
    it = activityTypes.find("question");
    extensions["http://id.tincanapi.com/extension/target"] = it->second + questionId;
  }
  /* 
     "The user with id '' has printed the book with course module id ''."
  */
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' has ([[:alnum:]]+) the book with course module id '([[:digit:]]+)'\\.")) )
  {
    #pragma omp critical
    userid = anonymizer(match[1]);
    verbname = match[2];
    activityType = "book";
    activity_id = match[3]; 
  }
  
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' viewed the recording with id '([[:digit:]]+)' for the Collab with course module id '([[:digit:]]+)'\\.")) )
  {
    #pragma omp critical
    userid = anonymizer(match[1]);
    verbname = "viewed";
    activityType = "collaborate";
    activity_id = match[3]; 
  }
  /*
    "The user with id '' viewed the chapter with id '' for the book with course module id ''."
  */
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' viewed the chapter with id '([[:digit:]]+)' for the book with course module id '([[:digit:]]+)'\\.")) )
  {
    //chapter
    #pragma omp critical
    userid = anonymizer(match[1]);
    verbname = "viewed";
    activityType = "book";
    chapterNumber =  match[2];
    activity_id = match[3]; 
  }

  /*
    "The user with id '' has created the discussion with id '' in the forum with course module id ''."
    "The user with id '' has created the discussion with id '' in the forum with the course module id ''."
    "The user with id '' has viewed the discussion with id '' in the forum with course module id ''."
    "The user with id '' has viewed the discussion with id '' in the forum with the course module id ''."

  */
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' has ([[:alnum:]]+) the discussion with id '([[:digit:]]+)' in the forum with (the )?course module id '([[:digit:]]+)'\\.")) )
  {
    //discussion
    #pragma omp critical
    userid = anonymizer(match[1]);
    verbname = match[2];
    activityType = "discussion";
    postNumber = match[3];
    activity_id = match[5]; 
  }
  /*
    "The user with id '' enrolled the user with id '' using the enrolment method 'manual' in the course with id ''."
    "The user with id '' enrolled the user with id '' using the enrolment method 'self' in the course with id ''."
    "The user with id '' unenrolled the user with id '' using the enrolment method 'self' from the course with id ''."
  */
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' ([[:alnum:]]+) the user with id '([[:digit:]]+)' using the enrolment method '(self|manual)' (in|from) the course with id '([[:digit:]]+)'\\.")) )
  {
    // user unenrolled, enrolled
    #pragma omp critical
    userid = anonymizer(match[1]);
    verbname = match[2];
    userWhoIsProcessed = match[3];
       
    activityType = "course";
    string method = match[4]; // self|manual
    activity_id = match[6]; 


    if ( method != "self" ) // in case user was unenrolled by some other user
    {
      stringstream target_user_homepage;
      #pragma omp critical
      {
	target_user_homepage << activityTypes["homepage"] << anonymizer(userWhoIsProcessed);
      }
         
      // actor was the (un)enroller, actual user that was (un)enrolled needs to be set as actor.
      string tmp_username; 
      string tmp_userid;
      #pragma omp critical
      {
	tmp_username = anonymizer(UserIDToUserName[userWhoIsProcessed]);
	tmp_userid = anonymizer(userWhoIsProcessed);
      }
      json target_user = {
        { "objectType", "Agent"},
        { "name", tmp_username}, 
        { "account",{}},
      };
      target_user["account"] = {
        {"name", tmp_userid }, 
        {"homePage", target_user_homepage.str()}
      };
         
      // add (un)enroller to related context
      extensions["http://id.tincanapi.com/extension/target"] = target_user;
    }
       
  }
  /*
    "The user with id '' (un)subscribed the user with id '' to/from the discussion with id '' in the forum with the course module id ''."
  */
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' ([[:alnum:]]+) the user with id '([[:digit:]]+)' (to|from) the discussion[[:blank:]]+with id '([[:digit:]]+)' in the forum with the course module id '([[:digit:]]+)'\\.")) )
  {
    // subscribed user to discussion
#pragma omp critical
    userid = anonymizer(match[1]);
    verbname = match[2];
#pragma omp critical
    userWhoIsProcessed = anonymizer(match[3]);
    activityType = "discussion";
    postNumber = match[5];
    activity_id = match[6];
    // check should target be event added if "self"
  }
  /*
    "The user with id '' subscribed the user with id '' to the forum with course module id ''."
  */
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' ([[:alnum:]]+) the user with id '([[:digit:]]+)' to the forum with course module id '([[:digit:]]+)'.")) )
  {
    // subscribed a user to forum
    // subscribed user to discussion
#pragma omp critical
    userid = anonymizer(match[1]);
    verbname = match[2];
#pragma omp critical
    userWhoIsProcessed = anonymizer(match[3]);
    activityType = "forum";
    activity_id = match[4];
    // check should target be event added if "self"
  }
  /*
    "The user with id '' has created the post with id '' in the discussion with id '' in the forum with course module id ''."
    "The user with id '' has created the post with id '' in the discussion with id '' in the forum with the course module id ''."
    "The user with id '' has updated the post with id '' in the discussion with id '' in the forum with course module id ''."
    "The user with id '' has deleted the post with id '' in the discussion with id '' in the forum with course module id ''."
    "The user with id '' has posted content in the forum post with id '' in the discussion '' located in the forum with course module id ''." // REPLY
    "The user with id '' has posted content in the forum post with id '' in the discussion '' located in the forum with the course module id ''." // REPLY

  */
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' has (created|posted content|updated|deleted) (in )?(the )?(forum )?post with id '([[:digit:]]+)' in the discussion (with id )?'([[:digit:]]+)' (located )?in the forum with (the )?course module id '([[:digit:]]+)'\\.")) )
  {
#pragma omp critical
    userid = anonymizer(match[1]);
    verbname = "posted";
#pragma omp critical
    userWhoIsProcessed = anonymizer(match[3]);
    activityType = "reply";
    postNumber = match[6];
    discussionNumber = match[8];
    activity_id = match[8];

    auto activityIt = activityTypes.find("forum");
    if ( activityIt == activityTypes.end()) throw xapi_activity_type_not_supported_error("forum");
    string tmp = activityIt->second + (string)match[11];

    // mark forum as parent 
    activityContext["contextActivities"]["parent"] = {
      { "id", tmp }
    };
       
  }
  /* special case when assignment status is updated by system */
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' updated the completion state for the course module with id '([[:digit:]]+)' for the user with id '([[:digit:]]+)'\\.")) )
  {
    // This can be almost anything for the completion state. It happens to be locale-specific, so it needs additional parsing.
    // Currently we support only finnish.
    auto it = contextModuleLocaleToActivityType.find(context_module_locale_specific);
    if ( it != contextModuleLocaleToActivityType.end())
    {
#pragma omp critical
      userid = anonymizer(match[3]); // target user "completes" module
      activityType = it->second;
      verbname = "completed";
      activity_id = match[2];
    }
    else
    {
      // we never should get here, but better safe than sorry.
      throw xapi_activity_type_not_supported_error("(Localized: "+context_module_locale_specific+")");
    }
      
    // We also assume that you update completion status only to "completed" one.
  }
  else if ( regex_search(description, match, regex("User ([[:digit:]]+) (updated|created) the question ([[:digit:]]+)\\.")))
  {
    verbname = match[2];
    // we cannot create proper log entry using this kind of statement moodle logs give us.
    throw xapi_activity_ignored_error(verbname+":question (question id without proper course module id)");
  }
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '(-?[[:digit:]]+)' ([[:alnum:]]+) the grade with id '([[:digit:]]+)' for the user with id '([[:digit:]]+)' for the grade item with id '([[:digit:]]+)'\\.") ))
  {
    throw xapi_activity_ignored_error("updated/deleted:grade");
  }
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' ([[:alnum:]]+) the (event|backup|instance of enrolment method) '(.*)' with id '([[:digit:]]+)'") ))
  {
    throw xapi_activity_ignored_error("updated:event/instance");
  }
  
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' (has )?([[:alnum:]]+) the (user|grader|history|overview|statistics|outline activity|log)( log| statistics)? report .*")))
  {
    throw xapi_activity_ignored_error("viewed:(log/statistics/history/overview/grade)report");
  }
  else if ( regex_search(description, match,
                         regex("[Tt]he user with id '([[:digit:]]+)' has ([[:alnum:]]+) the list of available badges .*")))
  {
    throw xapi_activity_ignored_error("viewed:available badge list");
  }
  else if ( regex_search(description, match,
			 regex("[Tt]he user with id '([[:digit:]]+)' viewed the report '([[:alnum:]]+)' for the quiz with course module id '([[:digit:]]+)'\\.")))
  {
    throw xapi_activity_ignored_error("viewed:report");
  }
  else if ( regex_search(description, match,
			 regex("[Tt]he user with id '([[:digit:]]+)' (has )?viewed the (sessions of the chat|recent activity|report|edit page|Open Grader|instance list|list of users|list of resources) .*")))
  {
    throw xapi_activity_ignored_error("viewed:(various reports)");
  }
  else if ( regex_search(description, match,
			 regex("[Tt]he user with id '([[:digit:]]+)' (un)?assigned the role with id '([[:digit:]]+)' (to|from) the user with id '([[:digit:]]+)'\\.")))
  {
    throw xapi_activity_ignored_error("(un)assigned:(role)");
  }
  else if ( regex_search(description, match,
			 regex("Kohde luotiin tunnuksella.*|Kohde luotu ID-numerolla.*|Kohde \\(tunnus [[:digit:]]+\\) poistettiin.|Kohde ID-numerolla [[:digit:]]+ poistettu.")))
  {
    throw xapi_activity_ignored_error("created/deleted:target(localized))");
  }
  else if (regex_search(description, match,
			 regex("[Tt]he user with id '([[:digit:]]+)' launched the session with id '([[:digit:]]+)' for the Collab with course module id '([[:digit:]]+)'\\.")))
  {
    throw xapi_activity_ignored_error("launched:Collab");
  }
  else if (regex_search(description, match,
			 regex("[Tt]he user with id '([[:digit:]]+)' deleted the recording with id '([[:digit:]]+)' for the Collab with course module id '([[:digit:]]+)'\\.")))
  {
    throw xapi_activity_ignored_error("deleted:Collab recording");
  }
  else
  {
    // final fallback
    throw xapi_parsing_error("Cannot make sense of: '"+ description+ "'");
  }
  // ignored
  // The user with id '' launched the session with id '' for the Collab with course module id ''.
  // The user with id '' assigned the role with id '' to the user with id ''.
  //"The user with id '' restored old course with id '' to a new course with id ''."
  // The user with id '' viewed the list of users in the course with id ''
  // The user with id '' viewed the log report for the course with id ''
  // The user with id '' has viewed the submission status page for the assignment with course module id ''.
  //  "The user with id '' viewed the grading form for the user with id '' for the assignment with course module id ''."
  //  "The user with id '' viewed the grading table for the assignment with course module id ''."
  // The user with id '' viewed the edit page for the quiz with course module id ''.
  // The user with id '' viewed the list of users in the course with id ''.
  
  if ( username.empty() ) throw xapi_parsing_error("no username found for: " + description);
  #pragma omp critical
  {
    UserNameToUserID[username] = userid;
    UserIDToUserName[userid] = username;
  }
  
  ////////////////////  
  // construct actual user completing a task
  stringstream  homepage;
  homepage << activityTypes["homepage"] << userid;
  stringstream mbox;
  mbox << "mailto:" << UserIDToEmail[userid];
  actor = {
    {"objectType", "Agent"},
    {"name", username},
    {"mbox", mbox.str()},
  };
  
  /*  actor["account"] = {
    {"name", userid }, 
    {"homePage", homepage.str()}
    };*/

  ////////////////////
  // constuct verb in statement
  json verb;
  {
    auto it = supportedVerbs.find(verbname);
    if ( it == supportedVerbs.end())
      throw xapi_verb_not_supported_error(verbname+"' in '"+description);

    string verb_xapi_id = it->second;
    if ( verb_xapi_id.length() == 0 )
    {
      if ( verbname.length() == 0 ) throw xapi_verb_not_supported_error(verbname+"(ignored)" + description);
      else throw xapi_verb_not_supported_error(verbname+"(ignored)");
    }
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
  
  /* construct object id */
  auto it = activityTypes.find(activityType);
  if ( it == activityTypes.end()) throw xapi_activity_type_not_supported_error(verbname+":"+activityType+"' with statement '"+description);
  string object_id = it->second + activity_id;
  
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
  else if ( it->first == "post" || it->first == "reply" )
  {
    stringstream ss;
    ss << object_id << "#p=" << postNumber;
    object_id = ss.str();
  }
  else if ( it->first == "submission"  )
  {
    stringstream ss;
    ss << object_id << "&userid=" << userWhoIsProcessed;
    // target extension holds proper moodle url to open user submission.
    // assignment url will be kept as such (without userid part)
    // so we may map submissions to specific assignments in mongodb queries.
    extensions["http://id.tincanapi.com/extension/target"] = ss.str();
  }
  /* find proper Xapi activity type */ 
  string moodleActivity = it->first;
  auto activityIt = moodleXapiActivity.find(moodleActivity);
  if ( activityIt == moodleXapiActivity.end()) throw xapi_activity_type_not_supported_error(moodleActivity);
  
  string xapiActivity = activityIt->second;
  // assign task map for later use in grading log parsing.
  if ( xapiActivity == "http://id.tincanapi.com/activitytype/school-assignment" ||
       verbname == "completed" )
  {
#pragma omp critical
    TaskNameToTaskID[context] = object_id;    
  }

    
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
  if ( extensions.empty() == false )
    statement["context"]["extensions"] = extensions;

  
  if ( GetTimestamp().length() > 17 )
    throw xapi_parsing_error("Timestamp length is off: " + GetTimestamp());
  return statement.dump();
}
