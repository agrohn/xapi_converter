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
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <stdexcept>
#include <iostream>
////////////////////////////////////////////////////////////////////////////////
using namespace std;
using json = nlohmann::json;
////////////////////////////////////////////////////////////////////////////////
int main( int argc, char **argv )
{
  json info;
  json data;

  ifstream infoFile(argv[1]);
  if ( !infoFile.is_open()) throw runtime_error(std::string("Cannot open file ")+std::string(argv[1]));

  ifstream dataFile(argv[2]);
  if ( !dataFile.is_open()) throw runtime_error(std::string("Cannot open file ")+std::string(argv[2]));  
  
  infoFile >> info;
  dataFile >> data;  

  infoFile.close();
  dataFile.close();
  json elem = info.at(0);
  // build an array with one element, containing data spiced with course name and id.
  json result = {
    {
      { "courseName", elem["courseName"] },
      { "courseId", elem["courseId"] },
      { "data", data }
    }
  };

  // write results to file
  ofstream out(argv[3]);
  out << result;

  return 0;
}
////////////////////////////////////////////////////////////////////////////////
