/*
 * Parses moodle log from CSV and constructs xAPI statements out of it, sending them to LRS.
 * \author anssi grohn at karelia dot fi (c) 2018.
 */
#include <xapi_converter.h>
//https://nithinkk.wordpress.com/2017/03/16/learning-locker/
using namespace std;
using namespace boost;
using json = nlohmann::json;

////////////////////////////////////////////////////////////////////////////////
const string HOMEPAGE_URL_PREFIX = "https://moodle.karelia.fi/user/profile.php?id=";
const string VERB_URL_PREFIX = "http://adlnet.gov/expapi/verbs/viewed";

const std::map<std::string, std::string> supportedVerbs = {
  /*{ "added", ""},
    { "assigned",""},
    { "created",""},
    { "deleted",""},
    { "enrolled",""},
    { "ended", ""}, 
    { "graded", ""},
    { "posted", ""},
    { "searched", ""},
    { "started", ""},*/

    // it will be interpreted as an 'attempt' to (quiz, assignment)
    { "submitted", "http://adlnet.gov/expapi/verbs/attempted"},
    /*{ "uploaded", ""},
    { "launched", ""},
    { "subscribed", ""},
    { "unassigned", ""},
    { "unenrolled", ""},
    { "updated", ""},*/
    { "viewed","http://id.tincanapi.com/verb/viewed"}
};

// supported activity target types
const std::map<std::string, std::string> activityTypes = {
  { "collaborate", "https://moodle.karelia.fi/mod/collaborate/view.php?id="},
  { "quiz", "https://moodle.karelia.fi/mod/quiz/view.php?id=" },
  { "page", "https://moodle.karelia.fi/mod/page/view.php?id=" },
  { "resource", "https://moodle.karelia.fi/mod/resource/view.php?id="},
  { "url", "https://moodle.karelia.fi/mod/url/view.php?id=" },
  { "forum", "https://moodle.karelia.fi/mod/forum/view.php?id="},
  { "hsuforum", "https://moodle.karelia.fi/mod/hsuforum/view.php?id="},
  { "lti", "https://moodle.karelia.fi/mod/lti/view.php?id=" },
  { "course", "https://moodle.karelia.fi/course/view.php?id="},
  { "assignment", "https://moodle.karelia.fi/mod/assign/view.php?id=" },
  { "quiz", "https://moodle.karelia.fi/mod/quiz/view.php?id=" }
};

class Entry
{

public:
  struct tm when {0};
  json statement;

  // do we need these?
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
  void Parse(const std::string & line ) 
  {
    vector< string > vec;
    // break line into comma-separated parts.
    Tokenizer tok(line);
    vec.assign(tok.begin(),tok.end());

    ParseTimestamp(vec.at(0));
    username = vec.at(1);
    related_username = vec.at(2);
    context = vec.at(3);
    component = vec.at(4);
    event = vec.at(5);
    description = vec.at(6);
    // vec.at(7) = origin
    ip_address = vec.at(8);
  }
  
  string ToXapiStatement()
  {
// Actor is starting point for our parsing. 
    json actor;
    
    regex re("[Tt]he user with id '([[:digit:]]+)'( has)*( had)* (manually )*([[:alnum:]]+)(.*)");
    smatch match;
    //////////
    // seek user info
    if ( regex_search(description, match, re) )
    {
      string		userid = match[1];
      stringstream	homepage(HOMEPAGE_URL_PREFIX);
      homepage << userid;
      actor = {
	{"objectType", "Agent"},
	{"name", username},
	{"account",
	 {
	   {"name", userid }, 
	   {"homePage", homepage.str()}
	 }
	}
      };  
    }
    else
    {
      throw xapi_parsing_error("could not extract xapi statement for actor: " + description);
    }
    
    //////////
    // create verb jsonseek verb

    // Find verb from description
    json verb;
    string verbname = match[5];
    {
      auto it = supportedVerbs.find(verbname);
      if ( it == supportedVerbs.end())
	throw xapi_verb_not_supported_error(verbname);
      
      string verb_xapi_id = it->second;
      verb = {
	{"id", verb_xapi_id },
	{"display",
	 {
	   {"en-GB", verbname} // this will probably depend on subtype (see object)
	 }
	}
      };
    }
    
    /////////
    // Object
    // construct object (activity)
    
    json object;
    string details = match[6];
    smatch match_details;
    string tmp_id;
    string activityType;
    
    if ( verbname == "submitted" )
    {
      regex re_details("the (submission|attempt) with id '([[:digit:]]+)' for the (assignment|quiz) with course module id '([[:digit:]]+)'.*");
      if ( regex_search(details, match_details, re_details) )
      {
	//verb["display"]["en-GB"] = attempted?
	activityType = match_details[3]; // assignment / quiz
	tmp_id       = match_details[4];
	
      } else throw xapi_parsing_error("submitted: " + details);
    }
    else
    {
      // course, page, collaborate, etc.
      regex re_details("the '*([[:alnum:]]+)'*( activity)* with (course module id|id) '([[:digit:]]+)'.*");
      if ( regex_search(details, match_details, re_details) )
      {
	activityType = match_details[1];
	tmp_id = match_details[4];
      } else throw xapi_parsing_error("Cannot make sense of: " + details);
    }
    
    auto it = activityTypes.find(activityType);
    if ( it == activityTypes.end()) throw xapi_activity_type_not_supported_error(activityType);
    
    string object_id = it->second + tmp_id;
    
    object = {
      { "objectType", "Activity"},
      { "id", object_id },
      { "definition",
	{
	  /*{ "description", { "en-GB", ""}},*/
	  { "type" , "http://id.tincanapi.com/activitytype/school-assignment"},
	  { "interactionType", "other" }
	}
      }
    };

    object["definition"]["name"] =  {
      {"en-GB", context}
    };   
    
    // construct full xapi statement
    statement["actor"] = actor;

    statement["verb"] = verb;
    statement["timestamp"] = GetTimestamp();
    statement["object"] = object;
    return statement.dump();
  }
};

int main( int argc, char **argv)
{
    using namespace std;
    using namespace boost;
    string data(argv[1]);
    string learningLockerURL;
 
    /// \TODO use boost options. https://coderwall.com/p/y3xnxg/using-getopt-vs-boost-in-c-to-handle-arguments
    // check if url was specififed 
    if ( argc > 2 )
    {
      learningLockerURL = argv[2];
    }
    else
    {
      cout << "alright, dry run - not sending statements.\n";
    }
	 
    vector<string> statements;
    
    ifstream in(data.c_str());
    if (!in.is_open()) return 1;



    string line;
    
    // skip first header line 
    getline(in,line);
    std::vector<Entry> unsupported;

    while (getline(in,line))
    {
	Entry e;

	try
	{
	  e.Parse(line);
	  statements.push_back(e.ToXapiStatement());
	}
	catch ( xapi_activity_type_not_supported_error & ex )
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
    if ( learningLockerURL.size() > 0 )
    {
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
    }
    /*
    cout << "unsupported entries:\n";
    cout << "--------------------\n";
    
    for(auto e : unsupported )
    {
      cout << e.description  << "\n";
      }
*/
    return 0;
}
