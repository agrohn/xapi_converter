/*
 * Parses moodle log from CSV and constructs xAPI statements out of it, sending them to LRS.
 * \author anssi grohn at karelia dot fi (c) 2018.
 */
#include <xapi_converter.h>
#include <xapi_errors.h>
#include <xapi_entry.h>
#include <xapi_grade.h>
#include <xapi_statementfactory.h>
#include <string>
#include <cctype>
#include <json.hpp>
#include <chrono>
#include <thread>

using json = nlohmann::json;
using namespace std;
//https://nithinkk.wordpress.com/2017/03/16/learning-locker/
////////////////////////////////////////////////////////////////////////////////
const int NUM_ARGUMENTS_WHEN_SENDING = 4;
const int NUM_ARGUMENTS_WITHOUT_SENDING = 3;
namespace po = boost::program_options;
std::map<std::string,int> errorMessages;
string throbber = "|/-\\|/-\\";
namespace XAPI
{
	class Progress
	{
	private:
		int current;
		int total;
	public:
		Progress( int c, int t ) current(c), total(t) {}
		std::string operator() {
			
		}
	};
}

XAPI::Application::Application() : desc("Command-line tool for sending XAPI statements to learning locker from  Moodle logs\nReleased under GPLv3 - use at your own risk. \n\nPrerequisities:\n\tLearning locker client credentials must be in json format in data/login.json.\n\tSimple object { \"key\": \"\", \"secret\":\"\"}\n\nUsage:\n")
{

  ////////////////////////////////////////////////////////////////////////////////
  // For some hints on usage...
  

  // options 
  desc.add_options()
  ("log", po::value<string>(), "<YOUR-LOG-DATA.csv> Actual course log data in CSV format")
  ("logjson", po::value<string>(), "<YOUR-LOG-DATA.json> Actual course log data in JSON format")
  ("grades", po::value<string>(), "<YOUR-GRADE_DATA.json>  Grade history data obtained from moodle")
  ("courseurl", po::value<string>(), "<course_url> Unique course moodle web address, ends in ?id=xxxx")
  ("coursename", po::value<string>(), "<course_name> Human-readable name for the course")
  ("host", po::value<string>(), "<addr> learning locker server hostname or ip. If not defined, performs dry run.")
  ("errorlog", po::value<string>(), "<errorlog> where error information is printed.");
  ("print", "statements json is printed to stdout");
  throbberState = 0;

}
////////////////////////////////////////////////////////////////////////////////
XAPI::Application::~Application()
{

}
////////////////////////////////////////////////////////////////////////////////
bool
XAPI::Application::ParseArguments( int argc, char **argv )
{
  namespace po = boost::program_options;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if ( vm.count("log")  )
  {
    data = vm["log"].as<string>();
	dataAsJSON = false;
  }
  
  if ( vm.count("logjson" ))
  {
	data = vm["logjson"].as<string>();
	dataAsJSON = true;
  }
  
  if ( vm.count("grades") )
    gradeData = vm["grades"].as<string>();

  if ( vm.count("courseurl") == 0) { cerr << "courseurl missing\n"; return false;}
  else context.courseurl = vm["courseurl"].as<string>();

  if ( vm.count("coursename") == 0) { cerr << "coursename missing\n"; return false;}
  else context.coursename = vm["coursename"].as<string>();
  // check if learning locker url was specified
  if ( vm.count("host") )
    learningLockerURL = vm["host"].as<string>();

  if ( vm.count("errorlog"))
    errorFile = vm["errorlog"].as<string>();
  
  print = vm.count("print") > 0;
  XAPI::StatementFactory::course_id   = context.courseurl;
  XAPI::StatementFactory::course_name = context.coursename;
  
  return true;
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::Application::PrintUsage()
{
  cout << desc << "\n";
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::Application::ParseCSVEventLog()
{
  // try to open activity log
  ifstream activitylog(data.c_str());
  if (!activitylog.is_open())
  {
    stringstream ss;
    ss << "Cannot open file '" << data << "'\n";
    throw xapi_parsing_error( ss.str());
  }
  ////////////////////////////////////////////////////////////////////////////////
  // actual parsing of statements in activity
  string line;
  // skip first header line 
  getline(activitylog,line);

  while (getline(activitylog,line))
  {
    UpdateThrobber("Loading CSV...");
    try
    {
      statements.push_back(	XAPI::StatementFactory::CreateActivity(line) );
    }
    catch ( xapi_activity_type_not_supported_error & ex )
    {
      errorMessages[ex.what()]+=1;
      //cerr << ex.what() << "\n";
    }
    catch ( xapi_parsing_error & ex )
    {
      errorMessages[ex.what()]+=1;
      //cerr << ex.what() << "\n";
    }
    catch ( std::out_of_range & ex )
    {
      errorMessages["could not parse time"]+=1;
      //cout << "could not parse time\n";
    }
    // vector now contains strings from one row, output to cout here
    //cout << "\n----------------------" << endl;
  }
  activitylog.close();
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::Application::ParseJSONEventLog()
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
  json activities = tmp[0];

	size_t numActivities = activities.size();
	size_t current=0;
  // for each log entry
  int entries_without_result = 0;
  for(auto it = activities.begin(); it != activities.end(); ++it)
  {
		int progress=(float)current++/(float)numActivities * 100.0f;
		stringstream ss;
		ss << "Loading JSON event log [" << progress << "%]...";
		UpdateThrobber(ss.str());
    // each log column is an array elemetn
    std::vector<string> lineasvec = *it;
    try
    {
      // use overwritten version of Parse
      statements.push_back(XAPI::StatementFactory::CreateActivity(lineasvec));
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
  activitylog.close();
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::Application::ParseGradeLog()
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
  json grading = tmp[0];
  // for each log entry
  int entries_without_result = 0;
  for(auto it = grading.begin(); it != grading.end(); ++it)
  {
    UpdateThrobber("Parsing grade log...");
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
std::string
XAPI::Application::GetStatementsJSON() 
{
  stringstream ss;
  ss << "[";
  auto last_element = statements.end()-1;
  for_each(statements.begin(), last_element, [&ss, this] (auto & s) {
      ss << s << ",";
      this->UpdateThrobber("Converting statements to xAPI...");
    });
  ss << *last_element << "]";
  return ss.str();
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::Application::CreateBatchesToSend()
{
  batches.clear();
  stringstream batch;
  batch << "[";
  auto last_element = statements.end()-1;
	
	while(statements.empty() == false)
	{
		
		this->UpdateThrobber("Creating batch ..." + batches.size());
		size_t batchSize = (batch.str() + statements.back()).length();
		if ( batchSize < clientBodyMaxSize )
		{
			batch << statements.back() << ",";
			statements.pop_back();
		}
		else
		{
			// append string to batches
			string tmp = batch.str();
			tmp[tmp.size()-1] = ']';
			batches.push_back(tmp);
			// reset stringstream
			batch.str("[");
		}
	}

}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::Application::SendStatements()
{
	CreateBatchesToSend();
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

			for( auto & batch : batches )
			{
				UpdateThrobber("Sending batches...");
				request.setOpt(new curlpp::options::PostFields(batch));
				request.setOpt(new curlpp::options::PostFieldSize(batch.size()));
				//cerr << batch << "\n";
				request.perform();
			}
			
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

}
////////////////////////////////////////////////////////////////////////////////
bool
XAPI::Application::HasGradeData() const
{
  return !gradeData.empty();
}
////////////////////////////////////////////////////////////////////////////////
bool
XAPI::Application::HasLogData() const
{
  return !data.empty();
}
bool
XAPI::Application::IsLogDataJSON() const
{
	return dataAsJSON;
}
////////////////////////////////////////////////////////////////////////////////
bool
XAPI::Application::IsDryRun() const
{
  return learningLockerURL.empty();
}
bool
XAPI::Application::ShouldPrint() const
{
  return print;
}
void
XAPI::Application::UpdateThrobber(const std::string & msg )
{
  throbberState++;
  throbberState %= throbber.length();
  cerr << '\r';
  if ( msg.length() > 0 ) cerr << msg << " ";
  cerr << throbber[throbberState];
  
}
void
XAPI::Application::LogErrors()
{
  if ( errorFile.length() > 0 )
  {
    ofstream err(errorFile);
    err << "Run yielded " << errorMessages.size() << " different errors.\n";
    if ( err.is_open() == false) throw new std::runtime_error("Cannot open error log file: "+ errorFile );
    
    for(auto & s:errorMessages)
    {
      err << s.first << " : " << s.second << "\n";
    }	
    err.close();
  }
}
////////////////////////////////////////////////////////////////////////////////
int main( int argc, char **argv)
{
    using namespace std;
    
    XAPI::Application app;
    if ( app.ParseArguments(argc, argv) == false )
    {
      app.PrintUsage();
      return 1;
    }
    
    cout << "course url: \"" << XAPI::StatementFactory::course_id << "\"\n";
    cout << "course name: \"" << XAPI::StatementFactory::course_name << "\"\n";
    app.UpdateThrobber();
    if ( app.HasLogData())
    {
	  if ( app.IsLogDataJSON())	app.ParseJSONEventLog();
	  else						app.ParseCSVEventLog();
	}
    if ( app.HasGradeData()) app.ParseGradeLog();
    if ( app.ShouldPrint())
    {
      cout << app.GetStatementsJSON() << "\n";
    }
    if ( app.IsDryRun())
      cout << "alright, dry run - not sending statements.\n";
    else
      app.SendStatements();

    app.LogErrors();
    
    return 0;
}
////////////////////////////////////////////////////////////////////////////////
