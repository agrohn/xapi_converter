#pragma once
#include <iostream>     
#include <fstream>      
#include <vector>
#include <string>
#include <algorithm>    
#include <iterator>     
#include <iomanip>



#include <cstdlib>
#include <cerrno>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <boost/program_options.hpp>

std::string base64_encode(const unsigned char *src, size_t len);

namespace XAPI
{

  class Progress
  {
  private:
    int current{0};
    int total{0};
  public:

    Progress( int c, int t ) : current(std::min(c,t)), total(t) {}
    operator std::string() {
      std::stringstream ss;
      if ( total == 0 ) ss << std::setw(3) << "0";
      else ss << std::setw(3) << (int)( (current/(float)total)*100.0);
      return ss.str();
    }
    Progress operator++(int value)
    {
      current++;
      current = std::min(current,total);
      return Progress(current, total);
    }
    Progress & operator+=(int value)
    {
      current = std::min(current+value,total);
      return *this;
    }
    void ResetCurrent()
    {
      current = 0;
    }
    void SetTotal(int t)
    {
      total = t;
    }
  };
  
  struct Context
  {
    std::string courseurl;
    std::string coursename;
  };
  
  struct Statistics
  {
    size_t statementsInTotal{0};
    size_t numBatches{0};

    std::map<int,size_t> batchAndStatementsCount;
  };
  enum BatchState { kIdle, kReadyForSending, kSending, kSent, kNumBatchStates };
  struct Batch
  {
    size_t start {0}; ///< batch start location in statements array.
    size_t end {0}; ///<batch end location in statements array.
    size_t size{0}; ///< batch size in bytes.
    BatchState state{XAPI::kIdle}; ///< Current batch state.
    std::stringstream contents; ///< Batch contents to send.
    Progress progress;///< Batch progress status.
    
    Batch( ) : progress(0,0) {}

    Batch & operator=( const Batch && other ) 
    {
      if ( this != &other )
      {
	start = other.start;
	end = other.end;
	size = other.size;
	state = other.state;
	progress = other.progress;
	contents.str(other.contents.str());
      }
      return *this;
    }
    
    Batch( const Batch & other ) : progress(0,0)
    {
      start = other.start;
      end = other.end;
      size = other.size;
      state = other.state;
      progress = other.progress;
      contents.str(other.contents.str());
    }

  };
  
  class Application
  {
  private:
    std::vector<Batch> batches;
    Statistics stats;
  public:
    int throbberState;
    bool dataAsJSON{false};
    std::string data;
    std::string gradeData;
    std::string learningLockerURL;
    std::string errorFile;
    // base url for all activity ids URIs
    std::string lmsBaseURL;
    struct Login { 
      std::string key;
      std::string secret;
    };
    Login login;
    
    size_t  clientBodyMaxSize{20000000}; 
    Context context;
    boost::program_options::variables_map vm;
    boost::program_options::options_description desc;
    std::vector<std::string> statements;
    bool print{false};
    bool write{false};
    bool anonymize{false};
    Application();
    virtual ~Application();
    bool ParseArguments( int argc, char **argv );
    void PrintUsage();
    
    void ParseCSVEventLog();
	void ParseJSONEventLog();
    void ParseGradeLog();

    void ComputeBatchSizes();
    void CreateBatches();
    void SendStatements();
    void WriteStatementFiles();
    bool HasGradeData() const;
    bool HasLogData() const;
    bool IsLogDataJSON() const;
    bool IsDryRun() const;
    bool ShouldPrint() const;
    bool ShouldWrite() const;
    std::string GetStatementsJSON();
    void UpdateThrobber(const std::string & msg = "");
    void DisplayBatchStates();
    void LogErrors();
    void LogStats();
    
  };
}
