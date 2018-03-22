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
/// @file QuBEExt.cpp
/// @brief Contains the definition of the class QuBEExt.
// -------------------------------------------------------------------------------------------

#include "QuBEExt.h"
#include "Options.h"

#include <sys/stat.h>

// -------------------------------------------------------------------------------------------
QuBEExt::QuBEExt() : ExtQBFSolver()
{
  const char *tp_env = getenv(Options::TP_VAR.c_str());
  MASSERT(tp_env != NULL, "You have not set the variable " << Options::TP_VAR);
  path_to_qube_ = string(tp_env) + "/qube/qube";
  path_to_deqqbf_ = string(tp_env) + "/depqbf/depqbf";
  struct stat st;
  MASSERT(stat(path_to_qube_.c_str(), &st) == 0, "QuBE executable not found.");
}

// -------------------------------------------------------------------------------------------
QuBEExt::~QuBEExt()
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
string QuBEExt::getSolverCommand() const
{
  return path_to_qube_ + " " + in_file_name_ + " -solve > " + out_file_name_;
}

// -------------------------------------------------------------------------------------------
string QuBEExt::getSolverCommandModel() const
{
  return path_to_deqqbf_ + " --qdo " + in_file_name_ + " > " + out_file_name_;
}

