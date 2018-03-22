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
/// @file LearnSynthQBFInc.h
/// @brief Contains the declaration of the class LearnSynthQBFInc.
// -------------------------------------------------------------------------------------------

#ifndef LearnSynthQBFInc_H__
#define LearnSynthQBFInc_H__

#include "defines.h"
#include "CNF.h"
#include "LearnStatisticsQBF.h"
#include "VarInfo.h"
#include "DepQBFApi.h"
#include "BackEnd.h"

class SatSolver;
class CNFImplExtractor;

// -------------------------------------------------------------------------------------------
///
/// @class LearnSynthQBFInc
/// @brief Implements a learning-based synthesis with incremental QBF solving.
///
/// This class is almost an exact copy of LearnSynthQBF. Hence, we refer to the documentation
/// of LearnSynthQBF for an explanation of the basic working principle.
/// The main difference is that this class utilizes incremental QBF solving features of
/// DepQBF. We also use the ability of DepQBF to compute unsatisfiable cores.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.2.0
class LearnSynthQBFInc : public BackEnd
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
///
/// @param impl_extractor The engine to use for circuit extraction. It will be deleted by
///        this class.
  LearnSynthQBFInc(CNFImplExtractor *impl_extractor);

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~LearnSynthQBFInc();

// -------------------------------------------------------------------------------------------
///
/// @brief Executes this back-end.
///
/// This class is almost an exact copy of LearnSynthQBF. Hence, we refer to the documentation
/// of LearnSynthQBF for an explanation of the basic working principle.
/// The main difference is that this class utilizes incremental QBF solving features of
/// DepQBF. We also use the ability of DepQBF to compute unsatisfiable cores.
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
///  <li> #computeWinningRegionAll()
/// </ul>
///
/// @return True if the specification was realizable, false otherwise.
  bool computeWinningRegion();

// -------------------------------------------------------------------------------------------
///
/// @brief Uses the standard method to compute the winning region, with incremental solving.
///
/// This method works like LearnSynthQBF#computeWinningRegionOne() but using incremental
/// solving and unsatisfiable cores.
///
/// @return True if the specification was realizable, false otherwise.
  bool computeWinningRegionOne();

// -------------------------------------------------------------------------------------------
///
/// @brief Uses incremental solving and push/pop to handle negation incrementally.
///
/// This method works like #computeWinningRegionOne() but uses push/pop, instead of
/// updating the next-state copy of the winning region only lazily.
///
/// @return True if the specification was realizable, false otherwise.
  bool computeWinningRegionOnePush();

// -------------------------------------------------------------------------------------------
///
/// @brief Uses incremental solving and a pool of variables to handle negation incrementally.
///
/// This method works like #computeWinningRegionOne() but uses a pool
/// of variables to handle the computation of counterexamples incrementally, instead of
/// updating the next-state copy of the winning region only lazily.
///
/// @return True if the specification was realizable, false otherwise.
  bool computeWinningRegionOnePool();

// -------------------------------------------------------------------------------------------
///
/// @brief Computes the winning region, always computing all counterexample generalizations.
///
/// This method works like LearnSynthQBF#computeWinningRegionAll() but using incremental
/// solving and unsatisfiable cores.
///
/// @return True if the specification was realizable, false otherwise.
  bool computeWinningRegionAll();

// -------------------------------------------------------------------------------------------
///
/// @brief Uses push/pop and always computes all counterexample generalizations.
///
/// This method works like #computeWinningRegionAll() but uses a push/pop to handle the
/// computation of counterexamples incrementally, instead of updating the next-state copy of
/// the winning region only lazily.
///
/// @return True if the specification was realizable, false otherwise.
  bool computeWinningRegionAllPush();

// -------------------------------------------------------------------------------------------
///
/// @brief Uses a pool of variables and always computes all counterexample generalizations .
///
/// This method works like #computeWinningRegionAll() but uses a pool
/// of variables to handle the computation of counterexamples incrementally, instead of
/// updating the next-state copy of the winning region only lazily.
///
/// @return True if the specification was realizable, false otherwise.
  bool computeWinningRegionAllPool();

// -------------------------------------------------------------------------------------------
///
/// @brief Computes a counterexample-state using a QBF-solver with incremental solving.
///
/// This method works like LearnSynthQBF#computeCounterexampleQBF() but using incremental
/// solving and unsatisfiable cores. 'Using incremental solving' means, instead of solving
///  exists x,i: forall c: exists x',tmp: F & T & !F'
/// where F is the #winning_region_ we solve
///  exists x,i: forall c: exists x',tmp: F & T & !G'
/// where G is a copy of F that is updated only lazily. That is, whenever new clauses are
/// discovered, they are added only to F but not to G. Only if this method returns false,
/// we update G := F and try again. The synthesis algorithm only aborts if false is returned
/// while G = F.
///
/// @param ce An empty vector. The computed counterexample is stored in this vector in form
///        of a cube over the state variables. The cube does not necessarily contain all
///        state variables. If some state variables are irrelevant, then they may be omitted.
/// @return True if a counterexample was found, false if no counterexample exists.
  bool computeCounterexampleQBF(vector<int> &ce);

// -------------------------------------------------------------------------------------------
///
/// @brief Computes a counterexample-state using a QBF-solver with incremental solving.
///
/// This method works like #computeCounterexampleQBF() but without the lazy update of the
/// next-state copy of the winning region. Instead, we use a pool of variables to express the
/// negated next-state copy of the winning region. Variables of this pool which are not yet
/// used are set to false until they are used.
///
/// @param ce An empty vector. The computed counterexample is stored in this vector in form
///        of a cube over the state variables. The cube does not necessarily contain all
///        state variables. If some state variables are irrelevant, then they may be omitted.
/// @return True if a counterexample was found, false if no counterexample exists.
  bool computeCounterexampleQBFPool(vector<int> &ce);

// -------------------------------------------------------------------------------------------
///
/// @brief Computes a blocking clause for a counterexample.
///
/// This method works like LearnSynthQBF#computeBlockingClause() but using unsatisfiable
/// cores to generalize the counterexample instead of just iterating over the literals.
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
/// This method works like LearnSynthQBF#computeAllBlockingClauses() but using unsatisfiable
/// cores to generalize the counterexample instead of just iterating over the literals.
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
/// #winning_region_.  The result is stored in #check_cnf_. In contrast to LearnSynthQBF,
/// this class calls this method not so often (only when #computeCounterexampleQBF() returns
/// unsatisfiable.
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
/// This method works like LearnSynthQBF#generalizeCounterexample() but using unsatisfiable
/// cores to generalize the counterexample instead of just iterating over the literals.
///
/// @param ce The counterexample to generalize (it will be overwritten by its own
///           generalization). This is a complete or partial cube over the present state
///           variables.
/// @return True if the passed vector is a valid (generalization of a) counterexample,
///         false otherwise.
  bool generalizeCounterexample(vector<int> &ce);

// -------------------------------------------------------------------------------------------
///
/// @brief Generalizes a counterexample-state further by dropping literals.
///
/// This method works like #generalizeCounterexample() but does not use the unsatisfiable
/// core feature of the QBF solver, but iterates over all the literals instead.
///
/// @param ce The counterexample to generalize further (it will be overwritten by its own
///           generalization). This is a complete or partial cube over the present state
///           variables.
  void generalizeCounterexampleFurther(vector<int> &ce);

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
/// @brief The QBF-solver used for computing counterexamples.
///
/// It is used in an incremental way. It is initialized to check_quant_: check_cnf_, i.e.,
///  exists x,i: forall c: exists x',tmp: F & T & !F'
/// where F is the #winning_region_. However, when new clauses are discovered, they are
/// only added to the current-state copy of F but not to the next-state copy. This allows to
/// to use the solver incrementally until it returns UNSAT. If it returns UNSAT, we recompute
/// the check_cnf_ (with correct next-state copy of the winning region) and restart the
/// solver with this CNF.
  DepQBFApi solver_check_;

// -------------------------------------------------------------------------------------------
///
/// @brief A pool of variables, which are used to express that a certain clause in negated.
///
/// This set of variables is supposed to support incremental solving within solver_check_.
/// The solver_check_ needs to solve
///  exists x,i: forall c: exists x',tmp: F & T & !F'
/// In each iteration, we add a clause (or several clauses) to F. In order to handle the
/// update to !F' incrementally, we add one big clause consisting of all the literals of the
/// vector neg_clause_var_pool_. Initially, none of the literals is 'used', so we add
/// assumptions that they are all false. Whenever we add a new clause to F, we simply pick
/// a new variable v from the pool, and say that v implies the negated new clause. Then this
/// variable v is no longer set to zero, but used to encode the negated clause. When we run
/// out of variables, we start a new incremental session, with a new pool of variables.
  vector<int> negcl_var_pool_;

// -------------------------------------------------------------------------------------------
///
/// @brief The activation variables to express that a var in negcl_var_pool_ is not yet used.
  vector<int> negcl_var_pool_act_;

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
/// @brief The QBF-solver used for generalizing counterexamples.
///
/// At the moment we do not really use this solver incrementally (we do not update the next-
/// state copy of the winning region lazily to do so because this sacrifices generalization
/// power in favor of incrementality and appears to be a bad deal). We only use its
/// capabilities of computing unsatisfiable cores (during one core computation, we use it
/// incrementally).
  DepQBFApi solver_gen_;

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
  LearnSynthQBFInc(const LearnSynthQBFInc &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  LearnSynthQBFInc& operator=(const LearnSynthQBFInc &other);

};

#endif // LearnSynthQBFInc_H__
