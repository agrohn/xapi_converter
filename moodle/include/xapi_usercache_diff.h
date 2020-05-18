#pragma once
#include <string>
#include <list>
#include <xapi_usercacheapp.h>
////////////////////////////////////////////////////////////////////////////////
namespace XAPI
{
  class UserCacheDiff : public XAPI::UserCacheApp
  {
  protected:

    std::string previousFile;
    std::string currentFile;
    std::string deletedFile;
    std::string addedFile;

    UserMap previousUsers;
    UserMap currentUsers;
    std::list<User> deletedUsers;
    std::list<User> addedUsers;

  public:
    UserCacheDiff(int argc, char **argv);
    bool ParseOptions() override;
    void Run() override;
  };
}
////////////////////////////////////////////////////////////////////////////////
