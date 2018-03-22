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
/// @file DepQBFApiInc.h
/// @brief Contains the declaration of the class DepQBFApiInc.
// -------------------------------------------------------------------------------------------

#ifndef DepQBFApiInc_H__
#define DepQBFApiInc_H__

#include "defines.h"
#include "QBFSolver.h"

class QDPLL;

// -------------------------------------------------------------------------------------------
///
/// @class DepQBFApiInc
/// @brief Interfaces an experimental DepQBF-version via its API.
///
/// This class represents an interface to an experimental version of the QBF-solver DepQBF
/// (see https://github.com/lonsing/depqbf). This version is not (yet) publicly available.
/// In addition to the features of DepQBFApi, this interface to DepQBF can also compute
/// unsatisfiable cores. Furthermore, it can use DepQBF incrementally in the sense that
/// clauses that talk only about variables that are quantified existentially on the outermost
/// quantifier block can be added without a need to restart the solver completely.
///
/// Otherwise, this class is equivalent to DepQBFApi.
///
/// @note This class is experimental.
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.0.0
class DepQBFApiInc : public QBFSolver
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
///
/// @param use_bloqqer True if bloqqer should be used as pre-processor, false if no
///        pre-processing should be used. If this parameter is omitted, no pre-processing is
///        used.
  DepQBFApiInc(bool use_bloqqer = false);

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~DepQBFApiInc();

// -------------------------------------------------------------------------------------------
///
/// @brief Checks if a given QBF is satisfiable (with quantification over variable kinds).
///
/// The QBF consists of a quantifier prefix and a Boolean formula in CNF. The quantifier
/// prefix assigns an existential or universal quantifier to every kind of variable.
///
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
///
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
/// @brief Starts an 'incremental' session with a quantifier prefix over variable kinds.
///
/// Here, 'incrementally' means that clauses that talk only about variables that are
/// quantified existentially on the outermost quantifier block can be added without a need
/// to restart the solver completely.
///
/// Every DepQBFApiInc instance can only have one open incremental session. If you call
/// this method twice, the old incremental session (if any) will be deleted (just like
/// calling @link #clearIncrementalSession clearIncrementalSession() @endlink) and a new
/// one will be opened.
///
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to every kind of variable that
///        occurs in the cnf.
  virtual void startIncrementalSession(const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix);

// -------------------------------------------------------------------------------------------
///
/// @brief Starts an 'incremental' session with a quantifier prefix over variable sets.
///
/// Here, 'incrementally' means that clauses that talk only about variables that are
/// quantified existentially on the outermost quantifier block can be added without a need
/// to restart the solver completely.
///
/// Every DepQBFApiInc instance can only have one open incremental session. If you call
/// this method twice, the old incremental session (if any) will be deleted (just like
/// calling @link #clearIncrementalSession clearIncrementalSession() @endlink) and a new
/// one will be opened.
///
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to different sets of variables
///        occurring in the cnf.
  virtual void startIncrementalSession(const vector<pair<vector<int>, Quant> > &quantifier_prefix);

// -------------------------------------------------------------------------------------------
///
/// @brief Closes a previously opened incremental session and frees all resources.
///
/// If no incremental session is open, then this method has no effect at all.
  virtual void clearIncrementalSession();

// -------------------------------------------------------------------------------------------
///
/// @brief Adds (conjuncts) a new CNF to the current incremental session.
///
/// @pre @link #startIncrementalSession startIncrementalSession() @endlink must have been
///      called before.
/// @param cnf A Boolean formula in CNF. It is conjuncted to the CNF in the currently open
///        incremental session.
  virtual void incAddCNF(const CNF &cnf);

// -------------------------------------------------------------------------------------------
///
/// @brief Adds (conjuncts) a new clause to the current incremental session.
///
/// @pre @link #startIncrementalSession startIncrementalSession() @endlink must have been
///      called before.
/// @param clause The clause which shall be conjuncted to the CNF in the currently open
///        incremental session.
  virtual void incAddClause(const vector<int> &clause);

// -------------------------------------------------------------------------------------------
///
/// @brief Adds (conjuncts) a new cube to the current incremental session.
///
/// @pre @link #startIncrementalSession startIncrementalSession() @endlink must have been
///      called before.
/// @param cube The cube which shall be conjuncted to the CNF in the currently open
///        incremental session. All literals of the cube are added as unit clauses to the
///        CNF of the currently open session.
  virtual void incAddCube(const vector<int> &cube);

// -------------------------------------------------------------------------------------------
///
/// @brief Checks if the QBF in the current incremental session is satisfiable.
///
/// @return True if the QBF in the current incremental session is satisfiable, false
///         otherwise.
  virtual bool incIsSat();

// -------------------------------------------------------------------------------------------
///
/// @brief Checks if the QBF in the incremental session is satisfiable under assumptions.
///
/// @param assumptions The assumptions under which the QBF in the current incremental session
///        should be solved. The assumptions are interpreted as a cube of literals. Every
///        literal is added temporarily as a unit clause to the QBF.
/// @return True if the QBF in the current incremental session is satisfiable under the
///         assumptions, false otherwise.
  virtual bool incIsSat(const vector<int> &assumptions);

// -------------------------------------------------------------------------------------------
///
/// @brief Checks if the incremental session is satisfiable and computes a model or core.
///
/// This method first checks if the QBF in the incremental session is satisfiable under
/// a given set of assumptions. If yes, then a satisfying assignment for the variables
/// quantified existentially on the outermost level is computed. Otherwise, an unsatisfiable
/// core is computed. An unsatisfiable core is a subset of the assumptions under which the
/// QBF is still unsatisfiable.
///
/// @param assumptions The assumptions under which the QBF in the current incremental session
///        should be solved. The assumptions are interpreted as a cube of literals. Every
///        literal is added temporarily as a unit clause to the QBF.
/// @param model_or_core The vector into which the resulting model or core will be stored.
///        In case of satisfiability a satisfying assignment (a cube of literals; negated ones
///        are false, unnegated ones are true) is stored in this vector. Otherwise, if this
///        method returns false, a subset of the passed assumptions is stored in this vector.
/// @return True if the QBF in the current incremental session is satisfiable under the
///         assumptions, false otherwise.
  virtual bool incIsSatModelOrCore(const vector<int> &assumptions,
                                   vector<int> &model_or_core);

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief Initializes the pre-processor with a QBF (quantifying over variable kinds).
///
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to every kind of variable that
///        occurs in the cnf.
/// @param cnf A Boolean formula in CNF.
  void initBloqqer(const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                   const CNF& cnf);

// -------------------------------------------------------------------------------------------
///
/// @brief Initializes the pre-processor with a QBF (quantifying over variable sets).
///
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to different sets of variables
///        occurring in the cnf.
/// @param cnf A Boolean formula in CNF.
  void initBloqqer(const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                   const CNF& cnf);

// -------------------------------------------------------------------------------------------
///
/// @brief Initializes a DepQBF instance with a QBF (quantifying over variable kinds).
///
/// @param solver The DepQBF solver instance to initialize.
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to every kind of variable that
///        occurs in the cnf.
/// @param cnf A Boolean formula in CNF.
  void initDepQBF(QDPLL *solver,
                  const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                  const CNF& cnf);

// -------------------------------------------------------------------------------------------
///
/// @brief Initializes a DepQBF instance with a QBF (quantifying over variable sets).
///
/// @param solver The DepQBF solver instance to initialize.
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to different sets of variables
///        occurring in the cnf.
/// @param cnf A Boolean formula in CNF.
  void initDepQBF(QDPLL *solver,
                  const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                  const CNF& cnf);

// -------------------------------------------------------------------------------------------
///
/// @brief Extracts an unsatisfiable core after unsatisfiability has been shown.
///
/// @param solver The DepQBF solver instance to extract the core from.
/// @param core The vector into which the resulting unsatisfiable core will be stored.
///        It is a subset of the used assumptions.
  void extractCore(QDPLL *solver, vector<int> &core);

// -------------------------------------------------------------------------------------------
///
/// @brief A debug function to cross-check bloqqer verdicts (SAT or UNSAT).
///
/// The QBF is also solved with DepQBF without pre-processing and the two verdicts are
/// compared. This method is used to debug bloqqer.
///
/// @param bloqqer_verdict The result returned by bloqqer. The value 10 means SAT, the value
///        20 means UNSAT.
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to every kind of variable that
///        occurs in the cnf.
/// @param cnf A Boolean formula in CNF.
  void debugCheckBloqqerVerdict(int bloqqer_verdict,
                                const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                                const CNF& cnf);

// -------------------------------------------------------------------------------------------
///
/// @brief A debug function to cross-check bloqqer verdicts (SAT or UNSAT).
///
/// The QBF is also solved with DepQBF without pre-processing and the two verdicts are
/// compared. This method is used to debug bloqqer.
///
/// @param bloqqer_verdict The result returned by bloqqer. The value 10 means SAT, the value
///        20 means UNSAT.
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to different sets of variables
///        occurring in the cnf.
/// @param cnf A Boolean formula in CNF.
  void debugCheckBloqqerVerdict(int bloqqer_verdict,
                                const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                                const CNF& cnf);

// -------------------------------------------------------------------------------------------
///
/// @brief A debug function to cross-check models produced by bloqqer.
///
/// DepQBF without preprocessing is used to check if the model found by bloqqer indeed
/// satisfies the QBF. This method is used to debug bloqqer.
///
/// @param model The satisfying assignment found by bloqqer. Negated variables in the vector
///        are FALSE, unnegated ones are TRUE.
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to every kind of variable that
///        occurs in the cnf.
/// @param cnf A Boolean formula in CNF.
  void debugCheckBloqqerModel(const vector<int> &model,
                              const vector<pair<VarInfo::VarKind, Quant> > &quantifier_prefix,
                              const CNF& cnf);

// -------------------------------------------------------------------------------------------
///
/// @brief A debug function to cross-check models produced by bloqqer.
///
/// DepQBF without preprocessing is used to check if the model found by bloqqer indeed
/// satisfies the QBF. This method is used to debug bloqqer.
///
/// @param model The satisfying assignment found by bloqqer. Negated variables in the vector
///        are FALSE, unnegated ones are TRUE.
/// @param quantifier_prefix The quantifier prefix as a vector of pairs. vector[0] is the
///        leftmost (i.e., outermost) quantifier block. The pairs in the quantifier_prefix
///        assign an existential or universal quantifier to different sets of variables
///        occurring in the cnf.
/// @param cnf A Boolean formula in CNF.
  void debugCheckBloqqerModel(const vector<int> &model,
                              const vector<pair<vector<int>, Quant> > &quantifier_prefix,
                              const CNF& cnf);


// -------------------------------------------------------------------------------------------
///
/// @brief A flag indicating whether or not the pre-processor bloqqer should be used.
  bool use_bloqqer_;

// -------------------------------------------------------------------------------------------
///
/// @brief A flag indicating whether or not unsatisfiable cores should be further minimized.
///
/// If this flag is false, then we take the core as returned by the solver. If it is true,
/// then we try to further minimize the core by trying to drop one remaining assumption
/// after the other in a loop. This is then guaranteed to result in an unsatisfiable core
/// which is a local minimum subset of the assumptions.
  bool min_cores_;

// -------------------------------------------------------------------------------------------
///
/// @brief The solver instance to use in the incremental session.
  QDPLL *inc_solver_;

// -------------------------------------------------------------------------------------------
///
/// @brief The variables for which a satisfying assignment can be constructed.
  vector<int> inc_model_vars_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  DepQBFApiInc(const DepQBFApiInc &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  DepQBFApiInc& operator=(const DepQBFApiInc &other);

};

#endif // DepQBFApiInc_H__
