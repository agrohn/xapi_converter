#pragma once
#include <string>
#include <xapi_usercacheapp.h>
////////////////////////////////////////////////////////////////////////////////
namespace XAPI
{
  class UserCacheCombiner : public XAPI::UserCacheApp
  {
  protected:
    std::vector<std::string> userCacheFiles;
    std::string outfile; 
  public:
    UserCacheCombiner(int argc, char **argv);
    bool ParseOptions() override;
    void Run() override;
  };
}
////////////////////////////////////////////////////////////////////////////////
