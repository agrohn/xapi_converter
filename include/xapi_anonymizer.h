#pragma once
#include <map>
#include <string>
#include <sstream>
////////////////////////////////////////////////////////////////////////////////
namespace XAPI
{
  class Anonymizer : public std::map<std::string, std::string>
  {
  public:
    bool enabled{false};
    Anonymizer() {    srand(time(NULL));   }
    inline std::string & operator()(const std::string & key )
    {
      auto it = this->find(key);
      if ( it == this->end())
      {
	if ( enabled )
	{
	  // create random string for usernaeme
	  std::stringstream ss;
	  ss << "user" << rand()%10000;
	  return insert( std::pair<std::string,std::string>(key, ss.str())).first->second;
	}
	else
	{
	  // mark key also as value (effectively does not anonymize anything).
	  return insert( std::pair<std::string,std::string>(key, key)).first->second;
	}
      }
      else 
	return it->second;
    }
  };
}