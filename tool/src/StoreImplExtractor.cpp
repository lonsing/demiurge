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
/// @file StoreImplExtractor.cpp
/// @brief Contains the definition of the class StoreImplExtractor.
// -------------------------------------------------------------------------------------------


#include "StoreImplExtractor.h"
#include "Options.h"
#include "CNF.h"
#include <sys/stat.h>

// -------------------------------------------------------------------------------------------
StoreImplExtractor::StoreImplExtractor()
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
StoreImplExtractor::~StoreImplExtractor()
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
void StoreImplExtractor::extractCircuit(const CNF &winning_region)
{
  // create a directory for the winning regions if it does not exist:
  struct stat st;
  if(stat("win", &st) != 0)
  {
    int fail = mkdir("win", 0777);
    MASSERT(fail == 0, "Could not create directory 'win' for storing winning regions.");
  }

  string filename = "win/" + Options::instance().getAigInFileNameOnly() + ".dimacs";
  winning_region.saveToFile(filename);
}

// -------------------------------------------------------------------------------------------
void StoreImplExtractor::run(const CNF &winning_region, const CNF &neg_winning_region)
{
  // Not implemented. This class does not extract circuits, it only stores the winning
  // region into a file.
}
