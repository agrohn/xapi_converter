#pragma once
////////////////////////////////////////////////////////////////////////////////
class xapi_parsing_error : public std::runtime_error {
private:
  std::string verb;
public:
  xapi_parsing_error(const std::string & tmp ) : std::runtime_error( tmp)
 {
    verb = tmp;
  }
  const std::string & get_verb() const { return verb; }
};
////////////////////////////////////////////////////////////////////////////////
class xapi_verb_not_supported_error : public xapi_parsing_error {
public:
  xapi_verb_not_supported_error(const std::string & tmp ) : xapi_parsing_error( "Verb '" + tmp + "' not supported"){}
};
////////////////////////////////////////////////////////////////////////////////
class xapi_activity_type_not_supported_error : public xapi_parsing_error {
public:
  xapi_activity_type_not_supported_error(const std::string & tmp ) : xapi_parsing_error( "Activity type'" + tmp + "' not supported"){}
};
