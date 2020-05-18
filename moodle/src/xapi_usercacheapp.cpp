#include <xapi_usercacheapp.h>
using namespace std;
using json = nlohmann::json;
//////////////////////////////////////////////////////////////////////////////////////////
void
XAPI::User::Parse( nlohmann::json & v )
{
  
  name = v["name"];
  email = v["email"];
  // remove mailto: if it exists
  if ( email.substr(0,7) == string("mailto:") )
  {
    email.erase(email.begin(), email.begin()+7);
  }
  
  vector<string> tmproles = v["roles"];
  roles.insert(tmproles.begin(),tmproles.end());
  json tmp_userid=v["id"];
  if ( tmp_userid.is_null() == false )
    userid = tmp_userid;
  
}
//////////////////////////////////////////////////////////////////////////////////////////
XAPI::UserCacheApp::~UserCacheApp()
{

}
//////////////////////////////////////////////////////////////////////////////////////////
void
XAPI::UserCacheApp::ParseUsers( const std::string & file, UserMap & usermap)
{
  json content;
  ifstream f(file);
  // skip invalid streams
  if ( !f ) throw runtime_error(string("Cannot open users file '")+file+string("'"));
  
  f >> content;
  for ( auto & v : content )
  {
    User user;
    user.Parse(v);
    usermap[user.email] = user;
  }
}
//////////////////////////////////////////////////////////////////////////////////////////
void
XAPI::UserCacheApp::WriteUsers( const list<User> & users, const string & outfile  )
{

  json data = json::array();

  for ( auto & u : users )
  {
    json userJson = {
                     { "name",  u.name },
                     { "email", u.email },
                     { "roles", u.roles },
                     { "id", u.userid }
    };
    data.push_back(userJson);
  }
  ofstream out(outfile);
  out << data.dump();
  out.close();

}

void XAPI::UserCacheApp::Run()
{
  
}
