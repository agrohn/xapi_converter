/*
  This file is part of xapi_converter.
  Copyright (C) 2018-2021 Anssi Gröhn
  
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
#include <map>
#include <list>
#include <string>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
using json = nlohmann::json;
using namespace std;

struct GradeRecord
{
  string date;
  float grade;
};

struct StudentAssignmentRecord
{
  string name;
  string email;
  std::list<string> submissions;
  std::list<GradeRecord> grades;
};

struct AssignmentRecord
{
  string duedate;
  std::map<string, StudentAssignmentRecord> students;
};





map<string, map<string,AssignmentRecord>> mapAssignmentStatus;

int main( int argc, char **argv )
{
  ifstream submissionsFile(argv[1]);
  json submissions;
  submissionsFile >> submissions;
  submissionsFile.close();
  
  ifstream assignmentsFile(argv[2]);
  json assignments;
  assignmentsFile >> assignments;
  assignmentsFile.close();
  
  ifstream scoresFile(argv[3]);
  json scores;
  scoresFile >> scores;
  scoresFile.close();

  // collect all assignments and duedates 
  for( auto & a : assignments )
  {

    try
    {
      string course     = a["courseName"];
      string duedate    = a["duedate"].is_null() ? "" : a["duedate"];
      string assignment = a["name"];
      mapAssignmentStatus[course][assignment] = { duedate, {} } ;
    }
    catch (...)
    {
      cerr << a << "\n";
    }
  }
  
  // set submissions to records
  for ( auto & e: submissions )
  {
    try {
      if ( e["_id"]["courseName"].is_null()) continue;
      
      string course = e["_id"]["courseName"];
      string student = e["_id"]["student"];
      string email = e["_id"]["email"];
      string assignment = e["_id"]["task"];
      string submissionsDate = e["_id"]["date"];
      
      mapAssignmentStatus[course][assignment].students[email].name = student;
      mapAssignmentStatus[course][assignment].students[email].email = email;
      mapAssignmentStatus[course][assignment].students[email].submissions.push_back( submissionsDate);
    }
    catch (...)
    {
      cerr << e << "\n";
    }
  }
  
  for( auto & e : scores )
  {
    try {
      if ( e["coursename"].is_null() ) continue;
      string course = e["coursename"];
      string student = e["Opiskelija"];
      string email = e["Email"];
      string assignment = e["Tehtava"];
      string gradeDate = e["Timestamp"];
      float gradeScore = e["Pisteet"];
      mapAssignmentStatus[course][assignment].students[email].grades.push_back( { gradeDate, gradeScore } );
    }
    catch( ...)
    {
      cerr << e << "\n";
    }
  }

  
  json result = json::array();
  for( auto & cit : mapAssignmentStatus )
  {

    string course = cit.first;

    json coursePart = {
      { "courseName", course },
      { "assignments", json::array() }
    };
    
    for( auto & ait : cit.second )
    {
      string assignment = ait.first;
      string duedate = ait.second.duedate;
      json assignmentPart = {
	 { "assignmentName", assignment},
	 { "duedate", duedate },
	 { "students", json::array() }
      };
	
      for( auto & sit : ait.second.students)
      {
	string student = sit.second.name;
	string email = sit.second.email;

	json studentPart = {
	  { "studentName", student },
	  { "email", email },
	  { "submissions", sit.second.submissions.size() },
	  { "latest_submission_date", "" },
	  { "latest_score_date",  "" },
	  { "best_score", -1.0}
	};
	

	for( auto & submissionDate : sit.second.submissions )
	{
	  if ( submissionDate > studentPart["latest_submission_date"] )
	  {
	    studentPart["latest_submission_date"] = submissionDate;
	  }
	}
	
	for ( auto & gradeRecord : sit.second.grades )
	{
	  string gradeDate = gradeRecord.date;
	  float score = gradeRecord.grade;
	  if ( score > studentPart["best_score"])
	  {
	    studentPart["best_score"] = score;	    
	    studentPart["latest_score_date"] = gradeDate;
	  }
	}
	assignmentPart["students"].push_back(studentPart);
      }
      coursePart["assignments"].push_back(assignmentPart);
    }
    result.push_back(coursePart);
  }

  ofstream output(argv[4]);
  output << result;
  return 0;
}

