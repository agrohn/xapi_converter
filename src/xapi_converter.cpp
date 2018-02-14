/*
 * Parses moodle log from CSV and constructs xAPI statements out of it, sending them to LRS.
 * \author anssi grohn at karelia dot fi (c) 2018.
 */
#include <iostream>     
#include <fstream>      
#include <vector>
#include <string>
#include <algorithm>    
#include <iterator>     
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <regex>
#include <json.hpp>
#include <stdexcept>

#include <cstdlib>
#include <cerrno>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
//https://nithinkk.wordpress.com/2017/03/16/learning-locker/
using namespace std;
using namespace boost;
using json = nlohmann::json;

static const unsigned char base64_table[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
* base64_encode - Base64 encode
* @src: Data to be encoded
* @len: Length of the data to be encoded
* @out_len: Pointer to output length variable, or %NULL if not used
* Returns: Allocated buffer of out_len bytes of encoded data,
* or empty string on failure
*/
std::string base64_encode(const unsigned char *src, size_t len)
{
    unsigned char *out, *pos;
    const unsigned char *end, *in;

    size_t olen;

    olen = 4*((len + 2) / 3); /* 3-byte blocks to 4-byte */

    if (olen < len)
        return std::string(); /* integer overflow */

    std::string outStr;
    outStr.resize(olen);
    out = (unsigned char*)&outStr[0];

    end = src + len;
    in = src;
    pos = out;
    while (end - in >= 3) {
        *pos++ = base64_table[in[0] >> 2];
        *pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
        *pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
        *pos++ = base64_table[in[2] & 0x3f];
        in += 3;
    }

    if (end - in) {
        *pos++ = base64_table[in[0] >> 2];
        if (end - in == 1) {
            *pos++ = base64_table[(in[0] & 0x03) << 4];
            *pos++ = '=';
        }
        else {
            *pos++ = base64_table[((in[0] & 0x03) << 4) |
                (in[1] >> 4)];
            *pos++ = base64_table[(in[1] & 0x0f) << 2];
        }
        *pos++ = '=';
    }

    return outStr;
}

////////////////////////////////////////////////////////////////////////////////
class xapi_parsing_error : public std::runtime_error {
private:
  std::string verb;
public:
  xapi_parsing_error(const std::string & tmp ) : std::runtime_error( tmp)
 {
    verb = tmp;
  }
  const std::string & get_verb() const { return verb; }
};
////////////////////////////////////////////////////////////////////////////////
class xapi_verb_not_supported_error : public xapi_parsing_error {
public:
  xapi_verb_not_supported_error(const std::string & tmp ) : xapi_parsing_error( "Verb '" + tmp + "' not supported"){}
};
////////////////////////////////////////////////////////////////////////////////
class xapi_activity_not_supported_error : public xapi_parsing_error {
public:
  xapi_activity_not_supported_error(const std::string & tmp ) : xapi_parsing_error( "Activity '" + tmp + "' not supported"){}
};
////////////////////////////////////////////////////////////////////////////////
const string HOMEPAGE_URL_PREFIX = "https://moodle.karelia.fi/user/profile.php?id=";
const string VERB_URL_PREFIX = "http://adlnet.gov/expapi/verbs/viewed";

const std::map<std::string, std::string> verbs = {
  /*{ "added", ""},
    { "assigned",""},
    { "created",""},
    { "deleted",""},
    { "enrolled",""},
    { "ended", ""}, 
    { "graded", ""},
    { "posted", ""},
    { "searched", ""},
    { "started", ""},
    { "submitted", ""},
    { "uploaded", ""},
    { "viewed", ""},
    { "launched", ""},
    { "subscribed", ""},
    { "unassigned", ""},
    { "unenrolled", ""},
    { "updated", ""},*/
  { "viewed","http://id.tincanapi.com/verb/viewed"},
};

// supported activity target types
const std::map<std::string, std::string> activities = {
  { "collaborate", "https://moodle.karelia.fi/mod/collaborate/view.php?id="},
  { "quiz", "https://moodle.karelia.fi/mod/quiz/view.php?id=" },
  { "page", "https://moodle.karelia.fi/mod/page/view.php?id=" },
  { "resource", "https://moodle.karelia.fi/mod/resource/view.php?id="},
  { "url", "https://moodle.karelia.fi/mod/url/view.php?id=" },
  { "forum", "https://moodle.karelia.fi/mod/forum/view.php?id="},
  { "hsuforum", "https://moodle.karelia.fi/mod/hsuforum/view.php?id="},
  { "lti", "https://moodle.karelia.fi/mod/lti/view.php?id=" },
  { "course", "https://moodle.karelia.fi/course/view.php?id="}
};

class Entry
{

public:
  struct tm when {0};
  string username;
  string related_username;
  string userid;
  string related_userid;
  string context; // generally the course
  string component;
  string event;
  string verb; // xapi-related
  string description; // moodle event id description
  string ip_address;

  string verbname;
  string verbid;

  string object_id;
  string object_type;
  
  // parses time structure from string.
  void ParseTimestamp(const string & strtime)
  {
    // this would be better, but strftime does not support single-digit days without some kind of padding.
    //stringstream ss(strtime);
    //ss >> std::get_time(&when, "%d.%m.%Y %H:%M:%S");

    std::vector<string> tokens;
    boost::split( tokens, strtime, boost::is_any_of(". :"));
    {
      stringstream ss;
      ss << tokens.at(0);
      if ( !(ss >> when.tm_mday) ) throw runtime_error("Conversion error, day");
    }
    {
      stringstream ss;
      ss << tokens.at(1);
      if ( !(ss >> when.tm_mon)  ) throw runtime_error("Conversion error, mon");
    }
    {
      stringstream ss;
      ss << tokens.at(2);
      if ( !(ss >> when.tm_year)  ) throw runtime_error("Conversion error, year");
    }

    {
      stringstream ss;
      ss << tokens.at(3);
      if ( !(ss >> when.tm_hour)  ) throw runtime_error("Conversion error, hour");
    }

    {
      stringstream ss;
      ss << tokens.at(4);
      if ( !(ss >> when.tm_sec)  ) throw runtime_error("Conversion error, second");
    }

  }
  /// Returns string as yyyy-mm-ddThh:mmZ
  string GetTimestamp() const
  {
    stringstream ss;
    ss << when.tm_year ;
    ss << "-" ;
    ss << std::setfill('0') << std::setw(2) << when.tm_mon;
    ss << "-";
    ss << std::setfill('0') << std::setw(2) << when.tm_mday;
    ss << "T";
    ss << std::setfill('0') << std::setw(2) << when.tm_hour;
    ss << ":";
    ss << std::setfill('0') << std::setw(2) << when.tm_sec;
    ss << "Z";
    return ss.str();
  }
  void ParseDetails() 
  {

    string regexString = "[Tt]he user with id '([[:digit:]]+)'( has)*( had)* (manually )*([[:alnum:]]+) the '*([[:alnum:]]+)'*( activity)* with (course module id|id) '([[:digit:]]+)'.*";

    regex re(regexString);
    smatch match;
    // "course with " + "id '1191'"
    // "'page' activity with" +" course module id '54973'"
    
    //////////
    // seek user info
    if ( regex_search(description, match, re) )
    {
      int i=0;
      for( auto m : match )
      {
	cout << i++ << " : " << m  << "\n";
      }
      cerr << "userid " << match[1] << "\n";
      userid = match[1];
    }
    else
    {
      throw xapi_parsing_error("could not extract xapi statement: " + description);
    }
    
    //////////
    // seek verb
    // Find verb from description
    string verbstring = match[5];
    {
      auto it = verbs.find(verbstring);
      if ( it == verbs.end()) throw xapi_verb_not_supported_error(verbstring);
      verbname = it->first;
      verbid = it->second;
    }


    /////////
    // Object
    
    // course, page, collaborate, etc.
    string activity = match[6];
    // match 7 = activity or just course view etc.
    //bool isActualActivity = (match[7] == " activity");
    // match 8 = module or course module id
    //bool isCourseModule = (match[8] != "id");
    // match 9 = activity target identifier
    string tmp_id = match[9];

    
    auto it = activities.find(activity);
    if ( it == activities.end()) throw xapi_activity_not_supported_error(activity);
    object_type = "Activity";
    object_id = it->second + tmp_id;

    
  }
  
  
  string ToXapiStatement() const
  {
    json j;

    // construct actor statement    
    stringstream homepage;
    homepage << HOMEPAGE_URL_PREFIX << userid;
    json actor = {
      {"objectType", "Agent"},
      {"name", username},
      {"account",
       {
	 {"name", userid },
	 {"homePage", homepage.str()}
       }
      }
    };
  
     
    // create verb json
    json verb = {
      {"id", verbid },
      {"display",
       {
	 {"en-GB", verbname}
       }
      }
    };
    // construct object (activity)
    json object = {
      { "objectType", object_type},
      { "id", object_id }

    };
    
    object["definition"]["name"] =  {
      {"en-GB", context}
    };    
    // construct full xapi statement
    j["actor"] = actor;
    j["verb"] = verb;
    j["timestamp"] = GetTimestamp();
    j["object"] = object;
    return j.dump();
  }
};

int main( int argc, char **argv)
{
    using namespace std;
    using namespace boost;
    string data(argv[1]);
    string learningLockerURL(argv[2]);
    vector<string> statements;
    
    ifstream in(data.c_str());
    if (!in.is_open()) return 1;

    typedef tokenizer< escaped_list_separator<char> > Tokenizer;
    vector< string > vec;
    string line;
    
    // skip first header line 
    getline(in,line);
    std::vector<Entry> unsupported;

    while (getline(in,line))
    {
	Entry e;
	// break line into comma-separated parts.
	Tokenizer tok(line);
        vec.assign(tok.begin(),tok.end());
	try
	{
	  e.ParseTimestamp(vec.at(0));
	  e.username = vec.at(1);
	  e.related_username = vec.at(2);
	  e.context = vec.at(3);
	  e.component = vec.at(4);
	  e.event = vec.at(5);
	  e.description = vec.at(6);
	  // vec.at(7) = origin
	  e.ip_address = vec.at(8);

	  e.ParseDetails();
	  statements.push_back(e.ToXapiStatement());
	}
	catch ( xapi_activity_not_supported_error & ex )
	{
	  cerr << ex.what() << "\n";
	  unsupported.push_back(e);
	  //return 1;
	}
	catch ( xapi_parsing_error & ex )
	{
	  cerr << ex.what() << "\n";
	}
	catch ( std::out_of_range & ex )
	{
	  cout << "could not parse time\n";
	}
	// vector now contains strings from one row, output to cout here
        
	
        cout << "\n----------------------" << endl;
    }

    // send XAPI statements in POST
    try {
      curlpp::Cleanup cleaner;
      curlpp::Easy request;

      ifstream loginDetails("data/login.json");
      json login;
      loginDetails >> login;

      
      string url = learningLockerURL+"/data/xAPI/statements";
      cerr << "learninglockerurl" << url << "\n";
      request.setOpt(new curlpp::options::Url(url)); 
      request.setOpt(new curlpp::options::Verbose(true)); 
      std::list<std::string> header;

      
      //https://stackoverflow.com/questions/25852551/how-to-add-basic-authentication-header-to-webrequest#25852562
      string tmp;
      string key = login["key"];
      string secret = login["secret"];
      {
	stringstream ss;
	ss << key << ":" << secret;
	tmp = ss.str();
      }
      string auth = "Basic " + base64_encode(reinterpret_cast<const unsigned char *>(tmp.c_str()), tmp.size());
      header.push_back("Authorization: " + auth);
      
      header.push_back("X-Experience-API-Version: 1.0.3");
      header.push_back("Content-Type: application/json; charset=utf-8");
      request.setOpt(new curlpp::options::HttpHeader(header)); 

      stringstream ss;
      ss << "[";
      auto last_element = statements.end()-1;
      for_each(statements.begin(), last_element, [&ss] (auto & s) {
	  ss << s << ",";
	});
      ss << *last_element << "]";
      
      request.setOpt(new curlpp::options::PostFields(ss.str()));
      request.setOpt(new curlpp::options::PostFieldSize(ss.str().size()));

      cerr << ss.str() << "\n";
      
      request.perform();

    }
    catch ( curlpp::LogicError & e ) {
      std::cout << e.what() << std::endl;
    }
    catch ( curlpp::RuntimeError & e ) {
      std::cout << e.what() << std::endl;
    }

    cout << "unsupported entries:\n";
    cout << "--------------------\n";
    // display unsupported entries
    for(auto e : unsupported )
    {
      cout << e.description  << "\n";
    }
    return 0;
}
