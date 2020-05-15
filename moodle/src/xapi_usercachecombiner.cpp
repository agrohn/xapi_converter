#include <json.hpp>
#include <fstream>
#include <string>
#include <list>
#include <vector>
#include <set>
#include <iostream>
////////////////////////////////////////////////////////////////////////////////
using namespace std;
using json = nlohmann::json;
////////////////////////////////////////////////////////////////////////////////
struct User
{
  string Opiskelija;
  string Email;
  string UserID;
  std::set<string> Roles;
};
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{

  list<string> userCacheFiles;
  std::map<string,User> users;
  // get all files into list
  for( int i=1;i<argc;i++)
  {
    userCacheFiles.push_back(argv[i]);
  }

  for( auto & userFile : userCacheFiles )
  {
    json content;
    ifstream f(userFile);
    f >> content;
    for ( auto & v : content )
    {
      User user;
      user.Opiskelija = v["_id"]["Opiskelija"];
      user.Email = v["_id"]["Email"];
      vector<string> roles = v["_id"]["Roles"];
      user.Roles.insert(roles.begin(),roles.end());
      user.UserID = v["_id"]["UserID"];

      // does entry for this user already exist
      auto it = users.find(user.Email);
      if ( it == users.end())
      {
        users[user.Email] = user;
      }
      else // exists, so merge
      {
        User & orig = it->second;
        // set contains always only unique values, so we can merge directly
        orig.Roles.insert(user.Roles.begin(), user.Roles.end());

        // does name need updating
        if ( orig.Opiskelija == "-" || orig.Opiskelija.size() == 0 )
        {
          orig.Opiskelija = user.Opiskelija;
        }
        // update original user id if found
        if ( orig.UserID.empty() )
        {
          orig.UserID = user.UserID;
        }
        else
        {
          // some sanity check
          if ( orig.UserID != user.UserID )
            throw runtime_error("Mixed moodle userids for same email!");
        }
      }
    }
  }

  

  json merged = json::array();
  
  for ( auto & user : users )
  {
    User & u = user.second;
    json userJson = {
                     { "Opiskelija",  u.Opiskelija },
                     { "Email", u.Email },
                     { "Roles", u.Roles },
                     { "UserID", u.UserID }
    };
    merged.push_back(userJson);
    
  }
  ofstream out("merged-users.json");
  out << merged.dump();
  out.close();

  return 0;
}
