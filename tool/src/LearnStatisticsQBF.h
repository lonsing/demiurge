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
/// @file LearnStatisticsQBF.h
/// @brief Contains the declaration of the class LearnStatisticsQBF.
// -------------------------------------------------------------------------------------------

#ifndef LearnStatisticsQBF_H__
#define LearnStatisticsQBF_H__

#include "defines.h"
#include "Stopwatch.h"

// -------------------------------------------------------------------------------------------
///
/// @class LearnStatisticsQBF
/// @brief Collects statistics and performance data for LearnSynthQBF (and all its variants).
///
/// This class collects statistics and performance data for the learning-based synthesis
/// method implemented with a QBF-solver (implemented in the class LearnSynthQBF).
class LearnStatisticsQBF
{
public:

// -------------------------------------------------------------------------------------------
///
/// @brief Constructor.
  LearnStatisticsQBF();

// -------------------------------------------------------------------------------------------
///
/// @brief Destructor.
  virtual ~LearnStatisticsQBF();

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
/// @brief Called before the relation determinization (the computation of circuits) starts.
///
/// This method just starts a Stopwatch.
  void notifyRelDetStart();

// -------------------------------------------------------------------------------------------
///
/// @brief Called after the relation determinization (the computation of circuits) starts.
///
/// This method just reads out the Stopwatch and stores the execution time.
  void notifyRelDetEnd();

// -------------------------------------------------------------------------------------------
///
/// @brief Called before the QBF solver is called to compute a counterexample-state.
///
/// This method just starts a Stopwatch.
  void notifyBeforeComputeCube();

// -------------------------------------------------------------------------------------------
///
/// @brief Called after the QBF solver is done computing a counterexample-state.
///
/// This method just reads out the Stopwatch and stores the execution time.
  void notifyAfterComputeCube();

// -------------------------------------------------------------------------------------------
///
/// @brief Called before the generalization of the counterexample-state starts.
///
/// This method just starts a Stopwatch.
  void notifyBeforeCubeMin();

// -------------------------------------------------------------------------------------------
///
/// @brief Called after the generalization of the counterexample-state is done.
///
/// This method just reads out the Stopwatch and stores the execution time. Furthermore, it
/// stores the results of generalizing the counterexample.
///
/// @param from_size The size of the counterexample (number of literals in the cube) before
///        generalization (by dropping literals from the cube).
/// @param to_size The size of the counterexample (number of literals in the cube) after
///        generalization (by dropping literals from the cube).
  void notifyAfterCubeMin(size_t from_size, size_t to_size);

// -------------------------------------------------------------------------------------------
///
/// @brief Called after the generalization of the counterexample-state is done.
///
/// This method just reads out the Stopwatch and stores the execution time. It does not
/// store the results of generalizing the counterexample. This can be done with a call to
/// @link #notifyCubeMin notifyCubeMin() @endlink.
  void notifyAfterCubeMin();

// -------------------------------------------------------------------------------------------
///
/// @brief Stores the result of a counterexample-cube generalization.
///
/// This method is useful if #notifyAfterCubeMin() is called without parameters.
///
/// @param from_size The size of the counterexample (number of literals in the cube) before
///        generalization (by dropping literals from the cube).
/// @param to_size The size of the counterexample (number of literals in the cube) after
///        generalization (by dropping literals from the cube).
  void notifyCubeMin(size_t from_size, size_t to_size);

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
/// @brief The time when the computation of the circuits was started.
  PointInTime rel_det_start_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief circuit computation time in CPU-seconds.
  double rel_det_cpu_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief Like #rel_det_cpu_time_ but real-time.
  size_t rel_det_real_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief The total number of counterexample-cube computations.
  size_t nr_of_cube_computations_;

// -------------------------------------------------------------------------------------------
///
/// @brief The time when the last computation of a counterexample-cube was started.
  PointInTime compute_start_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief The total counterexample computation time in CPU-seconds.
  double sum_compute_cpu_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief Like #sum_compute_cpu_time_ but real-time.
  size_t sum_compute_real_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief The total number of counterexample-cube generalizations.
  size_t nr_of_cube_min_;

// -------------------------------------------------------------------------------------------
///
/// @brief The time when the last generalization of a counterexample-cube was started.
  PointInTime min_start_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief The total counterexample generalization time in CPU-seconds.
  double sum_min_cpu_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief Like #sum_min_cpu_time_ but real-time.
  size_t sum_min_real_time_;

// -------------------------------------------------------------------------------------------
///
/// @brief The sum of the counterexample cube sizes (nr of literals) before generalization.
///
/// This is tracked to assess the effectiveness of the generalization.
  size_t sum_cube_size_from_;

// -------------------------------------------------------------------------------------------
///
/// @brief The sum of the counterexample cube sizes (nr of literals) after generalization.
///
/// This is tracked to assess the effectiveness of the generalization.
  size_t sum_cube_size_to_;

private:

// -------------------------------------------------------------------------------------------
///
/// @brief Copy constructor.
///
/// The copy constructor is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
  LearnStatisticsQBF(const LearnStatisticsQBF &other);

// -------------------------------------------------------------------------------------------
///
/// @brief Assignment operator.
///
/// The assignment operator is disabled (set private) and not implemented.
///
/// @param other The source for creating the copy.
/// @return The result of the assignment, i.e, *this.
  LearnStatisticsQBF& operator=(const LearnStatisticsQBF &other);

};

#endif // LearnStatisticsQBF_H__
