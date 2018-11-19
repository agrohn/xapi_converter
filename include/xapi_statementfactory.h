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
    static std::string CreateActivity( const std::string & moodleLogLine, bool anonymize = false );
    static std::string CreateActivity( const std::vector<std::string> & lineAsVector, bool anonymize = false);
    static std::string CreateGradeEntry( const std::vector<std::string> & lineAsVector, bool anonymize = false );
  };
}
////////////////////////////////////////////////////////////////////////////////
