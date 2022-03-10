/*
  This file is part of xapi_converter.  Parses memo logs and constructs 
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
#include <xapi_converter_devops.h>
#include <xapi_errors.h>
#include <xapi_commit.h>
#include <xapi_wikiupdate.h>
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
XAPI::AzureDevOps::AzureDevOps() : XAPI::Application("Command-line tool for sending XAPI statements to learning locker from Azure devops logs\nReleased under GPLv3 - use at your own risk. \n\nPrerequisities:\n\tLearning locker client credentials must be in json format in data/config.json.\n\tSimple object { \"login\": { \"key\": \"\", \"secret\":\"\"}, \"lms\" : { \"baseURL\" : \"\" }\n\nUsage")
{
  ////////////////////////////////////////////////////////////////////////////////
  // For some hints on usage...
  desc.add_options()
    ("courseurl", po::value<string>(), "<course_url> Unique course moodle web address, ends in ?id=xxxx")
    ("coursename", po::value<string>(), "<course_name> Human-readable name for the course")
    ("commits", po::value<string>(), "<commit.json> Commit log for specific project.")
    ("wikiupdates", po::value<string>(), "<wikiupdates.json> Commit log for project wikis.");

}
////////////////////////////////////////////////////////////////////////////////
XAPI::AzureDevOps::~AzureDevOps()
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
XAPI::AzureDevOps::ParseCustomArguments()
{
  if ( vm.count("commits") > 0 && vm.count("load") > 0)
  {
    cerr << "Error: cannot use --commits and --load at the same time!\n";
    return false;
  }
  else if ( vm.count("load") == 0 && vm.count("commits") == 0)
  {
    cerr << "Error: commit data is required!\n";
    return false;
  }
  else if ( vm.count("commits") > 0 )
  {
    commitData = vm["commits"].as<string>();
  }

  // wiki updates
  if ( vm.count("wikiupdates") > 0 && vm.count("load") > 0)
  {
    cerr << "Error: cannot use --wikiupdates and --load at the same time!\n";
    return false;
  }
  else if ( vm.count("load") == 0 && vm.count("wikiupdates") == 0)
  {
    cerr << "Error: wiki update data is required!\n";
    return false;
  }
  else if ( vm.count("wikiupdates") > 0 )
  {
    wikiData = vm["wikiupdates"].as<string>();
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
    courseUrl = vm["courseurl"].as<string>();
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
    courseName = vm["coursename"].as<string>();
  }
  
  return true;
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::AzureDevOps::ParseCommits( bool wiki)
{
  ifstream commitFile(wiki ? wikiData.c_str() : commitData.c_str() );
  if ( !commitFile.is_open())
  {
    stringstream ss;
    ss << "Cannot open file '" << commitData << "'\n";
    throw xapi_parsing_error( ss.str());
  }

  json data;
  commitFile >> data;
  std::vector<json> entries = data["value"].get<std::vector<json>>();
  Progress progress(0,entries.size());
#pragma omp parallel for
  for(size_t i=0; i < entries.size(); ++i)
  {
    if ( omp_get_thread_num() == 0 )
    {
      stringstream ss;
      ss << "Processing memo data [" << std::string(progress) << "%]...";
      UpdateThrobber(ss.str()); 
    }
    
    try
    {
      if ( wiki )
      {
	XAPI::WikiUpdateEntry entry;
	entry.Parse(entries[i]);
	entry.course_name = courseName;
	entry.course_id = courseUrl;
	string tmp = entry.ToXapiStatement();
        #pragma omp critical
	statements.push_back(tmp);

      }
      else
      {
	XAPI::CommitEntry entry;
	entry.Parse(entries[i]);
	entry.course_name = courseName;
	entry.course_id = courseUrl;
	string tmp = entry.ToXapiStatement();
        #pragma omp critical
	statements.push_back(tmp);
      }
    }
    catch ( std::runtime_error & ex )
    {
      #pragma omp critical
      errorMessages[ex.what()]++;
    }
    #pragma omp critical
    progress++;
  }

  stringstream ss;
  ss << "Processing memo data [" << std::string(progress) << "%]...";
  UpdateThrobber(ss.str()); 
}
////////////////////////////////////////////////////////////////////////////////
bool
XAPI::AzureDevOps::HasCommitData() const
{
  return !commitData.empty();
}
////////////////////////////////////////////////////////////////////////////////
bool
XAPI::AzureDevOps::HasWikiData() const
{
  return !wikiData.empty();
}
////////////////////////////////////////////////////////////////////////////////
int main( int argc, char **argv)
{
    using namespace std;

    XAPI::AzureDevOps app;
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

    if ( app.HasCommitData()) app.ParseCommits();
    if ( app.HasWikiData())   app.ParseCommits(true);
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
