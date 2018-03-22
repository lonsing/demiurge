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
/// @file LearnStatisticsSAT.h
/// @brief Contains the declaration of the class LearnStatisticsSAT.
// -------------------------------------------------------------------------------------------

#ifndef LearnStatisticsSAT_H__
#define LearnStatisticsSAT_H__

#include "defines.h"
#include "Stopwatch.h"

// -------------------------------------------------------------------------------------------
///
/// @class LearnStatisticsSAT
/// @brief Collects statistics and performance data for LearnSynthSAT.
///
/// This class collects statistics and performance data for the learning-based synthesis
/// method implemented with two SAT solvers instead of a QBF-solver (implemented in
/// the class LearnSynthSAT).
class LearnStatisticsSAT
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
  LearnStatisticsSAT();

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~LearnStatisticsSAT();

// -------------------------------------------------------------------------------------------
///
/// @brief Called before the computation of the winning region starts.
///
/// This method just starts a Stopwatch.
  void notifyWinRegStart();

// -------------------------------------------------------------------------------------------
///
/// @brief Called after the computation of the winning region is done.
///
/// This method just reads out the Stopwatch and stores the execution time.
  void notifyWinRegEnd();

// -------------------------------------------------------------------------------------------
///
/// @brief Called before the SAT solver is called to compute a counterexample-candidate.
///
/// This method just starts a Stopwatch.
  void notifyBeforeComputeCandidate();

// -------------------------------------------------------------------------------------------
///
/// @brief Called after the SAT solver is done computing a counterexample-candidate.
///
/// This method just reads out the Stopwatch and stores the execution time.
  void notifyAfterComputeCandidate();

// -------------------------------------------------------------------------------------------
///
/// @brief Called whenever the incremental session of the solver is restarted.
///
/// This method is called to count the number of performed restarts.
  void notifyRestart();

// -------------------------------------------------------------------------------------------
///
/// @brief Called before the SAT solver is called to check a counterexample-candidate.
///
/// This method just starts a Stopwatch.
  void notifyBeforeCheckCandidate();

// -------------------------------------------------------------------------------------------
///
/// @brief Called after the SAT solver successfully checked a counterexample-candidate.
///
/// This method just reads out the Stopwatch and stores the execution time. Furthermore, it
/// stores the results of generalizing the counterexample.
///
/// @param from_size The size of the counterexample (number of literals in the cube) before
///        minimization (by computing an unsatisfiable core).
/// @param to_size The size of the counterexample (number of literals in the cube) after
///        minimization (by computing an unsatisfiable core).
  void notifyAfterCheckCandidateFound(size_t from_size, size_t to_size);

// -------------------------------------------------------------------------------------------
///
/// @brief Called after the SAT solver unsuccessfully checked a counterexample-candidate.
///
/// This method just reads out the Stopwatch and stores the execution time.
  void notifyAfterCheckCandidateFailed();

// -------------------------------------------------------------------------------------------
///
/// @brief Called before the SAT solver is called to refine the U-set.
///
/// Refining the U-set means computing an unsatisfiable core of a state-input cube that is
/// useless for the antagonist in trying to break out of the winning region.
/// This method just starts a Stopwatch.
  void notifyBeforeRefine();

// -------------------------------------------------------------------------------------------
///
/// @brief Called after the SAT solver is done refining the U-set.
///
/// This method just reads out the Stopwatch and stores the execution time. Furthermore, it
/// stores the results of generalizing the useless state-input cube.
///
/// @param from_size The size of the useless state-input cube (number of literals in the cube)
///        before minimization (by computing an unsatisfiable core).
/// @param to_size The size of the useless state-input cube (number of literals in the cube)
///        after minimization (by computing an unsatisfiable core).
  void notifyAfterRefine(size_t from_size, size_t to_size);

// -------------------------------------------------------------------------------------------
///
/// @brief Merges the data store in this object with statistics stored in another object.
///
/// This is done by summing all (corresponding) execution times and iteration counters.
/// @param other The source object that should be merged into this object.
  void mergeWith(const LearnStatisticsSAT &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Writes the statistics to stdout as messages of type #Logger::LOG.
///
/// If messages of type Logger::LOG are disabled (see Logger), then nothing happens.
  void logStatistics() const;

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief The time when the computation of the winning region was started.
  PointInTime win_reg_start_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief The winning region computation time in CPU-seconds.
  double win_reg_cpu_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief Like #win_reg_cpu_time_ but real-time.
  size_t win_reg_real_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief The total number of candidate computations.
  size_t nr_of_cand_comp_;

// -------------------------------------------------------------------------------------------
///
/// @brief The total number solver restarts (starting a new incremental session).
  size_t nr_of_restarts_;

// -------------------------------------------------------------------------------------------
///
/// @brief The total number of candidate checks.
  size_t nr_of_cand_checks_;

// -------------------------------------------------------------------------------------------
///
/// @brief The total number of successful candidate checks.
  size_t nr_of_succ_checks_;

// -------------------------------------------------------------------------------------------
///
/// @brief The sum of the counterexample cube sizes (nr of literals) before minimization.
///
/// This is tracked to assess the effectiveness of the minimization (=generalization).
  size_t sum_check_size_from_;

// -------------------------------------------------------------------------------------------
///
/// @brief The sum of the counterexample cube sizes (nr of literals) after minimization.
///
/// This is tracked to assess the effectiveness of the minimization (=generalization).
  size_t sum_check_size_to_;

// -------------------------------------------------------------------------------------------
///
/// @brief The number of refinements of the useless state-input pairs.
  size_t nr_of_refines_;

// -------------------------------------------------------------------------------------------
///
/// @brief The sum of the useless state-input-cube sizes (nr of literals) before minimization.
///
/// This is tracked to assess the effectiveness of the minimization (=generalization).
  size_t sum_refine_size_from_;

// -------------------------------------------------------------------------------------------
///
/// @brief The sum of the useless state-input-cube sizes (nr of literals) after minimization.
///
/// This is tracked to assess the effectiveness of the minimization (=generalization).
  size_t sum_refine_size_to_;

// -------------------------------------------------------------------------------------------
///
/// @brief The time when the last computation of a counterexample-candidate was started.
  PointInTime cand_comp_start_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief The total candidate computation time in CPU-seconds.
  double cand_comp_cpu_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief Like #cand_comp_cpu_time_ but real-time.
  size_t cand_comp_real_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief The time when the last check of a counterexample-candidate was started.
  PointInTime cand_check_start_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief The total candidate checking time in CPU-seconds.
  double cand_check_cpu_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief Like #cand_check_cpu_time_ but real-time.
  size_t cand_check_real_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief The time when the last useless state-input-cube generalization was started.
  PointInTime refine_start_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief The total useless state-input-cube generalization time in CPU-seconds.
  double refine_cpu_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief Like #cand_check_cpu_time_ but real-time.
  size_t refine_real_time_;


private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  LearnStatisticsSAT(const LearnStatisticsSAT &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  LearnStatisticsSAT& operator=(const LearnStatisticsSAT &other);

};

#endif // LearnStatisticsSAT_H__
