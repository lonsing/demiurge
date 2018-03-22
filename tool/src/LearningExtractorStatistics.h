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
/// @file LearningExtractorStatistics.h
/// @brief Contains the declaration of the class LearningExtractorStatistics.
// -------------------------------------------------------------------------------------------

#ifndef LearningExtractorStatistics_H__
#define LearningExtractorStatistics_H__

#include "defines.h"
#include "Stopwatch.h"

// -------------------------------------------------------------------------------------------
///
/// @class LearningExtractorStatistics
/// @brief A class to collect performance measures for the LearningImplExtractor.
///
/// This class collects statistics and performance data for the learning-based synthesis
/// circuit extraction method implemented using QBF or SAT solving (implemented in the class
/// LearningImplExtractor).
//
/// @author Robert Koenighofer (robert.koenighofer@iaik.tugraz.at)
/// @version 1.1.0
class LearningExtractorStatistics
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
  LearningExtractorStatistics();

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~LearningExtractorStatistics();

// -------------------------------------------------------------------------------------------
///
/// @brief Called before the computation of the next control signal.
///
/// This method just starts a Stopwatch.
  void notifyBeforeCtrlSignal();

// -------------------------------------------------------------------------------------------
///
/// @brief Called after the computation of the next control signal is done.
///
/// This method just reads out the Stopwatch and stores the execution time.
  void notifyAfterCtrlSignal();

// -------------------------------------------------------------------------------------------
///
/// @brief Called before the computation of the next counterexample situation.
///
/// This method just starts a Stopwatch.
  void notifyBeforeClauseComp();

// -------------------------------------------------------------------------------------------
///
/// @brief Called after the computation of the next counterexample situation is done.
///
/// This method just reads out the Stopwatch and stores the execution time.
  void notifyAfterClauseComp();

// -------------------------------------------------------------------------------------------
///
/// @brief Called before the generalization of the next counterexample situation.
///
/// This method just starts a Stopwatch.
  void notifyBeforeClauseMin();

// -------------------------------------------------------------------------------------------
///
/// @brief Called after the generalization of the next counterexample situation is done.
///
/// This method just reads out the Stopwatch and stores the execution time. It also logs
/// the reduction of the clause size.
///
/// @param size_from The size of the clause before the generalization.
/// @param size_to The size of the clause after the generalization.
  void notifyAfterClauseMin(size_t size_from, size_t size_to);

// -------------------------------------------------------------------------------------------
///
/// @brief Called before optimizing the resulting circuit with ABC.
///
/// This method just starts a Stopwatch and logs the circuit size before optimization.
///
/// @param nr_of_and_gates The number of AND gates in the AIGER circuit before optimization
///        with ABC.
  void notifyBeforeABC(size_t nr_of_and_gates);

// -------------------------------------------------------------------------------------------
///
/// @brief Called after optimizing the resulting circuit with ABC is done.
///
/// This method just reads out the Stopwatch and stores the execution time. It also logs
/// the circuit size after optimization.
///
/// @param nr_of_and_gates The number of AND gates in the AIGER circuit after optimization
///        with ABC.
  void notifyAfterABC(size_t nr_of_and_gates);

// -------------------------------------------------------------------------------------------
///
/// @brief Logs the number of new AND gates that have been added to the specification.
///
/// @param nr_of_new_and_gates The number of new AND gates that have been added to the
///        specification.
  void notifyFinalSize(size_t nr_of_new_and_gates);

// -------------------------------------------------------------------------------------------
///
/// @brief Writes the statistics to stdout as messages of type #Logger::LOG.
///
/// If messages of type Logger::LOG are disabled (see Logger), then nothing happens.
///
/// @param include_details True if more detailed information should be printed, false if only
///        basic statistics and performance measures should be printed. If this parameter is
///        omitted, then detailed info will be printed.
  void logStatistics(bool include_details = true) const;

protected:

// -------------------------------------------------------------------------------------------
///
/// @brief The time when the circuit optimization with ABC was started.
  PointInTime abc_start_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief The time for optimizing the circuit with ABC in CPU-seconds.
  double abc_cpu_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief Like #abc_cpu_time_ but real-time.
  size_t abc_real_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief The time when the computation of a counterexample-situation was started.
  PointInTime clause_comp_start_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief The time when the generalization of a counterexample-situation was started.
  PointInTime clause_min_start_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief The time when the computation of the next control signal was started.
  PointInTime ctrl_signal_start_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief The number of counterexample-situations that had to be computed per control signal.
///
/// nr_of_clause_computations_[i] contains the number of counterexample-situations that had
/// to be computed during the computation of an output function for control signal i.
  vector<size_t> nr_of_clause_computations_;

// -------------------------------------------------------------------------------------------
///
/// @brief The sum of the clause size before generalization per control signal.
///
/// sum_clause_size_from_[i] contains the sum of the clause sizes before
/// generalization that had to be computed during the computation of an output function for
/// control signal i.
  vector<size_t> sum_clause_size_from_;

// -------------------------------------------------------------------------------------------
///
/// @brief The sum of the clause size after generalization per control signal.
///
/// sum_clause_size_to_[i] contains the sum of the clause sizes after
/// generalization that had to be computed during the computation of an output function for
/// control signal i.
  vector<size_t> sum_clause_size_to_;

// -------------------------------------------------------------------------------------------
///
/// @brief The total counterexample computation time per control signal.
///
/// sum_clause_comp_cpu_time_[i] contains the total counterexample computation time for
/// control signal i.
  vector<double> sum_clause_comp_cpu_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief Like #sum_clause_comp_cpu_time_ but real-time.
  vector<size_t> sum_clause_comp_real_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief The total counterexample generalization time per control signal.
///
/// sum_clause_min_cpu_time_[i] contains the total counterexample minimization time for
/// control signal i.
  vector<double> sum_clause_min_cpu_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief Like #sum_clause_min_cpu_time_ but real-time.
  vector<size_t> sum_clause_min_real_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief The total time per control signal.
///
/// ctrl_signal_cpu_time_[i] contains the total time for computing the output function of
/// control signal i.
/// This should be roughly sum_clause_comp_cpu_time_[i] + sum_clause_min_cpu_time_[i].
  vector<double> ctrl_signal_cpu_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief Like #ctrl_signal_cpu_time_ but real-time.
  vector<size_t> ctrl_signal_real_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief The circuit size before optimization with ABC.
  size_t size_before_abc_;

// -------------------------------------------------------------------------------------------
///
/// @brief The circuit size after optimization with ABC.
  size_t size_after_abc_;

// -------------------------------------------------------------------------------------------
///
/// @brief The number of new AND gates that have been added to the specification.
  size_t size_final_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  LearningExtractorStatistics(const LearningExtractorStatistics &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  LearningExtractorStatistics& operator=(const LearningExtractorStatistics &other);

};

#endif // LearningExtractorStatistics_H__
