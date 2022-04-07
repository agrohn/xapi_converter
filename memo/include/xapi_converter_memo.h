/*
  This file is part of xapi_converter.  
  Copyright (C) 2018-2021 Anssi Gröhn
  
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

  class Memo : public XAPI::Application
  {

  public:
    struct MemoData {
      std::string fileIdentifier;
      std::vector<std::string> memberHeaders;
      std::string detailSectionIdentifier;
      std::vector<std::string> activityTriggerWords;
      std::vector<std::string> dedicationTriggerWords;
    };
    std::string memoData;
    MemoData memo;
    std::string courseName;
    std::string courseUrl;
    

    Memo();
    virtual ~Memo();
    bool ParseCustomArguments() override;
    void ParseMemo();
    bool HasMemoData() const;
    void VerifySessionEntryHeaders( std::ifstream & file );

  };
}
