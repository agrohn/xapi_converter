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

struct CourseAssignmentRecord
{
  string courseId;
  string courseName;
  map<string,AssignmentRecord> assignments;
};

std::map<string, CourseAssignmentRecord> mapAssignmentStatus;

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

  ifstream studentsFile(argv[4]);
  json students;
  studentsFile >> students;
  studentsFile.close();
  
  // collect all assignments and duedates 
  for( auto & c : assignments )
  {
    try
    {
      string course     = c["courseName"];
      string courseId     = c["courseId"];
      for( auto & a : c["data"] )
      {
	string duedate    = a["duedate"].is_null() ? "" : a["duedate"];
	string assignment = a["name"];

	mapAssignmentStatus[courseId].courseName = course;
	mapAssignmentStatus[courseId].courseId = courseId;
	mapAssignmentStatus[courseId].assignments[assignment] = { duedate, {} } ;
      }
    }
    catch ( std::exception & ex )
    {
      cerr << ex.what() << "\n";
      cerr << "Failure at assignments: " << c << "\n";
      return 1;
    }
  }

  
  // collect all students to course assignments

  for( auto & c : students )
  {
    try
    {
      if ( c["courseName"].is_null()) continue;
      
      string course = c["courseName" ];
      string courseId = c["courseId"];
      for( auto & assignmentIt : mapAssignmentStatus[courseId].assignments )
      {
	string assignment = assignmentIt.first;	
	for ( auto & s : c["data"] )
	{
	  if ( s["_id"]["Opiskelija"].is_null()) continue;
	  string student = s["_id"]["Opiskelija"];
	  string email = s["_id"]["Email"];
	
	  mapAssignmentStatus[courseId].assignments[assignment].students[email].name = student;
	  mapAssignmentStatus[courseId].assignments[assignment].students[email].email = email;
	}
      }
    }
    catch (std::exception & ex )
    {
      cerr << ex.what() << "\n";
      cerr << "Failure at students : " << c << "\n";
      return 1;
    }
      
  }

  
  // set submissions for each student
  for ( auto & c: submissions )
  {
    try
    {
      if ( c["courseName"].is_null()) continue;
      
      string course = c["courseName"];
      string courseId = c["courseId"];
      for ( auto & s : c["data"] )
      {

	string email = s["_id"]["email"];
	string assignment = s["_id"]["task"];
	string submissionsDate = s["_id"]["date"];
	mapAssignmentStatus[courseId].assignments[assignment].students[email].submissions.push_back( submissionsDate);
      }
    }
    catch ( std::exception & ex )
    {
      cerr << ex.what() << "\n";
      cerr << "Failure at students : " << c << "\n";
      return 1;
    }
  }
  // set all scores for each student
  for( auto & c : scores )
  {
    try {
      
      if ( c["courseName"].is_null() ) continue;
      
      string course = c["courseName"];
      string courseId = c["courseId"];
      
      for ( auto & s : c["data"] )
      {
	if ( s["_id"]["Opiskelija"].is_null()) continue;
	string student = s["_id"]["Opiskelija"];
	string email = s["_id"]["Email"];
	string assignment = s["_id"]["Tehtava"];
	string gradeDate = s["_id"]["Timestamp"];
	float gradeScore = s["_id"]["Pisteet"];
	
	mapAssignmentStatus[courseId].assignments[assignment].students[email].grades.push_back( { gradeDate, gradeScore } );
      }
      
    }
    catch( std::exception & ex )
    {
      cerr << ex.what() << "\n";
      cerr << "Failure at students : " << c << "\n";
      return 1;
    }
  }

  
  json result = json::array();
  for( auto & cit : mapAssignmentStatus )
  {
    string courseId = cit.first;
    string course = cit.second.courseName;
    
    json coursePart = {
      { "courseName", course },
      { "courseId", courseId },
      { "assignments", json::array() }
    };
    
    for( auto & ait : cit.second.assignments )
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

  ofstream output(argv[5]);
  output << result;
  return 0;
}

