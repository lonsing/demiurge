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
/// @file DepQBFExt.h
/// @brief Contains the declaration of the class DepQBFExt.
// -------------------------------------------------------------------------------------------

#ifndef DepQBFExt_H__
#define DepQBFExt_H__

#include "defines.h"
#include "ExtQBFSolver.h"

class aiger;

// -------------------------------------------------------------------------------------------
///
/// @class DepQBFExt
/// @brief Calls the DepQBF QBF-solver in a separate process, communicating with files.
///
/// This class represents an interface to the QBF-solver DepQBF (see
/// https://github.com/lonsing/depqbf). For a given Quantified Boolean formula (a CNF with
/// a quantifier prefix), this class is able to determine satisfiability. Furthermore, in
/// case of satisfiability, it can extract satisfying assignments for variables quantified
/// existentially on the outermost level. The DepQBF solver is executed in a separate process.
/// Communication with this process works via files.
///
/// Most of work is actually implemented in the base class ExtQBFSolver. This class mainly
/// overrides some methods which are specific for DepQBF. However, it also interfaces the
/// QBF-Cert framework (see http://fmv.jku.at/qbfcert/), which uses DepQBF to extract Skolem
/// functions for QBFs.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.2.0
class DepQBFExt : public ExtQBFSolver
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
///
/// @param use_bloqqer True if bloqqer should be used as a preprocessor. False otherwise.
/// @param timeout An optional time-out in seconds. If set to 0, then no time-out will be
///        set. At the moment, a time-out can only be used with bloqqer. Setting a time-out
///        but not using bloqqer, the time-out will be ignored.
  DepQBFExt(bool use_bloqqer = false, size_t timeout = 0);

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~DepQBFExt();

// -------------------------------------------------------------------------------------------
///
/// @brief Executes the QBF-Cert framework to compute Skolem functions for QBFs.
///
/// The QBF consists of a quantifier prefix and a Boolean formula in CNF. The quantifier
/// prefix assigns an existential or universal quantifier to every kind of variable.
///
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to every kind of variable that
///        occurs in the cnf.
/// @param cnf A Boolean formula in CNF.
/// @return The resulting Skolem functions in AIGER format. The returned AIGER structure must
///         be deleted by the caller.
  virtual aiger* qbfCert(const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                         const CNF& cnf);

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
/// @exception DemiurgeException if the solver crashed or a timeout occurred.
/// @param ret The exit code of the process running the QBF solver.
/// @param get The variables of interest for which we want to have a satisfying assignment.
/// @param model An empty vector. In case of satisfiability, this method will write a
///        satisfying assignment in form of a cube into this vector.
/// @return True in case of satisfiability, false for unsatisfiability.
  virtual bool parseModel(int ret, const vector<int> &get, vector<int> &model) const;

// -------------------------------------------------------------------------------------------
///
/// @brief True if bloqqer should be used as a preprocessor. False otherwise.
  bool use_bloqqer_;

// -------------------------------------------------------------------------------------------
///
/// @brief An optional time-out in seconds. If set to 0, then no time-out will be set.
  size_t timeout_;

// -------------------------------------------------------------------------------------------
///
/// @brief The path to the DepQBF executable.
  string path_to_deqqbf_;

// -------------------------------------------------------------------------------------------
///
/// @brief The path to the DepQBF executable.
  string path_to_bloqqer_;

// -------------------------------------------------------------------------------------------
///
/// @brief The path to the QBF-Cert script.
  string path_to_qbfcert_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  DepQBFExt(const DepQBFExt &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  DepQBFExt& operator=(const DepQBFExt &other);

};

#endif // DepQBFExt_H__
