#include <xapi_entry.h>
#include <iomanip>
#include <ctime>
#include <regex>
#include <json.hpp>
////////////////////////////////////////////////////////////////////////////////
extern const std::map<std::string, std::string> supportedVerbs;
extern const std::map<std::string, std::string> activityTypes;
extern const std::map<std::string, std::string> moodleXapiActivity;
extern std::map<std::string, std::string> TaskNameToTaskID;
extern std::map<std::string, std::string> UserNameToUserID;
////////////////////////////////////////////////////////////////////////////////
namespace XAPI
{
  class ActivityEntry : public XAPI::Entry
  {
  public:

    // parses time structure from string.
    void ParseTimestamp(const std::string & strtime) override;
    void Parse(const std::string & line );
    std::string ToXapiStatement() override;
  };
}