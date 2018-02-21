/*
 * Parses moodle log from CSV and constructs xAPI statements out of it, sending them to LRS.
 * \author anssi grohn at karelia dot fi (c) 2018.
 */
#include <xapi_converter.h>
#include <xapi_errors.h>
#include <xapi_entry.h>
#include <xapi_grade.h>
#include <cctype>
#include <json.hpp>
using json = nlohmann::json;
//https://nithinkk.wordpress.com/2017/03/16/learning-locker/


////////////////////////////////////////////////////////////////////////////////

const string VERB_URL_PREFIX = "http://adlnet.gov/expapi/verbs/viewed";
std::map<std::string, std::string> TaskNameToTaskID = {};
std::map<std::string, std::string> UserNameToUserID = {};

const std::map<std::string, std::string> supportedVerbs = {
  /*{ "added", ""},
    { "assigned",""},
    { "created",""},
    { "deleted",""},
    { "enrolled",""},
    { "ended", ""}, 
    { "graded", ""},
    { "posted", ""},
    { "searched", ""},
    { "started", ""},*/
    { "submitted", "http://adlnet.gov/expapi/verbs/answered"},
    // { "attempted", "http://adlnet.gov/expapi/verbs/attempted"},
    { "scored", "http://adlnet.gov/expapi/verbs/scored" },
    /*{ "uploaded", ""},
    { "launched", ""},
    { "subscribed", ""},
    { "unassigned", ""},
    { "unenrolled", ""},
    { "updated", ""},*/
    { "viewed","http://id.tincanapi.com/verb/viewed"}
};

// supported activity target types
const std::map<std::string, std::string> activityTypes = {
  { "collaborate", "https://moodle.karelia.fi/mod/collaborate/view.php?id="},
  { "quiz", "https://moodle.karelia.fi/mod/quiz/view.php?id=" },
  { "page", "https://moodle.karelia.fi/mod/page/view.php?id=" },
  { "resource", "https://moodle.karelia.fi/mod/resource/view.php?id="},
  { "url", "https://moodle.karelia.fi/mod/url/view.php?id=" },
  { "forum", "https://moodle.karelia.fi/mod/forum/view.php?id="},
  { "hsuforum", "https://moodle.karelia.fi/mod/hsuforum/view.php?id="},
  { "lti", "https://moodle.karelia.fi/mod/lti/view.php?id=" },
  { "course", "https://moodle.karelia.fi/course/view.php?id="},
  { "assignment", "https://moodle.karelia.fi/mod/assign/view.php?id=" },
  { "quiz", "https://moodle.karelia.fi/mod/quiz/view.php?id=" }
};



int main( int argc, char **argv)
{
    using namespace std;

    string data(argv[1]);
    string gradeData(argv[2]);
    string learningLockerURL;
    
    /// \TODO use boost options. https://coderwall.com/p/y3xnxg/using-getopt-vs-boost-in-c-to-handle-arguments
    // check if url was specififed 
    if ( argc > 3 )
    {
      learningLockerURL = argv[3];
    }
    else
    {
      cout << "alright, dry run - not sending statements.\n";
    }
	 
    vector<string> statements;
    
    ifstream activitylog(data.c_str());
    if (!activitylog.is_open()) return 1;
    
    string line;
    
    // skip first header line 
    getline(activitylog,line);
    std::vector<Entry> unsupported;

    while (getline(activitylog,line))
    {
	Entry e;

	try
	{
	  e.Parse(line);
	  statements.push_back(e.ToXapiStatement());
	}
	catch ( xapi_activity_type_not_supported_error & ex )
	{
	  cerr << ex.what() << "\n";
	  unsupported.push_back(e);
	  //return 1;
	}
	catch ( xapi_parsing_error & ex )
	{
	  cerr << ex.what() << "\n";
	}
	catch ( std::out_of_range & ex )
	{
	  cout << "could not parse time\n";
	}
	// vector now contains strings from one row, output to cout here
        
	
        //cout << "\n----------------------" << endl;
    }
    activitylog.close();

    // Read grading history. 
    json tmp;
    ifstream gradinglog(gradeData.c_str());
    if ( !gradinglog.is_open()) return 1;
    gradinglog >> tmp;
    // This is a bit hack-ish, reading as json,
    // which returns it as array that contains one array containing log data.
    json grading = tmp[0];
    // for each log entry
    int entries_without_result = 0;
    for(auto it = grading.begin(); it != grading.end(); ++it)
    {
      // each log column is an array elemetn
      std::vector<string> lineasvec = *it;
      GradeEntry e;
      
      try
      {
	// use overwritten version of Parse
	e.Parse(lineasvec);
	string s = e.ToXapiStatement();
	statements.push_back(s);
	cerr << s << "\n";
      }
      catch ( xapi_no_result_error & ex )
      {
	entries_without_result++;
      }
      catch ( xapi_cached_user_not_found_error & ex )
      {
	cerr << ex.what() << "\n";
	unsupported.push_back(e);
	  //return 1;
      }
      catch ( xapi_cached_task_not_found_error & ex )
      {
	cerr << ex.what() << "\n";
      }
      catch ( std::exception & ex )
      {
	cerr << ex.what() << "\n";
      }
      // vector now contains strings from one row, output to cout here
      //cout << "\n----------------------" << endl;
    }
    gradinglog.close();
    
    // send XAPI statements in POST
    if ( learningLockerURL.size() > 0 )
    {
      try {
	curlpp::Cleanup cleaner;
	curlpp::Easy request;
	
	ifstream loginDetails("data/login.json");
	json login;
	loginDetails >> login;

      
	string url = learningLockerURL+"/data/xAPI/statements";
	cerr << "learninglockerurl" << url << "\n";
	request.setOpt(new curlpp::options::Url(url)); 
	request.setOpt(new curlpp::options::Verbose(true)); 
	std::list<std::string> header;

      
	//https://stackoverflow.com/questions/25852551/how-to-add-basic-authentication-header-to-webrequest#25852562
	string tmp;
	string key = login["key"];
	string secret = login["secret"];
	{
	  stringstream ss;
	  ss << key << ":" << secret;
	  tmp = ss.str();
	}
	string auth = "Basic " + base64_encode(reinterpret_cast<const unsigned char *>(tmp.c_str()), tmp.size());
	header.push_back("Authorization: " + auth);
      
	header.push_back("X-Experience-API-Version: 1.0.3");
	header.push_back("Content-Type: application/json; charset=utf-8");
	request.setOpt(new curlpp::options::HttpHeader(header)); 

	stringstream ss;
	ss << "[";
	auto last_element = statements.end()-1;
	for_each(statements.begin(), last_element, [&ss] (auto & s) {
	    ss << s << ",";
	  });
	ss << *last_element << "]";
      
	request.setOpt(new curlpp::options::PostFields(ss.str()));
	request.setOpt(new curlpp::options::PostFieldSize(ss.str().size()));

	cerr << ss.str() << "\n";
      
	request.perform();

      }
      catch ( curlpp::LogicError & e ) {
	std::cout << e.what() << std::endl;
      }
      catch ( curlpp::RuntimeError & e ) {
	std::cout << e.what() << std::endl;
      }
    }
    /*
    cout << "unsupported entries:\n";
    cout << "--------------------\n";
    
    for(auto e : unsupported )
    {
      cout << e.description  << "\n";
      }
*/
    return 0;
}
