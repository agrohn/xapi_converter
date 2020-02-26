/*
  This file is part of xapi_converter.  Parses moodle logs and constructs 
  xAPI statements out of them, sending them to LRS.

  Copyright (C) 2018-2019 Anssi Gröhn
  
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
#include <xapi_converter_vipuvarsi.h>
#include <xapi_errors.h>
#include <xapi_credits.h>
#include <xapi_anonymizer.h>
#include <omp.h>
#include <string>
#include <cctype>
#include <json.hpp>
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
////////////////////////////////////////////////////////////////////////////////
std::map<std::string, std::string> TaskNameToTaskID = {};
std::map<std::string, std::string> UserNameToUserID = {};
std::map<std::string, std::string> UserIDToUserName = {};
std::map<std::string, std::string> UserIDToEmail = {};
std::map<std::string, std::vector<std::string>> UserIDToRoles = {};
//extern XAPI::Anonymizer anonymizer;
#define DEFAULT_BATCH_FILENAME_PREFIX "batch_"
#define DEFAULT_CONFIG_FILENAME "config.json"
////////////////////////////////////////////////////////////////////////////////
XAPI::Vipuvarsi::Vipuvarsi() : XAPI::Application("Command-line tool for sending XAPI statements to learning locker from vipunen logs\nReleased under GPLv3 - use at your own risk. \n\nPrerequisities:\n\tLearning locker client credentials must be in json format in data/config.json.\n\tSimple object { \"login\": { \"key\": \"\", \"secret\":\"\"}, \"lms\" : { \"baseURL\" : \"\" }\n\nUsage")
{
  ////////////////////////////////////////////////////////////////////////////////
  // For some hints on usage...
  desc.add_options()
  ("credits", po::value<string>(), "<YOUR CREDITS DATA.txt> Credits history data obtained from Vipunen")
  ("users", po::value<string>(), "<YOUR-USERS-DATA.json>  User (participant) data obtained from moodle");

}
////////////////////////////////////////////////////////////////////////////////
XAPI::Vipuvarsi::~Vipuvarsi()
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
bool
XAPI::Vipuvarsi::ParseCustomArguments()
{
  if ( vm.count("credits") )
  {
    creditsData = vm["credits"].as<string>();
  }
  else
  {
    cerr << "Error: credits data is required!\n";
    return false;
  }
  return true;
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::Vipuvarsi::ParseCredits()
{
  ifstream creditsFile(creditsData.c_str());
  if ( !creditsFile.is_open())
  {
    stringstream ss;
    ss << "Cannot open file '" << creditsData << "'\n";
    throw xapi_parsing_error( ss.str());
  }
  vector<size_t> fieldLengths;
  string line;
  getline(creditsFile,line); // titles
  getline(creditsFile,line); // dashes
  
  
  while ( creditsFile.good() )
  {
    // Opiskelijanumero,OpintojaksoID,ToteutusID,ToteutusNimi,Laajuus,Arvosana,Tila,ArviointiPvm,HyvaksilukuPvm
    getline(creditsFile,line);
    stringstream ss;
    ss << line;
    std::vector<string> fields;

    string studentId;
    string courseId; 
    string realizationId;
    string realizationName;
    string credits;
    string grade;
    string state;
    string gradingDate;
    string ahotDate;

    getline(ss, studentId, '|');
    getline(ss, courseId, '|');
    getline(ss, realizationId, '|');
    getline(ss, realizationName, '|');
    getline(ss, credits, '|');
    getline(ss, grade, '|');
    getline(ss, state, '|');
    getline(ss, gradingDate, '|');
    getline(ss, ahotDate, '|');
    trim(studentId);
    trim(courseId);
    trim(realizationId);
    trim(realizationName);
    trim(credits);
    trim(grade);
    trim(state);
    trim(gradingDate);
    trim(ahotDate);

    fields.push_back(studentId);
    fields.push_back(courseId);
    fields.push_back(realizationId);
    fields.push_back(realizationName);
    fields.push_back(credits);
    fields.push_back(grade);
    fields.push_back(state);
    fields.push_back(gradingDate);
    fields.push_back(ahotDate);
    /*for( auto & t : fields )
    {
      cerr << t << "\n";
      }*/
    XAPI::CreditsEntry entry;
    entry.Parse(fields);
    statements.push_back(entry.ToXapiStatement());
    
    //break;
  }
  

  

  
}
////////////////////////////////////////////////////////////////////////////////
bool
XAPI::Vipuvarsi::HasCreditsData() const
{
  return !creditsData.empty();
}
////////////////////////////////////////////////////////////////////////////////
int main( int argc, char **argv)
{
    using namespace std;

    XAPI::Vipuvarsi app;
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
      cout << "done.";
    }
    catch (std::exception & ex )
    {
      cerr << ex.what() << "\n";
      cerr << "Please check your config file, it has errors.\n";
     
      return 1;
    }

    /*for(auto & a : activityTypes )
    {
    activityTypes[a.first] = app.lmsBaseURL + a.second;
    }*/
    
    app.UpdateThrobber();
    if ( app.HasCreditsData()) app.ParseCredits();
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
    
    if ( app.IsDryRun())
      cout << "alright, dry run - not sending statements.\n";
    else
      app.SendBatches();

    app.LogErrors();
    app.LogStats();

    return 0;
}
////////////////////////////////////////////////////////////////////////////////
