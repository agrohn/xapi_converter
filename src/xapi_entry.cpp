#include <xapi_entry.h>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
using namespace boost;
using json = nlohmann::json;
using namespace std;
////////////////////////////////////////////////////////////////////////////////
XAPI::Entry::Entry() {}
////////////////////////////////////////////////////////////////////////////////
XAPI::Entry::~Entry() {}
////////////////////////////////////////////////////////////////////////////////
/// Returns string as yyyy-mm-ddThh:mmZ
std::string
XAPI::Entry::GetTimestamp() const
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
////////////////////////////////////////////////////////////////////////////////
