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
#include <xapi_usercache_diff.h>
////////////////////////////////////////////////////////////////////////////////
using namespace std;
using json = nlohmann::json;
namespace po = boost::program_options;
XAPI::UserCacheDiff::UserCacheDiff(int argc, char **argv)
{
  desc.add_options()
  ("previous", po::value<string>(), "<file>.json Previously known users in JSON file")
  ("current", po::value<string>(), "<file.json> Currently known users in JSON file")
  ("deleted", po::value<string>(), "<file>.json Where deleted users are stored")
  ("added", po::value<string>(), "<file.json> Where added users are stored");
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);
}
////////////////////////////////////////////////////////////////////////////////
bool XAPI::UserCacheDiff::ParseOptions()
{
  if ( vm.count("previous") == 1 )
  {
    previousFile = vm["previous"].as<string>();
  }
  else
  {
    cerr << "You need at least one previous file.\n\n";
    cerr << desc << "\n";
    return false;
  }

  
  if ( vm.count("current") == 1 )
  {
    currentFile = vm["current"].as<string>();
  }
  else
  {
    cerr << "You need to have one current file.\n\n";
    cerr << desc << "\n";
    return false;
  }
  
  if ( vm.count("deleted") == 1 )
  {
    deletedFile = vm["deleted"].as<string>();
  }
  else
  {
    cerr << "You need to have one deleted users output file.\n\n";
    cerr << desc << "\n";
    return false;
  }

  if ( vm.count("added") == 1 )
  {
    addedFile = vm["added"].as<string>();
  }
  else
  {
    cerr << "You need to have one added users output file.\n\n";
    cerr << desc << "\n";
    return false;
  }
  
  return true;
}


void XAPI::UserCacheDiff::Run()
{
  ParseUsers(previousFile, previousUsers);
  ParseUsers(currentFile,  currentUsers);

  // If user is in previous list, but not in current list,
  // then mark user deleted.
  for( auto & u : previousUsers )
  {
    if ( currentUsers.find(u.first) == currentUsers.end())
      deletedUsers.push_back(u.second);
  }
  // If user is in current list, but not in previous list,
  // then mark user added.
  for( auto & u : currentUsers )
  {
    if ( previousUsers.find(u.first) == previousUsers.end())
      addedUsers.push_back(u.second);
  }
  // write resulting jsons
  WriteUsers(addedUsers, addedFile);
  WriteUsers(deletedUsers, deletedFile);
}
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{

  XAPI::UserCacheDiff usercachediff(argc, argv);
  if ( usercachediff.ParseOptions() == false )
  {
    return -1;
  }
  usercachediff.Run();
  
  return 0;
}
