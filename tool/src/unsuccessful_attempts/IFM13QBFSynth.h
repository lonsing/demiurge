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

#ifndef IFM13QBFSynth_H__
#define IFM13QBFSynth_H__

#include "defines.h"
#include "CNF.h"
#include "QBFSolver.h"
#include "BackEnd.h"

class LingelingApi;

class IFM13QBFSynth : public BackEnd
{
public:
  IFM13QBFSynth();
  virtual ~IFM13QBFSynth();
  virtual bool run();

protected:
  bool computeWinningRegion();
  size_t propagateBlockedStates(size_t max_level);
  bool recBlockCube(const vector<int> &state_cube, size_t level);
  CNF& getR(size_t index);

  vector<pair<VarInfo::VarKind, QBFSolver::Quant> > env_quant_;
  vector<pair<VarInfo::VarKind, QBFSolver::Quant> > sys_quant_;
  vector<CNF> r_;
  CNF win_;
  QBFSolver *qbf_solver_;
  LingelingApi *sat_solver_;
  CNF winning_region_;
  CNF neg_winning_region_;


private:
  IFM13QBFSynth(const IFM13QBFSynth &other);
  IFM13QBFSynth& operator=(const IFM13QBFSynth &other);

};

#endif // IFM13QBFSynth_H__
