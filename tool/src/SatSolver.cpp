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
/// @file SatSolver.cpp
/// @brief Contains the definition of the class SatSolver.
// -------------------------------------------------------------------------------------------

#include "SatSolver.h"

// -------------------------------------------------------------------------------------------
SatSolver::SatSolver(bool rand_models, bool min_cores) :
           min_cores_(min_cores),
           rand_models_(rand_models)
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
SatSolver::~SatSolver()
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
void SatSolver::doMinCores(bool min_cores)
{
  min_cores_ = min_cores;
}

// -------------------------------------------------------------------------------------------
void SatSolver::doRandModels(bool rand_models)
{
  rand_models_ = rand_models;
}

