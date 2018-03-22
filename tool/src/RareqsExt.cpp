// ----------------------------------------------------------------------------
// Copyright (c) 2013-2014 by Graz University of Technology and
//                            Johannes Kepler University Linz
//
// This is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, see
// <http://www.gnu.org/licenses/>.
//
// For more information about this software see
//   <http://www.iaik.tugraz.at/content/research/design_verification/others/>
// or email the authors directly.
//
// ----------------------------------------------------------------------------

// -------------------------------------------------------------------------------------------
/// @file RareqsExt.cpp
/// @brief Contains the definition of the class RareqsExt.
// -------------------------------------------------------------------------------------------

#include "RareqsExt.h"
#include "Options.h"
#include "FileUtils.h"

#include <sys/stat.h>

// -------------------------------------------------------------------------------------------
RareqsExt::RareqsExt() : ExtQBFSolver()
{
  path_to_rareqs_ = Options::instance().getTPDirName() + "/rareqs/rareqs";
  struct stat st;
  MASSERT(stat(path_to_rareqs_.c_str(), &st) == 0, "RAReQS executable not found.");
}

// -------------------------------------------------------------------------------------------
RareqsExt::~RareqsExt()
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
string RareqsExt::getSolverCommand() const
{
  return path_to_rareqs_ + " " + in_file_name_ + " > " + out_file_name_ + " 2>&1";
}

// -------------------------------------------------------------------------------------------
string RareqsExt::getSolverCommandModel() const
{
  return path_to_rareqs_ + " " + in_file_name_ + " > " + out_file_name_ + " 2>&1";
}

// -------------------------------------------------------------------------------------------
bool RareqsExt::parseModel(int ret, vector<int> &model) const
{
  MASSERT(ret == 20 || ret == 10, "Solver terminated with strange exit code.");

  string answer;
  bool success = FileUtils::readFile(out_file_name_, answer);
  MASSERT(success, "Could not read result file.");
  if(answer.find("s cnf 0") != string::npos)
    return false;
  MASSERT(answer.find("s cnf 1") != string::npos, "Strange response from Solver.");
  size_t start = answer.find("V ", 0);
  size_t end = answer.find("\n", start + 1);
  string model_str = answer.substr(start+2, end-start-2);
  istringstream iss(model_str);
  int val;
  while(iss.good())
  {
    iss >> val;
    model.push_back(val);
  }
  return true;
}

