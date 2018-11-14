#pragma once
#include <stdexcept>
#include <string>
////////////////////////////////////////////////////////////////////////////////
class xapi_parsing_error : public std::runtime_error {
private:
  std::string verb;
public:
  xapi_parsing_error(const std::string & tmp ) : std::runtime_error( tmp )
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
class xapi_could_not_create_statement_error : public xapi_parsing_error {
public:
  xapi_could_not_create_statement_error(const std::string & tmp ) : xapi_parsing_error(tmp){}
};
////////////////////////////////////////////////////////////////////////////////
class xapi_activity_type_not_supported_error : public xapi_parsing_error {
public:
  xapi_activity_type_not_supported_error(const std::string & tmp ) : xapi_parsing_error( "Activity type '" + tmp + "' not supported"){}
};
class xapi_cached_user_not_found_error : public xapi_parsing_error {
public:
  xapi_cached_user_not_found_error(const std::string & tmp ) : xapi_parsing_error( "User with id '" + tmp + "' not previously found in activity records."){}
};
class xapi_cached_task_not_found_error : public xapi_parsing_error {
public:
  xapi_cached_task_not_found_error(const std::string & tmp ) : xapi_parsing_error( "Id for task name '" + tmp + "' not found in previous logs."){}
};
class xapi_no_result_error : public xapi_parsing_error {
public:
  xapi_no_result_error() : xapi_parsing_error( "grade log entry did not contain a result."){}
};
class xapi_activity_ignored_error : public xapi_parsing_error {
public:
  xapi_activity_ignored_error(const std::string & tmp ) : xapi_parsing_error( "Activity '" + tmp + "' ignored."){}
};