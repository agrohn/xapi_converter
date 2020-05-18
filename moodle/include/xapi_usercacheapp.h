/*
  This file is part of xapi_converter.  
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
#pragma once
////////////////////////////////////////////////////////////////////////////////
#include <json.hpp>
#include <fstream>
#include <string>
#include <list>
#include <vector>
#include <set>
#include <iostream>
#include <sstream>
#include <boost/program_options.hpp>
namespace XAPI
{
  struct User
  {
    std::string name;
    std::string email;
    std::string userid;
    std::set<std::string> roles;
    void Parse( nlohmann::json & v );
  };
  ////////////////////////////////////////////////////////////////////////////////
  typedef std::map<std::string,User> UserMap;
  ////////////////////////////////////////////////////////////////////////////////
  class UserCacheApp
  {
  protected:
    boost::program_options::variables_map vm;
    boost::program_options::options_description desc;
  public:    
    static void ParseUsers( const std::string & file, UserMap & usermap );
    static void WriteUsers( const std::list<User> & users, const std::string & outfile  );
    virtual ~UserCacheApp();
    virtual bool ParseOptions() = 0;
    virtual void Run();
  };
}
