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
/// @file LearnSynthQBF.h
/// @brief Contains the declaration of the class LearnSynthQBF.
// -------------------------------------------------------------------------------------------

#ifndef LearnSynthQBF_H__
#define LearnSynthQBF_H__

#include "defines.h"
#include "CNF.h"
#include "LearnStatisticsQBF.h"
#include "VarInfo.h"
#include "QBFSolver.h"
#include "BackEnd.h"

class SatSolver;
class CNFImplExtractor;

// -------------------------------------------------------------------------------------------
///
/// @class LearnSynthQBF
/// @brief Implements a learning-based synthesis using a QBF solver.
///
/// The computation of the winning region works as follows. We start with the initial guess
/// that the winning region F is the set of all state states P. In a loop, we first compute
/// a counterexample to the correctness of this guess in form of a state from which the
/// antagonist (controlling the uncontrollable inputs i) can enforce to leave F. The state is
/// represented as a cube over all state-variables. Next, this state-cube is generalized into
/// a larger region of states by dropping literals as long as the cube contains only states
/// that are winning for the antagonist (and hence must be removed or have already been
/// removed from F). Finally, the generalized cube is removed from F. The next iteration
/// checks for counterexample-state on the refined F. This is repeated until no more
/// counterexample-states exist (then the specification is realizable and F is a winning
/// region) or the initial state has been removed from F (in which case the specification is
/// unrealizable).
///
/// This procedure is implemented in #computeWinningRegionOne(). Some variants of this
/// procedure are implemented in other methods of this class. The method
/// #computeWinningRegionOneSAT() uses two SAT solvers instead of a QBF solver to compute
/// a counterexample-state. The method #computeWinningRegionAll() computes (and excludes) not
/// only one generalization of the counterexample but all generalizations. Command-line
/// parameters (--mode) decide which of these methods to use.
///
/// Finally the QBFCertImplExtractor is used to extract a circuit.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.2.0
class LearnSynthQBF : public BackEnd
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
///
/// @param impl_extractor The engine to use for circuit extraction. It will be deleted by
///        this class.
  LearnSynthQBF(CNFImplExtractor *impl_extractor);

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~LearnSynthQBF();

// -------------------------------------------------------------------------------------------
///
/// @brief Executes this back-end.
///
/// The computation of the winning region works as follows. We start with the initial guess
/// that the winning region F is the set of all state states P. In a loop, we first compute
/// a counterexample to the correctness of this guess in form of a state from which the
/// antagonist (controlling the uncontrollable inputs i) can enforce to leave F. The state is
/// represented as a cube over all state-variables. Next, this state-cube is generalized into
/// a larger region of states by dropping literals as long as the cube contains only states
/// that are winning for the antagonist (and hence must be removed or have already been
/// removed from F). Finally, the generalized cube is removed from F. The next iteration
/// checks for counterexample-state on the refined F. This is repeated until no more
/// counterexample-states exist (then the specification is realizable and F is a winning
/// region) or the initial state has been removed from F (in which case the specification is
/// unrealizable).
///
/// Command-line parameters (--mode) are used to select different variants of this method.
/// There is
/// <ul>
///  <li> the standard method as described above,
///  <li> a variant which computes counterexamples using two SAT-solvers instead of a QBF
///       solver,
///  <li> a variant which computes and excludes all counterexample-generalizations instead
///       of just one.
/// </ul>
///
/// @return True if the specification was realizable, false otherwise.
  virtual bool run();

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief Computes the winning region and stores the result in #winning_region_.
///
/// Depending on the command-line parameters (--mode) this method calls one of
/// <ul>
///  <li> #computeWinningRegionOne()
///  <li> #computeWinningRegionOneSAT()
///  <li> #computeWinningRegionAll()
/// </ul>
///
/// @return True if the specification was realizable, false otherwise.
  bool computeWinningRegion();

// -------------------------------------------------------------------------------------------
///
/// @brief Uses the standard method to compute the winning region.
///
/// This standard method works as follows.
/// We start with the initial guess that the winning region F is the set of all state states
/// P. In a loop, we first compute a counterexample to the correctness of this guess in form
/// of a state from which the antagonist (controlling the uncontrollable inputs i) can enforce
/// to leave F. The state is represented as a cube over all state-variables. Next, this
/// state-cube is generalized into a larger region of states by dropping literals as long as
/// the cube contains only states that are winning for the antagonist (and hence must be
/// removed or have already been removed from F). Finally, the generalized cube is removed
/// from F. The next iteration checks for counterexample-state on the refined F. This is
/// repeated until no more counterexample-states exist (then the specification is realizable
/// and F is a winning region) or the initial state has been removed from F (in which case
/// the specification is unrealizable).
///
/// @return True if the specification was realizable, false otherwise.
  bool computeWinningRegionOne();

// -------------------------------------------------------------------------------------------
///
/// @brief Computes the winning region, using two SAT-solvers to compute counterexamples.
///
/// The working principle is the same as for #computeWinningRegionOne(). The only difference
/// is that counterexample-states (states from which the antagonist controlling the inputs
/// i can enforce to leave the winning region) are computed using two competing SAT-solvers
/// instead of a QBF-solver.
///
/// @return True if the specification was realizable, false otherwise.
  bool computeWinningRegionOneSAT();

// -------------------------------------------------------------------------------------------
///
/// @brief Computes the winning region, always computing all counterexample generalizations.
///
/// The working principle is the same as for #computeWinningRegionOne(). The only difference
/// is that this method computes and excludes all counterexample generalizations instead of
/// just one.
///
/// @return True if the specification was realizable, false otherwise.
  bool computeWinningRegionAll();

// -------------------------------------------------------------------------------------------
///
/// @brief Computes a counterexample-state using a QBF-solver.
///
/// 'Counterexample' here means: counterexample to the correctness of the current guess of
/// the winning region. A counterexample is simply a state (represented as cube over
/// the state variables) from which the antagonist can enforce to leave the winning region.
/// This method uses a single call to a QBF-solver in order to find such a state.
///
/// @param ce An empty vector. The computed counterexample is stored in this vector in form
///        of a cube over the state variables. The cube does not necessarily contain all
///        state variables. If some state variables are irrelevant, then they may be omitted.
/// @return True if a counterexample was found, false if no counterexample exists.
  bool computeCounterexampleQBF(vector<int> &ce);

// -------------------------------------------------------------------------------------------
///
/// @brief Computes a counterexample-state using two competing SAT-solvers.
///
/// 'Counterexample' here means: counterexample to the correctness of the current guess of
/// the winning region. A counterexample is simply a state (represented as cube over
/// the state variables) from which the antagonist can enforce to leave the winning region.
/// This method uses two competing SAT-solvers in order to find such a state. First, solver
/// #solver_i_ finds some input values i that could be chosen by the antagonist in order to
/// leave the #winning_region_. Next, solver_ctrl_ checks if there exists a response of the
/// protagonist to chose control values c such that the winning region is not left. If no
/// such response exists, then we have found a counterexample. Otherwise, we exclude the
/// state-input combination found by #solver_i_ and try again.
///
/// There is also an optimization which allows to use #solver_i_ incrementally: The next
/// state-copy of the winning region is updated only lazily.
///
/// @param ce An empty vector. The computed counterexample is stored in this vector in form
///        of a cube over the state variables. The cube does not necessarily contain all
///        state variables. If some state variables are irrelevant, then they may be omitted.
/// @return True if a counterexample was found, false if no counterexample exists.
  bool computeCounterexampleSAT(vector<int> &ce);

// -------------------------------------------------------------------------------------------
///
/// @brief Computes a blocking clause for a counterexample.
///
/// A naive blocking clause would be the plain negation of the counterexample. However, in
/// order to boost the overall performance of the algorithm, we generalize the counterexample
/// before we negate it. This is done by dropping literals from the counterexample-cube as
/// long as all states in the cube are counterexamples (or have already been removed from the
/// winning region). The method also checks if the blocking clause blocks the initial state.
///
/// @param ce The counterexample to block. It is a full or partial cube over the state
///           variables.
/// @param blocking_clause An empty vector. It is used to store the computed blocking clause.
///        The counterexample falsifies the blocking clause.
/// @return False if the blocking clause blocks the initial state. This means that the initial
///         state is removed from the winning region, i.e., the specification is unrealizable.
///         True is returned otherwise.
  bool computeBlockingClause(vector<int> &ce, vector<int> &blocking_clause);

// -------------------------------------------------------------------------------------------
///
/// @brief Computes all minimal blocking clauses for a counterexample.
///
/// This method is like #computeBlockingClause(). However, instead of computing just one
/// generalization of the counterexample (and the corresponding blocking clause) it computes
/// all minimal generalizations (and corresponding blocking clauses). It does so using a
/// simple hitting set tree algorithm as presented by "Raymond Reiter: A Theory of Diagnosis
/// from First Principles. Artif. Intell. 32(1): 57-95 (1987)".
///
/// @param ce The counterexample to block. It is a full or partial cube over the state
///           variables.
/// @param blocking_clauses An empty vector. It is used to store the computed blocking
///        clauses. The counterexample falsifies all blocking clauses in this vector.
/// @return False if one of the blocking clauses blocks the initial state. This means that
///         the initial state is removed from the winning region, i.e., the specification is
///         unrealizable. True is returned otherwise.
  bool computeAllBlockingClauses(vector<int> &ce, vector<vector<int> > &blocking_clauses);

// -------------------------------------------------------------------------------------------
///
/// @brief Tries to drop literals from clauses in the winning region.
///
/// This method examines all clauses (in #winning_region_) that have already been computed
/// and checks if more literals can be dropped. This is done in the same way as for dropping
/// literals when generalizing a counterexample.
///
/// The intuition behind this method is that, even if a literal could not be dropped before,
/// it could be dropped at a later point in time because the winning region has been refined
/// in the meantime. However, in practice, typically only very few additional literals can
/// be dropped, so this method often does not pay off.
  void reduceExistingClauses();

// -------------------------------------------------------------------------------------------
///
/// @brief Recomputes the CNF (of the QBF) that is used for computing counterexamples.
///
/// The CNF used for computing counterexamples is F & T & !F', where F is the
/// #winning_region_.  The result is stored in #check_cnf_. Since F is constantly updated
/// with new clauses, #check_cnf_ needs to be updated as well. In an old version, we updated
/// the #check_cnf_ in place, modifying only the clauses that need to be modified. This gives
/// quite ugly code. Since this operation is not performance critical (compared to
/// QBF-solving), we now recompute check_cnf_ completely in each iteration.
///
/// @param take_small_win If this parameter is true (or omitted) the #winning_region_ is used
///        to compute the #check_cnf_. Otherwise, the #winning_region_large_ is used. The
///        difference between these two winning region versions is as follows. From time to
///        we remove redundant clauses (clauses implied by other clauses) from
///        #winning_region_ but not from #winning_region_large_. Hence, #winning_region_ is
///        often smaller, but #winning_region_large_ may contain clauses the solver may have
///        to learn when using #winning_region_.
  void recomputeCheckCNF(bool take_small_win = true);

// -------------------------------------------------------------------------------------------
///
/// @brief Recomputes the CNF (of the QBF) that is used for generalizing counterexamples.
///
/// The CNF used for generalizing counterexamples is F & T & F', where F is the
/// #winning_region_.  The result is stored in #generalize_clause_cnf_. Since F is constantly
/// updated with new clauses, #generalize_clause_cnf_ needs to be updated as well. In an old
/// version, we updated the #generalize_clause_cnf_ in place. This results in ugly code at
/// some places (especially when the #winning_region_ is compressed or simplified). Since this
/// operation is not performance critical (compared to QBF-solving), we now recompute
/// #generalize_clause_cnf_ completely in each iteration.
///
/// @param take_small_win If this parameter is true (or omitted) the #winning_region_ is used
///        to compute the #generalize_clause_cnf_. Otherwise, the #winning_region_large_ is
///        used. The difference between these two winning region versions is as follows. From
///        time to we remove redundant clauses (clauses implied by other clauses) from
///        #winning_region_ but not from #winning_region_large_. Hence, #winning_region_ is
///        often smaller, but #winning_region_large_ may contain clauses the solver may have
///        to learn when using #winning_region_.
  void recomputeGenCNF(bool take_small_win = true);

// -------------------------------------------------------------------------------------------
///
/// @brief Restricts a vector of literals (a cube or clause) to state-variables only.
///
/// That is, all literals that do not talk about present state variables are removed.
///
/// @param vec The vector of literals (the cube or clause) to restricts to state-variables
///        only.
  void restrictToStates(vector<int> &vec) const;

// -------------------------------------------------------------------------------------------
///
/// @brief Generalizes a counterexample-state by dropping literals.
///
/// This method generalizes a counterexample-state (a cube over the present-state variables)
/// by dropping literals as long as all states in the cube are counterexamples
/// (or have already been removed from the winning region). Optionally, this method also
/// checks if the passed vector is a valid (generalization of a) counterexample.
///
/// @param ce The counterexample to generalize (it will be overwritten by its own
///           generalization). This is a complete or partial cube over the present state
///           variables.
/// @param check_sat If set to true, then this method checks if the passed counterexample
///        vector is indeed a valid (generalization of a) counterexample. If set to false,
///        then this step is skipped.  If this parameter is true and the passed vector
///        is not a valid counterexample, then it will not be modified.
/// @return True if the passed vector is a valid (generalization of a) counterexample, or
///         if the check was skipped by setting check_sat = false. False is returned only
///         if the check was performed (check_sat = true) and the passed vector is not a
///         valid counterexample.
  bool generalizeCounterexample(vector<int> &ce, bool check_sat = true) const;

// -------------------------------------------------------------------------------------------
///
/// @brief The current over-approximation of the winning region.
///
/// Only when #computeWinningRegion() is done, this field will store the correct winning
/// region.
  CNF winning_region_;

// -------------------------------------------------------------------------------------------
///
/// @brief The current over-approximation of the winning region in an uncompressed form.
///
/// This CNF is logically equivalent to #winning_region_. However, #winning_region_ is
/// 'compressed' from time to time by removing redundant clauses (clauses that are implied
/// by other clauses in the winning region). This field stores the uncompressed version of
/// the winning region. Having the uncompressed winning region can be good because throwing
/// away redundant clauses is not always beneficial. The CNF get smaller, but the solver may
/// have to re-discover the removed clauses.
  CNF winning_region_large_;

// -------------------------------------------------------------------------------------------
///
/// @brief Stores and maintains statistics and performance measures.
  LearnStatisticsQBF statistics_;

// -------------------------------------------------------------------------------------------
///
/// @brief The QBF-solver used for all queries.
///
/// The type of solver is selected with command-line arguments.
  QBFSolver *qbf_solver_;

// -------------------------------------------------------------------------------------------
///
/// @brief The quantifier prefix of the QBF for computing counterexamples.
///
/// This quantifier prefix is always
///   exists x,i: forall c: exists x',tmp:
  vector<pair<VarInfo::VarKind, QBFSolver::Quant> > check_quant_;

// -------------------------------------------------------------------------------------------
///
/// @brief The CNF matrix of the QBF for computing counterexamples.
///
/// This CNF is always  F & T & !F', where F is the #winning_region_.
  CNF check_cnf_;

// -------------------------------------------------------------------------------------------
///
/// @brief The quantifier prefix of the QBF for generalizing counterexamples.
///
/// This quantifier prefix is always
///   exists x: forall i: exists c,x',tmp:
  vector<pair<VarInfo::VarKind, QBFSolver::Quant> > gen_quant_;

// -------------------------------------------------------------------------------------------
///
/// @brief The CNF matrix of the QBF for generalizing counterexamples.
///
/// This CNF is always  F & T & F', where F is the #winning_region_.
  CNF generalize_clause_cnf_;

// -------------------------------------------------------------------------------------------
///
/// @brief The SAT-solver for antagonist moves when using #computeCounterexampleSAT().
///
/// It stores the CNF  F & T & !F', where F is the #winning_region_. However, !F' is only
/// updated lazily for performance reasons (this way, incremental solving can be exploited
/// better).
  SatSolver *solver_i_;

// -------------------------------------------------------------------------------------------
///
/// @brief The SAT-solver for protagonist moves when using #computeCounterexampleSAT().
///
/// It stores the CNF  F & T & !F', where F is the #winning_region_. However, !F' is only
/// updated lazily for performance reasons (this way, incremental solving can be exploited
/// better).
  SatSolver *solver_ctrl_;

// -------------------------------------------------------------------------------------------
///
/// @brief A list of variables the SAT-solver should not optimize away.
  vector<int> incremental_vars_to_keep_;

// -------------------------------------------------------------------------------------------
///
/// @brief A flag indicating if solver_i_ is precise.
///
/// We update the next-state copy of the winning region in solver_i_ only lazily to better
/// support incremental solving. If this flag is true, then this means that the next-state
/// copy of the winning region is accurate in solver_i_. If this flag is false, then we must
/// not trust any unsatisfiability verdicts coming from solver_i_ (but need to update the
/// next-state copy of the winning region instead).
  bool solver_i_precise_;

// -------------------------------------------------------------------------------------------
///
/// @brief The engine to use for circuit extraction.
///
/// It will be deleted by this class (in the destructor).
  CNFImplExtractor *impl_extractor_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  LearnSynthQBF(const LearnSynthQBF &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  LearnSynthQBF& operator=(const LearnSynthQBF &other);

};

#endif // LearnSynthQBF_H__
