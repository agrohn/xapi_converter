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
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <stdexcept>
#include <iostream>
#include <sstream>
////////////////////////////////////////////////////////////////////////////////
using namespace std;
using json = nlohmann::json;
////////////////////////////////////////////////////////////////////////////////
int main( int argc, char **argv )
{
  if ( ( argc==2 && argv[1][0] == '-') || argc==1 )
  {
    string line;
    json obj;
    stringstream ss;
    while( getline(cin,line) )
    {
      ss << line;
    }
    ss >> obj;
    cout << obj.dump() << "\n";
    return 0;
  }
  for ( int i=1;i<argc;i++)
  {
    ifstream file(argv[i]);
    if ( !file.is_open()) throw runtime_error(std::string("Cannot open file ")+std::string(argv[i]));
    json obj;
    file >> obj;
    cout << obj.dump() << "\n";
  }
  return 0;
}
////////////////////////////////////////////////////////////////////////////////
