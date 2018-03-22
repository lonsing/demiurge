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
//   <http://www.iaik.tugraz.at/content/research/design_verification/demiurge/>
// or email the authors directly.
//
// ----------------------------------------------------------------------------

// -------------------------------------------------------------------------------------------
/// @file DepQBFExt.cpp
/// @brief Contains the definition of the class DepQBFExt.
// -------------------------------------------------------------------------------------------

#include "DepQBFExt.h"
#include "CNF.h"
#include "Options.h"
#include "FileUtils.h"
#include "StringUtils.h"

#include <sys/stat.h>

extern "C" {
  #include "aiger.h"
}

// -------------------------------------------------------------------------------------------
DepQBFExt::DepQBFExt(bool use_bloqqer, size_t timeout) :
    ExtQBFSolver(),
    use_bloqqer_(use_bloqqer),
    timeout_(timeout)
{
  path_to_deqqbf_ = Options::instance().getTPDirName() + "/depqbf/depqbf";
  path_to_bloqqer_ = Options::instance().getTPDirName() + "/bloqqer/bloqqer";
  path_to_qbfcert_ = Options::instance().getTPDirName() + "/qbfcert/qbfcert_min.sh";
  struct stat st;
  MASSERT(stat(path_to_deqqbf_.c_str(), &st) == 0, "DepQBF executable not found.");
  MASSERT(stat(path_to_qbfcert_.c_str(), &st) == 0, "QBFCert script not found.");
}

// -------------------------------------------------------------------------------------------
DepQBFExt::~DepQBFExt()
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
aiger* DepQBFExt::qbfCert(const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                        const CNF& cnf)
{
  dumpQBF(quantifier_prefix, cnf, in_file_name_);
  string call_command = path_to_qbfcert_ + " " + in_file_name_ + " > /dev/null 2>&1";
  int ret = system(call_command.c_str());
  ret = WEXITSTATUS(ret);
  MASSERT(ret == 0, "QBFCert terminated with strange exit code.");

  string qbfcert_out = in_file_name_ + ".aiger";
  aiger *res = aiger_init();
  const char *err = aiger_open_and_read_from_file (res, qbfcert_out.c_str());
  MASSERT(err == NULL, "Could not open AIGER file " << qbfcert_out << " (" << err << ").");
  cleanup();
  remove(qbfcert_out.c_str());
  return res;
}

// -------------------------------------------------------------------------------------------
string DepQBFExt::getSolverCommand() const
{
  ostringstream oss;
  oss << timeout_;
  if(use_bloqqer_ == true && timeout_ == 0)
    return path_to_bloqqer_ + " " + in_file_name_ + " 1000000 > " + out_file_name_ + " 2>&1";;
  if(use_bloqqer_ == true && timeout_ != 0)
    return path_to_bloqqer_ + " " + in_file_name_ + " " + oss.str() + " > " + out_file_name_ + " 2>&1";;
  return path_to_deqqbf_ + " " + in_file_name_ + " > " + out_file_name_;
}

// -------------------------------------------------------------------------------------------
string DepQBFExt::getSolverCommandModel() const
{
  ostringstream oss;
  oss << timeout_;
  if(use_bloqqer_ == true && timeout_ == 0)
    return path_to_bloqqer_ + " " + in_file_name_ + " 1000000 > " + out_file_name_ + " 2>&1";
  if(use_bloqqer_ == true && timeout_ != 0)
    return path_to_bloqqer_ + " " + in_file_name_ + " " + oss.str() + " > " + out_file_name_  + " 2>&1";
  return path_to_deqqbf_ + " --qdo " + in_file_name_ + " > " + out_file_name_;
}

// -------------------------------------------------------------------------------------------
bool DepQBFExt::parseModel(int ret, const vector<int> &get, vector<int> &model) const
{
  if(ret != 10 && ret != 20)
    throw DemiurgeException("Timeout or crash");

  if(use_bloqqer_)
  {
    if(ret == 20)
      return false;
    string answer;
    bool success = FileUtils::readFile(out_file_name_, answer);
    MASSERT(success, "Could not read result file.");
    vector<string> lines;
    StringUtils::splitLines(answer, lines, false);
    for(size_t l_cnt = 0; l_cnt < lines.size(); ++l_cnt)
    {
      int lit = 0, val = 0;
      istringstream iss(lines[l_cnt]);
      iss >> lit;
      if(!iss.good())
        continue;
      iss >> val;
      if(!iss.good())
        continue;
      if(Utils::contains(get, lit))
        model.push_back(val);
    }
    return true;
  }
  else
  {
    string answer;
    bool success = FileUtils::readFile(out_file_name_, answer);
    MASSERT(success, "Could not read result file.");
    if(answer.find("s cnf 0") == 0)
      return false;
    MASSERT(answer.find("s cnf 1") == 0, "Strange response from Solver.");
    size_t start = answer.find("V ", 0);
    while(start != string::npos)
    {
      start = start + 2;
      size_t end = answer.find(" 0", start);
      MASSERT(end != string::npos, "Strange response from Solver.")
      istringstream iss(answer.substr(start, end-start));
      int value;
      iss >> value;
      model.push_back(value);
      start = answer.find("V ", end);
    }
    return true;
  }
}
