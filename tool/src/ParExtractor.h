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
/// @file ParExtractor.h
/// @brief Contains the declaration of the class ParExtractor.
// -------------------------------------------------------------------------------------------

#ifndef ParExtractor_H__
#define ParExtractor_H__

#include "defines.h"
#include "CNF.h"
#include "QBFSolver.h"
#include "CNFImplExtractor.h"
#include "LearningExtractorStatistics.h"

class SatSolver;
class ParExtractorWorker;
class DepQBFApi;

// -------------------------------------------------------------------------------------------
///
/// @class ParExtractor
/// @brief A parallelized version of the LearningImplExtractor.
///
/// This class implements a parallelized version of the LearningImplExtractor. Take a look
/// at the comments in the LearningImplExtractor to see what it does. The parallelization
/// is very simple at this point. It is merely a portfolio parallelization where different
/// methods run in different threads. Closer interaction is left for future work.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.2.0
class ParExtractor : public CNFImplExtractor
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
  ParExtractor(size_t nr_of_threads);

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~ParExtractor();

// -------------------------------------------------------------------------------------------
///
/// @brief Runs this circuit extractor.
///
/// @param winning_region The winning region from which the circuit should be extracted.
/// @param neg_winning_region The negation of the winning region.
  virtual void run(const CNF &winning_region, const CNF &neg_winning_region);


// -------------------------------------------------------------------------------------------
///
/// @brief Returns true if all worker threads are done and waiting for others to finish.
///
/// @return True if all worker threads are done and waiting for others to finish.
  bool allWaiting();

// -------------------------------------------------------------------------------------------
///
/// @brief Gives all worker threads the command to terminate.
  void killThemAll();

// -------------------------------------------------------------------------------------------
///
/// @brief An integer number encoding the current command to the circuit extractor workers.
///
/// There are two possible value.
/// <ul>
///  <li> EXTR_GO = 0: means that the circuit extractor threads should continue.
///  <li> EXTR_STOP = 1: means that the circuit extractor threads should stop.
/// </ul>
/// All worker-threads poll this flag from time to time. If the value is still EXTR_GO they
/// continue to work. If they see that the values is EXTR_STOP they stop, assuming
/// that some other thread has already found the solution. If one thread finds a solution,
/// it sets this flag to EXTR_STOP.
  volatile int extr_command_;

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief Logs the detailed statistics collected by the fastest worker.
  void logDetailedStatistics();

// -------------------------------------------------------------------------------------------
///
/// @brief The number of threads to instantiate and execute.
  size_t nr_of_threads_;

// -------------------------------------------------------------------------------------------
///
/// @brief The final number of new AND gates (just for the statistics).
  size_t nr_of_new_ands_;

// -------------------------------------------------------------------------------------------
///
/// @brief The worker threads.
  vector<ParExtractorWorker*> extractors_;

// -------------------------------------------------------------------------------------------
///
/// @brief A set of pthread objects we may have to kill if they do not terminate.
  vector<pthread_t> extractor_pthreads_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  ParExtractor(const ParExtractor &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  ParExtractor& operator=(const ParExtractor &other);

};


// -------------------------------------------------------------------------------------------
///
/// @class ParExtractorWorker
/// @brief A worker thread for the ParExtractor.
///
/// This class implements selected circuit extraction methods from the
/// LearningImplExtractor in a parallelized way.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.2.0
class ParExtractorWorker
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
///
/// @param coordinator A reference to the coordinator. All communication to other
///        worker-threads is done via the coordinator.
/// @param win_region The winning region from which the circuit should be extracted.
/// @param neg_win_region The negation of the winning region.
  ParExtractorWorker(ParExtractor &coordinator,
                     const CNF &win_region,
                     const CNF &neg_win_region);

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~ParExtractorWorker();

// -------------------------------------------------------------------------------------------
///
/// @brief Runs SAT-based learning without dependency optimization.
///
/// Roughly implements LearningImplExtractor::runLearningJiangSATInc1().
  void runSAT();

// -------------------------------------------------------------------------------------------
///
/// @brief Runs SAT-based learning with dependency optimization.
///
/// Roughly implements LearningImplExtractor::runLearningJiangSATTmpCtrl().
  void runSATDep();

// -------------------------------------------------------------------------------------------
///
/// @brief Logs the detailed statistics collected by the fastest worker.
  void logStats();

// -------------------------------------------------------------------------------------------
///
/// @brief Indicates if standalone_circuit_ is ready.
  bool done_;

// -------------------------------------------------------------------------------------------
///
/// @brief Indicates if this thread is waiting for others to finish.
  bool waiting_;

// -------------------------------------------------------------------------------------------
///
/// @brief True if this thread did a second minimization run (just for printing statistics).
  bool did_second_run_;


// -------------------------------------------------------------------------------------------
///
/// @brief The synthesis result as stand-alone circuit.
///
/// This AIGER circuit is combinatorial, i.e., has no latches. The inputs of the circuit are:
/// the uncontrollable inputs from the spec (in the order in which they appear in the spec)
/// followed by the state-signals (the output of the latches in the spec, in the order in
/// which they appear in the spec). The outputs of the circuit are the controllable inputs
/// in the order in which they are defined in the spec.
///
/// This circuit will be built up by one of the methods runLearningX. Then, it will be
/// optimized using ABC. Finally, it will be inserted into the original specification, as
/// required by the rules for SYNTComp.
  aiger *standalone_circuit_;

// -------------------------------------------------------------------------------------------
///
/// @brief A helper for collecting performance data and statistics.
  LearningExtractorStatistics statistics;

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief Waits for other worker threads to finish.
///
/// If a worker thread is finished it does not terminate and kill other worker threads
/// immediately. Instead, it waits for some time hoping that other worker threads will finish
/// as well. This way, we can choose among the different circuits created by the different
/// threads. However, if some thread loses her patience while waiting, it can kill other
/// worker threads.
///
/// @param elapsed_until_done The time in seconds this worker thread needed until it found a
///        solution. This number is used for heuristics that decide when to kill the other
///        workers.
  void waitForOthersToFinish(size_t elapsed_until_done);

// -------------------------------------------------------------------------------------------
///
/// @brief Creates a CNF that assigns a variable to the result of some other CNF.
///
/// This method creates a CNF that for (var <-> impl), where var is a variable and impl
/// is a CNF. It is used to say that a certain control signal is now defined by a function
/// that has been computed in CNF form.
///
/// @param var The variable to set equal to the result of the CNF impl.
/// @param impl The CNF that should define the value of var.
/// @param new_temps A vector of variables. New auxiliary variables introduced by this
///        method will be appended to this vector. The vector can also be omitted if not
///        needed.
/// @return A CNF representation of (var <-> impl).
  CNF makeEq(int var, CNF impl, vector<int> *new_temps = NULL);

// -------------------------------------------------------------------------------------------
///
/// @brief Adds the solution for one output function to the AIGER circuit we have so far.
///
/// The AIGER circuit we have so far is built up in #standalone_circuit_. This method adds
/// an output function (in CNF) for one control signal.
///
/// @param ctrl_var The control signal for which we would like to add an output function.
/// @param solution The output function (in CNF) defining the control signal.
  void addToStandAloneAiger(int ctrl_var, const CNF &solution);

// -------------------------------------------------------------------------------------------
///
/// @brief Same as #addToStandAloneAiger but operating on the copy #standalone_circuit2_.
///
/// @param ctrl_var The control signal for which we would like to add an output function.
/// @param solution The output function (in CNF) defining the control signal.
  void addToStandAloneAiger2(int ctrl_var, const CNF &solution);


// -------------------------------------------------------------------------------------------
///
/// @brief Transforms a CNF literal into an AIGER literal as used in #standalone_circuit_.
///
/// @param cnf_lit The literal as it occurs in CNF formulas.
/// @return The corresponding AIGER index of the literal as it is used in the
///         #standalone_circuit_.
  int cnfToAig(int cnf_lit);

// -------------------------------------------------------------------------------------------
///
/// @brief Creates the AND between two AIGER literals and returns the resulting literal.
///
/// This method extends the #standalone_circuit_ with logic to compute the AND between two
/// AIGER literals. The literal that represents the result of the AND is returned.
///
/// @param in1 The first operand of the AND: an AIGER literal from #standalone_circuit_.
/// @param in2 The second operand of the AND: an AIGER literal from #standalone_circuit_.
/// @return The literal that represents the result of the AND.
  int makeAnd(int in1, int in2);

// -------------------------------------------------------------------------------------------
///
/// @brief Same as #makeAnd but operating on the copy #standalone_circuit2_.
///
/// @param in1 The first operand of the AND: an AIGER literal from #standalone_circuit2_.
/// @param in2 The second operand of the AND: an AIGER literal from #standalone_circuit2_.
/// @return The literal that represents the result of the AND.
  int makeAnd2(int in1, int in2);

// -------------------------------------------------------------------------------------------
///
/// @brief Creates the OR between two AIGER literals and returns the resulting literal.
///
/// This method extends the #standalone_circuit_ with logic to compute the OR between two
/// AIGER literals. The literal that represents the result of the OR is returned.
///
/// @param in1 The first operand of the OR: an AIGER literal from #standalone_circuit_.
/// @param in2 The second operand of the OR: an AIGER literal from #standalone_circuit_.
/// @return The literal that represents the result of the OR.
  int makeOr(int in1, int in2);

// -------------------------------------------------------------------------------------------
///
/// @brief Same as #makeAnd but operating on the copy #standalone_circuit2_.
///
/// @param in1 The first operand of the OR: an AIGER literal from #standalone_circuit2_.
/// @param in2 The second operand of the OR: an AIGER literal from #standalone_circuit2_.
/// @return The literal that represents the result of the OR.
  int makeOr2(int in1, int in2);

// -------------------------------------------------------------------------------------------
///
/// @brief Copies missing AND gates from the spec.
///
/// This method is only used for an optimization in the method #runSATDep. This method allows
/// solutions to depend on temporary variables that appear in the original specification.
/// The rules for SYNTComp do not allow to refer to such temporary variables from the
/// specification in the final solution. Hence, this method simply copies the AND gates from
/// the specification into the standalone_circuit passed as argument.
///
/// @param standalone_circuit The circuit to complete with missing AND gates.
  void insertMissingAndFromTrans(aiger *standalone_circuit);

// -------------------------------------------------------------------------------------------
///
/// @brief A reference to the coordinator.
///
/// All communication to other worker-threads is done via the coordinator.
  ParExtractor &coordinator_;

// -------------------------------------------------------------------------------------------
///
/// @brief A SAT solver to check for counterexamples.
  SatSolver *check_;

// -------------------------------------------------------------------------------------------
///
/// @brief A SAT solver to generalize counterexamples.
  SatSolver *gen_;

// -------------------------------------------------------------------------------------------
///
/// @brief The winning region from which the circuit should be extracted.
  const CNF &win_region_;

// -------------------------------------------------------------------------------------------
///
/// @brief The negation of the winning region.
  const CNF &neg_win_region_;

// -------------------------------------------------------------------------------------------
///
/// @brief The next unused AIGER literal in #standalone_circuit_.
  int next_free_aig_lit_;

// -------------------------------------------------------------------------------------------
///
/// @brief We need to create fresh variables locally.
  int next_free_cnf_lit_;

// -------------------------------------------------------------------------------------------
///
/// @brief A map from variables as they occur in CNFs to the corresponding AIGER literals.
///
/// This vector maps variables as they occur in CNFs to the corresponding AIGER literals as
/// they occur in the #standalone_circuit_. Use #cnfToAig to transform literals.
///
/// @note The AIGER literal numbering scheme in the #standalone_circuit_ is different from the
///       numbering scheme in the original specification. The reason is that we want to avoid
///       big 'holes' of unused AIGER literals right from the beginning.
  vector<int> cnf_var_to_standalone_aig_var_;

// -------------------------------------------------------------------------------------------
///
/// @brief The list of uncontrollable input variables followed by the present state variables.
///
/// This vector contains the CNF version of the uncontrollable input variables followed by
/// the present state variables. This vector of variables is used often, and thus stored here
/// as a field.
  vector<int> ip_;

// -------------------------------------------------------------------------------------------
///
/// @brief The list of uncontrollable inputs, state variables, and control signals.
///
/// This vector contains the CNF version of the uncontrollable input variables followed by
/// the present state variables, followed by the control signals. This vector of variables is
/// used often, and thus stored here as a field.
  vector<int> ipc_;

// -------------------------------------------------------------------------------------------
///
/// @brief A second (optimized) version of the standalone_circuit.
///
/// We will always finish standalone_circuit_, and then work on standalone_circuit2_ if there
/// is still time.
  aiger *standalone_circuit2_;

// -------------------------------------------------------------------------------------------
///
/// @brief The next unused AIGER literal in #standalone_circuit2_.
  int next_free_aig_lit2_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  ParExtractorWorker(const ParExtractorWorker &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  ParExtractorWorker& operator=(const ParExtractorWorker &other);

};


// -------------------------------------------------------------------------------------------
///
/// @class ParExtractorQBFWorker
/// @brief A worker thread for the ParExtractor.
///
/// This class implements selected circuit extraction methods from the
/// LearningImplExtractor in a parallelized way.
///
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.2.0
class ParExtractorQBFWorker : public ParExtractorWorker
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
///
/// The ParExtractorQBFWorker is implemented in a separate class. The reason is as follows.
/// Instances of this worker can get killed by other threads. That is, the thread is not
/// nicely asked to terminate. It is really killed whereever it is in the code. The reason
/// is that QBF solver calls may take so long that we do not want to wait for them to finish.
/// Killing a thread is essentially done by throwing an exception. The stack is cleaned up,
/// and while doing this, the destructors of all objects on the stack are called. Some
/// objects may be in an inconsistent state, however. Calling their destructor may cause
/// the program to crash. We therefore store also the local variables as fields in this
/// class. This way, we can postpone the call of their destructor.
///
/// @param coordinator A reference to the coordinator. All communication to other
///        worker-threads is done via the coordinator.
/// @param win_region The winning region from which the circuit should be extracted.
/// @param neg_win_region The negation of the winning region.
  ParExtractorQBFWorker(ParExtractor &coordinator,
                        const CNF &win_region,
                        const CNF &neg_win_region);

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~ParExtractorQBFWorker();

// -------------------------------------------------------------------------------------------
///
/// @brief Runs QBF-based learning.
///
/// Roughly implements LearningImplExtractor::runLearningQBFInc().
  void runQBF();

// -------------------------------------------------------------------------------------------
///
/// @brief A C-style wrapper around runQBF() so that we can run it from a phread.
///
/// @param object A pointer to a ParExtractorQBFWorker object.
/// @return I have no idea. Don't use it.
  static void* startQBF(void *object);

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief A local variable of runQBF made a field variable to avoid calls to its destructor.
  DepQBFApi *check_solver_;

// -------------------------------------------------------------------------------------------
///
/// @brief A local variable of runQBF made a field variable to avoid calls to its destructor.
  DepQBFApi *gen_solver_;

// -------------------------------------------------------------------------------------------
///
/// @brief A local variable of runQBF made a field variable to avoid calls to its destructor.
  CNF neg_rel_;

// -------------------------------------------------------------------------------------------
///
/// @brief A local variable of runQBF made a field variable to avoid calls to its destructor.
  CNF check_;

// -------------------------------------------------------------------------------------------
///
/// @brief A local variable of runQBF made a field variable to avoid calls to its destructor.
  CNF gen_;

// -------------------------------------------------------------------------------------------
///
/// @brief A local variable of runQBF made a field variable to avoid calls to its destructor.
  CNF solution_;

// -------------------------------------------------------------------------------------------
///
/// @brief A local variable of runQBF made a field variable to avoid calls to its destructor.
  vector<int> tmp_;

// -------------------------------------------------------------------------------------------
///
/// @brief A local variable of runQBF made a field variable to avoid calls to its destructor.
  vector<int> none_;

// -------------------------------------------------------------------------------------------
///
/// @brief A local variable of runQBF made a field variable to avoid calls to its destructor.
  vector<int> ctrl_univ_;

// -------------------------------------------------------------------------------------------
///
/// @brief A local variable of runQBF made a field variable to avoid calls to its destructor.
  vector<int> ctrl_exist_;

// -------------------------------------------------------------------------------------------
///
/// @brief A local variable of runQBF made a field variable to avoid calls to its destructor.
  vector<int> false_pos_;

// -------------------------------------------------------------------------------------------
///
/// @brief A local variable of runQBF made a field variable to avoid calls to its destructor.
  vector<int> gen_false_pos_;

// -------------------------------------------------------------------------------------------
///
/// @brief A local variable of runQBF made a field variable to avoid calls to its destructor.
  vector<pair<vector<int>, QBFSolver::Quant> > quant_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  ParExtractorQBFWorker(const ParExtractorWorker &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  ParExtractorQBFWorker& operator=(const ParExtractorQBFWorker &other);

};


#endif // ParExtractor_H__
