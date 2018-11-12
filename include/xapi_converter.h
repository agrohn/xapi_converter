#pragma once
#include <iostream>     
#include <fstream>      
#include <vector>
#include <string>
#include <algorithm>    
#include <iterator>     




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
  struct Context
  {
    std::string courseurl;
    std::string coursename;
  };
  class Application
  {
  private:
    void CreateBatchesToSend();
    std::vector<std::string> batches;
  public:
    int throbberState;
    bool dataAsJSON{false};
    std::string data;
    std::string gradeData;
    std::string learningLockerURL;
    std::string errorFile;
    int  clientBodyMaxSize{20000000}; // 20 MB
    Context context;
    boost::program_options::variables_map vm;
    boost::program_options::options_description desc;
    std::vector<std::string> statements;
    bool print{false};
    
    Application();
    virtual ~Application();
    bool ParseArguments( int argc, char **argv );
    void PrintUsage();
    
    void ParseCSVEventLog();
	void ParseJSONEventLog();
    void ParseGradeLog();

    void SendStatements();
    bool HasGradeData() const;
    bool HasLogData() const;
    bool IsLogDataJSON() const;
    bool IsDryRun() const;
    bool ShouldPrint() const;
    std::string GetStatementsJSON();
    void UpdateThrobber(const std::string & msg = "");
    void LogErrors();
  };
}
