#include <xapi_statementfactory.h>
#include <xapi_activityentry.h>
#include <xapi_grade.h>
using namespace std;
using namespace XAPI;
////////////////////////////////////////////////////////////////////////////////
const string VERB_URL_PREFIX = "http://adlnet.gov/expapi/verbs/viewed";
std::map<std::string, std::string> TaskNameToTaskID = {};
std::map<std::string, std::string> UserNameToUserID = {};

std::string XAPI::StatementFactory::course_id = std::string();
std::string XAPI::StatementFactory::course_name = std::string();
////////////////////////////////////////////////////////////////////////////////
const std::map<std::string, std::string> supportedVerbs = {
  /*{ "added", ""},
    { "assigned",""},
    { "created",""},
    { "deleted",""},
    { "enrolled",""},
    { "ended", ""}, 
    { "graded", ""},
    { "posted", ""},
    { "searched", ""},
    { "started", ""},*/
    { "submitted", "http://adlnet.gov/expapi/verbs/answered"},
    // { "attempted", "http://adlnet.gov/expapi/verbs/attempted"},
    { "scored", "http://adlnet.gov/expapi/verbs/scored" },
    /*{ "uploaded", ""},
    { "launched", ""},
    { "subscribed", ""},
    { "unassigned", ""},
    { "unenrolled", ""},
    { "updated", ""},*/
    { "viewed","http://id.tincanapi.com/verb/viewed"}
};

// supported activity target types
const std::map<std::string, std::string> activityTypes = {
  { "collaborate", "https://moodle.karelia.fi/mod/collaborate/view.php?id="},
  { "quiz", "https://moodle.karelia.fi/mod/quiz/view.php?id=" },
  { "page", "https://moodle.karelia.fi/mod/page/view.php?id=" },
  { "resource", "https://moodle.karelia.fi/mod/resource/view.php?id="},
  { "url", "https://moodle.karelia.fi/mod/url/view.php?id=" },
  { "forum", "https://moodle.karelia.fi/mod/forum/view.php?id="},
  { "hsuforum", "https://moodle.karelia.fi/mod/hsuforum/view.php?id="},
  { "lti", "https://moodle.karelia.fi/mod/lti/view.php?id=" },
  { "course", "https://moodle.karelia.fi/course/view.php?id="},
  { "assignment", "https://moodle.karelia.fi/mod/assign/view.php?id=" },
  { "quiz", "https://moodle.karelia.fi/mod/quiz/view.php?id=" }
};
const std::map<std::string, std::string> moodleXapiActivity = {
  { "collaborate", "http://adlnet.gov/expapi/activities/media"},
  { "quiz", "http://id.tincanapi.com/activitytype/school-assignment" },
  { "page", "http://adlnet.gov/expapi/activities/media" },
  { "resource", "http://adlnet.gov/expapi/activities/media"},
  { "url", "http://adlnet.gov/expapi/activities/media" },
  { "forum", "https://w3id.org/xapi/acrossx/activities/online-discussion"},
  { "hsuforum", "https://w3id.org/xapi/acrossx/activities/online-discussion"},
  { "lti", "http://adlnet.gov/expapi/activities/media"},
  { "course", "http://adlnet.gov/expapi/activities/media"},
  { "assignment", "http://id.tincanapi.com/activitytype/school-assignment" },
  { "quiz", "http://id.tincanapi.com/activitytype/school-assignment" }
};
////////////////////////////////////////////////////////////////////////////////
XAPI::StatementFactory::StatementFactory()
{
  
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
