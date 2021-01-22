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
#include <xapi_converter.h>
#include <xapi_errors.h>
#include <xapi_entry.h>
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
#define BUFFERSIZE 1024
#include <b64/encode.h>
using json = nlohmann::json;
using namespace std;
//https://nithinkk.wordpress.com/2017/03/16/learning-locker/
////////////////////////////////////////////////////////////////////////////////
namespace po = boost::program_options;
std::map<std::string,int> errorMessages;
string throbber = "|/-\\|/-\\";
XAPI::Anonymizer anonymizer;
#define DEFAULT_BATCH_FILENAME_PREFIX "batch_"
#define DEFAULT_CONFIG_FILENAME "config.json"
////////////////////////////////////////////////////////////////////////////////
XAPI::Application::Application( const std::string & description) : desc(description)
{
  SetCommonOptions();
}
////////////////////////////////////////////////////////////////////////////////
XAPI::Application::~Application()
{
  
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::Application::SetCommonOptions()
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

  tmp.str("");
  tmp << "<count> maximum number of attempts per batch before considered a failure. By default, " << DEFAULT_MAX_BATCH_SEND_ATTEMPTS << ".";
  string batchMaxSendAttemptsDesc = tmp.str();

  tmp.str("");
  tmp << "<value> Seconds to wait before sending next batch to server when previous failed. By default, " << DEFAULT_BATCH_SEND_FAILURE_DELAY_SECONDS << ".";
  string batchSendFailureDelay = tmp.str();

  // options 
  desc.add_options()
  ("send", po::value<string>(), "<addr> learning locker server hostname or ip. If not defined, performs dry run.")
  ("errorlog", po::value<string>(), "<errorlog> where error information is printed.")
  ("write", "statements json files are written to output directory (by default working directory).")
  ("output-dir", po::value<string>(), "<path> directory where json files will be written to.")
  ("load", po::value<std::vector<string> >()->multitoken(), "<file1.json> [<file2.json> ...] Statement json files are loaded to memory from disk. Cannot be used same time with --logs.")
  ("anonymize", "Should user data be anonymized.")
  ("batch-prefix", po::value<string>(), "<string> file name prefix to be used while writing statement json files to disk.")
  ("batch-max-bytes", po::value<size_t>(), batchMaxBytesDescription.c_str())
  ("batch-max-statements", po::value<size_t>(),batchMaxStatementsDescription.c_str())
  ("batch-max-send-attempts", po::value<size_t>(),batchMaxSendAttemptsDesc.c_str())
  ("batch-send-delay", po::value<size_t>(), batchSendDelay.c_str() )
  ("batch-send-failure-delay", po::value<size_t>(), batchSendFailureDelay.c_str() )
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
  

  if ( vm.count("write") > 0 && vm.count("load") > 0 )
  {
    cerr << "Error: You want me to load files from disk and write them again? "
         << "--write or --load cannot be used same time!\nPlease see usage with --help.\n"; 
    return false;
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

  if ( vm.count("batch-max-send-attempts") > 0 )
  {
    maxBatchSendAttempts = vm["batch-max-send-attempts"].as<size_t>();
  }
  
  if ( vm.count("batch-send-delay")> 0)
  {
    sendDelayBetweenBatches = vm["batch-send-delay"].as<size_t>();
  }

  if ( vm.count("batch-send-failure-delay")> 0)
  {
    sendFailureDelayBetweenBatches = vm["batch-send-failure-delay"].as<size_t>();
  }

  print = vm.count("print") > 0;
  write = vm.count("write") > 0;

  if ( write && vm.count("output-dir") > 0 )
  {
    outputDir = vm["output-dir"].as<std::string>();
  }
  anonymize = vm.count("anonymize") > 0;
  anonymizer.enabled = anonymize;
  


  return  ParseCustomArguments();
}
////////////////////////////////////////////////////////////////////////////////
void
XAPI::Application::PrintUsage()
{
  cout << desc << "\n";
}
////////////////////////////////////////////////////////////////////////////////


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
    string auth;
    {
      stringstream ss;
      ss << login.key << ":" << login.secret;
      base64::encoder E;
      // encode using libb64
      stringstream out;
      E.encode(ss,out);
      stringstream authss;
      // E.encode inserts newlines every 72 chars, so let's get rid of them.
      string tmp;
      while( (out >>  tmp) ) authss << tmp;
      auth = "Basic " + authss.str();
      
    }


    size_t count = 0;
    int sendDelaySecondsRemaining = 0;


    bool lastBatchFailed = false;
    // Clear log from previous run, if it exists.
    {
      ofstream file(outputDir+"/"+std::string("curl.log"), std::fstream::trunc);
      file.close();
    }
    for( auto & batch : batches )
    {
      // submission might fail, keep trying until you get it done.
      int responseCode = 0;
      size_t attemptNumber = 1;

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
	      // Since we failed, we set also different wait time for next batch
              sendDelaySecondsRemaining = sendFailureDelayBetweenBatches;
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
	    lastBatchFailed = true;
	    attemptNumber++;
	    errorMessages[e.what()]++;
	    sendDelaySecondsRemaining = sendDelayBetweenBatches;
	    stringstream ss;
	    ss << "Sending batch " << count << " failed! Retrying in " << sendDelaySecondsRemaining << "...";
	    UpdateThrobber(ss.str());
	    attemptNumber++;
          }
          catch ( curlpp::RuntimeError & e ) {
	    lastBatchFailed = true;
	    errorMessages[e.what()]++;

	    sendDelaySecondsRemaining = sendDelayBetweenBatches;
	    stringstream ss;
	    ss << "Sending batch " << count << " failed! Retrying in " << sendDelaySecondsRemaining << "...";
	    UpdateThrobber(ss.str());
	    attemptNumber++;
          }
        }
      } while ( ((sendDelaySecondsRemaining > 0) || (responseCode != 200)) && attemptNumber <= maxBatchSendAttempts );
      // report error if max number of attempts is reached.
      if ( attemptNumber >= maxBatchSendAttempts )
      {
	stringstream ss("Sending batch ");
	ss << count << " failed, reached " << maxBatchSendAttempts << " send attempts.";
	errorMessages[ss.str()]++;
	throw xapi_max_send_attempts_reached_error(ss.str());
      }
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
    if ( err.is_open() == false) throw std::runtime_error("Cannot open error log file: "+ errorFile );
    
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
