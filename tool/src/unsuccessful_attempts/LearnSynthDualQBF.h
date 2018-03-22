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

#ifndef LearnSynthDualQBF_H__
#define LearnSynthDualQBF_H__

#include "defines.h"
#include "CNF.h"
#include "LearnStatistics.h"
#include "VarInfo.h"
#include "QBFSolver.h"
#include "BackEnd.h"


class LearnSynthDualQBF : public BackEnd
{
public:
  LearnSynthDualQBF();
  virtual ~LearnSynthDualQBF();
  virtual bool run();

protected:
  bool computeWinningRegion();
  bool computeWinningRegionOne();
  bool computeCounterexampleQBF(vector<int> &ce);
  bool computeBlockingClause(vector<int> &ce, vector<int> &blocking_clause);

  void reduceExistingClauses(bool take_small_win = true);
  void compressCNFs(bool update_all = true);
  void recomputeCheckCNF(bool take_small_win = true);
  void restrictToStates(vector<int> &vec) const;
  bool generalizeCounterexample(set<int> &ce, bool check_sat = true) const;
  void dumpNegCheckQBF();
  void dumpNegGenQBF(CNF additional_clauses, size_t id);

  CNF neg_winning_region_;
  CNF winning_region_;
  CNF winning_region_large_;
  LearnStatistics statistics_;
  QBFSolver *qbf_solver_;
  vector<pair<VarInfo::VarKind, QBFSolver::Quant> > check_quant_;
  CNF check_cnf_;
  vector<pair<VarInfo::VarKind, QBFSolver::Quant> > gen_quant_;
  CNF generalize_clause_cnf_;
  vector<int> last_check_clause_;
  CNF satCeU_;
  size_t it_count_;
  string filename_prefix_check_;
  string filename_prefix_gen_;
  CNF trans_eq_t_renamed_copy_;
  int t_renamed_copy_;

};

#endif // LearnSynthDualQBF_H__
