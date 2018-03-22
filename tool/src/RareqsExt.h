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
/// @file RareqsExt.h
/// @brief Contains the declaration of the class RareqsExt.
// -------------------------------------------------------------------------------------------

#ifndef RareqsExt_H__
#define RareqsExt_H__

#include "defines.h"
#include "ExtQBFSolver.h"

// -------------------------------------------------------------------------------------------
///
/// @class RareqsExt
/// @brief Calls the RAReQS QBF-solver in a separate process, communicating with files.
///
/// This class represents an interface to the QBF-solver RAReQS (see
/// http://sat.inesc-id.pt/~mikolas/sw/areqs/). For a given Quantified Boolean formula (a
/// CNF with a quantifier prefix), this class is able to determine satisfiability.
/// Furthermore, in case of satisfiability, it can extract satisfying assignments for
/// variables quantified existentially on the outermost level. The RAReQS solver is executed
/// in a separate process. Communication with this process works via files.
///
/// Most of work is actually implemented in the base class ExtQBFSolver. This class mainly
/// overrides some methods which are specific for RAReQS.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.2.0
class RareqsExt : public ExtQBFSolver
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
  RareqsExt();

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~RareqsExt();

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the command to execute DepQBF.
///
/// @return The command to execute DepQBF.
  virtual string getSolverCommand() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the command to execute DepQBF such that it produces models.
///
/// @return The command to execute DepQBF such that it produces models.
  virtual string getSolverCommandModel() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Parses the answer of the QBF solver to get the satisfiability verdict and a model.
///
/// @param ret The exit code of the process running the QBF solver.
/// @param model An empty vector. In case of satisfiability, this method will write a
///        satisfying assignment in form of a cube into this vector.
/// @return True in case of satisfiability, false for unsatisfiability.
  virtual bool parseModel(int ret, vector<int> &model) const;

// -------------------------------------------------------------------------------------------
///
/// @brief The path to the RAReQS executable.
  string path_to_rareqs_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  RareqsExt(const RareqsExt &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  RareqsExt& operator=(const RareqsExt &other);

};

#endif // RareqsExt_H__
