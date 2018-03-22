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
/// @file LearnSynthQBFInd.h
/// @brief Contains the declaration of the class LearnSynthQBFInd.
// -------------------------------------------------------------------------------------------

#ifndef LearnSynthQBFInd_H__
#define LearnSynthQBFInd_H__

#include "defines.h"
#include "CNF.h"
#include "LearnStatisticsQBF.h"
#include "VarInfo.h"
#include "QBFSolver.h"
#include "BackEnd.h"

class CNFImplExtractor;

// -------------------------------------------------------------------------------------------
///
/// @class LearnSynthQBFInd
/// @brief Implements a learning-based synthesis with inductive reachability reasoning.
///
/// This class is almost an exact copy of LearnSynthQBF. Hence, we refer to the documentation
/// of LearnSynthQBF for an explanation of the basic working principle.
/// The main difference is that this class uses a reachability optimization that is inspired
/// by the concept of inductiveness relative to a certain clause set used in IC3.
/// Refer to the VMCAI'14 publication "SAT-Based Synthesis Methods for Safety Specs" for a
/// detailed explanation how (and why) this works.
/// The class LearnSynthQBF computes counterexamples by solving:
///   exists x,i: forall c: exists x',tmp: F(x) & T(x,i,c,x') & !F(x') <br/>
/// and generalizes them by checking
///   exists x: forall i: exists c,x',tmp: s & F(x) & T(x,i,c,x') & F(x') <br/>
/// repeatedly, where s is the counterexample and F is the current over-approximation of the
/// winning region.
///
/// We know that F(x) is never left by the final implementation. Hence, F is an
/// over-approximation of the reachable states. Now if a state s is not initial and
/// !s & F & T => !s', then we know by induction that the state s is unreachable. We can
/// disregard unreachable states both in the computation of counterexamples as well as during
/// the generalization. This is done by gluing the constraints !s & F & T => !s' and !I(s) to
/// the generalization and check queries as following.
/// The check query becomes:
/// <br/> &nbsp;
///   exists x*,i*,c*,x,i: forall c: exists x',tmp:
///      (I(x) | (x* != x) & F(x*) & T(x*, i*, c*, x)) & F(x) & T(x,i,c,x') & !F(x')
/// <br/>
/// where x*, i* and c* are the previous-state copy of x,i,c. The query now says: find a
/// counterexample-state x that is either initial or has a predecessor x* in F such that x*
/// is different from x. (Otherwise the counterexample-state is unreachable.)
/// The generalization query becomes:
/// <br/> &nbsp;
///   exists x*,i*,c*,x: forall i: exists c,x',tmp:
///      (I(x) | F(x*) & !s* & T(x*, i*, c*, x)) & s & F(x) & T(x,i,c,x') & F(x')
/// <br/>
/// where x*, i* and c* are the previous-state copy of x,i,c. The query now says: a
/// state that prevents generalization must be either initial or have a predecessor in
/// F & !s. Otherwise it is unreachable and we do not care. This means that we can remove
/// states that are winning for the protagonist from the winning region of they are
/// unreachalbe in the final implementation.
///
/// The modification of the generalization is called optimization RG in the VMCAI paper. The
/// modification of the counterexample-computation is called optimization RC. Depending on
/// the command-line parameters (--mode) this class performs either just one or both
/// optimizations. Experiments suggest that optimization RG is often beneficial, while
/// optimization RC is usually not. Optimization RC leads to a winning region which cannot
/// be turned into a circuit with the standard method.
///
/// @todo Turning a winning region that was computed with optimization RC into a circuit is
///       not yet implemented. However, since optimization RC does not increase the
///       performance in our experiments (in contrast to optimization RG), this is not a
///       severe issue at the moment. We can consider optimization RC as experiment that did
///       not work out well.
/// @note This class is experimental.
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.1.0
class LearnSynthQBFInd : public BackEnd
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
///
/// @param impl_extractor The engine to use for circuit extraction. It will be deleted by
///        this class.
  LearnSynthQBFInd(CNFImplExtractor *impl_extractor);

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~LearnSynthQBFInd();

// -------------------------------------------------------------------------------------------
///
/// @brief Executes this back-end.
///
/// In contrast to the corresponding method of LearnSynthQBF, this method applies some
/// inductive reachability reasoning as an optimization (attempt). The reachability
/// optimization is inspired by the concept of inductiveness relative to a certain clause set
/// used in IC3. Refer to the VMCAI'14 publication "SAT-Based Synthesis Methods for Safety
/// Specs" for a detailed explanation how (and why) this works.
/// The method LearnSynthQBF#run() computes counterexamples by solving:
///   <br/> &nbsp; exists x,i: forall c: exists x',tmp: F(x) & T(x,i,c,x') & !F(x') <br/>
/// and generalizes them by checking
///   <br/> &nbsp; exists x: forall i: exists c,x',tmp: s & F(x) & T(x,i,c,x') & F(x') <br/>
/// repeatedly, where s is the counterexample and F is the current over-approximation of the
/// winning region.
///
/// We know that F(x) is never left by the final implementation. Hence, F is an
/// over-approximation of the reachable states. Now if a state s is not initial and
/// !s & F & T => !s', then we know by induction that the state s is unreachable. We can
/// disregard unreachable states both in the computation of counterexamples as well as during
/// the generalization. This is done by gluing the constraints !s & F & T => !s' and !I(s) to
/// the generalization and check queries as following.
/// The check query becomes:
/// <br/> &nbsp;
///   exists x*,i*,c*,x,i: forall c: exists x',tmp:
///      (I(x) | (x* != x) & F(x*) & T(x*, i*, c*, x)) & F(x) & T(x,i,c,x') & !F(x')
/// <br/>
/// where x*, i* and c* are the previous-state copy of x,i,c. The query now says: find a
/// counterexample-state x that is either initial or has a predecessor x* in F such that x*
/// is different from x. (Otherwise the counterexample-state is unreachable.)
/// The generalization query becomes:
/// <br/> &nbsp;
///   exists x*,i*,c*,x: forall i: exists c,x',tmp:
///      (I(x) | F(x*) & !s* & T(x*, i*, c*, x)) & s & F(x) & T(x,i,c,x') & F(x')
/// <br/>
/// where x*, i* and c* are the previous-state copy of x,i,c. The query now says: a
/// state that prevents generalization must be either initial or have a predecessor in
/// F & !s. Otherwise it is unreachable and we do not care. This means that we can remove
/// states that are winning for the protagonist from the winning region of they are
/// unreachable in the final implementation.
///
/// The modification of the generalization is called optimization RG in the VMCAI paper. The
/// modification of the counterexample-computation is called optimization RC. Depending on
/// the command-line parameters (--mode) this class performs either just one or both
/// optimizations. Experiments suggest that optimization RG is often beneficial, while
/// optimization RC is usually not.
///
/// @note When optimization RC in enabled, we cannot extract a circuit from the winning
///       region at the moment.
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
/// @brief Uses the standard method to compute the winning region, with inductive reasoning.
///
/// This method works like LearnSynthQBF#computeWinningRegionOne() but using inductive
/// reasoning regarding reachable states as an optimization (as explained in the description
/// of the class).
///
/// @return True if the specification was realizable, false otherwise.
  bool computeWinningRegionOne();

// -------------------------------------------------------------------------------------------
///
/// @brief Computes the winning region, always computing all counterexample generalizations.
///
/// This method works like LearnSynthQBF#computeWinningRegionOne() but using inductive
/// reasoning regarding reachable states as an optimization (as explained in the description
/// of the class).
///
/// @return True if the specification was realizable, false otherwise.
  bool computeWinningRegionAll();

// -------------------------------------------------------------------------------------------
///
/// @brief Computes a counterexample-state using a QBF-solver with inductive reasoning.
///
/// This method works like LearnSynthQBF#computeCounterexampleQBF() but using inductive
/// reasoning regarding reachable states as an optimization.  That is, instead of solving
///  <br/> &nbsp; exists x,i: forall c: exists x',tmp: F(x) & T(x,i,c,x') & !F(x') <br/>
/// we solve
/// <br/> &nbsp;
///   exists x*,i*,c*,x,i: forall c: exists x',tmp:
///      (I(x) | (x* != x) & F(x*) & T(x*, i*, c*, x)) & F(x) & T(x,i,c,x') & !F(x')
/// <br/>
/// to obtain a counterexample. The query now says: find a counterexample-state x that is
/// either initial or has a predecessor x* in F such that x* is different from x.
/// (Otherwise the counterexample-state is unreachable.)
///
/// If optimization RC is disabled (#do_reach_check_ is false), then counterexamples are
/// computed in the standard way.
///
/// @param ce An empty vector. The computed counterexample is stored in this vector in form
///        of a cube over the state variables. The cube does not necessarily contain all
///        state variables. If some state variables are irrelevant, then they may be omitted.
/// @return True if a counterexample was found, false if no counterexample exists.
  bool computeCounterexampleQBF(vector<int> &ce);

// -------------------------------------------------------------------------------------------
///
/// @brief Computes a blocking clause for a counterexample.
///
/// This method works like LearnSynthQBF#computeBlockingClause() but using inductive
/// reasoning regarding reachable states as an optimization.  That is, instead of solving
///  <br/> &nbsp; exists x: forall i: exists c,x',tmp: s & F(x) & T(x,i,c,x') & F(x') <br/>
/// where s is the counterexample-state to generalize, we solve
/// <br/> &nbsp;
///       exists x*,i*,c*,x: forall i: exists c,x',tmp:
///        (I(x) | F(x*) & !s* & T(x*, i*, c*, x)) & s & F(x) & T(x,i,c,x') & F(x')
/// <br/>
/// The query now says: a state that prevents generalization must be either initial or have a
/// predecessor in F & !s. Otherwise it is unreachable and we do not care. This means that we
/// can remove states that are winning for the protagonist from the winning region of they are
/// unreachable in the final implementation.
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
/// This method works like LearnSynthQBF#computeAllBlockingClauses() but using inductive
/// reasoning regarding reachable states as an optimization.  That is, instead of solving
///  <br/> &nbsp; exists x: forall i: exists c,x',tmp: s & F(x) & T(x,i,c,x') & F(x') <br/>
/// where s is the counterexample-state to generalize, we solve
/// <br/> &nbsp;
///       exists x*,i*,c*,x: forall i: exists c,x',tmp:
///        (I(x) | F(x*) & !s* & T(x*, i*, c*, x)) & s & F(x) & T(x,i,c,x') & F(x')
/// <br/>
/// The query now says: a state that prevents generalization must be either initial or have a
/// predecessor in F & !s. Otherwise it is unreachable and we do not care. This means that we
/// can remove states that are winning for the protagonist from the winning region of they are
/// unreachable in the final implementation.
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
/// The CNF used for computing counterexamples is F & T & !F' if optimization RC is disabled
/// (#check_reach_ = false) , or (I | (x* != x) & F* & T*) & F & T & !F' if optimization
/// RC in enabled (#check_reach_ = true). Here F is the current over-approximation of the
/// winning region (in #winning_region_). This formula is put into #check_cnf_ by this method.
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
/// The CNF used for generalizing counterexamples is (I | F* & T*) & F & T & F' where F is
/// the current over-approximation of the winning region (in #winning_region_).
///
/// @param take_small_win If this parameter is true (or omitted) the #winning_region_ is used
///        to compute the #check_cnf_. Otherwise, the #winning_region_large_ is used. The
///        difference between these two winning region versions is as follows. From time to
///        we remove redundant clauses (clauses implied by other clauses) from
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
/// This method works like LearnSynthQBF#generalizeCounterexample() but using inductive
/// reasoning regarding reachable states as an optimization.  That is, instead of solving
///  <br/> &nbsp; exists x: forall i: exists c,x',tmp: s & F(x) & T(x,i,c,x') & F(x') <br/>
/// where s is the counterexample-state to generalize, we solve
/// <br/> &nbsp;
///       exists x*,i*,c*,x: forall i: exists c,x',tmp:
///        (I(x) | F(x*) & !s* & T(x*, i*, c*, x)) & s & F(x) & T(x,i,c,x') & F(x')
/// <br/>
/// The query now says: a state that prevents generalization must be either initial or have a
/// predecessor in F & !s. Otherwise it is unreachable and we do not care. This means that we
/// can remove states that are winning for the protagonist from the winning region of they are
/// unreachable in the final implementation.
///
/// @param ce The counterexample to generalize (it will be overwritten by its own
///        generalization). This is a complete or partial cube over the present state
///        variables.
/// @param check_sat False if ce is a counterexample (for which a generalization exists) for
///        sure. False if this may not be the case and needs to be checked.
/// @return True if the passed vector is a valid (generalization of a) counterexample,
///         false otherwise.
  bool generalizeCounterexample(vector<int> &ce, bool check_sat = true) const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the previous-state copy of a literal.
///
/// @param literal The literal to transform.
/// @return The previous-state copy of a literal.
  int presentToPrevious(int literal) const;

// -------------------------------------------------------------------------------------------
///
/// @brief Computes the previous-state copy of a cube or clause.
///
/// @param cube_or_clause A cube or clause (in form of a vector of literals) over the
///        present state variables. This vector is overwritten by the corresponding cube of
///        clause over the previous-state literals (i.e., all literals are replaced by their
///        previous-state copy).
  void presentToPrevious(vector<int> &cube_or_clause) const;

// -------------------------------------------------------------------------------------------
///
/// @brief Computes the previous-state copy of a CNF.
///
/// @param cnf A CNF formula over the present state variables. This CNF is overwritten by the
///        corresponding CNF over the previous-state literals (i.e., all literals are
///        replaced by their previous-state copy).
  void presentToPrevious(CNF &cnf) const;

// -------------------------------------------------------------------------------------------
///
/// @brief Checks a winning region computed with optimization RC for correctness.
///
/// We usually check the correctness of the winning region in debug-mode by calling
/// Utils#debugCheckWinReg(). However, this method of Utils does not work for winning regions
/// that have been computed with optimization RC enabled. In such cases we this method.
  void debugCheckWinRegReach() const;

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
/// If optimization RC in enabled (#do_reach_check_ is true), then this quantifier prefix is
///  <br/> &nbsp; exists x*,i*,c*,x,i: forall c: exists x',tmp: <br/>
/// otherwise (if #do_reach_check_ is false) it is
///  <br/> &nbsp; exists x,i: forall c: exists x',tmp: <br/>
  vector<pair<VarInfo::VarKind, QBFSolver::Quant> > check_quant_;

// -------------------------------------------------------------------------------------------
///
/// @brief The CNF matrix of the QBF for computing counterexamples.
///
/// If optimization RC in enabled (#do_reach_check_ is true), then this CNF is
///  <br/> &nbsp; (I | (x* != x) & F* & T*) & F & T & !F' <br/>
/// otherwise (if #do_reach_check_ is false) it is
///  <br/> &nbsp; F & T & !F' <br/>
/// where F is the current over-approximation of the winning region (store in
/// #winning_region_).
  CNF check_cnf_;

// -------------------------------------------------------------------------------------------
///
/// @brief The quantifier prefix of the QBF for generalizing counterexamples.
///
/// This quantifier prefix is always
///   exists x*,i*,c*,x: forall i: exists c,x',tmp:
  vector<pair<VarInfo::VarKind, QBFSolver::Quant> > gen_quant_;

// -------------------------------------------------------------------------------------------
///
/// @brief The CNF matrix of the QBF for generalizing counterexamples.
///
/// This CNF is always (I | F* & T*) & F & T & F', where F is the #winning_region_.
  CNF generalize_clause_cnf_;

// -------------------------------------------------------------------------------------------
///
/// @brief A map from present-state variables to their previous-state copy.
  vector<int> current_to_previous_map_;

// -------------------------------------------------------------------------------------------
///
/// @brief A literal that is true if the current state is initial and false otherwise.
///
/// The clauses assigning the literal are part of #prev_trans_or_initial_. Since
/// #prev_trans_or_initial_ is part of #check_cnf_ and #generalize_clause_cnf_, this literal
/// is also assigned by #check_cnf_ (if optimization RC is enabled, i.e., if #do_reach_check_
/// is true) and #generalize_clause_cnf_.
  int current_state_is_initial_;

// -------------------------------------------------------------------------------------------
///
/// @brief Says: the current state is initial or the previous transition relation holds.
///
/// This CNF expresses that the current state is initial or the previous-state copy of the
/// transition relation holds. This is an important building block for both #check_cnf_ and
/// #generalize_clause_cnf_.
  CNF prev_trans_or_initial_;

// -------------------------------------------------------------------------------------------
///
/// @brief Says: #prev_trans_or_initial_ and the current state is different from the previous.
///
/// If the optimization RC is disabled (#do_reach_check_ is false) then this CNF is simply
/// TRUE. This CNF is an important building block for #check_cnf_ (the part that stays
/// constant over all iterations). Hence, it is computed once and copied whenever needed.
  CNF check_reach_;

// -------------------------------------------------------------------------------------------
///
/// @brief Enables or disables optimization RC.
///
/// Optimization RC (inductive reachability checks during the computation of counterexamples)
/// is enabled if this flag is true, otherwise it is disabled.
  bool do_reach_check_;

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
  LearnSynthQBFInd(const LearnSynthQBFInd &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  LearnSynthQBFInd& operator=(const LearnSynthQBFInd &other);

};

#endif // LearnSynthQBFInd_H__
