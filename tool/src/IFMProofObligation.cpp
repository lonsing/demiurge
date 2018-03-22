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
/// @file IFMProofObligation.cpp
/// @brief Contains the definition of the class IFMProofObligation.
// -------------------------------------------------------------------------------------------

#include "IFMProofObligation.h"

// -------------------------------------------------------------------------------------------
IFMProofObligation::IFMProofObligation(const vector<int> &state_cube, size_t state_level) :
                                       state_cube_(state_cube),
                                       state_level_(state_level)
{
  // nothing to be done
}

// -------------------------------------------------------------------------------------------
IFMProofObligation::IFMProofObligation(const vector<int> &state_cube,
                                       size_t state_level,
                                       const vector<int> &pre_state_in_cube,
                                       const vector<int> &pre_ctrl_cube) :
  state_cube_(state_cube),
  state_level_(state_level),
  pre_state_in_cube_(pre_state_in_cube),
  pre_ctrl_cube_(pre_ctrl_cube)
{
  // nothing to be done
}

// -------------------------------------------------------------------------------------------
IFMProofObligation::IFMProofObligation(const IFMProofObligation &other) :
  state_cube_(other.state_cube_),
  state_level_(other.state_level_),
  pre_state_in_cube_(other.pre_state_in_cube_),
  pre_ctrl_cube_(other.pre_ctrl_cube_)
{
  // nothing to be done
}

// -------------------------------------------------------------------------------------------
IFMProofObligation& IFMProofObligation::operator=(const IFMProofObligation &other)
{
  state_cube_ = other.state_cube_;
  state_level_ = other.state_level_;
  pre_state_in_cube_ = other.pre_state_in_cube_;
  pre_ctrl_cube_ = other.pre_ctrl_cube_;
  return *this;
}

// -------------------------------------------------------------------------------------------
IFMProofObligation::~IFMProofObligation()
{
  // nothing to be done
}

// -------------------------------------------------------------------------------------------
const vector<int>& IFMProofObligation::getState() const
{
  return state_cube_;
}

// -------------------------------------------------------------------------------------------
size_t IFMProofObligation::getLevel() const
{
  return state_level_;
}

// -------------------------------------------------------------------------------------------
bool IFMProofObligation::hasPre() const
{
  return !pre_ctrl_cube_.empty();
}

// -------------------------------------------------------------------------------------------
const vector<int>& IFMProofObligation::getPreStateInCube() const
{
  return pre_state_in_cube_;
}

// -------------------------------------------------------------------------------------------
const vector<int>& IFMProofObligation::getPreCtrlCube() const
{
  return pre_ctrl_cube_;
}

// -------------------------------------------------------------------------------------------
void IFMProofObligation::setPre(const vector<int> &pre_state_in_cube,
                                const vector<int> &pre_ctrl_cube)
{
  MASSERT(!pre_state_in_cube.empty(), "Strange predecessor state.");
  pre_state_in_cube_ = pre_state_in_cube;
  pre_ctrl_cube_ = pre_ctrl_cube;
}


