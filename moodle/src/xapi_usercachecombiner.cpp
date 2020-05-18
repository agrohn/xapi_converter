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
#include <xapi_usercachecombiner.h>
////////////////////////////////////////////////////////////////////////////////
using namespace std;
using json = nlohmann::json;
namespace po = boost::program_options;
////////////////////////////////////////////////////////////////////////////////
XAPI::UserCacheCombiner::UserCacheCombiner(int argc, char **argv )
{
  desc.add_options()
  ("out", po::value<string>(), "<file>.json Merged user JSON file")
  ("sources", po::value< vector<string> >()->multitoken(), "<file1.json> [<file2.json> ...] Array of user cache json files to be merged");
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);
}
////////////////////////////////////////////////////////////////////////////////
bool XAPI::UserCacheCombiner::ParseOptions()
{
  if ( vm.count("sources") > 0 )
  {
    userCacheFiles = vm["sources"].as<vector<string>>();
  }
  else
  {
    cerr << "You need at least one input file with --sources option.\n\n";
    cerr << desc << "\n";
    return false;
  }
  if ( vm.count("out") == 1 )
  {
    outfile = vm["out"].as<string>();
  }
  else
  {
    cerr << "You need to have only a single output file via --out option!\n";
    cerr << desc << "\n";
    return false;
  }
  return true;
}
////////////////////////////////////////////////////////////////////////////////
void XAPI::UserCacheCombiner::Run()
{
  std::map<string,User> users;
  for( auto & userFile : userCacheFiles )
  {
    json content;
    ifstream f(userFile);
    // skip invalid streams
    if ( !f ) continue;
    
    f >> content;
    for ( auto & v : content )
    {
      User user;
      user.name = v["name"];
      user.email = v["email"];
      // remove mailto: if it exists
      if ( user.email.substr(0,7) == string("mailto:") )
      {
        user.email.erase(user.email.begin(), user.email.begin()+7);
      }
      
      vector<string> roles = v["roles"];
      user.roles.insert(roles.begin(),roles.end());
      json userid=v["id"];
      if ( userid.is_null() == false )
        user.userid = userid;

      // does entry for this user already exist
      auto it = users.find(user.email);
      if ( it == users.end())
      {
        users[user.email] = user;
      }
      else // exists, so merge
      {
        User & orig = it->second;
        // set contains always only unique values, so we can merge directly
        orig.roles.insert(user.roles.begin(), user.roles.end());

        // does name need updating
        if ( orig.name == "-" || orig.name.size() == 0 )
        {
          orig.name = user.name;
        }
        // update original user id if found
        if ( orig.userid.empty() )
        {
          orig.userid = user.userid;
        }
        else if ( user.userid.empty() == false )
        {
          // some sanity check
          if ( orig.userid != user.userid )
          {
            stringstream ss;
            ss << "Mixed moodle userids for same email! ";
            ss << orig.userid << " vs " << user.userid;
            throw runtime_error(ss.str());
          }
        }
      }
    }
  }
  
  // create resulting json 
  json merged = json::array();
  
  for ( auto & user : users )
  {
    User & u = user.second;
    json userJson = {
                     { "name",  u.name },
                     { "email", u.email },
                     { "roles", u.roles },
                     { "id", u.userid }
    };
    merged.push_back(userJson);
  }
  // write resulting json
  ofstream out(outfile);
  out << merged.dump();
  out.close();
}
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{

  XAPI::UserCacheCombiner combiner(argc,argv);
  if ( combiner.ParseOptions() == false )
    return 1;
  combiner.Run();
  return 0;
}
