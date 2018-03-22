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
/// @file LearnStatisticsSAT.cpp
/// @brief Contains the definition of the class LearnStatisticsSAT.
// -------------------------------------------------------------------------------------------

#include "LearnStatisticsSAT.h"
#include "Logger.h"

// -------------------------------------------------------------------------------------------
LearnStatisticsSAT::LearnStatisticsSAT() :
                       win_reg_cpu_time_(0.0),
                       win_reg_real_time_(0),
                       rel_det_cpu_time_(0.0),
                       rel_det_real_time_(0),
                       nr_of_cand_comp_(0),
                       nr_of_restarts_(0),
                       nr_of_cand_checks_(0),
                       nr_of_succ_checks_(0),
                       sum_check_size_from_(0),
                       sum_check_size_to_(0),
                       nr_of_refines_(0),
                       sum_refine_size_from_(0),
                       sum_refine_size_to_(0),
                       cand_comp_cpu_time_(0.0),
                       cand_comp_real_time_(0),
                       cand_check_cpu_time_(0.0),
                       cand_check_real_time_(0),
                       refine_cpu_time_(0.0),
                       refine_real_time_(0)
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
LearnStatisticsSAT::~LearnStatisticsSAT()
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsSAT::notifyWinRegStart()
{
  win_reg_start_time_ = Stopwatch::start();
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsSAT::notifyWinRegEnd()
{
  win_reg_cpu_time_ += Stopwatch::getCPUTimeSec(win_reg_start_time_);
  win_reg_real_time_ += Stopwatch::getRealTimeSec(win_reg_start_time_);
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsSAT::notifyRelDetStart()
{
  rel_det_start_time_ = Stopwatch::start();
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsSAT::notifyRelDetEnd()
{
  rel_det_cpu_time_ += Stopwatch::getCPUTimeSec(rel_det_start_time_);
  rel_det_real_time_ += Stopwatch::getRealTimeSec(rel_det_start_time_);
}


// -------------------------------------------------------------------------------------------
void LearnStatisticsSAT::notifyBeforeComputeCandidate()
{
  cand_comp_start_time_ = Stopwatch::start();
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsSAT::notifyAfterComputeCandidate()
{
  cand_comp_cpu_time_ += Stopwatch::getCPUTimeSec(cand_comp_start_time_);
  cand_comp_real_time_ += Stopwatch::getRealTimeSec(cand_comp_start_time_);
  ++nr_of_cand_comp_;
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsSAT::notifyRestart()
{
  ++nr_of_restarts_;
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsSAT::notifyBeforeCheckCandidate()
{
  cand_check_start_time_ = Stopwatch::start();
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsSAT::notifyAfterCheckCandidateFound(size_t from_size, size_t to_size)
{
  cand_check_cpu_time_ += Stopwatch::getCPUTimeSec(cand_check_start_time_);
  cand_check_real_time_ += Stopwatch::getRealTimeSec(cand_check_start_time_);
  sum_check_size_from_ += from_size;
  sum_check_size_to_ += to_size;
  ++nr_of_succ_checks_;
  ++nr_of_cand_checks_;
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsSAT::notifyAfterCheckCandidateFailed()
{
  cand_check_cpu_time_ += Stopwatch::getCPUTimeSec(cand_check_start_time_);
  cand_check_real_time_ += Stopwatch::getRealTimeSec(cand_check_start_time_);
  ++nr_of_cand_checks_;
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsSAT::notifyBeforeRefine()
{
  refine_start_time_ = Stopwatch::start();
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsSAT::notifyAfterRefine(size_t from_size, size_t to_size)
{
  refine_cpu_time_ += Stopwatch::getCPUTimeSec(refine_start_time_);
  refine_real_time_ += Stopwatch::getRealTimeSec(refine_start_time_);
  ++nr_of_refines_;
  sum_refine_size_from_ += from_size;
  sum_refine_size_to_ += to_size;
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsSAT::mergeWith(const LearnStatisticsSAT &other)
{
  win_reg_cpu_time_ += other.win_reg_cpu_time_;
  win_reg_real_time_ += other.win_reg_real_time_;
  rel_det_cpu_time_ += other.rel_det_cpu_time_;
  rel_det_real_time_ += other.rel_det_real_time_;
  nr_of_cand_comp_ += other.nr_of_cand_comp_;
  nr_of_restarts_ += other.nr_of_restarts_;
  nr_of_cand_checks_ += other.nr_of_cand_checks_;
  nr_of_succ_checks_ += other.nr_of_succ_checks_;
  sum_check_size_from_ += other.sum_check_size_from_;
  sum_check_size_to_ += other.sum_check_size_to_;
  nr_of_refines_ += other.nr_of_refines_;
  sum_refine_size_from_ += other.sum_refine_size_from_;
  sum_refine_size_to_ += other.sum_refine_size_to_;
  cand_comp_cpu_time_ += other.cand_comp_cpu_time_;
  cand_comp_real_time_ += other.cand_comp_real_time_;
  cand_check_cpu_time_ += other.cand_check_cpu_time_;
  cand_check_real_time_ += other.cand_check_real_time_;
  refine_cpu_time_ += other.refine_cpu_time_;
  refine_real_time_ += other.refine_real_time_;
}

// -------------------------------------------------------------------------------------------
void LearnStatisticsSAT::logStatistics() const
{
  L_LOG("Nr. of bad state candidate computations: " << nr_of_cand_comp_);
  L_LOG(" Bad state computations took:");
  L_LOG("   " << cand_comp_cpu_time_ <<  " sec CPU time in total.");
  L_LOG("   " << cand_comp_real_time_ <<  " sec real time in total.");
  double avg_cand_comp_ctime = 0.0;
  double avg_cand_comp_rtime = 0.0;
  if(nr_of_cand_comp_ != 0)
  {
    avg_cand_comp_ctime = cand_comp_cpu_time_ / nr_of_cand_comp_;
    avg_cand_comp_rtime = static_cast<double>(cand_comp_real_time_)/nr_of_cand_comp_;
  }
  L_LOG("   " << avg_cand_comp_ctime <<  " sec CPU time on average.");
  L_LOG("   " << avg_cand_comp_rtime <<  " sec real time on average.");
  L_LOG(" Nr. of restarts in bad state candidate computations: " << nr_of_restarts_);


  L_LOG("Nr. of bad state candidate checks: " << nr_of_cand_checks_);
  L_LOG(" Successful: " << nr_of_succ_checks_);
  L_LOG(" Bad state candidate checks took:");
  L_LOG("   " << cand_check_cpu_time_ <<  " sec CPU time in total.");
  L_LOG("   " << cand_check_real_time_ <<  " sec real time in total.");
  double avg_cand_check_ctime = 0.0;
  double avg_cand_check_rtime = 0.0;
  if(nr_of_cand_checks_ != 0)
  {
    avg_cand_check_ctime = cand_check_cpu_time_ / nr_of_cand_checks_;
    avg_cand_check_rtime = static_cast<double>(cand_check_real_time_) / nr_of_cand_checks_;
  }
  L_LOG("   " << avg_cand_check_ctime <<  " sec CPU time on average.");
  L_LOG("   " << avg_cand_check_rtime <<  " sec real time on average.");

  L_LOG(" Total bad state cube size reduction: "
        << sum_check_size_from_ << " --> " << sum_check_size_to_);
  double avg_check_size_from = 0.0;
  double avg_check_size_to = 0.0;
  if(nr_of_succ_checks_ != 0)
  {
    avg_check_size_from = static_cast<double>(sum_check_size_from_) / nr_of_succ_checks_;
    avg_check_size_to = static_cast<double>(sum_check_size_to_) / nr_of_succ_checks_;
  }
  L_LOG(" Average bad state cube size reduction: "
        << avg_check_size_from  << " --> " << avg_check_size_to);

  L_LOG("Nr. of refinements: " << nr_of_refines_);
  L_LOG(" Refinements took:");
  L_LOG("   " << refine_cpu_time_ <<  " sec CPU time in total.");
  L_LOG("   " << refine_real_time_ <<  " sec real time in total.");
  double avg_refine_ctime = 0.0;
  double avg_refine_rtime = 0.0;
  if(nr_of_refines_ != 0)
  {
    avg_refine_ctime = refine_cpu_time_ / nr_of_refines_;
    avg_refine_rtime = static_cast<double>(refine_real_time_) / nr_of_refines_;
  }
  L_LOG("   " << avg_refine_ctime <<  " sec CPU time on average.");
  L_LOG("   " << avg_refine_rtime <<  " sec real time on average.");

  L_LOG(" Total refinement cube size reduction: "
        << sum_refine_size_from_ << " --> " << sum_refine_size_to_);
  double avg_refine_from = 0.0;
  double avg_refine_to = 0.0;
  if(nr_of_refines_ != 0)
  {
    avg_refine_from = static_cast<double>(sum_refine_size_from_) / nr_of_refines_;
    avg_refine_to = static_cast<double>(sum_refine_size_to_) / nr_of_refines_;
  }
  L_LOG(" Average refinement cube size reduction: "
        << avg_refine_from  << " --> " << avg_refine_to);
  L_LOG("Winning region computation time: " << win_reg_cpu_time_ << " sec CPU time.");
  L_LOG("Winning region computation time: " << win_reg_real_time_ << " sec real time.");
  L_LOG("Relation determinization time: " << rel_det_cpu_time_ << " sec CPU time.");
  L_LOG("Relation determinization time: " << rel_det_real_time_ << " sec real time.");
}
