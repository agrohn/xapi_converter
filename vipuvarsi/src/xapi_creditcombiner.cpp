#include <json.hpp>
#include <fstream>
#include <string>
#include <iostream>
using namespace std;
using json = nlohmann::json;
int main(int argc, char **argv)
{


  ifstream nullMarksFile(argv[1]);
  json nullMarks;
  nullMarksFile >> nullMarks;
  nullMarksFile.close();

  ifstream creditsFile(argv[2]);
  json credits;
  creditsFile >> credits;
  creditsFile.close();
  std::map<string, json> nullMarksByCourseId;
  
  for ( auto & e : nullMarks )
  {
    string id = e["courseid"];
    nullMarksByCourseId[id] = e;
  }
  //cerr << "null marks count : " << nullMarksByCourseId.size() << "\n";
  int count = 0;
  for ( auto & entry : credits )
  {
    //cerr << "new student : " << entry["studentid"] << "\n";

    std::vector<json> courseList = entry["courses"];
    map<string, int> studentCourseEntries;
    
    for ( auto & tmp : courseList )
    {
      string cid = tmp["courseid"];
      studentCourseEntries[cid] = 1;
    }

    for ( auto & nullCourseEntry : nullMarksByCourseId )
    {
      
      //cerr << "checking does exist:" << nullCourseEntry.first << "..";
      // if entry does not exists 
      if ( studentCourseEntries.find(nullCourseEntry.first) ==
           studentCourseEntries.end())
      {
        

        courseList.push_back( nullCourseEntry.second );
      }
      else
      {
        //cerr << "entry " << nullCourseEntry.first << " already there, skipping\n";
      }
    }
    //cerr << "non-matching found: " << count << "\n";
    // update originals
    sort( courseList.begin(), courseList.end(), [] ( json & a, json & b ) -> bool {
                                                  string aStr = a["courseid"];
                                                  string bStr = b["courseid"];
                                                  return (aStr.compare(bStr) <= 0);
                                                });
    entry["courses"] = courseList;
    /*if ( nullMarksByCourseId.size() != courseList.size())
    {
      cerr << "something fishy here, " << nullMarksByCourseId.size() << " vs. " << courseList.size() << "\n";
      return 1;
      }*/
    entry["studentid"] = count++;

  }
  ofstream out("tmp-result.json");
  out << credits.dump();
  out.close();

  return 0;
}
