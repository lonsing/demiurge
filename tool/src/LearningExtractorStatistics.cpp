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
/// @file LearningExtractorStatistics.cpp
/// @brief Contains the definition of the class LearningExtractorStatistics.
// -------------------------------------------------------------------------------------------

#include "LearningExtractorStatistics.h"
#include "VarManager.h"
#include "Logger.h"

// -------------------------------------------------------------------------------------------
LearningExtractorStatistics::LearningExtractorStatistics() :
    size_before_abc_(0),
    size_after_abc_(0),
    size_final_(0)
{
  size_t nr_of_ctrl_vars = VarManager::instance().getVarsOfType(VarInfo::CTRL).size();
  nr_of_clause_computations_.reserve(nr_of_ctrl_vars);
  sum_clause_size_from_.reserve(nr_of_ctrl_vars);
  sum_clause_size_to_.reserve(nr_of_ctrl_vars);
  sum_clause_comp_cpu_time_.reserve(nr_of_ctrl_vars);
  sum_clause_comp_real_time_.reserve(nr_of_ctrl_vars);
  sum_clause_min_cpu_time_.reserve(nr_of_ctrl_vars);
  sum_clause_min_real_time_.reserve(nr_of_ctrl_vars);
  ctrl_signal_cpu_time_.reserve(nr_of_ctrl_vars);
  ctrl_signal_real_time_.reserve(nr_of_ctrl_vars);
}

// -------------------------------------------------------------------------------------------
LearningExtractorStatistics::~LearningExtractorStatistics()
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
void LearningExtractorStatistics::notifyBeforeCtrlSignal()
{
  ctrl_signal_start_time_ = Stopwatch::start();
  nr_of_clause_computations_.push_back(0);
  sum_clause_size_from_.push_back(0);
  sum_clause_size_to_.push_back(0);
  sum_clause_comp_cpu_time_.push_back(0.0);
  sum_clause_comp_real_time_.push_back(0);
  sum_clause_min_cpu_time_.push_back(0.0);
  sum_clause_min_real_time_.push_back(0);
}

// -------------------------------------------------------------------------------------------
void LearningExtractorStatistics::notifyAfterCtrlSignal()
{
  ctrl_signal_cpu_time_.push_back(Stopwatch::getCPUTimeSec(ctrl_signal_start_time_));
  ctrl_signal_real_time_.push_back(Stopwatch::getRealTimeSec(ctrl_signal_start_time_));
}

// -------------------------------------------------------------------------------------------
void LearningExtractorStatistics::notifyBeforeClauseComp()
{
  clause_comp_start_time_ = Stopwatch::start();
}

// -------------------------------------------------------------------------------------------
void LearningExtractorStatistics::notifyAfterClauseComp()
{
  nr_of_clause_computations_.back() += 1;
  sum_clause_comp_cpu_time_.back() += Stopwatch::getCPUTimeSec(clause_comp_start_time_);
  sum_clause_comp_real_time_.back() += Stopwatch::getRealTimeSec(clause_comp_start_time_);
}

// -------------------------------------------------------------------------------------------
void LearningExtractorStatistics::notifyBeforeClauseMin()
{
  clause_min_start_time_ = Stopwatch::start();
}

// -------------------------------------------------------------------------------------------
void LearningExtractorStatistics::notifyAfterClauseMin(size_t size_from, size_t size_to)
{
  sum_clause_min_cpu_time_.back() += Stopwatch::getCPUTimeSec(clause_min_start_time_);
  sum_clause_min_real_time_.back() += Stopwatch::getRealTimeSec(clause_min_start_time_);
  sum_clause_size_from_.back() += size_from;
  sum_clause_size_to_.back() += size_to;
}

// -------------------------------------------------------------------------------------------
void LearningExtractorStatistics::notifyBeforeABC(size_t nr_of_and_gates)
{
  abc_start_time_ = Stopwatch::start();
  size_before_abc_ = nr_of_and_gates;
}

// -------------------------------------------------------------------------------------------
void LearningExtractorStatistics::notifyAfterABC(size_t nr_of_and_gates)
{
  abc_cpu_time_ = Stopwatch::getCPUTimeSec(abc_start_time_);
  abc_real_time_ = Stopwatch::getRealTimeSec(abc_start_time_);
  size_after_abc_ = nr_of_and_gates;
}

// -------------------------------------------------------------------------------------------
void LearningExtractorStatistics::notifyFinalSize(size_t nr_of_new_and_gates)
{
  size_final_ = nr_of_new_and_gates;
}

// -------------------------------------------------------------------------------------------
void LearningExtractorStatistics::logStatistics(bool include_details) const
{
  // Size:
  L_LOG("Final circuit size: " << size_final_ << " new AND gates.");
  L_LOG("Size before ABC: " << size_before_abc_ << " AND gates.");
  L_LOG("Size after ABC: " << size_after_abc_ << " AND gates.");

  // Time ABC:
  L_LOG("Time for optimizing with ABC: " << abc_real_time_ << " seconds.");

  // Time per control signal:
  double total_ctrl_signal_cpu_time = 0.0;
  size_t total_ctrl_signal_real_time = 0;
  for(size_t cnt = 0; cnt < ctrl_signal_cpu_time_.size(); ++ cnt)
  {
    total_ctrl_signal_cpu_time += ctrl_signal_cpu_time_[cnt];
    total_ctrl_signal_real_time += ctrl_signal_real_time_[cnt];
  }
  ostringstream ss;
  ss << "Total time for all control signals: " << total_ctrl_signal_cpu_time;
  ss << "/" << total_ctrl_signal_real_time  << " sec";
  if(include_details)
  {
    ss << " (";
    for(size_t cnt = 0; cnt < ctrl_signal_cpu_time_.size(); ++ cnt)
    {
      ss << ctrl_signal_cpu_time_[cnt] << "/" << ctrl_signal_real_time_[cnt] << " sec";
      if(cnt < ctrl_signal_cpu_time_.size() - 1)
        ss << ", ";
    }
    ss << " )";
  }
  L_LOG(ss.str());

  // Nr of iterations:
  size_t total_nr_of_iterations = 0;
  for(size_t cnt = 0; cnt < nr_of_clause_computations_.size(); ++ cnt)
    total_nr_of_iterations += nr_of_clause_computations_[cnt];
  ostringstream its;
  its << "Nr of iterations: " << total_nr_of_iterations;
  if(include_details)
  {
    its << " (";
    for(size_t cnt = 0; cnt < nr_of_clause_computations_.size(); ++ cnt)
    {
      its << nr_of_clause_computations_[cnt];
      if(cnt < nr_of_clause_computations_.size() - 1)
        its << ", ";
    }
    its << " )";
  }
  L_LOG(its.str());

  // Clause computation time:
  double total_cand_comp_cpu_time = 0.0;
  size_t total_cand_comp_real_time = 0;
  for(size_t cnt = 0; cnt < sum_clause_comp_cpu_time_.size(); ++ cnt)
  {
    total_cand_comp_cpu_time += sum_clause_comp_cpu_time_[cnt];
    total_cand_comp_real_time += sum_clause_comp_real_time_[cnt];
  }
  ostringstream cs;
  cs << "Total clause computation time: " << total_cand_comp_cpu_time;
  cs << "/" << total_cand_comp_real_time  << " sec";
  if(include_details)
  {
    cs << " (";
    for(size_t cnt = 0; cnt < sum_clause_comp_cpu_time_.size(); ++ cnt)
    {
      cs << sum_clause_comp_cpu_time_[cnt] << "/" << sum_clause_comp_cpu_time_[cnt] << " sec";
      if(cnt < sum_clause_comp_cpu_time_.size() - 1)
        cs << ", ";
    }
    cs << " )";
  }
  L_LOG(cs.str());

  // Clause minimization time:
  double total_cand_min_cpu_time = 0.0;
  size_t total_cand_min_real_time = 0;
  for(size_t cnt = 0; cnt < sum_clause_min_cpu_time_.size(); ++ cnt)
  {
    total_cand_min_cpu_time += sum_clause_min_cpu_time_[cnt];
    total_cand_min_real_time += sum_clause_min_real_time_[cnt];
  }
  ostringstream ms;
  ms << "Total clause minimization time: " << total_cand_min_cpu_time;
  ms << "/" << total_cand_min_real_time  << " sec";
  if(include_details)
  {
    ms << " (";
    for(size_t cnt = 0; cnt < sum_clause_min_cpu_time_.size(); ++ cnt)
    {
      ms << sum_clause_min_cpu_time_[cnt] << "/" << sum_clause_min_cpu_time_[cnt] << " sec";
      if(cnt < sum_clause_min_cpu_time_.size() - 1)
        ms << ", ";
    }
    ms << " )";
  }
  L_LOG(ms.str());

  // Clause reduction:
  size_t total_from_size = 0;
  size_t total_to_size = 0;
  for(size_t cnt = 0; cnt < sum_clause_size_from_.size(); ++ cnt)
  {
    total_from_size += sum_clause_size_from_[cnt];
    total_to_size += sum_clause_size_to_[cnt];
  }
  ostringstream ts;
  ts << "Total clause size reduction: " << total_from_size  << " --> " << total_to_size;
  if(include_details)
  {
    ts << " (";
    for(size_t cnt = 0; cnt < sum_clause_size_from_.size(); ++ cnt)
    {
      ts << sum_clause_size_from_[cnt] << " --> " << sum_clause_size_to_[cnt];
      if(cnt < sum_clause_size_from_.size() - 1)
        ts << ", ";
    }
    ts << " )";
  }
  L_LOG(ts.str());
  double avg_size_from = 0.0;
  double avg_size_to = 0.0;
  if(total_nr_of_iterations != 0)
  {
    avg_size_from = static_cast<double>(total_from_size) / total_nr_of_iterations;
    avg_size_to = static_cast<double>(total_to_size) / total_nr_of_iterations;
  }
  ostringstream as;
  as << "Average clause size reduction: " << avg_size_from  << " --> " << avg_size_to;
  if(include_details)
  {
    as << " (";
    for(size_t cnt = 0; cnt < sum_clause_size_from_.size(); ++ cnt)
    {
      avg_size_from = 0.0;
      avg_size_to = 0.0;
      if(nr_of_clause_computations_[cnt] != 0)
      {
        avg_size_from = static_cast<double>(sum_clause_size_from_[cnt]);
        avg_size_from /= nr_of_clause_computations_[cnt];
        avg_size_to = static_cast<double>(sum_clause_size_to_[cnt]);
        avg_size_to /= nr_of_clause_computations_[cnt];
      }
      as << avg_size_from << " --> " << avg_size_to;
      if(cnt < sum_clause_size_from_.size() - 1)
        as << ", ";
    }
    as << " )";
  }
  L_LOG(as.str());

}
