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

#ifndef IC3Synth_H__
#define IC3Synth_H__

#include "defines.h"
#include "BackEnd.h"
#include "QBFSolver.h"
#include "CNF.h"

class LingelingApi;

class IC3Synth : public BackEnd
{
public:
  IC3Synth();
  virtual ~IC3Synth();
  virtual bool run();

protected:
  bool computeWinningRegion();
  bool strengthen(size_t level);
  void propagateClauses(size_t max_level);
  bool excludeInd(const vector<int> &counterexample_cube, size_t level);
  void excludeForce(const vector<int> &counterexample_cube, size_t level);
  vector<int> blockReach(const vector<int> &counterexample_cube, size_t level);
  vector<int> blockForce(const vector<int> &counterexample_cube, size_t level);
  CNF& getF(size_t index);
  void restrictToStates(vector<int> &vec) const;
  bool containsInitialState(const set<int> &cube) const;

  CNF winning_region_;
  vector<CNF> f_;
  vector<pair<VarInfo::VarKind, QBFSolver::Quant> > check_quant_;
  vector<pair<VarInfo::VarKind, QBFSolver::Quant> > gen_quant_;
  QBFSolver *qbf_solver_;
  LingelingApi *sat_solver_;

private:
  IC3Synth(const IC3Synth &other);
  IC3Synth& operator=(const IC3Synth &other);


};

#endif // IC3Synth_H__
