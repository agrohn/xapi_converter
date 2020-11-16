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
#include <xapi_converter.h>

#include <cstdlib>
#include <cerrno>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <boost/program_options.hpp>

namespace XAPI
{
  struct Context
  {
    std::string courseurl;
    std::string coursename;
    std::string courseStartDate;
    std::string courseEndDate;
  }; 
  class MoodleParser : public Application
  {

  public:
    std::string data;
    std::string gradeData;
    std::string userData;
    Context context;
    bool makeAssignments{false}; ///< should assignment authorized events be created.
    MoodleParser();
    virtual ~MoodleParser();
  protected:
    bool ParseCustomArguments() override;
  public:
    void ParseJSONEventLog();
    void ParseGradeLog();
    void ParseUsers();

    void CreateAssignments();
    bool HasGradeData() const;
    bool HasLogData() const;
    bool HasUserData() const;
    bool ShouldMakeAssignments() const;

  };
}
