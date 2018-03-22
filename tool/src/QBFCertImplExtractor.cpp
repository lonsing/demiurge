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
/// @file QBFCertImplExtractor.cpp
/// @brief Contains the definition of the class QBFCertImplExtractor.
// -------------------------------------------------------------------------------------------

#include "QBFCertImplExtractor.h"
#include "VarManager.h"
#include "AIG2CNF.h"
#include "CNF.h"
#include "DepQBFExt.h"
#include "StringUtils.h"
#include "Options.h"
#include "Logger.h"
#include "FileUtils.h"
#include "Stopwatch.h"

extern "C" {
  #include "aiger.h"
}

// -------------------------------------------------------------------------------------------
QBFCertImplExtractor::QBFCertImplExtractor() :
    size_before_abc_(0),
    size_after_abc_(0),
    size_final_(0),
    qbfcert_real_time_(0),
    abc_real_time_(0)
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
QBFCertImplExtractor::~QBFCertImplExtractor()
{
  // nothing to do
}

// -------------------------------------------------------------------------------------------
void QBFCertImplExtractor::run(const CNF &winning_region, const CNF &neg_winning_region)
{
  // We would like to find skolem functions for the c signals in:
  // forall x,i: exists c,x': (NOT W(x)) OR ( T(x,i,c,x') AND W(x')),
  // where W(x) denotes the winning region.

  // Instead, we compute Herbrand functions for the c signals in:
  // exists x,i: forall c: exists x': W(x) AND T(x,i,c,x') AND NOT W(x')),
  // where W(x) denotes the winning region.

  CNF strategy(neg_winning_region);
  strategy.swapPresentToNext();
  strategy.addCNF(AIG2CNF::instance().getTrans());
  strategy.addCNF(winning_region);

  // build the quantifier prefix:
  vector<pair<VarInfo::VarKind, DepQBFExt::Quant> > quant;
  quant.push_back(make_pair(VarInfo::INPUT, DepQBFExt::E));
  quant.push_back(make_pair(VarInfo::PRES_STATE, DepQBFExt::E));
  quant.push_back(make_pair(VarInfo::CTRL, DepQBFExt::A));
  quant.push_back(make_pair(VarInfo::NEXT_STATE, DepQBFExt::E));
  quant.push_back(make_pair(VarInfo::TMP, DepQBFExt::E));

  // now we compute functions for the control signals c:
  DepQBFExt solver;
#ifndef NDEBUG
  MASSERT(solver.isSat(quant,strategy) == false, "Error in winning region.");
#endif

  PointInTime qbfcert_start_time = Stopwatch::start();
  aiger* answer = solver.qbfCert(quant, strategy);
  qbfcert_real_time_ = Stopwatch::getRealTimeSec(qbfcert_start_time);

  // The answer contains all existentially quantified variables
  // (INPUT, PRES_STATE, NEXT_STATE, TMP) as inputs (in this order). We need to remove them.
  // We also need to remove the error-state variable as input.
  size_t nr_in = VarManager::instance().getVarsOfType(VarInfo::INPUT).size();
  nr_in += VarManager::instance().getVarsOfType(VarInfo::PRES_STATE).size() - 1;

  // remove error-state from inputs:
  size_t err_idx = VarManager::instance().getVarsOfType(VarInfo::INPUT).size();
  unsigned error_state_in_aig = answer->inputs[err_idx].lit;
  unsigned neg_error_state_in_aig = error_state_in_aig + 1;
  for(; err_idx < nr_in + 1; ++err_idx)
    answer->inputs[err_idx].lit = answer->inputs[err_idx+1].lit;
  // remove all other inputs:
  answer->num_inputs = nr_in;

  // make sure the error-state does not occur as input to any AND gate:
  for(unsigned cnt = 0; cnt < answer->num_ands; ++cnt)
  {
    if(answer->ands[cnt].rhs0 == error_state_in_aig)
      answer->ands[cnt].rhs0 = 0;
    if(answer->ands[cnt].rhs1 == error_state_in_aig)
      answer->ands[cnt].rhs1 = 0;
    if(answer->ands[cnt].rhs0 == neg_error_state_in_aig)
      answer->ands[cnt].rhs0 = 1;
    if(answer->ands[cnt].rhs1 == neg_error_state_in_aig)
      answer->ands[cnt].rhs1 = 1;
  }

  // if ctrl-variables are not assigned by the aiger-circuit (this happens sometimes)
  // they must be irrelevant, and are assigned to FALSE:
  unsigned nr_ctrl = VarManager::instance().getVarsOfType(VarInfo::CTRL).size();
  if(answer->num_outputs < nr_ctrl)
  {
    L_DBG("QBFCert did not assign all ctrl-signals. Setting them to 0.");
  }
  while(answer->num_outputs < nr_ctrl)
    aiger_add_output(answer, 0, NULL);

  aiger_reencode(answer);
  size_before_abc_ = answer->num_ands;
  PointInTime abc_start_time = Stopwatch::start();
  aiger *optimized = optimizeWithABC(answer);
  abc_real_time_ = Stopwatch::getRealTimeSec(abc_start_time);
  size_after_abc_ = optimized->num_ands;
  aiger_reset(answer);
  size_final_ = insertIntoSpec(optimized);
  aiger_reset(optimized);
}


// -------------------------------------------------------------------------------------------
void QBFCertImplExtractor::logDetailedStatistics()
{
  L_LOG("Final circuit size: " << size_final_ << " new AND gates.");
  L_LOG("Size before ABC: " << size_before_abc_ << " AND gates.");
  L_LOG("Size after ABC: " << size_after_abc_ << " AND gates.");
  L_LOG("Time for QBFCert: " << qbfcert_real_time_ << " seconds.");
  L_LOG("Time for optimizing with ABC: " << abc_real_time_ << " seconds.");
}
