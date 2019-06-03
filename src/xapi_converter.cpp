/*
 * Parses moodle logs and constructs xAPI statements out of them, sending them to LRS.
 * \author anssi grohn at karelia dot fi (c) 2018.
 */
#include <xapi_converter.h>
#include <xapi_errors.h>
#include <xapi_entry.h>
#include <xapi_grade.h>
#include <xapi_statementfactory.h>
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
std::map<std::string,int> errorMessages;
string throbber = "|/-\\|/-\\";
extern XAPI::Anonymizer anonymizer;
#define DEFAULT_BATCH_FILENAME_PREFIX "batch_"
#define DEFAULT_CONFIG_FILENAME "config.json"
////////////////////////////////////////////////////////////////////////////////
XAPI::Application::Application() : desc("Command-line tool for sending XAPI statements to learning locker from  Moodle logs\nReleased under GPLv3 - use at your own risk. \n\nPrerequisities:\n\tLearning locker client credentials must be in json format in data/config.json.\n\tSimple object { \"login\": { \"key\": \"\", \"secret\":\"\"}, \"lms\" : { \"baseURL\" : \"\" }\n\nUsage")
{

  ////////////////////////////////////////////////////////////////////////////////
  // For some hints on usage...
  
  stringstream tmp;
  tmp << "<value> max bytes per batch (needs to be less than or equal to default " << CLIENT_BODY_MAX_SIZE_BYTES <<").";
  string batchMaxBytesDescription = tmp.str();

  tmp.str("");
  tmp << "<value> max number of statements per batch. By default, " << DEFAULT_BATCH_STATEMENT_COUNT_CAP << ".";
  string batchMaxStatementsDescription = tmp.str();

  tmp.str("");
  tmp << "<value> Seconds to wait before sending next batch to server. By default, " << DEFAULT_BATCH_SEND_DELAY_SECONDS << ".";
  string batchSendDelay = tmp.str();
  // options 
  desc.add_options()
  ("log", po::value<string>(), "<YOUR-LOG-DATA.json> Actual course log data in JSON format")
  ("grades", po::value<string>(), "<YOUR-GRADE-DATA.json>  Grade history data obtained from moodle")
  ("users", po::value<string>(), "<YOUR-USERS-DATA.json>  User (participant) data obtained from moodle")
  ("courseurl", po::value<string>(), "<course_url> Unique course moodle web address, ends in ?id=xxxx")
  ("coursename", po::value<string>(), "<course_name> Human-readable name for the course")
  ("send", po::value<string>(), "<addr> learning locker server hostname or ip. If not defined, performs dry run.")
  ("errorlog", po::value<string>(), "<errorlog> where error information is printed.")
  ("write", "statements json files are written to output directory (by default working directory).")
  ("output-dir", po::value<string>(), "<path> directory where json files will be written to.")
  ("load", po::value<std::vector<string> >()->multitoken(), "<file1.json> [<file2.json> ...] Statement json files are loaded to memory from disk. Cannot be used same time with --logs.")
  ("anonymize", "Should user data be anonymized.")
  ("batch-prefix", po::value<string>(), "<string> file name prefix to be used while writing statement json files to disk.")
  ("batch-max-bytes", po::value<size_t>(), batchMaxBytesDescription.c_str())
  ("batch-max-statements", po::value<size_t>(),batchMaxStatementsDescription.c_str())
  ("batch-send-delay", po::value<size_t>(), batchSendDelay.c_str() )
  ("authorize-assignments", "if defined, assignment authorization events will be created." )
  ("course-start", po::value<string>(), "<date> Course start date in format YYYY-MM-DD." )
  ("help", "print this help message.")
  ("generate-config", "Generates configuration file template config.json.template to into current directory.")
  ("config", po::value<string>(), "<filename> Loads configuration from given file instead of data/config.json" )
  ("print", "statements json is printed to stdout.");
  
  throbberState = 0;
  outputDir = ".";
  batchFilenamePrefix = DEFAULT_BATCH_FILENAME_PREFIX;
  stats.startTime = chrono::system_clock::now();
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

  if ( vm.count("help") > 0 )
  {
    PrintUsage();
    exit(0);
    //return false;
  }

  if ( vm.count("generate-config") > 0 )
  {
    // generate config template
    json full = {
      {
	"login",  {
	  {"key", "",},
	  {"secret", "" }
	}
      },
      {
	"lms",{
	  { "baseURL", "" }
	}
      }
    };
    // write over previous one with no remorse
    ofstream file("config.json.template");
    file << full.dump(2);
    file.close();
    exit(0);
    
  }
  
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
  else if ( vm.count("load") > 0 )
  {
    load = true;
    vector<string> filenames = vm["load"].as< vector<string> >();
    set<string> uniqueFilenames(filenames.begin(), filenames.end());
    for( auto filename : uniqueFilenames )
    {
      batches.emplace_back();
      batches.back().filename = filename;
    }
  }

  if ( vm.count("grades") )
    gradeData = vm["grades"].as<string>();

  if ( vm.count("users") )
    userData = vm["users"].as<string>();
  
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
  
  // check if learning locker url was specified
  if ( vm.count("send") )
    learningLockerURL = vm["send"].as<string>();

  if ( vm.count("errorlog"))
    errorFile = vm["errorlog"].as<string>();

  // Use custom config or default if not specified.
  if ( vm.count("config") > 0 )
    configFile = vm["config"].as<string>();
  else
    configFile = DEFAULT_CONFIG_FILENAME;
  
  if ( vm.count("batch-prefix") > 0)
  {
    batchFilenamePrefix = vm["batch-prefix"].as<string>();
  }

  if ( vm.count("batch-max-bytes")> 0)
  {
    clientBodyMaxSize = std::min(vm["batch-max-bytes"].as<size_t>(),
                                 CLIENT_BODY_MAX_SIZE_BYTES);
  }
  
  if ( vm.count("batch-max-statements")> 0)
  {
    maxStatementsInBatch = vm["batch-max-statements"].as<size_t>();
  }
  
  if ( vm.count("batch-send-delay")> 0)
  {
    sendDelayBetweenBatches = vm["batch-send-delay"].as<size_t>();
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
  
  print = vm.count("print") > 0;
  write = vm.count("write") > 0;

  if ( write && vm.count("output-dir") > 0 )
  {
    outputDir = vm["output-dir"].as<std::string>();
  }
  anonymize = vm.count("anonymize") > 0;
  anonymizer.enabled = anonymize;
  
  XAPI::StatementFactory::course_id   = context.courseurl;
  XAPI::StatementFactory::course_name = context.coursename;
  XAPI::StatementFactory::course_start_date = context.courseStartDate;
  XAPI::StatementFactory::course_end_date = context.courseEndDate;
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
XAPI::Application::ParseUsers()
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
    XAPI::StatementFactory::CacheUser(name, userid, email);
  }
  usersFile.close();
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::Application::CreateAssignments()
{
  for( auto & it : TaskNameToTaskID )
  {
    statements.push_back(XAPI::StatementFactory::CreateAssignmentInitEntry( it.first, it.second ));
  }
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::Application::ComputeBatchSizes()
{
  Batch batch;
  Progress p(0,std::max(statements.size(),(size_t)1));
  if ( statements.empty() )
  {
    stringstream ss;
    ss << "Computing batches " << batches.size() << " [" << std::string(p+=1)+"%]...";
    this->UpdateThrobber(ss.str());
    stats.numBatches = batches.size();
    return;
  }


  // Each batch is an array, so it has at least '[' and ']' + first statement.
  batch.size = 2 + statements[0].length();
  batch.end = 1;  // end index is exclusive

  // process statements from second index forward
  for( size_t i=1;i<statements.size();i++)
  {
    size_t statementLength = statements[i].length();
    // batch size needs also to take array separator ',' into account 
    size_t newBatchSize = (1 + batch.size + statementLength); 
    size_t newBatchStatementCount = i+1-batch.start;
    
    if ( newBatchSize < clientBodyMaxSize &&
         newBatchStatementCount <= maxStatementsInBatch )
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
  size_t completed = 0;
  for( size_t b=0;b<batches.size();b++)
  {
    completed += batches[b].progress.IsComplete() ? 1:0;
  }
  ss << "Done " << (ShouldLoad() ? "loading " : "creating ") << completed << "/" << batches.size() << " batches ";
  this->UpdateThrobber(ss.str());
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::Application::LoadBatches()
{
  cerr << "\n";
  #pragma omp parallel for 
  for( size_t i=0;i<batches.size();i++)
  {
    Batch & batch = batches[i];
    batch.progress.SetTotal(2);
    if ( omp_get_thread_num() == 0 ) DisplayBatchStates();
    
    ifstream file(batch.filename, ios::in | ios::binary | ios::ate);
    
    if ( file.is_open() == false )
      throw runtime_error("cannot open batch file for reading '" + batch.filename  + "'");

    struct stat st;
    stat(batch.filename.c_str(), &st);
    unsigned long fileSize = st.st_size;

    file.seekg(0, ios::beg);
    batch.progress++;

    if ( omp_get_thread_num() == 0 ) DisplayBatchStates();
    batch.contents.reserve(fileSize);
    // as shown here:
    // https://stackoverflow.com/questions/2912520/read-file-contents-into-a-string-in-c
    std::streambuf * raw_buffer = file.rdbuf();
    char * buf = new char[fileSize+1];
    buf[fileSize] = '\0';
    raw_buffer->sgetn(buf,fileSize);
    batch.contents.append(buf);
    delete buf;
    // now this would be the place to count also items so sending routine
    // would report them correctly, but it would takes more time (json::parse)
    batch.progress++;
    file.close();
    
    batch.size = fileSize;
    if ( omp_get_thread_num() == 0 )  DisplayBatchStates();
  }
  DisplayBatchStates();
    cerr << "\n";
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
    batch.contents.append("[");
    for( size_t si=batch.start;si<batch.end;si++)
    {

      batch.contents.append(statements[si]);
      if ( si < batch.end-1) batch.contents.append(",");
      batch.progress++;

      if ( omp_get_thread_num() == 0 )
        DisplayBatchStates();

    }
    batch.contents.append("]");
  }
  DisplayBatchStates();
  cerr << "\n";
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::Application::SendBatches()
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
    int sendDelaySecondsRemaining = 0;


    bool lastBatchFailed = false;
    
    for( auto & batch : batches )
    {
      // submission might fail, keep trying until you get it done.
      int responseCode = 0;
      int attemptNumber = 1;
      do
      {
        if ( sendDelaySecondsRemaining > 0  )
        {

          stringstream ss;
          if ( lastBatchFailed )
          {
            ss << "Sending batch " << count << " failed! Retrying in " << sendDelaySecondsRemaining << "...";
          }
          else
          {
            ss << "Sending batch " << count+1 << " in " << sendDelaySecondsRemaining  << "...";
          }
          UpdateThrobber(ss.str());
          // sleep for some time
          std::this_thread::sleep_for(std::chrono::milliseconds(1000));
          sendDelaySecondsRemaining--;
        }
        else
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
            ss << "Sending batch " << count << " ";
            if ( responseCode != 0)
            {
              ss << "(attempt " << attemptNumber << ") ";
            }
            if ( batch.filename.empty() ) 
              ss << (batch.end - batch.start) << " statements ";
            else
              ss << batch.filename  << " ";

                    
            ss << "(" << batch.size << "B)"   << "...";

	  
            UpdateThrobber(ss.str());
            std::string & batchContents = batch.contents;
            request.setOpt(new curlpp::options::PostFields(batchContents));
            request.setOpt(new curlpp::options::PostFieldSize(batchContents.length()));
            ostringstream response;
            request.setOpt( new curlpp::options::WriteStream(&response));
            //cerr << "\n" << batch << "\n";
            request.perform();
            responseCode = curlpp::infos::ResponseCode::get(request);

            if ( responseCode != 200)
            {
              lastBatchFailed = true;
              sendDelaySecondsRemaining = sendDelayBetweenBatches;
              ss.str("");
              ss << "Sending batch " << count << " failed! Retrying in " << sendDelaySecondsRemaining << "...";
              UpdateThrobber(ss.str());
              attemptNumber++;
	    
              ofstream file(outputDir+"/"+std::string("curl.log"), std::fstream::app);
              file << response.str() << "\n";
	      file << "\n--- failed batch contents BEGIN ---\n";
	      file << batchContents;
	      file << "\n--- failed batch contents END ---\n";
              file.close();
            }
            else
            {
              lastBatchFailed = false;
              // if batch is not the last one, add delay.
              if ( count < batches.size()-1)
              {
                sendDelaySecondsRemaining = sendDelayBetweenBatches;
              }
              cerr << "\n";
            }

          }
          catch ( curlpp::LogicError & e ) {
	  
            std::cout << "Logic error " << e.what() << std::endl;
          }
          catch ( curlpp::RuntimeError & e ) {
            std::cout << "Runtime error " << e.what() << std::endl;
          }
        }
      } while ( (sendDelaySecondsRemaining > 0) || (responseCode != 200) );
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
XAPI::Application::WriteBatchFiles()
{
  
  int count = 0;
  // get count of numbers is base 10 figure.
  size_t paddingWidth = batches.size() > 0 ? ((size_t)log10(batches.size()))+1 : 0 ;

  for( auto & batch : batches )
  {
    try {
      std::string & batchContents = batch.contents;
      stringstream ss;
      ss << "Writing batch " <<  count+1 << "/" << batches.size() << " (" << batchContents.length() << " bytes, #" << (batch.end - batch.start) << " statements)...";
      stringstream name;
      // ensure there are always zero-filled counter for easier sorting
      name << outputDir << "/" << batchFilenamePrefix << setfill('0') << setw(paddingWidth) << count << ".json";
      UpdateThrobber(ss.str());
      ofstream out(name.str());
      if ( !out ) throw runtime_error("Could not open file: '" + name.str() + "'");
      out << batchContents;
      count++;
    }
    catch ( std::exception & e ) {
      std::cout << "error " << e.what() << std::endl;
    }
  }
  
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::Application::PrintBatches() const
{
  for(auto b : batches )
  {
    cout << b.contents << "\n";
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
////////////////////////////////////////////////////////////////////////////////
bool
XAPI::Application::HasUserData() const
{
  return !userData.empty();
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
bool
XAPI::Application::ShouldLoad() const
{
  return load;
}
bool
XAPI::Application::ShouldMakeAssignments() const
{
  return makeAssignments;
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
    ofstream err(outputDir+"/"+errorFile);
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
         << "size " <<  batches[e.first].size << "/" << batches[e.first].contents.length() << " bytes, "
         << "range " << batches[e.first].start << "-" << (batches[e.first].end-1)
         << "\n";
  }
  
  chrono::duration<float> processingTime(chrono::system_clock::now() - stats.startTime);
  cerr << "Process ran: " << processingTime.count() << "seconds\n";
}
////////////////////////////////////////////////////////////////////////////////
int main( int argc, char **argv)
{
    using namespace std;

    XAPI::Application app;
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
    
    if ( app.IsDryRun())
      cout << "alright, dry run - not sending statements.\n";
    else
      app.SendBatches();

    app.LogErrors();
    app.LogStats();

    return 0;
}
////////////////////////////////////////////////////////////////////////////////
