/*
  This file is part of xapi_converter.  Parses attendance logs and constructs 
  xAPI statements out of them, sending them to LRS.

  Copyright (C) 2018-2021 Anssi Gröhn
  
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
#include <xapi_converter_attendance.h>
#include <xapi_errors.h>
#include <xapi_attend.h>
#include <xapi_anonymizer.h>
#include <omp.h>
#include <string>
#include <cctype>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include <curlpp/Infos.hpp>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <sys/stat.h>
#include <stack>
using json = nlohmann::json;
using namespace std;
//https://nithinkk.wordpress.com/2017/03/16/learning-locker/
////////////////////////////////////////////////////////////////////////////////
namespace po = boost::program_options;
extern std::map<std::string,int> errorMessages;
////////////////////////////////////////////////////////////////////////////////
std::map<std::string, std::string> UserNameToUserID = {};
std::map<std::string, std::string> UserIDToUserName = {};
std::map<std::string, std::string> UserIDToEmail = {};
std::map<std::string, std::vector<std::string>> UserIDToRoles = {};
extern XAPI::Anonymizer anonymizer;
#define DEFAULT_BATCH_FILENAME_PREFIX "batch_"
#define DEFAULT_CONFIG_FILENAME "config.json"
////////////////////////////////////////////////////////////////////////////////
XAPI::Attendance::Attendance() : XAPI::Application("Command-line tool for sending XAPI statements to learning locker from attendance logs\nReleased under GPLv3 - use at your own risk. \n\nPrerequisities:\n\tLearning locker client credentials must be in json format in data/config.json.\n\tSimple object { \"login\": { \"key\": \"\", \"secret\":\"\"}, \"lms\" : { \"baseURL\" : \"\" }\n\nUsage")
{
  ////////////////////////////////////////////////////////////////////////////////
  // For some hints on usage...
  desc.add_options()
  ("attendances", po::value<string>(), "<USER ATTENDANCE DATA.csv> Attendance log for each students and class.");

}
////////////////////////////////////////////////////////////////////////////////
XAPI::Attendance::~Attendance()
{
  
}
//https://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
static inline std::string & trim(std::string & s)
{
  s.erase(s.begin(), std::find_if(s.begin(),s.end(), [](int ch){ return !std::isspace(ch);}));
  s.erase(std::find_if(s.rbegin(),s.rend(), [](int ch){ return !std::isspace(ch);}).base(), s.end());
  return s;
}
////////////////////////////////////////////////////////////////////////////////
static std::vector<string> ParseCSVLine( const std::string & line )
{
  stack<bool> quotes;

  
  stringstream tmp;
  tmp << line;
  vector<string> result;

  stringstream partial;
  string part;
  
  while(getline(tmp, part, ','))
  {
    partial << part;
    if ( quotes.empty() && part[0] == '"' )     quotes.push(true);
    if ( quotes.empty() == false && (*part.rbegin()) == '"' )      quotes.pop();
    
    
    if ( quotes.empty() )
    {
      result.push_back(partial.str());
      partial.str("");
      partial.clear();
    }
    else
    {
      // insert delimeter that was discarded previously
      partial << ",";
    }

  }


  return  result;
}
////////////////////////////////////////////////////////////////////////////////
bool
XAPI::Attendance::ParseCustomArguments()
{
  if ( vm.count("attendances") > 0 && vm.count("load") > 0)
  {
    cerr << "Error: cannot use --attendances and --load at the same time!\n";
    return false;
  }
  else if ( vm.count("load") == 0 && vm.count("attendances") == 0)
  {
    cerr << "Error: attendance data is required!\n";
    return false;
  }
  else if ( vm.count("attendances") > 0 )
  {
    attendanceData = vm["attendances"].as<string>();
  }
  
  
  
  return true;
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::Attendance::ParseAttendance()
{
  ifstream attendanceFile(attendanceData.c_str());
  if ( !attendanceFile.is_open())
  {
    stringstream ss;
    ss << "Cannot open file '" << attendanceData << "'\n";
    throw xapi_parsing_error( ss.str());
  }

  string line;
  // read lines until we get proper marker for headers
  do
  {
    getline(attendanceFile,line);
  } while ( line.rfind("Nimi",0) != 0 );
  
  // get header information
  vector<string> headers = ParseCSVLine(line);
  
  
  vector<string> lines;
  while ( attendanceFile.good() )
  {
    getline(attendanceFile,line);
    lines.push_back(line);
  }
  Progress progress(0,lines.size());
  attendanceFile.close();

#pragma omp parallel for
  for(size_t i=0; i < lines.size(); ++i)
  {
    if ( omp_get_thread_num() == 0 )
    {
      stringstream ss;
      ss << "Processing attendance data [" << std::string(progress) << "%]...";
      UpdateThrobber(ss.str()); 
    }


    std::vector<string> attendanceRecord = ParseCSVLine(lines[i]);
    for ( size_t lecture=2; lecture<attendanceRecord.size(); lecture++)
    {


      string studentId = trim(attendanceRecord[0]);
      // trim first and last double quote, if any
      if ( studentId[0] == '"' ) studentId = studentId.substr(1);
      if ( *studentId.rbegin() == '"' ) studentId = studentId.substr(0,studentId.size()-1);
      
      string studentEmail = trim(attendanceRecord[1]);
      string lectureId = trim(headers[lecture]);
      string attendanceType = trim(attendanceRecord[lecture]);
      
      string properLectureDate= lectureId.substr(2);
      // conjure up a date
      std::vector<string> dateparts;
      
      boost::split( dateparts, properLectureDate, boost::is_any_of("."));
      if ( dateparts.size() != 3 )
      {
	throw runtime_error("invalid date string");
      }
      string attendanceDate = dateparts[2]+"-"+dateparts[1]+"-"+dateparts[0];
	
      std::vector<string> fields;
      fields.push_back(studentId);
      fields.push_back(studentEmail);
      fields.push_back(attendanceDate);

      fields.push_back(attendanceType);
      fields.push_back(lectureId);
      try
      {
	XAPI::AttendanceEntry entry;
	entry.Parse(fields);
	  
	string tmp = entry.ToXapiStatement();
        #pragma omp critical
	statements.push_back(tmp);
      }
      catch ( std::runtime_error & ex )
      {
        #pragma omp critical
	errorMessages[ex.what()]++;
      }
      #pragma omp critical
      progress++;
    }
  }
  stringstream ss;
  ss << "Processing attendance data [" << std::string(progress) << "%]...";
  UpdateThrobber(ss.str()); 
}
////////////////////////////////////////////////////////////////////////////////
bool
XAPI::Attendance::HasAttendanceData() const
{
  return !attendanceData.empty();
}
////////////////////////////////////////////////////////////////////////////////
int main( int argc, char **argv)
{
    using namespace std;

    XAPI::Attendance app;
    if ( app.ParseArguments(argc, argv) == false )
    {
      return 1;
    }
    
    try
    {
      cout << "Loading config from " << app.configFile << "... ";
      // parse all config
      ifstream configDetails(app.configFile);
      json config;
      configDetails >> config;
      app.login.key = config["login"]["key"];
      app.login.secret = config["login"]["secret"];
      cout << "done.\n";
    }
    catch (std::exception & ex )
    {
      cerr << ex.what() << "\n";
      cerr << "Please check your config file, it has errors.\n";
     
      return 1;
    }
    
    app.UpdateThrobber();

    if ( app.HasAttendanceData()) app.ParseAttendance();
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
