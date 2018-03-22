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
/// @file ParallelLearner.h
/// @brief Contains many class declarations, all related to a parallelized implementation.
/// @todo Split this file into several files, one per class, and combine them in a namespace.
// -------------------------------------------------------------------------------------------

#ifndef ParallelLearner_H__
#define ParallelLearner_H__

#include "defines.h"
#include "BackEnd.h"
#include "CNF.h"
#include "LearnStatisticsSAT.h"
#include <thread>
#include <mutex>
#include "QBFSolver.h"
#include "UnivExpander.h"

class SatSolver;
class ClauseExplorerSAT;
class IFM13Explorer;
class IFMProofObligation;
class ClauseMinimizerQBF;
class CounterGenSAT;
class TemplExplorer;
class CNFImplExtractor;
class DepQBFApi;


// -------------------------------------------------------------------------------------------
///
/// @class PrevStateInfo
/// @brief Just a container for previous-state information.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.2.0
class PrevStateInfo
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Standard constructor.
///
/// @param use_ind True if previous state info (optimization RG) should be used during the
///        generalization of clauses, and False otherwise.
  PrevStateInfo(bool use_ind);

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// @param other The source for creating the copy.
  PrevStateInfo(const PrevStateInfo &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  PrevStateInfo& operator=(const PrevStateInfo &other);

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
/// @brief A flag indicating if optimization RG should be used.
///
/// See LearnSynthQBFInd for an explanation.
  bool use_ind_;

// -------------------------------------------------------------------------------------------
///
/// @brief Says: the current state is initial or the previous transition relation holds.
///
/// This CNF expresses that the current state is initial or the previous-state copy of the
/// transition relation holds. This is an important building block for the CNF to
/// generalize counterexamples if optimization RG is enabled.
  CNF prev_trans_or_initial_;

// -------------------------------------------------------------------------------------------
///
/// @brief A map from present-state variables to their previous-state copy.
  vector<int> current_to_previous_map_;

// -------------------------------------------------------------------------------------------
///
/// @brief A literal that is true if the current state is initial and false otherwise.
///
/// The clauses assigning the literal are part of #prev_trans_or_initial_.
  int current_state_is_initial_;

// -------------------------------------------------------------------------------------------
///
/// @brief The previous-state copy of the literal that characterizes all safe states.
  int prev_safe_;

// -------------------------------------------------------------------------------------------
///
/// @brief Activation variables for a previous state clause.
///
/// If px_unused_[i] is true, then this means that the previous state variable nr i does
/// not occur in the previous-state clause controlled by these variables. If set to
/// false then it occurs.
  vector<int> px_unused_;

// -------------------------------------------------------------------------------------------
///
/// @brief Activation variables for a previous state clause.
///
/// If px_neg_[i] is true, then this means that the previous state variable nr i occurs
/// negated in the previous-state clause controlled by these variables. If set to false,
/// then it occurs unnegated.
  vector<int> px_neg_;

// -------------------------------------------------------------------------------------------
///
/// @brief Activation literals for a previous state clause.
///
/// Their value is defined by px_unused_ and px_neg_.
  vector<int> px_act_;

// -------------------------------------------------------------------------------------------
///
/// @brief The vector of all variables of type VarInfo::PREV.
  vector<int> prev_vars_;

};


// -------------------------------------------------------------------------------------------
///
/// @class ParallelLearner
/// @brief The coordinator of the parallelized implementation.
///
/// This class implements a parallelized version of the learning-based synthesis methods.
/// The idea is to combine various different techniques to refine an over-approximation of
/// the winning region with new clauses. There is a lot of flexibility to play with. We can
/// have threads discovering new clauses with SAT- or QBF-based learning techniques, we can
/// have threads minimizing existing clauses with a QBF solver, etc. All threads share a
/// common clause database. If (new or refined) clauses are discovered, they are communicated
/// to all threads such that all of them can benefit from the new clauses in the next call.
/// The threads performing actual work are implemented in other classes. This class is just
/// the coordinator which starts the worker-threads and forwards information about new
/// clauses to the worker-threads.
///
/// The parallel implementation has two advantages. First, it exploits hardware parallelism
/// (modern CPUs have many cores), which means (potentially) shorter computation times.
/// Second, it is an easy way of combining different methods. With one thread, one would have
/// come up with points where to switch from one method to the other and back. Hence, it may
/// also be useful to use the parallelized implementation even if only one core is available.
///
/// The question which methods we should combine how in order to achieve the best speed-up is
/// not yet fully solved. This class is just one attempt, and can be seen as a playground for
/// combining different methods.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.2.0
class ParallelLearner : public BackEnd
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
///
/// @param nr_of_threads The number of threads to create. This does not have to be the number
///        of cores in your CPU. It can also make sense to combine several methods running in
///        several threads on one single core. The methods can complement each other and
///        achieve a speedup compared to one single method in isolation.
/// @param impl_extractor The engine to use for circuit extraction. It will be deleted by
///        this class.
  ParallelLearner(size_t nr_of_threads, CNFImplExtractor *impl_extractor);

// -------------------------------------------------------------------------------------------
///
/// @brief Desctructor.
  virtual ~ParallelLearner();

// -------------------------------------------------------------------------------------------
///
/// @brief Runs the back-end.
///
/// This method mainly starts the worker-threads and waits until they are finished. The
/// ParallelLearner itself does not do any work. It does not even run in an own thread. All
/// the work is done in the worker-threads.
///
/// @return True if the specification was realizable, false otherwise.
  virtual bool run();

// -------------------------------------------------------------------------------------------
///
/// @brief Notifies all worker-threads that a new clause of the winning region is available.
///
/// It also adds this clause to our global clause database #winning_region_.
/// @param clause The new clause that has been discovered.
/// @param src An integer number defining which kind of worker-thread discovered the clause.
///        Some worker-threads may treat clauses from different sources in a special way.
  void notifyNewWinRegClause(const vector<int> &clause, int src);

// -------------------------------------------------------------------------------------------
///
/// @brief Notifies all ClauseExplorerSAT-threads that a new U-clause is available.
///
/// In essence, the ClauseExplorerSAT implements the same SAT-based learning method as
/// LearnSynthSAT. It has just a few additional features which allow the implementation to
/// communicate with others.  A U-clause states that a certain state-input is useless for
/// the antagonist in trying to leave the winning region (see LearnSynthSAT for details).
/// If one ClauseExplorerSAT discovers such a clause, it communicates it to all other
/// ClauseExplorerSAT-instances such that they can also benefit from it and do not need to
/// re-discover the same fact again. For that to work, all ClauseExplorerSAT must be
/// synchronized to a certain extend: All ClauseExplorerSAT must do solver restarts
/// simultaneously. We pass the restart-level (the number of restarts done so far) as
/// additional parameter so that every ClauseExplorerSAT-instances can then decide if this
/// clause is helpful for him, or already out-dated.
///
/// @param clause A new U-clause stating that a certain state-input is useless for the
///        antagonist in trying to leave the winning region (see LearnSynthSAT for details).
/// @param level The restart level (the number of solver restarts that have already been
///        performed. This information allows the receiver instance to decide whether or not
///        this clause is useful for her (or if it is already out-dated).
  void notifyNewUselessInputClause(const vector<int> &clause, int level);

// -------------------------------------------------------------------------------------------
///
/// @brief Adds a new counterexample to the ParallelLearner's database.
///
/// A counterexample is a state-input combination with which the antagonist can enforce to
/// leave the winning region. The counterexample is stored together with a generalization
/// (some literals dropped) that has already been computed. This information can then be used
/// by CounterGenSAT-threads. They take such a counterexample and compute all other
/// generalizations (generalizations other than 'gen').
///
/// @param ce This is a cube of the current-state variables and the uncontrollable inputs.
///        With this state-input combination, the antagonist can enforce to leave the winning
///        region. That is, there are no control-values such that the successor state is in
///        the winning region again.
/// @param gen One generalization of the counterexample. This is a subset of the literals of
///        ce such that no control-values can lead to a successor state inside the winning
///        region.
  void notifyNewCounterexample(const vector<int> &ce, const vector<int> &gen);

// -------------------------------------------------------------------------------------------
///
/// @brief Computes a new restart point and notifies it to all ClauseExplorerSAT-instances.
///
/// The ClauseExplorerSAT update their next-state copy of the winning region only in a lazy
/// manner. This allows them to use incremental SAT-solving more effectively. See
/// LearnSynthSAT for a description. If no more solutions exists with one next-state copy of
/// the winning region, it must be updated to the newest version. This is done by this method.
/// A new next-state copy of the winning region is computed and communicated to all
/// ClauseExplorerSAT-instances. It is absolutely important that all ClauseExplorerSAT
/// instances work with the same next-state copy of the winning region. Otherwise, they could
/// not exchange U-clauses (or rather, the U-clause discovered by one thread would not make
/// sense for the other). See also #notifyNewUselessInputClause().
  void triggerExplorerRestart();

// -------------------------------------------------------------------------------------------
///
/// @brief The first restart is special because mode 0 threads are already allowed to work.
  void triggerInitialMode1Restart();


// -------------------------------------------------------------------------------------------
///
/// @brief A container with information about previous-state variables.
  PrevStateInfo psi_;


// -------------------------------------------------------------------------------------------
///
/// @brief An integer number containing the realizability verdict.
///
/// There are three possible value.
/// <ul>
///  <li> UNKNOWN = 0: used as long as no thread has found a solution.
///  <li> REALIZABLE = 1: used if some thread concludes that the spec must be realizable.
///  <li> UNREALIZABLE = 2: used if some thread concludes that the spec must be unrealizable.
/// </ul>
/// All worker-threads poll this flag from time to time. If the value is still UNKNOWN they
/// continue to work. If they see that the values is not UNKNOWN anymore they stop assuming
/// that some other thread has already found the solution. If one thread finds an answer to
/// the realizability question it sets the flag (the #winning_region_ must be precise enough
/// then already).
  volatile int result_;

// -------------------------------------------------------------------------------------------
///
/// @brief The current version of the winning region.
///
/// All worker-threads discover or minimize clauses of the winning region. This is the
/// current version of the winning region.
  CNF winning_region_;

// -------------------------------------------------------------------------------------------
///
/// @brief A lock that protects the #winning_region_ from race-conditions.
///
/// Many worker-threads refine the #winning_region_ (add new clauses). This lock ensures that
/// only one thread is modifying the #winning_region_ at one time.
  mutex winning_region_lock_;

// -------------------------------------------------------------------------------------------
///
/// @brief All clauses of the winning region that have not yet been minimized.
///
/// Whenever a clause is added to #winning_region_, it is also added to this field.
/// ClauseMinimizerQBF-instances then take clauses from #unminimized_clauses_ and try to
/// minimize them further. This is done to prevent that clauses are minimized twice or
/// multiple times.
  CNF unminimized_clauses_;

// -------------------------------------------------------------------------------------------
///
/// @brief A lock that protects the #unminimized_clauses_ from race-conditions.
///
/// Many worker-threads refine the #unminimized_clauses_ (add new clauses). This lock ensures
/// that only one thread is modifying the #unminimized_clauses_ at one time.
  mutex unminimized_clauses_lock_;

// -------------------------------------------------------------------------------------------
///
/// @brief The counterexample-cubes together with their computed generalizations.
///
/// The first item of the pairs stored in this list is always the counterexample itself. The
/// second item of the pair is the generalization that has already been computed.
///
/// A counterexample is a state-input combination with which the antagonist can enforce to
/// leave the winning region. The generalization is a sub-cube of the counterexample. This
/// information can then be used by CounterGenSAT-threads. They take such a counterexample
/// and compute all other generalizations.
  list<pair<vector<int>, vector<int> > > counterexamples_;

// -------------------------------------------------------------------------------------------
///
/// @brief A lock that protects the #counterexamples_ from race-conditions.
///
/// Many worker-threads modify the #counterexamples_ (add new ones). This lock ensures
/// that only one thread is modifying the #counterexamples_ at one time.
  mutex counterexamples_lock_;

// -------------------------------------------------------------------------------------------
///
/// @brief A lock used to synchronize the printing of debugging messages.
///
/// Debugging race conditions or other bugs in this implementation is tough. In order to
/// prevent debugging messages from being interleaved strangely, this lock is used. It makes
/// debug messages printed with the PLOG macro appear one after the other.
  static mutex print_lock_;

// -------------------------------------------------------------------------------------------
///
/// @brief A lock that must be hold when the VarManager is modified.
///
/// Sometimes (at the moment only when doing restarts in #triggerExplorerRestart()) we need
/// to modify the VarManager, e.g., if we create new variables. This lock is used to prevent
/// race-conditions in these operations. It must be held whenever the VarManager is modified.
  mutex var_man_lock_;

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief The number of threads to instantiate and execute.
  size_t nr_of_threads_;

// -------------------------------------------------------------------------------------------
///
/// @brief The TemplExplorer-instances to execute in a separate thread.
  vector<TemplExplorer*> templ_explorers_;

// -------------------------------------------------------------------------------------------
///
/// @brief The ClauseExplorerSAT-instances to execute in a separate thread.
  vector<ClauseExplorerSAT*> clause_explorers_;

// -------------------------------------------------------------------------------------------
///
/// @brief The IFM13Explorer-instances to execute in a separate thread.
  vector<IFM13Explorer*> ifm_explorers_;

// -------------------------------------------------------------------------------------------
///
/// @brief The ClauseMinimizerQBF-instances to execute in a separate thread.
  vector<ClauseMinimizerQBF*> clause_minimizers_;

// -------------------------------------------------------------------------------------------
///
/// @brief The CounterGenSAT-instances to execute in a separate thread.
  vector<CounterGenSAT*> ce_generalizers_;

// -------------------------------------------------------------------------------------------
///
/// @brief Stores and maintains statistics and performance measures.
///
/// Basically, this is a merge of the statistics from all worker-threads.
  LearnStatisticsSAT statistics_;

// -------------------------------------------------------------------------------------------
///
/// @brief The engine to use for circuit extraction.
///
/// It will be deleted by this class (in the destructor).
  CNFImplExtractor *impl_extractor_;

// -------------------------------------------------------------------------------------------
///
/// @brief The list of variables to keep in solver_i within the ClauseExplorerSAT.
  vector<int> vars_to_keep_i_;

// -------------------------------------------------------------------------------------------
///
/// @brief The expander for eliminating universal quantifiers in our heuristic.
  UnivExpander expander_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  ParallelLearner(const ParallelLearner &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  ParallelLearner& operator=(const ParallelLearner &other);

};


// -------------------------------------------------------------------------------------------
///
/// @class ClauseExplorerSAT
/// @brief Implements the method of LearnSynthSAT in a parallelized way.
///
/// This class basically implements the same method as LearnSynthSAT. It only has a few
/// additional methods to communicate with other threads. Whenever it finds a new clause of
/// the winning region, it communicates this clause to all other worker-threads (via the
/// ParallelLearner, which acts as a coordinator). Also, if it finds out that some state-input
/// combination is not useful for the antagonist in trying to leave the winning region, it
/// communicates a clause to all other instances of the same class. For that to work, all
/// ClauseExplorerSAT-instances must be synchronized to a certain extend: All
/// ClauseExplorerSAT-instances must do solver restarts simultaneously, i.e., they must
/// all work with the same next-state copy of the winning region. This is done by
/// coordinating and communicating solver restarts via the ParallelLearner.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.2.0
class ClauseExplorerSAT
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
///
/// @param instance_nr A unique instance number. It is used to bring in some asymmetry between
///        the different ClauseExplorerSAT-instances: depending on the instance number, the
///        instance selects a different SatSolver to use.
/// @param coordinator A reference to the coordinator. All communication to other
///        worker-threads is done via the coordinator.
/// @param psi A container for previous-state information (needed when optimization RG is
///        enabled).
  ClauseExplorerSAT(size_t instance_nr, ParallelLearner &coordinator, PrevStateInfo &psi);

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~ClauseExplorerSAT();

// -------------------------------------------------------------------------------------------
///
/// @brief The work-horse of this class.
///
/// This method computes clauses refining the winning region, just like LearnSynthSAT. It only
/// has a few additional methods to communicate with other threads. Whenever it finds a new
/// clause of the winning region, it communicates it to all other worker-threads (via the
/// ParallelLearner, which acts as a coordinator). Also, if it finds out that some state-input
/// combination is not useful for the antagonist in trying to leave the winning region, it
/// communicates a clause to all other instances of the same class. In every iteration, it
/// also considers all new incoming clauses found by others. All
/// ClauseExplorerSAT-instances must be synchronized to a certain extend: All
/// ClauseExplorerSAT-instances must do solver restarts simultaneously, i.e., they must
/// all work with the same next-state copy of the winning region. This is done by
/// coordinating and communicating solver restarts via the ParallelLearner.
  void exploreClauses();

// -------------------------------------------------------------------------------------------
///
/// @brief The method must be called before new information is passed to this object.
///
/// After the new information has been set, #notifyAfterNewInfo() must be called. This way
/// the coordinator (the ParallelLearner) can inform all ClauseExplorerSAT-instances
/// simultaneously: It calls #notifyBeforeNewInfo() on all instances, then communicates the
/// new information to all instances, then calls #notifyAfterNewInfo() on all instances.
/// This way, no thread can work with the new information before the others got it.
/// This method must be used for #notifyNewWinRegClause(), #notifyNewUselessInputClause(),
/// and #notifyNewUselessInputClause().
/// If this method finds out that the specification is realizable or unrealizable, it sets the
/// flag ParallelLearner::result_ accordingly. It also polls this flag regularly. If it has
/// been set by some other thread, it quits.
  void notifyBeforeNewInfo();


// -------------------------------------------------------------------------------------------
///
/// @brief The method must be called after new information is passed to this object.
///
/// See #notifyBeforeNewInfo() for an explanation.
  void notifyAfterNewInfo();

// -------------------------------------------------------------------------------------------
///
/// @brief Notifies this worker about a new winning region clause.
///
/// @note Be sure to call #notifyBeforeNewInfo() before this method and #notifyAfterNewInfo()
///       after calling this method.
/// @param clause The new clause that has been discovered.
/// @param src An integer number defining which kind of worker-thread discovered the clause.
///        For performance reasons, this method distinguishes between clauses discovered by
///        ClauseExplorerSAT-instances, and clauses discovered by other kinds of workers.
  void notifyNewWinRegClause(const vector<int> &clause, int src);

// -------------------------------------------------------------------------------------------
///
/// @brief Notifies this ClauseExplorerSAT-instance that a new U-clause is available.
///
/// A U-clause states that a certain state-input is useless for
/// the antagonist in trying to leave the winning region (see LearnSynthSAT for details).
/// If one ClauseExplorerSAT discovers such a clause, it communicates it to all other
/// ClauseExplorerSAT-instances such that they can also benefit from it and do not need to
/// re-discover the same fact again.
///
/// @note Be sure to call #notifyBeforeNewInfo() before this method and #notifyAfterNewInfo()
///       after calling this method.
/// @param clause A new U-clause stating that a certain state-input is useless for the
///        antagonist in trying to leave the winning region (see LearnSynthSAT for details).
/// @param level The restart level (the number of solver restarts that have already been
///        performed. This information allows this instance to decide whether or not
///        this clause is useful for her (or if it is already out-dated).
  void notifyNewUselessInputClause(const vector<int> &clause, int level);

// -------------------------------------------------------------------------------------------
///
/// @brief Notifies this ClauseExplorerSAT-instance that it should restart its solver.
///
/// The ClauseExplorerSAT update their next-state copy of the winning region only in a lazy
/// manner. This allows them to use incremental SAT-solving more effectively. See
/// LearnSynthSAT for a description. If no more solutions exists with one next-state copy of
/// the winning region, it must be updated to the newest version. This method tells this
/// instance that such a newer version should be used.
///
/// It is absolutely important that all ClauseExplorerSAT
/// instances work with the same next-state copy of the winning region. Otherwise, they could
/// not exchange U-clauses (or rather, the U-clause discovered by one thread would not make
/// sense for the other).
///
/// @see #notifyNewUselessInputClause()
/// @see ParallelLearner::triggerExplorerRestart()
/// @param solver_i The new solver to continue with.
  void notifyRestart(SatSolver *solver_i);

// -------------------------------------------------------------------------------------------
///
/// @brief Instantiates a fresh solver for computing counterexample-states.
///
/// We create a fresh solver upon every restart.
///
/// @return A fresh solver for computing counterexample-states.
  SatSolver *getFreshISolver() const;

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the statistics and performance measures computed by this object.
///
/// It is used by the ParallelLearner to merge the statistics of the different workers.
///
/// @return The statistics and performance measures computed by this object.
  const LearnStatisticsSAT& getStatistics() const;

// -------------------------------------------------------------------------------------------
///
/// @brief The current mode of operation.
///
/// If set to 1, this instance will compute new clauses by performing universal expansion on
/// the CNF formula to compute counterexample-states. If expansion makes the formula too big,
/// we automatically fall back to mode 0, which is the default mode implemented also in
/// LearnSynthSAT.
  volatile int mode_;

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief Considers new information discovered by other threads in the solvers.
///
/// New information includes:
/// <ul>
///  <li> A new CNF for the solver to compute counterexamples (we need to do a restart). This
///       info came from #notifyRestart().
///  <li> New clauses refining the winning region (coming from #notifyNewWinRegClause()).
///  <li> New clauses defining useless state-input combinations (coming from
///       #notifyNewUselessInputClause()).
/// </ul>
  void considerNewInfoFromOthers();

// -------------------------------------------------------------------------------------------
///
/// @brief Checks if some other thread is computing a restart. If yes: waits until it is done.
///
/// This is just a performance optimization. Before we trigger a restart, we check if someone
/// else is already working on a restart (computing a restart point takes some time because
/// we also compress the CNF of the winning region for performance reasons). If so, we wait
/// until it is done. Otherwise, we would do two very similar restarts one after the other.
/// This would be correct, but more inefficient.
///
/// @return True if a restart is available. False otherwise.
  bool waitUntilOngoingRestartDone();

// -------------------------------------------------------------------------------------------
///
/// @brief A unique instance number.
///
/// It is used to bring in some asymmetry between the different ClauseExplorerSAT-instances:
/// depending on the instance number, the instance selects a different SatSolver to use.
/// Furthermore, it is used for debugging to print more meaningful messages (WHO is printing
/// a message).
  size_t instance_nr_;

// -------------------------------------------------------------------------------------------
///
/// @brief Stores and maintains statistics and performance measures.
///
/// Basically, this is a merge of the statistics from all worker-threads.
  LearnStatisticsSAT statistics_;

// -------------------------------------------------------------------------------------------
///
/// @brief A reference to the coordinator.
///
/// All communication to other worker-threads is done via the coordinator.
  ParallelLearner &coordinator_;

// -------------------------------------------------------------------------------------------
///
/// @brief A flag indicating if the next-state copy of the winning region is accurate.
///
/// @see LearnSynthSAT
  bool precise_;

// -------------------------------------------------------------------------------------------
///
/// @brief The solver for computing counterexample-candidates.
  SatSolver *solver_i_;

// -------------------------------------------------------------------------------------------
///
/// @brief The next solver_i_ to use. It contains the CNFs after the next restart.
  SatSolver *next_solver_i_;

// -------------------------------------------------------------------------------------------
///
/// @brief The name of solver_i_. It is used for creating a fresh solver instance in restarts.
  string solver_i_name_;

// -------------------------------------------------------------------------------------------
///
/// @brief The solver for checking counterexample-candidates.
///
/// It is also used to generalize counterexamples if optimization RG is disabled.
  SatSolver *solver_ctrl_;

// -------------------------------------------------------------------------------------------
///
/// @brief  A set of variables the solver should not optimize by the SAT-solver.
///
/// These are all variables except for temporary ones (type VarInfo::TMP).
  vector<int> vars_to_keep_;

// -------------------------------------------------------------------------------------------
///
/// @brief A lock to protect many fields from race conditions.
///
/// This lock protects the fields
/// <ul>
///  <li> #next_solver_i_
///  <li> #new_win_reg_clauses_for_solver_i_
///  <li> #new_win_reg_clauses_for_solver_ctrl_
///  <li> #new_foreign_win_reg_clauses_for_solver_i_
///  <li> #new_useless_input_clauses_
///  <li> #new_useless_input_clauses_level_
/// </ul>
/// From race conditions. These fields are modified by other threads when new information
/// comes in. They are read by this thread when this new information is taken into account.
///
/// @todo A more fine-grained locking could improve the performance.
  mutex new_info_lock_;

// -------------------------------------------------------------------------------------------
///
/// @brief All new winning region clauses that have not yet been fed into solver_i_.
///
/// This CNF contains only clauses that have been discovered by ClauseExplorerSAT-threads.
/// Clauses discovered by other threads are stored in
/// #new_foreign_win_reg_clauses_for_solver_i_ because they are treaded specially (for
/// performance reasons).
  CNF new_win_reg_clauses_for_solver_i_;

// -------------------------------------------------------------------------------------------
///
/// @brief All new winning region clauses that have not yet been fed into solver_ctrl_.
///
/// We do not distinguish clauses discovered by ClauseExplorerSAT-threads and clauses
/// discovered by other threads. We also add all clauses to solver_ctrl_ immediately.
  CNF new_win_reg_clauses_for_solver_ctrl_;

// -------------------------------------------------------------------------------------------
///
/// @brief All foreign winning region clauses that have not yet been fed into solver_i_.
///
/// This CNF contains only clauses that have not been discovered by ClauseExplorerSAT-threads.
/// Clauses discovered by ClauseExplorerSAT-threads are stored in
/// #new_win_reg_clauses_for_solver_i_. Clauses discovered by other threads are handled in
/// a special way for performance reasons. They are not added immediately because this could
/// make the solver imprecise, which prevents it from terminating. Foreign clauses are only
/// added if the #solver_i_ is already imprecise (precise_ is false). Otherwise, other
/// threads could spam ClauseExplorerSAT-threads with clauses that prevent the
/// ClauseExplorerSAT-threads from being precise long enough to conclude realizability.
/// (Acutally I don't know if this is really an issue, but the optimization is implemented
/// just in case).
  CNF new_foreign_win_reg_clauses_for_solver_i_;

// -------------------------------------------------------------------------------------------
///
/// @brief All U-clauses that have not yet been added to #solver_i_.
///
/// A U-clause states that a certain state-input is useless for
/// the antagonist in trying to leave the winning region (see LearnSynthSAT for details).
  CNF new_useless_input_clauses_;

// -------------------------------------------------------------------------------------------
///
/// @brief The level on which the clauses in #new_useless_input_clauses_ have been found.
///
/// 'Level' means the number of solver restarts that have already been performed.
/// #new_useless_input_clauses_ are only considered if their level matches the
/// #restart_level_ of this object.
  int new_useless_input_clauses_level_;

// -------------------------------------------------------------------------------------------
///
/// @brief The number of solver restarts that have already been performed.
  int restart_level_;

// -------------------------------------------------------------------------------------------
///
/// @brief The restart level we have after we consider #next_solver_i_.
///
/// The thing is: this thread can skip a restart level completely if it does not get
/// scheduled often. In this case, we have to increase our restart level by more than one
/// when we actually do the restart (take #next_solver_i_). The new restart level is
/// tracked by this field.
  int new_restart_level_;

// -------------------------------------------------------------------------------------------
///
/// @brief The solver for generalizing counterexamples if optimization RG is enabled.
  SatSolver *solver_ctrl_ind_;

// -------------------------------------------------------------------------------------------
///
/// @brief Information about the previous states if optimization RG is enabled.
  const PrevStateInfo &psi_;

// -------------------------------------------------------------------------------------------
///
/// @brief The current local knowledge about the winning region.
///
/// It is used in a heuristic to restart solver_ctrl_ind_ from time to time.
  CNF win_;

// -------------------------------------------------------------------------------------------
///
/// @brief A counter saying how often solver_ctrl_ind_ has already been restarted.
///
/// It is used for our heuristic that defines when to restart this solver.
  size_t reset_c_cnt_;

// -------------------------------------------------------------------------------------------
///
/// @brief An estimate of the number of clauses added to solver_ctrl_ind_ so far.
///
/// It is used for our heuristic that defines when to restart this solver.
  size_t clauses_added_;

// -------------------------------------------------------------------------------------------
///
/// @brief An expander for solver_ctrl_ind_.
  UnivExpander exp_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  ClauseExplorerSAT(const ClauseExplorerSAT &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  ClauseExplorerSAT& operator=(const ClauseExplorerSAT &other);

};

// -------------------------------------------------------------------------------------------
///
/// @class CounterGenSAT
/// @brief Takes counterexamples found by other threads and computes all generalizations.
///
/// A counterexample is a state-input combination with which the antagonist can enforce to
/// leave the winning region. A generalization is a state-cube from which the antagonist can
/// enforce to leave the winning region with this input. This class takes counterexamples
/// found by other threads and computes all minimal generalizations. The negation of a
/// generalization is a clause over the state-variables, which is added to the winning
/// region. The counterexamples to generalize are read out of the ParallelLearner's database.
/// If there are no more counterexamples left in this database (this class removes
/// counterexamples after processing them), then this class is bored. When this class is
/// bored, it takes counterexample generalizations that have been computed earlier and tries
/// to generalize them further by dropping even more literals.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.2.0
class CounterGenSAT
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
///
/// @param coordinator A reference to the coordinator. All communication to other
///        worker-threads is done via the coordinator.
/// @param psi A container for previous-state information (needed when optimization RG is
///        enabled).
  CounterGenSAT(ParallelLearner &coordinator, PrevStateInfo &psi);

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~CounterGenSAT();

// -------------------------------------------------------------------------------------------
///
/// @brief The work-horse of this class.
///
/// It Takes counterexamples found by other threads and computes all generalizations.
/// A counterexample is a state-input combination with which the antagonist can enforce to
/// leave the winning region. A generalization is a state-cube from which the antagonist can
/// enforce to leave the winning region with this input. This class takes counterexamples
/// found by other threads and computes all minimal generalizations. The negation of a
/// generalization is a clause over the state-variables, which is added to the winning
/// region. The counterexamples to generalize are read out of the ParallelLearner's database.
/// If there are no more counterexamples left in this database (this class removes
/// counterexamples after processing them), then this class is bored. When this class is
/// bored, it takes counterexample generalizations that have been computed earlier and tries
/// to generalize them further by dropping even more literals.
/// In every iteration, this method also considers all new incoming clauses found by others.
/// If it finds out that the specification is realizable or unrealizable, it sets the
/// flag ParallelLearner::result_ accordingly. It also polls this flag regularly. If it has
/// been set by some other thread, it quits.
  void generalizeCounterexamples();

// -------------------------------------------------------------------------------------------
///
/// @brief Notifies this worker about a new winning region clause.
///
/// @param clause The new clause that has been discovered.
/// @param src An integer number defining which kind of worker-thread discovered the clause.
///        At the moment, this information is ignored.
  void notifyNewWinRegClause(const vector<int> &clause, int src);


// -------------------------------------------------------------------------------------------
///
/// @brief Returns the statistics and performance measures computed by this object.
///
/// It is used by the ParallelLearner to merge the statistics of the different workers.
///
/// @return The statistics and performance measures computed by this object.
  const LearnStatisticsSAT& getStatistics() const;


protected:

// -------------------------------------------------------------------------------------------
///
/// @brief Generalizes a counterexample.
///
/// This method may call #generalizeCeFuther() if optimization RG is enabled.
///
/// @param state_cube The state-part of the counterexample. This is a state for which the
///        input in in_cube ensures that the winning region is left. It will be overwritten
///        by its own generalization. The result will be a subset of the state-literals
///        such that in_cube still ensures that the winning region is left, for all states
///        in the generalized cube.
/// @param in_cube The input vector which ensures that the winning region is left when
///        starting from state_cube.
/// @return True if in_cube ensures that the winning region is left from all states of
///         state_cube (if this is a counterexample, i.e., if a generalization exists).
///         False otherwise.
  bool generalizeCounterexample(vector<int> &state_cube, const vector<int> &in_cube);

// -------------------------------------------------------------------------------------------
///
/// @brief Generalizes a generalized counterexample further using optimization RG.
///
/// Optimization RG is explained in LearnSynthQBFInd.
/// @param core The state-part of the counterexample. This is a state-cube
///        (representing a set of states) for which the input in in_cube ensures that the
///        winning region is left. It will be overwritten by its own generalization. The
///        result will be a subset of the state-literals such that in_cube still ensures that
///        the winning region is left, for all states in the generalized cube, or the state
///        is unreachable.
/// @param in_cube The input vector which ensures that the winning region is left when
///        starting from state_cube.
  void generalizeCeFuther(vector<int> &core, const vector<int> &in_cube);

// -------------------------------------------------------------------------------------------
///
/// @brief Called if there are no more counterexamples to generalize.
///
/// We want to do something useful with the computation time nevertheless. And what this
/// function does is the following: it takes counterexamples that have been generalized
/// before and tries to generalize them further. This could be possible because the
/// winning region has been refined in the meantime.
  void bored();

// -------------------------------------------------------------------------------------------
///
/// @brief Reduces the counterexample that are considered for further generalization.
///
/// The counterexamples considered for further generalization are stored in #do_if_bored_.
/// Similar to Utils::compressStateCNF() we remove all counterexamples that are implied
/// by others anyway.
  void compressBored();

// -------------------------------------------------------------------------------------------
///
/// @brief Considers new winning region clauses that have been found by other threads.
  void considerNewInfoFromOthers();

// -------------------------------------------------------------------------------------------
///
/// @brief A reference to the coordinator.
///
/// All communication to other worker-threads is done via the coordinator.
  ParallelLearner &coordinator_;

// -------------------------------------------------------------------------------------------
///
/// @brief All new winning region clauses that have not been considered yet.
  CNF new_win_reg_clauses_;

// -------------------------------------------------------------------------------------------
///
/// @brief A lock that protects the #new_win_reg_clauses_ from race-conditions.
///
/// Many worker-threads modify the #new_win_reg_clauses_ (add new ones). This lock ensures
/// that only one thread is modifying or reading the #new_win_reg_clauses_ at one time.
  mutex new_win_reg_clauses_lock_;

// -------------------------------------------------------------------------------------------
///
/// @brief  A set of variables the solver should not optimize by the SAT-solver.
///
/// These are all variables except for temporary ones (type VarInfo::TMP).
  vector<int> vars_to_keep_;

// -------------------------------------------------------------------------------------------
///
/// @brief The solver for checking counterexample-candidates.
///
/// It is also used to generalize counterexamples if optimization RG is disabled.
  SatSolver *solver_ctrl_;

// -------------------------------------------------------------------------------------------
///
/// @brief An incremental SatSolver containing the winning region.
///
/// It is mainly used to check if new clauses found by this class are already implied by
/// other clauses in the winning region. If so, then the new clauses are not added.
  SatSolver *solver_win_;

// -------------------------------------------------------------------------------------------
///
/// @brief The solver for generalizing counterexamples if optimization RG is enabled.
  SatSolver *solver_ctrl_ind_;

// -------------------------------------------------------------------------------------------
///
/// @brief Stores and maintains statistics and performance measures.
///
/// Basically, this is a merge of the statistics from all worker-threads.
  LearnStatisticsSAT statistics_;

// -------------------------------------------------------------------------------------------
///
/// @brief The counterexamples to generalize further whenever we are bored.
///
/// We are bored whenever the counterexample-database of the ParallelLearner does not contain
/// any new counterexamples. All counterexample generalizations found by this class are
/// written into this field. If we are bored, we try to generalize one of these
/// counterexamples further. This is to utilize our computation time.
///
/// The first item of the pairs stored in this vectors is always the state-cube of the
/// counterexample. The second item of the pair is the corresponding input-cube witnessing
/// the fact that the successor state will be outside of the winning region.
  vector<pair<vector<int>, vector<int> > > do_if_bored_;

// -------------------------------------------------------------------------------------------
///
/// @brief The index (in #do_if_bored_) of the next item to process if bored.
  size_t next_bored_index_;

// -------------------------------------------------------------------------------------------
///
/// @brief The size of do_if_bored_ when we compressed it the last time.
///
/// This is just tracked for a heuristic that decides when to call #compressBored() the next
/// time.
  size_t last_bored_compress_size_;

// -------------------------------------------------------------------------------------------
///
/// @brief Information about the previous states if optimization RG is enabled.
  const PrevStateInfo &psi_;

// -------------------------------------------------------------------------------------------
///
/// @brief The vector of current-state variables.
  vector<int> s_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  CounterGenSAT(const CounterGenSAT &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  CounterGenSAT& operator=(const CounterGenSAT &other);

};


// -------------------------------------------------------------------------------------------
///
/// @class ClauseMinimizerQBF
/// @brief Uses a QBF solver to minimize existing winning region clauses further.
///
/// This class is very simple: it just takes clauses from the winning region and tries to
/// minimize them further using a QBF solver. The minimization is done in the very same way
/// in LearnSynthQBF.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.2.0
class ClauseMinimizerQBF
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
///
/// @param instance_nr A unique instance number. It is used to bring in some asymmetry between
///        the different ClauseMinimizerQBF-instances: depending on the instance number, the
///        instance selects a different Solver to use.
/// @param coordinator A reference to the coordinator. All communication to other
///        worker-threads is done via the coordinator.
/// @param psi A container for previous-state information (needed when optimization RG is
///        enabled).
  ClauseMinimizerQBF(size_t instance_nr,
                     ParallelLearner &coordinator,
                     PrevStateInfo &psi);

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~ClauseMinimizerQBF();

// -------------------------------------------------------------------------------------------
///
/// @brief The work-horse of this class.
///
/// It takes clauses from the winning region and tries to minimize them further using a
/// QBF-solver. The minimization is done in the very same way in LearnSynthQBF.
/// In every iteration, this method also considers all new incoming clauses found by others.
/// If it finds out that the specification is realizable or unrealizable, it sets the
/// flag ParallelLearner::result_ accordingly. It also polls this flag regularly. If it has
/// been set by some other thread, it quits.
  void minimizeClauses();

// -------------------------------------------------------------------------------------------
///
/// @brief Notifies this worker about a new winning region clause.
///
/// @param clause The new clause that has been discovered.
/// @param src An integer number defining which kind of worker-thread discovered the clause.
///        At the moment, this information is ignored.
  void notifyNewWinRegClause(const vector<int> &clause, int src);

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief Implements #minimizeClauses() with incremental QBF solving.
  void minimizeClausesInc();

// -------------------------------------------------------------------------------------------
///
/// @brief Implements #minimizeClauses() with non-incremental QBF solving.
  void minimizeClausesNoInc();

// -------------------------------------------------------------------------------------------
///
/// @brief A reference to the coordinator.
///
/// All communication to other worker-threads is done via the coordinator.
  ParallelLearner &coordinator_;

// -------------------------------------------------------------------------------------------
///
/// @brief The quantifier prefix of the QBF for generalizing clauses.
///
/// This quantifier prefix is always
///   exists x: forall i: exists c,x',tmp:
  vector<pair<vector<int>, QBFSolver::Quant> > gen_quant_;

// -------------------------------------------------------------------------------------------
///
/// @brief The QBF-solver used for the generalization queries.
///
/// The type of solver is selected with command-line arguments.
  QBFSolver *qbf_solver_;

// -------------------------------------------------------------------------------------------
///
/// @brief An interface to DepQBF for incremental QBF solving.
  DepQBFApi *inc_qbf_solver_;

// -------------------------------------------------------------------------------------------
///
/// @brief A SatSolver to check if minimized clauses are already implied by others.
  SatSolver *sat_solver_;

// -------------------------------------------------------------------------------------------
///
/// @brief All new winning region clauses that have not been considered yet.
  CNF new_win_reg_clauses_;

// -------------------------------------------------------------------------------------------
///
/// @brief A lock that protects the #new_win_reg_clauses_ from race-conditions.
///
/// Many worker-threads modify the #new_win_reg_clauses_ (add new ones). This lock ensures
/// that only one thread is modifying or reading the #new_win_reg_clauses_ at one time.
  mutex new_win_reg_clauses_lock_;

// -------------------------------------------------------------------------------------------
///
/// @brief Information about the previous states if optimization RG is enabled.
  const PrevStateInfo &psi_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  ClauseMinimizerQBF(const ClauseMinimizerQBF &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  ClauseMinimizerQBF& operator=(const ClauseMinimizerQBF &other);
};


// -------------------------------------------------------------------------------------------
///
/// @class IFM13Explorer
/// @brief Implements the method of IFM13Synth in a parallelized way.
///
/// This class basically implements the same method as IFM13Synth. It only has a few
/// additional methods to communicate with other threads. Whenever it finds a new clause of
/// the winning region (of the protagonist not the antagonist), it communicates this clause
/// to all other worker-threads (via the ParallelLearner, which acts as a coordinator). On
/// the other hand, it also considers refinements of the winning region done by other threads.
///
/// @see IFM13Synth
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.2.0
class IFM13Explorer
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
///
/// @param coordinator A reference to the coordinator. All communication to other
///        worker-threads is done via the coordinator.
  IFM13Explorer(ParallelLearner &coordinator);

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~IFM13Explorer();

// -------------------------------------------------------------------------------------------
///
/// @brief The work-horse of this class.
///
/// It basically implements the same method as IFM13Synth. However, it also communicates with
/// other threads: Whenever it finds a new clause of the winning region (of the protagonist
/// not the antagonist), it communicates this clause to all other worker-threads (via the
/// ParallelLearner, which acts as a coordinator). On the other hand, it also considers
/// refinements of the winning region done by other threads.
/// If it finds out that the specification is realizable or unrealizable, it sets the
/// flag ParallelLearner::result_ accordingly. It also polls this flag regularly. If it has
/// been set by some other thread, it quits.
  void exploreClauses();

// -------------------------------------------------------------------------------------------
///
/// @brief Notifies this worker about a new winning region clause.
///
/// @param clause The new clause that has been discovered.
/// @param src An integer number defining which kind of worker-thread discovered the clause.
///        At the moment, this information is ignored.
  void notifyNewWinRegClause(const vector<int> &clause, int src);

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief Considers new winning region clauses that have been found by other threads.
  void considerNewInfoFromOthers();

// -------------------------------------------------------------------------------------------
///
/// @brief Propagates clauses forward and searches for equivalent clause sets.
///
/// Similar to IC3, this method propagates clauses of frames forward. It also checks if two
/// adjacent clauses sets become syntactically equal.
///
/// @param max_level The maximum frame level until which propagation should be performed.
/// @return The number N returned by this method has the following meaning: N=0 means that
///         no equal clause sets have been found. N > 0 means that R[N] = R[N-1].
  size_t propagateBlockedStates(size_t max_level);

// -------------------------------------------------------------------------------------------
///
/// @brief Analyzes the rank of the passed state.
///
/// There are two possible conclusions: (a) the rank of the state is > level, (b) the rank of
/// the state is <= level. In order to analyze a certain state, this method may analyze
/// some of its successor states first (using a queue of proof obligations internally). This
/// method is always called for the initial state only.
///
/// @param state_cube A full cube over the state variables, describing the state that should
///        be analyzed.
/// @param level The level for the analysis (this method checks if the rank of the state is
///        > level or <= level).
/// @return IS_GREATER (an alias for true) if the rank of the state is > level. IS_LOSE (an
///         alias for false) otherwise.
  bool recBlockCube(const vector<int> &state_cube, size_t level);

// -------------------------------------------------------------------------------------------
///
/// @brief Marks a state-input-pair as blocked.
///
/// The meaning is: we want to store the fact that a certain state-input pair is not
/// helpful for the antagonist (the party controlling the inputs i) in trying to move from
/// R[level] to R[level-1]. Hence, we update U[level] := U[level] & !state_in_cube. We also
/// update the U-sets of all smaller levels in the same way for syntactic containment
/// reasons (as described in the paper).
///
/// @param state_in_cube The state-input pair as a cube over present state variables and
///        uncontrollable inputs. The cube is potentially incomplete, i.e., can represent
///        a larger set of state-input pairs.
/// @param level The level in which the transition should be blocked (i.e., the state-input
///        pair should by marked as useless for the antagonist).
  void addBlockedTransition(const vector<int> &state_in_cube, size_t level);

// -------------------------------------------------------------------------------------------
///
/// @brief Removes a state(-cube) from the frame R[level].
///
/// This method updates R[level] := R[level] & !state_cube. state_cube is a potentially
/// incomplete state-cube. We also update the R-sets of all smaller levels in the same way
/// for syntactic containment reasons (as described in the paper).
///
/// @param state_cube The state-cube that should be removed from R[level]. The cube is
///        potentially incomplete, i.e., can represent a larger set of states.
/// @param level The level in which the state should be blocked (i.e., removed form the
///        corresponding frame R[level]).
  void addBlockedState(const vector<int> &state_cube, size_t level);

// -------------------------------------------------------------------------------------------
///
/// @brief Checks if a state is contained in a certain frame.
///
/// The passed state cube is complete, i.e., contains all state variables. Hence, containment
/// can be (and is) checked syntactically. We simply check if state_cube satisfies all clauses
/// of R[level].
///
/// @param state_cube A full cube over the present state variables.
/// @param level The index i of a certain frame R[i].
/// @return False if state_cube satisfies R[level], true otherwise.
  bool isBlocked(const vector<int> &state_cube, size_t level);

// -------------------------------------------------------------------------------------------
///
/// @brief Removes a state(-cube) from the current over-approximation of the winning region.
///
/// This method updates W := W & !state_cube. state_cube is a potentially incomplete
/// state-cube.
///
/// @param state_cube The state-cube that should be removed from the current
///        over-approximation of the winning region. The cube is potentially incomplete, i.e.,
//         can represent a larger set of states.
  void addLose(const vector<int> &state_cube);

// -------------------------------------------------------------------------------------------
///
/// @brief Checks if a state satisfies the current over-approximation of the winning region.
///
/// The passed state cube is complete, i.e., contains all state variables. Hence, containment
/// can be (and is) checked syntactically. We simply check if state_cube satisfies all clauses
/// of W (stored in @link #win_ win_ @endlink).
///
/// @param state_cube A full cube over the present state variables.
/// @return False if state_cube satisfies W, true otherwise.
  bool isLose(const vector<int> &state_cube);

// -------------------------------------------------------------------------------------------
///
/// @brief Generalizes and excludes a blocked transition.
///
/// @param state_in_cube The state-input cube (the transition) that should be generalized and
///        and blocked.
/// @param ctrl_cube The control signals that can be chosen by the protagonist in order to
///        avoid ending up in R[level-1].
/// @param level The frame in which the transition should be blocked.
/// @return False if state_cube satisfies W, true otherwise.
  void genAndBlockTrans(const vector<int> &state_in_cube,
                        const vector<int> &ctrl_cube,
                        size_t level);

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the frame R[index] in CNF.
///
/// If the frame with this index does not yet exists, it is created.
///
/// @param index The index of the requested frame.
/// @return The frame R[index] in CNF.
  CNF& getR(size_t index);

// -------------------------------------------------------------------------------------------
///
/// @brief Returns the blocked transitions U[index] in CNF.
///
/// If the set of blocked transitions with this index does not yet exists, it is created.
/// This function is actually only needed for debugging purposes.
///
/// @param index The index of the requested set of blocked transitions.
/// @return The set U[index] of blocked transitions in CNF.
  CNF& getU(size_t index);

// -------------------------------------------------------------------------------------------
///
/// @brief Returns a solver with CNF: U[index] & T & R[index-1]'.
///
/// This solver is used to search for moves the antagonist can make in order to reach
/// R[index-1]. If the solver for this index does not yet exists, it is created.
///
/// @param index The index of the requested solver.
/// @return A solver with CNF: U[index] & T & R[index-1]'. getGotoNextLowerSolver(0) returns
///         NULL.
  SatSolver* getGotoNextLowerSolver(size_t index);

// -------------------------------------------------------------------------------------------
///
/// @brief Returns a solver with CNF: T & R[index-1]'.
///
/// This solver is used to generalize blocked transitions. If the solver for this index does
/// not yet exists, it is created.
///
/// @param index The index of the requested solver.
/// @return A solver with CNF: T & R[index-1]'. getGenBlockTransSolver(0) returns NULL.
  SatSolver* getGenBlockTransSolver(size_t index);

// -------------------------------------------------------------------------------------------
///
/// @brief Takes a list of proof obligations and removes the element with lowest rank.
///
/// @param queue A list of proof obligations.
/// @return The element that has been removed from the list. It is always the element
///         (among the elements) with minimal rank.
  IFMProofObligation popMin(list<IFMProofObligation> &queue);

// -------------------------------------------------------------------------------------------
///
/// @brief A reference to the coordinator.
///
/// All communication to other worker-threads is done via the coordinator.
  ParallelLearner &coordinator_;

// -------------------------------------------------------------------------------------------
///
/// @brief All new winning region clauses that have not been considered yet.
  CNF new_win_reg_clauses_;

// -------------------------------------------------------------------------------------------
///
/// @brief A lock that protects the #new_win_reg_clauses_ from race-conditions.
///
/// Many worker-threads modify the #new_win_reg_clauses_ (add new ones). This lock ensures
/// that only one thread is modifying or reading the #new_win_reg_clauses_ at one time.
  mutex new_win_reg_clauses_lock_;

// -------------------------------------------------------------------------------------------
///
/// @brief The frames R[] of the algorithm. Each frame represents a set of states in CNF.
///
/// You should use @link #getR getR() @endlink to access the frames. The reason is that this
/// method initializes frames if they do not yet exist.
  vector<CNF> r_;

// -------------------------------------------------------------------------------------------
///
/// @brief The blocked transitions U[] of the algorithm.
///
/// These sets are maintained only in debug mode. In release-mode they are only present
/// inside the goto_next_lower_solvers_.
/// You should use @link #getU getU() @endlink to access the frames. The reason is that this
/// method initializes elements if they do not yet exist.
  vector<CNF> u_;

// -------------------------------------------------------------------------------------------
///
/// @brief The current over-approximation of the winning region W (for the protagonist).
  CNF win_;

// -------------------------------------------------------------------------------------------
///
/// @brief The solvers storing the CNF: U[k] & T & R[k-1]'.
///
/// These solvers are used to search for moves the antagonist can make in order to reach
/// R[k-1].
/// You should use @link #getGotoNextLowerSolver getGotoNextLowerSolver() @endlink to access
/// these solvers. The reason is that this method initializes the solvers if they do not yet
/// exist. goto_next_lower_solvers_[0] is always NULL.
  vector<SatSolver*> goto_next_lower_solvers_;

// -------------------------------------------------------------------------------------------
///
/// @brief The solvers storing the CNF: T & R[k-1]'.
///
/// You should use @link #getGenBlockTransSolver getGenBlockTransSolver() @endlink to access
/// these solvers. The reason is that this method initializes the solvers if they do not yet
/// exist. gen_block_trans_solvers_[0] is always NULL.
  vector<SatSolver*> gen_block_trans_solvers_;

// -------------------------------------------------------------------------------------------
///
/// @brief A solver that stores the CNF: T & W'.
  SatSolver *goto_win_solver_; // contains T & W'

// -------------------------------------------------------------------------------------------
///
/// @brief The vector of present- and next-state variables, and uncontrollable inputs.
  vector<int> sin_;

// -------------------------------------------------------------------------------------------
///
/// @brief The vector of present- and next-state variables, and all inputs.
  vector<int> sicn_;

// -------------------------------------------------------------------------------------------
///
/// @brief The cube encoding the initial state.
  vector<int> initial_state_cube_;

// -------------------------------------------------------------------------------------------
///
/// @brief The literal saying that we are in an unsafe state.
///
/// We cache it so that we do not have to get it from the VarManager every time. We may of
/// lock the VarManager otherwise.
  int error_var_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  IFM13Explorer(const IFM13Explorer &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  IFM13Explorer& operator=(const IFM13Explorer &other);

};

// -------------------------------------------------------------------------------------------
///
/// @class TemplExplorer
/// @brief Implements the method of TemplateSynth in a parallelized way.
///
/// This class basically implements the same method as TemplateSynth. It only has a few
/// additional methods to communicate with other threads.  It considers all clauses of the
/// winning region found by other threads already. However, it does not produce any
/// information for the other threads. It is an information sink.
///
/// @see TemplateSynth
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.2.0
class TemplExplorer
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
///
/// @param coordinator A reference to the coordinator. All communication to other
///        worker-threads is done via the coordinator.
  TemplExplorer(ParallelLearner &coordinator);

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~TemplExplorer();

// -------------------------------------------------------------------------------------------
///
/// @brief Computes the winning region as instantiation of a generic template.
  void computeWinningRegion();

// -------------------------------------------------------------------------------------------
///
/// @brief Notifies this worker about a new winning region clause.
///
/// @param clause The new clause that has been discovered.
/// @param src An integer number defining which kind of worker-thread discovered the clause.
///        At the moment, this information is ignored.
  void notifyNewWinRegClause(const vector<int> &clause, int src);

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief Computes the winning region as instantiation of a generic CNF template.
///
/// @param nr_of_clauses The number N of clauses to use in the template.
/// @param timeout A time-out is seconds.
/// @param use_sat True if a SAT solver shall be used, false if a QBF solver shall be used.
/// @return The value 0 if no winning region exists, the value 1 if a winning region was
///         successfully computed, the value 2 if we had a time-out.
  int findWinRegCNFTempl(size_t nr_of_clauses, size_t timeout, bool use_sat);

// -------------------------------------------------------------------------------------------
///
/// @brief Resolves the template by calling a QBF solver.
///
/// @param win_constr CNF constraints that constitute a correct solution for the winning
///        region using the generic template. Essentially, three constraints are encoded in
///        this CNF:
///        <ol>
///          <li> I(x) => W(x): every initial state is contained in the winning region
///          <li> W(x) => P(x): every state of the winning region is safe
///          <li> forall x,i: exists c,x': W(x) => (T(x,i,c,x') & W(x')): from every state in
///               the winning region we can enforce to stay in the winning region by setting
///               the c-signals appropriately.
///        </ol>
/// @param w1 The literal that represents the present-state copy of the winning region in
///        win_constr.
/// @param w2 The literal that represents the next-state copy of the winning region in
///        win_constr.
/// @param solution An empty vector. If a solution exists, then the corresponding template
///        parameter values are written into this vector.
/// @param timeout A time-out is seconds.
/// @return The value 0 if no winning region exists, the value 1 if a winning region was
///         successfully computed, the value 2 if we had a time-out.
  int syntQBF(const CNF& win_constr, int w1, int w2, vector<int> &solution, size_t timeout);

// -------------------------------------------------------------------------------------------
///
/// @brief Resolves the template by calling a SAT solver in a CEGIS loop.
///
/// @param win_constr CNF constraints that constitute a correct solution for the winning
///        region using the generic template. Essentially, three constraints are encoded in
///        this CNF:
///        <ol>
///          <li> I(x) => W(x): every initial state is contained in the winning region
///          <li> W(x) => P(x): every state of the winning region is safe
///          <li> forall x,i: exists c,x': W(x) => (T(x,i,c,x') & W(x')): from every state in
///               the winning region we can enforce to stay in the winning region by setting
///               the c-signals appropriately.
///        </ol>
/// @param w1 The literal that represents the present-state copy of the winning region in
///        win_constr.
/// @param w2 The literal that represents the next-state copy of the winning region in
///        win_constr.
/// @param solution An empty vector. If a solution exists, then the corresponding template
///        parameter values are written into this vector.
/// @param timeout A time-out is seconds.
/// @return The value 0 if no winning region exists, the value 1 if a winning region was
///         successfully computed, the value 2 if we had a time-out.
  int syntSAT(const CNF& win_constr, int w1, int w2, vector<int> &solution, size_t timeout);

// -------------------------------------------------------------------------------------------
///
/// @brief A helper for the CEGIS loop of #syntSAT(), eliminating a counterexample.
///
/// @param ce A counterexample that has been computed previously by calling #check(). A
///        counterexample is simply a set of values for state variables and input variables
///        for which we fall out of the winning region.
/// @param gen The CNF containing the constraints for a correct winning region. We simply
///        add constraints here saying that the winning region must now also work for the
///        counterexample.
/// @param solver The solver to which the constraints eliminating the counterexample shall
///        be added.
  void exclude(const vector<int> &ce, const CNF &gen, SatSolver* solver);

// -------------------------------------------------------------------------------------------
///
/// @brief A helper for the CEGIS loop of #syntSAT(), checking if a candidate solution works.
///
/// @param cand A canidate solution in the form of concrete values for the template
///        parameters.
/// @param win_constr CNF constraints that constitute a correct solution for the winning
///        region using the generic template. Essentially, three constraints are encoded in
///        this CNF:
///        <ol>
///          <li> I(x) => W(x): every initial state is contained in the winning region
///          <li> W(x) => P(x): every state of the winning region is safe
///          <li> forall x,i: exists c,x': W(x) => (T(x,i,c,x') & W(x')): from every state in
///               the winning region we can enforce to stay in the winning region by setting
///               the c-signals appropriately.
///        </ol>
/// @param w1 The literal that represents the present-state copy of the winning region in
///        win_constr.
/// @param w2 The literal that represents the next-state copy of the winning region in
///        win_constr.
/// @param timeout A time-out is seconds.
/// @param ce An empty vector. If the candidate solution is incorrect, a counterexample in
///        the form of concrete values for state and input variables for which we fall out
///        of the winning region is stored in this vector.
/// @return The value 1 the candidate solution is correct, the value 0 if the candicate
///         solution is incorrect, 2 if a time-out was reached.
  int check(const vector<int> &cand, const CNF& win_constr, int w1, int w2, size_t timeout,
            vector<int> &ce);

// -------------------------------------------------------------------------------------------
///
/// @brief Returns a fresh temporary variable.
///
/// @return A fresh temporary variable.
  int newTmp();

// -------------------------------------------------------------------------------------------
///
/// @brief Returns a fresh template parameter.
///
/// @return A fresh template parameter.
  int newParam();

// -------------------------------------------------------------------------------------------
///
/// @brief A helper that transforms all current state variables into next state variables.
///
/// @param vec A vector of state variables or their negation.
  void swapPresentToNext(vector<int> &vec) const;

// -------------------------------------------------------------------------------------------
///
/// @brief A reference to the coordinator.
///
/// All communication to other worker-threads is done via the coordinator.
  ParallelLearner &coordinator_;

// -------------------------------------------------------------------------------------------
///
/// @brief The resulting winning region.
  CNF final_winning_region_;

// -------------------------------------------------------------------------------------------
///
/// @brief A lock that protects the #known_clauses_ from race-conditions.
///
/// Many worker-threads modify the #known_clauses_ (add new ones). This lock ensures
/// that only one thread is modifying or reading the #known_clauses_ at one time.
  mutex known_clauses_lock_;

// -------------------------------------------------------------------------------------------
///
/// @brief The resulting winning region.
  CNF known_clauses_;

// -------------------------------------------------------------------------------------------
///
/// @brief The list of present-state variables.
///
/// This list is often used, so we keep it as a field to increase readability of the code.
  vector<int> s_;

// -------------------------------------------------------------------------------------------
///
/// @brief The list of (uncontrollable) input variables.
///
/// This list is often used, so we keep it as a field to increase readability of the code.
  vector<int> i_;

// -------------------------------------------------------------------------------------------
///
/// @brief The list of control signals (controllable input variables).
///
/// This list is often used, so we keep it as a field to increase readability of the code.
  vector<int> c_;

// -------------------------------------------------------------------------------------------
///
/// @brief The list of next-state variables.
///
/// This list is often used, so we keep it as a field to increase readability of the code.
  vector<int> n_;

// -------------------------------------------------------------------------------------------
///
/// @brief The list of template parameters.
///
/// We do not want to access the global VarManager (to avoid too much locking), so we keep
/// everything local.
  vector<int> templ_;

// -------------------------------------------------------------------------------------------
///
/// @brief The original list of temporary variables.
///
/// We do not want to access the global VarManager (to avoid too much locking), so we keep
/// everything local.
  vector<int> orig_tmp_;

// -------------------------------------------------------------------------------------------
///
/// @brief The current list of temporary variables.
///
/// We do not want to access the global VarManager (to avoid too much locking), so we keep
/// everything local.
  vector<int> current_tmp_;

// -------------------------------------------------------------------------------------------
///
/// @brief The initial next free CNF variable.
///
/// We do not want to access the global VarManager (to avoid too much locking), so we keep
/// everything local.
  int orig_next_free_var_;

// -------------------------------------------------------------------------------------------
///
/// @brief The current next free CNF variable.
///
/// We do not want to access the global VarManager (to avoid too much locking), so we keep
/// everything local.
  int next_free_var_;

// -------------------------------------------------------------------------------------------
///
/// @brief The present-state error variable.
///
/// It is stored here locally so that we do not always have to access the global variable
/// manager (which may cause a concurrency issue).
  int p_err_;

// -------------------------------------------------------------------------------------------
///
/// @brief The next-state error variable.
///
/// It is stored here locally so that we do not always have to access the global variable
/// manager (which may cause a concurrency issue).
  int n_err_;

// -------------------------------------------------------------------------------------------
///
/// @brief The point in time where the SAT-based method has been started.
///
/// This is used for a heuristic that defines when to abort the SAT-based exploration.
  PointInTime start_;


// -------------------------------------------------------------------------------------------
///
/// @brief A map from present-state variables to corresponding next state variables.
  vector<int> pres_to_nxt_;


};

#endif // ParallelLearner_H__
