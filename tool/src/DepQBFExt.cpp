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

#include <sys/stat.h>

// -------------------------------------------------------------------------------------------
DepQBFExt::DepQBFExt() : ExtQBFSolver()
{
  const char *tp_env = getenv(Options::TP_VAR.c_str());
  MASSERT(tp_env != NULL, "You have not set the variable " << Options::TP_VAR);
  path_to_deqqbf_ = string(tp_env) + "/depqbf/depqbf";
  path_to_qbfcert_ = string(tp_env) + "/qbfcert/qbfcert_min.sh";
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
string DepQBFExt::qbfCert(const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                        const CNF& cnf)
{
  dumpQBF(quantifier_prefix, cnf, in_file_name_);
  string call_command = path_to_qbfcert_ + " " + in_file_name_ + " > /dev/null 2>&1";
  int ret = system(call_command.c_str());
  ret = WEXITSTATUS(ret);
  MASSERT(ret == 0, "QBFCert terminated with strange exit code.");

  string answer;
  string qbfcert_out = in_file_name_ + ".aiger";
  bool success = FileUtils::readFile(qbfcert_out, answer);
  MASSERT(success, "Could not read result file.");
  cleanup();
  remove(qbfcert_out.c_str());
  return answer;
}

// -------------------------------------------------------------------------------------------
string DepQBFExt::getSolverCommand() const
{
  return path_to_deqqbf_ + " " + in_file_name_ + " > " + out_file_name_;
}

// -------------------------------------------------------------------------------------------
string DepQBFExt::getSolverCommandModel() const
{
  return path_to_deqqbf_ + " --qdo " + in_file_name_ + " > " + out_file_name_;
}
