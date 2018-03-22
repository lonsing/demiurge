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

#include "IFM13ReachSynth.h"
#include "VarManager.h"
#include "Options.h"
#include "AIG2CNF.h"
#include "Logger.h"
#include "SatSolver.h"
#include "DepQBFApi.h"
#include "Utils.h"
#include "QBFCertImplExtractor.h"

#define IS_LOSE false
#define IS_GREATER true

// -------------------------------------------------------------------------------------------
IFM13ReachSynth::IFM13ReachSynth() :
                 BackEnd()
{
  const vector<int> &s = VarManager::instance().getVarsOfType(VarInfo::PRES_STATE);
  const vector<int> &n = VarManager::instance().getVarsOfType(VarInfo::NEXT_STATE);
  const vector<int> &c = VarManager::instance().getVarsOfType(VarInfo::CTRL);
  const vector<int> &i = VarManager::instance().getVarsOfType(VarInfo::INPUT);

  si_.reserve(s.size() + i.size());
  si_.insert(si_.end(), s.begin(), s.end());
  si_.insert(si_.end(), i.begin(), i.end());


  sin_.reserve(s.size() + i.size() + n.size());
  sin_.insert(sin_.end(), s.begin(), s.end());
  sin_.insert(sin_.end(), i.begin(), i.end());
  sin_.insert(sin_.end(), n.begin(), n.end());

  sicn_.reserve(s.size() + i.size() + c.size() + n.size());
  sicn_.insert(sicn_.end(), s.begin(), s.end());
  sicn_.insert(sicn_.end(), i.begin(), i.end());
  sicn_.insert(sicn_.end(), c.begin(), c.end());
  sicn_.insert(sicn_.end(), n.begin(), n.end());

  cn_.reserve(c.size() + n.size());
  cn_.insert(cn_.end(), c.begin(), c.end());
  cn_.insert(cn_.end(), n.begin(), n.end());

  r_.reserve(10000);
  r_.push_back(AIG2CNF::instance().getUnsafeStates());
  u_.reserve(10000);
  u_.push_back(AIG2CNF::instance().getUnsafeStates());
  win_.addCNF(AIG2CNF::instance().getSafeStates());

  goto_next_lower_solvers_.reserve(10000);
  goto_next_lower_solvers_.push_back(NULL);
  goto_next_lower_solvers_.push_back(Options::instance().getSATSolver());
  goto_next_lower_solvers_.back()->startIncrementalSession(sicn_);
  goto_next_lower_solvers_.back()->incAddCNF(AIG2CNF::instance().getTrans());
  goto_next_lower_solvers_.back()->incAddCNF(AIG2CNF::instance().getNextUnsafeStates());

  gen_block_trans_solvers_.reserve(10000);
  gen_block_trans_solvers_.push_back(NULL);
  gen_block_trans_solvers_.push_back(Options::instance().getSATSolver());
  gen_block_trans_solvers_.back()->startIncrementalSession(sicn_);
  gen_block_trans_solvers_.back()->incAddCNF(AIG2CNF::instance().getTrans());
  gen_block_trans_solvers_.back()->incAddCNF(AIG2CNF::instance().getNextUnsafeStates());

  goto_undecided_solvers_.reserve(10000);
  goto_undecided_solvers_.push_back(NULL);
  goto_undecided_solvers_.push_back(NULL);

  goto_win_solver_ = Options::instance().getSATSolver();
  goto_win_solver_->startIncrementalSession(sicn_);
  goto_win_solver_->incAddCNF(AIG2CNF::instance().getTrans());
  goto_win_solver_->incAddCNF(AIG2CNF::instance().getNextSafeStates());
}

// -------------------------------------------------------------------------------------------
IFM13ReachSynth::~IFM13ReachSynth()
{
  delete goto_win_solver_;
  goto_win_solver_ = NULL;
}

// -------------------------------------------------------------------------------------------
bool IFM13ReachSynth::run()
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
  Utils::debugCheckWinReg(winning_region_, neg_winning_region_);
  if(Options::instance().doRealizabilityOnly())
    return true;

  L_INF("Starting to extract a circuit ...");
  QBFCertImplExtractor extractor;
  extractor.extractCircuit(winning_region_, neg_winning_region_);
  L_INF("Synthesis done.");
  return true;
}

// -------------------------------------------------------------------------------------------
bool IFM13ReachSynth::computeWinningRegion()
{
  const vector<int> &state_vars = VarManager::instance().getVarsOfType(VarInfo::PRES_STATE);
  vector<int> initial_state_cube(state_vars.size(), 0);
  for(size_t cnt = 0; cnt < state_vars.size(); ++cnt)
    initial_state_cube[cnt] = -state_vars[cnt];

  //SatSolver *solver = Options::instance().getSATSolver();

  size_t k = 1;
  while(true)
  {
    L_DBG("------ Iteration k=" << k);
    //debugPrintRs();
    //vector<int> problematic_state_in_cube;
    //CNF check(getR(k));
    //check.addCNF(AIG2CNF::instance().getInitial());
    //bool is_sat = solver->isSatModelOrCore(check, vector<int>(), si_, problematic_state_in_cube);
    //while(is_sat)
    //{
      if(analyze(initial_state_cube, k) == IS_LOSE)
        return false;
    cout << "after analyze" << endl;
    //  check = getR(k);
    //  check.addCNF(AIG2CNF::instance().getInitial());
    //  is_sat = solver->isSatModelOrCore(check, vector<int>(), si_, problematic_state_in_cube);
    //}
    //debugPrintRs();
    //exit(0);

    //debugCheckInvariants(k);
    size_t equal = propagateBlockedStates(k);
    //debugCheckInvariants(k);
    if(equal != 0)
    {
      L_LOG("Found two equal clause sets: R" << (equal-1) << " and R" << equal);
      L_LOG("Nr of iterations: " << k);
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
bool IFM13ReachSynth::analyze(const vector<int> &state_cube, size_t level)
{

  //const vector<int> &ctrl = VarManager::instance().getVarsOfType(VarInfo::CTRL);

  //MASSERT(getR(level).isSatBy(state_cube), "Impossible");
  //MASSERT(!getR(level - 1).isSatBy(state_cube), "Impossible");

  //cout << "called analyze with level " << level << endl;
  vector<int> model_or_core;

  bool go_aggressive = false;
  while(true)
  {
    do
    {
      bool something_refined = false;
      if(!getR(level).isSatBy(state_cube))
      {
        return IS_GREATER;
      }
      if(!win_.isSatBy(state_cube))
      {
        return IS_LOSE;
      }

      vector<vector<int> > trace_to_bad;
      //cout << "before calling analyze reach" << endl;
      bool trace_found = analyzeReach(state_cube, level, trace_to_bad);
      //cout << "after calling analyze reach" << endl;
      if(!trace_found)
      {
        MASSERT(!getR(level).isSatBy(state_cube), "analyzeReach does not work.");
        //cout << "no trace found" << endl;
        return IS_GREATER;
      }
      //MASSERT(trace_to_bad.size() == level + 1, "analyzeReach does not work.");
      MASSERT(Utils::extract(trace_to_bad.back(), VarInfo::PRES_STATE) == state_cube, "analyzeReach does not work.");
      //MASSERT(Utils::contains(trace_to_bad.front(), VarManager::instance().getPresErrorStateVar()), "analyzeReach does not work.");

      for(size_t dist_to_err = trace_to_bad.size() - 1; dist_to_err > 0; --dist_to_err)
      //for(size_t dist_to_err = 1; dist_to_err < trace_to_bad.size(); ++dist_to_err)
      //size_t dist_to_err = trace_to_bad.size() - 1;
      //size_t dist_to_err = level;
      {
        vector<int> &trace_step = trace_to_bad[dist_to_err];
        //vector<int> &trace_step = trace_to_bad.back();
        vector<int> trace_state_cube = Utils::extract(trace_step, VarInfo::PRES_STATE);
        vector<int> trace_in_cube = Utils::extract(trace_step, VarInfo::INPUT);
        MASSERT(getR(level).isSatBy(trace_state_cube), "analyzeReach does not work.");

        if(win_.isSatBy(trace_state_cube))
        {
          bool can_goto_win = goto_win_solver_->incIsSatModelOrCore(trace_state_cube,
                                                                    trace_in_cube,
                                                                    cn_,
                                                                    model_or_core);
          if(can_goto_win)
          {
            //cout << "can goto win" << endl;
            //cout << "dist to error: " << dist_to_err << endl;
            vector<int> succ = Utils::extractNextAsPresent(model_or_core);
            if(dist_to_err == 1 || !getR(dist_to_err - 1).isSatBy(succ))
            {
              //cout << "successor is outside Ri-1" << endl;
              vector<int> ctrl_cube = Utils::extract(model_or_core, VarInfo::CTRL);
              genAndBlockTrans(trace_step, ctrl_cube, dist_to_err);
              something_refined = true;
              //break;
            }
            else
            {
              //cout << "successor is in Ri-1" << endl;
              if(go_aggressive)
              {
                //cout << "doing aggressive" << endl;
                analyze(succ, dist_to_err - 1); // ??
                if(!win_.isSatBy(state_cube))
                  return IS_LOSE;
                if(!getR(level).isSatBy(state_cube))
                  return IS_GREATER;
                something_refined = true;
                go_aggressive = false;
                //break;
              }
            }
          }
          else
          {
            //cout << "cannot goto win" << endl;
            addLose(model_or_core);
            if(Utils::isSubset(model_or_core, state_cube))
              return IS_LOSE;
          }
        }
      }
      if(!something_refined)
        go_aggressive = true;
    } while(true);

    MASSERT(false, "Doh.");


    if(level == 1)
      return IS_LOSE;
    SatSolver *s = getGotoUndecidedSolver(level);
    bool can_goto_undec = s->incIsSatModelOrCore(state_cube, sicn_, model_or_core);
    if(!can_goto_undec)
      return IS_LOSE;
    vector<int> succ = Utils::extractNextAsPresent(model_or_core);
    bool rec_res = analyze(succ, level - 1);
    if(rec_res == IS_GREATER)
    {
      // TODO: generalize SI-pair and add to U
    }
  }

  return false;
}

// -------------------------------------------------------------------------------------------
size_t IFM13ReachSynth::propagateBlockedStates(size_t max_level)
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
      if(it2 != r2clauses.end() && *it1 == *it2)
      {
        ++it1;
        ++it2;
        continue;
      }
      vector<int> neg_clause(*it1);
      for(size_t lit_cnt = 0; lit_cnt < neg_clause.size(); ++lit_cnt)
        neg_clause[lit_cnt] = -neg_clause[lit_cnt];
      if(!getGotoNextLowerSolver(i+1)->incIsSat(neg_clause))
      {
        // There is no edge from s (the negated clause) to Ri
        // --> no state in s can be part of Ri+1
        it2 = r2clauses.insert(it2,*it1);
        vector<int> propagated(*it1);
        Utils::swapPresentToNext(propagated);
        getGotoNextLowerSolver(i+2)->incAddClause(propagated);
        getGenBlockTransSolver(i+2)->incAddClause(propagated);
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
bool IFM13ReachSynth::analyzeReach(const vector<int> &state_cube,
                                   size_t level,
                                   vector<vector<int> > &trace_to_bad)
{
  //cout << "analyzeReach called at level " << level << endl;
  MASSERT(getR(level).isSatBy(state_cube), "Impossible");
  //MASSERT(!getR(level - 1).isSatBy(state_cube), "Impossible");
  while(true)
  {
    if(indRel(state_cube, level))
    {
      return false;
    }
    if(level > 1 && indRel(state_cube, level - 1))
    {
      return false;
    }

    //cout << "checking for transition to next lower" << endl;
    // make a transition from state_cube to R[level-1]\R[level-2]
    //CNF gotoNextLower;
    //if(level >= 2)
    //{
    //  gotoNextLower.addCNF(getR(level - 2));
    //  gotoNextLower.negate();
    //}
    //gotoNextLower.addCNF(getR(level-1));
    //gotoNextLower.swapPresentToNext();
    //gotoNextLower.addCNF(getU(level));
    //gotoNextLower.addCNF(AIG2CNF::instance().getTrans());
    SatSolver *s = getGotoNextLowerSolver(level);
    vector<int> model_or_core;
    bool sat = s->incIsSatModelOrCore(state_cube, sin_, model_or_core);
    if(!sat)
    {
      //cout << "none found" << endl;
      addBlockedState(model_or_core, level);
      return false;
    }
    vector<int> succ = Utils::extractNextAsPresent(model_or_core);
    vector<int> si = Utils::extractPresIn(model_or_core);
    if(level == 1)
    {
      trace_to_bad.clear();
      trace_to_bad.push_back(succ);
      trace_to_bad.push_back(si);
      return true;
    }
    bool reach = analyzeReach(succ, level - 1, trace_to_bad);
    if(reach)
    {
      trace_to_bad.push_back(si);
      return true;
    }
  }
  return false;
}

// -------------------------------------------------------------------------------------------
bool IFM13ReachSynth::analyzeReachPoor(const vector<int> &state_cube,
                                       size_t level,
                                       vector<vector<int> > &trace_to_bad)
{
  //cout << "analyzeReach called at level " << level << endl;
  MASSERT(getR(level).isSatBy(state_cube), "Impossible");
  //MASSERT(!getR(level - 1).isSatBy(state_cube), "Impossible");
  if(indRel(state_cube, level))
  {
    return false;
  }
  if(level > 1 && indRel(state_cube, level - 1))
  {
    return false;
  }

  SatSolver *s = getGotoNextLowerSolver(level);
  vector<int> model_or_core;
  bool sat = s->incIsSatModelOrCore(state_cube, sin_, model_or_core);
  if(!sat)
  {
    //cout << "none found" << endl;
    addBlockedState(model_or_core, level);
    return false;
  }
  vector<int> succ = Utils::extractNextAsPresent(model_or_core);
  vector<int> si = Utils::extractPresIn(model_or_core);
  trace_to_bad.clear();
  trace_to_bad.push_back(succ);
  trace_to_bad.push_back(si);
  return true;

}

// -------------------------------------------------------------------------------------------
bool IFM13ReachSynth::indRel(const vector<int> &state_cube, size_t level)
{

  //cout << "indRel called with level: " << level << endl;
  int err = VarManager::instance().getPresErrorStateVar();

  // check initiation, i.e., if state_cube intersects with the bad states:
  if(!Utils::contains(state_cube, -err))
    return false;

  SatSolver *s = getGenBlockTransSolver(level + 1);
  s->incPush();
  s->incAddCube(state_cube);
  vector<int> next_state_cube(state_cube);
  Utils::swapPresentToNext(next_state_cube);
  s->incAddNegCubeAsClause(next_state_cube);
  bool sat = s->incIsSat();
  s->incPop();
  if(sat)
    return false;

  // generalize:
  // TODO: should we work with a very precise transition relation???
  vector<int> smallest_so_far(state_cube);
  for(size_t cnt = 0; cnt < state_cube.size(); ++cnt)
  {
    // ensure 'initiation', i.e., tmp must not intersects with the bad states:
    if(state_cube[cnt] == -err)
      continue;
    vector<int> tmp(smallest_so_far);
    Utils::remove(tmp, state_cube[cnt]);
    s->incPush();
    s->incAddCube(tmp);
    vector<int> next_tmp_cube(tmp);
    Utils::swapPresentToNext(next_tmp_cube);
    s->incAddNegCubeAsClause(next_tmp_cube);
    bool sat = s->incIsSat();
    s->incPop();
    if(!sat)
      smallest_so_far = tmp;
  }
  addBlockedState(smallest_so_far, level + 1);
  return true;
}

// -------------------------------------------------------------------------------------------
void IFM13ReachSynth::addBlockedTransition(const vector<int> &state_in_cube, size_t level)
{
  vector<int> blocking_clause(state_in_cube);
  for(size_t cnt = 0; cnt < blocking_clause.size(); ++cnt)
    blocking_clause[cnt] = -blocking_clause[cnt];
  for(size_t l_cnt = 1; l_cnt <= level; ++l_cnt)
  {
    getU(l_cnt).addClauseAndSimplify(blocking_clause);
    getGotoNextLowerSolver(l_cnt)->incAddClause(blocking_clause);
    if(l_cnt >= 2)
      getGotoUndecidedSolver(l_cnt)->incAddClause(blocking_clause);
  }
}

// -------------------------------------------------------------------------------------------
void IFM13ReachSynth::addBlockedState(const vector<int> &cube, size_t level)
{
  // We require that the cube does not intersect with R0, i.e., the bad states.
  // The intuitive reason is that the bad states do not have to have a successor (in
  // R[level-1]) order to be problematic.
  // Often there are bad states, which do not have a successor in the bad states.
  // Before minimization with unsat cores, the blocked state is actually completely outside
  // of the bad states. Now, instead of coming up with a complex generalization procedure
  // which ensures that the cube does not intersect with the bad states, we can simply add
  // this property later, because the bad states are characterized by one state bit in our
  // setting. This saves time.
  int err = VarManager::instance().getPresErrorStateVar();
  vector<int> blocking_clause;
  blocking_clause.reserve(cube.size() + 1);
  for(size_t cnt = 0; cnt < cube.size(); ++cnt)
    blocking_clause.push_back(-cube[cnt]);
  if(!Utils::contains(blocking_clause, err))
    blocking_clause.push_back(err);
  vector<int> next_blocking_clause(blocking_clause);
  Utils::swapPresentToNext(next_blocking_clause);
  for(size_t l_cnt = 0; l_cnt <= level; ++l_cnt)
  {
    getR(l_cnt).addClause(blocking_clause);
    getGotoNextLowerSolver(l_cnt+1)->incAddClause(next_blocking_clause);
    getGenBlockTransSolver(l_cnt+1)->incAddClause(next_blocking_clause);
    if(l_cnt+1 >= 2)
      getGotoUndecidedSolver(l_cnt+1)->incAddClause(next_blocking_clause);
  }
}

// -------------------------------------------------------------------------------------------
void IFM13ReachSynth::addLose(const vector<int> &cube)
{
  vector<int> blocking_clause(cube);
  for(size_t cnt = 0; cnt < blocking_clause.size(); ++cnt)
    blocking_clause[cnt] = -blocking_clause[cnt];
  win_.addClauseAndSimplify(blocking_clause);
  Utils::swapPresentToNext(blocking_clause);
  goto_win_solver_->incAddClause(blocking_clause);
  for(size_t cnt = 2; cnt < goto_undecided_solvers_.size(); ++cnt)
    goto_undecided_solvers_[cnt]->incAddClause(blocking_clause);
}

// -------------------------------------------------------------------------------------------
void IFM13ReachSynth::genAndBlockTrans(const vector<int> &state_in_cube,
                                       const vector<int> &ctrl_cube,
                                       size_t level)
{
  SatSolver *gen_solver = getGenBlockTransSolver(level);
  vector<int> model_or_core;
  bool isSat = gen_solver->incIsSatModelOrCore(state_in_cube, ctrl_cube,
                                               state_in_cube, model_or_core);
  MASSERT(isSat == false, "Impossible.");
  addBlockedTransition(model_or_core, level);
}


// -------------------------------------------------------------------------------------------
CNF& IFM13ReachSynth::getR(size_t index)
{
  for(size_t i = r_.size(); i <= index; ++i)
    r_.push_back(CNF());
  return r_[index];
}

// -------------------------------------------------------------------------------------------
CNF& IFM13ReachSynth::getU(size_t index)
{
  for(size_t i = u_.size(); i <= index; ++i)
    u_.push_back(CNF());
  return u_[index];
}

// -------------------------------------------------------------------------------------------
SatSolver* IFM13ReachSynth::getGotoNextLowerSolver(size_t index)
{
  for(size_t i = goto_next_lower_solvers_.size(); i <= index; ++i)
  {
    goto_next_lower_solvers_.push_back(Options::instance().getSATSolver());
    goto_next_lower_solvers_.back()->startIncrementalSession(sicn_);
    goto_next_lower_solvers_.back()->incAddCNF(AIG2CNF::instance().getTrans());
  }
  return goto_next_lower_solvers_[index];
}

// -------------------------------------------------------------------------------------------
SatSolver* IFM13ReachSynth::getGenBlockTransSolver(size_t index)
{
  for(size_t i = gen_block_trans_solvers_.size(); i <= index; ++i)
  {
    gen_block_trans_solvers_.push_back(Options::instance().getSATSolver());
    gen_block_trans_solvers_.back()->startIncrementalSession(sicn_);
    gen_block_trans_solvers_.back()->incAddCNF(AIG2CNF::instance().getTrans());
  }
  return gen_block_trans_solvers_[index];
}

// -------------------------------------------------------------------------------------------
SatSolver* IFM13ReachSynth::getGotoUndecidedSolver(size_t index)
{
  for(size_t i = goto_undecided_solvers_.size(); i <= index; ++i)
  {
    goto_undecided_solvers_.push_back(Options::instance().getSATSolver());
    goto_undecided_solvers_.back()->startIncrementalSession(sicn_);
    goto_undecided_solvers_.back()->incAddCNF(AIG2CNF::instance().getTrans());
    goto_undecided_solvers_.back()->incAddCNF(win_);
  }
  return goto_undecided_solvers_[index];
}

// -------------------------------------------------------------------------------------------
void IFM13ReachSynth::debugPrintRs() const
{
  for(size_t cnt = 0; cnt < r_.size(); ++cnt)
  {
    L_DBG("R" << cnt << ":");
    L_DBG(r_[cnt].toString());
  }
}

// -------------------------------------------------------------------------------------------
void IFM13ReachSynth::debugCheckInvariants(size_t k)
{

  SatSolver *sat_solver = Options::instance().getSATSolver();

  // check if W implies P:
  CNF check(AIG2CNF::instance().getUnsafeStates());
  check.addCNF(win_);
  bool v = sat_solver->isSat(check);
  MASSERT(!v, "W does not imply P");

  // check if R0 is still !P:
  check.clear();
  check.addCNF(getR(0));
  check.negate();
  check.addCNF(AIG2CNF::instance().getUnsafeStates());
  v = sat_solver->isSat(check);
  MASSERT(!v, "R0 is unequal to !P");
  check.clear();
  check.addCNF(AIG2CNF::instance().getUnsafeStates());
  check.negate();
  check.addCNF(getR(0));
  v = sat_solver->isSat(check);
  MASSERT(!v, "R0 is unequal to !P");

  // check if R_i implies Ri+1:
  for(size_t i = 0; i < k; ++i)
  {
    CNF check(getR(i+1));
    check.negate();
    check.addCNF(getR(i));
    bool v = sat_solver->isSat(check);
    MASSERT(!v, "R" << i << " does not imply R" << i+1);
  }

  // check if all R_i imply not s_0:
  for(size_t i = 0; i <= k; ++i)
  {
    CNF check(getR(i));
    check.addCNF(AIG2CNF::instance().getInitial());
    bool v = sat_solver->isSat(check);
    MASSERT(!v, "R" << i << " does not imply !s0" << i+1);
  }

  // check if U_i implies Ui+1:
  for(size_t i = 1; i < k; ++i)
  {
    CNF check(getU(i+1));
    check.negate();
    check.addCNF(getU(i));
    bool v = sat_solver->isSat(check);
    MASSERT(!v, "U" << i << " does not imply U" << i+1);
  }


  // check if Force^e_1(R_i) => Ri+1
  // i.e., if all states from which then environment can enforce R_i are contained in Ri+1:
  // \exists x,i: \forall c: \exists x',tmp: !Ri+1 & T & ri'
  DepQBFApi qbf_solver;
  vector<pair<VarInfo::VarKind, QBFSolver::Quant> > env_quant;
  env_quant.push_back(make_pair(VarInfo::PRES_STATE, QBFSolver::E));
  env_quant.push_back(make_pair(VarInfo::INPUT, QBFSolver::E));
  env_quant.push_back(make_pair(VarInfo::CTRL, QBFSolver::A));
  env_quant.push_back(make_pair(VarInfo::NEXT_STATE, QBFSolver::E));
  env_quant.push_back(make_pair(VarInfo::TMP, QBFSolver::E));

  for(size_t i = 0; i < k; ++i)
  {
    CNF check(getR(i));
    check.swapPresentToNext();
    check.addCNF(AIG2CNF::instance().getTrans());
    CNF neg_r_next(getR(i+1));
    neg_r_next.negate();
    check.addCNF(neg_r_next);
    bool v = qbf_solver.isSat(env_quant, check);
    MASSERT(!v, "R" << i+1  << " misses a state from which the ENV can enforce R" << i);
  }

  // U_i+1 must not exclude a state-input pair for which the environment can enforce
  // to go to R_i
  for(size_t i = 0; i < k; ++i)
  {
    CNF check(getR(i));
    check.swapPresentToNext();
    check.addCNF(AIG2CNF::instance().getTrans());
    //check.addCNF(getR(i+1));
    CNF neg_u_next(getU(i+1));
    neg_u_next.negate();
    check.addCNF(neg_u_next);
    bool v = qbf_solver.isSat(env_quant, check);
    MASSERT(!v, "U" << i+1  << " excludes an input for which the ENV can enforce R" << i);
  }

  delete sat_solver;
}




