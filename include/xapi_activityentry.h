#include <xapi_entry.h>
#include <xapi_anonymizer.h>
#include <iomanip>
#include <ctime>
#include <regex>
#include <json.hpp>
////////////////////////////////////////////////////////////////////////////////
extern const std::map<std::string, std::string> supportedVerbs;
extern const std::map<std::string, std::string> activityTypes;
extern const std::map<std::string, std::string> moodleXapiActivity;
extern const std::map<std::string, std::string> contextModuleLocaleToActivityType;
extern std::map<std::string, std::string> TaskNameToTaskID;
extern std::map<std::string, std::string> UserNameToUserID;
extern std::map<std::string, std::string> UserIDToUserName;
extern XAPI::Anonymizer  anonymized_usernames;
////////////////////////////////////////////////////////////////////////////////
namespace XAPI
{
  class ActivityEntry : public XAPI::Entry
  {
  public:

    // parses time structure from string.
    void ParseTimestamp(const std::string & strtime) override;
    void Parse(const std::string & line, bool anonymize = false  );
    void Parse(const std::vector<std::string> & lineAsVector, bool anonymize = false );
    std::string ToXapiStatement(bool anonymize = false) override;
  };
}
