#pragma once
#include <xapi_entry.h>
#include <string>
////////////////////////////////////////////////////////////////////////////////
namespace XAPI
{
  class AssignmentInitEntry : public XAPI::Entry
  {
  public:
    std::string name;
    std::string id;
    std::string ToXapiStatement() override;
    void ParseTimestamp(const std::string & strtime) override;
  };
}
////////////////////////////////////////////////////////////////////////////////