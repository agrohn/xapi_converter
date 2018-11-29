#include <xapi_statementfactory.h>
#include <xapi_activityentry.h>
#include <xapi_grade.h>
#include <cstdlib>
using namespace std;
using namespace XAPI;
////////////////////////////////////////////////////////////////////////////////
std::map<std::string, std::string> TaskNameToTaskID = {};
std::map<std::string, std::string> UserNameToUserID = {};
std::map<std::string, std::string> UserIDToUserName = {};
std::string XAPI::StatementFactory::course_id = std::string();
std::string XAPI::StatementFactory::course_name = std::string();
////////////////////////////////////////////////////////////////////////////////
XAPI::Anonymizer  anonymizer; 
////////////////////////////////////////////////////////////////////////////////
const std::map<std::string, std::string> supportedVerbs = {
  /*{ "added", ""},*/
  { "assigned",""}, // ignore role assignments
  { "created","http://activitystrea.ms/schema/1.0/create"},
  { "deleted","http://activitystrea.ms/schema/1.0/delete"},
  { "enrolled","http://activitystrea.ms/schema/1.0/join"},
  /*{ "ended", ""}, */
  { "exported", ""}, // ignore grade exports
  /* { "graded", ""},*/
  { "posted", "http://id.tincanapi.com/verb/replied"},
  { "searched", "http://activitystrea.ms/schema/1.0/search"},
  { "started", "http://activitystrea.ms/schema/1.0/start"},
  { "submitted", "http://activitystrea.ms/schema/1.0/submit"},
  // { "attempted", "http://adlnet.gov/expapi/verbs/attempted"},
  { "scored", "http://adlnet.gov/expapi/verbs/scored" },
  /*{ "uploaded", ""},
    { "launched", ""},*/
  { "reviewed", "http://id.tincanapi.com/verb/reviewed" },
  { "subscribed", "http://activitystrea.ms/schema/1.0/follow"},
  { "unassigned", ""}, // ignore role unassignments
  { "unenrolled", "http://activitystrea.ms/schema/1.0/remove"},
  { "updated", "http://activitystrea.ms/schema/1.0/update"},
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
  { "lesson", "/mod/lesson/view.php?id=" },
  { "section", "/course/view.php?id=" }, // needs to add also #section-<NUMBER>
  { "hvp", "/mod/hvp/view.php?id=" },
  { "attempt", "/mod/quiz/review.php?attempt=" },
  { "book", "/mod/book/view.php?id="},
  { "chapter", "/mod/book/view.php?id="}, // needs to add also &chapterid=<NUMBER>
  { "discussion", "/mod/forum/discuss.php?d="},
  { "user", "/user/profile.php?id="},
  { "post", "/mod/forum/discuss.php?d="}, // needs to add #p<POST NUMBER>
  { "reply", "/mod/forum/discuss.php?d="}, // needs to add #p<POST NUMBER>
  { "homepage", "/user/profile.php?id="} // not really activity, just helps in creating user home page addresses.
};
// some heuristics to match completion state updates 
const std::map<std::string, std::string> contextModuleLocaleToActivityType = {
  { "Sivu",       "page" },
  { "Tentti",      "quiz"},
  { "H5P",         "hvp" },
  { "Ohjeteksti", "course"} // Since it is pretty impossible to provide a link to specific label, we do it with course.
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
  { "reply", "http://id.tincanapi.com/activitytype/forum-reply"}
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
