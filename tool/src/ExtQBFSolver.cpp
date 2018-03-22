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
/// @file ExtQBFSolver.cpp
/// @brief Contains the definition of the class ExtQBFSolver.
// -------------------------------------------------------------------------------------------

#include "ExtQBFSolver.h"
#include "VarManager.h"
#include "CNF.h"
#include "Options.h"
#include "FileUtils.h"


// -------------------------------------------------------------------------------------------
ExtQBFSolver::ExtQBFSolver() : QBFSolver()
{
  ostringstream unique_string;
  unique_string << this;
  string temp_dir = Options::instance().getTmpDirName();
  in_file_name_ = Options::instance().getUniqueTmpFileName("qbf_query") + ".qdimacs";
  out_file_name_ = Options::instance().getUniqueTmpFileName("qbf_answer") + ".out";
}

// -------------------------------------------------------------------------------------------
ExtQBFSolver::~ExtQBFSolver()
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
bool ExtQBFSolver::isSat(const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                         const CNF& cnf)
{
  dumpQBF(quantifier_prefix, cnf, in_file_name_);
  int ret = system(getSolverCommand().c_str());
  ret = WEXITSTATUS(ret);
  bool sat = parseAnswer(ret);
  cleanup();
  return sat;
}

// -------------------------------------------------------------------------------------------
bool ExtQBFSolver::isSat(const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                         const CNF& cnf)
{
  dumpQBF(quantifier_prefix, cnf, in_file_name_);
  int ret = system(getSolverCommand().c_str());
  ret = WEXITSTATUS(ret);
  bool sat = parseAnswer(ret);
  cleanup();
  return sat;
}

// -------------------------------------------------------------------------------------------
bool ExtQBFSolver::isSatModel(const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                              const CNF& cnf,
                              vector<int> &model)
{
  dumpQBF(quantifier_prefix, cnf, in_file_name_);
  int ret = system(getSolverCommandModel().c_str());
  ret = WEXITSTATUS(ret);
  bool sat = parseModel(ret, model);
  cleanup();
  return sat;
}

// -------------------------------------------------------------------------------------------
bool ExtQBFSolver::isSatModel(const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                              const CNF& cnf,
                              vector<int> &model)
{
  dumpQBF(quantifier_prefix, cnf, in_file_name_);
  int ret = system(getSolverCommandModel().c_str());
  ret = WEXITSTATUS(ret);
  bool sat = parseModel(ret, model);
  cleanup();
  return sat;
}

// -------------------------------------------------------------------------------------------
void ExtQBFSolver::dumpQBF(const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                           const CNF& cnf,
                           const string &filename)
{
  VarManager& VM = VarManager::instance();
  ofstream oss(filename.c_str());
  oss << "p cnf " << VM.getMaxCNFVar() << " " << cnf.getNrOfClauses() << endl;
  for(size_t cnt = 0; cnt < quantifier_prefix.size(); ++cnt)
  {
    if(quantifier_prefix[cnt].second == E)
      oss << "e ";
    else
      oss << "a ";
    // The following code may look strange, but QBFCert does not like two
    // subsequent 'a' lists, or 'e' lists, so we merge them:
    while(1)
    {
      const vector<int> &vars = VM.getVarsOfType(quantifier_prefix[cnt].first);
      for(size_t var_cnt = 0; var_cnt < vars.size(); ++var_cnt)
        oss << vars[var_cnt] << " ";
      if(cnt == quantifier_prefix.size() - 1)
        break;
      if(quantifier_prefix[cnt].second != quantifier_prefix[cnt+1].second)
        break;
      ++cnt;
    }
    oss << "0" << endl;
  }
  oss << cnf.toString();
  oss.close();
}

// -------------------------------------------------------------------------------------------
void ExtQBFSolver::dumpQBF(const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                           const CNF& cnf,
                           const string &filename)
{
  int max_cnf_var = 0;
  for(size_t level = 0; level < quantifier_prefix.size(); ++level)
  {
    for(size_t var_cnt = 0; var_cnt < quantifier_prefix[level].first.size(); ++var_cnt)
    {
      if(quantifier_prefix[level].first[var_cnt] > max_cnf_var)
        max_cnf_var = quantifier_prefix[level].first[var_cnt];
    }
  }

  ofstream oss(filename.c_str());
  oss << "p cnf " << max_cnf_var << " " << cnf.getNrOfClauses() << endl;
  for(size_t cnt = 0; cnt < quantifier_prefix.size(); ++cnt)
  {
    if(quantifier_prefix[cnt].second == E)
      oss << "e ";
    else
      oss << "a ";
    // The following code may look strange, but QBFCert does not like two
    // subsequent 'a' lists, or 'e' lists, so we merge them:
    while(1)
    {
      const vector<int> &vars = quantifier_prefix[cnt].first;
      for(size_t var_cnt = 0; var_cnt < vars.size(); ++var_cnt)
        oss << vars[var_cnt] << " ";
      if(cnt == quantifier_prefix.size() - 1)
        break;
      if(quantifier_prefix[cnt].second != quantifier_prefix[cnt+1].second)
        break;
      ++cnt;
    }
    oss << "0" << endl;
  }
  oss << cnf.toString();
  oss.close();
}

// -------------------------------------------------------------------------------------------
bool ExtQBFSolver::parseAnswer(int ret) const
{
  MASSERT(ret == 20 || ret == 10, "Solver terminated with strange exit code.");
  if(ret == 10)
    return true;
  return false;
}

// -------------------------------------------------------------------------------------------
bool ExtQBFSolver::parseModel(int ret, vector<int> &model) const
{
  MASSERT(ret == 20 || ret == 10, "Solver terminated with strange exit code.");

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

// -------------------------------------------------------------------------------------------
void ExtQBFSolver::cleanup()
{
  remove(in_file_name_.c_str());
  remove(out_file_name_.c_str());
}

