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
/// @file QuBEExt.h
/// @brief Contains the declaration of the class QuBEExt.
// -------------------------------------------------------------------------------------------

#ifndef QuBEExt_H__
#define QuBEExt_H__

#include "defines.h"
#include "ExtQBFSolver.h"

// -------------------------------------------------------------------------------------------
///
/// @class QuBEExt
/// @brief Calls the QuBE QBF-solver in a separate process, communicating with files.
///
/// This class represents an interface to the QBF-solver QuBE (see
/// www.star-lab.it/~qube/). For a given Quantified Boolean formula (a CNF with
/// a quantifier prefix), this class is able to determine satisfiability. Furthermore, in
/// case of satisfiability, it can extract satisfying assignments for variables quantified
/// existentially on the outermost level. The QuBE solver is executed in a separate process.
/// Communication with this process works via files.
/// QuBE can only decide the satisfiability of a formula, but it cannot produce satisfying
/// assignments. Hence, if a satisfying assignment is needed, we simply delegate the call to
/// DepQBF.
///
/// Most of work is actually implemented in the base class ExtQBFSolver. This class mainly
/// overrides some methods which are specific for QuBE.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.2.0
class QuBEExt : public ExtQBFSolver
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
  QuBEExt();

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~QuBEExt();

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the command to execute QuBE.
///
/// @return The command to execute QuBE.
  virtual string getSolverCommand() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the command to execute DepQBF such that it produces models.
///
/// This is not a copy-past error in this comment. We really execute DepQBF because QuBE
/// cannot produce models.
///
/// @return The command to execute DepQBF such that it produces models.
  virtual string getSolverCommandModel() const;

// -------------------------------------------------------------------------------------------
///
/// @brief The path to the QuBE executable.
  string path_to_qube_;

// -------------------------------------------------------------------------------------------
///
/// @brief The path to the DepQBF executable.
///
/// QuBE cannot compute satisfying assignments. Hence, if a satisfying assignment is needed,
///      we delegate the call to DepQBF.
  string path_to_deqqbf_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  QuBEExt(const QuBEExt &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  QuBEExt& operator=(const QuBEExt &other);

};

#endif // QuBEExt_H__
