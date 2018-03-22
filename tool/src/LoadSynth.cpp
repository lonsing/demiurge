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
/// @file LoadSynth.cpp
/// @brief Contains the definition of the class LoadSynth.
// -------------------------------------------------------------------------------------------

#include "LoadSynth.h"
#include "CNFImplExtractor.h"
#include "Options.h"
#include "CNF.h"

// -------------------------------------------------------------------------------------------
LoadSynth::LoadSynth(CNFImplExtractor *impl_extractor) :
  BackEnd(),
  impl_extractor_(impl_extractor)
{
  // nothing to be done
}

// -------------------------------------------------------------------------------------------
LoadSynth::~LoadSynth()
{
  delete impl_extractor_;
  impl_extractor_ = NULL;
}

// -------------------------------------------------------------------------------------------
bool LoadSynth::run()
{
  string filename = "win/" + Options::instance().getAigInFileNameOnly() + ".dimacs";
  CNF win_reg(filename);
  impl_extractor_->extractCircuit(win_reg);
  impl_extractor_->logStatistics();
  return true;
}

