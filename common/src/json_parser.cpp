/*
  This file is part of xapi_converter.
  Copyright (C) 2021 Anssi Gröhn
  
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
#include <iostream>
#include <string>
#include <sstream>
#include <list>
////////////////////////////////////////////////////////////////////////////////
using namespace std;
using json = nlohmann::json;
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv) 
{
  // read json from stdin, and parse field value according to first parameter.
  // arrays are denoted by integers, fields are separated by dots (.)
  string line;
  stringstream ss;
  
  while ( getline(cin,line) )
  {
    ss << line;
  }
  stringstream tmp;
  tmp << argv[1];
  string section;
  list<string> format;

  while( getline(tmp,section,'.').eof() == false )
  {
    format.push_back(section);
  }
  
  json j;
  ss >> j;
  try {
    while( format.empty() == false )
    {
      stringstream numss;
      numss << format.front();
      int index;
      if ( !(numss >> index))
      {
	j = j[format.front()];
      }
      else
      {
	j = j[index];
      }
      format.pop_front();
    }
    cout << j << "\n";
    return 0;
  }
  catch ( std::exception & e )
  {
    cerr << e.what() << "\n";
    return 1;
  }
}

