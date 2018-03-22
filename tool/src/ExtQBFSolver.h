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
/// @file ExtQBFSolver.h
/// @brief Contains the declaration of the class ExtQBFSolver.
// -------------------------------------------------------------------------------------------

#ifndef ExtQBFSolver_H__
#define ExtQBFSolver_H__

#include "defines.h"
#include "QBFSolver.h"

// -------------------------------------------------------------------------------------------
///
/// @class ExtQBFSolver
/// @brief A generic interface to QBF-solvers started as external processes.
///
/// This is an abstract base class for all interfaces to QBF-solvers which start the
/// QBF-solver in a separate process. Practically all QBF-solvers understand the QDIMACS
/// format as input. Hence, different QBF-solvers can be interfaced in the same way. The
/// only things that differ from solver to solver are the location of the executable and the
/// command-line options that must be set. These solver-specific aspects are handled in the
/// derived classes (by overriding certain methods). Everything else is handled in this
/// abstract base class.
/// At he moment there are two implementations of this abstract class: DepQBFExt and
/// QuBEExt.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.2.0
class ExtQBFSolver : public QBFSolver
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
  ExtQBFSolver();

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~ExtQBFSolver();

// -------------------------------------------------------------------------------------------
///
/// @brief Checks if a given QBF is satisfiable (with quantification over variable kinds).
///
/// The QBF consists of a quantifier prefix and a Boolean formula in CNF. The quantifier
/// prefix assigns an existential or universal quantifier to every kind of variable.
/// The QBF is dumped into a file, a QBF-solver is called, and the exit code is used to
/// infer the satisfiability result.
///
/// @exception DemiurgeException if the solver crashed or a timeout occurred.
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to every kind of variable that
///        occurs in the cnf.
/// @param cnf A Boolean formula in CNF.
/// @return True if the QBF is satisfiable, false otherwise.
  virtual bool isSat(const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                     const CNF& cnf);

// -------------------------------------------------------------------------------------------
///
/// @brief Checks if a given QBF is satisfiable (with quantification over variable sets).
///
/// The QBF consists of a quantifier prefix and a Boolean formula in CNF. The quantifier
/// prefix assigns an existential or universal quantifier to different sets variable.
/// The QBF is dumped into a file, a QBF-solver is called, and the exit code is used to
/// infer the satisfiability result.
///
/// @exception DemiurgeException if the solver crashed or a timeout occurred.
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to different sets of variables
///        occurring in the cnf.
/// @param cnf A Boolean formula in CNF.
/// @return True if the QBF is satisfiable, false otherwise.
  virtual bool isSat(const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                     const CNF& cnf);

// -------------------------------------------------------------------------------------------
///
/// @brief Checks satisfiability and extracts a model (quantifying over variable kinds).
///
/// Just like @link #isSat isSat() @endlink, this method checks a quantified Boolean formula
/// for satisfiability. If the QBF is satisfiable, this method also extracts a model
/// (a satisfying assignment) for all variables which are quantified existentially on the
/// outermost level. The model is provided as cube: negated variables in the cube are FALSE,
/// unnegated ones are TRUE.
///
/// @exception DemiurgeException if the solver crashed or a timeout occurred.
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to every kind of variable that
///        occurs in the cnf.
/// @param cnf A Boolean formula in CNF.
/// @param model The resulting model in form of a cube in case of satisfiability. Negated
///        variables in the cube are FALSE, unnegated ones are TRUE. If the formula is
///        unsatisfiable (this method returns false) then this parameter is not modified.
/// @return True if the QBF is satisfiable, false otherwise.
  virtual bool isSatModel(const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                          const CNF& cnf,
                          vector<int> &model);

// -------------------------------------------------------------------------------------------
///
/// @brief Checks satisfiability and extracts a model (quantifying over variable sets).
///
/// Just like @link #isSat isSat() @endlink, this method checks a quantified Boolean formula
/// for satisfiability. If the QBF is satisfiable, this method also extracts a model
/// (a satisfying assignment) for all variables which are quantified existentially on the
/// outermost level. The model is provided as cube: negated variables in the cube are FALSE,
/// unnegated ones are TRUE.
///
/// @exception DemiurgeException if the solver crashed or a timeout occurred.
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to different sets of variables
///        occurring in the cnf.
/// @param cnf A Boolean formula in CNF.
/// @param model The resulting model in form of a cube in case of satisfiability. Negated
///        variables in the cube are FALSE, unnegated ones are TRUE. If the formula is
///        unsatisfiable (this method returns false) then this parameter is not modified.
/// @return True if the QBF is satisfiable, false otherwise.
  virtual bool isSatModel(const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                          const CNF& cnf,
                          vector<int> &model);

// -------------------------------------------------------------------------------------------
///
/// @brief Dumps a QBF (quantification over variable kinds) into a file in QDIMACS format.
///
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to every kind of variable that
///        occurs in the cnf.
/// @param cnf A Boolean formula in CNF.
/// @param filename The name of the file (including path) to write to.
  static void dumpQBF(const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                      const CNF& cnf,
                      const string &filename);

// -------------------------------------------------------------------------------------------
///
/// @brief Dumps a QBF (quantification over variable kinds) into a file in QDIMACS format.
///
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to different sets of variables
///        occurring in the cnf.
/// @param cnf A Boolean formula in CNF.
/// @param filename The name of the file (including path) to write to.
  static void dumpQBF(const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                      const CNF& cnf,
                      const string &filename);

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief Parses the answer of the QBF solver to get the satisfiability verdict.
///
/// @exception DemiurgeException if the solver crashed or a timeout occurred.
/// @param ret The exit code of the process running the QBF solver.
/// @return True in case of satisfiability, false for unsatisfiability.
  virtual bool parseAnswer(int ret) const;

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
/// @brief Deletes the temporary files that have been used for communication.
  virtual void cleanup();

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the command to start the solver if no model is needed.
///
/// This command should contain the path to the executable as well as all necessary
/// command line parameters. This method is supposed to be implemented by derived classes.
///
/// @return The command to start the solver if no model is needed.
  virtual string getSolverCommand() const = 0;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the command to start the solver if a model is needed.
///
/// This command should contain the path to the executable as well as all necessary
/// command line parameters. This method is supposed to be implemented by derived classes.
///
/// @return The command to start the solver if a model is needed.
  virtual string getSolverCommandModel() const = 0;

// -------------------------------------------------------------------------------------------
///
/// @brief The name of the QDIMACS file containing the QBF to be solved.
  string in_file_name_;

// -------------------------------------------------------------------------------------------
///
/// @brief The name of the file that is produced by the QBF-solver (containing the model).
  string out_file_name_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  ExtQBFSolver(const ExtQBFSolver &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  ExtQBFSolver& operator=(const ExtQBFSolver &other);

};

#endif // ExtQBFSolver_H__
