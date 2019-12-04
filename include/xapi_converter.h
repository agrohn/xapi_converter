/*
  This file is part of xapi_converter.  
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
#pragma once
#include <iostream>     
#include <fstream>      
#include <vector>
#include <string>
#include <algorithm>    
#include <iterator>     
#include <iomanip>
#include <chrono>


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
    inline bool IsComplete() const
    {
      return (current== total);
    }
      
  };
  
  struct Context
  {
    std::string courseurl;
    std::string coursename;
    std::string courseStartDate;
    std::string courseEndDate;
  };
  
  struct Statistics
  {
    size_t statementsInTotal{0};
    size_t numBatches{0};

    std::map<int,size_t> batchAndStatementsCount;
    std::chrono::system_clock::time_point startTime;
  };

  struct Batch
  {
    size_t start {0}; ///< batch start location in statements array.
    size_t end {0}; ///<batch end location in statements array.
    size_t size{0}; ///< batch size in bytes.

    std::string contents; ///< Batch contents to send.
    Progress progress;///< Batch progress status.
    std::string filename; ///< In case batch is loaded from disk, filename.
    
    Batch( ) : progress(0,0) {}

    Batch & operator=( const Batch && other ) 
    {
      if ( this != &other )
      {
        start = other.start;
        end = other.end;
        size = other.size;

        progress = other.progress;
        contents = other.contents;
        filename = other.filename;
      }
      return *this;
    }
    
    Batch( const Batch & other ) : progress(0,0)
    {
      start = other.start;
      end = other.end;
      size = other.size;

      progress = other.progress;
      contents = other.contents;
      filename = other.filename;
    }

  };
  /// absolute maximum for POST message body size.
  const size_t CLIENT_BODY_MAX_SIZE_BYTES = 20000000;
  const size_t DEFAULT_BATCH_STATEMENT_COUNT_CAP = 10000;
  const size_t DEFAULT_BATCH_SEND_DELAY_SECONDS = 10;
  class Application
  {
  private:
    std::vector<Batch> batches;
    Statistics stats;
  public:
    int throbberState;
    std::string data;
    std::string gradeData;
    std::string userData;
    std::string learningLockerURL;
    std::string errorFile;
    std::string outputDir;
    // base url for all activity ids URIs
    std::string lmsBaseURL;
    std::string configFile;
    struct Login { 
      std::string key;
      std::string secret;
    };
    Login login;
    
    size_t  clientBodyMaxSize{CLIENT_BODY_MAX_SIZE_BYTES};
    size_t  sendDelayBetweenBatches{DEFAULT_BATCH_SEND_DELAY_SECONDS};
    Context context;
    boost::program_options::variables_map vm;
    boost::program_options::options_description desc;
    std::vector<std::string> statements;
    bool print{false};
    bool write{false};
    bool load{false};
    bool anonymize{false};
    bool makeAssignments{false}; ///< should assignment authorized events be created.
    bool makeRoles{false}; ///< Should roles assigning events be created.
    size_t maxStatementsInBatch{DEFAULT_BATCH_STATEMENT_COUNT_CAP};
    std::string batchFilenamePrefix;
    Application();
    virtual ~Application();
    bool ParseArguments( int argc, char **argv );
    void PrintUsage();
    

    void ParseJSONEventLog();
    void ParseGradeLog();
    void ParseUsers();
    
    void CreateAssignments();
    void CreateRoles();
    
    void ComputeBatchSizes();
    void LoadBatches();
    void CreateBatches();
    void SendBatches();
    void WriteBatchFiles();
    void PrintBatches() const;
    bool HasGradeData() const;
    bool HasLogData() const;
    bool HasUserData() const;
    
    bool IsDryRun() const;
    bool ShouldPrint() const;
    bool ShouldWrite() const;
    bool ShouldLoad() const;
    bool ShouldMakeAssignments() const;
    bool ShouldMakeRoles() const;
    void UpdateThrobber(const std::string & msg = "");
    void DisplayBatchStates();
    void LogErrors();
    void LogStats();
    
  };
}
