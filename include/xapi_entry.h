#pragma once
#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <regex>
#include <json.hpp>
////////////////////////////////////////////////////////////////////////////////
extern const std::map<std::string, std::string> supportedVerbs;
extern const std::map<std::string, std::string> activityTypes;
extern std::map<std::string, std::string> TaskNameToTaskID;
extern std::map<std::string, std::string> UserNameToUserID;
////////////////////////////////////////////////////////////////////////////////
namespace XAPI
{
  const std::string HOMEPAGE_URL_PREFIX = "https://moodle.karelia.fi/user/profile.php?id=";
  class Entry
  {
  protected:
    Entry();
  public:
    struct tm when {0};
    nlohmann::json statement;
    
    // do we need these?
    std::string username;
    std::string related_username;
    std::string userid;
    std::string related_userid;
    std::string context; // generally the course
    std::string component;
    std::string event;
    std::string verb; // xapi-related
    std::string description; // moodle event id description
    std::string ip_address;
  public:    
    virtual ~Entry();
    // parses time structure from string.
    virtual void ParseTimestamp(const std::string & strtime) = 0;

    /// Returns string as yyyy-mm-ddThh:mmZ
    std::string GetTimestamp() const;

    virtual std::string ToXapiStatement() = 0;

  };
}