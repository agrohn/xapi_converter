/*
  This file is part of xapi_converter.  
  Copyright (C) 2018-2021 Anssi Gr�hn
  
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
#include <xapi_entry.h>
#include <string>
#include <nlohmann/json.hpp>
////////////////////////////////////////////////////////////////////////////////
namespace XAPI
{
  class WorkitemActivityEntry : public XAPI::Entry
  {    
  public:
    WorkitemActivityEntry();
    virtual ~WorkitemActivityEntry();
    void ParseTimestamp( const std::string & strtime) override;
    void Parse( const nlohmann::json & workitemRevision);
    std::string ToXapiStatement() override;
  };
}
////////////////////////////////////////////////////////////////////////////////
