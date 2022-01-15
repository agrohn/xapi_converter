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
#include <string>
#include <vector>
////////////////////////////////////////////////////////////////////////////////
namespace XAPI
{
  class xapi_entry;
  //////////////////////////////////////////////////////////////////////////////
  class StatementFactory
  {
  private:
    StatementFactory();
  public:
    static std::string course_id;
    static std::string course_name;
    static std::string course_start_date;
    static std::string course_end_date;
    static std::string CreateActivity( const std::string & moodleLogLine );
    static std::string CreateActivity( const std::vector<std::string> & lineAsVector );
    static std::string CreateGradeEntry( const std::vector<std::string> & lineAsVector );
    static std::string CreateAssignmentInitEntry( const std::string & name, const std::string & id );
    static std::string CreateRoleAssignEntry( const std::string & id, const std::vector<std::string> & roles );
    static void CacheUser( const std::vector<std::string> & lineAsVector );
    static void CacheUser( const std::string & moodleLogLine );
    static void CacheUser( const std::string & name, const std::string & userid, const std::string & email, const std::vector<std::string> & roles );
    static void CacheGroup( const std::string & id, const std::string & name );

  };
}
////////////////////////////////////////////////////////////////////////////////
