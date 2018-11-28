#pragma once
#include <string>
#include <vector>
////////////////////////////////////////////////////////////////////////////////
namespace XAPI
{
  class xapi_entry;
  //////////////////////////////////////////////////////////////////////////////
  class StatementFactory
  {
  private:
    StatementFactory();
  public:
    static std::string course_id;
    static std::string course_name;
    static std::string CreateActivity( const std::string & moodleLogLine );
    static std::string CreateActivity( const std::vector<std::string> & lineAsVector );
    static std::string CreateGradeEntry( const std::vector<std::string> & lineAsVector );
  };
}
////////////////////////////////////////////////////////////////////////////////
