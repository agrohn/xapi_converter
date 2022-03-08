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
#include <xapi_entry.h>
#include <string>
////////////////////////////////////////////////////////////////////////////////
namespace XAPI
{
  class MemoEntry : public XAPI::Entry
  {    
  public:
    MemoEntry();
    std::vector<std::string> tags;
    virtual ~MemoEntry();
    void ParseTimestamp(const std::string & strtime) override;
    std::string ToXapiStatement() override;

  };
}
////////////////////////////////////////////////////////////////////////////////
