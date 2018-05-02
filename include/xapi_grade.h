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
    void Parse( const std::vector<std::string> & vec ) ;
    
    bool HasGradeScore();
    float GetGradeScore();

    std::string GetTaskName();
    std::string ToXapiStatement() override;
  };
}
////////////////////////////////////////////////////////////////////////////////