/*
  This file is part of xapi_converter.  Parses memo logs and constructs 
  xAPI statements out of them, sending them to LRS.

  Copyright (C) 2018-2021 Anssi Gr�hn
  
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
#include <xapi_converter_memo.h>
#include <xapi_errors.h>
#include <xapi_memo.h>
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
#include <regex>
#include <sstream>
using json = nlohmann::json;
using namespace std;
//https://nithinkk.wordpress.com/2017/03/16/learning-locker/
////////////////////////////////////////////////////////////////////////////////
namespace po = boost::program_options;
extern std::map<std::string,int> errorMessages;
////////////////////////////////////////////////////////////////////////////////
std::map<std::string, std::string> EmailToUserID = {};
std::map<std::string, std::string> UserIDToUserName = {};
std::map<std::string, std::string> UserIDToEmail = {};

extern XAPI::Anonymizer anonymizer;
#define DEFAULT_BATCH_FILENAME_PREFIX "batch_"
#define DEFAULT_CONFIG_FILENAME "config.json"
////////////////////////////////////////////////////////////////////////////////
struct MemoUserEntry
{
  std::string user;
  std::string email;
  std::string role;
  std::string attendance;
  std::string devblogentry;
  std::string date;
  std::string weeknum;
  std::string activity; 
  std::string dedication;
};
////////////////////////////////////////////////////////////////////////////////
inline  std::ostream & operator<<(std::ostream & s, MemoUserEntry & m)
  {
    s << "user: " << m.user << "\n";
    s << "email: " << m.email << "\n";
    s << "role: " << m.role << "\n";
    s << "attendance: " << m.attendance << "\n";
    s << "devblogentry: " << m.devblogentry << "\n";
    return s;
  }
////////////////////////////////////////////////////////////////////////////////
XAPI::Memo::Memo() : XAPI::Application("Command-line tool for sending XAPI statements to learning locker from memo logs\nReleased under GPLv3 - use at your own risk. \n\nPrerequisities:\n\tLearning locker client credentials must be in json format in data/config.json.\n\tSimple object { \"login\": { \"key\": \"\", \"secret\":\"\"}, \"lms\" : { \"baseURL\" : \"\" }\n\nUsage")
{
  ////////////////////////////////////////////////////////////////////////////////
  // For some hints on usage...
  desc.add_options()
    ("courseurl", po::value<string>(), "<course_url> Unique course moodle web address, ends in ?id=xxxx")
    ("coursename", po::value<string>(), "<course_name> Human-readable name for the course")
    
    ("memo", po::value<string>(), "<SESSION MEMO DATA.txt> Memo txt file with appropriate format. Must have identifiers in place.");

}
////////////////////////////////////////////////////////////////////////////////
XAPI::Memo::~Memo()
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
void XAPI::Memo::VerifySessionEntryHeaders( ifstream & file )
{
  string line;
  getline(file,line);
  if ( line.rfind("Ohjaajat: ") != 0 )
  {
    stringstream ss;
    ss << "Session entry faulty: line ohjaajat not found. " << line;
    throw xapi_parsing_error(ss.str());
  }
  
  // headers as they shold be 

  for ( auto& header : memo.memberHeaders )
  {
    getline(file,line);
    if ( line != header )
    {
      
      stringstream ss;
      ss << "Session entry faulty: " << header << " not found.'" << line << "'"; 
      throw xapi_parsing_error(ss.str());
    }
    
  }
}
////////////////////////////////////////////////////////////////////////////////
static void CacheUser( const std::string username, const std::string & email)
{
  

  if ( UserIDToEmail.find(username) == UserIDToEmail.end() &&
       EmailToUserID.find(email) == EmailToUserID.end() )
  {
    UserIDToEmail[username] = email;
    EmailToUserID[email] = anonymizer(username);
    UserIDToUserName[username] = anonymizer(username);
  }
}
////////////////////////////////////////////////////////////////////////////////
static std::vector<MemoUserEntry> ParseMemoUserEntries( ifstream & file )
{
  vector<MemoUserEntry> result;
  string line;
  while ( file.eof() == false )
  {
    // assuming fivesequential lines to form an entry.
    // First empty line at name is interpreted as
    // end of user entries.

    // name
    getline(file, line);
    line = trim(line);
    if (line.size() == 0 )
    {
      cerr << "end of user entries\n";
      break;
    }
    
    MemoUserEntry e;
    line = trim(line);
    e.user = line;

    // email
    getline(file,line);
    line = trim(line);
    transform(line.begin(), line.end(), line.begin(), ::tolower);
    e.email = line;

    // role
    getline(file,line);
    line = trim(line);
    transform(line.begin(), line.end(), line.begin(), ::tolower);
    e.role = line;
    // attendance
    getline(file,line);
    line = trim(line);
    transform(line.begin(), line.end(), line.begin(), ::tolower);
    e.attendance = line;
    // devblog
    getline(file,line);
    line = trim(line);
    transform(line.begin(), line.end(), line.begin(), ::tolower);
    e.devblogentry = line;

    CacheUser( e.user, e.email);
    
    result.push_back(e);
  }
   
  return result;
}
////////////////////////////////////////////////////////////////////////////////
static int DetermineProcessedUser( const std::string & user, const std::vector<MemoUserEntry> & entries )
{
  // determine which user we are dealing with
  int userindex = -1;
  string tmpline = user;
  transform(tmpline.begin(), tmpline.end(), tmpline.begin(), ::tolower);
  for ( size_t i=0;i<entries.size();i++)
  {
    string tmpname = entries[i].user;
    transform(tmpname.begin(), tmpname.end(), tmpname.begin(), ::tolower);
    // name should be right in the beginning
    if ( tmpline.rfind(tmpname) == 0 )
    {
      userindex = i;
      break;
    };
  }
  return userindex;
}
////////////////////////////////////////////////////////////////////////////////
bool
XAPI::Memo::ParseCustomArguments()
{
  if ( vm.count("memo") > 0 && vm.count("load") > 0)
  {
    cerr << "Error: cannot use --memo and --load at the same time!\n";
    return false;
  }
  else if ( vm.count("load") == 0 && vm.count("memo") == 0)
  {
    cerr << "Error: memo data is required!\n";
    return false;
  }
  else if ( vm.count("memo") > 0 )
  {
    memoData = vm["memo"].as<string>();
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
XAPI::Memo::ParseMemo()
{
  ifstream memoFile(memoData.c_str());
  if ( !memoFile.is_open())
  {
    stringstream ss;
    ss << "Cannot open file '" << memoData << "'\n";
    throw xapi_parsing_error( ss.str());
  }

  string line;
  getline(memoFile, line);
  transform( line.begin(), line.end(), line.begin(), ::tolower);
  smatch groupMatch;
  if ( regex_search(line, groupMatch, regex("ohjaustapaamiset ryhm.* ([[:alnum:]]+)")) == false || memoFile.eof() )
  {
    stringstream ss;
    ss << "File '" << memoData << "' is not in proper memo file format! ";
    throw xapi_parsing_error(ss.str());
  }
  // get group identifier
  string groupId = groupMatch[1];
  
  std::vector<MemoUserEntry> allEntries;
  while( memoFile.eof() == false)
  {
    // read lines until we get proper marker for headers
    smatch match;
    do
    {
      getline(memoFile,line);
    } while ( regex_search(line, match, regex("[[:space:]]*Viikko [(]*([[:digit:]]+)[)](.*): ([[:digit:]]+\\.[[:digit:]]+\\.[[:digit:]]{4})")) == false &&
	      memoFile.eof() == false);
    
    if ( memoFile.eof() )
    {
      cerr << "Reached end-of-file\n";
      break;
    }
    
    // get meeting information
    std::string weeknumber = match[1];
    std::string date = match[3];
    cerr << "processing week " << weeknumber << " and date " << date << " from " << match[0] << "\n";
    // read empty line 
    getline(memoFile, line);
    bool found = false;
    while( found == false && memoFile.eof() == false)
    {
      // verify headers for user section
      try {
	VerifySessionEntryHeaders(memoFile);
	found = true;
      }
      catch ( xapi_parsing_error & err )
      {
	cerr << err.what() << "\n";
      }
    }
    // we are finished, time to go
    if ( found == false ) break;
    
    vector<MemoUserEntry> users = ParseMemoUserEntries(memoFile);
    cout << "we found " << users.size() << " users this time.\n";
    // assign date to records
    for ( auto & u : users )
    {
      u.date = date;
      u.weeknum = weeknumber;
    }
    
    // parse next section identifier
    bool foundDetailSectionIdentifier = false;
    do
    {
      getline(memoFile,line);
      line = trim(line);
      foundDetailSectionIdentifier = line.rfind(memo.detailSectionIdentifier) == 0;
    } while( foundDetailSectionIdentifier == false && memoFile.eof() == false);
    
    if ( foundDetailSectionIdentifier == false )  throw xapi_parsing_error("Could not find detail section identifier");
    
    int currentUser = -1;
    bool sessionActive = true;
    do
    {
      getline(memoFile, line);
      smatch match;

      if ( line.rfind("Ohjaajien asiat") == 0)
      {
	cerr << "section skip\n";
	sessionActive = false;
      }
      else if ( regex_search(line, match, regex("[[:space:]]*(.+)")))
      {

	int userIdentifier = DetermineProcessedUser(match[1], users);
	if ( userIdentifier != -1 )
	{
	    currentUser = userIdentifier;
	    cerr << "found user line " << line << ", it matches " << currentUser << "\n";
	    continue;
	}
	// if we have previously identified user, we check its keywords
	if (currentUser != -1 )
	{
	  //cerr << "reading trigger words for user " << currentUser << "\n";
	  // seek trigger words and map to current user-
	  string lowercaseLine = line;
	  // convert to lower case so keyword matches work
	  // (except if there are scandic characters)
	  transform(lowercaseLine.begin(), lowercaseLine.end(),
		    lowercaseLine.begin(), ::tolower);
	  
	  cerr << "processing trigger words for line: " << lowercaseLine << ":\n";
	  for( auto & keyword : memo.activityTriggerWords )
	  {
	    cerr <<" attempting to find '" << keyword << "'...";
	    if (lowercaseLine.rfind(keyword) != string::npos )
	    {
	      users[currentUser].activity = keyword;
	      cerr << "set\n";
	    }
	    else
	    {
	      cerr << "not\n";
	    }
	  }
	  for( auto & keyword : memo.dedicationTriggerWords )
	  {
	    cerr <<" attempting to find '" << keyword << "'...";
	    if (lowercaseLine.rfind(keyword) != string::npos )
	    {
	      users[currentUser].dedication = keyword;
	      cerr << "found\n";
	    }
	    else
	    {
	      cerr << "not\n";
	    }
	  }
	}
      }
      // if begins with [[:space:]]*\*
    } while( sessionActive && memoFile.eof() == false);

    // copy current session data into all entries vector
    copy(users.begin(), users.end(), back_inserter(allEntries));
  } // while( memoFile.eof() == false)
  
    Progress progress(0,allEntries.size());
    memoFile.close();
    
#pragma omp parallel for
  for(size_t i=0; i < allEntries.size(); ++i)
  {
    if ( omp_get_thread_num() == 0 )
    {
      stringstream ss;
      ss << "Processing memo data [" << std::string(progress) << "%]...";
      UpdateThrobber(ss.str()); 
    }
    //cerr << allEntries[i] << "\n";
    
    
      // conjure up a date
      std::vector<string> dateparts;
      
      boost::split( dateparts, allEntries[i].date, boost::is_any_of("."));
      if ( dateparts.size() != 3 )
      {
	throw runtime_error("invalid date string");
      }
      string memoDate = dateparts[2]+"-"+dateparts[1]+"-"+dateparts[0];
      
      // event is used to identify meetting
      string meetingIdentifier;
      {
	stringstream ss;
	ss << "https://www.karelia.fi/tiko/ict-toimeksiantoprojekti/group/";
	ss << groupId << "/";
	ss << memoDate;
	meetingIdentifier = ss.str();
      }

      try
      {
	XAPI::MemoEntry entry;
	entry.ParseTimestamp(memoDate);
	/* 
	   "X attended meeting, context course, purpose role, trigger words as tags" 
	   "X skipped meeting, context course, severity (known|unknown)"
	   "X updated blog"
	   
	   recycling originally moodle-related fields below,
	*/
	
	entry.userid = allEntries[i].user;
	entry.context = trim(allEntries[i].devblogentry);
	entry.component = trim(allEntries[i].attendance);

	// role is optional
	if ( allEntries[i].role.size() > 0 )
	{
	  entry.description = trim(allEntries[i].role);
	}
	entry.event = meetingIdentifier;
	entry.course_name = courseName;
	entry.course_id = courseUrl;
	entry.dedication = allEntries[i].dedication;
	entry.activity = allEntries[i].activity;
	
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

  stringstream ss;
  ss << "Processing memo data [" << std::string(progress) << "%]...";
  UpdateThrobber(ss.str()); 
}
////////////////////////////////////////////////////////////////////////////////
bool
XAPI::Memo::HasMemoData() const
{
  return !memoData.empty();
}
////////////////////////////////////////////////////////////////////////////////
int main( int argc, char **argv)
{
    using namespace std;

    XAPI::Memo app;
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
      app.memo.fileIdentifier = config["memo"]["identifier"];
      app.memo.detailSectionIdentifier = config["memo"]["detailsection"];
      app.memo.memberHeaders = config["memo"]["headers"].get<vector<string>>();
      app.memo.activityTriggerWords = config["memo"]["activityTriggerWords"].get<vector<string>>();
      app.memo.dedicationTriggerWords = config["memo"]["dedicationTriggerWords"].get<vector<string>>();
      cout << "done.\n";
    }
    catch (std::exception & ex )
    {
      cerr << ex.what() << "\n";
      cerr << "Please check your config file, it has errors.\n";
     
      return 1;
    }
    
    app.UpdateThrobber();

    if ( app.HasMemoData()) app.ParseMemo();
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
