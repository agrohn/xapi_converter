#pragma once
#include <map>
#include <string>
#include <sstream>
////////////////////////////////////////////////////////////////////////////////
namespace XAPI
{
  class Anonymizer : public std::map<std::string, std::string>
  {
  private:
    std::string get_random_string(size_t length = 32)
    {
      const char alphanum[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";
      size_t alphanumLength = sizeof(alphanum) - 1;
      std::stringstream ss;
      
      for( size_t i=0;i<length;i++)
      {
	ss << alphanum[rand() % alphanumLength];
      }
      return ss.str();
    }
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
	  return insert( std::pair<std::string,std::string>(key, get_random_string())).first->second;
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