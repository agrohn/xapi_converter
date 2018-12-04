/*
 * Parses moodle log from CSV and constructs xAPI statements out of it, sending them to LRS.
 * \author anssi grohn at karelia dot fi (c) 2018.
 */
#include <xapi_converter.h>
#include <xapi_errors.h>
#include <xapi_entry.h>
#include <xapi_grade.h>
#include <xapi_statementfactory.h>
#include <xapi_anonymizer.h>
#include <string>
#include <cctype>
#include <json.hpp>
#include <chrono>
#include <thread>
#include <curlpp/Infos.hpp>
using json = nlohmann::json;
using namespace std;
//https://nithinkk.wordpress.com/2017/03/16/learning-locker/
////////////////////////////////////////////////////////////////////////////////
namespace po = boost::program_options;
std::map<std::string,int> errorMessages;
string throbber = "|/-\\|/-\\";
extern XAPI::Anonymizer anonymizer;
////////////////////////////////////////////////////////////////////////////////
XAPI::Application::Application() : desc("Command-line tool for sending XAPI statements to learning locker from  Moodle logs\nReleased under GPLv3 - use at your own risk. \n\nPrerequisities:\n\tLearning locker client credentials must be in json format in data/config.json.\n\tSimple object { \"login\": { \"key\": \"\", \"secret\":\"\"}, \"lms\" : { \"baseURL\" : \"\" }\n\nUsage:\n")
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
  ("errorlog", po::value<string>(), "<errorlog> where error information is printed.")
  ("write", "statements json files are written to local directory.")
  ("anonymize", "Should user data be anonymized.")
  ("print", "statements json is printed to stdout.");
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
  write = vm.count("write") > 0;
  anonymize = vm.count("anonymize") > 0;
  anonymizer.enabled = anonymize;
  
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
  size_t fileSizeInBytes;
  {
    ifstream temp(data.c_str(), ios::binary | ios::ate);
    fileSizeInBytes = temp.tellg();
  }
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
  Progress progress(0,fileSizeInBytes);
  cerr << "\n";
  while (getline(activitylog,line))
  {
    UpdateThrobber("Processing CSV...[" + std::string(progress+=line.length()) + "]...");
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
  activitylog.close();
  json activities = tmp[0];

  Progress progress(0,activities.size());
  // for each log entry
  int entries_without_result = 0;
  cerr << "\n";
  #pragma omp parallel for
  for(size_t i=0;i<activities.size(); ++i)
  {
    #pragma omp critical
    {
      stringstream ss;
      ss << "Caching user data [" << std::string(progress++) << "%]...";
      UpdateThrobber(ss.str());
    }
    try {
      std::vector<string> lineasvec = activities[i];
      XAPI::StatementFactory::CacheUser(lineasvec);
    } catch ( regex_error & ex )
    {
      cerr << ex.what() << "\n";
    }
  }
  cerr << "\n";
  progress.ResetCurrent();
  #pragma omp parallel for
  for(size_t i=0;i<activities.size(); ++i)
  {
    #pragma omp critical
		{
      stringstream ss;
      ss << "Processing JSON event log [" << std::string(progress++) << "%]...";
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
    //catch ( std::exception & ex )
    //{
    // errorMessages[ex.what()]++;
      //cerr << ex.what() << "\n";
    //}
    // vector now contains strings from one row, output to cout here
    //cout << "\n----------------------" << endl;
  }

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
std::string
XAPI::Application::GetStatementsJSON() 
{
  stringstream ss;
  ss << "[";
  auto last_element = statements.end()-1;
  Progress p(0,statements.size());
  cerr << "\n";
  for_each(statements.begin(), last_element, [&ss, this, &p] (auto & s) {
      ss << s << ",";
      this->UpdateThrobber("Converting statements to xAPI [" + std::string(p++) +"] ...");
    });
  ss << *last_element << "]";
  this->UpdateThrobber("Converting statements to xAPI [" + std::string(p++) +"] ...");
  return ss.str();
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::Application::ComputeBatchSizes()
{
  Batch batch;
  Progress p(0,statements.size());

  // Each batch is an array, so it has at least '[' and ']' + first statement.
  batch.size = 2 + statements[0].length();
  batch.end = 1;  // end index is exclusive

  // process statements from second index forward
  for( size_t i=1;i<statements.size();i++)
  {
    size_t statementLength = statements[i].length();
    // batch size needs also to take array separator ',' into account 
    size_t newBatchSize = (1 + batch.size + statementLength) ; 
    
    if ( newBatchSize < clientBodyMaxSize )
    {
      batch.size = newBatchSize;
      batch.end = i+1;
    }
    else      // batch is full, create new
    {
      batch.end = i;
      // update stats
      stats.batchAndStatementsCount[batches.size()] = batch.end-batch.start;
      batches.push_back(batch);

      // new batch
      batch = Batch();
      batch.start = i;
      batch.end = i+1;
      batch.size = statementLength + 2;
      p.ResetCurrent();

    }
    stringstream ss;
    ss << "Computing batches " << batches.size() << " [" << std::string(p+=1)+"%]...";
    this->UpdateThrobber(ss.str());
  }
  // update last progress to 100%
  stringstream ss;
  ss << "Computing batches " << batches.size() << " [" << std::string(p+=1)+"%]...";
  this->UpdateThrobber(ss.str());
  stats.batchAndStatementsCount[batches.size()] = batch.end-batch.start;

  batches.push_back(batch);
  
  stats.numBatches = batches.size();
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::Application::DisplayBatchStates()
{
  stringstream ss;
  ss << "Creating " << batches.size() << " batches ";
  for( size_t b=0;b<batches.size();b++)
  {
    ss <<  " [" << std::string(batches[b].progress)+"%]";
  }
  this->UpdateThrobber(ss.str());
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::Application::CreateBatches()
{
  cerr << "\n";
  stats.statementsInTotal = statements.size();
  
  #pragma omp parallel for 
  for( size_t i=0;i<batches.size();i++)
  {
    Batch & batch = batches[i];
    batch.progress.SetTotal( batch.end-batch.start );
    batch.contents << "[";
    for( size_t si=batch.start;si<batch.end;si++)
    {

      batch.contents << statements[si];
      if ( si < batch.end-1) batch.contents << ",";
      batch.progress++;
      #pragma omp critical
      {
        DisplayBatchStates();
      }
    }
    batch.contents << "]";
  }
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::Application::SendStatements()
{
  
  // send XAPI statements in POST
  if ( learningLockerURL.size() > 0 )
  {
    //https://stackoverflow.com/questions/25852551/how-to-add-basic-authentication-header-to-webrequest#25852562
    string tmp;
    {
      stringstream ss;
      ss << login.key << ":" << login.secret;
      tmp = ss.str();
    }
    string auth = "Basic " + base64_encode(reinterpret_cast<const unsigned char *>(tmp.c_str()), tmp.size());
    int count = 0;
    for( auto & batch : batches )
    {
      // submission might fail, keep trying until you get it done.
      int responseCode = 0;
      int attemptNumber = 1;
      do
      {

	try {
	  curlpp::Cleanup cleaner;
	  curlpp::Easy request;
	
	  string url = learningLockerURL+"/data/xAPI/statements";
	  //cerr << "\nLearning locker URL: " << url << "\n";
	  request.setOpt(new curlpp::options::Url(url)); 
	  request.setOpt(new curlpp::options::Verbose(false)); 
	  std::list<std::string> header;
	  header.push_back("Authorization: " + auth);
	
	  header.push_back("X-Experience-API-Version: 1.0.3");
	  header.push_back("Content-Type: application/json; charset=utf-8");
	  request.setOpt(new curlpp::options::HttpHeader(header)); 
	

	  stringstream ss;
	  ss << "Sending batch " << count;
	  if ( responseCode == 0) {
	    cerr << "\n";
	  } else ss << " (attempt " << attemptNumber << ")";

	  ss << "bytes: " << batch.size << ", "  << "statements: " << (batch.end - batch.start);
	  ss << "...";
	  
	  UpdateThrobber(ss.str());
	  std::string batchContents = batch.contents.str();
	  request.setOpt(new curlpp::options::PostFields(batchContents));
	  request.setOpt(new curlpp::options::PostFieldSize(batchContents.length()));
	  ostringstream response;
	  request.setOpt( new curlpp::options::WriteStream(&response));
	  //cerr << "\n" << batch << "\n";
	  request.perform();
	  responseCode = curlpp::infos::ResponseCode::get(request);

	  if ( responseCode != 200)
	  {
	    ss.str("");
	    ss << "Sending batch " << count << " failed! Retrying.";
	    UpdateThrobber(ss.str());
	    attemptNumber++;
	    // sleep for some time
	    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	    ofstream file("curl.log", std::fstream::app);
	    file << response.str() << "\n";
	    file.close();
	  }

	}
	catch ( curlpp::LogicError & e ) {
	  
	  std::cout << "Logic error " << e.what() << std::endl;
	}
	catch ( curlpp::RuntimeError & e ) {
	  std::cout << "Runtime error " << e.what() << std::endl;
	}
      } while (responseCode != 200);
      count++;
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
void
XAPI::Application::WriteStatementFiles()
{
  
  int count = 0;
  for( auto & batch : batches )
  {
    
    try {
      std::string batchContents = batch.contents.str();
      stringstream ss;
      ss << "Writing batch " <<  count << " (" << batchContents.length() << " bytes, #" << (batch.end - batch.start) << " statements)...";
      stringstream name;
      name << "batch_" << count << ".json";
      UpdateThrobber(ss.str());
      ofstream out(name.str());
      if ( !out ) throw runtime_error("Could not open file: '" + name.str() + "'");
      out << batchContents;
      count++;
      cerr << "\n";
    }
    catch ( std::exception & e ) {
      std::cout << "error " << e.what() << std::endl;
    }
  }
  
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
bool
XAPI::Application::ShouldWrite() const
{
  return write;
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
void
XAPI::Application::LogStats()
{
  stats.statementsInTotal = statements.size();
  cerr << "#all statements : " << stats.statementsInTotal << "\n";
  cerr << "#batches : " << stats.numBatches << "\n";
  for( auto e : stats.batchAndStatementsCount )
  {
    cerr << "Batch " << e.first << ": "
         << "#" <<  e.second << " statements, "
         << "size " <<  batches[e.first].size << "/" << batches[e.first].contents.str().length() << " bytes, "
         << "range " << batches[e.first].start << "-" << (batches[e.first].end-1)
         << "\n";
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
    // parse all config
    ifstream configDetails("data/config.json");
    json config;
    configDetails >> config;
    
    app.login.key = config["login"]["key"];
    app.login.secret = config["login"]["secret"];
    
    // update URL prefices
    app.lmsBaseURL = config["lms"]["baseURL"];
    for(auto & a : activityTypes )
    {
      activityTypes[a.first] = app.lmsBaseURL + a.second;
    }
    
    app.UpdateThrobber();
    if ( app.HasLogData())
    {
	  if ( app.IsLogDataJSON())	app.ParseJSONEventLog();
	  else				app.ParseCSVEventLog();
	}
    if ( app.HasGradeData()) app.ParseGradeLog();
    if ( app.ShouldPrint())
    {
      cout << app.GetStatementsJSON() << "\n";
    }
    app.ComputeBatchSizes();
    app.CreateBatches();
    if ( app.ShouldWrite())
    {
      app.WriteStatementFiles();
    }
    
    if ( app.IsDryRun())
      cout << "alright, dry run - not sending statements.\n";
    else
      app.SendStatements();

    app.LogErrors();
    app.LogStats();
    return 0;
}
////////////////////////////////////////////////////////////////////////////////
