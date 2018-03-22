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
/// @file VarInfo.h
/// @brief Contains the declaration of the class VarInfo.
// -------------------------------------------------------------------------------------------

#ifndef VarInfo_H__
#define VarInfo_H__

#include "defines.h"

// -------------------------------------------------------------------------------------------
///
/// @class VarInfo
/// @brief Stores additional information to a variable.
///
/// At the moment, a variable has a kind, a name (optional), an index with which id appears
/// in the AIGER input file, and an index with which is appears in CNF formulas.
/// This class is mainly a container for all this information without any intelligence.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.0.0
class VarInfo
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief A type for the kind of variable.
  enum VarKind
  {

// -------------------------------------------------------------------------------------------
///
/// @brief The value for present-state variables.
    PRES_STATE,

// -------------------------------------------------------------------------------------------
///
/// @brief The value for next-state variables.
    NEXT_STATE,

// -------------------------------------------------------------------------------------------
///
/// @brief The value for uncontrollable input variables.
    INPUT,

// -------------------------------------------------------------------------------------------
///
/// @brief The value for controllable variables.
    CTRL,

// -------------------------------------------------------------------------------------------
///
/// @brief The value for temporary variables without any special meaning.
///
/// These are usually Tseitin variables that are just abbreviations for some formulas.
    TMP,

// -------------------------------------------------------------------------------------------
///
/// @brief The value for template parameter variables.
///
/// This kind is only used by TemplateSynth.
    TEMPL_PARAMS,

// -------------------------------------------------------------------------------------------
///
/// @brief The value for previous-time-step variables.
///
/// This kind is used for the previous-state copy of state variables but also inputs and
/// control signals. At the moment, it is mainly used for optimization RG and RC in
/// LearnSynthSAT and LearnSynthQBFInd.
    PREV
  };

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor without name.
///
/// @param var_kind The kind of the variable.
/// @param lit_in_cnf The index with which this variable occurs in the CNFs.
/// @param lit_in_aig The index with which this variable occurs in the AIGER input file.
  VarInfo(VarKind var_kind, int lit_in_cnf, int lit_in_aig);

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor without name.
///
/// @param var_kind The kind of the variable.
/// @param lit_in_cnf The index with which this variable occurs in the CNFs.
/// @param lit_in_aig The index with which this variable occurs in the AIGER input file.
/// @param name The name of the variable.
  VarInfo(VarKind var_kind, int lit_in_cnf, int lit_in_aig, const string &name);

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// Makes a copy of the VarInfo.
///
/// @param other The source for creating the copy.
  VarInfo(const VarInfo &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// Makes this object a copy of another object.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  virtual VarInfo& operator=(const VarInfo &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~VarInfo();

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the kind of this variable.
///
/// @return The kind of this variable.
  VarKind getKind() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the index with which this variable occurs in the CNFs.
///
/// @return The index with which this variable occurs in the CNFs.
  int getLitInCNF() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the index with which this variable occurs in the AIGER input file.
///
/// @return The index with which this variable occurs in the AIGER input file.
  int getLitInAIG() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the name of the variable.
///
/// @return The name of the variable. If no name was set, the returned string is empty.
  const string& getName() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Sets the kind of the variable.
///
/// @param kind The new kind of the variable.
  void setKind(VarKind kind);

// -------------------------------------------------------------------------------------------
///
/// @brief Sets the name of the variable.
///
/// @param name The new name of the variable.
  void setName(const string &name);

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief The kind of the variable.
  VarKind var_kind_;

// -------------------------------------------------------------------------------------------
///
/// @brief The index with which this variable occurs in the CNFs.
  int lit_in_cnf_;

// -------------------------------------------------------------------------------------------
///
/// @brief The index with which this variable occurs in the AIGER input file.
  int lit_in_aig_;

// -------------------------------------------------------------------------------------------
///
/// @brief The name of the variable.
///
/// This can be an empty string if the variable has no name.
  string name_;
};

#endif // VarInfo_H__
