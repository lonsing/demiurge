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
/// @file LearnSynthQBFInd.cpp
/// @brief Contains the definition of the class LearnSynthQBFInd.
// -------------------------------------------------------------------------------------------

#include "LearnSynthQBFInd.h"
#include "VarManager.h"
#include "Options.h"
#include "AIG2CNF.h"
#include "Logger.h"
#include "QBFCertImplExtractor.h"
#include "Utils.h"
#include "LingelingApi.h"

// -------------------------------------------------------------------------------------------
LearnSynthQBFInd::LearnSynthQBFInd() :
                  BackEnd(),
                  qbf_solver_(Options::instance().getQBFSolver()),
                  current_state_is_initial_(0),
                  do_reach_check_(false)
{

  if(Options::instance().getBackEndMode() == 3 || Options::instance().getBackEndMode() == 6)
    do_reach_check_ = true;

  // build the quantifier prefix for checking for counterexamples:
  //   check_quant_ = exists x,i: forall c: exists x',tmp:
  if(do_reach_check_)
  {
    // if do_reach_check_ = true, the quantifier prefix for checking for counterexamples is:
    //   check_quant_ = exists x*,i*,c*,x,i: forall c: exists x',tmp:
    // x*,i*,c* are variables of type VarInfo::PREV
    check_quant_.push_back(make_pair(VarInfo::PREV, QBFSolver::E));
  }
  check_quant_.push_back(make_pair(VarInfo::PRES_STATE, QBFSolver::E));
  check_quant_.push_back(make_pair(VarInfo::INPUT, QBFSolver::E));
  check_quant_.push_back(make_pair(VarInfo::CTRL, QBFSolver::A));
  check_quant_.push_back(make_pair(VarInfo::NEXT_STATE, QBFSolver::E));
  check_quant_.push_back(make_pair(VarInfo::TMP, QBFSolver::E));

  // build the quantifier prefix for generalizing cubes:
  //    gen_quant_ = exists x*,i*,c*: exists x: forall i: exists c,x',tmp:
  gen_quant_.push_back(make_pair(VarInfo::PREV, QBFSolver::E));
  gen_quant_.push_back(make_pair(VarInfo::PRES_STATE, QBFSolver::E));
  gen_quant_.push_back(make_pair(VarInfo::INPUT, QBFSolver::A));
  gen_quant_.push_back(make_pair(VarInfo::CTRL, QBFSolver::E));
  gen_quant_.push_back(make_pair(VarInfo::NEXT_STATE, QBFSolver::E));
  gen_quant_.push_back(make_pair(VarInfo::TMP, QBFSolver::E));

  // build previous-state copy of the transition relation. We want to have it enabled
  // if the current state is initial. The result is stored in prev_trans_or_initial_.
  AIG2CNF& A2C = AIG2CNF::instance();
  VarManager &VM = VarManager::instance();
  current_to_previous_map_.resize(VM.getMaxCNFVar()+1, 0);
  const vector<int> &s = VM.getVarsOfType(VarInfo::PRES_STATE);
  const vector<int> &s_next = VM.getVarsOfType(VarInfo::NEXT_STATE);
  const vector<int> &c = VM.getVarsOfType(VarInfo::CTRL);
  const vector<int> &i = VM.getVarsOfType(VarInfo::INPUT);
  vector<int> t = VM.getVarsOfType(VarInfo::TMP);

  for(size_t v_cnt = 0; v_cnt < s.size(); ++v_cnt)
    current_to_previous_map_[s[v_cnt]] = VM.createFreshPrevVar();
  for(size_t v_cnt = 0; v_cnt < c.size(); ++v_cnt)
    current_to_previous_map_[c[v_cnt]] = VM.createFreshPrevVar();
  for(size_t v_cnt = 0; v_cnt < i.size(); ++v_cnt)
    current_to_previous_map_[i[v_cnt]] = VM.createFreshPrevVar();
  for(size_t v_cnt = 0; v_cnt < t.size(); ++v_cnt)
    current_to_previous_map_[t[v_cnt]] = VM.createFreshTmpVar();
  for(size_t v_cnt = 0; v_cnt < s_next.size(); ++v_cnt)
    current_to_previous_map_[s_next[v_cnt]] = s[v_cnt];

  current_state_is_initial_ = VM.createFreshTmpVar();
  list<vector<int> > prev_trans_clauses = A2C.getTrans().getClauses();
  for(CNF::ClauseIter it = prev_trans_clauses.begin(); it != prev_trans_clauses.end(); ++it)
  {
    presentToPrevious(*it);
    it->push_back(current_state_is_initial_);
  }
  prev_trans_or_initial_.swapWith(prev_trans_clauses);
  // if one of the state variables is true, then current_state_is_initial must be false:
  for(size_t cnt = 0; cnt < s.size(); ++cnt)
    prev_trans_or_initial_.add2LitClause(-s[cnt], -current_state_is_initial_);

  // check_reach_ contains
  // - the previous transition relation (enabled only if the current state is not initial)
  // - constraints that say that the previous state is different from the current state
  // check_reach_ can also be set to TRUE if we do not want to include reachability in the
  // computation of bad states
  if(do_reach_check_)
  {
    check_reach_.addCNF(prev_trans_or_initial_);
    vector<int> one_is_diff;
    one_is_diff.reserve(s.size() + 1);
    one_is_diff.push_back(current_state_is_initial_);
    for(size_t cnt = 0; cnt < s.size(); ++cnt)
    {
      int diff = VM.createFreshTmpVar();
      one_is_diff.push_back(diff);
      int curr = s[cnt];
      int prev = presentToPrevious(curr);
      // actually, we only need one direction of the implication:
      check_reach_.add3LitClause(curr, prev, -diff);
      check_reach_.add3LitClause(-curr, -prev, -diff);
      //check_reach_.add3LitClause(-curr, prev, diff);
      //check_reach_.add3LitClause(curr, -prev, diff);
    }
    check_reach_.addClause(one_is_diff);
  }
}

// -------------------------------------------------------------------------------------------
LearnSynthQBFInd::~LearnSynthQBFInd()
{
  delete qbf_solver_;
  qbf_solver_ = NULL;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBFInd::run()
{
  VarManager::instance().push();
  L_INF("Starting to compute a winning region ...");
  statistics_.notifyWinRegStart();
  bool realizable = computeWinningRegion();
  statistics_.notifyWinRegEnd();
  if(!realizable)
  {
    L_RES("The specification is unrealizable.");
    statistics_.logStatistics();
    return false;
  }
  L_RES("The specification is realizable.");
  if(Options::instance().doRealizabilityOnly())
  {
     statistics_.logStatistics();
     return true;
  }

  MASSERT(!do_reach_check_, "Not yet implemented.");

  L_INF("Starting to extract a circuit ...");
  statistics_.notifyRelDetStart();
  QBFCertImplExtractor extractor;
  extractor.extractCircuit(winning_region_);
  statistics_.notifyRelDetEnd();
  L_INF("Synthesis done.");
  statistics_.logStatistics();
  return true;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBFInd::computeWinningRegion()
{
  bool real = false;
  int m = Options::instance().getBackEndMode();
  if(m == 2 || m == 3)
    real = computeWinningRegionOne();
  else // m == 5 || m == 6
    real =  computeWinningRegionAll();
  if(real)
  {
    if(do_reach_check_)
      debugCheckWinRegReach();
    else
      Utils::debugCheckWinReg(winning_region_);
  }
  return real;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBFInd::computeWinningRegionOne()
{
  AIG2CNF& A2C = AIG2CNF::instance();
  VarManager &VM = VarManager::instance();
  L_DBG("Nr. of state variables: " << VM.getVarsOfType(VarInfo::PRES_STATE).size());
  L_DBG("Nr. of uncontrollable inputs: " << VM.getVarsOfType(VarInfo::INPUT).size());
  L_DBG("Nr. of controllable inputs: " << VM.getVarsOfType(VarInfo::CTRL).size());

  winning_region_.clear();
  winning_region_ = A2C.getSafeStates();
  winning_region_large_.clear();
  winning_region_large_ = A2C.getSafeStates();

  // build check_cnf = P(x) AND T(x,i,c,x') AND NOT P(x')
  recomputeCheckCNF();

  // build generalize_clause_cnf = P(x*) AND (I(x) OR T(x*,i*,c*,x*)) AND P(x) AND T(x,i,c,x') AND P(x')
  recomputeGenCNF();

  vector<int> counterexample;
  vector<int> blocking_clause;
  blocking_clause.reserve(VM.getVarsOfType(VarInfo::PRES_STATE).size());
  bool ce_found = computeCounterexampleQBF(counterexample);

  size_t count = 0;
  while(ce_found)
  {
    count++;
    L_DBG("Found bad state (nr " << count << "), excluding it from the winning region.");
    bool realizable = computeBlockingClause(counterexample, blocking_clause);
    if(!realizable)
      return false;

    // update the CNFs:
    Utils::debugPrint(blocking_clause, "Blocking clause: ");
    winning_region_.addClauseAndSimplify(blocking_clause);
    winning_region_large_.addClauseAndSimplify(blocking_clause);
    vector<int> prev_blocking_clause(blocking_clause);
    presentToPrevious(prev_blocking_clause);
    generalize_clause_cnf_.addClauseAndSimplify(prev_blocking_clause);
    generalize_clause_cnf_.addClauseAndSimplify(blocking_clause);
    Utils::swapPresentToNext(blocking_clause);
    generalize_clause_cnf_.addClauseAndSimplify(blocking_clause);
    //if(count % 100 == 0)
    //{
    //  reduceExistingClauses();
    //  recomputeGenCNF(false);
    //}

    Utils::compressStateCNF(winning_region_);
    recomputeCheckCNF(false);

    if(count % 100 == 0)
    {
      L_DBG("Statistics at count=" << count);
      L_DBG("Winning region clauses: " << winning_region_.getNrOfClauses());
      L_DBG("Winning region lits: " << winning_region_.getNrOfLits());
      L_DBG("Winning region full clauses: " << winning_region_large_.getNrOfClauses());
      L_DBG("Winning region full lits: " << winning_region_large_.getNrOfLits());
      L_DBG("Check CNF clauses: " << check_cnf_.getNrOfClauses());
      L_DBG("Check CNF lits: " << check_cnf_.getNrOfLits());
      L_DBG("Generalize CNF clauses: " << generalize_clause_cnf_.getNrOfClauses());
      L_DBG("Generalize CNF lits: " << generalize_clause_cnf_.getNrOfLits());
      if(Logger::instance().isEnabled(Logger::DBG))
        statistics_.logStatistics();
    }
    ce_found = computeCounterexampleQBF(counterexample);
  }
  return true;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBFInd::computeWinningRegionAll()
{
  AIG2CNF& A2C = AIG2CNF::instance();
  VarManager &VM = VarManager::instance();
  L_DBG("Nr. of state variables: " << VM.getVarsOfType(VarInfo::PRES_STATE).size());
  L_DBG("Nr. of uncontrollable inputs: " << VM.getVarsOfType(VarInfo::INPUT).size());
  L_DBG("Nr. of controllable inputs: " << VM.getVarsOfType(VarInfo::CTRL).size());

  winning_region_.clear();
  winning_region_ = A2C.getSafeStates();
  winning_region_large_.clear();
  winning_region_large_ = A2C.getSafeStates();

  // build check_cnf = P(x) AND T(x,i,c,x') AND NOT P(x')
  recomputeCheckCNF();

  // build generalize_clause_cnf = P(x*) AND (I(x) OR T(x*,i*,c*,x*)) AND P(x) AND T(x,i,c,x') AND P(x')
  recomputeGenCNF();

  vector<int> counterexample;
  vector<vector<int> > all_blocking_clauses;
  all_blocking_clauses.reserve(1000);
  bool ce_found = computeCounterexampleQBF(counterexample);
  size_t count = 0;
  while(ce_found)
  {
    count++;
    L_DBG("Found bad state (nr " << count << "), excluding it from the winning region.");

    bool realizable = computeAllBlockingClauses(counterexample, all_blocking_clauses);
    if(!realizable)
      return false;

    for(size_t clause_count = 0; clause_count < all_blocking_clauses.size(); ++clause_count)
    {
      vector<int> &blocking_clause = all_blocking_clauses[clause_count];
      // update the CNFs:
      Utils::debugPrint(blocking_clause, "Blocking clause: ");
      winning_region_.addClauseAndSimplify(blocking_clause);
      winning_region_large_.addClauseAndSimplify(blocking_clause);
    }
    if(count % 100 == 0)
      reduceExistingClauses();
    Utils::compressStateCNF(winning_region_);
    recomputeCheckCNF(true);
    recomputeGenCNF(true);
    if(count % 100 == 0)
    {
      L_DBG("Statistics at count=" << count);
      L_DBG("Winning region clauses: " << winning_region_.getNrOfClauses());
      L_DBG("Winning region lits: " << winning_region_.getNrOfLits());
      L_DBG("Winning region full clauses: " << winning_region_large_.getNrOfClauses());
      L_DBG("Winning region full lits: " << winning_region_large_.getNrOfLits());
      L_DBG("Check CNF clauses: " << check_cnf_.getNrOfClauses());
      L_DBG("Check CNF lits: " << check_cnf_.getNrOfLits());
      L_DBG("Generalize CNF clauses: " << generalize_clause_cnf_.getNrOfClauses());
      L_DBG("Generalize CNF lits: " << generalize_clause_cnf_.getNrOfLits());
      if(Logger::instance().isEnabled(Logger::DBG))
        statistics_.logStatistics();
    }
    ce_found = computeCounterexampleQBF(counterexample);
  }
  return true;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBFInd::computeCounterexampleQBF(vector<int> &ce)
{
  ce.clear();
  statistics_.notifyBeforeComputeCube();
  bool found = qbf_solver_->isSatModel(check_quant_, check_cnf_, ce);
  restrictToStates(ce);
  statistics_.notifyAfterComputeCube();
  return found;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBFInd::computeBlockingClause(vector<int> &ce, vector<int> &blocking_clause)
{
  statistics_.notifyBeforeCubeMin();
  Utils::randomize(ce);
  if(Utils::containsInit(ce))
    return false;
  size_t orig_size = ce.size();
  generalizeCounterexample(ce, false);
  if(Utils::containsInit(ce))
    return false;
  blocking_clause = ce;
  Utils::negateLiterals(blocking_clause);
  L_DBG("computeBlockingClause(): " << orig_size << " --> " << blocking_clause.size());
  statistics_.notifyAfterCubeMin(orig_size, blocking_clause.size());
  return true;
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBFInd::computeAllBlockingClauses(vector<int> &ce,
                                                 vector<vector<int> > &blocking_clauses)
{
  statistics_.notifyBeforeCubeMin();
  list<set<int> > must_not_contain_queue;
  list<vector<int> > gen_ces;
  vector<int> orig_ce(ce);
  vector<int> first_ce(orig_ce);
  generalizeCounterexample(first_ce, false);
  statistics_.notifyCubeMin(orig_ce.size(), first_ce.size());
  if(Utils::containsInit(first_ce))
    return false;
  blocking_clauses.clear();
  vector<int> blocking_clause(first_ce);
  Utils::negateLiterals(blocking_clause);
  blocking_clauses.push_back(blocking_clause);

  gen_ces.push_back(first_ce);
  for(size_t cnt = 0; cnt < first_ce.size(); ++cnt)
  {
    set<int> singleton_set;
    singleton_set.insert(first_ce[cnt]);
    must_not_contain_queue.push_back(singleton_set);
  }

  // we add our blocking_clause to generalize_clause_cnf_ to benefit from it immediately:
  vector<int> prev_blocking_clause(blocking_clause);
  presentToPrevious(prev_blocking_clause);
  generalize_clause_cnf_.addClause(prev_blocking_clause);
  generalize_clause_cnf_.addClause(blocking_clause);
  Utils::swapPresentToNext(blocking_clause);
  generalize_clause_cnf_.addClause(blocking_clause);
  while(!must_not_contain_queue.empty())
  {
    set<int> must_not_contain = must_not_contain_queue.front();
    must_not_contain_queue.pop_front();

    // check if we already have a ce in gen_ces which satisfies our needs:
    bool found = false;
    for(list<vector<int> >::const_iterator it = gen_ces.begin(); it != gen_ces.end(); ++it)
    {
      if(Utils::intersectionEmpty(*it, must_not_contain))
      {
        found = true;
        break;
      }
    }
    if(found)
      continue;

    // if not: we compute one
    vector<int> new_ce(orig_ce);
    set<int>::const_iterator it = must_not_contain.begin();
    for( ; it != must_not_contain.end(); ++it)
      Utils::remove(new_ce, *it);
    size_t orig_size = new_ce.size();
    bool is_ok = generalizeCounterexample(new_ce);
    statistics_.notifyCubeMin(orig_size, new_ce.size());
    if(is_ok)
    {
      if(Utils::containsInit(new_ce))
        return false;
      gen_ces.push_back(new_ce);
      blocking_clause = new_ce;
      Utils::negateLiterals(blocking_clause);
      blocking_clauses.push_back(blocking_clause);
      for(size_t cnt = 0; cnt < new_ce.size(); ++cnt)
      {
        set<int> new_must_not_contain(must_not_contain);
        new_must_not_contain.insert(new_ce[cnt]);
        must_not_contain_queue.push_back(new_must_not_contain);
      }
      // we add our blocking_clause to generalize_clause_cnf_ to benefit from it immediately:
      vector<int> prev_blocking_clause(blocking_clause);
      presentToPrevious(prev_blocking_clause);
      generalize_clause_cnf_.addClause(prev_blocking_clause);
      generalize_clause_cnf_.addClause(blocking_clause);
      Utils::swapPresentToNext(blocking_clause);
      generalize_clause_cnf_.addClause(blocking_clause);
    }
  }

  statistics_.notifyAfterCubeMin();

  return true;
}

// -------------------------------------------------------------------------------------------
void LearnSynthQBFInd::reduceExistingClauses()
{
  statistics_.notifyBeforeCubeMin();

  size_t or_cl = winning_region_.getNrOfClauses();
  size_t or_lits = winning_region_.getNrOfLits();
  const list<vector<int> > orig_clauses =  winning_region_.getClauses();
  for(CNF::ClauseConstIter it = orig_clauses.begin(); it != orig_clauses.end(); ++it)
  {
    vector<int> clause(*it);
    for(size_t cnt = 0; cnt < it->size(); ++cnt)
    {
      vector<int> tmp(clause);
      Utils::remove(tmp, (*it)[cnt]);
      CNF tmp_cnf(generalize_clause_cnf_);
      tmp_cnf.addNegClauseAsCube(tmp);
      vector<int> prev_tmp;
      for(size_t i = 0; i < tmp.size(); ++i)
        prev_tmp.push_back(presentToPrevious(tmp[i]));
      prev_tmp.push_back(-current_state_is_initial_);
      tmp_cnf.addClause(prev_tmp);
      if(!qbf_solver_->isSat(gen_quant_, tmp_cnf))
        clause = tmp;
    }
    if(it->size() > clause.size())
    {
      winning_region_.addClauseAndSimplify(clause);
      winning_region_large_.addClauseAndSimplify(clause);
    }
  }
  size_t new_cl = winning_region_.getNrOfClauses();
  size_t new_lits = winning_region_.getNrOfLits();;
  L_DBG("reduceExistingClauses(): " << or_cl << " --> " << new_cl << " clauses.");
  L_DBG("reduceExistingClauses(): " << or_lits << " --> " << new_lits << " literals.");
  statistics_.notifyAfterCubeMin(0, 0);
}

// -------------------------------------------------------------------------------------------
void LearnSynthQBFInd::recomputeCheckCNF(bool take_small_win)
{
  VarManager::resetToLastPush();
  check_cnf_.clear();
  check_cnf_.addCNF(winning_region_);
  check_cnf_.negate();
  check_cnf_.swapPresentToNext();
  check_cnf_.addCNF(AIG2CNF::instance().getTrans());
  check_cnf_.addCNF(check_reach_);
  if(take_small_win)
  {
    check_cnf_.addCNF(winning_region_);
    if(do_reach_check_)
    {
      CNF prev_win(winning_region_);
      presentToPrevious(prev_win);
      check_cnf_.addCNF(prev_win);
    }
  }
  else
  {
    check_cnf_.addCNF(winning_region_large_);
    if(do_reach_check_)
    {
      CNF prev_win(winning_region_large_);
      presentToPrevious(prev_win);
      check_cnf_.addCNF(prev_win);
    }
  }
}

// -------------------------------------------------------------------------------------------
void LearnSynthQBFInd::recomputeGenCNF(bool take_small_win)
{
  generalize_clause_cnf_.clear();
  if(take_small_win)
  {
    generalize_clause_cnf_.addCNF(winning_region_);
    generalize_clause_cnf_.swapPresentToNext();
    generalize_clause_cnf_.addCNF(AIG2CNF::instance().getTrans());
    generalize_clause_cnf_.addCNF(winning_region_);
    generalize_clause_cnf_.addCNF(prev_trans_or_initial_);
    CNF prev_win(winning_region_);
    presentToPrevious(prev_win);
    generalize_clause_cnf_.addCNF(prev_win);
  }
  else
  {
    generalize_clause_cnf_.addCNF(winning_region_large_);
    generalize_clause_cnf_.swapPresentToNext();
    generalize_clause_cnf_.addCNF(AIG2CNF::instance().getTrans());
    generalize_clause_cnf_.addCNF(winning_region_large_);
    generalize_clause_cnf_.addCNF(prev_trans_or_initial_);
    CNF prev_win(winning_region_large_);
    presentToPrevious(prev_win);
    generalize_clause_cnf_.addCNF(prev_win);
  }
}

// -------------------------------------------------------------------------------------------
void LearnSynthQBFInd::restrictToStates(vector<int> &vec) const
{
  for(size_t cnt = 0; cnt < vec.size(); ++cnt)
  {
    int var = (vec[cnt] < 0) ? -vec[cnt] : vec[cnt];
    // We can always drop the error-bit of the state space (because we can be sure that all
    // states with error=1 have already been removed from the winning region). This saves
    // one iteration during the generalization.
    if(var == VarManager::instance().getPresErrorStateVar() ||
       VarManager::instance().getInfo(var).getKind() != VarInfo::PRES_STATE)
    {
      vec[cnt] = vec[vec.size() - 1];
      vec.pop_back();
      cnt--;
    }
  }
}

// -------------------------------------------------------------------------------------------
bool LearnSynthQBFInd::generalizeCounterexample(vector<int> &ce, bool check_sat) const
{
  if(check_sat)
  {
    CNF tmp_cnf1(generalize_clause_cnf_);
    tmp_cnf1.addCube(ce);
    if(qbf_solver_->isSat(gen_quant_, tmp_cnf1))
      return false;
  }
  vector<int> gen_ce(ce);
  for(size_t cnt = 0; cnt < ce.size(); ++cnt)
  {
    vector<int> tmp = gen_ce;
    Utils::remove(tmp, ce[cnt]);
    CNF tmp_cnf(generalize_clause_cnf_);
    tmp_cnf.addCube(tmp);
    tmp_cnf.addNegCubeAsClause(gen_ce);
    vector<int> next_gen_ce(gen_ce);
    Utils::swapPresentToNext(next_gen_ce);
    tmp_cnf.addNegCubeAsClause(next_gen_ce);
    vector<int> prev_tmp(tmp);
    presentToPrevious(prev_tmp);
    prev_tmp.push_back(-current_state_is_initial_);
    tmp_cnf.addNegCubeAsClause(prev_tmp);
    if(!qbf_solver_->isSat(gen_quant_, tmp_cnf))
      gen_ce = tmp;
  }
  ce = gen_ce;
  Utils::sort(ce);
  return true;
}


// -------------------------------------------------------------------------------------------
int LearnSynthQBFInd::presentToPrevious(int literal) const
{
  int var = (literal < 0) ? -literal : literal;
  if(literal < 0)
    return -current_to_previous_map_[var];
  else
    return current_to_previous_map_[var];
}

// -------------------------------------------------------------------------------------------
void LearnSynthQBFInd::presentToPrevious(vector<int> &cube_or_clause) const
{
  for(size_t cnt = 0; cnt < cube_or_clause.size(); ++cnt)
    cube_or_clause[cnt] = presentToPrevious(cube_or_clause[cnt]);
}

// -------------------------------------------------------------------------------------------
void LearnSynthQBFInd::presentToPrevious(CNF &cnf) const
{
  list<vector<int> > orig_clauses = cnf.getClauses();
  cnf.clear();
  for(CNF::ClauseConstIter it = orig_clauses.begin(); it != orig_clauses.end(); ++it)
  {
    vector<int> clause(*it);
    presentToPrevious(clause);
    cnf.addClause(clause);
  }
}

// -------------------------------------------------------------------------------------------
void LearnSynthQBFInd::debugCheckWinRegReach() const
{
#ifndef NDEBUG
  L_DBG("Checking winning region for correctness ...");
  LingelingApi sat_solver;
  CNF check(winning_region_);
  check.addCNF(AIG2CNF::instance().getInitial());
  MASSERT(sat_solver.isSat(check), "Initial state is not winning.");

  // All states of the winning region must be safe
  check.clear();
  check.addCNF(winning_region_);
  check.addCNF(AIG2CNF::instance().getUnsafeStates());
  MASSERT(!sat_solver.isSat(check), "Winning region is not safe.");

  // for all potentially reachable state in the winning region, we can enforce to stay in the
  // winning region:
  // exists x*,i*,c*: exists x,i: forall c: exists x':
  //   (I(x) | (W(x*) & T(x*,i*,c*,x))) & W(x) & T(x,i,c,x') & !W(x')
  // must be unsat
  check.clear();
  check.addCNF(winning_region_);
  check.swapPresentToNext();
  check.negate();
  check.addCNF(winning_region_);
  check.addCNF(AIG2CNF::instance().getTrans());
  check.addCNF(prev_trans_or_initial_);
  CNF prev_win(winning_region_);
  presentToPrevious(prev_win);
  check.addCNF(prev_win);

  vector<pair<VarInfo::VarKind, QBFSolver::Quant> > check_quant;
  check_quant.push_back(make_pair(VarInfo::PREV, QBFSolver::E));
  check_quant.push_back(make_pair(VarInfo::PRES_STATE, QBFSolver::E));
  check_quant.push_back(make_pair(VarInfo::INPUT, QBFSolver::E));
  check_quant.push_back(make_pair(VarInfo::CTRL, QBFSolver::A));
  check_quant.push_back(make_pair(VarInfo::NEXT_STATE, QBFSolver::E));
  check_quant.push_back(make_pair(VarInfo::TMP, QBFSolver::E));
  MASSERT(!qbf_solver_->isSat(check_quant, check), "Winning region is not inductive.");
#endif
}
