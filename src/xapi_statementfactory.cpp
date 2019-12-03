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
#include <xapi_statementfactory.h>
#include <xapi_activityentry.h>
#include <xapi_grade.h>
#include <xapi_assignmentinit.h>
#include <cstdlib>
using namespace std;
using namespace XAPI;
////////////////////////////////////////////////////////////////////////////////
std::map<std::string, std::string> TaskNameToTaskID = {};
std::map<std::string, std::string> UserNameToUserID = {};
std::map<std::string, std::string> UserIDToUserName = {};
std::map<std::string, std::string> UserIDToEmail = {};
std::map<std::string, std::vector<std::string>> UserIDToRoles = {};

std::string XAPI::StatementFactory::course_id = std::string();
std::string XAPI::StatementFactory::course_name = std::string();
std::string XAPI::StatementFactory::course_start_date = std::string();
std::string XAPI::StatementFactory::course_end_date = std::string();
////////////////////////////////////////////////////////////////////////////////
XAPI::Anonymizer  anonymizer; 
////////////////////////////////////////////////////////////////////////////////
const std::map<std::string, std::string> supportedVerbs = {
  /*{ "added", ""},*/
  { "assigned","http://activitystrea.ms/schema/1.0/assign"}, 
  { "authorized", "http://activitystrea.ms/schema/1.0/authorize"}, // when task / assignment is set. Used to make retrieving task lists easily.
  { "answered", "http://adlnet.gov/expapi/verbs/answered" },
  { "created","http://activitystrea.ms/schema/1.0/create"},
  { "deleted","http://activitystrea.ms/schema/1.0/delete"},
  { "enrolled","http://activitystrea.ms/schema/1.0/join"},
  { "joined","http://activitystrea.ms/schema/1.0/join"},
  { "left", "http://activitystrea.ms/schema/1.0/leave" },
  { "commented", "http://adlnet.gov/expapi/verbs/commented" },
  /*{ "ended", ""}, */
  { "exported", ""}, // ignore grade exports
  { "evaluated", "http://www.tincanapi.co.uk/verbs/evaluated"}, // grading is interpreted as evaluation
  { "posted", "http://id.tincanapi.com/verb/replied"},
  { "searched", "http://activitystrea.ms/schema/1.0/search"},
  { "started", "http://activitystrea.ms/schema/1.0/start"},
  { "submitted", "http://activitystrea.ms/schema/1.0/submit"},
  // { "attempted", "http://adlnet.gov/expapi/verbs/attempted"},
  { "scored", "http://adlnet.gov/expapi/verbs/scored" },
  /* { "launched", ""},*/
  { "reviewed", "http://id.tincanapi.com/verb/reviewed" },
  { "subscribed", "http://activitystrea.ms/schema/1.0/follow"},
  { "unsubscribed", "http://activitystrea.ms/schema/1.0/stop-following"},
  { "unassigned", ""}, // ignore role unassignments
  { "unenrolled", "http://activitystrea.ms/schema/1.0/remove"},
  { "updated", "http://activitystrea.ms/schema/1.0/update"},
  { "uploaded", "http://activitystrea.ms/schema/1.0/attach" },
  { "viewed","http://id.tincanapi.com/verb/viewed"},
  { "completed", "http://activitystrea.ms/schema/1.0/complete"},
  { "previewed", ""}, // ignore previews
  { "printed", ""}, // ignore book prints
  { "restored", ""} // ignore course restores to a new course

};

// supported activity target types
std::map<std::string, std::string> activityTypes = {
  { "collaborate", "/mod/collaborate/view.php?id="},
  { "quiz", "/mod/quiz/view.php?id=" },
  { "page", "/mod/page/view.php?id=" },
  { "resource", "/mod/resource/view.php?id="},
  { "url", "/mod/url/view.php?id=" },
  { "forum", "/mod/forum/view.php?id="},
  { "hsuforum", "/mod/hsuforum/view.php?id="},
  { "lti", "/mod/lti/view.php?id=" },
  { "course", "/course/view.php?id="},
  { "assignment", "/mod/assign/view.php?id=" },
  { "submission", "/mod/assign/view.php?id=" }, // submissions are for assignments. Extensions will hold unique moodle id for submission (attempt-id, target)
  { "lesson", "/mod/lesson/view.php?id=" },
  { "section", "/course/view.php?id=" }, // needs to add also #section-<NUMBER>
  { "hvp", "/mod/hvp/view.php?id=" },
  { "attempt", "/mod/quiz/review.php?attempt=" }, // attempts are for quizzes
  { "book", "/mod/book/view.php?id="},
  { "chapter", "/mod/book/view.php?id="}, // needs to add also &chapterid=<NUMBER>
  { "discussion", "/mod/forum/discuss.php?d="},
  { "user", "/user/profile.php?id="},
  { "post", "/mod/forum/discuss.php?d="}, // needs to add #p<POST NUMBER>
  { "reply", "/mod/forum/discuss.php?d="}, // needs to add #p<POST NUMBER>
  { "homepage", "/user/profile.php?id="}, // not really activity, just helps in creating user home page addresses.
  { "label", "/course/modedit.php?update=" }, // unique ids for entire site labels, it seems
  { "question", "/question/question.php?cmid=62172&id=" }, // edit URL
  { "questionnaire", "/mod/questionnaire/view.php?id=" },
  { "feedback", "/mod/feedback/view.php?id=" },
  { "chat", "/mod/chat/view.php?id=" }
};
// some heuristics to match completion state updates 
const std::map<std::string, std::string> contextModuleLocaleToActivityType = {
  { "Sivu",             "page" },
  { "Tentti",           "quiz"},
  { "H5P",              "hvp" },
  { "Ohjeteksti",       "label"},
  { "Keskustelualue",   "forum"},
  { "Palaute",          "feedback"},
  { "Tehtävä",          "assignment"},
  /*{ "Tiedosto",         "file" }, these do not have purpose atm*/
  { "Verkko-osoite",    "url" }
};

const std::map<std::string, std::string> moodleXapiActivity = {
  { "collaborate", "http://adlnet.gov/expapi/activities/media"},
  { "quiz", "http://id.tincanapi.com/activitytype/school-assignment" }, // or http://adlnet.gov/expapi/activities/assessment
  { "page", "http://adlnet.gov/expapi/activities/media" },
  { "resource", "http://adlnet.gov/expapi/activities/media"},
  { "url", "http://adlnet.gov/expapi/activities/media" },
  { "forum", "https://w3id.org/xapi/acrossx/activities/online-discussion"},
  { "hsuforum", "https://w3id.org/xapi/acrossx/activities/online-discussion"},
  { "lti", "http://adlnet.gov/expapi/activities/media"},
  { "course", "http://adlnet.gov/expapi/activities/media"},
  { "assignment", "http://id.tincanapi.com/activitytype/school-assignment" },
  { "lesson", "http://id.tincanapi.com/activitytype/school-assignment" },
  { "quiz", "http://id.tincanapi.com/activitytype/school-assignment" },
  { "section", "http://id.tincanapi.com/activitytype/section" },
  { "hvp", "http://adlnet.gov/expapi/activities/media" },
  { "attempt", "http://id.tincanapi.com/activitytype/solution" },
  { "book", "http://id.tincanapi.com/activitytype/book" },
  { "chapter", "http://id.tincanapi.com/activitytype/chapter" },
  { "discussion", "http://id.tincanapi.com/activitytype/discussion" },
  { "user", "http://id.tincanapi.com/activitytype/user-profile" },
  { "post", "http://id.tincanapi.com/activitytype/forum-reply"},
  { "reply", "http://id.tincanapi.com/activitytype/forum-reply"},
  { "label", "http://activitystrea.ms/schema/1.0/note"},
  { "submission", "http://id.tincanapi.com/activitytype/school-assignment"},
  { "question", "http://activitystrea.ms/schema/1.0/question" },
  { "questionnaire", "http://id.tincanapi.com/activitytype/survey" },
  /* { "file", "http://activitystrea.ms/schema/1.0/file" }, this does not have purpose atm */
  { "feedback", "http://id.tincanapi.com/activitytype/survey" },
  { "chat", "http://id.tincanapi.com/activitytype/chat-channel" }

};
////////////////////////////////////////////////////////////////////////////////
XAPI::StatementFactory::StatementFactory()
{
  
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::StatementFactory::CacheUser( const std::string & line )
{
  ActivityEntry e;
  e.course_id = course_id;
  e.course_name = course_name;
  e.Parse(line);
  e.UpdateUserData();
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::StatementFactory::CacheUser( const std::vector<std::string> & lineAsVector )
{
  ActivityEntry e;
  // set from command line
  e.course_id = course_id;
  e.course_name = course_name;
  e.Parse(lineAsVector);
  e.UpdateUserData();
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::StatementFactory::CacheUser( const std::string & name,
                                   const std::string & userid,
                                   const std::string & email,
                                   const std::vector<std::string> & roles)
{
#pragma omp critical
    {
      UserNameToUserID[name] = anonymizer(userid);
      UserIDToUserName[userid] = anonymizer(name);
      // This is the only point where email can be set - this makes users json file obligatory from now on.
      UserIDToEmail[userid] = anonymizer(email);
      UserIDToRoles[userid] = roles;
    }
}
////////////////////////////////////////////////////////////////////////////////
std::string
XAPI::StatementFactory::CreateActivity( const std::string & line )
{
  ActivityEntry e;
  // set from command line
  e.course_id = course_id;
  e.course_name = course_name;
  e.Parse(line);
  return e.ToXapiStatement();
}
////////////////////////////////////////////////////////////////////////////////
std::string
XAPI::StatementFactory::CreateActivity( const std::vector<std::string> & lineAsVector )
{
  ActivityEntry e;
  // set from command line
  e.course_id = course_id;
  e.course_name = course_name;
  e.Parse(lineAsVector);
  return e.ToXapiStatement();
}
////////////////////////////////////////////////////////////////////////////////
std::string
XAPI::StatementFactory::CreateGradeEntry( const std::vector<std::string> & lineAsVector )
{
  GradeEntry e;
  // set from command line
  e.course_id = course_id;
  e.course_name = course_name;
  e.Parse(lineAsVector);
  return e.ToXapiStatement();
}
////////////////////////////////////////////////////////////////////////////////
std::string
XAPI::StatementFactory::CreateAssignmentInitEntry( const std::string & name, const std::string & id )
{
  AssignmentInitEntry e;
  
  // set from command line
  e.course_id = course_id;
  e.course_name = course_name;
  e.name = name;
  e.id = id;
  e.ParseTimestamp(course_start_date);
  return e.ToXapiStatement();
}
////////////////////////////////////////////////////////////////////////////////
std::string
XAPI::StatementFactory::CreateRoleAssignEntry( const std::string & id, const std::vector<std::string> & roles )
{
  AssignmentInitEntry e;
  
  // set from command line
  e.course_id = course_id;
  e.course_name = course_name;
  e.name = UserIDToUserName[name];
  e.id = id;
  e.ParseTimestamp(course_start_date);
  return e.ToXapiStatement();
}
////////////////////////////////////////////////////////////////////////////////
