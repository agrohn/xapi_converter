/*
  This file is part of xapi_converter.  Parses moodle logs and constructs 
  xAPI statements out of them, sending them to LRS.

  Copyright (C) 2018-2020 Anssi Gröhn
  
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <xapi_converter_moodle.h>
#include <xapi_errors.h>
#include <xapi_entry.h>
#include <xapi_grade.h>
#include <xapi_statementfactory.h>
#include <xapi_anonymizer.h>
#include <omp.h>
#include <string>
#include <cctype>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include <curlpp/Infos.hpp>
#include <fstream>
#include <sys/stat.h>
using json = nlohmann::json;
using namespace std;
//https://nithinkk.wordpress.com/2017/03/16/learning-locker/
////////////////////////////////////////////////////////////////////////////////
namespace po = boost::program_options;
extern std::map<std::string,int> errorMessages;
extern XAPI::Anonymizer anonymizer;
#define DESCRIPTION "Command-line tool for sending XAPI statements to learning locker from  Moodle logs\n"\
  "Released under GPLv3 - use at your own risk. \n\n"\
  "Prerequisities:\n\tLearning locker client credentials must be in json format in data/config.json.\n"\
  "\tSimple object { \"login\": { \"key\": \"\", \"secret\":\"\"}, \"lms\" : { \"baseURL\" : \"\" }"\
  "\n\nUsage"
////////////////////////////////////////////////////////////////////////////////
XAPI::MoodleParser::MoodleParser() : XAPI::Application(DESCRIPTION)
{

  ////////////////////////////////////////////////////////////////////////////////
  // For some hints on usage...
  // options 
  desc.add_options()
  ("log", po::value<string>(), "<YOUR-LOG-DATA.json> Actual course log data in JSON format")
  ("grades", po::value<string>(), "<YOUR-GRADE-DATA.json> Grade history data obtained from moodle")
  ("users", po::value<string>(), "<YOUR-USERS-DATA.json> User (participant) data obtained from moodle")
  ("groups", po::value<string>(), "<YOUR-GROUPS-DATA.json> Group data obtained from moodle")
  ("courseurl", po::value<string>(), "<course_url> Unique course moodle web address, ends in ?id=xxxx")
  ("coursename", po::value<string>(), "<course_name> Human-readable name for the course")
  ("authorize-assignments", "if defined, assignment authorization events will be created." )
  ("course-start", po::value<string>(), "<date> Course start date in format YYYY-MM-DD." );

}
XAPI::MoodleParser::~MoodleParser() 
{

}
////////////////////////////////////////////////////////////////////////////////
bool
XAPI::MoodleParser::ParseCustomArguments()
{

  if ( vm.count("log") > 0 && vm.count("load") > 0 )
  {
    cerr << "Error: --log or --load cannot be used same time!\nPlease see usage with --help.\n";
    return false;
  }
  if ( vm.count("write") > 0 && vm.count("load") > 0 )
  {
    cerr << "Error: You want me to load files from disk and write them again? "
         << "--write or --load cannot be used same time!\nPlease see usage with --help.\n"; 
    return false;
  }
  else if ( vm.count("log") > 0 )
  {
    data = vm["log"].as<string>();
  }


  if ( vm.count("grades") )
    gradeData = vm["grades"].as<string>();
  
  if ( vm.count("users" ))
  {
    userData = vm["users"].as<string>();
  }

  if ( vm.count("groups"))
  {
    groupData = vm["groups"].as<string>();
  }
  // users should always be set for conversion
  if ( vm.count("users") == 0 && vm.count("load") == 0)
  {
    cerr << "Error: users file is not set!\nPlease see usage with --help.\n";
    return false;
  }

  if ( vm.count("groups") == 0 && vm.count("load") == 0)
  {
    cerr << "Error: groups file is not set!\nPlease see usage with --help.\n";
    return false;
  }
  
  if ( vm.count("courseurl") == 0)
  {
    // require course url only when log is specified.
    if ( load == false )
    {
      cerr << "Error: courseurl is missing!\nPlease see usage with --help.\n";
      return false;
    }
  }
  else
  {
    context.courseurl = vm["courseurl"].as<string>();
  }
  // require course name only when log is specified
  if ( vm.count("coursename") == 0)
  {
    if ( load == false )
    {
      cerr << "Error: coursename is missing!\nPlease see usage with --help.\n";
      return false;
    }
  }
  else
  {
    context.coursename = vm["coursename"].as<string>();
  }
  

  
  if ( vm.count("authorize-assignments") > 0 ) 
  {
    if ( vm.count("course-start") == 0 )
    {
      cerr << "Error: authorize-assignments requires course start date!\nPlease see usage with --help.\n";
      return false;
    }
    makeAssignments = true;
  }
  
  if ( vm.count("course-start") > 0)
  {
    context.courseStartDate = vm["course-start"].as<std::string>();
  }
  
  if ( vm.count("course-end") > 0 )
  {
    context.courseEndDate = vm["course-end"].as<std::string>();
  }
  
  XAPI::StatementFactory::course_id   = context.courseurl;
  XAPI::StatementFactory::course_name = context.coursename;
  XAPI::StatementFactory::course_start_date = context.courseStartDate;
  XAPI::StatementFactory::course_end_date = context.courseEndDate;
  return true;
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::MoodleParser::ParseJSONEventLog()
{
  json tmp;
  ifstream activitylog(data.c_str());
  if ( !activitylog.is_open())
  {
    stringstream ss;
    ss << "Cannot open file '" << data << "'\n";
    throw xapi_parsing_error( ss.str());
  }
  activitylog >> tmp;
  activitylog.close();
  json activities;
  
  try
  {
    // Prepare for differences between moodle versions
    if ( tmp.is_array() && tmp.size() == 1)
    {
      activities = tmp[0];
    }
    else
    {
      activities = tmp;
    }
  }
  catch ( std::exception & ex )
  {
    stringstream ss;
    ss << ex.what() << "\n"
         << "Something is wrong with your JSON log file, it should be an array containing a single array of objects.\n";
    throw runtime_error( ss.str());
  }

  Progress progress(0,activities.size());
  // for each log entry
  int entries_without_result = 0;
  cerr << "\n";
  #pragma omp parallel for
  for(size_t i=0;i<activities.size(); ++i)
  {
    if ( omp_get_thread_num() == 0 )
    {
      stringstream ss;
      ss << "Caching user data [" << std::string(progress) << "%]...";
      UpdateThrobber(ss.str()); 
    }
    
    try {
      std::vector<string> lineasvec = activities[i];
      XAPI::StatementFactory::CacheUser(lineasvec);
    } catch ( regex_error & ex )
    {
      cerr << ex.what() << "\n";
    }
#pragma omp critical
    progress++;
  }
  
  stringstream ss;
  ss << "Caching user data [" << std::string(progress) << "%]...";
  UpdateThrobber(ss.str()); 
  
  cerr << "\n";
  progress.ResetCurrent();
  
  #pragma omp parallel for
  for(size_t i=0;i<activities.size(); ++i)
  {

    if ( omp_get_thread_num() == 0 )
		{
      stringstream ss;
      ss << "Processing JSON event log [" << std::string(progress) << "%]...";
      UpdateThrobber(ss.str());
		}
    // each log column is an array element
    std::vector<string> lineasvec = activities[i];
    try
    {
      // use overwritten version of Parse
      string tmp = XAPI::StatementFactory::CreateActivity(lineasvec);
      #pragma omp critical
      statements.push_back(tmp);
    }
    catch ( xapi_no_result_error & ex )
    {
      #pragma omp critical
      entries_without_result++;
    }
    catch ( xapi_cached_user_not_found_error & ex )
    {
      #pragma omp critical
      errorMessages[ex.what()]++;
    }
    catch ( xapi_cached_task_not_found_error & ex )
    {
      #pragma omp critical
      errorMessages[ex.what()]++;
    }
    catch ( xapi_verb_not_supported_error & ex )
    {
      #pragma omp critical
      errorMessages[ex.what()]++;
    }
    catch ( xapi_activity_ignored_error & ex )
    {
      #pragma omp critical
      errorMessages[ex.what()]++;
    }
    catch ( xapi_activity_type_not_supported_error & ex )
    {
      #pragma omp critical
      errorMessages[ex.what()]++;
    }
    catch (xapi_parsing_error & ex )
    {
      #pragma omp critical
      errorMessages[ex.what()]++;
    }
    /*catch ( std::exception & ex )
    {
      #pragma omp critical
      errorMessages[ex.what()]++;
    }*/

#pragma omp critical
    progress++;
    
    //catch ( std::exception & ex )
    //{
    // errorMessages[ex.what()]++;
      //cerr << ex.what() << "\n";
    //}
    // vector now contains strings from one row, output to cout here
    //cout << "\n----------------------" << endl;
  }
  ss.str("");
  ss << "Processing JSON event log [" << std::string(progress) << "%]...";
  UpdateThrobber(ss.str());
  
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::MoodleParser::ParseGradeLog()
{
  ////////////////////////////////////////////////////////////////////////////////
  // Read grading history. 
  json tmp;
  ifstream gradinglog(gradeData.c_str());
  if ( !gradinglog.is_open())
  {
    stringstream ss;
    ss << "Cannot open file '" << gradeData << "'\n";
    throw xapi_parsing_error( ss.str());
  }
  gradinglog >> tmp;
  // This is a bit hack-ish, reading as json,
  // which returns it as array that contains one array containing log data.
  json grading;
  if ( tmp.is_array() && tmp.size() == 1 )
  {
    grading = tmp[0];
  }
  else
  {
    grading = tmp;
  }
  // for each log entry
  int entries_without_result = 0;
  Progress progress(0,grading.size());
  cerr << "\n";

  for(auto it = grading.begin(); it != grading.end(); ++it)
  {
    UpdateThrobber("Processing grade log ["+std::string(progress++)+"%]...");
    // each log column is an array element
    std::vector<string> lineasvec = *it;
    try
    {
      // use overwritten version of Parse
      statements.push_back(XAPI::StatementFactory::CreateGradeEntry(lineasvec));
    }
    catch ( xapi_no_result_error & ex )
    {
      entries_without_result++;
    }
    catch ( xapi_cached_user_not_found_error & ex )
    {
      errorMessages[ex.what()]++;
      //cerr << ex.what() << "\n";
    }
    catch ( xapi_cached_task_not_found_error & ex )
    {
      errorMessages[ex.what()]++;
      //cerr << ex.what() << "\n";
    }
    catch ( std::exception & ex )
    {
      errorMessages[ex.what()]++;
      //cerr << ex.what() << "\n";
    }
    // vector now contains strings from one row, output to cout here
    //cout << "\n----------------------" << endl;
  }
  gradinglog.close();
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::MoodleParser::ParseUsers()
{
  json users;
  ifstream usersFile(userData.c_str());
  if ( !usersFile.is_open())
  {
    stringstream ss;
    ss << "Cannot open file '" << userData << "'\n";
    throw xapi_parsing_error( ss.str());
  }
  usersFile >> users;
  Progress progress(0,users.size());
  cerr << "\n";

  for(auto it = users.begin(); it != users.end(); ++it)
  {
    UpdateThrobber("Parsing users ["+std::string(progress++)+"%]...");

    json user = *it;
    string name = user["name"];
    string userid = user["id"];
    string email = user["email"];
    vector<string> roles = user["roles"];
    XAPI::StatementFactory::CacheUser(name, userid, email, roles);
  }
  usersFile.close();
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::MoodleParser::ParseGroups()
{
  json groups;
  ifstream groupsFile(groupData.c_str());
  if ( !groupsFile.is_open())
  {
    stringstream ss;
    ss << "Cannot open file '" << groupData << "'\n";
    throw xapi_parsing_error( ss.str());
  }
  groupsFile >> groups;
  Progress progress(0,groups.size());
  cerr << "\n";

  for(auto it = groups.begin(); it != groups.end(); ++it)
  {
    UpdateThrobber("Parsing groups ["+std::string(progress++)+"%]...");

    json group = *it;
    string name = group["name"];
    string id = group["id"];
    XAPI::StatementFactory::CacheGroup( id, name);
  }
  groupsFile.close();
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::MoodleParser::CreateAssignments()
{
  for( auto & it : TaskNameToTaskID )
  {
    statements.push_back(XAPI::StatementFactory::CreateAssignmentInitEntry( it.first, it.second ));
  }
}
////////////////////////////////////////////////////////////////////////////////
bool
XAPI::MoodleParser::HasGradeData() const
{
  return !gradeData.empty();
}
////////////////////////////////////////////////////////////////////////////////
bool
XAPI::MoodleParser::HasLogData() const
{
  return !data.empty();
}
////////////////////////////////////////////////////////////////////////////////
bool
XAPI::MoodleParser::HasUserData() const
{
  return !userData.empty();
}
bool
XAPI::MoodleParser::HasGroupData() const
{
  return !groupData.empty();
}
////////////////////////////////////////////////////////////////////////////////
bool
XAPI::MoodleParser::ShouldMakeAssignments() const
{
  return makeAssignments;
}
////////////////////////////////////////////////////////////////////////////////
int main( int argc, char **argv)
{
    using namespace std;

    XAPI::MoodleParser app;
    if ( app.ParseArguments(argc, argv) == false )
    {
      return 1;
    }

    if ( app.ShouldLoad() == false )
    {
      cout << "course url: \"" << XAPI::StatementFactory::course_id << "\"\n";
      cout << "course name: \"" << XAPI::StatementFactory::course_name << "\"\n";
    }
    
    try
    {
      cout << "Loading config from " << app.configFile << "...\n";
      // parse all config
      ifstream configDetails(app.configFile);
      json config;
      configDetails >> config;
      
      app.login.key = config["login"]["key"];
      app.login.secret = config["login"]["secret"];
      // update URL prefices
      app.lmsBaseURL = config["lms"]["baseURL"];
    }
    catch (std::exception & ex )
    {
      cerr << ex.what() << "\n";
      cerr << "Please check your config file, it has errors.\n";
     
      return 1;
    }

    for(auto & a : activityTypes )
    {
      activityTypes[a.first] = app.lmsBaseURL + a.second;
    }
    
    app.UpdateThrobber();
    if ( app.HasUserData())   app.ParseUsers();
    if ( app.HasGroupData())   app.ParseGroups();
    if ( app.HasLogData())    app.ParseJSONEventLog();
    if ( app.HasGradeData())  app.ParseGradeLog();
    if ( app.ShouldMakeAssignments()) app.CreateAssignments();
    if ( app.ShouldLoad())
    {
      app.LoadBatches();
    }
    else
    {
      app.ComputeBatchSizes();
      app.CreateBatches();
      
      if ( app.ShouldWrite())
      {
        app.WriteBatchFiles();
      }
    }
    
    if ( app.ShouldPrint()) app.PrintBatches();
    int retval = 0;
    if ( app.IsDryRun())
      cout << "alright, dry run - not sending statements.\n";
    else
    {
      try
      {
	app.SendBatches();
      }
      catch ( xapi_max_send_attempts_reached_error & ex )
      {
	retval = 1;
      }
    }

    app.LogErrors();
    app.LogStats();

    return retval;
}
////////////////////////////////////////////////////////////////////////////////
