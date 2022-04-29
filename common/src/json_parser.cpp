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
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <list>
#include <algorithm>
////////////////////////////////////////////////////////////////////////////////
using namespace std;
using json = nlohmann::json;
////////////////////////////////////////////////////////////////////////////////
void handle_array_element( json j, list<string> format)
{
  bool hadArrayHandling = false;
  while( format.empty() == false )
  {
    stringstream numss;
    numss << format.front();
    int index;
    if ( format.front() == "[]" )
    {
      hadArrayHandling = true;
      format.pop_front();
      for ( auto & e : j )
      {
	handle_array_element(e,format);
      }
      // since actual handling occurs within array elements,
      // we can break loop here.

      break;
    }
    else if ( !(numss >> index))
    {

      vector<string> tokens;
      boost::split(tokens, format.front(), boost::is_any_of("|"));
      if ( tokens.size() > 1 )
      {
        list<string> splitFormat = format;
	splitFormat.pop_front();
	hadArrayHandling = true;
	for( auto & s : tokens )
	{
             splitFormat.push_front(s);
             handle_array_element( j, splitFormat);
             splitFormat.pop_front();
	}
      }
      else
      {
	j = j[format.front()];
      }
    }
    else 
    {
      j = j[index];
    }
    format.pop_front();
  }
  if (hadArrayHandling == false )
    cout << j << "\n";
}
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
  // add final value after comma (if it exists)
  if ( section.empty() == false )
    format.push_back(section);

  json j;
  ss >> j;
  try {
    handle_array_element(j,format);
    return 0;
  }
  catch ( std::exception & e )
  {
    cerr << e.what() << "\n";
    return 1;
  }
}

