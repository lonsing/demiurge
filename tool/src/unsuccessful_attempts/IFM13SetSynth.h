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

#ifndef IFM13SetSynth_H__
#define IFM13SetSynth_H__

#include "defines.h"
#include "CNF.h"
#include "CNFSet.h"
#include "BackEnd.h"
#include "IFMProofObligation.h"

class SatSolver;

class IFM13SetSynth : public BackEnd
{
public:
  IFM13SetSynth();
  virtual ~IFM13SetSynth();
  virtual bool run();

protected:
  bool computeWinningRegion();
  size_t propagateBlockedStates(size_t max_level);
  bool recBlockCube(const vector<int> &state_cube, size_t level);
  void addBlockedTransition(const vector<int> &state_in_cube, size_t level);
  void addBlockedState(const vector<int> &cube, size_t level);
  bool isBlocked(const vector<int> &state_cube, size_t level);
  void addLose(const vector<int> &cube);
  bool isLose(const vector<int> &state_cube);
  void genAndBlockTrans(const vector<int> &state_in_cube,
                        const vector<int> &ctrl_cube,
                        size_t level);
  CNFSet& getR(size_t index);
  CNF& getU(size_t index);
  SatSolver* getGotoNextLowerSolver(size_t index);
  SatSolver* getGenBlockTransSolver(size_t index);
  IFMProofObligation popMin(list<IFMProofObligation> &queue);
  void debugPrintRs() const;
  void debugPrintCubeOrClause(const vector<int> &cube_or_clause) const;
  void debugCheckInvariants(size_t k);
  void debugCheckWinningRegion() const;

  vector<CNFSet> r_;
  vector<SatSolver*> goto_next_lower_solvers_; // contains U[k] & T & R[k-1]'
  vector<SatSolver*> gen_block_trans_solvers_; // contains T & R[k-1]'
  SatSolver *goto_win_solver_; // contains T & W'
  vector<CNF> u_;
  CNF win_;
  vector<int> sin_;
  vector<int> sicn_;
  CNF winning_region_;
  CNF neg_winning_region_;

private:
  IFM13SetSynth(const IFM13SetSynth &other);
  IFM13SetSynth& operator=(const IFM13SetSynth &other);

};

#endif // IFM13SetSynth_H__
