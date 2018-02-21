#pragma once
#include <xapi_entry.h>
#include <algorithm>

class GradeEntry : public Entry
{
  const std::map<std::string,int> monthValues= {
    { "tammikuu", 1},
    { "helmmikuu", 2},
    { "maaliskuu", 3},
    { "huhtikuu", 4},
    { "toukokuu", 5},
    { "kesäkuu", 6},
    { "heinäkuu", 7},
    { "elokuu", 8},
    { "syyskuu", 9},
    { "lokakuu", 10},
    { "marraskuu", 11},
    { "joulukuu", 12}
  };
  
public:

  void ParseTimestamp(const string & strtime) override
  {
    std::vector<string> tokens;
    boost::split( tokens, strtime, boost::is_any_of(". :"));
    /*int c=0;
    for( string & t : tokens )
    {
      cerr << c++ << ":" << t << "\n";
      }*/
    
    {
      stringstream ss;
      ss << tokens.at(1);
      if ( !(ss >> when.tm_mday) ) throw runtime_error("Conversion error, day");
    }
    {
      stringstream ss;
      auto it = monthValues.find(tokens.at(2));
      ss << it->second;
      
      if ( !(ss >> when.tm_mon)  ) throw runtime_error("Conversion error, mon");
    }
    {
      stringstream ss;
      ss << tokens.at(3);
      if ( !(ss >> when.tm_year)  ) throw runtime_error("Conversion error, year");
    }

    {
      stringstream ss;
      ss << tokens.at(4);
      if ( !(ss >> when.tm_hour)  ) throw runtime_error("Conversion error, hour");
    }

    {
      stringstream ss;
      ss << tokens.at(5);
      if ( !(ss >> when.tm_sec)  ) throw runtime_error("Conversion error, second");
    }
  }
  
  void Parse( const std::vector<std::string> & vec ) 
  {
      // break line into comma-separated parts.
    /*int c=0;
    for( auto s : vec )
    {
      cerr << c++ << ":" << s << "\n";
      }*/
		     
    ParseTimestamp(vec.at(0));
    username = vec.at(1); // gradee

    related_username = vec.at(6); // grader
    
    context = vec.at(3); // task name
    
    component = vec.at(5); // latest grade score
    // decimal comma into dot
    for( auto & c : component)    {      if ( c == ',' ) c = '.';    }
    event = vec.at(4); // previous grade score
    // decimal comma into dot
    for( auto & c : event)    {      if ( c == ',' ) c = '.';    }
    
    description = vec.at(7); // type
    
    if (!HasGradeScore()) throw xapi_no_result_error();
  }
  bool HasGradeScore() {
    return !component.empty();
  } 
  float GetGradeScore()
  {
    stringstream ss;
    ss << component;
    int value;
    ss >> value;
    return value;
  }
  std::string GetTaskName(){
    // prefix required since other logs have it.
    return context;
  }
  std::string ToXapiStatement() override
  {
    json statement;
    
    json actor;
    json verb;
    json object;
    json result;
    
    if ( UserNameToUserID.find(username) == UserNameToUserID.end())
    {
      throw xapi_cached_user_not_found_error(userid);
    }
    stringstream	homepage;
    homepage << HOMEPAGE_URL_PREFIX <<  UserNameToUserID[userid];

    // define user receiving a score
    actor = {
      	{"objectType", "Agent"},
	{"name", username},
	{"account",
	 {
	   {"name", UserNameToUserID[userid] },
	   {"homePage", homepage.str() }
	 }
	}
    };
    // construct scoring verb
    string verbname = "scored";
    auto it = supportedVerbs.find(verbname);
    string verb_xapi_id = it->second;
    verb = {
      {"id", verb_xapi_id },
      {"display",
       {
	 {"en-GB", verbname} // this will probably depend on subtype (see object)
       }
      }
    };

    
    // construct result 
    result["score"] = 	{
	  { "min", 0 },
	  { "max", 100 },
	  { "raw", GetGradeScore()}
    };
    result["success"] = true;
    result["completion"] = true;
    

    if ( TaskNameToTaskID.find(GetTaskName()) == TaskNameToTaskID.end())
    {
      throw xapi_cached_task_not_found_error(GetTaskName());
    }
    // define object result relates to
    object = {
      { "objectType", "Activity"},
      { "id", TaskNameToTaskID[GetTaskName()] },
      { "definition",
	{
	  /*{ "description", { "en-GB", ""}},*/
	  { "type" , "http://id.tincanapi.com/activitytype/school-assignment"},
	  { "interactionType", "other" }
	}
      }
    };
    
    object["definition"]["name"] =  {
      {"en-GB", context}
    };   

    statement["actor"] = actor;
    statement["verb"] = verb;
    statement["timestamp"] = GetTimestamp();
    statement["result"] = result;
    statement["object"] = object;
    return statement.dump();
  }
};