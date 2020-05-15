#include <json.hpp>
#include <fstream>
#include <string>
#include <list>
#include <vector>
#include <set>
#include <iostream>
#include <sstream>
////////////////////////////////////////////////////////////////////////////////
using namespace std;
using json = nlohmann::json;
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
  
  if ( argc < 3 )
  {
    cout << "Usage:\n"
         << argv[0] << " <merged-output-file>.json <usercache-file1>.json [<usercache-file2>.json  ... ] \n";
    return 0; 
  }
  string outfile = argv[1];
  
  list<string> userCacheFiles;
  std::map<string,User> users;
  // get all files into list
  for( int i=2;i<argc;i++)
  {
    userCacheFiles.push_back(argv[i]);
  }

  for( auto & userFile : userCacheFiles )
  {
    cerr << "opening file " << userFile << "\n";
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
