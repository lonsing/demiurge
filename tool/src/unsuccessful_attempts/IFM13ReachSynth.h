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

#ifndef IFM13ReachSynth_H__
#define IFM13ReachSynth_H__

#include "defines.h"
#include "CNF.h"
#include "BackEnd.h"

class SatSolver;

class IFM13ReachSynth : public BackEnd
{
public:
  IFM13ReachSynth();
  virtual ~IFM13ReachSynth();
  virtual bool run();

protected:
  bool computeWinningRegion();
  bool analyze(const vector<int> &state_cube, size_t level);
  bool analyzeReach(const vector<int> &state_cube, size_t level, vector<vector<int> > &trace_to_bad);
  bool analyzeReachPoor(const vector<int> &state_cube, size_t level, vector<vector<int> > &trace_to_bad);
  bool indRel(const vector<int> &state_in_cube, size_t level);
  size_t propagateBlockedStates(size_t max_level);
  void addBlockedTransition(const vector<int> &state_in_cube, size_t level);
  void addBlockedState(const vector<int> &cube, size_t level);
  void addLose(const vector<int> &cube);
  void genAndBlockTrans(const vector<int> &state_in_cube,
                        const vector<int> &ctrl_cube,
                        size_t level);
  CNF& getR(size_t index);
  CNF& getU(size_t index);
  SatSolver* getGotoNextLowerSolver(size_t index);
  SatSolver* getGenBlockTransSolver(size_t index);
  SatSolver* getGotoUndecidedSolver(size_t index);
  void debugPrintRs() const;
  void debugCheckInvariants(size_t k);

  vector<CNF> r_;
  vector<CNF> u_;
  SatSolver *goto_win_solver_; // contains T & W'
  vector<SatSolver*> gen_block_trans_solvers_; // contains T & R[k-1]'
  vector<SatSolver*> goto_next_lower_solvers_; // contains U[k] & T & R[k-1]'
  vector<SatSolver*> goto_undecided_solvers_;  // contains U[k] & T & R[k-1]' & W'
  CNF win_;
  vector<int> si_;
  vector<int> sin_;
  vector<int> sicn_;
  vector<int> cn_;
  CNF winning_region_;
  CNF neg_winning_region_;


private:
  IFM13ReachSynth(const IFM13ReachSynth &other);
  IFM13ReachSynth& operator=(const IFM13ReachSynth &other);

};

#endif // IFM13ReachSynth_H__
