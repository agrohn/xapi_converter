#include <json.hpp>
#include <fstream>
#include <string>
#include <list>
#include <vector>
#include <set>
#include <iostream>
#include <sstream>
#include <boost/program_options.hpp>
////////////////////////////////////////////////////////////////////////////////
using namespace std;
using json = nlohmann::json;
namespace po = boost::program_options;
////////////////////////////////////////////////////////////////////////////////
struct User
{
  string name;
  string email;
  string userid;
  std::set<string> roles;
};
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
  boost::program_options::variables_map vm;
  boost::program_options::options_description desc;
  
  desc.add_options()
  ("out", po::value<string>(), "<file>.json Merged user JSON file")
  ("sources", po::value< vector<string> >()->multitoken(), "<file1.json> [<file2.json> ...] Array of user cache json files to be merged");
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);
  // need option to merge file
  // need option to diff files (new, deleted)
  // need option to define "new" and "old" files for comparison. -> boost program_options, here we come.
  vector<string> userCacheFiles;
  
  if ( vm.count("sources") > 0 )
  {
    userCacheFiles = vm["sources"].as<vector<string>>();
  }
  else
  {
    cerr << "You need at least one input file with --sources option.\n";
    return -1;
  }
  string outfile;
  
  if ( vm.count("out") == 1 )
  {
    outfile = vm["out"].as<string>();
  }
  else
  {
    cerr << "You need to have only a single output file via --out option!\n";
    return -1;
  }
  

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

  return 0;
}
