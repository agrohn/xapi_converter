#pragma once
#include <xapi_entry.h>
#include <string>
////////////////////////////////////////////////////////////////////////////////
namespace XAPI
{
  class GradeEntry : public XAPI::Entry
  {
  public:
    
    void ParseTimestamp(const std::string & strtime) override;
    void Parse( const std::vector<std::string> & vec, bool anonymize = false ) ;
    
    bool HasGradeScore();
    float GetGradeScore();

    std::string GetTaskName();
    std::string ToXapiStatement(bool anonymize = false) override;
  };
}
////////////////////////////////////////////////////////////////////////////////