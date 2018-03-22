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

#include "IFM13QBFSynth.h"
#include "Logger.h"
#include "VarManager.h"
#include "Options.h"
#include "AIG2CNF.h"
#include "Utils.h"
#include "LingelingApi.h"
#include "QBFCertImplExtractor.h"


#define IS_LOSE false
#define IS_GREATER true

// -------------------------------------------------------------------------------------------
IFM13QBFSynth::IFM13QBFSynth() : BackEnd(),
                                 qbf_solver_(Options::instance().getQBFSolver()),
                                 sat_solver_(new LingelingApi())
{
  r_.reserve(10000);
  r_.push_back(AIG2CNF::instance().getUnsafeStates());
  win_.addCNF(AIG2CNF::instance().getSafeStates());

  env_quant_.push_back(make_pair(VarInfo::PRES_STATE, QBFSolver::E));
  env_quant_.push_back(make_pair(VarInfo::INPUT, QBFSolver::E));
  env_quant_.push_back(make_pair(VarInfo::CTRL, QBFSolver::A));
  env_quant_.push_back(make_pair(VarInfo::NEXT_STATE, QBFSolver::E));
  env_quant_.push_back(make_pair(VarInfo::TMP, QBFSolver::E));

  sys_quant_.push_back(make_pair(VarInfo::PRES_STATE, QBFSolver::E));
  sys_quant_.push_back(make_pair(VarInfo::INPUT, QBFSolver::A));
  sys_quant_.push_back(make_pair(VarInfo::CTRL, QBFSolver::E));
  sys_quant_.push_back(make_pair(VarInfo::NEXT_STATE, QBFSolver::E));
  sys_quant_.push_back(make_pair(VarInfo::TMP, QBFSolver::E));
}

// -------------------------------------------------------------------------------------------
IFM13QBFSynth::~IFM13QBFSynth()
{
  delete qbf_solver_;
  qbf_solver_ = NULL;

  delete sat_solver_;
  sat_solver_ = NULL;
}

// -------------------------------------------------------------------------------------------
bool IFM13QBFSynth::run()
{
  //L_DBG(VarManager::instance().toString());
  L_INF("Starting to compute a winning region ...");
  bool realizable = computeWinningRegion();

  if(!realizable)
  {
    L_RES("The specification is unrealizable.");
    return false;
  }
  L_RES("The specification is realizable.");
  if(Options::instance().doRealizabilityOnly())
    return true;

  L_INF("Starting to extract a circuit ...");
  QBFCertImplExtractor extractor;
  extractor.extractCircuit(winning_region_, neg_winning_region_);
  L_INF("Synthesis done.");
  return true;
}

// -------------------------------------------------------------------------------------------
bool IFM13QBFSynth::computeWinningRegion()
{
  const vector<int> &state_vars = VarManager::instance().getVarsOfType(VarInfo::PRES_STATE);
  vector<int> initial_state_cube(state_vars.size(), 0);
  for(size_t cnt = 0; cnt < state_vars.size(); ++cnt)
    initial_state_cube[cnt] = -state_vars[cnt];

  size_t k = 1;
  while(true)
  {
    L_DBG("------ Iteration k=" << k);
    if(recBlockCube(initial_state_cube, k) == IS_LOSE)
      return false;
    //L_DBG("after block");

    //debugCheckInvariants(k);
    //L_DBG("before prop");
    size_t equal = propagateBlockedStates(k);
    //L_DBG("after prop");
    //debugCheckInvariants(k);
    if(equal != 0)
    {
      L_DBG("Found two equal clause sets: R" << (equal-1) << " and R" << equal);
      //L_DBG("Clauses: " << getR(equal).toString());
      winning_region_ = getR(equal);
      winning_region_.negate();
      neg_winning_region_ = getR(equal);
      return true;
    }
    ++k;
  }
}

// -------------------------------------------------------------------------------------------
size_t IFM13QBFSynth::propagateBlockedStates(size_t max_level)
{
  for(size_t i = 0; i <= max_level; ++i)
    getR(i).removeDuplicates();

  for(size_t i = 1; i <= max_level; ++i)
  {
    bool equal = true;
    const list<vector<int> > &r1clauses = getR(i).getClauses();
    CNF& r2cnf = getR(i+1);
    list<vector<int> > r2clauses = r2cnf.getClauses();
    list<vector<int> >::const_iterator it1 = r1clauses.begin();
    list<vector<int> >::iterator it2 = r2clauses.begin();

    for(; it1 != r1clauses.end(); )
    {
      if(*it1 == *it2)
      {
        ++it1;
        ++it2;
        continue;
      }
      const vector<int> &clause = *it1;
      CNF check(getR(i));
      check.swapPresentToNext();
      check.addCNF(AIG2CNF::instance().getTrans());
      for(size_t lit_cnt = 0; lit_cnt < clause.size(); ++lit_cnt)
        check.add1LitClause(-clause[lit_cnt]);
      if(!qbf_solver_->isSat(env_quant_, check))
      {
        // From no state of s (the negated clause) the environment can enforce
        // to go to Ri  -->  no state in s can be part of Ri+1
        it2 = r2clauses.insert(it2, clause);
        ++it1;
        ++it2;
      }
      else
      {
        // Propagation failed
        ++it1;
        equal = false;
      }
    }
    r2cnf.swapWith(r2clauses);
    if(equal)
      return i + 1;
  }
  return 0;
}

// -------------------------------------------------------------------------------------------
bool IFM13QBFSynth::recBlockCube(const vector<int> &state_cube, size_t level)
{

  //L_DBG("recBlockCube called with level " << level);
  //Utils::debugPrint(state_cube);
  while(true)
  {
    if(!win_.isSatBy(state_cube))
      return IS_LOSE;
    if(!getR(level).isSatBy(state_cube))
      return IS_GREATER;

    CNF goto_lower(getR(level - 1));
    goto_lower.swapPresentToNext();
    goto_lower.addCNF(AIG2CNF::instance().getTrans());
    CNF check(goto_lower);
    for(size_t cnt = 0; cnt < state_cube.size(); ++cnt)
      check.add1LitClause(state_cube[cnt]);
    bool env_can_enforce_lower = qbf_solver_->isSat(env_quant_, check);
    if(!env_can_enforce_lower)
    {
      // from state_cube, the environment cannot enforce to reach R[level-1]
      // generalize:
      vector<int> gen_state(state_cube);
      for(size_t cnt1 = 0; cnt1 < state_cube.size(); ++cnt1)
      {
        int lit_to_remove = state_cube[cnt1];
        vector<int> tmp(gen_state);
        for(size_t tmp_cnt = 0; tmp_cnt < tmp.size(); ++tmp_cnt)
        {
          if(tmp[tmp_cnt] == lit_to_remove)
          {
            tmp[tmp_cnt] = tmp.back();
            tmp.pop_back();
            break;
          }
        }
        check = goto_lower;
        for(size_t tmp_cnt = 0; tmp_cnt < tmp.size(); ++tmp_cnt)
          check.add1LitClause(tmp[tmp_cnt]);
        if(!qbf_solver_->isSat(env_quant_, check))
          gen_state = tmp;
      }
      //L_DBG("red in R " << state_cube.size() << " --> " << gen_state.size());

      // remove from R[level] and all smaller frames:
      vector<int> blocking_clause;
      blocking_clause.reserve(gen_state.size() + 1);
      for(size_t cnt = 0; cnt < gen_state.size(); ++cnt)
        blocking_clause.push_back(-gen_state[cnt]);
      blocking_clause.push_back(VarManager::instance().getPresErrorStateVar());
      for(size_t l_cnt = 0; l_cnt <= level; ++l_cnt)
        getR(l_cnt).addClause(blocking_clause);
      return IS_GREATER;
    }

    CNF goto_win(win_);
    goto_win.swapPresentToNext();
    goto_win.addCNF(AIG2CNF::instance().getTrans());
    check = goto_win;
    for(size_t cnt = 0; cnt < state_cube.size(); ++cnt)
      check.add1LitClause(state_cube[cnt]);
    // if level=1, we already know that state_cube cannot be part of W:
    if(level == 1 || !qbf_solver_->isSat(sys_quant_, check))
    {
      // generalize:
      vector<int> gen_state(state_cube);
      for(size_t cnt1 = 0; cnt1 < state_cube.size(); ++cnt1)
      {
        int lit_to_remove = state_cube[cnt1];
        vector<int> tmp(gen_state);
        for(size_t tmp_cnt = 0; tmp_cnt < tmp.size(); ++tmp_cnt)
        {
          if(tmp[tmp_cnt] == lit_to_remove)
          {
            tmp[tmp_cnt] = tmp.back();
            tmp.pop_back();
            break;
          }
        }
        check = goto_win;
        for(size_t tmp_cnt = 0; tmp_cnt < tmp.size(); ++tmp_cnt)
          check.add1LitClause(tmp[tmp_cnt]);
        // TODO: treat gen_state as removed from W
        if(!qbf_solver_->isSat(sys_quant_, check))
          gen_state = tmp;
      }
      //L_DBG("red in W " << state_cube.size() << " --> " << gen_state.size());
      vector<int> blocking_clause(gen_state);
      for(size_t cnt = 0; cnt < blocking_clause.size(); ++cnt)
        blocking_clause[cnt] = -blocking_clause[cnt];
      win_.addClauseAndSimplify(blocking_clause);
      //L_DBG("remove from W " << level);
      //Utils::debugPrint(blocking_clause);
      return IS_LOSE;
    }
    // find a successor which is undecided:
    MASSERT(level > 1, "States in level 1 must always be decidable.");
    CNF undec(win_);
    undec.addCNF(getR(level - 1));
    undec.swapPresentToNext();
    undec.addCNF(AIG2CNF::instance().getTrans());
    for(size_t cnt = 0; cnt < state_cube.size(); ++cnt)
      undec.add1LitClause(state_cube[cnt]);
    const vector<int> &n = VarManager::instance().getVarsOfType(VarInfo::NEXT_STATE);
    vector<int> model;
    bool succ_exists = sat_solver_->isSatModelOrCore(undec, vector<int>(), n, model);
    MASSERT(succ_exists, "There must be an undecided successor");
    vector<int> undecided_successor = Utils::extractNextAsPresent(model);
    recBlockCube(undecided_successor, level - 1);
  }
}

// -------------------------------------------------------------------------------------------
CNF& IFM13QBFSynth::getR(size_t index)
{
  for(size_t i = r_.size(); i <= index; ++i)
    r_.push_back(CNF());
  return r_[index];
}
