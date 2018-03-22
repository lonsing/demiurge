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
/// @file VarInfo.cpp
/// @brief Contains the definition of the class VarInfo.
// -------------------------------------------------------------------------------------------

#include "VarInfo.h"

// -------------------------------------------------------------------------------------------
VarInfo::VarInfo(VarKind var_kind, int lit_in_cnf, int lit_in_aig) :
  var_kind_(var_kind),
  lit_in_cnf_(lit_in_cnf),
  lit_in_aig_(lit_in_aig),
  name_("")
{
  // nothing to be done
}

// -------------------------------------------------------------------------------------------
VarInfo::VarInfo(VarKind var_kind, int lit_in_cnf, int lit_in_aig, const string &name) :
  var_kind_(var_kind),
  lit_in_cnf_(lit_in_cnf),
  lit_in_aig_(lit_in_aig),
  name_(name)
{
  // nothing to be done
}

// -------------------------------------------------------------------------------------------
VarInfo::VarInfo(const VarInfo &other):
  var_kind_(other.var_kind_),
  lit_in_cnf_(other.lit_in_cnf_),
  lit_in_aig_(other.lit_in_aig_),
  name_(other.name_)
{
  // nothing to be done
}

// -------------------------------------------------------------------------------------------
VarInfo& VarInfo::operator=(const VarInfo &other)
{
  var_kind_ = other.var_kind_;
  lit_in_cnf_ = other.lit_in_cnf_;
  lit_in_aig_ = other.lit_in_aig_;
  name_ = other.name_;
  return *this;
}

// -------------------------------------------------------------------------------------------
VarInfo::~VarInfo()
{
  // nothing to be done
}

// -------------------------------------------------------------------------------------------
VarInfo::VarKind VarInfo::getKind() const
{
  return var_kind_;
}

// -------------------------------------------------------------------------------------------
int VarInfo::getLitInCNF() const
{
  return lit_in_cnf_;
}

// -------------------------------------------------------------------------------------------
int VarInfo::getLitInAIG() const
{
  return lit_in_aig_;
}

// -------------------------------------------------------------------------------------------
const string& VarInfo::getName() const
{
  return name_;
}

// -------------------------------------------------------------------------------------------
void VarInfo::setKind(VarKind kind)
{
  var_kind_ = kind;
}

// -------------------------------------------------------------------------------------------
void VarInfo::setName(const string &name)
{
  name_ = name;
}
