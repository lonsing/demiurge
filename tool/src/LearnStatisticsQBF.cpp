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
/// @file LearnStatisticsQBF.cpp
/// @brief Contains the definition of the class LearnStatisticsQBF.
// -------------------------------------------------------------------------------------------

#include "LearnStatisticsQBF.h"
#include "Logger.h"

// -------------------------------------------------------------------------------------------
LearnStatisticsQBF::LearnStatisticsQBF() :
  win_reg_cpu_time_(0.0),
  win_reg_real_time_(0),
  rel_det_cpu_time_(0.0),
  rel_det_real_time_(0),
  nr_of_cube_computations_(0),
  sum_compute_cpu_time_(0.0),
  sum_compute_real_time_(0),
  nr_of_cube_min_(0),
  sum_min_cpu_time_(0.0),
  sum_min_real_time_(0),
  sum_cube_size_from_(0),
  sum_cube_size_to_(0)
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
LearnStatisticsQBF::~LearnStatisticsQBF()
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsQBF::notifyWinRegStart()
{
  win_reg_start_time_ = Stopwatch::start();
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsQBF::notifyWinRegEnd()
{
  win_reg_cpu_time_ += Stopwatch::getCPUTimeSec(win_reg_start_time_);
  win_reg_real_time_ += Stopwatch::getRealTimeSec(win_reg_start_time_);
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsQBF::notifyRelDetStart()
{
  rel_det_start_time_ = Stopwatch::start();
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsQBF::notifyRelDetEnd()
{
  rel_det_cpu_time_ += Stopwatch::getCPUTimeSec(rel_det_start_time_);
  rel_det_real_time_ += Stopwatch::getRealTimeSec(rel_det_start_time_);
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsQBF::notifyBeforeComputeCube()
{
  compute_start_time_ = Stopwatch::start();
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsQBF::notifyAfterComputeCube()
{
  ++nr_of_cube_computations_;
  sum_compute_cpu_time_ += Stopwatch::getCPUTimeSec(compute_start_time_);
  sum_compute_real_time_ += Stopwatch::getRealTimeSec(compute_start_time_);
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsQBF::notifyBeforeCubeMin()
{
  min_start_time_ = Stopwatch::start();
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsQBF::notifyAfterCubeMin(size_t from_size, size_t to_size)
{
  ++nr_of_cube_min_;
  sum_min_cpu_time_ += Stopwatch::getCPUTimeSec(min_start_time_);
  sum_min_real_time_ += Stopwatch::getRealTimeSec(min_start_time_);
  sum_cube_size_from_ += from_size;
  sum_cube_size_to_ += to_size;
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsQBF::notifyAfterCubeMin()
{
  sum_min_cpu_time_ += Stopwatch::getCPUTimeSec(min_start_time_);
  sum_min_real_time_ += Stopwatch::getRealTimeSec(min_start_time_);
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsQBF::notifyCubeMin(size_t from_size, size_t to_size)
{
  ++nr_of_cube_min_;
  sum_cube_size_from_ += from_size;
  sum_cube_size_to_ += to_size;
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsQBF::logStatistics() const
{
  L_LOG("Nr. of bad state computations for winning region: " << nr_of_cube_computations_);
  L_LOG(" Bad state computations took:");
  L_LOG("   " << sum_compute_cpu_time_ <<  " sec CPU time in total.");
  L_LOG("   " << sum_compute_real_time_ <<  " sec real time in total.");
  double avg_compute_ctime = 0.0;
  double avg_compute_rtime = 0.0;
  if(nr_of_cube_computations_ != 0)
  {
    avg_compute_ctime = sum_compute_cpu_time_ / nr_of_cube_computations_;
    avg_compute_rtime = static_cast<double>(sum_compute_real_time_)/nr_of_cube_computations_;
  }
  L_LOG("   " << avg_compute_ctime <<  " sec CPU time on average.");
  L_LOG("   " << avg_compute_rtime <<  " sec real time on average.");

  L_LOG("Nr. of cube minimizations for winning region: " << nr_of_cube_min_);
  L_LOG(" Cube minimizations took:");
  L_LOG("   " << sum_min_cpu_time_ <<  " sec CPU time in total.");
  L_LOG("   " << sum_min_real_time_ <<  " sec real time in total.");
  double avg_min_ctime = 0.0;
  double avg_min_rtime = 0.0;
  if(nr_of_cube_min_ != 0)
  {
    avg_min_ctime = sum_min_cpu_time_ / nr_of_cube_min_;
    avg_min_rtime = static_cast<double>(sum_min_real_time_) / nr_of_cube_min_;
  }
  L_LOG("   " << avg_min_ctime <<  " sec CPU time on average.");
  L_LOG("   " << avg_min_rtime <<  " sec real time on average.");

  L_LOG("Total cube size reduction: " << sum_cube_size_from_ << " --> " << sum_cube_size_to_);
  double avg_size_from = 0.0;
  double avg_size_to = 0.0;
  if(nr_of_cube_min_ != 0)
  {
    avg_size_from = static_cast<double>(sum_cube_size_from_) / nr_of_cube_min_;
    avg_size_to = static_cast<double>(sum_cube_size_to_) / nr_of_cube_min_;
  }
  L_LOG("Average cube size reduction: " << avg_size_from  << " --> " << avg_size_to);
  L_LOG("Winning region computation time: " << win_reg_cpu_time_ << " sec CPU time.");
  L_LOG("Winning region computation time: " << win_reg_real_time_ << " sec real time.");
  L_LOG("Relation determinization time: " << rel_det_cpu_time_ << " sec CPU time.");
  L_LOG("Relation determinization time: " << rel_det_real_time_ << " sec real time.");
}

