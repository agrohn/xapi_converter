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
    static std::string course_start_date;
    static std::string course_end_date;
    static std::string CreateActivity( const std::string & moodleLogLine );
    static std::string CreateActivity( const std::vector<std::string> & lineAsVector );
    static std::string CreateGradeEntry( const std::vector<std::string> & lineAsVector );
    static std::string CreateAssignmentInitEntry( const std::string & name, const std::string & id );
    static void CacheUser( const std::vector<std::string> & lineAsVector );
    static void CacheUser( const std::string & moodleLogLine );
    static void CacheUser( const std::string & name, const std::string & userid, const std::string & email);
  };
}
////////////////////////////////////////////////////////////////////////////////
