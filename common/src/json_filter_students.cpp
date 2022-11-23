/*
  This file is part of xapi_converter.
  Copyright (C) 2018-2022 Anssi Gröhn
  
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
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <stdexcept>
#include <iostream>
#include <map>
#include <algorithm>
////////////////////////////////////////////////////////////////////////////////
using namespace std;
using json = nlohmann::json;
////////////////////////////////////////////////////////////////////////////////
int main( int argc, char **argv )
{
  json inactive;
  json filtered = json::array();
  ifstream inactive_file(argv[1]);
  ifstream bit_students(argv[2]);
  map<string,bool> valid_students;
  inactive_file >> inactive;
  while(!bit_students.eof())
  {
    string tmp;
    getline(bit_students, tmp);
    valid_students[tmp] = true;
  }

  for( auto & e : inactive  )
  {
    string tmp = e["_id"]["Email"];
    for( auto & c : tmp ) c=tolower(c);

    
    if ( valid_students.find(tmp) != valid_students.end())
    {
      filtered.push_back(e);
    }
  }
  cout << filtered.dump() << "\n";
  return 0;
}
