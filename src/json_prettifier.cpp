#include <json.hpp>
#include <fstream>
#include <string>
#include <stdexcept>
#include <iostream>
////////////////////////////////////////////////////////////////////////////////
using namespace std;
using json = nlohmann::json;
////////////////////////////////////////////////////////////////////////////////
int main( int argc, char **argv )
{
  for ( int i=1;i<argc;i++)
  {
    ifstream file(argv[i]);
    if ( !file.is_open()) throw runtime_error(std::string("Cannot open file ")+std::string(argv[i]));
    json obj;
    file >> obj;
    cout << obj.dump(4) << "\n";
  }
  return 0;
}
////////////////////////////////////////////////////////////////////////////////
